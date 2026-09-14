/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/*
 * Shared bits of the headless tests.  Glyphs are written as rows of '#' and
 * '.', which is close enough to the file format to read at a glance.
 */
#ifndef GPFORGE_TEST_UTIL_H
#define GPFORGE_TEST_UTIL_H

#include <stdio.h>
#include <string.h>

#include "font.h"

static unsigned int failures;

#define FAIL(...) do { \
	printf("FAIL %s:%i: ", __func__, __LINE__); \
	printf(__VA_ARGS__); \
	putchar('\n'); \
	failures++; \
} while (0)

#define CHECK(cond, ...) do { \
	if (!(cond)) \
		FAIL(__VA_ARGS__); \
} while (0)

static inline void set_ink(struct gpf_glyph *glyph, const char **rows,
                           int bearing_x, int bearing_y)
{
	uint8_t bits[64 * 64];
	unsigned int w = strlen(rows[0]), h = 0, x, y;

	while (rows[h])
		h++;

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++)
			bits[y * w + x] = (rows[y][x] == '#');
	}

	glyph->kind = GPF_GLYPH_INK;
	glyph->has_advance = 1;
	glyph->has_bearing = 1;

	gpf_glyph_set_bits(glyph, bits, w, h, bearing_x, bearing_y);
}

static inline struct gpf_glyph *add_ink(struct gpf_variant *variant,
                                        uint32_t code, const char **rows,
                                        int bearing_x, int bearing_y,
                                        int advance)
{
	struct gpf_glyph *glyph = gpf_variant_glyph_add(variant, code);

	set_ink(glyph, rows, bearing_x, bearing_y);
	glyph->advance = advance;

	return glyph;
}

/*
 * Compares the ink against rows, ignoring the metrics.
 */
static inline int ink_eq(const struct gpf_glyph *glyph, const char **rows)
{
	unsigned int w = strlen(rows[0]), h = 0, x, y;

	while (rows[h])
		h++;

	if (glyph->width != w || glyph->height != h)
		return 0;

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			if (gpf_glyph_pixel(glyph, x, y) != (rows[y][x] == '#'))
				return 0;
		}
	}

	return 1;
}

static inline void print_ink(const struct gpf_glyph *glyph)
{
	unsigned int x, y;

	for (y = 0; y < glyph->height; y++) {
		printf("\t");
		for (x = 0; x < glyph->width; x++)
			putchar(gpf_glyph_pixel(glyph, x, y) ? '#' : '.');
		putchar('\n');
	}
}

#endif /* GPFORGE_TEST_UTIL_H */
