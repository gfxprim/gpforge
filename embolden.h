/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file embolden.h
 * @brief Generates bold letters.
 */
#ifndef GPFORGE_EMBOLDEN_H
#define GPFORGE_EMBOLDEN_H

#include "font.h"

/**
 * @brief Emboldens a glyph.
 *
 * Smears the ink one pixel right in place, every set pixel sets the pixel to
 * its right. The ink box grows one pixel wider and the bearings and the
 * advance do not change.
 *
 * @param self A glyph to embolden.
 * @param cell_width Cell width for monospaced fonts. If non-zero and the
 *                   wider ink box would not fit the cell, the box keeps its
 *                   width and the smear out of it is dropped.
 * @return Non-zero if the allocation failed, zero otherwise.
 */
int gpf_embolden(struct gpf_glyph *self, int cell_width);

#endif /* GPFORGE_EMBOLDEN_H */
