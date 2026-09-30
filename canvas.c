/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdlib.h>

#include <gfxprim.h>

#include "edit.h"
#include "gui.h"
#include "canvas.h"

#define MARGIN 2
/* columns of room around the cell, for ink that reaches outside of it */
#define SLACK 2

struct canvas {
	/* the drawn range in glyph coordinates */
	int left, right, top, bottom;
	int zoom;
	/* where the origin and the baseline land in the pixmap */
	int ox, oy;
	/* the room left of and above the grid, where it is numbered */
	int rule_w, rule_h;
};

/* the widest number the rulers show, and the position has two of them */
#define RULE_CHARS 3

static int glyph_advance(void)
{
	struct gpf_resolved res;
	int advance;

	if (gpf_resolve(gui.font, gui.variant, gui.code, &res)) {
		struct gpf_variant *variant = gui.font->variants[gui.variant];

		if (variant && variant->advance)
			return variant->advance;

		return gui.font->meta.em ? gui.font->meta.em : 8;
	}

	advance = res.glyph.advance;

	gpf_resolved_clear(&res);

	return advance;
}

static void geometry(struct canvas *self, gp_pixmap *p)
{
	const gp_widget_render_ctx *ctx = gp_widgets_render_ctx();
	struct gpf_font *font = gui.font;
	int zx, zy;

	self->top = font->meta.ascent + SLACK;
	self->bottom = -font->meta.descent - SLACK;
	self->left = -SLACK;
	self->right = glyph_advance() + SLACK;

	self->rule_w = gp_text_max_width(ctx->font_mono, RULE_CHARS) + 2 * MARGIN;
	self->rule_h = gp_text_height(ctx->font_mono) + 2 * MARGIN;

	zx = ((int)p->w - self->rule_w - MARGIN) /
	     GPF_MAX(1, self->right - self->left);
	zy = ((int)p->h - self->rule_h - MARGIN) /
	     GPF_MAX(1, self->top - self->bottom);

	/*
	 * The canvas starts at whatever multiplier fits the widget and the
	 * spinner takes it from there, so that a dense display can be dialed
	 * up without the glyph starting out microscopic.
	 */
	if (!gui.canvas_zoom)
		gpf_gui_set_canvas_zoom(GPF_MAX(1, GPF_MIN(zx, zy)));

	self->zoom = gui.canvas_zoom;

	/* centred in what the rulers leave */
	self->ox = self->rule_w +
	           ((int)p->w - self->rule_w -
	            (self->right - self->left) * self->zoom) / 2
	           - self->left * self->zoom;
	self->oy = self->rule_h +
	           ((int)p->h - self->rule_h -
	            (self->top - self->bottom) * self->zoom) / 2
	           + self->top * self->zoom;
}

/* a column to a pixmap x */
static int cx(const struct canvas *self, int col)
{
	return self->ox + col * self->zoom;
}

/* a height above the baseline to a pixmap y */
static int cy(const struct canvas *self, int height)
{
	return self->oy - height * self->zoom;
}

/*
 * A pixmap position to a glyph pixel.  Returns non-zero when it is outside of
 * the drawn region.
 */
static int hit(const struct canvas *self, int x, int y, int *col, int *height)
{
	int dx = x - cx(self, self->left);
	int dy = y - cy(self, self->top);

	if (dx < 0 || dy < 0)
		return 1;

	*col = self->left + dx / self->zoom;
	*height = self->top - dy / self->zoom;

	if (*col >= self->right || *height <= self->bottom)
		return 1;

	return 0;
}

/*
 * The canvas is drawn on gpf_gui_back_color() and the grid in the other one of
 * the foreground and the background colors.
 */
static gp_pixel grid_color(const gp_widget_render_ctx *ctx)
{
	if (gui.canvas && gui.canvas->focused)
		return gp_widgets_color(ctx, GP_WIDGETS_COL_BG);

	return gp_widgets_color(ctx, GP_WIDGETS_COL_FG);
}

static void draw_grid(const struct canvas *self, gp_pixmap *p,
                      const gp_widget_render_ctx *ctx)
{
	gp_pixel col = grid_color(ctx);
	int i;

	if (self->zoom < 4)
		return;

	for (i = self->left; i <= self->right; i++)
		gp_vline_xyy(p, cx(self, i), cy(self, self->top), cy(self, self->bottom), col);

	for (i = self->bottom; i <= self->top; i++)
		gp_hline_xxy(p, cx(self, self->left), cx(self, self->right), cy(self, i), col);
}

static void draw_guides(const struct canvas *self, gp_pixmap *p,
                        const gp_widget_render_ctx *ctx)
{
	struct gpf_font *font = gui.font;
	gp_pixel base = gp_widgets_color(ctx, GP_WIDGETS_COL_BLUE);
	gp_pixel adv = gp_widgets_color(ctx, GP_WIDGETS_COL_GREEN);
	/*
	 * The pixel grid is drawn in the secondary colour now, so the metric
	 * references need one of their own or they disappear into it.
	 */
	gp_pixel hint = gp_widgets_color(ctx, GP_WIDGETS_COL_RED);
	int x0 = cx(self, self->left), x1 = cx(self, self->right);
	int y0 = cy(self, self->top), y1 = cy(self, self->bottom);

	if (font->meta.x_height)
		gp_hline_xxy(p, x0, x1, cy(self, font->meta.x_height), hint);

	if (font->meta.cap_height)
		gp_hline_xxy(p, x0, x1, cy(self, font->meta.cap_height), hint);

	gp_hline_xxy(p, x0, x1, cy(self, font->meta.ascent), hint);
	gp_hline_xxy(p, x0, x1, cy(self, -font->meta.descent), hint);

	/* the baseline and the origin */
	gp_hline_xxy(p, x0, x1, cy(self, 0), base);
	gp_vline_xyy(p, cx(self, 0), y0, y1, base);

	gp_vline_xyy(p, cx(self, self->right - SLACK), y0, y1, adv);
}

static void draw_ink(const struct canvas *self, gp_pixmap *p,
                     const gp_widget_render_ctx *ctx,
                     const struct gpf_resolved *res)
{
	const struct gpf_glyph *glyph = &res->glyph;
	gp_pixel col = gp_widgets_color(ctx, GP_WIDGETS_COL_TEXT);
	unsigned int x, y;

	for (y = 0; y < glyph->height; y++) {
		for (x = 0; x < glyph->width; x++) {
			if (!gpf_glyph_pixel(glyph, x, y))
				continue;

			gp_fill_rect_xywh(p, cx(self, glyph->bearing_x + x),
			                  cy(self, glyph->bearing_y - y),
			                  self->zoom, self->zoom, col);
		}
	}
}

/*
 * The pixel the keyboard edits, drawn as a frame so that it is visible on ink
 * and on background alike.  The mouse moves it too, there is one cursor.
 */
static void draw_cursor(const struct canvas *self, gp_pixmap *p,
                        const gp_widget_render_ctx *ctx)
{
	gp_pixel col = gp_widgets_color(ctx, GP_WIDGETS_COL_ALERT);
	int x = cx(self, gui.cur_col), y = cy(self, gui.cur_height);

	if (gui.cur_col < self->left || gui.cur_col >= self->right)
		return;

	if (gui.cur_height <= self->bottom || gui.cur_height > self->top)
		return;

	gp_rect_xywh(p, x, y, self->zoom, self->zoom, col);
}

/*
 * The cursor position in the glyph coordinates, the column right of the origin
 * and the height above the baseline, the same numbers the bearings are in.
 * Drawn in the corner where the rulers meet, padded to a constant width so
 * that the comma stays in place as the cursor moves.  Returns where it ends.
 */
static int draw_pos(gp_pixmap *p, const gp_widget_render_ctx *ctx)
{
	return MARGIN + gp_print(p, ctx->font_mono, MARGIN, MARGIN,
	                         GP_ALIGN_RIGHT | GP_VALIGN_BELOW,
	                         gp_widgets_color(ctx, GP_WIDGETS_COL_TEXT),
	                         gpf_gui_back_color(gui.canvas), "%*i,%-*i",
	                         RULE_CHARS, gui.cur_col,
	                         RULE_CHARS, gui.cur_height);
}

/*
 * Every how many pixels a ruler is numbered: all of them when the numbers fit,
 * every second, fifth, tenth... when they do not.  Zero is always numbered.
 */
static int rule_step(int zoom, int size)
{
	static const int steps[] = {1, 2, 5, 10, 20, 50, 100};
	unsigned int i;

	for (i = 0; i < GP_ARRAY_SIZE(steps); i++) {
		if (steps[i] * zoom >= size)
			return steps[i];
	}

	return steps[GP_ARRAY_SIZE(steps) - 1];
}

/*
 * The columns numbered above the grid and the heights left of it, in the same
 * coordinates as the position.  A column number that would run into the
 * position is left out.
 */
static void draw_rulers(const struct canvas *self, gp_pixmap *p,
                        const gp_widget_render_ctx *ctx, int pos_end)
{
	const gp_text_style *font = ctx->font_mono;
	gp_pixel fg = gp_widgets_color(ctx, GP_WIDGETS_COL_TEXT);
	gp_pixel bg = gpf_gui_back_color(gui.canvas);
	int num_w = gp_text_max_width(font, RULE_CHARS);
	int step, i;

	step = rule_step(self->zoom, num_w + MARGIN);

	for (i = self->left; i < self->right; i++) {
		int x = cx(self, i) + self->zoom / 2;

		if (i % step)
			continue;

		if (x - num_w / 2 < pos_end + MARGIN &&
		    cy(self, self->top) - MARGIN < self->rule_h)
			continue;

		gp_print(p, font, x, cy(self, self->top) - MARGIN,
		         GP_ALIGN_CENTER | GP_VALIGN_ABOVE, fg, bg, "%i", i);
	}

	step = rule_step(self->zoom, gp_text_height(font));

	for (i = self->bottom + 1; i <= self->top; i++) {
		if (i % step)
			continue;

		gp_print(p, font, cx(self, self->left) - MARGIN,
		         cy(self, i) + self->zoom / 2,
		         GP_ALIGN_LEFT | GP_VALIGN_CENTER, fg, bg, "%i", i);
	}
}

void gpf_canvas_draw(gp_widget *self)
{
	gp_pixmap *p = gp_widget_pixmap_get(self);
	const gp_widget_render_ctx *ctx = gp_widgets_render_ctx();
	struct gpf_resolved res;
	struct canvas canvas;
	int have;

	if (!p)
		return;

	gp_fill(p, gpf_gui_back_color(gui.canvas));

	/* no font open, an empty canvas is the whole of it */
	if (!gui.font)
		return;

	geometry(&canvas, p);

	draw_grid(&canvas, p, ctx);
	draw_guides(&canvas, p, ctx);

	have = !gpf_resolve(gui.font, gui.variant, gui.code, &res);

	if (have) {
		draw_ink(&canvas, p, ctx, &res);
		gpf_resolved_clear(&res);
	}

	draw_cursor(&canvas, p, ctx);
	draw_rulers(&canvas, p, ctx, draw_pos(p, ctx));
}

/*
 * Painting goes through gpf_gui_pixel_set(), which is also what copies the
 * pixel into the accented letters when the update box is ticked.
 */
static void paint(int col, int height, int set)
{
	gui.cur_col = col;
	gui.cur_height = height;

	gpf_gui_pixel_set(col, height, set);
}

/* what a press near one of the metric lines grabs */
enum drag {
	DRAG_NONE = 0,
	DRAG_ADVANCE,
	DRAG_ORIGIN,
};

/*
 * The advance and the origin are dragged, which is how the sidebearings are
 * edited.  The band is narrow so that drawing next to either line still draws.
 */
static enum drag drag_at(const struct canvas *canvas, int x)
{
	int band = GPF_MAX(3, canvas->zoom / 4);
	int advance = canvas->right - SLACK;
	struct gpf_variant *variant = gui.font->variants[gui.variant];

	if (abs(x - cx(canvas, 0)) <= band)
		return DRAG_ORIGIN;

	/* in a monospace variant the advance belongs to the variant */
	if (variant && variant->spacing_mono)
		return DRAG_NONE;

	if (abs(x - cx(canvas, advance)) <= band)
		return DRAG_ADVANCE;

	return DRAG_NONE;
}

static void drag_to(int col)
{
	struct gpf_glyph *glyph = gpf_gui_edit_glyph();

	if (!glyph)
		return;

	switch (gui.drag) {
	case DRAG_ADVANCE:
		if (col < 0)
			col = 0;

		if (glyph->advance == col)
			return;

		glyph->advance = col;
	break;
	case DRAG_ORIGIN:
		if (col == gui.drag_col)
			return;

		/* moving the origin right puts the ink left of it */
		gpf_glyph_shift(glyph, gui.drag_col - col, 0);
		gui.drag_col = col;
	break;
	default:
		return;
	}

	gpf_gui_edited();
}

static int press(int x, int y)
{
	gp_pixmap *p = gp_widget_pixmap_get(gui.canvas);
	struct canvas canvas;
	struct gpf_resolved res;
	int col, height, set;

	if (!p)
		return 0;

	geometry(&canvas, p);

	if (hit(&canvas, x, y, &col, &height))
		return 0;

	gui.drag = drag_at(&canvas, x);

	if (gui.drag) {
		gui.drag_col = col;
		gui.in_stroke = 1;

		gpf_gui_edit_begin();

		return 1;
	}

	/*
	 * A press toggles the pixel and the rest of the drag paints that same
	 * value, or a diagonal drag would flip pixels back off behind itself.
	 */
	set = 1;

	if (!gpf_resolve(gui.font, gui.variant, gui.code, &res)) {
		set = !gpf_glyph_pixel_get(&res.glyph, col, height);
		gpf_resolved_clear(&res);
	}

	gpf_gui_edit_begin();

	gui.in_stroke = 1;
	gui.paint_val = set;

	paint(col, height, set);

	return 1;
}

static int motion(int x, int y)
{
	gp_pixmap *p = gp_widget_pixmap_get(gui.canvas);
	struct canvas canvas;
	int col, height;

	if (!p || !gui.in_stroke)
		return 0;

	geometry(&canvas, p);

	if (hit(&canvas, x, y, &col, &height))
		return 0;

	if (gui.drag) {
		drag_to(col);
		return 1;
	}

	paint(col, height, gui.paint_val);

	return 1;
}

/*
 * The keyboard cursor follows the mouse, so that there is only one cursor and
 * the space toggles the pixel the mouse points at.
 */
static int hover(int x, int y)
{
	gp_pixmap *p = gp_widget_pixmap_get(gui.canvas);
	struct canvas canvas;
	int col, height;

	if (!p)
		return 0;

	geometry(&canvas, p);

	if (hit(&canvas, x, y, &col, &height))
		return 0;

	if (col == gui.cur_col && height == gui.cur_height)
		return 1;

	gui.cur_col = col;
	gui.cur_height = height;

	gpf_canvas_draw(gui.canvas);
	gp_widget_redraw(gui.canvas);

	return 1;
}

static int release(void)
{
	if (!gui.in_stroke)
		return 0;

	gui.in_stroke = 0;
	gui.drag = DRAG_NONE;

	gpf_gui_edit_commit();

	return 1;
}

static int toggle_cursor(void)
{
	struct gpf_resolved res;
	int set = 1;

	if (!gpf_resolve(gui.font, gui.variant, gui.code, &res)) {
		set = !gpf_glyph_pixel_get(&res.glyph, gui.cur_col,
		                          gui.cur_height);
		gpf_resolved_clear(&res);
	}

	gpf_gui_edit_begin();

	paint(gui.cur_col, gui.cur_height, set);

	gpf_gui_edit_commit();

	return 1;
}

static int move_cursor(int dx, int dy)
{
	gp_pixmap *p = gp_widget_pixmap_get(gui.canvas);
	struct canvas canvas;

	if (!p)
		return 0;

	geometry(&canvas, p);

	gui.cur_col = GPF_MAX(canvas.left, GPF_MIN(canvas.right - 1,
	                                           gui.cur_col + dx));
	gui.cur_height = GPF_MAX(canvas.bottom + 1,
	                         GPF_MIN(canvas.top, gui.cur_height + dy));

	gpf_canvas_draw(gui.canvas);
	gp_widget_redraw(gui.canvas);

	return 1;
}

/*
 * Shift+arrows move the ink, which is how the bearings are edited, or the
 * accent of a composed glyph.
 */
static int shift_ink(int dx, int dy)
{
	gpf_gui_shift(dx, dy);

	return 1;
}

static int zoom(int wheel)
{
	gpf_gui_set_canvas_zoom(gui.canvas_zoom + wheel);
	gpf_canvas_draw(gui.canvas);
	gp_widget_redraw(gui.canvas);

	return 1;
}

int gpf_canvas_input(gp_event *ev)
{
	int shift, alt;

	if (!gui.font)
		return 0;

	switch (ev->type) {
	case GP_EV_KEY:
		shift = gp_ev_any_key_pressed(ev, GP_KEY_LEFT_SHIFT,
		                              GP_KEY_RIGHT_SHIFT);

		alt = gp_ev_any_key_pressed(ev, GP_KEY_LEFT_ALT,
		                            GP_KEY_RIGHT_ALT);

		if (ev->code == GP_EV_KEY_UP) {
			if (ev->key.key == GP_BTN_LEFT ||
			    ev->key.key == GP_BTN_TOUCH)
				return release();

			return 0;
		}

		if (ev->code != GP_EV_KEY_DOWN)
			return 0;

		int inc = alt ? 5 : 1;

		switch (ev->key.key) {
		case GP_BTN_LEFT:
		case GP_BTN_TOUCH:
			return press(ev->st->cursor_x, ev->st->cursor_y);
		case GP_KEY_SPACE:
			return toggle_cursor();
		case GP_KEY_LEFT:
			return shift ? shift_ink(-inc, 0) : move_cursor(-inc, 0);
		case GP_KEY_RIGHT:
			return shift ? shift_ink(inc, 0) : move_cursor(inc, 0);
		case GP_KEY_UP:
			return shift ? shift_ink(0, inc) : move_cursor(0, inc);
		case GP_KEY_DOWN:
			return shift ? shift_ink(0, -inc) : move_cursor(0, -inc);
		default:
			return 0;
		}
	case GP_EV_REL:
		switch (ev->code) {
		case GP_EV_REL_POS:
			if (gui.in_stroke)
				return motion(ev->st->cursor_x, ev->st->cursor_y);

			return hover(ev->st->cursor_x, ev->st->cursor_y);
		case GP_EV_REL_WHEEL:
			return zoom(ev->val);
		break;
		}
	break;
	}

	return 0;
}
