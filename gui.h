/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file gui.h
 * @brief The editor state and the functions everything in the GUI goes through.
 *
 * The state is one global, gui, and the functions here, implemented in
 * gpforge.c, are the funnels the widgets change it through: selecting, editing
 * and redrawing.  The widgets themselves have
 * headers of their own, see browser.h, canvas.h, preview.h and cell.h, and so
 * do the dialogs.
 *
 * The editor runs with no font open, gpf_gui::font is NULL then, and it is the
 * GUI that checks for it, never the model.
 */
#ifndef GPFORGE_GUI_H
#define GPFORGE_GUI_H

#include <widgets/gp_widgets.h>

#include "font.h"
#include "resolve.h"
#include "face.h"
#include "undo.h"
#include "unicode_blocks.h"

/**
 * @brief The glyph selected last in a block, to go back to when the block is
 *        switched to again.
 */
struct gpf_block_sel {
	/** @brief The block, by its first codepoint: indices move on a rebuild. */
	uint32_t min;
	/** @brief The glyph selected in it. */
	uint32_t code;
};

/**
 * @brief The editor state.
 */
struct gpf_gui {
	/** @brief The font being edited, NULL when there is none. */
	struct gpf_font *font;
	/** @brief The path the font was opened from. */
	const char *path;
	/** @brief Where a font imported from a BDF is saved, once it has been
	 *         asked. */
	char *save_path;

	/** @brief The variant being edited. */
	enum gpf_variant_id variant;
	/** @brief The selected glyph. */
	uint32_t code;
	/** @brief The block list, see gpf_gui_blocks(). */
	struct gpf_ucode_block *block_list;
	/** @brief The index of the block being browsed. */
	unsigned int block;
	/** @brief The glyph selected last in each block, see gpf_gui_select(). */
	struct gpf_block_sel *block_sels;
	/** @brief The pixel multiplier the table glyphs are drawn with. */
	unsigned int zoom;
	/** @brief The same for the glyph on the canvas, 0 until it is fitted. */
	unsigned int canvas_zoom;

	/** @brief The column of the pixel the keyboard edits. */
	int cur_col;
	/** @brief The height of the pixel the keyboard edits. */
	int cur_height;
	/** @brief A mouse stroke is in progress. */
	int in_stroke;
	/** @brief The value the stroke paints. */
	int paint_val;
	/** @brief A metric being dragged instead, see canvas.c. */
	int drag;
	/** @brief The column the drag is at. */
	int drag_col;

	/** @brief The journal. */
	struct gpf_undo *undo;
	/** @brief The font has changed since it was last written. */
	int dirty;
	/** @brief Copy the edits of a letter into its accented forms. */
	int update;
	/** @brief The page the browser is at; it pages rather than scrolls. */
	unsigned int browser_page;
	/** @brief The first cell of the accented letters strip, which scrolls. */
	unsigned int deps_off;
	/** @brief The letter or the mark the strip lists, so that the offset
	 *         resets. */
	uint32_t deps_anchor;
	/** @brief Turn to the page with the selection on the next draw. */
	int browser_follow;

	/**
	 * @brief The glyph copied last, the ink and the metrics.
	 *
	 * Kept across selections, variants and fonts, pasting it is how a
	 * glyph is started from another one.
	 */
	struct gpf_glyph clipboard;
	/** @brief Something was copied, a space copies no ink. */
	int clipboard_full;

	/** @brief The widget uids of the layout. */
	gp_htable *uids;

	/** @brief The glyph canvas, see canvas.h. */
	gp_widget *canvas;
	/** @brief The glyph table, see browser.h. */
	gp_widget *browser;
	/** @brief The sample text, see preview.h. */
	gp_widget *preview;
	/** @brief The variant strip, see preview.h. */
	gp_widget *strip;
	/** @brief The status line, what the selected glyph resolves to. */
	gp_widget *status;
	/** @brief The block selector. */
	gp_widget *blocks;
	/** @brief The canvas zoom spinner. */
	gp_widget *canvas_zoom_widget;
	/** @brief The family name and size. */
	gp_widget *family_label;
	/** @brief The accented letters strip, see preview.h. */
	gp_widget *deps;
	/** @brief The glyph metrics, editable. */
	gp_widget *advance_widget;
	gp_widget *bearing_x_widget;
	gp_widget *bearing_y_widget;
	/** @brief The block's first codepoint, in hex, editable. */
	gp_widget *block_start;
	/** @brief The line the user types to see it in the preview. */
	gp_widget *preview_text;
	/** @brief Which of the three rules the preview draws over its text. */
	int rule_underline;
	int rule_strike;
	int rule_overline;
	/** @brief Enabled only when there is something to save. */
	gp_widget *save_button;
	/** @brief Enabled only when there is a font to export. */
	gp_widget *export_button;
	/** @brief The same, for the font properties. */
	gp_widget *family_button;
	/** @brief Enabled only when the journal has something to take back. */
	gp_widget *undo_button;
	gp_widget *redo_button;
	/** @brief Enabled once there is something to paste. */
	gp_widget *paste_button;
	/** @brief What the lint says about the glyph being edited. */
	gp_widget *lint_label;
	/** @brief And how much it says about the whole font, next to its
	 *         button. */
	gp_widget *lint_errors;
	gp_widget *lint_warnings;

	/** @brief The compiled faces, built lazily, one per variant. */
	gp_font_face *faces[GPF_VARIANTS];
};

/** @brief The editor state. */
extern struct gpf_gui gui;

/**
 * @brief The resolved variant compiled into a gfxprim face.
 *
 * Built on demand and kept until gpf_gui_invalidate().
 *
 * @param id A variant.
 * @return The face, NULL when there is no font open.
 */
const gp_font_face *gpf_gui_face(enum gpf_variant_id id);

/**
 * @brief Throws the compiled faces away.
 *
 * Called when the font changes.  The block list is kept, it only changes with
 * the set of glyphs the font has.
 */
void gpf_gui_invalidate(void);

/**
 * @brief Selects a glyph and turns the table to it.
 *
 * @param code A codepoint.
 */
void gpf_gui_select(uint32_t code);

/**
 * @brief Selects a glyph without moving the table to it.
 *
 * Picking one out of the accented letters strip must not throw away the page
 * you were working on.
 *
 * @param code A codepoint.
 */
void gpf_gui_select_keep(uint32_t code);

/**
 * @brief Creates a glyph the font has not got.
 *
 * Draws it once, as ink: its base with the accent this font draws on it.
 * From then on it is an ordinary glyph.  Clicking an empty slot of the
 * accented letters strip is what asks for it.
 *
 * @param code A codepoint of an accented letter.
 */
void gpf_gui_create_glyph(uint32_t code);

/**
 * @brief Sets or clears a pixel of the glyph being edited.
 *
 * The same pixel of the accented letters that follow it is set too, a carbon
 * copy, see gpf_gui_accented().  This is what every pixel edit goes through.
 *
 * @param col A column counted from the glyph origin.
 * @param height A height from the baseline, positive up.
 * @param set A value to set the pixel to, 0 or 1.
 */
void gpf_gui_pixel_set(int col, int height, int set);

/**
 * @brief The accented letters that follow the glyph being edited.
 *
 * The ones the font has, when the update box is ticked; editing `Á` does not
 * drag `À` along, only the letter they are both accented forms of does.
 *
 * @param codes An array to fill in.
 * @param max The size of the array.
 * @return The number of codepoints, zero when the update box is not ticked.
 */
unsigned int gpf_gui_accented(uint32_t *codes, unsigned int max);

/**
 * @brief Selects a block and turns the table to its first page.
 *
 * @param block An index into the block list.
 */
void gpf_gui_set_block(unsigned int block);

/**
 * @brief The color a widget is drawn on.
 *
 * The foreground color while the widget is focused, which is when the keys
 * work on it, and the background color otherwise.
 *
 * @param self A widget.
 * @return A pixel value.
 */
gp_pixel gpf_gui_back_color(gp_widget *self);

/**
 * @brief Sets the canvas zoom and the spinner that shows it.
 *
 * @param zoom The pixel multiplier, at least one.
 */
void gpf_gui_set_canvas_zoom(unsigned int zoom);

/**
 * @brief Makes the block selector follow the table.
 *
 * Unlike gpf_gui_set_block() it leaves the page alone, it is called when the
 * selection moved into another block.
 *
 * @param block An index into the block list.
 */
void gpf_gui_sync_block(unsigned int block);

/**
 * @brief Switches the variant being edited.
 *
 * @param id A variant.
 */
void gpf_gui_set_variant(enum gpf_variant_id id);

/**
 * @brief Moves the current variant in the variant ordering.
 *
 * @param offset How much the variant should be moved.
 */
void gpf_gui_move_variant(int offset);

/**
 * @brief Repaints everything that shows the font.
 *
 * During a stroke only the canvas and the accented letters strip are
 * repainted, the rest waits for the commit.
 */
void gpf_gui_redraw(void);

/**
 * @brief The block list, built on demand.
 *
 * @return The list, do not free it; NULL when there is no font open.
 */
const struct gpf_ucode_block *gpf_gui_blocks(void);

/**
 * @brief The block being browsed.
 *
 * @return The block, do not free it; NULL when there is no font open.
 */
const struct gpf_ucode_block *gpf_gui_block(void);

/**
 * @brief Marks the font as changed or as written.
 *
 * The save button follows this and nothing else.
 *
 * @param dirty Non-zero when the font has changed since it was last written.
 */
void gpf_gui_set_dirty(int dirty);

/**
 * @brief Whether the glyph has changed since the font was written.
 *
 * The entry that says so is the one the ink comes from, which in a variant
 * that inherits it is the parent's.
 *
 * @param code A codepoint.
 * @return Non-zero when the glyph was modified.
 */
int gpf_gui_modified(uint32_t code);

/**
 * @brief The glyph entry to edit in the current variant.
 *
 * An inherited or derived glyph is materialized here, which is what creating
 * an overlay is, and a glyph the font does not have yet starts empty.
 *
 * @return The entry, NULL when there is no font open.
 */
struct gpf_glyph *gpf_gui_edit_glyph(void);

/**
 * @brief The glyph entry to edit, for any glyph.
 *
 * The same as gpf_gui_edit_glyph(), and it retargets the pending journal
 * entry, so that an edit that lands on another glyph is undoable and marked
 * as modified.
 *
 * @param code A codepoint.
 * @return The entry, NULL when there is no font open.
 */
struct gpf_glyph *gpf_gui_edit_code(uint32_t code);

/**
 * @brief Moves the ink of the glyph being edited.
 *
 * The accented letters that follow it move along with it.
 *
 * @param dx An x offset, positive right.
 * @param dy An y offset, positive up.
 */
void gpf_gui_shift(int dx, int dy);

/**
 * @brief Starts an edit, one undo entry.
 *
 * Journals nothing with no font open.
 */
void gpf_gui_edit_begin(void);

/**
 * @brief Commits the edit started by gpf_gui_edit_begin() and repaints.
 */
void gpf_gui_edit_commit(void);

/**
 * @brief The font changed.
 *
 * The faces are stale, the display is stale, the file is dirty.
 */
void gpf_gui_edited(void);

/**
 * @brief Allocates the backing pixmap on a resize.
 *
 * The buffered pixmap mode.
 *
 * @param ev A resize event.
 */
void gpf_pixmap_resize(gp_widget_event *ev);

#endif /* GPFORGE_GUI_H */
