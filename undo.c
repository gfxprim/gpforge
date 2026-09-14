/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdlib.h>
#include <string.h>

#include <core/gp_common.h>
#include <utils/gp_vec.h>

#include "undo.h"

struct entry {
	enum gpf_variant_id id;
	uint32_t code;
	/* entries of one group are taken back together, 0 is no group */
	unsigned int group;
	/* a snapshot that is not present means the glyph was not there */
	int has_before, has_after;
	struct gpf_glyph before, after;
	/* a family entry instead, the before and the after, NULL for a glyph */
	struct gpf_family_meta *meta;
	/* and the authors, gp_vecs, before and after too */
	struct gpf_author *authors[2];
};

struct gpf_undo {
	struct entry *entries;
	/* how many entries are applied, everything above is redo */
	size_t pos;
	/* the pos the file was written at, NO_SAVED when that is gone */
	size_t saved;

	/* the group being recorded, and the last one handed out */
	unsigned int group;
	unsigned int groups;

	/* the pending begin() */
	int in_edit;
	enum gpf_variant_id id;
	uint32_t code;
	int has_before;
	struct gpf_glyph before;
};

#define NO_SAVED ((size_t)-1)

struct gpf_undo *gpf_undo_new(void)
{
	struct gpf_undo *self = calloc(1, sizeof(struct gpf_undo));

	if (!self)
		return NULL;

	self->entries = gp_vec_new(0, sizeof(struct entry));
	if (!self->entries) {
		free(self);
		return NULL;
	}

	return self;
}

void gpf_undo_group_begin(struct gpf_undo *self)
{
	self->group = ++self->groups;
}

void gpf_undo_group_end(struct gpf_undo *self)
{
	self->group = 0;
}

static void entry_clear(struct entry *self)
{
	free(self->before.bits);
	free(self->after.bits);
	free(self->meta);
	gp_vec_free(self->authors[0]);
	gp_vec_free(self->authors[1]);
	memset(self, 0, sizeof(*self));
}

void gpf_undo_free(struct gpf_undo *self)
{
	size_t i;

	if (!self)
		return;

	for (i = 0; i < gp_vec_len(self->entries); i++)
		entry_clear(&self->entries[i]);

	gp_vec_free(self->entries);
	free(self->before.bits);
	free(self);
}

static int glyph_snapshot(struct gpf_glyph *dst, const struct gpf_glyph *src)
{
	*dst = *src;
	dst->bits = NULL;

	if (!src->width || !src->height)
		return 0;

	dst->bits = malloc((size_t)src->width * src->height);
	if (!dst->bits)
		return 1;

	memcpy(dst->bits, src->bits, (size_t)src->width * src->height);

	return 0;
}

static int glyph_same(const struct gpf_glyph *a, const struct gpf_glyph *b)
{
	if (a->kind != b->kind || a->advance != b->advance ||
	    a->has_advance != b->has_advance || a->has_bearing != b->has_bearing)
		return 0;

	if (a->bearing_x != b->bearing_x || a->bearing_y != b->bearing_y)
		return 0;

	if (a->has_shift != b->has_shift || a->shift_x != b->shift_x ||
	    a->shift_y != b->shift_y)
		return 0;

	if (a->width != b->width || a->height != b->height)
		return 0;

	if (a->base != b->base || a->accent != b->accent ||
	    a->dx != b->dx || a->dy != b->dy)
		return 0;

	if (!a->width)
		return 1;

	return !memcmp(a->bits, b->bits, (size_t)a->width * a->height);
}

static struct gpf_glyph *glyph_get(struct gpf_font *font,
                                   enum gpf_variant_id id, uint32_t code)
{
	struct gpf_variant *variant = font->variants[id];

	if (!variant)
		return NULL;

	return gpf_variant_glyph(variant, code);
}

int gpf_undo_begin(struct gpf_undo *self, struct gpf_font *font,
                   enum gpf_variant_id id, uint32_t code)
{
	struct gpf_glyph *glyph = glyph_get(font, id, code);

	free(self->before.bits);
	memset(&self->before, 0, sizeof(self->before));

	self->in_edit = 1;
	self->id = id;
	self->code = code;
	self->has_before = !!glyph;

	if (!glyph)
		return 0;

	return glyph_snapshot(&self->before, glyph);
}

int gpf_undo_retarget(struct gpf_undo *self, struct gpf_font *font,
                      enum gpf_variant_id id, uint32_t code)
{
	if (self->in_edit && self->id == id && self->code == code)
		return 0;

	if (self->in_edit)
		gpf_undo_commit(self, font);

	return gpf_undo_begin(self, font, id, code);
}

/*
 * Appends an entry, which the journal then owns: it is freed here when it
 * cannot be added.
 */
static int push(struct gpf_undo *self, struct entry *entry)
{
	struct entry *entries;
	size_t i;

	/* the redo thrown away may be what the file has */
	if (self->saved != NO_SAVED && self->saved > self->pos)
		self->saved = NO_SAVED;

	/* an edit after an undo throws the redo away */
	for (i = self->pos; i < gp_vec_len(self->entries); i++)
		entry_clear(&self->entries[i]);

	entries = gp_vec_resize(self->entries, self->pos);
	if (!entries) {
		entry_clear(entry);
		return 1;
	}

	self->entries = entries;

	if (!GP_VEC_APPEND(self->entries, *entry)) {
		entry_clear(entry);
		return 1;
	}

	self->pos = gp_vec_len(self->entries);

	return 0;
}

void gpf_undo_commit(struct gpf_undo *self, struct gpf_font *font)
{
	struct gpf_glyph *glyph;
	struct entry entry = {.id = self->id, .code = self->code,
	                      .group = self->group};

	if (!self->in_edit)
		return;

	self->in_edit = 0;

	glyph = glyph_get(font, self->id, self->code);

	/* nothing happened, do not clutter the journal with it */
	if (!glyph && !self->has_before)
		return;

	/* the editor marks the glyph on the first touch, take that back */
	if (glyph && self->has_before && glyph_same(glyph, &self->before)) {
		glyph->modified = self->before.modified;
		return;
	}

	entry.has_before = self->has_before;
	entry.has_after = !!glyph;

	/*
	 * Something did change, and the file does not have it: the journal is
	 * where every edit passes, so this is where the glyph is marked.
	 */
	if (glyph)
		glyph->modified = 1;

	if (self->has_before) {
		entry.before = self->before;
		memset(&self->before, 0, sizeof(self->before));
	}

	if (glyph && glyph_snapshot(&entry.after, glyph)) {
		entry_clear(&entry);
		return;
	}

	push(self, &entry);
}

/*
 * The font keeps a copy of its own, so that the entry can be applied again.
 */
static int authors_set(struct gpf_font *font, const struct gpf_author *authors)
{
	struct gpf_author *dup = gpf_authors_dup(authors);

	if (!dup)
		return 1;

	gp_vec_free(font->authors);
	font->authors = dup;

	return 0;
}

int gpf_undo_family(struct gpf_undo *self, struct gpf_font *font,
                    const struct gpf_family_meta *meta,
                    const struct gpf_author *authors)
{
	struct entry entry = {.group = self->group};

	if (gpf_family_meta_same(&font->meta, meta) &&
	    gpf_authors_same(font->authors, authors))
		return 0;

	entry.meta = malloc(2 * sizeof(*entry.meta));
	entry.authors[0] = gpf_authors_dup(font->authors);
	entry.authors[1] = gpf_authors_dup(authors);

	if (!entry.meta || !entry.authors[0] || !entry.authors[1] ||
	    authors_set(font, authors)) {
		entry_clear(&entry);
		return 1;
	}

	entry.meta[0] = font->meta;
	entry.meta[1] = *meta;

	font->meta = *meta;

	return push(self, &entry);
}

/*
 * Puts a snapshot back, or removes the glyph when there was none.
 */
static int restore(struct gpf_font *font, enum gpf_variant_id id, uint32_t code,
                   int has, const struct gpf_glyph *snapshot)
{
	struct gpf_variant *variant;
	struct gpf_glyph *glyph;

	if (!has) {
		/* only this variant, the others are none of our business */
		if (font->variants[id])
			gpf_variant_glyph_del(font->variants[id], code);

		return 0;
	}

	variant = gpf_font_variant(font, id, 1);
	if (!variant)
		return 1;

	glyph = gpf_variant_glyph_add(variant, code);
	if (!glyph)
		return 1;

	free(glyph->bits);

	if (glyph_snapshot(glyph, snapshot))
		return 1;

	return 0;
}

/*
 * Whether a glyph differs from the file: it does when an entry between the
 * current position and the one the file was written at touched it.
 */
static int glyph_modified(struct gpf_undo *self, enum gpf_variant_id id,
                          uint32_t code)
{
	size_t i, from, to;

	if (self->saved == NO_SAVED)
		return 1;

	from = GP_MIN(self->pos, self->saved);
	to = GP_MAX(self->pos, self->saved);

	for (i = from; i < to; i++) {
		const struct entry *entry = &self->entries[i];

		if (!entry->meta && entry->id == id && entry->code == code)
			return 1;
	}

	return 0;
}

/*
 * The snapshots carry whatever the flag was when they were taken, which says
 * nothing about the file, so it is worked out again for the entries applied.
 */
static void mark_modified(struct gpf_undo *self, struct gpf_font *font,
                          size_t from, size_t to)
{
	size_t i;

	for (i = from; i < to; i++) {
		const struct entry *entry = &self->entries[i];
		struct gpf_glyph *glyph;

		if (entry->meta)
			continue;

		glyph = glyph_get(font, entry->id, entry->code);
		if (glyph)
			glyph->modified = glyph_modified(self, entry->id, entry->code);
	}
}

/*
 * Puts one side of an entry back.  A glyph outranks the family in what is
 * reported, since it is the thing the editor has to go to.
 */
static void apply(struct gpf_font *font, const struct entry *entry, int redo,
                  enum gpf_undo_what *what, enum gpf_variant_id *id,
                  uint32_t *code)
{
	if (entry->meta) {
		font->meta = entry->meta[redo];
		authors_set(font, entry->authors[redo]);

		if (*what == GPF_UNDO_NONE)
			*what = GPF_UNDO_FAMILY;

		return;
	}

	if (redo) {
		restore(font, entry->id, entry->code, entry->has_after,
		        &entry->after);
	} else {
		restore(font, entry->id, entry->code, entry->has_before,
		        &entry->before);
	}

	*id = entry->id;
	*code = entry->code;
	*what = GPF_UNDO_GLYPH;
}

enum gpf_undo_what gpf_undo(struct gpf_undo *self, struct gpf_font *font,
                            enum gpf_variant_id *id, uint32_t *code)
{
	enum gpf_undo_what what = GPF_UNDO_NONE;
	struct entry *entry;
	unsigned int group;
	size_t end = self->pos;

	if (!self->pos)
		return GPF_UNDO_NONE;

	entry = &self->entries[--self->pos];
	group = entry->group;

	apply(font, entry, 0, &what, id, code);

	/* the rest of the group goes with it */
	while (group && self->pos && self->entries[self->pos-1].group == group) {
		entry = &self->entries[--self->pos];
		apply(font, entry, 0, &what, id, code);
	}

	mark_modified(self, font, self->pos, end);

	return what;
}

enum gpf_undo_what gpf_redo(struct gpf_undo *self, struct gpf_font *font,
                            enum gpf_variant_id *id, uint32_t *code)
{
	enum gpf_undo_what what = GPF_UNDO_NONE;
	struct entry *entry;
	unsigned int group;
	size_t start = self->pos;

	if (self->pos >= gp_vec_len(self->entries))
		return GPF_UNDO_NONE;

	entry = &self->entries[self->pos++];
	group = entry->group;

	apply(font, entry, 1, &what, id, code);

	while (group && self->pos < gp_vec_len(self->entries) &&
	       self->entries[self->pos].group == group) {
		entry = &self->entries[self->pos++];
		apply(font, entry, 1, &what, id, code);
	}

	mark_modified(self, font, start, self->pos);

	return what;
}

void gpf_undo_saved(struct gpf_undo *self)
{
	self->saved = self->pos;
}

int gpf_undo_is_saved(struct gpf_undo *self)
{
	return self->saved == self->pos;
}

int gpf_undo_can_undo(struct gpf_undo *self)
{
	return self && self->pos;
}

int gpf_undo_can_redo(struct gpf_undo *self)
{
	return self && self->pos < gp_vec_len(self->entries);
}
