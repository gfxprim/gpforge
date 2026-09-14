/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <utils/gp_vec.h>

#include "resolve.h"
#include "lint.h"

struct sink {
	/* a gp_vec, or NULL when only the first finding is wanted */
	struct gpf_lint_finding **vec;
	struct gpf_lint_finding *first;
	int found;
};

static void emit(struct sink *sink, enum gpf_variant_id id, uint32_t code,
                 enum gpf_lint_severity sev, const char *fmt, ...)
	__attribute__((format(printf, 5, 6)));

static void emit(struct sink *sink, enum gpf_variant_id id, uint32_t code,
                 enum gpf_lint_severity sev, const char *fmt, ...)
{
	struct gpf_lint_finding finding = {.id = id, .code = code, .sev = sev};
	va_list va;

	if (sink->found && !sink->vec)
		return;

	va_start(va, fmt);
	vsnprintf(finding.msg, sizeof(finding.msg), fmt, va);
	va_end(va);

	sink->found = 1;

	if (sink->first && !sink->first->msg[0])
		*sink->first = finding;

	if (sink->vec)
		GP_VEC_APPEND(*sink->vec, finding);
}

/*
 * Box drawing, block elements and geometric shapes are cell art: they are
 * drawn to fill the cell and to touch the neighbouring one, which is the whole
 * point of them.  Geometry rules do not apply.
 */
static int cell_art(uint32_t code)
{
	return code >= 0x2500 && code <= 0x25ff;
}

static const char x_letters[] = "acemnorsuvwxz";
static const char cap_letters[] = "BDEFHIKLMNPRT";

static int in_set(const char *set, uint32_t code)
{
	/* strchr() finds the terminator, and U+0000 is not a letter */
	if (!code || code >= 0x80)
		return 0;

	return !!strchr(set, (int)code);
}

/*
 * A glyph whose ink is the same as its parent's has not been emboldened: the
 * automatic embolden would have widened it, so this one was drawn by hand and
 * the hand stopped too early.
 */
static int same_ink(const struct gpf_glyph *a, const struct gpf_glyph *b)
{
	if (a->width != b->width || a->height != b->height)
		return 0;

	if (a->bearing_x != b->bearing_x || a->bearing_y != b->bearing_y)
		return 0;

	if (!a->width)
		return 1;

	return !memcmp(a->bits, b->bits, (size_t)a->width * a->height);
}

static void check_bold(struct gpf_font *font, enum gpf_variant_id id,
                       uint32_t code, const struct gpf_glyph *glyph,
                       struct sink *sink)
{
	enum gpf_variant_id parent = gpf_variant_parent(id);
	struct gpf_resolved res;

	if (parent == GPF_NO_PARENT || !glyph->width)
		return;

	if (gpf_resolve(font, parent, code, &res))
		return;

	if (same_ink(glyph, &res.glyph)) {
		emit(sink, id, code, GPF_LINT_WARN, "no wider than %s",
		     gpf_variant_name(parent));
	}

	gpf_resolved_clear(&res);
}

static void check_compose(struct gpf_font *font, enum gpf_variant_id id,
                          uint32_t code, const struct gpf_glyph *entry,
                          struct sink *sink)
{
	struct gpf_resolved res;
	uint32_t missing = 0;

	if (gpf_resolve(font, id, entry->base, &res))
		missing = entry->base;
	else
		gpf_resolved_clear(&res);

	if (!missing) {
		if (gpf_resolve(font, id, entry->accent, &res))
			missing = entry->accent;
		else
			gpf_resolved_clear(&res);
	}

	if (missing) {
		emit(sink, id, code, GPF_LINT_ERROR,
		     "composed from U+%04X, which the font has not got", missing);
	}
}

/*
 * Everything that can be said about one glyph in one variant.  The editor
 * calls this for the glyph it is showing, the report calls it for all of them.
 */
static void check_glyph(struct gpf_font *font, enum gpf_variant_id id,
                        uint32_t code, struct sink *sink)
{
	struct gpf_family_meta *m = &font->meta;
	struct gpf_variant *variant = font->variants[id];
	struct gpf_glyph *entry;
	struct gpf_resolved res;
	int right, bottom;

	if (!variant)
		return;

	entry = gpf_variant_glyph(variant, code);

	if (entry && entry->kind == GPF_GLYPH_COMPOSE)
		check_compose(font, id, code, entry, sink);

	if (gpf_resolve(font, id, code, &res)) {
		/*
		 * A variant that has an entry for a glyph it cannot resolve is
		 * a glyph the base variant lost: the ink lives in the base and
		 * the overlay is all that is left.
		 */
		if (entry && entry->kind == GPF_GLYPH_METRICS) {
			emit(sink, id, code, GPF_LINT_ERROR,
			     "metrics for a glyph %s has not got",
			     gpf_variant_name(GPF_MONO));
		}

		return;
	}

	right = res.glyph.bearing_x + (int)res.glyph.width;
	bottom = res.glyph.bearing_y - (int)res.glyph.height + 1;

	if (res.glyph.width && res.glyph.advance <= 0) {
		emit(sink, id, code, GPF_LINT_ERROR,
		     "ink with an advance of %i", res.glyph.advance);
	}

	if (variant->spacing_mono && variant->advance &&
	    res.glyph.advance != variant->advance) {
		emit(sink, id, code, GPF_LINT_ERROR,
		     "advance %i, the variant's is %i", res.glyph.advance,
		     variant->advance);
	}

	if (res.glyph.width && !cell_art(code)) {
		enum gpf_lint_severity sev = variant->spacing_mono
		                           ? GPF_LINT_ERROR : GPF_LINT_WARN;

		if (res.glyph.bearing_x < 0) {
			emit(sink, id, code, sev, "ink starts %i left of the origin",
			     -res.glyph.bearing_x);
		}

		if (right > res.glyph.advance) {
			emit(sink, id, code, sev,
			     "ink %i wide in a cell of %i", right,
			     res.glyph.advance);
		}

		if (res.glyph.bearing_y > m->ascent) {
			emit(sink, id, code, GPF_LINT_WARN, "ink %i above the ascent",
			     res.glyph.bearing_y - m->ascent);
		}

		if (bottom < -m->descent) {
			emit(sink, id, code, GPF_LINT_WARN, "ink %i below the descent",
			     -m->descent - bottom);
		}
	}

	if (res.glyph.width && m->x_height && in_set(x_letters, code) &&
	    res.glyph.bearing_y != m->x_height) {
		emit(sink, id, code, GPF_LINT_WARN, "x-height %i, the font's is %i",
		     res.glyph.bearing_y, m->x_height);
	}

	if (res.glyph.width && m->cap_height && in_set(cap_letters, code) &&
	    res.glyph.bearing_y != m->cap_height) {
		emit(sink, id, code, GPF_LINT_WARN,
		     "cap height %i, the font's is %i", res.glyph.bearing_y,
		     m->cap_height);
	}

	if (entry && entry->kind == GPF_GLYPH_INK &&
	    (id == GPF_BOLD || id == GPF_BOLD_MONO))
		check_bold(font, id, code, &res.glyph, sink);

	gpf_resolved_clear(&res);
}

int gpf_lint_glyph(struct gpf_font *font, enum gpf_variant_id id, uint32_t code,
                   struct gpf_lint_finding *out)
{
	struct sink sink = {.first = out};

	memset(out, 0, sizeof(*out));

	check_glyph(font, id, code, &sink);

	return !sink.found;
}

struct gpf_lint_finding *gpf_lint(struct gpf_font *font)
{
	struct gpf_lint_finding *vec = gp_vec_new(0, sizeof(struct gpf_lint_finding));
	struct sink sink = {.vec = &vec};
	uint32_t *codes = gpf_font_codes(font);
	unsigned int i;
	size_t j;

	if (!vec || !codes) {
		gp_vec_free(codes);
		return vec;
	}

	for (i = 0; i < GPF_VARIANTS; i++) {
		for (j = 0; j < gp_vec_len(codes); j++) {
			sink.found = 0;
			check_glyph(font, i, codes[j], &sink);
		}
	}

	gp_vec_free(codes);

	return vec;
}

void gpf_lint_count(struct gpf_font *font, unsigned int *errors,
                    unsigned int *warnings)
{
	struct gpf_lint_finding *findings = gpf_lint(font);
	size_t i;

	*errors = 0;
	*warnings = 0;

	for (i = 0; findings && i < gp_vec_len(findings); i++) {
		if (findings[i].sev == GPF_LINT_ERROR)
			(*errors)++;
		else
			(*warnings)++;
	}

	gp_vec_free(findings);
}

const char *gpf_lint_severity_name(enum gpf_lint_severity sev)
{
	switch (sev) {
	case GPF_LINT_ERROR:
		return "error";
	case GPF_LINT_WARN:
		return "warn";
	}

	return "?";
}
