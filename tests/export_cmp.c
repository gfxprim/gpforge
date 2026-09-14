/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/*
 * Checks a compiled C export glyph by glyph, against two things:
 *
 * - the BDF it was exported from, read here by a reader of its own rather than
 *   by the importer, so that the importer is not the judge of its own output.
 *   A BDF is one face, and it is the mono one; the bytes are allowed to differ
 *   — gpforge trims the ink box where the BDF padded it — so the comparison is
 *   the ink as drawn, plus the advance;
 *
 * - the model, gpf_resolve() of every variant the family has a face for, bold
 *   included.  That is exact: the metrics and the ink are what the export was
 *   asked to write.
 *
 * The export is a shared object with an export_family() function appended to
 * it, because the family symbol itself is hidden.
 *
 * Usage: export_cmp <font.so> <font.bdf>
 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gfxprim.h>

#include "format_bdf.h"
#include "resolve.h"

#define CODES 0x10000

static unsigned int failures;

/* no fallbacks: a glyph that is not in the table is not in the font */
static const gp_glyph *glyph_lookup(const gp_font_face *face, uint32_t ch)
{
	unsigned int i;

	for (i = 0; i < face->glyph_tables; i++) {
		const gp_glyphs *tbl = &face->glyphs[i];
		uint32_t off;

		if (ch < tbl->min_glyph || ch > tbl->max_glyph)
			continue;

		if (!tbl->offsets)
			return (gp_glyph*)(tbl->glyphs + tbl->offset * (ch - tbl->min_glyph));

		off = tbl->offsets[ch - tbl->min_glyph];

		if (off == GP_NOGLYPH)
			return NULL;

		return (gp_glyph*)(tbl->glyphs + off);
	}

	return NULL;
}

static int in_tables(const gp_font_face *face, uint32_t ch)
{
	unsigned int i;

	for (i = 0; i < face->glyph_tables; i++) {
		if (ch >= face->glyphs[i].min_glyph && ch <= face->glyphs[i].max_glyph)
			return 1;
	}

	return 0;
}

/* a pixel of the glyph, in glyph coordinates: x from the origin, y up from the baseline */
static int pixel(const gp_glyph *glyph, int x, int y)
{
	unsigned int stride = (glyph->width + 7) / 8;
	int gx = x - glyph->bearing_x;
	int gy = glyph->bearing_y - y;

	if (gx < 0 || gx >= glyph->width || gy < 0 || gy >= glyph->height)
		return 0;

	return !!(glyph->bitmap[gy * stride + gx/8] & (0x80 >> (gx%8)));
}

struct bdf_glyph {
	int advance;
	int w, h, xoff, yoff;
	unsigned int stride;
	uint8_t *rows;
};

/* in the coordinates of pixel(), where the row on the baseline is y = 1 */
static int bdf_pixel(const struct bdf_glyph *glyph, int x, int y)
{
	int c = x - glyph->xoff;
	int r = glyph->yoff + glyph->h - y;

	if (c < 0 || c >= glyph->w || r < 0 || r >= glyph->h)
		return 0;

	return !!(glyph->rows[r * glyph->stride + c/8] & (0x80 >> (c%8)));
}

/*
 * The glyphs of a unicode BDF by codepoint.  Only what the comparison needs:
 * ENCODING, DWIDTH, BBX and the bitmap rows.
 */
static struct bdf_glyph **bdf_load(const char *path)
{
	struct bdf_glyph **glyphs = calloc(CODES, sizeof(*glyphs));
	struct bdf_glyph cur = {};
	FILE *f = fopen(path, "r");
	char line[512];
	long code = -1;
	int row = -1;

	if (!f || !glyphs) {
		fprintf(stderr, "can't load %s\n", path);
		exit(1);
	}

	while (fgets(line, sizeof(line), f)) {
		if (row >= 0 && row < cur.h) {
			unsigned int i;

			for (i = 0; i < cur.stride; i++) {
				unsigned int byte;

				if (sscanf(line + 2 * i, "%2x", &byte) == 1)
					cur.rows[row * cur.stride + i] = byte;
			}

			row++;
			continue;
		}

		if (sscanf(line, "ENCODING %ld", &code) == 1)
			continue;

		if (sscanf(line, "DWIDTH %d", &cur.advance) == 1)
			continue;

		if (sscanf(line, "BBX %d %d %d %d", &cur.w, &cur.h,
		           &cur.xoff, &cur.yoff) == 4)
			continue;

		if (!strncmp(line, "BITMAP", 6)) {
			cur.stride = (cur.w + 7) / 8;
			cur.rows = calloc(1, cur.stride * cur.h + 1);
			row = 0;
			continue;
		}

		if (!strncmp(line, "ENDCHAR", 7)) {
			if (code >= 0 && code < CODES && !glyphs[code]) {
				glyphs[code] = malloc(sizeof(cur));
				*glyphs[code] = cur;
			} else {
				free(cur.rows);
			}

			memset(&cur, 0, sizeof(cur));
			code = -1;
			row = -1;
		}
	}

	fclose(f);
	return glyphs;
}

static void cmp_bdf(const gp_font_face *face, struct bdf_glyph **bdf)
{
	unsigned int cnt = 0;
	uint32_t ch;
	int x, y;

	for (ch = 0; ch < CODES; ch++) {
		const gp_glyph *g = glyph_lookup(face, ch);

		if (!in_tables(face, ch))
			continue;

		if (!g && !bdf[ch])
			continue;

		if (!g || !bdf[ch]) {
			printf("FAIL %s U+%04X: %s\n", face->family_name, ch,
			       g ? "not in the BDF" : "missing in the export");
			failures++;
			continue;
		}

		cnt++;

		if (g->advance_x != bdf[ch]->advance) {
			printf("FAIL %s U+%04X: advance %u, the BDF says %i\n",
			       face->family_name, ch, g->advance_x,
			       bdf[ch]->advance);
			failures++;
			continue;
		}

		for (y = -64; y < 64; y++) {
			for (x = -64; x < 64; x++) {
				if (pixel(g, x, y) == bdf_pixel(bdf[ch], x, y))
					continue;

				printf("FAIL %s U+%04X: ink differs from the BDF at %i,%i\n",
				       face->family_name, ch, x, y);
				failures++;
				goto next;
			}
		}
next:
		;
	}

	printf("%-24s %5u glyphs against the BDF\n", face->family_name, cnt);
}

static void cmp_model(const gp_font_face *face, struct gpf_font *font,
                      enum gpf_variant_id id)
{
	unsigned int cnt = 0, dropped = 0, x, y;
	uint32_t ch;

	if (face->ascent != font->meta.ascent ||
	    face->descent != font->meta.descent) {
		printf("FAIL %s: ascent/descent %u/%u, the font says %i/%i\n",
		       face->family_name, face->ascent, face->descent,
		       font->meta.ascent, font->meta.descent);
		failures++;
	}

	for (ch = 0; ch < CODES; ch++) {
		const gp_glyph *g = glyph_lookup(face, ch);
		struct gpf_resolved res;
		const struct gpf_glyph *m;

		if (gpf_resolve(font, id, ch, &res)) {
			if (g) {
				printf("FAIL %s U+%04X: not in the %s variant\n",
				       face->family_name, ch, gpf_variant_name(id));
				failures++;
			}
			continue;
		}

		m = &res.glyph;

		if (!g) {
			if (in_tables(face, ch)) {
				printf("FAIL %s U+%04X: missing in the export\n",
				       face->family_name, ch);
				failures++;
			} else {
				dropped++;
			}

			gpf_resolved_clear(&res);
			continue;
		}

		cnt++;

		if (g->advance_x != m->advance || g->bearing_x != m->bearing_x ||
		    g->bearing_y != m->bearing_y || g->width != m->width ||
		    g->height != m->height) {
			printf("FAIL %s U+%04X: metrics %u %i,%i %ux%u, the model says %i %i,%i %ux%u\n",
			       face->family_name, ch, g->advance_x, g->bearing_x,
			       g->bearing_y, g->width, g->height, m->advance,
			       m->bearing_x, m->bearing_y, m->width, m->height);
			failures++;
			gpf_resolved_clear(&res);
			continue;
		}

		for (y = 0; y < m->height; y++) {
			for (x = 0; x < m->width; x++) {
				int gx = m->bearing_x + x, gy = m->bearing_y - y;

				if (pixel(g, gx, gy) == !!gpf_glyph_pixel(m, x, y))
					continue;

				printf("FAIL %s U+%04X: ink differs from the model at %i,%i\n",
				       face->family_name, ch, gx, gy);
				failures++;
				goto next;
			}
		}
next:
		gpf_resolved_clear(&res);
	}

	printf("%-24s %5u glyphs against %s", face->family_name, cnt,
	       gpf_variant_name(id));

	if (dropped)
		printf(", %u outside the ranges", dropped);

	printf("\n");
}

int main(int argc, char *argv[])
{
	const gp_font_family *(*export_family)(void);
	const gp_font_family *fam;
	struct bdf_glyph **bdf;
	struct gpf_font *font;
	char err[256];
	unsigned int i;
	int mono_seen = 0;
	void *so;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <font.so> <font.bdf>\n", argv[0]);
		return 1;
	}

	setvbuf(stdout, NULL, _IONBF, 0);

	so = dlopen(argv[1], RTLD_NOW);
	if (!so) {
		fprintf(stderr, "%s\n", dlerror());
		return 1;
	}

	export_family = (const gp_font_family *(*)(void))dlsym(so, "export_family");
	if (!export_family) {
		fprintf(stderr, "%s\n", dlerror());
		return 1;
	}

	fam = export_family();

	font = gpf_bdf_read(argv[2], err, sizeof(err));
	if (!font) {
		fprintf(stderr, "%s: %s\n", argv[2], err);
		return 1;
	}

	bdf = bdf_load(argv[2]);

	for (i = 0; fam->fonts[i]; i++) {
		const gp_font_face *face = fam->fonts[i];
		int mono = !!(face->style & GP_FONT_MONO);
		int bold = !!(face->style & GP_FONT_BOLD);
		enum gpf_variant_id id;

		if (mono)
			id = bold ? GPF_BOLD_MONO : GPF_MONO;
		else
			id = bold ? GPF_BOLD : GPF_REGULAR;

		if (id == GPF_MONO) {
			cmp_bdf(face, bdf);
			mono_seen = 1;
		}

		cmp_model(face, font, id);
	}

	if (!mono_seen) {
		printf("FAIL: the export has no mono face\n");
		failures++;
	}

	for (i = 0; i < CODES; i++) {
		if (bdf[i])
			free(bdf[i]->rows);
		free(bdf[i]);
	}

	free(bdf);
	gpf_font_free(font);
	dlclose(so);

	if (failures) {
		printf("%u failures\n", failures);
		return 1;
	}

	return 0;
}
