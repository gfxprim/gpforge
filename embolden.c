/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdlib.h>

#include "embolden.h"

int gpf_embolden(struct gpf_glyph *self, int cell_width)
{
	unsigned int w, x, y;
	uint8_t *bits;

	if (!self->width || !self->height)
		return 0;

	w = self->width + 1;

	/* the smear would leave the cell, so it is the smear that gives way */
	if (cell_width && self->bearing_x + (int)w > cell_width)
		w = self->width;

	bits = calloc(1, (size_t)w * self->height);
	if (!bits)
		return 1;

	for (y = 0; y < self->height; y++) {
		for (x = 0; x < self->width; x++) {
			if (!self->bits[y * self->width + x])
				continue;

			bits[y * w + x] = 1;

			if (x + 1 < w)
				bits[y * w + x + 1] = 1;
		}
	}

	free(self->bits);

	self->bits = bits;
	self->width = w;

	return 0;
}
