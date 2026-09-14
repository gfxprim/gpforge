/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdlib.h>
#include <string.h>

#include <utils/gp_vec.h>

#include "resolve.h"
#include "face.h"

/*
 * The ASCII table is indexed directly by gfxprim, so it always spans the whole
 * 0x20 - 0x7f range.  Everything above that goes into a second table.
 */
#define ASCII_MIN 0x20
#define ASCII_MAX 0x7f

/*
 * A glyph is five bytes of metrics followed by the bitmap, rows byte aligned,
 * the same layout as struct gp_glyph.
 */
static int emit_glyph(uint8_t **data, const struct gpf_glyph *glyph,
                      gp_glyph_offset *off)
{
	unsigned int row_bytes = (glyph->width + 7) / 8;
	size_t need = 5 + (size_t)row_bytes * glyph->height;
	size_t pos = gp_vec_len(*data);
	unsigned int x, y;
	uint8_t *dst;

	/* the compiled in fonts align glyphs on four bytes, do the same */
	need = (need + 3) & ~(size_t)3;

	dst = gp_vec_expand(*data, need);
	if (!dst)
		return 1;

	*data = dst;
	dst += pos;

	memset(dst, 0, need);

	dst[0] = glyph->width;
	dst[1] = glyph->height;
	dst[2] = glyph->bearing_x;
	dst[3] = glyph->bearing_y;
	dst[4] = glyph->advance;

	for (y = 0; y < glyph->height; y++) {
		for (x = 0; x < glyph->width; x++) {
			if (gpf_glyph_pixel(glyph, x, y))
				dst[5 + y * row_bytes + x/8] |= 0x80 >> (x%8);
		}
	}

	*off = pos;

	return 0;
}

static int build_table(struct gpf_font *font, enum gpf_variant_id id,
                       gp_glyphs *table, uint32_t min, uint32_t max,
                       uint8_t **data, gp_font_face *face,
                       gp_glyph_offset blank)
{
	uint32_t code;
	size_t cnt = max - min + 1;

	table->offsets = malloc(cnt * sizeof(gp_glyph_offset));
	if (!table->offsets)
		return 1;

	table->min_glyph = min;
	table->max_glyph = max;

	for (code = min; code <= max; code++) {
		struct gpf_resolved res;
		int width;

		/*
		 * A missing glyph points at the blank rather than at
		 * GP_NOGLYPH: gp_glyph_get() falls back to '?' and returns
		 * NULL when that is missing too, and gfxprim's text metric
		 * dereferences the result without checking.  A font being
		 * built has no ASCII to fall back on.
		 */
		table->offsets[code - min] = blank;

		if (gpf_resolve(font, id, code, &res))
			continue;

		if (emit_glyph(data, &res.glyph, &table->offsets[code - min])) {
			gpf_resolved_clear(&res);
			return 1;
		}

		width = res.glyph.bearing_x + (int)res.glyph.width;

		if (width > face->max_glyph_width)
			face->max_glyph_width = width;

		if (res.glyph.advance > face->max_glyph_advance)
			face->max_glyph_advance = res.glyph.advance;

		gpf_resolved_clear(&res);
	}

	return 0;
}

gp_font_face *gpf_face_build(struct gpf_font *font, enum gpf_variant_id id)
{
	struct gpf_family_meta *m = &font->meta;
	gp_font_face *face;
	uint8_t *data;
	uint32_t *codes;
	uint32_t high_max = 0;
	unsigned int tables = 1, i;
	size_t j;
	struct gpf_glyph blank = {};
	gp_glyph_offset blank_off;

	codes = gpf_font_codes(font);
	if (!codes)
		return NULL;

	for (j = 0; j < gp_vec_len(codes); j++) {
		if (codes[j] > ASCII_MAX)
			high_max = codes[j];
	}

	gp_vec_free(codes);

	if (high_max)
		tables = 2;

	face = calloc(1, sizeof(gp_font_face) + tables * sizeof(gp_glyphs));
	if (!face)
		return NULL;

	data = gp_vec_new(0, 1);
	if (!data) {
		free(face);
		return NULL;
	}

	snprintf(face->family_name, sizeof(face->family_name), "%s", m->family);

	face->style = (id == GPF_MONO || id == GPF_BOLD_MONO) ? GP_FONT_MONO : GP_FONT_REGULAR;

	if (id == GPF_BOLD || id == GPF_BOLD_MONO)
		face->style |= GP_FONT_BOLD;

	face->glyph_tables = tables;
	face->ascent = m->ascent;
	face->descent = m->descent;
	face->glyph_bitmap_format = GP_FONT_BITMAP_1BPP;

	/*
	 * The typographic metrics, which is what the format carries them for:
	 * gp_font_face has had them since the em, x-height and cap height
	 * landed upstream.  The em is the size the font is meant to be read as
	 * — what a layout gets for 1em — so it is the family file's em and not
	 * the pixel size.
	 */
	face->em = m->em;
	face->x_height = m->x_height;
	face->cap_height = m->cap_height;
	face->ch_width = m->ch_width;
	face->line_gap = m->line_gap;
	face->underline_pos = m->underline_pos;
	face->underline_thickness = m->underline_thickness;
	face->strike_pos = m->strike_pos;
	face->strike_thickness = m->strike_thickness;
	face->overline_pos = m->overline_pos;
	face->overline_thickness = m->overline_thickness;

	/* one blank glyph, shared by every codepoint the font has no glyph for */
	if (emit_glyph(&data, &blank, &blank_off))
		goto err;

	if (build_table(font, id, &face->glyphs[0], ASCII_MIN, ASCII_MAX,
	                &data, face, blank_off))
		goto err;

	if (tables > 1) {
		if (build_table(font, id, &face->glyphs[1], ASCII_MAX + 1,
		                high_max, &data, face, blank_off))
			goto err;
	}

	/*
	 * The glyph data may have been reallocated while it was filled in, so
	 * the table pointers are set once it is complete.
	 */
	for (i = 0; i < tables; i++)
		face->glyphs[i].glyphs = data;

	face->priv = data;

	return face;
err:
	for (i = 0; i < tables; i++)
		free(face->glyphs[i].offsets);

	gp_vec_free(data);
	free(face);

	return NULL;
}

void gpf_face_free(gp_font_face *self)
{
	unsigned int i;

	if (!self)
		return;

	for (i = 0; i < self->glyph_tables; i++)
		free(self->glyphs[i].offsets);

	gp_vec_free(self->priv);
	free(self);
}
