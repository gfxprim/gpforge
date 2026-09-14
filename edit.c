/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdlib.h>
#include <string.h>

#include "edit.h"

int gpf_glyph_pixel_get(const struct gpf_glyph *self, int col, int height)
{
	int x = col - self->bearing_x;
	int y = self->bearing_y - height;

	if (x < 0 || y < 0 || x >= (int)self->width || y >= (int)self->height)
		return 0;

	return self->bits[y * self->width + x];
}

int gpf_glyph_pixel_set(struct gpf_glyph *self, int col, int height, int set)
{
	int left, right, top, bottom;
	unsigned int w, h, x, y;
	uint8_t *bits;
	int ret;

	if (gpf_glyph_pixel_get(self, col, height) == !!set)
		return 0;

	if (self->width) {
		left = GPF_MIN(self->bearing_x, col);
		right = GPF_MAX(self->bearing_x + (int)self->width, col + 1);
		top = GPF_MAX(self->bearing_y, height);
		bottom = GPF_MIN(self->bearing_y - (int)self->height, height - 1);
	} else {
		if (!set)
			return 0;

		left = col;
		right = col + 1;
		top = height;
		bottom = height - 1;
	}

	w = right - left;
	h = top - bottom;

	bits = calloc(1, (size_t)w * h);
	if (!bits)
		return 1;

	for (y = 0; y < self->height; y++) {
		for (x = 0; x < self->width; x++) {
			bits[(y + top - self->bearing_y) * w +
			     x + self->bearing_x - left] =
				self->bits[y * self->width + x];
		}
	}

	bits[(top - height) * w + col - left] = !!set;

	/* set_bits() trims, which is how clearing shrinks the box back */
	ret = gpf_glyph_set_bits(self, bits, w, h, left, top);

	free(bits);

	return ret;
}

void gpf_glyph_shift(struct gpf_glyph *self, int dx, int dy)
{
	if (!self->width)
		return;

	self->bearing_x += dx;
	self->bearing_y += dy;
}

void gpf_glyph_flip_h(struct gpf_glyph *self)
{
	unsigned int x, y;

	for (y = 0; y < self->height; y++) {
		for (x = 0; x < self->width / 2; x++) {
			uint8_t tmp = self->bits[y * self->width + x];

			self->bits[y * self->width + x] =
				self->bits[y * self->width + self->width - x - 1];
			self->bits[y * self->width + self->width - x - 1] = tmp;
		}
	}

	if (self->width)
		self->bearing_x = self->advance - self->bearing_x - self->width;
}

void gpf_glyph_flip_v(struct gpf_glyph *self)
{
	unsigned int x, y;

	for (y = 0; y < self->height / 2; y++) {
		for (x = 0; x < self->width; x++) {
			uint8_t tmp = self->bits[y * self->width + x];

			self->bits[y * self->width + x] =
				self->bits[(self->height - y - 1) * self->width + x];
			self->bits[(self->height - y - 1) * self->width + x] = tmp;
		}
	}
}

void gpf_glyph_clear_ink(struct gpf_glyph *self)
{
	free(self->bits);

	self->bits = NULL;
	self->width = 0;
	self->height = 0;
	self->bearing_x = 0;
	self->bearing_y = 0;
}
