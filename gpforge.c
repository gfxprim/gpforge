/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/*
 * gpforge, a bitmap font editor.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#include <gfxprim.h>

#include "format.h"
#include "format_bdf.h"
#include "format_c.h"
#include "edit.h"
#include "compose.h"
#include "lint.h"
#include "gui.h"
#include "canvas.h"
#include "browser.h"
#include "preview.h"
#include "dialog_block.h"
#include "dialog_family.h"
#include "dialog_range.h"
#include "dialog_save.h"

struct gpf_gui gui;

gp_app_info app_info = {
	.name = "gpforge",
	.desc = "A bitmap font editor",
	.version = "0.1",
	.license = "GPL-2.0-or-later",
	.authors = (gp_app_info_author []) {
		{.name = "Cyril Hrubis", .email = "metan@ucw.cz", .years = "2026"},
		{}
	}
};

const gp_font_face *gpf_gui_face(enum gpf_variant_id id)
{
	if (!gui.font || id >= GPF_VARIANTS)
		return NULL;

	if (!gui.faces[id])
		gui.faces[id] = gpf_face_build(gui.font, id);

	return gui.faces[id];
}

void gpf_gui_invalidate(void)
{
	unsigned int i;

	for (i = 0; i < GPF_VARIANTS; i++) {
		gpf_face_free(gui.faces[i]);
		gui.faces[i] = NULL;
	}
}

const struct gpf_ucode_block *gpf_gui_blocks(void)
{
	if (!gui.font)
		return NULL;

	if (!gui.block_list)
		gui.block_list = gpf_blocks_build(gui.font);

	return gui.block_list;
}

const struct gpf_ucode_block *gpf_gui_block(void)
{
	const struct gpf_ucode_block *blocks = gpf_gui_blocks();

	if (!blocks || gui.block >= gp_vec_len(blocks))
		return NULL;

	return &blocks[gui.block];
}

/*
 * The metric spinners show the resolved glyph and edit it.  They are the same
 * edit as dragging the lines on the canvas, for when the number is what you
 * know.
 */
static void update_metrics(void)
{
	struct gpf_variant *variant = gui.font ? gui.font->variants[gui.variant]
	                                       : NULL;
	struct gpf_resolved res;
	int advance = 0, bearing_x = 0, bearing_y = 0;

	if (gui.font && !gpf_resolve(gui.font, gui.variant, gui.code, &res)) {
		advance = res.glyph.advance;
		bearing_x = res.glyph.bearing_x;
		bearing_y = res.glyph.bearing_y;

		gpf_resolved_clear(&res);
	}

	if (gui.advance_widget) {
		gp_widget_int_val_set(gui.advance_widget, advance);

		/* in a monospace variant the advance belongs to the variant */
		gp_widget_disabled_set(gui.advance_widget,
		                       variant && variant->spacing_mono);
	}

	if (gui.bearing_x_widget)
		gp_widget_int_val_set(gui.bearing_x_widget, bearing_x);

	if (gui.bearing_y_widget)
		gp_widget_int_val_set(gui.bearing_y_widget, bearing_y);
}

/*
 * How many errors and warnings the whole font has.  Recomputed at every
 * redraw that is not in the middle of a stroke, like everything else the lint
 * says: a whole font is a few milliseconds, and a kept count would be wrong
 * the moment a pixel moves.
 */
static void lint_count_set(gp_widget *label, const char *text)
{
	/* a label sized to its text relayouts on every set, so only on change */
	if (label && strcmp(gp_widget_label_get(label), text))
		gp_widget_label_set(label, text);
}

static void update_lint_count(void)
{
	unsigned int errors, warnings;
	char buf[16];

	if (!gui.font) {
		lint_count_set(gui.lint_errors, "");
		lint_count_set(gui.lint_warnings, "");
		return;
	}

	gpf_lint_count(gui.font, &errors, &warnings);

	snprintf(buf, sizeof(buf), "%u", errors);
	lint_count_set(gui.lint_errors, buf);

	snprintf(buf, sizeof(buf), "%u", warnings);
	lint_count_set(gui.lint_warnings, buf);
}

static void set_lint_label(struct gpf_lint_finding *finding)
{
	if (!gui.lint_label)
		return;

	if (!finding) {
		gp_widget_label_set(gui.lint_label, "");
		gp_widget_label_colors_set(gui.lint_label, GP_WIDGETS_COL_TEXT, GP_WIDGETS_COL_BG);
		return;
	}

	gp_widget_label_printf(gui.lint_label, " U+%04X %s: %s",
	                       gui.code,
	                       gpf_lint_severity_name(finding->sev),
	                       finding->msg);

	switch (finding->sev) {
	case GPF_LINT_ERROR:
		gp_widget_label_colors_set(gui.lint_label, GP_WIDGETS_COL_TEXT, GP_WIDGETS_COL_ALERT);
	break;
	case GPF_LINT_WARN:
		gp_widget_label_colors_set(gui.lint_label, GP_WIDGETS_COL_TEXT, GP_WIDGETS_COL_WARN);
	break;
	}

}

static void update_status(void)
{
	struct gpf_resolved res;
	const char *dirty = gui.dirty ? "*" : " ";
	struct gpf_lint_finding finding;
	char ch[5] = "";

	if (!gui.status)
		return;

	if (!gui.font) {
		update_metrics();
		set_lint_label(NULL);
		gp_widget_label_set(gui.status, "no font open");
		return;
	}

	if (gpf_code_printable(gui.code))
		ch[gp_to_utf8(gui.code, ch)] = 0;

	update_metrics();

	/* what is wrong with this glyph, on a line of its own so it can be read */
	if (gui.lint_label) {
		if (gpf_lint_glyph(gui.font, gui.variant, gui.code, &finding))
			set_lint_label(NULL);
		else
			set_lint_label(&finding);
	}

	if (gpf_resolve(gui.font, gui.variant, gui.code, &res)) {
		gp_widget_label_printf(gui.status, "%sU+%04X '%s'  not in the font",
		                       dirty, gui.code, ch);
		return;
	}

	gp_widget_label_printf(gui.status, "%sU+%04X '%s'  %s  ink %ux%u",
	                       dirty, gui.code, ch, gpf_provenance_name(res.prov),
	                       res.glyph.width, res.glyph.height);

	gpf_resolved_clear(&res);
}

/*
 * The font being edited, or that there is none.
 */
static void update_family(void)
{
	if (!gui.family_label)
		return;

	if (!gui.font) {
		gp_widget_label_set(gui.family_label, "no font");
		return;
	}

	gp_widget_label_printf(gui.family_label, "%s %i",
	                       gui.font->meta.family, gui.font->meta.size);
}

static void button_enable(gp_widget *self)
{
	if (self)
		gp_widget_disabled_set(self, 0);
}

/*
 * Disables a button, handing the focus over first when it is the one that has
 * it.
 *
 * A widget disabled while focused keeps the focus: the library refuses to
 * focus *out* of a disabled widget, so the flag stays set, the container goes
 * on routing clicks to it and the click that should have gone elsewhere is
 * swallowed.  That is what broke edit, undo, redo, undo — the last undo was
 * delivered to the redo button, which had just disabled itself.  The focus
 * goes to the button taking over, or to the canvas, which is where the editing
 * is anyway.
 */
static void button_disable(gp_widget *self, gp_widget *next)
{
	if (!self || gp_widget_disabled_get(self))
		return;

	if (self->focused) {
		if (next && !gp_widget_disabled_get(next))
			gp_widget_focus_set(next);
		else if (gui.canvas)
			gp_widget_focus_set(gui.canvas);
	}

	gp_widget_disabled_set(self, 1);
}

/*
 * The journal is what the undo and redo buttons are: nothing to take back on a
 * font just opened, nothing to put back until something has been taken.  Both
 * are asked of the journal rather than tracked, since it is the thing that
 * knows — and a disabled button that is already disabled costs nothing.
 *
 * Enabling comes first: the button that is about to be disabled hands its
 * focus to the other one, which has to be usable by the time it is offered.
 */
static void update_undo(void)
{
	int undo = gpf_undo_can_undo(gui.undo);
	int redo = gpf_undo_can_redo(gui.undo);

	if (undo)
		button_enable(gui.undo_button);

	if (redo)
		button_enable(gui.redo_button);

	if (!undo)
		button_disable(gui.undo_button, gui.redo_button);

	if (!redo)
		button_disable(gui.redo_button, gui.undo_button);
}

/*
 * There is nothing to export when there is no font open.  The font properties
 * stay enabled: with no font they are how one is started.
 */
static void update_export(void)
{
	if (gui.font)
		button_enable(gui.export_button);
	else
		button_disable(gui.export_button, gui.family_button);
}

/*
 * The codepoint a block starts at, shown in hex next to the selector.  Typing
 * into it is how you get to a block, or a glyph, without spinning to it.
 */
static void update_block_start(void)
{
	const struct gpf_ucode_block *block = gpf_gui_block();

	if (!gui.block_start || !block)
		return;

	gp_widget_tbox_printf(gui.block_start, "%04X", block->min);
}

/*
 * Rebuilds the block list, keeping the position: the block that was being
 * browsed stays selected, on the same page, if it is still on the list.
 *
 * The list is made from which glyphs the font has, so it is rebuilt when a
 * glyph is added or removed, not on every edit.
 */
static void rebuild_blocks(void)
{
	const struct gpf_ucode_block *block = gpf_gui_block();
	uint32_t min = block ? block->min : 0;
	const struct gpf_ucode_block *blocks;
	size_t i;

	gp_vec_free(gui.block_list);
	gui.block_list = NULL;

	blocks = gpf_gui_blocks();
	if (!blocks)
		return;

	gui.block = 0;

	for (i = 0; i < gp_vec_len(blocks); i++) {
		if (blocks[i].min <= min)
			gui.block = i;
	}

	if (blocks[gui.block].min != min)
		gui.browser_page = 0;

	/* the coverage counts are in the selector */
	if (gui.blocks)
		gp_widget_redraw(gui.blocks);

	update_block_start();
}

/*
 * The glyph entry to edit in the current variant.  Editing a glyph that is
 * inherited, emboldened or composed materializes it here first, which is what
 * creating an overlay is; a glyph the font does not have starts empty with the
 * variant's advance.
 */
static struct gpf_glyph *edit_code(enum gpf_variant_id id, uint32_t code)
{
	struct gpf_variant *variant;
	struct gpf_resolved res;
	struct gpf_glyph *glyph;

	if (!gui.font)
		return NULL;

	variant = gpf_font_variant(gui.font, id, 1);

	if (!variant)
		return NULL;

	/*
	 * The journal follows the glyph that is about to change, which is not
	 * always the selected one: a pixel on a composed letter lands on one
	 * of its parts, and it is the part that is edited, marked as changed
	 * and taken back by undo.
	 */
	gpf_undo_retarget(gui.undo, gui.font, id, code);

	glyph = gpf_variant_glyph(variant, code);

	if (glyph && glyph->kind == GPF_GLYPH_INK) {
		/*
		 * Marked here and not when the journal entry is committed: the
		 * entry of the glyph being edited is still pending when the
		 * screen is painted, so a strip of letters edited in one go
		 * went red one cell at a time, the last of them a whole edit
		 * behind.
		 */
		glyph->modified = 1;
		return glyph;
	}

	if (gpf_resolve(gui.font, id, code, &res)) {
		memset(&res, 0, sizeof(res));
		res.glyph.advance = variant->advance ? variant->advance
		                                     : gui.font->meta.em;
	}

	glyph = gpf_variant_glyph_add(variant, code);
	if (!glyph) {
		gpf_resolved_clear(&res);
		return NULL;
	}

	free(glyph->bits);

	glyph->bits = res.glyph.bits;
	glyph->width = res.glyph.width;
	glyph->height = res.glyph.height;
	glyph->advance = res.glyph.advance;
	glyph->bearing_x = res.glyph.bearing_x;
	glyph->bearing_y = res.glyph.bearing_y;
	glyph->kind = GPF_GLYPH_INK;
	glyph->has_advance = 1;
	glyph->has_bearing = 1;

	/* the ink was taken over, not copied */
	res.glyph.bits = NULL;

	rebuild_blocks();

	glyph->modified = 1;

	return glyph;
}

struct gpf_glyph *gpf_gui_edit_code(uint32_t code)
{
	return edit_code(gui.variant, code);
}

struct gpf_glyph *gpf_gui_edit_glyph(void)
{
	return gpf_gui_edit_code(gui.code);
}

/*
 * The accented letters that follow the glyph being edited: the ones the font
 * has, when the update box is ticked.  Editing `Á` does not drag `À` along —
 * only the letter they are both accented forms of does.
 */
unsigned int gpf_gui_accented(uint32_t *codes, unsigned int max)
{
	uint32_t all[256];
	unsigned int i, n, cnt = 0;

	if (!gui.update || !gui.font)
		return 0;

	n = gpf_decompose_accented(gui.code, all, GP_ARRAY_SIZE(all));

	for (i = 0; i < n && cnt < max; i++) {
		struct gpf_resolved res;

		/* a letter the font has not got is not drawn by editing another */
		if (gpf_resolve(gui.font, gui.variant, all[i], &res))
			continue;

		gpf_resolved_clear(&res);

		codes[cnt++] = all[i];
	}

	return cnt;
}

/*
 * A pixel of the glyph being edited, and the same pixel of every accented
 * letter that follows it.  A carbon copy: the letters keep their own accents,
 * and the part of them that is the letter is the part that changes.
 */
void gpf_gui_pixel_set(int col, int height, int set)
{
	struct gpf_glyph *glyph = gpf_gui_edit_glyph();
	uint32_t codes[64];
	unsigned int i, n;

	if (!glyph || gpf_glyph_pixel_set(glyph, col, height, set))
		return;

	n = gpf_gui_accented(codes, GP_ARRAY_SIZE(codes));

	for (i = 0; i < n; i++) {
		struct gpf_glyph *dep = gpf_gui_edit_code(codes[i]);

		if (dep)
			gpf_glyph_pixel_set(dep, col, height, set);
	}

	gpf_gui_edited();
}

static int glyph_eq(const struct gpf_glyph *a, const struct gpf_glyph *b)
{
	if (a->width != b->width || a->height != b->height ||
	    a->advance != b->advance || a->bearing_x != b->bearing_x ||
	    a->bearing_y != b->bearing_y)
		return 0;

	if (!a->width || !a->height)
		return 1;

	return !memcmp(a->bits, b->bits, (size_t)a->width * a->height);
}

/*
 * Whether what the variant would inherit is already the letter composed from
 * its own parts, in which case there is nothing to store.
 */
static int inherits_same(enum gpf_variant_id id,
                         const struct gpf_decomposition *decomp,
                         const struct gpf_glyph *base,
                         const struct gpf_glyph *accent, int dx, int dy)
{
	struct gpf_glyph letter = {0};
	struct gpf_resolved res;
	int ret;

	if (gpf_resolve(gui.font, id, decomp->code, &res))
		return 0;

	ret = !gpf_compose(&letter, base, accent, dx, dy) &&
	      glyph_eq(&letter, &res.glyph);

	free(letter.bits);
	gpf_resolved_clear(&res);

	return ret;
}

/*
 * The parent's ink in its own place is stored as the numbers block saving
 * would turn it into, bold tells an ink entry in regular from a numbers one.
 */
static void store_as_saved(enum gpf_variant_id id, struct gpf_glyph *glyph)
{
	struct gpf_glyph numbers;

	if (gpf_entry_form(gui.font, id, glyph, &numbers) != GPF_ENTRY_METRICS)
		return;

	free(glyph->bits);
	numbers.code = glyph->code;
	numbers.modified = 1;
	*glyph = numbers;
}

/*
 * The letter is drawn once, here, as ink: the base with the accent this
 * font draws on it.  From then on it is an ordinary glyph — nothing composes
 * it again, and editing it edits it.
 */
static void create_glyph(enum gpf_variant_id id,
                         const struct gpf_decomposition *decomp, int derived)
{
	struct gpf_resolved base = {0};
	struct gpf_glyph accent = {0};
	struct gpf_glyph *glyph;
	int dx = 0, dy = 0;

	/* an accent above an i is drawn on the dotless letter */
	if (gpf_draw_base(gui.font, id, decomp, &base))
		return;

	if (!gpf_accent_ink(gui.font, id, decomp, &accent)) {
		gpf_accent_offset(gui.font, &base.glyph, &accent, decomp->place,
		                  &dx, &dy);
	}

	if (derived &&
	    inherits_same(id, decomp, &base.glyph, &accent, dx, dy))
		goto out;

	glyph = edit_code(id, decomp->code);

	/* with no accent to draw this copies the base, which gpf_compose() does */
	if (glyph && !gpf_compose(glyph, &base.glyph, &accent, dx, dy) && derived)
		store_as_saved(id, glyph);
out:
	gpf_resolved_clear(&base);
	free(accent.bits);
}

static int derives_from(enum gpf_variant_id id, enum gpf_variant_id ancestor)
{
	while ((id = gpf_variant_parent(id)) != GPF_NO_PARENT) {
		if (id == ancestor)
			return 1;
	}

	return 0;
}

/*
 * A variant that has its own entry for the base letter, ink or only numbers,
 * draws the accented one from it too.  Left to inherit, the letter would be
 * the one drawn here, with none of the variant's base in it — not even where
 * it sits.  A variant that derives the base whole derives the letter the same
 * way.
 */
static int has_own_base(enum gpf_variant_id id,
                        const struct gpf_decomposition *decomp)
{
	struct gpf_variant *variant = gui.font->variants[id];
	struct gpf_glyph *entry;

	if (!variant)
		return 0;

	/* a letter already drawn there is not drawn over */
	entry = gpf_variant_glyph(variant, decomp->code);
	if (entry && entry->kind != GPF_GLYPH_METRICS)
		return 0;

	/* with no dotless ı the variant's i is what it is drawn from */
	return gpf_variant_glyph(variant, decomp->draw_base) ||
	       gpf_variant_glyph(variant, decomp->base_code);
}

/*
 * The empty slots of the accented letters strip are the letters the font has
 * not got, and clicking one is how a font gets them: which letters it covers
 * is a decision, made one letter at a time.
 */
void gpf_gui_create_glyph(uint32_t code)
{
	const struct gpf_decomposition *decomp = gpf_decompose(code);
	unsigned int id;

	if (!gui.font || !decomp)
		return;

	gpf_gui_edit_begin();

	create_glyph(gui.variant, decomp, 0);

	/* parents come before their children, so each builds on what it inherits */
	for (id = 0; id < GPF_VARIANTS; id++) {
		if (derives_from(id, gui.variant) && has_own_base(id, decomp))
			create_glyph(id, decomp, 1);
	}

	gpf_gui_edited();
	gpf_gui_edit_commit();
}


void gpf_gui_edit_begin(void)
{
	/* with no font open there is nothing to edit and nothing to journal */
	if (!gui.font)
		return;

	/* an edit and whatever it drags along with it are one entry */
	gpf_undo_group_begin(gui.undo);

	gpf_undo_begin(gui.undo, gui.font, gui.variant, gui.code);
}

void gpf_gui_edit_commit(void)
{
	if (!gui.font)
		return;

	gpf_undo_commit(gui.undo, gui.font);

	gpf_undo_group_end(gui.undo);

	/*
	 * Which glyphs count as changed is decided by the journal, and the
	 * journal only knows at the commit — so the last word on the red
	 * codepoints is said here, after the strokes have stopped.
	 */
	gpf_gui_redraw();
}

/*
 * The font has changed, or it has just been written.  The save button is the
 * one thing that says which, so it follows this and nothing else.
 */
void gpf_gui_set_dirty(int dirty)
{
	gui.dirty = dirty;

	if (dirty)
		button_enable(gui.save_button);
	else
		button_disable(gui.save_button, gui.export_button);
}

/*
 * Whether the glyph has changed since the font was written.  The entry that
 * says so is the one the ink comes from, which in a variant that inherits it
 * is the parent's.
 */
int gpf_gui_modified(uint32_t code)
{
	struct gpf_resolved res;
	struct gpf_variant *variant;
	struct gpf_glyph *entry;

	if (!gui.font)
		return 0;

	if (gpf_resolve(gui.font, gui.variant, code, &res))
		return 0;

	variant = gui.font->variants[res.ink_from];
	entry = variant ? gpf_variant_glyph(variant, code) : NULL;

	gpf_resolved_clear(&res);

	return entry && entry->modified;
}

void gpf_gui_edited(void)
{
	gpf_gui_set_dirty(1);

	/* the compiled faces have the old glyph in them */
	gpf_gui_invalidate();

	gpf_gui_redraw();
}

static void redraw(gp_widget *widget, void (*draw)(gp_widget *self))
{
	if (!widget)
		return;

	draw(widget);
	gp_widget_redraw(widget);
}

/*
 * While a stroke is being drawn only the canvas and the accented letters are
 * repainted — that is what is being watched, and the letters have all been
 * changed by then, in one go, before this is called.
 *
 * The glyph table, the variant strip and the sample text wait for the end of
 * the stroke: they are not where the pixel is going, and the table paints a
 * hundred cells while the sample text compiles the whole font into a face.
 * Doing that for every pixel of a drag is what makes the drag feel like it is
 * catching up with the mouse.
 */
void gpf_gui_redraw(void)
{
	/*
	 * Before the stroke test: the update copies commit as they go, so what
	 * the journal holds changes in the middle of a drag as well.
	 */
	update_undo();

	redraw(gui.canvas, gpf_canvas_draw);
	redraw(gui.deps, gpf_deps_draw);

	if (gui.in_stroke)
		return;

	redraw(gui.browser, gpf_browser_draw);
	redraw(gui.strip, gpf_strip_draw);
	redraw(gui.preview, gpf_preview_draw);

	update_status();
	update_lint_count();
}

/*
 * Remembers the selection for the block it is in, so that switching back to
 * the block with the selector puts it back.
 */
static void remember_sel(uint32_t code)
{
	const struct gpf_ucode_block *blocks = gpf_gui_blocks();
	struct gpf_block_sel sel;
	size_t i;
	int idx;

	if (!blocks)
		return;

	idx = gpf_blocks_find(blocks, code);
	if (idx < 0)
		return;

	sel.min = blocks[idx].min;
	sel.code = code;

	for (i = 0; i < gp_vec_len(gui.block_sels); i++) {
		if (gui.block_sels[i].min == sel.min) {
			gui.block_sels[i].code = code;
			return;
		}
	}

	if (!gui.block_sels)
		gui.block_sels = gp_vec_new(0, sizeof(struct gpf_block_sel));

	if (gui.block_sels)
		GP_VEC_APPEND(gui.block_sels, sel);
}

/*
 * The glyph selected last in a block, or its first codepoint when none was.
 * A remembered glyph that is out of the block, which was redefined since, does
 * not count.
 */
static uint32_t remembered_sel(const struct gpf_ucode_block *block)
{
	size_t i;

	for (i = 0; i < gp_vec_len(gui.block_sels); i++) {
		const struct gpf_block_sel *sel = &gui.block_sels[i];

		if (sel->min == block->min && sel->code <= block->max)
			return sel->code;
	}

	return block->min;
}

static void select_code(uint32_t code, int follow)
{
	gui.code = code;

	remember_sel(code);

	if (follow)
		gpf_browser_show_sel();

	gpf_gui_redraw();
}

void gpf_gui_select(uint32_t code)
{
	select_code(code, 1);
}

void gpf_gui_select_keep(uint32_t code)
{
	select_code(code, 0);
}

void gpf_gui_set_block(unsigned int block)
{
	gui.block = block;
	gui.browser_page = 0;

	/* the selector shows the block, whoever changed it */
	if (gui.blocks)
		gp_widget_redraw(gui.blocks);

	update_block_start();

	gpf_gui_redraw();
}

/*
 * Deletes every glyph in a range, in every variant, as one undo entry: it is
 * one action to the user however many glyphs it touches.
 */
static void delete_range(uint32_t min, uint32_t max)
{
	unsigned int i;
	uint32_t code;

	gpf_undo_group_begin(gui.undo);

	for (i = 0; i < GPF_VARIANTS; i++) {
		struct gpf_variant *variant = gui.font->variants[i];

		if (!variant)
			continue;

		for (code = min; code <= max; code++) {
			if (!gpf_variant_glyph(variant, code))
				continue;

			gpf_undo_begin(gui.undo, gui.font, i, code);

			gpf_variant_glyph_del(variant, code);

			gpf_undo_commit(gui.undo, gui.font);
		}
	}

	gpf_undo_group_end(gui.undo);

	gpf_gui_set_dirty(1);

	/* the compiled faces have the deleted glyphs in them */
	gpf_gui_invalidate();

	rebuild_blocks();

	gpf_gui_redraw();
}

/*
 * The block selector shows where in the table we are, so it follows the
 * paging.  The ops read gui.block on every render, so a redraw is all it
 * takes to show a new value.
 */
void gpf_gui_sync_block(unsigned int block)
{
	if (block == gui.block)
		return;

	gui.block = block;

	if (gui.blocks)
		gp_widget_redraw(gui.blocks);

	update_block_start();

	redraw(gui.preview, gpf_preview_draw);
}

void gpf_gui_set_variant(enum gpf_variant_id id)
{
	if (gui.variant == id)
		return;

	gui.variant = id;

	gpf_gui_redraw();
}

void gpf_gui_move_variant(int offset)
{
	int new_variant = gui.variant;

	new_variant += offset;

	if (new_variant < 0)
		new_variant = 0;

	if (new_variant >= GPF_VARIANTS)
		new_variant = GPF_VARIANTS-1;

	gpf_gui_set_variant(new_variant);
}

void gpf_pixmap_resize(gp_widget_event *ev)
{
	gp_widget *self = ev->self;
	gp_pixmap *pixmap = gp_pixmap_alloc(self->w, self->h, ev->ctx->pixel_type);

	gp_pixmap_free(gp_widget_pixmap_set(self, pixmap));
}

static int pixmap_event(gp_widget_event *ev, void (*draw)(gp_widget *self))
{
	switch (ev->type) {
	case GP_WIDGET_EVENT_RESIZE:
		gpf_pixmap_resize(ev);
	break;
	case GP_WIDGET_EVENT_COLOR_SCHEME:
	break;
	/* the library redraws the widget right after this */
	case GP_WIDGET_EVENT_FOCUS:
	break;
	default:
		return 0;
	}

	draw(ev->self);

	return 0;
}

static int canvas_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_INPUT)
		return gpf_canvas_input(ev->input_ev);

	return pixmap_event(ev, gpf_canvas_draw);
}

static int update_on_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	gui.update = gp_widget_bool_get(ev->self);

	return 0;
}

static int deps_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_INPUT)
		return gpf_deps_input(ev->input_ev);

	return pixmap_event(ev, gpf_deps_draw);
}

static int strip_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_INPUT)
		return gpf_strip_input(ev->input_ev);

	return pixmap_event(ev, gpf_strip_draw);
}

static int preview_on_event(gp_widget_event *ev)
{
	return pixmap_event(ev, gpf_preview_draw);
}

/*
 * The preview repaints as the line is typed — that is what it is for, and the
 * face it draws with is already compiled, so a keystroke costs a repaint of
 * one pixmap and nothing else.
 */
static int preview_text_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		redraw(gui.preview, gpf_preview_draw);

	return 0;
}

/*
 * The three rules the preview draws over its text.  They are a property of
 * the preview and not of the font, so they live here and not in the model.
 */
static int rule_on_event(gp_widget_event *ev, int *flag)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	*flag = gp_widget_bool_get(ev->self);

	redraw(gui.preview, gpf_preview_draw);

	return 0;
}

static int rule_under_on_event(gp_widget_event *ev)
{
	return rule_on_event(ev, &gui.rule_underline);
}

static int rule_strike_on_event(gp_widget_event *ev)
{
	return rule_on_event(ev, &gui.rule_strike);
}

static int rule_over_on_event(gp_widget_event *ev)
{
	return rule_on_event(ev, &gui.rule_overline);
}

static int browser_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_INPUT)
		return gpf_browser_input(ev->input_ev);

	/*
	 * The page is a number of pages into the block, and a resize changes
	 * how many cells a page holds, so the old number is somewhere else
	 * now — past the end of the block, even.  Turn to the selection.
	 */
	if (ev->type == GP_WIDGET_EVENT_RESIZE)
		gui.browser_follow = 1;

	return pixmap_event(ev, gpf_browser_draw);
}

/*
 * Jumps to a codepoint typed in hex: to the glyph if the block list has it,
 * and to the nearest block start otherwise.
 */
static int block_start_on_event(gp_widget_event *ev)
{
	const struct gpf_ucode_block *blocks = gpf_gui_blocks();
	unsigned long code;
	size_t i, found = 0;
	char *end;

	switch (ev->type) {
	case GP_WIDGET_EVENT_NEW:
		gp_widget_tbox_filter_set(ev->self, GP_TBOX_FILTER_HEX);
		gp_widget_tbox_clear_on_input(ev->self);
		return 0;
	case GP_WIDGET_EVENT_WIDGET:
		if (ev->sub_type != GP_WIDGET_TBOX_TRIGGER)
			return 0;
	break;
	default:
		return 0;
	}

	if (!blocks || !gp_vec_len(blocks))
		return 0;

	code = strtoul(gp_widget_tbox_text(ev->self), &end, 16);

	if (*end)
		goto out;

	for (i = 0; i < gp_vec_len(blocks); i++) {
		if (blocks[i].min <= code)
			found = i;
	}

	gpf_gui_set_block(found);

	if (code >= blocks[found].min && code <= blocks[found].max)
		gpf_gui_select(code);
out:
	/* whatever was typed, the field goes back to showing the block */
	update_block_start();

	return 0;
}

/*
 * The pixel multiplier the glyphs are drawn with, so that the table stays
 * readable on a dense display.
 */
gp_pixel gpf_gui_back_color(gp_widget *self)
{
	const gp_widget_render_ctx *ctx = gp_widgets_render_ctx();

	if (self && self->focused)
		return gp_widgets_color(ctx, GP_WIDGETS_COL_FG);

	return gp_widgets_color(ctx, GP_WIDGETS_COL_BG);
}

/*
 * The canvas multiplier is fitted to the widget once and then it is the
 * user's, which is why it is set from the drawing as well as from the spinner.
 */
void gpf_gui_set_canvas_zoom(unsigned int zoom)
{
	gui.canvas_zoom = GPF_MAX(1u, zoom);

	if (gui.canvas_zoom_widget) {
		gp_widget_int_val_set(gui.canvas_zoom_widget,
		                      gui.canvas_zoom);
	}
}

static int canvas_zoom_on_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	gui.canvas_zoom = GPF_MAX((int64_t)1, gp_widget_int_val_get(ev->self));

	redraw(gui.canvas, gpf_canvas_draw);

	return 0;
}

static int advance_on_event(gp_widget_event *ev)
{
	struct gpf_glyph *glyph;

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	gpf_gui_edit_begin();

	glyph = gpf_gui_edit_glyph();
	if (!glyph)
		return 0;

	glyph->advance = gp_widget_int_val_get(ev->self);

	gpf_gui_edited();
	gpf_gui_edit_commit();

	return 0;
}

/*
 * A bearing is where the ink sits, so setting one moves the ink.
 */
static int bearing_on_event(gp_widget_event *ev, int horiz)
{
	struct gpf_glyph *glyph;
	int val;

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	val = gp_widget_int_val_get(ev->self);

	gpf_gui_edit_begin();

	glyph = gpf_gui_edit_glyph();
	if (!glyph)
		return 0;

	if (horiz)
		gpf_glyph_shift(glyph, val - glyph->bearing_x, 0);
	else
		gpf_glyph_shift(glyph, 0, val - glyph->bearing_y);

	gpf_gui_edited();
	gpf_gui_edit_commit();

	return 0;
}

static int bearing_x_on_event(gp_widget_event *ev)
{
	return bearing_on_event(ev, 1);
}

static int bearing_y_on_event(gp_widget_event *ev)
{
	return bearing_on_event(ev, 0);
}

static int zoom_on_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	gui.zoom = GPF_MAX((int64_t)1, gp_widget_int_val_get(ev->self));

	/* the page holds a different number of cells now */
	gui.browser_follow = 1;

	gpf_gui_redraw();

	return 0;
}

static int blocks_on_event(gp_widget_event *ev)
{
	const struct gpf_ucode_block *block;

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	gpf_gui_set_block(gp_widget_choice_sel_get(ev->self));

	/* back to where we were in the block, or to its start */
	block = gpf_gui_block();
	if (block)
		gpf_gui_select(remembered_sel(block));

	return 0;
}

/*
 * The block list carries a coverage count, which is also the answer to
 * whether a block is complete enough to export.
 */
/*
 * A part of a block, rather than all of it.
 */
static int glyph_range_del_on_event(gp_widget_event *ev)
{
	const struct gpf_ucode_block *block = gpf_gui_block();
	uint32_t min, max;
	unsigned int cnt;

	if (ev->type != GP_WIDGET_EVENT_WIDGET || !block)
		return 0;

	min = block->min;
	max = block->max;

	/*
	 * The dialog is the confirmation — its button says Delete — and a
	 * second one would have to be opened from inside this one, which the
	 * widget library does not survive.
	 */
	if (gpf_dialog_range("Delete glyphs", &min, &max))
		return 0;

	cnt = gpf_font_range_glyphs(gui.font, min, max);

	if (!cnt)
		return 0;

	delete_range(min, max);

	return 0;
}

/*
 * The letter in every variant, not just the one being edited: the glyph
 * delete button drops the overlay of one variant and the letter falls back to
 * what the others have, this is what gets rid of it.
 */
static int glyph_clear_on_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET || !gui.font)
		return 0;

	if (!gpf_font_range_glyphs(gui.font, gui.code, gui.code))
		return 0;

	delete_range(gui.code, gui.code);

	return 0;
}

static const char *blocks_get_choice(gp_widget *self, size_t idx)
{
	static char bufs[4][64];
	static unsigned int cur;
	char *buf = bufs[cur++ % 4];
	const struct gpf_ucode_block *blocks = gpf_gui_blocks();

	(void)self;

	if (!blocks || idx >= gp_vec_len(blocks))
		return "";

	snprintf(buf, 64, "%s %u/%u", blocks[idx].name, blocks[idx].coverage,
	         gpf_block_glyphs(&blocks[idx]));

	return buf;
}

static size_t blocks_get(gp_widget *self, enum gp_widget_choice_op op)
{
	(void)self;

	switch (op) {
	case GP_WIDGET_CHOICE_OP_SEL:
		return gui.block;
	case GP_WIDGET_CHOICE_OP_CNT:
		return gp_vec_len(gpf_gui_blocks());
	}

	return 0;
}

static void blocks_set(gp_widget *self, size_t sel)
{
	(void)self;

	gui.block = sel;
}

static const gp_widget_choice_ops blocks_ops = {
	.get_choice = blocks_get_choice,
	.get = blocks_get,
	.set = blocks_set,
};

static const gp_widget_choice_desc blocks_desc = {
	.ops = &blocks_ops,
};

/*
 * Adding a block is picking one the font does not have and declaring it: the
 * declaration is what keeps it on the list while it is still empty, which is
 * what starting a new script looks like.
 */
static int block_add_on_event(gp_widget_event *ev)
{
	const struct gpf_ucode_block *picked;
	char name[GPF_NAME_MAX];
	uint32_t min, max;
	int idx;

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	if (!gui.font)
		return 0;

	picked = gpf_dialog_block_add();
	if (!picked)
		return 0;

	/* the pick points into a list that the rebuild below throws away */
	min = picked->min;
	max = picked->max;
	snprintf(name, sizeof(name), "%s", picked->name);

	if (gpf_font_block_add(gui.font, min, max, name))
		return 0;

	rebuild_blocks();

	idx = gpf_blocks_find(gpf_gui_blocks(), min);

	if (idx >= 0)
		gpf_gui_set_block(idx);

	return 0;
}

/*
 * Removing a block removes its glyphs with it, which is the only thing "remove
 * this block" can mean for a block that has any: the block is on the list
 * because of them.
 */
static int block_del_on_event(gp_widget_event *ev)
{
	const struct gpf_ucode_block *block = gpf_gui_block();
	uint32_t min, max;

	if (ev->type != GP_WIDGET_EVENT_WIDGET || !block)
		return 0;

	min = block->min;
	max = block->max;

	if (block->coverage) {
		if (gp_dialog_msg_printf_run(GP_DIALOG_MSG_QUESTION,
		                             "Remove block",
		                             "Delete all %u glyphs in %s?",
		                             block->coverage,
		                             block->name) != GP_DIALOG_YES)
			return 0;

		delete_range(min, max);
	}

	gpf_font_block_del(gui.font, min);

	rebuild_blocks();

	if (gui.code >= min && gui.code <= max) {
		const struct gpf_ucode_block *cur = gpf_gui_block();

		if (cur)
			gpf_gui_select(cur->min);
	}

	gpf_gui_redraw();

	return 0;
}

static int is_dir(const char *path)
{
	struct stat st;

	if (stat(path, &st))
		return 0;

	return S_ISDIR(st.st_mode);
}

static struct gpf_font *load(const char *path, char *err, size_t err_len)
{
	if (is_dir(path))
		return gpf_font_read(path, err, err_len);

	return gpf_bdf_read(path, err, err_len);
}

/*
 * Puts a font in the editor, throwing away everything that was derived from
 * the previous one.  The undo journal goes with it: it holds glyphs of a font
 * that is no longer here.
 */
static void font_set(struct gpf_font *font, const char *path, char *save_path)
{
	gpf_gui_invalidate();

	gp_vec_free(gui.block_list);
	gui.block_list = NULL;

	gpf_font_free(gui.font);
	gpf_undo_free(gui.undo);
	free(gui.save_path);

	gui.font = font;
	gui.path = path;
	gui.save_path = save_path;
	gui.undo = gpf_undo_new();

	gpf_gui_set_dirty(0);

	/* the selections were in the blocks of the previous font */
	gp_vec_free(gui.block_sels);
	gui.block_sels = NULL;

	gui.variant = GPF_MONO;
	gui.code = 'A';
	gui.block = 0;
	gui.browser_page = 0;
	gui.browser_follow = 1;
	gui.canvas_zoom = 0;

	if (gui.blocks)
		gp_widget_redraw(gui.blocks);

	update_family();

	/*
	 * The block selector was laid out for whatever font was open before,
	 * or for none at all, and its width comes from the longest block name
	 * in it.  Ask for the resize or the first block opened into an empty
	 * editor is shown clipped.
	 */
	if (gui.blocks)
		gp_widget_resize(gui.blocks);

	update_block_start();
	update_export();
	gpf_gui_redraw();
}

static void save(void);

/*
 * Editing with no way back is not worth a keystroke, so anything that would
 * lose the journal asks first — and the answer to unsaved work is usually to
 * save it, which is why this is not a yes or no question.
 */
static int confirm_discard(const char *title)
{
	if (!gui.dirty)
		return 1;

	switch (gpf_dialog_save(title)) {
	case GPF_SAVE_SAVE:
		save();
		/* a save that was cancelled cancels what asked for it */
		return !gui.dirty;
	case GPF_SAVE_DISCARD:
		return 1;
	default:
		return 0;
	}
}

static void open_font(void)
{
	gp_dialog_file_opts opts = {
		.flags = GP_DIALOG_OPEN_FILE | GP_DIALOG_OPEN_DIR,
	};
	char err[GPF_ERR_MAX] = "";
	struct gpf_font *font;
	gp_dialog *dialog;
	const char *picked;
	char *path;

	if (!confirm_discard("Open"))
		return;

	dialog = gp_dialog_file_open_new(NULL, &opts);
	if (!dialog)
		return;

	if (gp_dialog_run(dialog) != GP_WIDGET_DIALOG_PATH) {
		gp_dialog_free(dialog);
		return;
	}

	picked = gp_dialog_file_path(dialog);
	path = picked ? strdup(picked) : NULL;

	gp_dialog_free(dialog);

	if (!path)
		return;

	font = load(path, err, sizeof(err));
	if (!font) {
		gp_dialog_msg_printf_run(GP_DIALOG_MSG_ERR, "Open", "%s", err);
		free(path);
		return;
	}

	/* a directory is its own save target, a BDF is not */
	font_set(font, path, is_dir(path) ? strdup(path) : NULL);
}

/*
 * A new font is started from the font properties dialog: the properties are
 * the ones it starts from and accepting them creates it.  It has no directory
 * yet, so the first save asks for one.
 */
static void create_font(void)
{
	struct gpf_family_meta meta;
	struct gpf_author *authors = NULL;
	struct gpf_font *font;
	int advance = 8;

	gpf_family_meta_guess(&meta, NULL, 16, 12, 4, advance);

	if (gpf_dialog_family(&meta, &authors, &advance)) {
		gp_vec_free(authors);
		return;
	}

	font = gpf_font_create(&meta, advance);
	if (!font) {
		gp_vec_free(authors);
		return;
	}

	font->authors = authors;

	font_set(font, "", NULL);
}

static void new_font(void)
{
	if (!confirm_discard("New font"))
		return;

	create_font();
}

static void quit(void)
{
	if (!confirm_discard("Quit"))
		return;

	/*
	 * Asked and answered — the exit path asks again otherwise, since it
	 * cannot tell this from the window being closed behind our back.
	 */
	gpf_gui_set_dirty(0);

	gp_widgets_exit(0);
}

/*
 * A dialog opened while the window is being closed has no event coming: the
 * dialog loop waits before it draws, and the click that closed the window went
 * to the window manager.  One timer that does nothing is enough to make the
 * loop come round once and paint.
 */
static uint32_t wake_up(gp_timer *self)
{
	(void)self;

	return GP_TIMER_STOP;
}

static GP_TIMER_DECLARE(wake_timer, 10, 0, "wake", wake_up, NULL);

/*
 * The window manager's close button and the quit key are handled inside the
 * widget library, which exits without asking the application whether it may:
 * the FREE event arrives once the decision is made, with the backend still up.
 * So this is the last chance to offer to save the work, and there is nothing
 * left to cancel with — the dialog would be lying if it offered it.
 */
static void save_on_exit(void)
{
	static int asked;

	if (!gui.dirty || asked)
		return;

	asked = 1;

	gp_app_timer_start(&wake_timer);

	if (gp_dialog_msg_run(GP_DIALOG_MSG_QUESTION, "Closing",
	                      "The font has unsaved changes, save them?")
	    == GP_DIALOG_YES)
		save();
}

/*
 * Puts the editor where an undone edit happened, so that taking something back
 * shows what came back.
 */
static void goto_edit(enum gpf_variant_id id, uint32_t code)
{
	int block;

	gui.variant = id;

	/* the glyph may have come back, or gone, and its block with it */
	rebuild_blocks();

	block = gpf_blocks_find(gpf_gui_blocks(), code);

	if (block >= 0)
		gpf_gui_set_block(block);

	gpf_gui_invalidate();
	gpf_gui_select(code);
}

/*
 * The family metadata changed.  Every painter reads the metrics from the font
 * at each paint, so the faces are all that has to be thrown away — but the
 * table cells are sized from the ascent and the descent, so a page holds a
 * different number of them and the table has to turn to the selection again.
 */
static void family_changed(void)
{
	update_family();

	gui.browser_follow = 1;

	gpf_gui_edited();
}

static int undo_redo(int redo)
{
	enum gpf_undo_what what;
	enum gpf_variant_id id;
	uint32_t code;

	if (!gui.font)
		return 0;

	if (redo)
		what = gpf_redo(gui.undo, gui.font, &id, &code);
	else
		what = gpf_undo(gui.undo, gui.font, &id, &code);

	switch (what) {
	case GPF_UNDO_NONE:
		return 0;
	case GPF_UNDO_GLYPH:
		goto_edit(id, code);
	break;
	/* the metrics are nowhere in particular, so the editor stays put */
	case GPF_UNDO_FAMILY:
		family_changed();
	break;
	}

	/* back where the file was written there is nothing left to save */
	gpf_gui_set_dirty(!gpf_undo_is_saved(gui.undo));

	return 1;
}

/*
 * The family metadata and the authors, edited in a dialog as a copy and set as
 * one entry of the journal.
 */
static void edit_family(void)
{
	struct gpf_family_meta meta;
	struct gpf_author *authors;

	/* with no font open there is nothing to lose, so nothing is asked */
	if (!gui.font) {
		create_font();
		return;
	}

	meta = gui.font->meta;

	authors = gpf_authors_dup(gui.font->authors);
	if (!authors)
		return;

	if (gpf_dialog_family(&meta, &authors, NULL))
		goto out;

	if (gpf_family_meta_same(&meta, &gui.font->meta) &&
	    gpf_authors_same(authors, gui.font->authors))
		goto out;

	if (gpf_undo_family(gui.undo, gui.font, &meta, authors))
		goto out;

	family_changed();
out:
	gp_vec_free(authors);
}

static void save(void)
{
	char err[GPF_ERR_MAX] = "";
	const char *path;

	if (!gui.font)
		return;

	path = gui.save_path ? gui.save_path : gui.path;

	/*
	 * A font imported from a BDF has no directory of its own yet, so the
	 * first save asks for one.
	 */
	if (!is_dir(path)) {
		gp_dialog *dialog = gp_dialog_file_save_new(NULL, NULL);
		const char *picked;

		if (!dialog)
			return;

		if (gp_dialog_run(dialog) != GP_WIDGET_DIALOG_PATH) {
			gp_dialog_free(dialog);
			return;
		}

		picked = gp_dialog_file_path(dialog);

		free(gui.save_path);
		gui.save_path = picked ? strdup(picked) : NULL;

		gp_dialog_free(dialog);

		if (!gui.save_path)
			return;

		path = gui.save_path;
	}

	if (gpf_font_write(gui.font, path, err, sizeof(err))) {
		gp_dialog_msg_printf_run(GP_DIALOG_MSG_ERR, "Save", "%s", err);
		return;
	}

	/* the file has the smallest form, so the editor should show it too */
	gpf_font_simplify(gui.font);

	/* the overlays it dropped may have been the only glyphs in a block */
	rebuild_blocks();

	gpf_font_clear_modified(gui.font);
	gpf_undo_saved(gui.undo);

	gpf_gui_set_dirty(0);

	gpf_gui_redraw();
}

/*
 * The C export names the symbols and the family after the file it is written
 * to, the way `gpforge-cli c` takes them as arguments: haxor-narrow-18.c is
 * the family gfxprim looks up as "haxor-narrow-18", and its symbols are
 * font_family_haxor_narrow_18 and friends.
 */
static void export_names(const char *path, char *name, size_t name_len,
                         char *id, size_t id_len)
{
	const char *base = strrchr(path, '/');
	const char *dot;
	size_t i;

	base = base ? base + 1 : path;
	dot = strrchr(base, '.');

	snprintf(name, name_len, "%.*s",
	         (int)(dot ? (size_t)(dot - base) : strlen(base)), base);

	snprintf(id, id_len, "%s", name);

	/* the id is a C identifier, which a family name is not */
	for (i = 0; id[i]; i++) {
		if (!isalnum((unsigned char)id[i]))
			id[i] = '_';
	}
}

/*
 * Export, the one thing the editor writes that is not the font: the gfxprim C
 * font, or a BDF for what still wants one.  Which of the two is the name the
 * file is saved under — `.c` is the C font and anything else is a BDF — and
 * not a second question, because a handler that runs two dialogs comes back
 * with the layout in pieces.  That is also why a name the rule cannot answer
 * does not exist: refusing one would be a second dialog for a typo.
 *
 * A BDF holds one face, and the face it holds is the variant being edited:
 * the variant strip is where that is chosen, here as everywhere else.  The C
 * font is the whole family and asks nothing.
 */
static void export_font(void)
{
	char err[GPF_ERR_MAX] = "";
	gp_dialog *dialog;
	const char *picked, *base, *ext;
	char *path;
	FILE *f;
	int ret;

	if (!gui.font)
		return;

	dialog = gp_dialog_file_save_new(NULL, NULL);
	if (!dialog)
		return;

	if (gp_dialog_run(dialog) != GP_WIDGET_DIALOG_PATH) {
		gp_dialog_free(dialog);
		return;
	}

	picked = gp_dialog_file_path(dialog);
	path = picked ? strdup(picked) : NULL;

	gp_dialog_free(dialog);

	if (!path)
		return;

	/* a dot in a directory name is not an extension */
	base = strrchr(path, '/');
	ext = strrchr(base ? base + 1 : path, '.');

	f = fopen(path, "w");
	if (!f) {
		gp_dialog_msg_printf_run(GP_DIALOG_MSG_ERR, "Export", "%s: %s",
		                         path, strerror(errno));
		free(path);
		return;
	}

	if (ext && !strcmp(ext, ".c")) {
		char name[GPF_NAME_MAX], id[GPF_NAME_MAX];

		export_names(path, name, sizeof(name), id, sizeof(id));

		ret = gpf_export_c(gui.font, id, name, f, err, sizeof(err));
	} else {
		ret = gpf_bdf_write(gui.font, gui.variant, f, err, sizeof(err));
	}

	if (fclose(f) && !ret) {
		snprintf(err, sizeof(err), "%s: %s", path, strerror(errno));
		ret = 1;
	}

	if (ret)
		gp_dialog_msg_printf_run(GP_DIALOG_MSG_ERR, "Export", "%s", err);

	free(path);
}

/*
 * Reverting drops the entry in this variant, which is what an overlay is: the
 * glyph goes back to whatever the lattice resolves it to.
 */
static void revert(void)
{
	struct gpf_variant *variant;

	if (!gui.font)
		return;

	variant = gui.font->variants[gui.variant];

	if (!variant || !gpf_variant_glyph(variant, gui.code))
		return;

	gpf_gui_edit_begin();

	gpf_variant_glyph_del(variant, gui.code);

	rebuild_blocks();

	gpf_gui_edited();
	gpf_gui_edit_commit();
}

void gpf_gui_shift(int dx, int dy)
{
	struct gpf_glyph *glyph;
	uint32_t codes[64];
	unsigned int i, n;

	if (!gui.font)
		return;

	gpf_gui_edit_begin();

	glyph = gpf_gui_edit_glyph();

	if (!glyph) {
		gpf_gui_edit_commit();
		return;
	}

	gpf_glyph_shift(glyph, dx, dy);

	/*
	 * The letter moved, so its accented forms move with it — the accent
	 * sits on the letter and goes where it goes.
	 */
	n = gpf_gui_accented(codes, GP_ARRAY_SIZE(codes));

	for (i = 0; i < n; i++) {
		struct gpf_glyph *dep = gpf_gui_edit_code(codes[i]);

		if (dep)
			gpf_glyph_shift(dep, dx, dy);
	}

	gpf_gui_edited();
	gpf_gui_edit_commit();
}

/*
 * The selected glyph as the variant resolves it, so an inherited or a derived
 * glyph is copied as it is seen.  A glyph the font has not got is nothing to
 * copy, and the clipboard is left as it was.
 */
static void copy(void)
{
	struct gpf_resolved res;
	struct gpf_glyph *clip = &gui.clipboard;

	if (!gui.font || gpf_resolve(gui.font, gui.variant, gui.code, &res))
		return;

	if (gpf_glyph_set_bits(clip, res.glyph.bits, res.glyph.width,
	                       res.glyph.height, res.glyph.bearing_x,
	                       res.glyph.bearing_y)) {
		gpf_resolved_clear(&res);
		return;
	}

	clip->advance = res.glyph.advance;
	gui.clipboard_full = 1;

	gpf_resolved_clear(&res);

	button_enable(gui.paste_button);
}

/*
 * Replaces the selected glyph with the copied one, as one undo entry.
 *
 * The accented forms do not follow even with the update box ticked: a whole
 * glyph pasted into them would take their accents away.  A monospace variant
 * keeps its advance, which is not a glyph property there.
 */
static void paste(void)
{
	const struct gpf_glyph *clip = &gui.clipboard;
	struct gpf_glyph *glyph;
	int mono;

	if (!gui.font || !gui.clipboard_full)
		return;

	mono = gui.font->variants[gui.variant] &&
	       gui.font->variants[gui.variant]->spacing_mono;

	gpf_gui_edit_begin();

	glyph = gpf_gui_edit_glyph();

	if (glyph && !gpf_glyph_set_bits(glyph, clip->bits, clip->width,
	                                 clip->height, clip->bearing_x,
	                                 clip->bearing_y)) {
		if (!mono)
			glyph->advance = clip->advance;

		gpf_gui_edited();
	}

	gpf_gui_edit_commit();
}

/*
 * The next glyph the lint has something to say about, in whatever variant it
 * is in: the report is a list you walk rather than a list you read, and one
 * button is the whole of it.  The findings are recomputed on every press, so
 * a glyph that has just been fixed is not offered again.
 */
static void lint_jump(void)
{
	struct gpf_lint_finding *findings;
	size_t i, next = 0;
	int found = 0;

	if (!gui.font)
		return;

	findings = gpf_lint(gui.font);
	if (!findings)
		return;

	for (i = 0; i < gp_vec_len(findings); i++) {
		if (findings[i].id < gui.variant)
			continue;

		if (findings[i].id == gui.variant && findings[i].code <= gui.code)
			continue;

		next = i;
		found = 1;
		break;
	}

	/* past the last one, round to the first */
	if (!found && gp_vec_len(findings))
		found = 1;

	if (!found) {
		if (gui.lint_label)
			gp_widget_label_set(gui.lint_label,
			                    "nothing for the lint to say");

		gp_vec_free(findings);
		return;
	}

	gui.variant = findings[next].id;

	gpf_gui_select(findings[next].code);

	gp_vec_free(findings);
}

static void glyph_command(int key)
{
	struct gpf_glyph *glyph;
	int mono;

	if (!gui.font)
		return;

	mono = gui.font->variants[gui.variant] &&
	       gui.font->variants[gui.variant]->spacing_mono;

	if ((key == GP_KEY_LEFT_BRACE || key == GP_KEY_RIGHT_BRACE) && mono) {
		gp_dialog_msg_run(GP_DIALOG_MSG_INFO, "Advance",
		                  "A monospace variant has one advance for the "
		                  "whole font, it is not a glyph property.");
		return;
	}

	gpf_gui_edit_begin();

	glyph = gpf_gui_edit_glyph();
	if (!glyph)
		return;

	switch (key) {
	case GP_KEY_DELETE:
		gpf_glyph_clear_ink(glyph);
	break;
	case GP_KEY_X:
		gpf_glyph_flip_h(glyph);
	break;
	case GP_KEY_Y:
		gpf_glyph_flip_v(glyph);
	break;
	case GP_KEY_LEFT_BRACE:
		if (glyph->advance > 0)
			glyph->advance--;
	break;
	case GP_KEY_RIGHT_BRACE:
		glyph->advance++;
	break;
	}

	gpf_gui_edited();
	gpf_gui_edit_commit();
}

/*
 * Input the widgets did not take, which is where the editor's own keys live.
 */
static int app_on_event(gp_widget_event *ev)
{
	gp_event *input;
	int ctrl, shift;

	if (ev->type == GP_WIDGET_EVENT_FREE) {
		save_on_exit();
		return 0;
	}

	if (ev->type != GP_WIDGET_EVENT_INPUT)
		return 0;

	input = ev->input_ev;

	if (input->type != GP_EV_KEY || input->code != GP_EV_KEY_DOWN)
		return 0;

	ctrl = gp_ev_any_key_pressed(input, GP_KEY_LEFT_CTRL, GP_KEY_RIGHT_CTRL);
	shift = gp_ev_any_key_pressed(input, GP_KEY_LEFT_SHIFT, GP_KEY_RIGHT_SHIFT);

	if (ctrl) {
		switch (input->key.key) {
		case GP_KEY_Z:
			return undo_redo(shift);
		case GP_KEY_S:
			save();
			return 1;
		case GP_KEY_O:
			open_font();
			return 1;
		case GP_KEY_P:
			edit_family();
			return 1;
		case GP_KEY_N:
			new_font();
			return 1;
		case GP_KEY_Q:
			quit();
			return 1;
		case GP_KEY_C:
			copy();
			return 1;
		case GP_KEY_V:
			paste();
			return 1;
		default:
			return 0;
		}
	}

	switch (input->key.key) {
	case GP_KEY_L:
		lint_jump();
		return 1;
	case GP_KEY_V:
		revert();
		return 1;
	case GP_KEY_DELETE:
	case GP_KEY_X:
	case GP_KEY_Y:
	case GP_KEY_LEFT_BRACE:
	case GP_KEY_RIGHT_BRACE:
		glyph_command(input->key.key);
		return 1;
	default:
		return 0;
	}
}

static int lint_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		lint_jump();

	return 0;
}

static int open_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		open_font();

	return 0;
}

static int save_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		save();

	return 0;
}

static int export_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		export_font();

	return 0;
}

static int new_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		new_font();

	return 0;
}

static int family_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		edit_family();

	return 0;
}

/*
 * The buttons under the canvas.  Everything they do has a key as well, they
 * are there so that the keys can be discovered.
 */
static int undo_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		undo_redo(0);

	return 0;
}

static int redo_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		undo_redo(1);

	return 0;
}

static int glyph_del_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		revert();

	return 0;
}

static int copy_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		copy();

	return 0;
}

static int paste_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		paste();

	return 0;
}

static int move_left_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		gpf_gui_shift(-1, 0);

	return 0;
}

static int move_right_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		gpf_gui_shift(1, 0);

	return 0;
}

static int move_up_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		gpf_gui_shift(0, 1);

	return 0;
}

static int move_down_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		gpf_gui_shift(0, -1);

	return 0;
}

static int flip_h_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		glyph_command(GP_KEY_X);

	return 0;
}

static int flip_v_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		glyph_command(GP_KEY_Y);

	return 0;
}

static const gp_widget_json_addr app_callbacks[] = {
	{.id = "advance_on_event", .on_event = advance_on_event},
	{.id = "bearing_x_on_event", .on_event = bearing_x_on_event},
	{.id = "bearing_y_on_event", .on_event = bearing_y_on_event},
	{.id = "block_add_on_event", .on_event = block_add_on_event},
	{.id = "block_del_on_event", .on_event = block_del_on_event},
	{.id = "block_start_on_event", .on_event = block_start_on_event},
	{.id = "blocks_desc", .addr = (void *)&blocks_desc},
	{.id = "blocks_on_event", .on_event = blocks_on_event},
	{.id = "browser_on_event", .on_event = browser_on_event},
	{.id = "canvas_on_event", .on_event = canvas_on_event},
	{.id = "canvas_zoom_on_event", .on_event = canvas_zoom_on_event},
	{.id = "copy_on_event", .on_event = copy_on_event},
	{.id = "deps_on_event", .on_event = deps_on_event},
	{.id = "export_on_event", .on_event = export_on_event},
	{.id = "family_on_event", .on_event = family_on_event},
	{.id = "flip_h_on_event", .on_event = flip_h_on_event},
	{.id = "flip_v_on_event", .on_event = flip_v_on_event},
	{.id = "glyph_clear_on_event", .on_event = glyph_clear_on_event},
	{.id = "glyph_del_on_event", .on_event = glyph_del_on_event},
	{.id = "glyph_range_del_on_event", .on_event = glyph_range_del_on_event},
	{.id = "lint_on_event", .on_event = lint_on_event},
	{.id = "move_down_on_event", .on_event = move_down_on_event},
	{.id = "move_left_on_event", .on_event = move_left_on_event},
	{.id = "move_right_on_event", .on_event = move_right_on_event},
	{.id = "move_up_on_event", .on_event = move_up_on_event},
	{.id = "new_on_event", .on_event = new_on_event},
	{.id = "open_on_event", .on_event = open_on_event},
	{.id = "paste_on_event", .on_event = paste_on_event},
	{.id = "preview_on_event", .on_event = preview_on_event},
	{.id = "preview_text_on_event", .on_event = preview_text_on_event},
	{.id = "redo_on_event", .on_event = redo_on_event},
	{.id = "rule_over_on_event", .on_event = rule_over_on_event},
	{.id = "rule_strike_on_event", .on_event = rule_strike_on_event},
	{.id = "rule_under_on_event", .on_event = rule_under_on_event},
	{.id = "save_on_event", .on_event = save_on_event},
	{.id = "strip_on_event", .on_event = strip_on_event},
	{.id = "undo_on_event", .on_event = undo_on_event},
	{.id = "update_on_event", .on_event = update_on_event},
	{.id = "zoom_on_event", .on_event = zoom_on_event},
	{}
};

static const gp_widget_json_callbacks callbacks = {
	.addrs = app_callbacks,
};

static gp_widget *widget_by_uid(const char *uid, enum gp_widget_type type)
{
	gp_widget *widget = gp_widget_by_uid(gui.uids, uid, type);

	if (!widget)
		fprintf(stderr, "gpforge: the layout has no '%s'\n", uid);

	return widget;
}

int main(int argc, char *argv[])
{
	char err[GPF_ERR_MAX] = "";
	gp_widget *layout;

	gp_widgets_getopt(&argc, &argv);

	/* starting with nothing open is a way to start: open or new comes next */
	gui.path = argv[0];

	if (gui.path) {
		gui.font = load(gui.path, err, sizeof(err));
		if (!gui.font) {
			fprintf(stderr, "%s\n", err);
			return 1;
		}
	}

	gui.code = 'A';
	gui.zoom = 2;
	gui.browser_follow = 1;

	gui.undo = gpf_undo_new();
	if (!gui.undo) {
		fprintf(stderr, "gpforge: out of memory\n");
		return 1;
	}

	gp_app_on_event_set(app_on_event);
	gp_app_event_unmask(GP_WIDGET_EVENT_INPUT);

	layout = gp_app_layout_load2("gpforge", &callbacks, &gui.uids);
	if (!layout) {
		fprintf(stderr, "gpforge: cannot load the layout\n");
		return 1;
	}

	gui.canvas = widget_by_uid("canvas", GP_WIDGET_PIXMAP);
	gui.browser = widget_by_uid("browser", GP_WIDGET_PIXMAP);
	gui.preview = widget_by_uid("preview", GP_WIDGET_PIXMAP);
	gui.strip = widget_by_uid("strip", GP_WIDGET_PIXMAP);
	gui.deps = widget_by_uid("deps", GP_WIDGET_PIXMAP);
	gui.status = widget_by_uid("status", GP_WIDGET_LABEL);
	gui.blocks = widget_by_uid("blocks", GP_WIDGET_SPINBUTTON);
	gui.block_start = widget_by_uid("block_start", GP_WIDGET_TBOX);
	gui.preview_text = widget_by_uid("preview_text", GP_WIDGET_TBOX);
	gui.canvas_zoom_widget = widget_by_uid("canvas_zoom", GP_WIDGET_SPINNER);
	gui.lint_label = widget_by_uid("lint", GP_WIDGET_LABEL);
	gui.lint_errors = widget_by_uid("lint_errors", GP_WIDGET_LABEL);
	gui.lint_warnings = widget_by_uid("lint_warnings", GP_WIDGET_LABEL);
	gui.save_button = widget_by_uid("save", GP_WIDGET_BUTTON);
	gui.export_button = widget_by_uid("export", GP_WIDGET_BUTTON);
	gui.family_button = widget_by_uid("family_button", GP_WIDGET_BUTTON);
	gui.undo_button = widget_by_uid("undo", GP_WIDGET_BUTTON);
	gui.redo_button = widget_by_uid("redo", GP_WIDGET_BUTTON);
	gui.paste_button = widget_by_uid("paste", GP_WIDGET_BUTTON);
	gui.advance_widget = widget_by_uid("advance", GP_WIDGET_SPINNER);
	gui.bearing_x_widget = widget_by_uid("bearing_x", GP_WIDGET_SPINNER);
	gui.bearing_y_widget = widget_by_uid("bearing_y", GP_WIDGET_SPINNER);

	gp_widget_events_unmask(gui.canvas, GP_WIDGET_EVENT_RESIZE |
	                                    GP_WIDGET_EVENT_COLOR_SCHEME |
	                                    GP_WIDGET_EVENT_INPUT |
	                                    GP_WIDGET_EVENT_FOCUS);
	gp_widget_events_unmask(gui.strip, GP_WIDGET_EVENT_RESIZE |
	                                   GP_WIDGET_EVENT_COLOR_SCHEME |
	                                   GP_WIDGET_EVENT_INPUT |
	                                   GP_WIDGET_EVENT_FOCUS);
	gp_widget_events_unmask(gui.deps, GP_WIDGET_EVENT_RESIZE |
	                                  GP_WIDGET_EVENT_COLOR_SCHEME |
	                                  GP_WIDGET_EVENT_INPUT |
	                                  GP_WIDGET_EVENT_FOCUS);
	gp_widget_events_unmask(gui.preview, GP_WIDGET_EVENT_RESIZE |
	                                     GP_WIDGET_EVENT_COLOR_SCHEME);
	gp_widget_events_unmask(gui.browser, GP_WIDGET_EVENT_RESIZE |
	                                     GP_WIDGET_EVENT_COLOR_SCHEME |
	                                     GP_WIDGET_EVENT_INPUT |
	                                     GP_WIDGET_EVENT_FOCUS);

	gui.family_label = gp_widget_by_uid(gui.uids, "family", GP_WIDGET_LABEL);

	update_family();

	update_status();
	update_lint_count();
	update_block_start();

	/* nothing has been edited yet, so there is nothing to save */
	gpf_gui_set_dirty(0);

	update_export();
	update_undo();

	/* nothing has been copied yet */
	button_disable(gui.paste_button, NULL);

	gp_widgets_main_loop(layout, NULL, 0, NULL);

	return 0;
}
