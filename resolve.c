/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdlib.h>
#include <string.h>

#include "embolden.h"
#include "compose.h"
#include "edit.h"
#include "resolve.h"

/* a compose block may reference a composed glyph, but not forever */
#define GPF_COMPOSE_DEPTH_MAX 8

static const char *provenance_names[] = {
	[GPF_PROV_STORED] = "stored",
	[GPF_PROV_COMPOSED] = "composed",
	[GPF_PROV_INHERITED] = "inherited",
	[GPF_PROV_EMBOLDENED] = "emboldened",
	[GPF_PROV_BOLD_MONO] = "bold-mono",
};

const char *gpf_provenance_name(enum gpf_provenance prov)
{
	return provenance_names[prov];
}

void gpf_resolved_clear(struct gpf_resolved *self)
{
	free(self->glyph.bits);
	memset(self, 0, sizeof(*self));
}

static int glyph_copy(struct gpf_glyph *dst, const struct gpf_glyph *src)
{
	free(dst->bits);
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

/*
 * Takes the ink over, the metrics stay readable in src.
 */
static void glyph_move(struct gpf_glyph *dst, struct gpf_glyph *src)
{
	free(dst->bits);
	*dst = *src;
	src->bits = NULL;
}

/*
 * A numbers block overrides the metrics it carries and nothing else.  The
 * shift moves the ink wherever the parent has it.
 */
static void apply_metrics(struct gpf_glyph *glyph, const struct gpf_glyph *entry)
{
	if (!entry)
		return;

	if (entry->has_advance)
		glyph->advance = entry->advance;

	if (entry->has_shift) {
		glyph->bearing_x += entry->shift_x;
		glyph->bearing_y += entry->shift_y;
	}
}

static struct gpf_glyph *entry_get(struct gpf_font *font,
                                   enum gpf_variant_id id, uint32_t code)
{
	struct gpf_variant *variant = font->variants[id];

	if (!variant)
		return NULL;

	return gpf_variant_glyph(variant, code);
}

/*
 * An entry that carries ink, either drawn or composed.
 */
static int entry_has_ink(const struct gpf_glyph *entry)
{
	return entry && entry->kind != GPF_GLYPH_METRICS;
}

static int resolve(struct gpf_font *font, enum gpf_variant_id id,
                   uint32_t code, struct gpf_resolved *res, int depth);

static int materialize(struct gpf_font *font, enum gpf_variant_id id,
                       const struct gpf_glyph *entry, struct gpf_resolved *res,
                       int depth)
{
	struct gpf_resolved base = {0}, accent = {0};
	int ret;

	res->ink_from = id;

	if (entry->kind == GPF_GLYPH_INK) {
		res->prov = GPF_PROV_STORED;
		return glyph_copy(&res->glyph, entry);
	}

	if (depth >= GPF_COMPOSE_DEPTH_MAX)
		return 1;

	if (resolve(font, id, entry->base, &base, depth + 1))
		return 1;

	if (resolve(font, id, entry->accent, &accent, depth + 1)) {
		gpf_resolved_clear(&base);
		return 1;
	}

	ret = gpf_compose(&res->glyph, &base.glyph, &accent.glyph,
	                  entry->dx, entry->dy);

	gpf_resolved_clear(&base);
	gpf_resolved_clear(&accent);

	res->prov = GPF_PROV_COMPOSED;
	res->glyph.code = entry->code;

	return ret;
}

/*
 * The base variant, the ink is either drawn or composed here.  A numbers
 * block has no parent to inherit ink from, which is how an empty glyph such
 * as a space is stored.
 */
static int resolve_mono(struct gpf_font *font, uint32_t code,
                        struct gpf_resolved *res, int depth)
{
	struct gpf_glyph *entry = entry_get(font, GPF_MONO, code);

	if (!entry)
		return 1;

	if (entry_has_ink(entry))
		return materialize(font, GPF_MONO, entry, res, depth);

	res->glyph.code = code;
	res->glyph.kind = GPF_GLYPH_INK;
	res->glyph.has_advance = 1;
	res->glyph.has_bearing = 1;
	res->glyph.advance = entry->advance;
	res->prov = GPF_PROV_STORED;
	res->ink_from = GPF_MONO;

	return 0;
}

/*
 * The composition that defines a glyph, taken from this variant or inherited
 * from a parent.  A composition is inherited as a definition, not as ink: a
 * variant that redraws the base redraws everything composed from it, which is
 * the whole reason for storing accented glyphs this way.  An ink entry anywhere
 * on the way up overrides the definition, that is what drawing one means.
 */
struct gpf_glyph *gpf_compose_def(struct gpf_font *font,
                                  enum gpf_variant_id id, uint32_t code)
{
	while (id != GPF_NO_PARENT) {
		struct gpf_glyph *entry = entry_get(font, id, code);

		if (entry && entry->kind == GPF_GLYPH_COMPOSE)
			return entry;

		if (entry_has_ink(entry))
			return NULL;

		id = gpf_variant_parent(id);
	}

	return NULL;
}

static int resolve_regular(struct gpf_font *font, uint32_t code,
                          struct gpf_resolved *res, int depth)
{
	struct gpf_glyph *entry = entry_get(font, GPF_REGULAR, code);
	struct gpf_glyph *def;

	if (entry_has_ink(entry))
		return materialize(font, GPF_REGULAR, entry, res, depth);

	def = gpf_compose_def(font, GPF_MONO, code);

	if (def && !materialize(font, GPF_REGULAR, def, res, depth)) {
		apply_metrics(&res->glyph, entry);
		return 0;
	}

	if (resolve_mono(font, code, res, depth))
		return 1;

	res->prov = GPF_PROV_INHERITED;
	res->ink_from = GPF_MONO;

	apply_metrics(&res->glyph, entry);

	return 0;
}

/* the cell of the monospace family, zero when there is none to keep */
static int mono_cell(struct gpf_font *font)
{
	struct gpf_variant *mono = font->variants[GPF_MONO];

	if (!mono || !mono->spacing_mono)
		return 0;

	return mono->advance;
}

static int resolve_bold_mono(struct gpf_font *font, uint32_t code,
                             struct gpf_resolved *res, int depth)
{
	struct gpf_glyph *entry = entry_get(font, GPF_BOLD_MONO, code);
	struct gpf_glyph *def;

	if (entry_has_ink(entry))
		return materialize(font, GPF_BOLD_MONO, entry, res, depth);

	/* the parts are emboldened on the way out of this variant */
	def = gpf_compose_def(font, GPF_MONO, code);

	if (def && !materialize(font, GPF_BOLD_MONO, def, res, depth)) {
		apply_metrics(&res->glyph, entry);
		return 0;
	}

	if (resolve_mono(font, code, res, depth))
		return 1;

	/* bold-mono keeps the cell the monospace family shares */
	if (gpf_embolden(&res->glyph, mono_cell(font))) {
		gpf_resolved_clear(res);
		return 1;
	}

	res->prov = GPF_PROV_EMBOLDENED;
	res->ink_from = GPF_MONO;

	apply_metrics(&res->glyph, entry);

	return 0;
}

static int ink_eq(const struct gpf_glyph *a, const struct gpf_glyph *b)
{
	if (a->width != b->width || a->height != b->height)
		return 0;

	if (!a->width || !a->height)
		return 1;

	return !memcmp(a->bits, b->bits, (size_t)a->width * a->height);
}

/*
 * Whether regular's ink differs from mono's, not whether regular has an ink
 * entry.  Editing materializes a glyph as ink and saving writes the same ink
 * back as a numbers block, so the two have to resolve bold the same way.
 */
static int regular_overrides_ink(const struct gpf_glyph *regular_entry,
                                 const struct gpf_glyph *mono)
{
	if (!entry_has_ink(regular_entry))
		return 0;

	if (!mono)
		return 1;

	return !ink_eq(regular_entry, mono);
}

/*
 * The three step rule.  The choice is made per glyph, which is what stops a
 * hand fix having to be drawn twice.
 */
static int resolve_bold(struct gpf_font *font, uint32_t code,
                        struct gpf_resolved *res, int depth)
{
	struct gpf_glyph *entry = entry_get(font, GPF_BOLD, code);
	struct gpf_glyph *regular_entry;
	struct gpf_resolved regular = {0}, mono = {0};
	int has_mono, widen = 0;

	/* 1. hand drawn in bold */
	if (entry_has_ink(entry))
		return materialize(font, GPF_BOLD, entry, res, depth);

	/*
	 * A composition is built here out of this variant's parts, so its
	 * metrics are its base's and the step 2 and 3 rules below do not
	 * apply to it.
	 */
	regular_entry = gpf_compose_def(font, GPF_REGULAR, code);

	if (regular_entry &&
	    !materialize(font, GPF_BOLD, regular_entry, res, depth)) {
		apply_metrics(&res->glyph, entry);
		return 0;
	}

	if (resolve_regular(font, code, &regular, depth))
		return 1;

	has_mono = !resolve_mono(font, code, &mono, depth);

	regular_entry = entry_get(font, GPF_REGULAR, code);

	if (regular_overrides_ink(regular_entry,
	                          has_mono ? &mono.glyph : NULL)) {
		/* 2. regular overrides the ink, embolden the shape it should */
		glyph_move(&res->glyph, &regular.glyph);

		/* proportional, so the pixel goes on the advance below */
		if (gpf_embolden(&res->glyph, 0)) {
			gpf_resolved_clear(&mono);
			gpf_resolved_clear(res);
			return 1;
		}

		res->prov = GPF_PROV_EMBOLDENED;
		res->ink_from = GPF_REGULAR;
		widen = 1;
	} else {
		/* 3. whatever bold-mono has, hand fix or embolden */
		struct gpf_resolved bold_mono = {0};

		if (resolve_bold_mono(font, code, &bold_mono, depth)) {
			gpf_resolved_clear(&mono);
			gpf_resolved_clear(&regular);
			return 1;
		}

		glyph_move(&res->glyph, &bold_mono.glyph);

		res->prov = bold_mono.prov == GPF_PROV_EMBOLDENED ?
		            GPF_PROV_EMBOLDENED : GPF_PROV_BOLD_MONO;
		res->ink_from = GPF_BOLD_MONO;

		/*
		 * Bold-mono ink was made to fit the monospace cell, which is
		 * room only as long as regular keeps the advance mono has.
		 */
		widen = has_mono && regular.glyph.advance != mono.glyph.advance;

		gpf_resolved_clear(&bold_mono);
	}

	/* the metrics always come from regular */
	res->glyph.advance = regular.glyph.advance;
	res->glyph.bearing_x = regular.glyph.bearing_x;
	res->glyph.bearing_y = regular.glyph.bearing_y;

	/*
	 * A proportional advance is fitted to regular's ink, so bold keeps
	 * the gap regular leaves on the right and the advance grows by what
	 * the bold ink is wider.  A hand drawn glyph was drawn to its own
	 * advance and does not get here.
	 */
	if (widen && res->glyph.width)
		res->glyph.advance += (int)res->glyph.width - (int)regular.glyph.width;

	apply_metrics(&res->glyph, entry);

	gpf_resolved_clear(&mono);
	gpf_resolved_clear(&regular);

	return 0;
}

static int resolve(struct gpf_font *font, enum gpf_variant_id id,
                   uint32_t code, struct gpf_resolved *res, int depth)
{
	switch (id) {
	case GPF_MONO:
		return resolve_mono(font, code, res, depth);
	case GPF_REGULAR:
		return resolve_regular(font, code, res, depth);
	case GPF_BOLD_MONO:
		return resolve_bold_mono(font, code, res, depth);
	case GPF_BOLD:
		return resolve_bold(font, code, res, depth);
	default:
		return 1;
	}
}

/*
 * Neutralizing the entry makes every resolution path treat it as "no ink
 * here", which is exactly what the entry is an override of.
 */
int gpf_resolve_derived(struct gpf_font *font, enum gpf_variant_id id,
                        uint32_t code, struct gpf_resolved *res)
{
	struct gpf_variant *variant = font->variants[id];
	struct gpf_glyph *entry = variant ? gpf_variant_glyph(variant, code) : NULL;
	struct gpf_glyph saved;
	int ret;

	if (!entry)
		return gpf_resolve(font, id, code, res);

	saved = *entry;

	entry->kind = GPF_GLYPH_METRICS;
	entry->has_advance = 0;
	entry->has_shift = 0;

	ret = gpf_resolve(font, id, code, res);

	*entry = saved;

	return ret;
}

enum gpf_entry_form gpf_entry_form(struct gpf_font *font,
                                   enum gpf_variant_id id,
                                   const struct gpf_glyph *glyph,
                                   struct gpf_glyph *out)
{
	struct gpf_resolved base;
	enum gpf_entry_form form = GPF_ENTRY_INK;
	struct gpf_glyph *numbers = out;

	memset(out, 0, sizeof(*out));
	out->code = glyph->code;
	out->kind = GPF_GLYPH_METRICS;

	if (glyph->kind != GPF_GLYPH_INK)
		return GPF_ENTRY_INK;

	if (gpf_variant_parent(id) == GPF_NO_PARENT)
		return GPF_ENTRY_INK;

	if (gpf_resolve_derived(font, id, glyph->code, &base))
		return GPF_ENTRY_INK;

	if (glyph->width != base.glyph.width || glyph->height != base.glyph.height)
		goto out;

	if (glyph->width &&
	    memcmp(glyph->bits, base.glyph.bits,
	           (size_t)glyph->width * glyph->height))
		goto out;

	/* the same ink, so only the numbers can differ */
	form = GPF_ENTRY_METRICS;

	if (glyph->bearing_x != base.glyph.bearing_x ||
	    glyph->bearing_y != base.glyph.bearing_y) {
		numbers->has_shift = 1;
		numbers->shift_x = glyph->bearing_x - base.glyph.bearing_x;
		numbers->shift_y = glyph->bearing_y - base.glyph.bearing_y;
	}

	if (glyph->advance != base.glyph.advance) {
		numbers->has_advance = 1;
		numbers->advance = glyph->advance;
	}

	if (!numbers->has_shift && !numbers->has_advance)
		form = GPF_ENTRY_NONE;
out:
	gpf_resolved_clear(&base);

	return form;
}

void gpf_font_simplify(struct gpf_font *font)
{
	unsigned int i;
	size_t j;

	/* from the base outwards, a variant derives through its parent */
	for (i = 0; i < GPF_VARIANTS; i++) {
		struct gpf_variant *variant = font->variants[i];

		if (!variant || gpf_variant_parent(i) == GPF_NO_PARENT)
			continue;

		for (j = gpf_variant_glyphs(variant); j > 0; j--) {
			struct gpf_glyph *glyph = &variant->glyphs[j-1];
			struct gpf_glyph numbers;

			switch (gpf_entry_form(font, i, glyph, &numbers)) {
			case GPF_ENTRY_NONE:
				gpf_variant_glyph_del(variant, glyph->code);
			break;
			case GPF_ENTRY_METRICS:
				free(glyph->bits);
				numbers.code = glyph->code;
				*glyph = numbers;
			break;
			case GPF_ENTRY_INK:
			break;
			}
		}
	}
}

int gpf_resolve(struct gpf_font *font, enum gpf_variant_id id, uint32_t code,
                struct gpf_resolved *res)
{
	memset(res, 0, sizeof(*res));

	if (resolve(font, id, code, res, 0)) {
		gpf_resolved_clear(res);
		return 1;
	}

	res->glyph.code = code;

	return 0;
}

int gpf_measure(struct gpf_font *font, enum gpf_variant_id id,
                enum gpf_measure what, int *val)
{
	static const uint32_t codes[] = {
		[GPF_MEASURE_X_HEIGHT] = 'x',
		[GPF_MEASURE_CAP_HEIGHT] = 'H',
		[GPF_MEASURE_CH_WIDTH] = '0',
	};
	struct gpf_resolved res;
	int ret = 0;

	if (gpf_resolve(font, id, codes[what], &res))
		return 1;

	switch (what) {
	case GPF_MEASURE_X_HEIGHT:
	case GPF_MEASURE_CAP_HEIGHT:
		if (res.glyph.height)
			*val = res.glyph.bearing_y;
		else
			ret = 1;
	break;
	case GPF_MEASURE_CH_WIDTH:
		*val = res.glyph.advance;
	break;
	}

	gpf_resolved_clear(&res);

	return ret;
}
