/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <gfxprim.h>

#include "gui.h"
#include "browser.h"
#include "cell.h"

struct grid {
	struct gpf_cell cell;
	unsigned int cols, rows;
	unsigned int page_cells;
	/* the block being shown and where we are in it */
	const struct gpf_ucode_block *block;
	uint32_t cnt;
	uint32_t first;
};

static int geometry(struct grid *self, gp_pixmap *p)
{
	const gp_widget_render_ctx *ctx = gp_widgets_render_ctx();

	self->block = gpf_gui_block();
	if (!self->block)
		return 1;

	gpf_cell_init(&self->cell, ctx, 0);

	self->cols = GPF_MAX(1u, p->w / self->cell.w);
	self->rows = GPF_MAX(1u, p->h / self->cell.h);
	self->page_cells = self->cols * self->rows;

	gpf_cell_center(&self->cell, p, self->cols, self->rows);

	self->cnt = gpf_block_glyphs(self->block);
	self->first = gui.browser_page * self->page_cells;

	return 0;
}

void gpf_browser_show_sel(void)
{
	gp_pixmap *p = gp_widget_pixmap_get(gui.browser);
	const struct gpf_ucode_block *blocks = gpf_gui_blocks();
	struct grid grid;
	int idx;

	if (!p || !blocks)
		return;

	idx = gpf_blocks_find(blocks, gui.code);

	if (idx >= 0)
		gpf_gui_sync_block(idx);

	if (geometry(&grid, p))
		return;

	if (gui.code < grid.block->min || gui.code > grid.block->max)
		return;

	gui.browser_page = (gui.code - grid.block->min) / grid.page_cells;
}

void gpf_browser_draw(gp_widget *self)
{
	gp_pixmap *p = gp_widget_pixmap_get(self);
	const gp_widget_render_ctx *ctx = gp_widgets_render_ctx();
	struct grid grid;
	unsigned int cell;

	if (!p)
		return;

	gp_fill(p, gp_widgets_color(ctx, GP_WIDGETS_COL_FG));

	if (!gui.font)
		return;

	if (gui.browser_follow) {
		gui.browser_follow = 0;
		gpf_browser_show_sel();
	}

	if (geometry(&grid, p))
		return;

	for (cell = 0; cell < grid.page_cells; cell++) {
		uint32_t idx = grid.first + cell;

		if (idx >= grid.cnt)
			break;

		gpf_cell_draw(p, ctx, &grid.cell,
		              (cell % grid.cols) * grid.cell.w,
		              (cell / grid.cols) * grid.cell.h,
		              grid.block->min + idx);
	}

	gpf_cell_grid(p, ctx, &grid.cell, grid.cols,
	              (cell + grid.cols - 1) / grid.cols);
}

static void redraw_browser(void)
{
	gpf_browser_draw(gui.browser);
	gp_widget_redraw(gui.browser);
}

/*
 * Moves the selection inside the block being browsed.  It stops at either end
 * of it: the block is changed with the selector, never by arrowing past its
 * edge.
 */
static void move_sel(int delta)
{
	gp_pixmap *p = gp_widget_pixmap_get(gui.browser);
	struct grid grid;
	long idx;

	if (!p || geometry(&grid, p))
		return;

	/* the selection can be in another block, jump into this one first */
	if (gui.code < grid.block->min || gui.code > grid.block->max)
		idx = delta > 0 ? 0 : (long)grid.cnt - 1;
	else
		idx = (long)(gui.code - grid.block->min) + delta;

	if (idx < 0)
		idx = 0;

	if (idx >= (long)grid.cnt)
		idx = (long)grid.cnt - 1;

	gpf_gui_select(grid.block->min + idx);
}

/*
 * A wheel step is a page of the block being browsed.  Paging stops at either
 * end of it, the block is changed with the selector.  Paging is browsing: the
 * selection stays where it is.
 */
static void turn_page(int delta)
{
	gp_pixmap *p = gp_widget_pixmap_get(gui.browser);
	struct grid grid;
	long page = (long)gui.browser_page + delta;
	long pages;

	if (!p || geometry(&grid, p))
		return;

	pages = (grid.cnt + grid.page_cells - 1) / grid.page_cells;

	if (page < 0 || page >= pages)
		return;

	gui.browser_page = page;

	redraw_browser();
}

static void select_at(gp_coord x, gp_coord y)
{
	gp_pixmap *p = gp_widget_pixmap_get(gui.browser);
	struct grid grid;
	int cell;
	uint32_t idx;

	if (!p || geometry(&grid, p))
		return;

	cell = gpf_cell_at(&grid.cell, grid.cols, grid.rows, x, y);
	if (cell < 0)
		return;

	idx = grid.first + cell;

	if (idx < grid.cnt)
		gpf_gui_select(grid.block->min + idx);
}

int gpf_browser_input(gp_event *ev)
{
	gp_pixmap *p = gp_widget_pixmap_get(gui.browser);
	struct grid grid;
	unsigned int cols = 16, page = 128;

	if (!gui.font)
		return 0;

	if (ev->type == GP_EV_REL && ev->code == GP_EV_REL_WHEEL) {
		turn_page(ev->val < 0 ? 1 : -1);
		return 1;
	}

	if (ev->type != GP_EV_KEY || ev->code != GP_EV_KEY_DOWN)
		return 0;

	if (p && !geometry(&grid, p)) {
		cols = grid.cols;
		page = grid.page_cells;
	}

	switch (ev->key.key) {
	case GP_BTN_LEFT:
	case GP_BTN_TOUCH:
		select_at(ev->st->cursor_x, ev->st->cursor_y);
		return 1;
	case GP_KEY_LEFT:
		move_sel(-1);
		return 1;
	case GP_KEY_RIGHT:
		move_sel(1);
		return 1;
	case GP_KEY_UP:
		move_sel(-cols);
		return 1;
	case GP_KEY_DOWN:
		move_sel(cols);
		return 1;
	case GP_KEY_PAGE_UP:
		move_sel(-page);
		return 1;
	case GP_KEY_PAGE_DOWN:
		move_sel(page);
		return 1;
	default:
		return 0;
	}
}
