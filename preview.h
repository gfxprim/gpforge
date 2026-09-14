/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file preview.h
 * @brief The preview, the variant strip and the accented letters strip.
 *
 * The preview and the variant strip both draw through gfxprim's own text
 * renderer with the resolved variant compiled into a gp_font_face, so what is
 * on the screen is what a consumer of the font would render, bearings
 * included.  Neither follows the table's zoom: the preview is there to show
 * the font at its own size, and the strip is a selector.
 *
 * The accented letters strip lists the accented forms of the selected letter,
 * or the letters that wear the selected mark, drawn as table cells.
 */
#ifndef GPFORGE_PREVIEW_H
#define GPFORGE_PREVIEW_H

#include <widgets/gp_widgets.h>

/**
 * @brief Paints the sample text into the preview pixmap.
 *
 * Draws the sample of the block being browsed, the selected glyph between
 * reference glyphs, the line typed into the preview text box, and the
 * underline, strikethrough and overline the check boxes ask for, all in the
 * variant being edited.  Each line is drawn at 1x and 2x, and at 3x as well
 * when the preview has room for all of them.
 *
 * @param self The preview pixmap widget.
 */
void gpf_preview_draw(gp_widget *self);

/**
 * @brief Paints the variant strip.
 *
 * The selected glyph once per variant, as each of them resolves it, with the
 * variant being edited highlighted.
 *
 * @param self The variant strip pixmap widget.
 */
void gpf_strip_draw(gp_widget *self);

/**
 * @brief Handles an input event on the variant strip.
 *
 * The strip is the variant selector, there is no separate widget for it: a
 * click on a cell switches to its variant.
 *
 * @param ev An input event.
 * @return Non-zero when the event was handled.
 */
int gpf_strip_input(gp_event *ev);

/**
 * @brief Paints the accented letters strip.
 *
 * Lists the accented forms of the selected letter, or of the letter the
 * selected glyph is an accented form of.  When a mark is selected it lists the
 * letters that wear it instead, each drawn composed with the mark being
 * edited.
 *
 * @param self The accented letters strip pixmap widget.
 */
void gpf_deps_draw(gp_widget *self);

/**
 * @brief Handles an input event on the accented letters strip.
 *
 * A click selects the glyph without moving the table, and a click on a letter
 * the font has not got creates it from its base and accent.  The wheel
 * scrolls the strip by a cell.
 *
 * @param ev An input event.
 * @return Non-zero when the event was handled.
 */
int gpf_deps_input(gp_event *ev);

#endif /* GPFORGE_PREVIEW_H */
