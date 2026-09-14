/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <utils/gp_vec.h>
#include <utils/gp_vec_str.h>

#include "font.h"

static const char *variant_names[GPF_VARIANTS] = {
	[GPF_MONO] = "mono",
	[GPF_REGULAR] = "regular",
	[GPF_BOLD_MONO] = "bold-mono",
	[GPF_BOLD] = "bold",
};

const char *gpf_variant_name(enum gpf_variant_id id)
{
	if (id >= GPF_VARIANTS)
		return NULL;

	return variant_names[id];
}

int gpf_variant_by_name(const char *name, enum gpf_variant_id *id)
{
	unsigned int i;

	for (i = 0; i < GPF_VARIANTS; i++) {
		if (!strcmp(name, variant_names[i])) {
			*id = i;
			return 0;
		}
	}

	return 1;
}

enum gpf_variant_id gpf_variant_parent(enum gpf_variant_id id)
{
	switch (id) {
	case GPF_REGULAR:
	case GPF_BOLD_MONO:
		return GPF_MONO;
	case GPF_BOLD:
		return GPF_REGULAR;
	default:
		return GPF_NO_PARENT;
	}
}

struct gpf_font *gpf_font_new(void)
{
	struct gpf_font *font = calloc(1, sizeof(struct gpf_font));

	if (!font)
		return NULL;

	font->meta.underline_thickness = 1;
	font->meta.strike_thickness = 1;
	font->meta.overline_thickness = 1;

	return font;
}

void gpf_family_meta_guess(struct gpf_family_meta *meta, const char *family,
                           int size, int ascent, int descent, int advance)
{
	memset(meta, 0, sizeof(*meta));

	snprintf(meta->family, sizeof(meta->family), "%s",
	         family && *family ? family : "New");

	meta->size = size;
	meta->ascent = ascent;
	meta->descent = descent;

	/* where the importer starts it, at the box */
	meta->em = ascent + descent;
	meta->x_height = ascent / 2;
	meta->cap_height = (3 * ascent) / 4;
	meta->ch_width = advance;

	meta->underline_pos = descent > 2 ? -2 : -1;
	meta->underline_thickness = 1;
	meta->strike_pos = meta->x_height / 2;
	meta->strike_thickness = 1;
	meta->overline_pos = ascent + 1;
	meta->overline_thickness = 1;

	meta->default_glyph = '?';
}

struct gpf_font *gpf_font_create(const struct gpf_family_meta *meta,
                                 int advance)
{
	struct gpf_font *font = gpf_font_new();
	struct gpf_variant *mono;

	if (!font)
		return NULL;

	font->meta = *meta;

	mono = gpf_font_variant(font, GPF_MONO, 1);
	if (!mono) {
		gpf_font_free(font);
		return NULL;
	}

	mono->advance = advance;

	return font;
}

int gpf_family_meta_same(const struct gpf_family_meta *a,
                         const struct gpf_family_meta *b)
{
	return !strcmp(a->family, b->family) &&
	       !strcmp(a->license, b->license) &&
	       a->size == b->size &&
	       a->ascent == b->ascent &&
	       a->descent == b->descent &&
	       a->line_gap == b->line_gap &&
	       a->em == b->em &&
	       a->x_height == b->x_height &&
	       a->cap_height == b->cap_height &&
	       a->ch_width == b->ch_width &&
	       a->underline_pos == b->underline_pos &&
	       a->underline_thickness == b->underline_thickness &&
	       a->strike_pos == b->strike_pos &&
	       a->strike_thickness == b->strike_thickness &&
	       a->overline_pos == b->overline_pos &&
	       a->overline_thickness == b->overline_thickness &&
	       a->default_glyph == b->default_glyph;
}

static void variant_free(struct gpf_variant *self)
{
	size_t i;

	if (!self)
		return;

	for (i = 0; i < gp_vec_len(self->glyphs); i++)
		free(self->glyphs[i].bits);

	gp_vec_free(self->glyphs);
	gp_vec_free(self->comments);
	free(self);
}

void gpf_font_free(struct gpf_font *self)
{
	unsigned int i;

	if (!self)
		return;

	for (i = 0; i < GPF_VARIANTS; i++)
		variant_free(self->variants[i]);

	gp_vec_free(self->comments);
	gp_vec_free(self->blocks);
	gp_vec_free(self->authors);
	free(self);
}

struct gpf_variant *gpf_font_variant(struct gpf_font *self,
                                     enum gpf_variant_id id, int create)
{
	struct gpf_variant *variant;

	if (id >= GPF_VARIANTS)
		return NULL;

	if (self->variants[id] || !create)
		return self->variants[id];

	variant = calloc(1, sizeof(struct gpf_variant));
	if (!variant)
		return NULL;

	variant->glyphs = gp_vec_new(0, sizeof(struct gpf_glyph));
	if (!variant->glyphs) {
		free(variant);
		return NULL;
	}

	variant->id = id;
	variant->spacing_mono = (id == GPF_MONO || id == GPF_BOLD_MONO);
	variant->derive_embolden = (id == GPF_BOLD_MONO || id == GPF_BOLD);

	self->variants[id] = variant;

	return variant;
}

/*
 * Returns the index of code, or the index it would be inserted at.
 */
static size_t glyph_lookup(struct gpf_variant *self, uint32_t code, int *found)
{
	size_t l = 0, r = gp_vec_len(self->glyphs);

	*found = 0;

	while (l < r) {
		size_t m = l + (r - l) / 2;

		if (self->glyphs[m].code == code) {
			*found = 1;
			return m;
		}

		if (self->glyphs[m].code < code)
			l = m + 1;
		else
			r = m;
	}

	return l;
}

struct gpf_glyph *gpf_variant_glyph(struct gpf_variant *self, uint32_t code)
{
	int found;
	size_t idx = glyph_lookup(self, code, &found);

	if (!found)
		return NULL;

	return &self->glyphs[idx];
}

struct gpf_glyph *gpf_variant_glyph_add(struct gpf_variant *self, uint32_t code)
{
	struct gpf_glyph *glyphs;
	int found;
	size_t idx = glyph_lookup(self, code, &found);

	if (found)
		return &self->glyphs[idx];

	/* the gap gp_vec_ins() opens is zeroed for us */
	glyphs = gp_vec_ins(self->glyphs, idx, 1);
	if (!glyphs)
		return NULL;

	self->glyphs = glyphs;
	self->glyphs[idx].code = code;

	return &self->glyphs[idx];
}

static int code_cmp(const void *a, const void *b)
{
	uint32_t ca = *(const uint32_t *)a, cb = *(const uint32_t *)b;

	return ca < cb ? -1 : ca > cb;
}

void gpf_font_clear_modified(struct gpf_font *self)
{
	unsigned int i;
	size_t j;

	for (i = 0; i < GPF_VARIANTS; i++) {
		struct gpf_variant *variant = self->variants[i];

		if (!variant)
			continue;

		for (j = 0; j < gpf_variant_glyphs(variant); j++)
			variant->glyphs[j].modified = 0;
	}
}

void gpf_font_ink_extent(struct gpf_font *self, int *top, int *bottom)
{
	unsigned int i;
	size_t j;

	*top = self->meta.ascent;
	*bottom = -self->meta.descent;

	for (i = 0; i < GPF_VARIANTS; i++) {
		struct gpf_variant *variant = self->variants[i];

		if (!variant)
			continue;

		for (j = 0; j < gpf_variant_glyphs(variant); j++) {
			struct gpf_glyph *glyph = &variant->glyphs[j];
			int low;

			if (!glyph->height)
				continue;

			low = glyph->bearing_y - (int)glyph->height + 1;

			if (glyph->bearing_y > *top)
				*top = glyph->bearing_y;

			if (low < *bottom)
				*bottom = low;
		}
	}
}

uint32_t *gpf_font_codes(struct gpf_font *self)
{
	uint32_t *vec = gp_vec_new(0, sizeof(uint32_t));
	unsigned int i;
	size_t j, cnt;

	if (!vec)
		return NULL;

	for (i = 0; i < GPF_VARIANTS; i++) {
		struct gpf_variant *variant = self->variants[i];

		if (!variant)
			continue;

		for (j = 0; j < gpf_variant_glyphs(variant); j++) {
			if (!GP_VEC_APPEND(vec, variant->glyphs[j].code)) {
				gp_vec_free(vec);
				return NULL;
			}
		}
	}

	cnt = gp_vec_len(vec);

	qsort(vec, cnt, sizeof(uint32_t), code_cmp);

	for (j = cnt; j > 1; j--) {
		if (vec[j-1] == vec[j-2])
			vec = gp_vec_del(vec, j-1, 1);
	}

	return vec;
}

int gpf_font_block_declared(struct gpf_font *self, uint32_t min)
{
	size_t i;

	for (i = 0; i < gp_vec_len(self->blocks); i++) {
		if (self->blocks[i].min == min)
			return 1;
	}

	return 0;
}

int gpf_font_block_add(struct gpf_font *self, uint32_t min, uint32_t max,
                       const char *name)
{
	struct gpf_block_decl decl = {.min = min, .max = max};
	struct gpf_block_decl *blocks;
	size_t i;

	if (gpf_font_block_declared(self, min))
		return 0;

	snprintf(decl.name, sizeof(decl.name), "%s", name ? name : "");

	if (!self->blocks) {
		self->blocks = gp_vec_new(0, sizeof(struct gpf_block_decl));
		if (!self->blocks)
			return 1;
	}

	for (i = 0; i < gp_vec_len(self->blocks); i++) {
		if (self->blocks[i].min > min)
			break;
	}

	blocks = gp_vec_ins(self->blocks, i, 1);
	if (!blocks)
		return 1;

	self->blocks = blocks;
	self->blocks[i] = decl;

	return 0;
}

const char *gpf_author_invalid(const char *years, const char *name,
                               const char *email)
{
	struct gpf_author author;
	size_t i;

	if (!isdigit((unsigned char)years[0]))
		return "the years do not start with a year";

	for (i = 0; years[i]; i++) {
		if (!isdigit((unsigned char)years[i]) && years[i] != '-' &&
		    years[i] != ',')
			return "the years are not years";
	}

	if (!name[0])
		return "the author needs a name";

	if (!email[0] || strpbrk(email, " \t<>") || !strchr(email, '@'))
		return "the email is not an email";

	if (strlen(years) >= sizeof(author.years) ||
	    strlen(name) >= sizeof(author.name) ||
	    strlen(email) >= sizeof(author.email))
		return "the author is too long";

	return NULL;
}

int gpf_authors_add(struct gpf_author **authors, const char *years,
                    const char *name, const char *email)
{
	struct gpf_author author;

	if (gpf_author_invalid(years, name, email))
		return 1;

	strcpy(author.years, years);
	strcpy(author.name, name);
	strcpy(author.email, email);

	if (!*authors) {
		*authors = gp_vec_new(0, sizeof(struct gpf_author));
		if (!*authors)
			return 1;
	}

	if (!GP_VEC_APPEND(*authors, author))
		return 1;

	return 0;
}

void gpf_authors_del(struct gpf_author **authors, size_t idx)
{
	if (idx >= gp_vec_len(*authors))
		return;

	*authors = gp_vec_del(*authors, idx, 1);
}

struct gpf_author *gpf_authors_dup(const struct gpf_author *authors)
{
	size_t len = gp_vec_len(authors);
	struct gpf_author *dup = gp_vec_new(len, sizeof(struct gpf_author));

	if (!dup)
		return NULL;

	if (len)
		memcpy(dup, authors, len * sizeof(struct gpf_author));

	return dup;
}

int gpf_authors_same(const struct gpf_author *a, const struct gpf_author *b)
{
	size_t i;

	if (gp_vec_len(a) != gp_vec_len(b))
		return 0;

	for (i = 0; i < gp_vec_len(a); i++) {
		if (strcmp(a[i].years, b[i].years) ||
		    strcmp(a[i].name, b[i].name) ||
		    strcmp(a[i].email, b[i].email))
			return 0;
	}

	return 1;
}

void gpf_variant_glyph_del(struct gpf_variant *self, uint32_t code)
{
	int found;
	size_t idx = glyph_lookup(self, code, &found);

	if (!found)
		return;

	free(self->glyphs[idx].bits);

	self->glyphs = gp_vec_del(self->glyphs, idx, 1);
}

static unsigned int variant_clear(struct gpf_variant *self,
                                  uint32_t min, uint32_t max)
{
	size_t i, first = 0, cnt = 0;

	for (i = 0; i < gp_vec_len(self->glyphs); i++) {
		if (self->glyphs[i].code < min)
			continue;

		if (self->glyphs[i].code > max)
			break;

		if (!cnt)
			first = i;

		free(self->glyphs[i].bits);
		cnt++;
	}

	if (cnt)
		self->glyphs = gp_vec_del(self->glyphs, first, cnt);

	return cnt;
}

unsigned int gpf_font_range_glyphs(struct gpf_font *self, uint32_t min,
                                   uint32_t max)
{
	unsigned int i, cnt = 0;
	size_t j;

	for (i = 0; i < GPF_VARIANTS; i++) {
		struct gpf_variant *variant = self->variants[i];

		if (!variant)
			continue;

		for (j = 0; j < gpf_variant_glyphs(variant); j++) {
			uint32_t code = variant->glyphs[j].code;

			if (code >= min && code <= max)
				cnt++;
		}
	}

	return cnt;
}

unsigned int gpf_font_block_clear(struct gpf_font *self, uint32_t min,
                                  uint32_t max)
{
	unsigned int i, cnt = 0;

	for (i = 0; i < GPF_VARIANTS; i++) {
		if (self->variants[i])
			cnt += variant_clear(self->variants[i], min, max);
	}

	return cnt;
}

void gpf_font_block_del(struct gpf_font *self, uint32_t min)
{
	size_t i;

	for (i = 0; i < gp_vec_len(self->blocks); i++) {
		if (self->blocks[i].min != min)
			continue;

		self->blocks = gp_vec_del(self->blocks, i, 1);
		return;
	}
}

void gpf_glyph_clear(struct gpf_glyph *self)
{
	uint32_t code = self->code;

	free(self->bits);
	memset(self, 0, sizeof(struct gpf_glyph));
	self->code = code;
}

int gpf_glyph_set_bits(struct gpf_glyph *self, const uint8_t *bits,
                       unsigned int w, unsigned int h,
                       int bearing_x, int bearing_y)
{
	unsigned int x, y;
	int min_x = w, max_x = -1, min_y = h, max_y = -1;
	unsigned int nw, nh;
	uint8_t *nbits;

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			if (!bits[y * w + x])
				continue;

			if ((int)x < min_x)
				min_x = x;
			if ((int)x > max_x)
				max_x = x;
			if ((int)y < min_y)
				min_y = y;
			if ((int)y > max_y)
				max_y = y;
		}
	}

	free(self->bits);
	self->bits = NULL;

	/* An empty glyph, a space, carries no ink and no bearings */
	if (max_x < 0) {
		self->width = 0;
		self->height = 0;
		self->bearing_x = 0;
		self->bearing_y = 0;
		return 0;
	}

	nw = max_x - min_x + 1;
	nh = max_y - min_y + 1;

	nbits = malloc(nw * nh);
	if (!nbits)
		return 1;

	for (y = 0; y < nh; y++) {
		for (x = 0; x < nw; x++)
			nbits[y * nw + x] = !!bits[(y + min_y) * w + x + min_x];
	}

	self->bits = nbits;
	self->width = nw;
	self->height = nh;
	self->bearing_x = bearing_x + (int)min_x;
	self->bearing_y = bearing_y - (int)min_y;

	return 0;
}
