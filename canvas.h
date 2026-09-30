/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file canvas.h
 * @brief The glyph canvas.
 *
 * There is no bounding box to manage: the canvas is the line box and the
 * glyph's own advance, and the ink box and the bearings are whatever the ink
 * says they are.
 *
 * The drawn region is derived from the metrics and the advance, never from the
 * ink, so that setting a pixel at the edge of the glyph does not move the view
 * out from under the next click.
 */
#ifndef GPFORGE_CANVAS_H
#define GPFORGE_CANVAS_H

#include <widgets/gp_widgets.h>

/**
 * @brief Paints the selected glyph, enlarged, into the canvas pixmap.
 *
 * Draws the pixel grid with its columns and rows numbered, the ink, the metric
 * lines, the cursor and its position.
 * With no font open it fills the background and returns.  The background is
 * the foreground color while the canvas is focused and the background color
 * otherwise, so it is redrawn on a focus change.
 *
 * @param self The canvas pixmap widget.
 */
void gpf_canvas_draw(gp_widget *self);

/**
 * @brief Handles an input event on the canvas.
 *
 * A press starts a stroke, or drags the advance or the origin when it lands on
 * one of their lines, motion continues it and a release commits it as one undo
 * entry.  The arrows move the cursor, and so does the mouse, space toggles the
 * pixel under it, shift and the arrows move the ink, alt makes a step five
 * pixels, and the wheel zooms.
 *
 * @param ev An input event.
 * @return Non-zero when the event was handled.
 */
int gpf_canvas_input(gp_event *ev);

#endif /* GPFORGE_CANVAS_H */
