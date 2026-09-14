/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <utils/gp_utf.h>

#include "format.h"
#include "resolve.h"
#include "gui.h"
#include "cell.h"

void gpf_cell_init(struct gpf_cell *self, const gp_widget_render_ctx *ctx,
                   unsigned int max_h)
{
	struct gpf_font *font = gui.font;
	unsigned int glyph_w = font->meta.em ? (unsigned int)font->meta.em : 8;
	unsigned int label_w = gp_text_wbbox(ctx->font_mono, "0000");
	unsigned int line_h, fixed;
	int top, bottom;

	/* the box is what the ink needs, which is not always ascent + descent */
	gpf_font_ink_extent(font, &top, &bottom);

	self->top = top;

	line_h = GPF_MAX(1, top - bottom + 1);

	/*
	 * The theme's padding, above and below both of the lines that carry a
	 * letter.  A letter drawn against the line above it is hard to read,
	 * and the accent of an `À` reaches above the ascent its font reports —
	 * the padding is more than the descent, so it covers that as well.
	 */
	self->pad = ctx->padd;

	self->label_h = gp_text_height(ctx->font_mono);

	/* the band the reference character gets: its height and the padding */
	self->ref_h = gp_text_height(ctx->font) + 2 * self->pad;

	self->zoom = GPF_MAX(1u, gui.zoom);

	fixed = self->label_h + self->ref_h + 2 * self->pad;

	if (max_h > fixed) {
		unsigned int fits = (max_h - fixed) / line_h;

		self->zoom = GPF_MAX(1u, GPF_MIN(self->zoom, fits));
	}

	self->w = GPF_MAX(self->zoom * glyph_w, label_w) + 2 * self->pad;
	self->h = self->zoom * line_h + fixed;

	self->x0 = 0;
	self->y0 = 0;
}

void gpf_cell_center(struct gpf_cell *self, const gp_pixmap *p,
                     unsigned int cols, unsigned int rows)
{
	/* a grid larger than the pixmap keeps its first cell in view */
	self->x0 = GPF_MAX(0, ((int)p->w - (int)(cols * self->w)) / 2);
	self->y0 = GPF_MAX(0, ((int)p->h - (int)(rows * self->h)) / 2);
}

int gpf_cell_at(const struct gpf_cell *self, unsigned int cols,
                unsigned int rows, gp_coord x, gp_coord y)
{
	x -= self->x0;
	y -= self->y0;

	if (x < 0 || y < 0)
		return -1;

	if ((unsigned int)x / self->w >= cols ||
	    (unsigned int)y / self->h >= rows)
		return -1;

	return (y / self->h) * cols + x / self->w;
}

/*
 * The codepoint is the status line of a cell: red when the glyph has been
 * edited and not yet written, and inverted when this variant carries an entry
 * for it rather than taking it from a parent.  Both are answers to "what have
 * I actually touched", which is what the table is read for.
 */
static void label_colors(const gp_widget_render_ctx *ctx, uint32_t code,
                         int have, gp_pixel *bg, gp_pixel *fg)
{
	struct gpf_variant *variant = gui.font->variants[gui.variant];
	struct gpf_glyph *glyph = variant ? gpf_variant_glyph(variant, code)
	                                  : NULL;

	if (gpf_gui_modified(code)) {
		*bg = gp_widgets_color(ctx, GP_WIDGETS_COL_RED);
		*fg = gp_widgets_color(ctx, GP_WIDGETS_COL_FG);
		return;
	}

	if (!have || !glyph)
		return;

	*bg = gp_widgets_color(ctx, GP_WIDGETS_COL_TEXT);
	*fg = gp_widgets_color(ctx, GP_WIDGETS_COL_FG);
}

/*
 * Whether the widget font has this character, rather than the '?' it hands out
 * for everything it has not got: a table of question marks says nothing, and
 * the line is there to say what the letter should look like.
 */
static int widget_font_has(const gp_text_style *style, uint32_t code)
{
	if (code == '?')
		return 1;

	return gp_glyph_get(style->font, code) != gp_glyph_get(style->font, '?');
}

/*
 * One cell: the codepoint, the reference character, the glyph.  `have` says
 * whether the font has the codepoint, which is what the label is about, and
 * `glyph` is the ink to draw, which is not always the same thing — the compose
 * view draws a letter the font has not got.
 */
static void draw_cell(gp_pixmap *p, const gp_widget_render_ctx *ctx,
                      const struct gpf_cell *self, int x, int y, uint32_t code,
                      const struct gpf_glyph *glyph, int have)
{
	/* the line between the reference character and the glyph */
	int split = y + (int)self->label_h + (int)self->ref_h;
	int oy = split + (int)self->pad + (int)self->zoom * self->top;
	unsigned int gx, gy;
	gp_pixel bg, ink, label_bg, label_fg;
	char buf[8];
	int ox;

	bg = gp_widgets_color(ctx, code == gui.code ? GP_WIDGETS_COL_SELECT
	                                            : GP_WIDGETS_COL_FG);

	ink = gp_widgets_color(ctx, GP_WIDGETS_COL_TEXT);

	gp_fill_rect_xywh(p, x, y, self->w, self->h, bg);

	label_bg = ctx->fg_color;
	label_fg = have ? ink : gp_widgets_color(ctx, GP_WIDGETS_COL_DISABLED);

	label_colors(ctx, code, have, &label_bg, &label_fg);

	gp_fill_rect_xywh(p, x, y, self->w, self->label_h, label_bg);

	snprintf(buf, sizeof(buf), "%04X", code);

	/* only the codepoint says whether the glyph is there at all */
	gp_text(p, ctx->font_mono, x + self->w/2, y,
	        GP_ALIGN_CENTER | GP_VALIGN_BELOW, label_fg, label_bg, buf);

	/*
	 * And under it the same character as the widget font draws it, which
	 * is what the letter is supposed to look like — the only thing in the
	 * editor that does not come from the font being edited.
	 */
	if (gpf_code_printable(code) && widget_font_has(ctx->font, code)) {
		char ch[5];

		ch[gp_to_utf8(code, ch)] = 0;

		gp_text(p, ctx->font, x + self->w/2,
		        y + (int)self->label_h + (int)self->pad,
		        GP_ALIGN_CENTER | GP_VALIGN_BELOW,
		        gp_widgets_color(ctx, GP_WIDGETS_COL_DISABLED), bg, ch);
	}

	/* the line the glyph starts under, so that the two are told apart */
	gp_hline_xxy(p, x, x + (int)self->w - 1, split,
	             gp_widgets_color(ctx, GP_WIDGETS_COL_DISABLED));

	if (!glyph)
		return;

	/* the glyph sits in the middle of the cell, on its advance */
	ox = x + ((int)self->w - (int)self->zoom * glyph->advance) / 2;

	for (gy = 0; gy < glyph->height; gy++) {
		for (gx = 0; gx < glyph->width; gx++) {
			int px = ox + (int)self->zoom *
			         (glyph->bearing_x + (int)gx);
			int py = oy - (int)self->zoom *
			         (glyph->bearing_y - (int)gy);

			if (!gpf_glyph_pixel(glyph, gx, gy))
				continue;

			/*
			 * An accent that reaches above the ascent, or ink
			 * wider than the advance, belongs to this glyph and
			 * not to the cell next to it.  The cell is the box it
			 * gets.
			 */
			if (py <= split || py + (int)self->zoom > y + (int)self->h)
				continue;

			if (px < x || px + (int)self->zoom > x + (int)self->w)
				continue;

			gp_fill_rect_xywh(p, px, py, self->zoom, self->zoom, ink);
		}
	}
}

void gpf_cell_draw(gp_pixmap *p, const gp_widget_render_ctx *ctx,
                   const struct gpf_cell *self, int x, int y, uint32_t code)
{
	struct gpf_resolved res;
	int have = !gpf_resolve(gui.font, gui.variant, code, &res);

	draw_cell(p, ctx, self, self->x0 + x, self->y0 + y, code,
	          have ? &res.glyph : NULL, have);

	if (have)
		gpf_resolved_clear(&res);
}

void gpf_cell_draw_glyph(gp_pixmap *p, const gp_widget_render_ctx *ctx,
                         const struct gpf_cell *self, int x, int y,
                         uint32_t code, const struct gpf_glyph *glyph)
{
	struct gpf_resolved res;
	int have = !gpf_resolve(gui.font, gui.variant, code, &res);

	if (have)
		gpf_resolved_clear(&res);

	draw_cell(p, ctx, self, self->x0 + x, self->y0 + y, code, glyph, have);
}

void gpf_cell_grid(gp_pixmap *p, const gp_widget_render_ctx *ctx,
                   const struct gpf_cell *self, unsigned int cols,
                   unsigned int rows)
{
	gp_pixel col = gp_widgets_color(ctx, GP_WIDGETS_COL_DISABLED);
	unsigned int w = cols * self->w;
	unsigned int h = rows * self->h;
	unsigned int i;

	int x0 = self->x0, y0 = self->y0;

	for (i = 0; i <= cols; i++)
		gp_vline_xyy(p, x0 + i * self->w, y0, y0 + h, col);

	for (i = 0; i <= rows; i++)
		gp_hline_xxy(p, x0, x0 + w, y0 + i * self->h, col);
}
