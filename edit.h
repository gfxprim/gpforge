/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file edit.h
 * @brief Glyph editing.
 *
 * Pixels are addressed in glyph coordinates, a column counted from the
 * origin and a height counted from the baseline, positive up, never in ink
 * box coordinates.
 *
 * The ink box is derived from the ink, it grows and shrinks as pixels are
 * set and cleared, and nothing outside of this file needs to know it exists.
 */
#ifndef GPFORGE_EDIT_H
#define GPFORGE_EDIT_H

#include "font.h"

/**
 * @brief Returns a glyph pixel at a given position.
 *
 * @param self A glyph.
 * @param col A column counted from a glyph origin.
 * @param height A height from a baseline, positive up.
 *
 * @return A pixel value 0 or 1.
 */
int gpf_glyph_pixel_get(const struct gpf_glyph *self, int col, int height);

/**
 * @brief Sets or clears a glyph pixel.
 *
 * The ink box grows or is trimmed as needed.
 *
 * @param self A glyph.
 * @param col A column counted from a glyph origin.
 * @param height A height from a baseline, positive up.
 * @param set A value to set the pixel to 0 or 1.
 * @return Non-zero on an allocation failure.
 */
int gpf_glyph_pixel_set(struct gpf_glyph *self, int col, int height, int set);

/**
 * @brief Moves the glyph ink.
 *
 * Moves the ink, which is how the bearings are edited. A glyph with no
 * ink is left alone.
 *
 * @param self A glyph to shift.
 * @param dx An x offset to shift the glyph by, positive right.
 * @param dy A y offset to shift the glyph by, positive up.
 */
void gpf_glyph_shift(struct gpf_glyph *self, int dx, int dy);

/**
 * @brief Mirrors the ink horizontally.
 *
 * Mirrors the ink horizontally within the advance.
 *
 * @param self A glyph to mirror.
 */
void gpf_glyph_flip_h(struct gpf_glyph *self);

/**
 * @brief Mirrors the ink vertically.
 *
 * Mirrors the ink vertically within the ink box.
 *
 * @param self A glyph to mirror.
 */
void gpf_glyph_flip_v(struct gpf_glyph *self);

/**
 * @brief Clears the glyph ink.
 *
 * The bearings are reset to zero, the advance is kept.
 *
 * @param self A glyph to clear.
 */
void gpf_glyph_clear_ink(struct gpf_glyph *self);

#endif /* GPFORGE_EDIT_H */
