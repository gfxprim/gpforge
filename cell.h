/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file cell.h
 * @brief A glyph cell rendering.
 *
 * One glyph cell: the codepoint, then the character as the widget font draws
 * it, then the glyph itself.  The glyph table is a grid of these and so is the
 * strip of accented letters, and they are the same thing on the screen because
 * they are the same thing to read.
 *
 * The middle line is a reference: what the letter is supposed to look like,
 * from a font that is not the one being edited, right above the one that is.
 */
#ifndef GPFORGE_CELL_H
#define GPFORGE_CELL_H

#include <gfxprim.h>

#include "font.h"

/**
 * @brief A glyph cell geometry.
 *
 * Filled in by gpf_cell_init(), the same for every cell of a grid.
 */
struct gpf_cell {
	/** @brief The cell size in pixels. */
	unsigned int w, h;
	/** @brief The height of the codepoint line at the top. */
	unsigned int label_h;
	/** @brief The line under it, where the widget font draws the character. */
	unsigned int ref_h;
	/** 
	 * @brief How far the font's ink reaches above the baseline, at least
	 *        the ascent.
	 */
	int top;
	/** @brief The theme's padding, above the glyph and below it. */
	unsigned int pad;
	/** 
	 * @brief The pixel multiplier, the table's, or less when there is no
	 *        room.
	 */
	unsigned int zoom;
	/** @brief Where the grid starts in the pixmap, see gpf_cell_center(). */
	int x0, y0;
	/**
	 * @brief The color the cells are drawn on, the selected one is
	 *        highlighted instead.
	 *
	 * The foreground color set by gpf_cell_init(), a grid that follows the
	 * focus of its widget sets it with gpf_gui_back_color().
	 */
	gp_pixel bg;
};

/**
 * @brief Sizes a cell for the current font and zoom.
 *
 * The glyph box comes from the ink extent of the whole font plus the theme's
 * padding, not from the ascent and the descent, so that an accent over a
 * capital is not cut off.
 *
 * @param self A cell to fill in.
 * @param ctx The widget render context.
 * @param max_h Zero means "as much as you like", anything else caps the zoom
 *              so that the cell fits it.
 */
void gpf_cell_init(struct gpf_cell *self, const gp_widget_render_ctx *ctx,
                   unsigned int max_h);

/**
 * @brief Centres a grid of cells in a pixmap.
 *
 * A pixmap is sized by the layout and a cell by the font, so the one is rarely
 * a whole number of the other, and the slack is split on both sides rather
 * than left as a strip on the right and at the bottom.  The positions the
 * cells are drawn at stay relative to the grid; the drawing adds the origin.
 *
 * @param self A cell, its origin is set.
 * @param p The pixmap the grid is drawn into.
 * @param cols The grid width in cells.
 * @param rows The grid height in cells.
 */
void gpf_cell_center(struct gpf_cell *self, const gp_pixmap *p,
                     unsigned int cols, unsigned int rows);

/**
 * @brief Finds the cell under a point.
 *
 * @param self A cell.
 * @param cols The grid width in cells.
 * @param rows The grid height in cells.
 * @param x A pixmap x coordinate.
 * @param y A pixmap y coordinate.
 * @return The index of the cell, counted along the rows, or -1 when the point
 *         is not on the grid.
 */
int gpf_cell_at(const struct gpf_cell *self, unsigned int cols,
                unsigned int rows, gp_coord x, gp_coord y);

/**
 * @brief Draws a glyph cell.
 *
 * The codepoint, dimmed when the font has not got the glyph, inverted when the
 * variant has an entry of its own and red when that entry is modified; then
 * the character as the widget font draws it, then the glyph as the variant
 * being edited resolves it, centred on its advance.
 *
 * @param p The pixmap to draw into.
 * @param ctx The widget render context.
 * @param self A cell.
 * @param x The cell x offset relative to the grid origin.
 * @param y The cell y offset relative to the grid origin.
 * @param code The glyph codepoint.
 */
void gpf_cell_draw(gp_pixmap *p, const gp_widget_render_ctx *ctx,
                   const struct gpf_cell *self, int x, int y, uint32_t code);

/**
 * @brief Draws a glyph cell with the ink handed to it.
 *
 * The same cell with the ink handed to it instead of resolved: the compose
 * view draws a letter that is not in the font, as it would come out with the
 * accent that is being edited on it.  The codepoint above it still says what
 * the font holds, which is not the same question.
 *
 * @param p The pixmap to draw into.
 * @param ctx The widget render context.
 * @param self A cell.
 * @param x The cell x offset relative to the grid origin.
 * @param y The cell y offset relative to the grid origin.
 * @param code The codepoint the label is drawn for.
 * @param glyph The ink to draw.
 */
void gpf_cell_draw_glyph(gp_pixmap *p, const gp_widget_render_ctx *ctx,
                         const struct gpf_cell *self, int x, int y,
                         uint32_t code, const struct gpf_glyph *glyph);

/**
 * @brief Draws the lines between the cells of a grid.
 *
 * @param p The pixmap to draw into.
 * @param ctx The widget render context.
 * @param self A cell.
 * @param cols The grid width in cells.
 * @param rows The grid height in cells.
 */
void gpf_cell_grid(gp_pixmap *p, const gp_widget_render_ctx *ctx,
                   const struct gpf_cell *self, unsigned int cols,
                   unsigned int rows);

#endif /* GPFORGE_CELL_H */
