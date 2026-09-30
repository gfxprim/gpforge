/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file browser.h
 * @brief The glyph table.
 *
 * The glyph browser, shows unicode block at a time, with its full range: the
 * cells a font has no glyph in yet are the ones a block is filled in through,
 * so they have to be there to be drawn into.  Glyphs are drawn in the text
 * colour whatever they were resolved from, the status line is where a glyph's
 * provenance is said out loud.
 *
 * The grid pages rather than scrolls, the way gbdfed does it, a page always
 * starts at a whole number of pages into the block, and paging or arrowing
 * stops at either end of it: the block is changed with the selector alone.
 */
#ifndef GPFORGE_BROWSER_H
#define GPFORGE_BROWSER_H

#include <widgets/gp_widgets.h>

/**
 * @brief Paints the glyph table into its pixmap.
 *
 * Paints the current page of the block being browsed, with the selected
 * glyph highlighted, on gpf_gui_back_color().  Turns to the page with the selection first when
 * gpf_gui::browser_follow asks for it.
 *
 * @param self The browser pixmap widget.
 */
void gpf_browser_draw(gp_widget *self);

/**
 * @brief Handles an input event on the glyph table.
 *
 * The browser owns the glyph selection, so it handles the navigation keys: a
 * click selects, the arrows move the selection, Page Up and Page Down move it
 * by a page and the wheel turns a page without moving it.
 *
 * @param ev An input event.
 * @return Non-zero when the event was handled.
 */
int gpf_browser_input(gp_event *ev);

/**
 * @brief Turns the table to the selected glyph.
 *
 * Pages to the block and the page the selected glyph is on. Called when the
 * selection moves, never from the drawing, or a wheel step would be dragged
 * back to the selection.
 */
void gpf_browser_show_sel(void);

#endif /* GPFORGE_BROWSER_H */
