/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include <utils/gp_vec.h>

#include "resolve.h"
#include "format_bdf.h"

#define LINE_MAX_LEN 1024

/* Half of GPF_NAME_MAX, the two are joined into the family name */
#define BDF_NAME_MAX (GPF_NAME_MAX / 2)

struct bdf_state {
	struct gpf_font *font;
	struct gpf_variant *variant;

	char family_name[BDF_NAME_MAX];
	char weight_name[BDF_NAME_MAX];
	char registry[BDF_NAME_MAX];
	char encoding[BDF_NAME_MAX];
	int spacing_mono;
	int latin2;
	/* SIZE says the point size and the resolution it is meant at */
	int size_pt;
	int res_y;
};

/*
 * The corpus has each font twice, once encoded in ISO10646 and once in
 * iso8859-2, and the encoding of the latter is not unicode.  The table maps
 * the high half onto unicode.
 */
static const uint16_t iso8859_2_to_unicode[] = {
	0x00a0, 0x0104, 0x02d8, 0x0141, 0x00a4, 0x013d, 0x015a, 0x00a7,
	0x00a8, 0x0160, 0x015e, 0x0164, 0x0179, 0x00ad, 0x017d, 0x017b,
	0x00b0, 0x0105, 0x02db, 0x0142, 0x00b4, 0x013e, 0x015b, 0x02c7,
	0x00b8, 0x0161, 0x015f, 0x0165, 0x017a, 0x02dd, 0x017e, 0x017c,
	0x0154, 0x00c1, 0x00c2, 0x0102, 0x00c4, 0x0139, 0x0106, 0x00c7,
	0x010c, 0x00c9, 0x0118, 0x00cb, 0x011a, 0x00cd, 0x00ce, 0x010e,
	0x0110, 0x0143, 0x0147, 0x00d3, 0x00d4, 0x0150, 0x00d6, 0x00d7,
	0x0158, 0x016e, 0x00da, 0x0170, 0x00dc, 0x00dd, 0x0162, 0x00df,
	0x0155, 0x00e1, 0x00e2, 0x0103, 0x00e4, 0x013a, 0x0107, 0x00e7,
	0x010d, 0x00e9, 0x0119, 0x00eb, 0x011b, 0x00ed, 0x00ee, 0x010f,
	0x0111, 0x0144, 0x0148, 0x00f3, 0x00f4, 0x0151, 0x00f6, 0x00f7,
	0x0159, 0x016f, 0x00fa, 0x0171, 0x00fc, 0x00fd, 0x0163, 0x02d9,
};

static uint32_t map_code(const struct bdf_state *state, uint32_t code)
{
	if (!state->latin2 || code < 0xa0 || code > 0xff)
		return code;

	return iso8859_2_to_unicode[code - 0xa0];
}

static char *skip_space(char *str)
{
	while (*str == ' ' || *str == '\t')
		str++;

	return str;
}

static void strip_eol(char *line)
{
	size_t len = strlen(line);

	while (len && (line[len-1] == '\n' || line[len-1] == '\r' ||
	               line[len-1] == ' ' || line[len-1] == '\t'))
		line[--len] = 0;
}

/*
 * Splits a BDF line into a keyword and the rest.
 */
static char *split_kw(char *line, char **kw)
{
	char *val;

	*kw = skip_space(line);

	val = *kw;
	while (*val && *val != ' ' && *val != '\t')
		val++;

	if (*val)
		*val++ = 0;

	return skip_space(val);
}

/*
 * BDF property values are quoted strings, the quotes are not part of them.
 */
static void unquote(char *val, char *dst, size_t dst_len)
{
	size_t len;

	if (*val == '"')
		val++;

	snprintf(dst, dst_len, "%s", val);

	len = strlen(dst);

	if (len && dst[len-1] == '"')
		dst[len-1] = 0;
}

static int hex_digit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';

	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;

	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	return -1;
}

/*
 * Reads a single STARTCHAR block.  The glyph is added to the variant unless
 * it has no encoding.
 */
static int read_glyph(struct bdf_state *state, FILE *f,
                      char *err, size_t err_len)
{
	char line[LINE_MAX_LEN];
	long code = -1;
	int advance = 0;
	int w = 0, h = 0, xoff = 0, yoff = 0;
	int have_bbx = 0;
	uint8_t *bits = NULL;
	unsigned int row = 0;
	struct gpf_glyph *glyph;

	while (fgets(line, sizeof(line), f)) {
		char *kw, *val;

		strip_eol(line);
		val = split_kw(line, &kw);

		if (!strcmp(kw, "ENCODING")) {
			code = strtol(val, NULL, 10);
			continue;
		}

		if (!strcmp(kw, "DWIDTH")) {
			advance = strtol(val, NULL, 10);
			continue;
		}

		if (!strcmp(kw, "BBX")) {
			if (sscanf(val, "%i %i %i %i", &w, &h, &xoff, &yoff) != 4) {
				snprintf(err, err_len, "malformed BBX '%s'", val);
				return 1;
			}
			have_bbx = 1;
			continue;
		}

		if (!strcmp(kw, "BITMAP")) {
			if (!have_bbx) {
				snprintf(err, err_len, "BITMAP without a BBX");
				return 1;
			}

			if (w < 0 || h < 0 || w > 255 || h > 255) {
				snprintf(err, err_len, "BBX out of range");
				return 1;
			}

			bits = calloc(1, (size_t)w * h + 1);
			if (!bits) {
				snprintf(err, err_len, "out of memory");
				return 1;
			}

			continue;
		}

		if (!strcmp(kw, "ENDCHAR"))
			break;

		if (!bits)
			continue;

		/* a bitmap row, two hex digits per byte */
		if (row < (unsigned int)h) {
			unsigned int bytes = ((unsigned int)w + 7) / 8;
			unsigned int i;

			for (i = 0; i < bytes; i++) {
				int hi = hex_digit(kw[2 * i]);
				int lo = hex_digit(kw[2 * i + 1]);
				unsigned int b, x;

				if (hi < 0 || lo < 0) {
					free(bits);
					snprintf(err, err_len,
					         "malformed bitmap row '%s'", kw);
					return 1;
				}

				b = 16 * hi + lo;

				for (x = 0; x < 8; x++) {
					unsigned int px = 8 * i + x;

					if (px >= (unsigned int)w)
						break;

					bits[row * w + px] = !!(b & (0x80 >> x));
				}
			}
		}

		row++;
	}

	if (code < 0 || !bits) {
		free(bits);
		return 0;
	}

	glyph = gpf_variant_glyph_add(state->variant, map_code(state, code));
	if (!glyph) {
		free(bits);
		snprintf(err, err_len, "out of memory");
		return 1;
	}

	glyph->kind = GPF_GLYPH_INK;
	glyph->has_advance = 1;
	glyph->has_bearing = 1;
	glyph->advance = advance;

	if (gpf_glyph_set_bits(glyph, bits, w, h, xoff, h + yoff)) {
		free(bits);
		snprintf(err, err_len, "out of memory");
		return 1;
	}

	free(bits);

	return 0;
}

/*
 * The design square, which BDF says twice and gets wrong at least once per
 * file: HaxorSquare-8x8 kept the PIXEL_SIZE of the twelve pixel font it was
 * edited from, HaxorNarrow-16 kept a SIZE of 13, and HaxorNarrow-15 has no
 * PIXEL_SIZE at all.  So take the first one that is not absurd — a design
 * square is at least the ascent and at most the whole box — and fall back to
 * the box.
 */
static void measure_size(struct bdf_state *state)
{
	struct gpf_family_meta *m = &state->font->meta;
	int box = m->ascent + m->descent;
	int vals[2] = {m->size, 0};
	int i;

	if (state->size_pt && state->res_y)
		vals[1] = (state->size_pt * state->res_y + 36) / 72;

	for (i = 0; i < 2; i++) {
		if (vals[i] < m->ascent || vals[i] > box)
			continue;

		m->size = vals[i];
		return;
	}

	m->size = box;
}

/*
 * The metadata BDF does not have is measured off the glyphs.
 */
static void measure(struct gpf_font *font, struct gpf_variant *variant)
{
	struct gpf_family_meta *m = &font->meta;
	size_t i;
	int max_advance = 0;

	for (i = 0; i < gpf_variant_glyphs(variant); i++) {
		if (variant->glyphs[i].advance > max_advance)
			max_advance = variant->glyphs[i].advance;
	}

	if (variant->spacing_mono)
		variant->advance = max_advance;

	/*
	 * The em is the size the font is meant to be read as, which is a
	 * decision and not something a BDF knows: the box is a starting point
	 * and the family file is where it is then set.  A six pixel font meant
	 * to be shown at twice the size has an em of twelve.
	 */
	m->em = m->ascent + m->descent;

	gpf_measure(font, variant->id, GPF_MEASURE_X_HEIGHT, &m->x_height);
	gpf_measure(font, variant->id, GPF_MEASURE_CAP_HEIGHT, &m->cap_height);
	gpf_measure(font, variant->id, GPF_MEASURE_CH_WIDTH, &m->ch_width);

	m->underline_pos = m->descent > 2 ? -2 : -1;
	m->underline_thickness = 1;

	m->strike_pos = m->x_height / 2;
	m->strike_thickness = 1;

	/*
	 * Over the letters and clear of the ascent, which the accents reach —
	 * an overline drawn on them is no overline.
	 */
	m->overline_pos = m->ascent + 1;
	m->overline_thickness = 1;
}

struct gpf_font *gpf_bdf_read(const char *path, char *err, size_t err_len)
{
	char line[LINE_MAX_LEN];
	struct bdf_state state = {0};
	FILE *f;
	int len;

	f = fopen(path, "r");
	if (!f) {
		snprintf(err, err_len, "%s: %s", path, strerror(errno));
		return NULL;
	}

	state.font = gpf_font_new();
	if (!state.font) {
		fclose(f);
		snprintf(err, err_len, "out of memory");
		return NULL;
	}

	state.spacing_mono = 1;

	while (fgets(line, sizeof(line), f)) {
		char *kw, *val;

		strip_eol(line);
		val = split_kw(line, &kw);

		if (!strcmp(kw, "FAMILY_NAME")) {
			unquote(val, state.family_name, sizeof(state.family_name));
			continue;
		}

		if (!strcmp(kw, "WEIGHT_NAME")) {
			unquote(val, state.weight_name, sizeof(state.weight_name));
			continue;
		}

		if (!strcmp(kw, "CHARSET_REGISTRY")) {
			unquote(val, state.registry, sizeof(state.registry));
			continue;
		}

		if (!strcmp(kw, "CHARSET_ENCODING")) {
			unquote(val, state.encoding, sizeof(state.encoding));
			continue;
		}

		if (!strcmp(kw, "SPACING")) {
			char spacing[8];

			unquote(val, spacing, sizeof(spacing));
			state.spacing_mono = (spacing[0] != 'P' && spacing[0] != 'p');
			continue;
		}

		if (!strcmp(kw, "PIXEL_SIZE")) {
			state.font->meta.size = strtol(val, NULL, 10);
			continue;
		}

		if (!strcmp(kw, "SIZE")) {
			char *end;

			state.size_pt = strtol(val, &end, 10);
			strtol(end, &end, 10);
			state.res_y = strtol(end, NULL, 10);
			continue;
		}

		if (!strcmp(kw, "FONT_ASCENT")) {
			state.font->meta.ascent = strtol(val, NULL, 10);
			continue;
		}

		if (!strcmp(kw, "FONT_DESCENT")) {
			state.font->meta.descent = strtol(val, NULL, 10);
			continue;
		}

		if (!strcmp(kw, "DEFAULT_CHAR")) {
			long code = strtol(val, NULL, 10);

			if (code > 0)
				state.font->meta.default_glyph = code;
			continue;
		}

		if (!strcmp(kw, "STARTCHAR")) {
			if (!state.variant) {
				enum gpf_variant_id id;

				/*
				 * The two properties arrive in either order,
				 * so the encoding is decided once, when the
				 * first glyph shows up.
				 */
				state.latin2 = !strcasecmp(state.registry, "iso8859") &&
				               state.encoding[0] == '2';


				id = state.spacing_mono ? GPF_MONO : GPF_REGULAR;

				state.variant = gpf_font_variant(state.font, id, 1);
				if (!state.variant) {
					snprintf(err, err_len, "out of memory");
					goto err;
				}

				state.variant->spacing_mono = state.spacing_mono;
			}

			if (read_glyph(&state, f, err, err_len))
				goto err;

			continue;
		}
	}

	fclose(f);

	if (!state.variant) {
		snprintf(err, err_len, "%s: no glyphs", path);
		gpf_font_free(state.font);
		return NULL;
	}

	len = snprintf(state.font->meta.family,
	               sizeof(state.font->meta.family), "%s", state.family_name);

	if (state.weight_name[0] && strcmp(state.weight_name, "Medium") &&
	    len > 0 && len < (int)sizeof(state.font->meta.family) - 1) {
		snprintf(state.font->meta.family + len,
		         sizeof(state.font->meta.family) - len, " %s",
		         state.weight_name);
	}

	measure(state.font, state.variant);
	measure_size(&state);

	return state.font;
err:
	fclose(f);
	gpf_font_free(state.font);
	return NULL;
}

/*
 * BDF export.  One file is one face, so the caller says which of the four
 * variants it wants and the lattice resolves it: exporting `bold` writes the
 * emboldened glyphs out as ink, since a BDF has nowhere to say that a glyph is
 * derived.  What comes back through the importer is a
 * font with one variant, which is the shape a BDF can hold.
 *
 * A bitmap font has no physical size of its own, so the numbers that pretend
 * it does are picked to agree with each other: at 72 dpi a pixel is a point,
 * and PIXEL_SIZE, POINT_SIZE and SIZE all say the same thing.
 */
#define BDF_DPI 72

#define BDF_PROP_MAX (GPF_NAME_MAX + 32)
#define BDF_PROPS 24

/*
 * STARTPROPERTIES says how many property lines follow, so they are collected
 * and counted rather than printed as they come: a count that disagrees with
 * the lines is a file no reader will take.
 */
static void add_prop(char props[][BDF_PROP_MAX], unsigned int *cnt,
                     const char *fmt, ...)
{
	va_list ap;

	if (*cnt >= BDF_PROPS)
		return;

	va_start(ap, fmt);
	vsnprintf(props[(*cnt)++], BDF_PROP_MAX, fmt, ap);
	va_end(ap);
}

/* '-' separates the XLFD fields, so it cannot be inside one */
static void xlfd_field(char *dst, size_t len, const char *str)
{
	size_t i;

	snprintf(dst, len, "%s", str);

	for (i = 0; dst[i]; i++) {
		if (dst[i] == '-')
			dst[i] = ' ';
	}
}

/* what the header has to know before a glyph can be written */
struct bdf_extent {
	unsigned int cnt;
	int min_x, max_x, min_y, max_y;
	long advances;
	/* SPACING "M" is a promise that every glyph has the same advance */
	int fixed;
	int advance;
};

static void measure_face(struct gpf_font *font, enum gpf_variant_id id,
                         const uint32_t *codes, struct bdf_extent *self)
{
	size_t i;

	/* the line box is part of the font box, whatever the ink does */
	self->fixed = 1;
	self->min_x = 0;
	self->max_x = 0;
	self->min_y = -font->meta.descent;
	self->max_y = font->meta.ascent;

	for (i = 0; i < gp_vec_len(codes); i++) {
		struct gpf_resolved res;
		int x = 0, y = 0;

		if (gpf_resolve(font, id, codes[i], &res))
			continue;

		x = res.glyph.bearing_x;
		y = res.glyph.bearing_y - (int)res.glyph.height;

		self->min_x = GPF_MIN(self->min_x, x);
		self->max_x = GPF_MAX(self->max_x, x + (int)res.glyph.width);
		self->max_x = GPF_MAX(self->max_x, res.glyph.advance);
		self->min_y = GPF_MIN(self->min_y, y);
		self->max_y = GPF_MAX(self->max_y, res.glyph.bearing_y);

		if (!self->cnt)
			self->advance = res.glyph.advance;
		else if (res.glyph.advance != self->advance)
			self->fixed = 0;

		self->advances += res.glyph.advance;
		self->cnt++;

		gpf_resolved_clear(&res);
	}
}

/*
 * BBX is the ink box against the baseline, so the y offset is the bottom row
 * and not the bearing.  An empty glyph is `BBX 0 0 0 0` and no bitmap rows,
 * which is what gbdfed writes for a space and what the reader trims back to.
 */
static void write_glyph(FILE *f, uint32_t code, const struct gpf_glyph *glyph,
                        int size)
{
	unsigned int row_bytes = (glyph->width + 7) / 8;
	unsigned int x, y, i;

	if (code < 0x10000)
		fprintf(f, "STARTCHAR uni%04X\n", code);
	else
		fprintf(f, "STARTCHAR u%06X\n", code);

	fprintf(f, "ENCODING %u\n", code);
	fprintf(f, "SWIDTH %i 0\n", glyph->advance * 1000 / size);
	fprintf(f, "DWIDTH %i 0\n", glyph->advance);
	fprintf(f, "BBX %u %u %i %i\n", glyph->width, glyph->height,
	        glyph->bearing_x, glyph->bearing_y - (int)glyph->height);
	fprintf(f, "BITMAP\n");

	for (y = 0; y < glyph->height; y++) {
		for (i = 0; i < row_bytes; i++) {
			unsigned int bits = 0;

			for (x = 0; x < 8; x++) {
				if (8 * i + x >= glyph->width)
					break;

				if (gpf_glyph_pixel(glyph, 8 * i + x, y))
					bits |= 0x80 >> x;
			}

			fprintf(f, "%02X", bits);
		}

		fprintf(f, "\n");
	}

	fprintf(f, "ENDCHAR\n");
}

int gpf_bdf_write(struct gpf_font *font, enum gpf_variant_id id, FILE *f,
                  char *err, size_t err_len)
{
	struct gpf_family_meta *m = &font->meta;
	struct gpf_variant *variant = font->variants[id];
	int mono = variant ? variant->spacing_mono
	                   : (id == GPF_MONO || id == GPF_BOLD_MONO);
	int bold = (id == GPF_BOLD || id == GPF_BOLD_MONO);
	const char *weight = bold ? "Bold" : "Medium";
	const char *spacing;
	char props[BDF_PROPS][BDF_PROP_MAX];
	char family[GPF_NAME_MAX];
	struct bdf_extent ext = {};
	unsigned int cnt = 0, i;
	uint32_t *codes;
	int size, avg;
	size_t j;

	codes = gpf_font_codes(font);
	if (!codes) {
		snprintf(err, err_len, "out of memory");
		return 1;
	}

	measure_face(font, id, codes, &ext);

	if (!ext.cnt) {
		snprintf(err, err_len, "the %s variant has no glyphs",
		         gpf_variant_name(id));
		gp_vec_free(codes);
		return 1;
	}

	/*
	 * A monospace variant whose advances are not all the one advance is a
	 * lint error, not something to say "M" about: the file describes what
	 * it holds.
	 */
	spacing = (mono && ext.fixed) ? "M" : "P";

	size = m->size > 0 ? m->size : m->ascent + m->descent;

	/* AVERAGE_WIDTH is in tenths of a pixel */
	avg = (10 * ext.advances + ext.cnt / 2) / ext.cnt;

	add_prop(props, &cnt, "FOUNDRY \"gpforge\"");
	add_prop(props, &cnt, "FAMILY_NAME \"%s\"", m->family);
	add_prop(props, &cnt, "WEIGHT_NAME \"%s\"", weight);
	add_prop(props, &cnt, "SLANT \"R\"");
	add_prop(props, &cnt, "SETWIDTH_NAME \"Normal\"");
	add_prop(props, &cnt, "SPACING \"%s\"", spacing);
	add_prop(props, &cnt, "CHARSET_REGISTRY \"ISO10646\"");
	add_prop(props, &cnt, "CHARSET_ENCODING \"1\"");
	add_prop(props, &cnt, "PIXEL_SIZE %i", size);
	add_prop(props, &cnt, "POINT_SIZE %i", 10 * size);
	add_prop(props, &cnt, "RESOLUTION_X %i", BDF_DPI);
	add_prop(props, &cnt, "RESOLUTION_Y %i", BDF_DPI);
	add_prop(props, &cnt, "FONT_ASCENT %i", m->ascent);
	add_prop(props, &cnt, "FONT_DESCENT %i", m->descent);
	add_prop(props, &cnt, "AVERAGE_WIDTH %i", avg);

	if (m->default_glyph)
		add_prop(props, &cnt, "DEFAULT_CHAR %u", m->default_glyph);

	xlfd_field(family, sizeof(family), m->family);

	fprintf(f, "STARTFONT 2.1\n");
	fprintf(f, "COMMENT Generated by gpforge, the %s variant of %s.\n",
	        gpf_variant_name(id), m->family);

	if (m->license[0])
		fprintf(f, "COMMENT SPDX-License-Identifier: %s\n", m->license);

	fprintf(f, "FONT -gpforge-%s-%s-R-Normal--%i-%i-%i-%i-%s-%i-ISO10646-1\n",
	        family, weight, size, 10 * size, BDF_DPI, BDF_DPI, spacing, avg);

	fprintf(f, "SIZE %i %i %i\n", size, BDF_DPI, BDF_DPI);

	fprintf(f, "FONTBOUNDINGBOX %i %i %i %i\n", ext.max_x - ext.min_x,
	        ext.max_y - ext.min_y, ext.min_x, ext.min_y);

	fprintf(f, "STARTPROPERTIES %u\n", cnt);

	for (i = 0; i < cnt; i++)
		fprintf(f, "%s\n", props[i]);

	fprintf(f, "ENDPROPERTIES\n");

	fprintf(f, "CHARS %u\n", ext.cnt);

	for (j = 0; j < gp_vec_len(codes); j++) {
		struct gpf_resolved res;

		if (gpf_resolve(font, id, codes[j], &res))
			continue;

		write_glyph(f, codes[j], &res.glyph, size);

		gpf_resolved_clear(&res);
	}

	fprintf(f, "ENDFONT\n");

	gp_vec_free(codes);

	return 0;
}
