/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdlib.h>

#include <gfxprim.h>

#include "unicode_blocks.h"
#include "compose.h"
#include "gui.h"
#include "preview.h"
#include "cell.h"

/*
 * Sidebearings are judged in context, never on a single glyph, so the second
 * line puts the glyph between reference glyphs.
 */
static void spacing_str(char *buf, size_t len)
{
	char ch[5];

	ch[gp_to_utf8(gui.code, ch)] = 0;

	snprintf(buf, len, "non%snon  HOH%sHOH", ch, ch);
}

/*
 * A block with no sample text of its own is previewed with its own glyphs,
 * which is all a block of symbols can be shown as anyway.
 */
static const char *block_sample(char *buf, size_t len)
{
	const struct gpf_ucode_block *block = gpf_gui_block();
	struct gpf_resolved res;
	const char *sample;
	uint32_t code;
	size_t pos = 0;

	if (!block)
		return "";

	sample = gpf_ucode_block_sample(block->name);
	if (sample)
		return sample;

	for (code = block->min; code <= block->max; code++) {
		if (pos + 5 >= len)
			break;

		if (gpf_resolve(gui.font, gui.variant, code, &res))
			continue;

		gpf_resolved_clear(&res);

		pos += gp_to_utf8(code, buf + pos);
		buf[pos++] = ' ';
	}

	buf[pos] = 0;

	return buf;
}

/*
 * The line the user typed, drawn under the two the preview picks for itself.
 * It is read off the widget at every paint rather than kept anywhere: the
 * widget is where it lives, and the preview is recomputed either way.
 */
static const char *custom_line(void)
{
	const char *text;

	if (!gui.preview_text)
		return NULL;

	text = gp_widget_tbox_text(gui.preview_text);

	if (!text || !text[0])
		return NULL;

	return text;
}

/*
 * The underline, the strikethrough and the overline, where the font says and
 * at the size the line is drawn at: gp_text_decor_pos() and
 * gp_text_decor_thickness() scale the face's metric by the style, which is
 * what they are for.  The position is the top of the rule, counted up from the
 * baseline.
 */
static void draw_rule(gp_pixmap *p, const gp_text_style *style, int x,
                      int baseline, unsigned int w,
                      enum gp_text_decor_type type, gp_pixel fg)
{
	gp_fill_rect_xywh(p, x, baseline - gp_text_decor_pos(style, type),
	                  w, gp_text_decor_thickness(style, type), fg);
}

static void draw_rules(gp_pixmap *p, const gp_text_style *style, int x, int y,
                       const char *str, gp_pixel fg)
{
	int baseline = y + (int)gp_text_ascent(style);
	unsigned int w = gp_text_wbbox(style, str);

	if (gui.rule_underline) {
		draw_rule(p, style, x, baseline, w, GP_TEXT_DECOR_UNDERLINE,
		          fg);
	}

	if (gui.rule_strike) {
		draw_rule(p, style, x, baseline, w,
		          GP_TEXT_DECOR_STRIKETHROUGH, fg);
	}

	if (gui.rule_overline)
		draw_rule(p, style, x, baseline, w, GP_TEXT_DECOR_OVERLINE, fg);
}

/* the space between the zooms of a line, and between the lines */
#define ZOOM_GAP 2
#define LINE_GAP 6

#define MAX_ZOOM 3

/*
 * A line of the preview, drawn once at each zoom its mask has a bit for, bit
 * zero being 1x.
 */
struct preview_line {
	const char *str;
	unsigned int zooms;
};

static int lines_height(const gp_font_face *face,
                        const struct preview_line *lines, unsigned int cnt)
{
	unsigned int i, z;
	int h = 2;

	for (i = 0; i < cnt; i++) {
		for (z = 1; z <= MAX_ZOOM; z++) {
			if (lines[i].zooms & (1u << (z - 1)))
				h += z * gp_font_height(face) + ZOOM_GAP;
		}

		h += LINE_GAP - ZOOM_GAP;
	}

	/* nothing follows the last line */
	return h - LINE_GAP;
}

/*
 * Every line at 1x and 2x, and at 3x as well when there is room for all of
 * them.  A preview too short for all the 2x lines drops them one at a time:
 * the context line keeps its 2x the longest, since spacing is what is judged
 * at a zoom, then the typed line, then the sample.
 */
static void lines_fit(const gp_font_face *face, struct preview_line *lines,
                      unsigned int cnt, int h)
{
	static const unsigned int prio[] = {1, 2, 0};
	unsigned int i;

	for (i = 0; i < GP_ARRAY_SIZE(prio); i++) {
		if (prio[i] >= cnt)
			continue;

		lines[prio[i]].zooms |= 2;

		if (lines_height(face, lines, cnt) > h)
			lines[prio[i]].zooms &= ~2u;
	}

	for (i = 0; i < cnt; i++) {
		if (!(lines[i].zooms & 2))
			return;
	}

	for (i = 0; i < cnt; i++)
		lines[i].zooms |= 4;

	if (lines_height(face, lines, cnt) <= h)
		return;

	for (i = 0; i < cnt; i++)
		lines[i].zooms &= ~4u;
}

void gpf_preview_draw(gp_widget *self)
{
	gp_pixmap *p = gp_widget_pixmap_get(self);
	const gp_widget_render_ctx *ctx = gp_widgets_render_ctx();
	const gp_font_face *face = gpf_gui_face(gui.variant);
	gp_text_style style = {};
	struct preview_line lines[3];
	const char *custom = custom_line();
	char sample[256], spacing[64];
	unsigned int i, z, cnt = 0;
	gp_pixel fg, bg;
	int y = 2;

	if (!p)
		return;

	fg = gp_widgets_color(ctx, GP_WIDGETS_COL_TEXT);
	bg = gp_widgets_color(ctx, GP_WIDGETS_COL_FG);

	gp_fill(p, bg);

	if (!face)
		return;

	style.font = face;

	spacing_str(spacing, sizeof(spacing));

	/*
	 * The sample follows the block being worked on, the context line the
	 * glyph, and the typed line is whatever the user wants to look at.
	 */
	lines[cnt++] = (struct preview_line){
		.str = block_sample(sample, sizeof(sample)), .zooms = 1};
	lines[cnt++] = (struct preview_line){.str = spacing, .zooms = 1};

	if (custom)
		lines[cnt++] = (struct preview_line){.str = custom, .zooms = 1};

	lines_fit(face, lines, cnt, p->h);

	for (i = 0; i < cnt; i++) {
		for (z = 1; z <= MAX_ZOOM; z++) {
			if (!(lines[i].zooms & (1u << (z - 1))))
				continue;

			style.pixel_xmul = z;
			style.pixel_ymul = z;

			gp_text(p, &style, 2, y, GP_ALIGN_RIGHT | GP_VALIGN_BELOW,
			        fg, bg, lines[i].str);

			draw_rules(p, &style, 2, y, lines[i].str, fg);

			y += gp_text_height(&style) + ZOOM_GAP;
		}

		y += LINE_GAP - ZOOM_GAP;
	}
}

/*
 * The same glyph in all four variants, and the variant selector: the whole
 * format is derivation, so the payoff of an edit is what happens downstream,
 * and clicking the variant you want to look at is the natural way to switch.
 */
static const char *short_names[GPF_VARIANTS] = {
	[GPF_MONO] = "mono",
	[GPF_REGULAR] = "reg",
	[GPF_BOLD_MONO] = "bmono",
	[GPF_BOLD] = "bold",
};

/*
 * The label has to survive a bigger UI font, a TrueType one included, so it
 * falls back to a short name and then to nothing rather than running into the
 * next cell.
 */
static const char *fit_label(const gp_widget_render_ctx *ctx,
                             enum gpf_variant_id id, unsigned int cell_w)
{
	const char *name = gpf_variant_name(id);

	if (gp_text_wbbox(ctx->font, name) <= cell_w)
		return name;

	name = short_names[id];

	if (gp_text_wbbox(ctx->font, name) <= cell_w)
		return name;

	return NULL;
}

static void draw_variant(gp_pixmap *p, const gp_widget_render_ctx *ctx,
                         enum gpf_variant_id id, unsigned int cell_w)
{
	struct gpf_resolved res;
	unsigned int label_h = gp_text_height(ctx->font);
	unsigned int line_h = gui.font->meta.ascent + gui.font->meta.descent;
	unsigned int gx, gy, zoom;
	const char *label;
	int x = id * cell_w;
	int ox, oy;
	gp_pixel bg, fg;

	bg = gp_widgets_color(ctx, id == gui.variant ? GP_WIDGETS_COL_SELECT
	                                             : GP_WIDGETS_COL_FG);
	fg = gp_widgets_color(ctx, GP_WIDGETS_COL_TEXT);

	gp_fill_rect_xywh(p, x, 0, cell_w, p->h, bg);

	label = fit_label(ctx, id, cell_w - 2);

	if (label) {
		/*
		 * The text background has to be the colour that is actually
		 * under it, or an antialiased font fringes against the wrong
		 * one on the selected cell.
		 */
		gp_text(p, ctx->font, x + cell_w/2, 0,
		        GP_ALIGN_CENTER | GP_VALIGN_BELOW, fg, bg, label);
	}

	if (!line_h)
		return;

	zoom = GPF_MAX(1u, (p->h - label_h - 2) / line_h);

	if (gpf_resolve(gui.font, id, gui.code, &res))
		return;

	oy = label_h + 2 + zoom * gui.font->meta.ascent;
	ox = x + ((int)cell_w - zoom * res.glyph.advance) / 2;

	for (gy = 0; gy < res.glyph.height; gy++) {
		for (gx = 0; gx < res.glyph.width; gx++) {
			if (!gpf_glyph_pixel(&res.glyph, gx, gy))
				continue;

			gp_fill_rect_xywh(p,
			        ox + zoom * (res.glyph.bearing_x + (int)gx),
			        oy - zoom * (res.glyph.bearing_y - (int)gy),
			        zoom, zoom, fg);
		}
	}

	gpf_resolved_clear(&res);
}

void gpf_strip_draw(gp_widget *self)
{
	gp_pixmap *p = gp_widget_pixmap_get(self);
	const gp_widget_render_ctx *ctx = gp_widgets_render_ctx();
	unsigned int i, cell_w;

	if (!p)
		return;

	gp_fill(p, gp_widgets_color(ctx, GP_WIDGETS_COL_FG));

	if (!gui.font)
		return;

	cell_w = GPF_MAX(1u, p->w / GPF_VARIANTS);

	for (i = 0; i < GPF_VARIANTS; i++)
		draw_variant(p, ctx, i, cell_w);
}

/*
 * Everything composed from the glyph being edited, in the same cells as the
 * glyph table and at its zoom: fixing a base letter is worth doing because of
 * what it does to these, so they are on the screen while it is being done, and
 * clicking one goes there.
 */
/* the longest list the strip holds: a caron goes on thirty-five letters */
#define GPF_DEPS_MAX 256

/*
 * What the strip shows: the accented forms of the selected letter, or of the
 * letter it is an accented form of — so selecting `i` and selecting `í` show
 * the same list.  Picking one out of the strip then leaves the strip alone,
 * with the glyph you picked marked in it.
 *
 * A mark is not a letter with accented forms, it is the accent itself, and
 * selecting one lists the letters that wear it instead — drawn with the mark
 * as it is being drawn now, since editing a caron is editing every letter that
 * has one and this is where that is seen.
 *
 * Returns non-zero when the list is an accent's letters, which is what is
 * drawn composed rather than as the font has it.
 */
static int deps_codes(uint32_t *codes, unsigned int max, unsigned int *cnt)
{
	*cnt = gpf_decompose_by_accent(gui.code, codes, max);

	if (*cnt)
		return 1;

	*cnt = gpf_decompose_accented(gpf_decompose_base(gui.code), codes, max);

	return 0;
}

/*
 * The letter as it would come out with this accent on it: the base the font
 * has, plus the ink being edited, where gpf_accent_offset() puts it — which is
 * where clicking an empty slot would draw it.  Nothing is stored; the compose
 * view shows what drawing it now would give.
 */
static int compose_preview(uint32_t code, const struct gpf_glyph *accent,
                           struct gpf_glyph *out)
{
	const struct gpf_decomposition *decomp = gpf_decompose(code);
	struct gpf_resolved base;
	int dx, dy, ret;

	if (!decomp)
		return 1;

	if (gpf_draw_base(gui.font, gui.variant, decomp, &base))
		return 1;

	gpf_accent_offset(gui.font, &base.glyph, accent, decomp->place,
	                  &dx, &dy);

	ret = gpf_compose(out, &base.glyph, accent, dx, dy);

	gpf_resolved_clear(&base);

	return ret;
}

static int deps_grid(gp_pixmap *p, struct gpf_cell *cell, unsigned int *cols,
                     unsigned int *rows)
{
	const gp_widget_render_ctx *ctx = gp_widgets_render_ctx();

	if (!p || !gui.font)
		return 1;

	gpf_cell_init(cell, ctx, p->h);

	*cols = GPF_MAX(1u, p->w / cell->w);
	*rows = GPF_MAX(1u, p->h / cell->h);

	gpf_cell_center(cell, p, *cols, *rows);

	return 0;
}

/*
 * The list, and where in it the strip is looking.  The whole list is fetched,
 * since what fits is rarely all of it — a caron goes on thirty-five letters —
 * and the offset is clamped here, so that the drawing and the clicking cannot
 * disagree about which cell is which.
 */
static int deps_list(uint32_t *codes, unsigned int max, unsigned int *cnt,
                     unsigned int visible)
{
	int compose = deps_codes(codes, max, cnt);
	uint32_t anchor = compose ? gui.code : gpf_decompose_base(gui.code);

	/* another letter is another list, and it starts at its beginning */
	if (anchor != gui.deps_anchor) {
		gui.deps_anchor = anchor;
		gui.deps_off = 0;
	}

	if (*cnt <= visible)
		gui.deps_off = 0;
	else if (gui.deps_off > *cnt - visible)
		gui.deps_off = *cnt - visible;

	return compose;
}

/*
 * A wheel step is one cell.  The strip is a list of letters and not a page of
 * codepoints like the table, so it is nudged along rather than turned over;
 * scrolling is browsing and leaves the selection where it is.
 */
static int deps_scroll(int delta)
{
	gp_pixmap *p = gp_widget_pixmap_get(gui.deps);
	uint32_t codes[GPF_DEPS_MAX];
	struct gpf_cell cell;
	unsigned int cols, rows, cnt, visible;
	long off;

	if (deps_grid(p, &cell, &cols, &rows))
		return 0;

	visible = cols * rows;

	deps_list(codes, GPF_DEPS_MAX, &cnt, visible);

	if (cnt <= visible)
		return 0;

	off = (long)gui.deps_off + delta;

	off = GPF_MAX(0l, GPF_MIN(off, (long)(cnt - visible)));

	if ((unsigned int)off == gui.deps_off)
		return 0;

	gui.deps_off = off;

	gpf_deps_draw(gui.deps);
	gp_widget_redraw(gui.deps);

	return 1;
}

void gpf_deps_draw(gp_widget *self)
{
	gp_pixmap *p = gp_widget_pixmap_get(self);
	const gp_widget_render_ctx *ctx = gp_widgets_render_ctx();
	uint32_t codes[GPF_DEPS_MAX];
	struct gpf_cell cell;
	struct gpf_resolved accent;
	unsigned int cols, rows, cnt, shown, i;
	int compose;

	if (p)
		gp_fill(p, gp_widgets_color(ctx, GP_WIDGETS_COL_FG));

	if (deps_grid(p, &cell, &cols, &rows))
		return;

	compose = deps_list(codes, GPF_DEPS_MAX, &cnt, cols * rows);

	shown = GPF_MIN(cnt - gui.deps_off, cols * rows);

	/*
	 * The mark itself, which is the ink the letters are drawn with here.
	 * A mark the variant has no glyph for has nothing to put on them, so
	 * the letters are shown as the font has them instead.
	 */
	if (compose && gpf_resolve(gui.font, gui.variant, gui.code, &accent))
		compose = 0;

	for (i = 0; i < shown; i++) {
		uint32_t code = codes[gui.deps_off + i];
		int x = (i % cols) * cell.w;
		int y = (i / cols) * cell.h;
		struct gpf_glyph glyph = {};

		if (compose && !compose_preview(code, &accent.glyph, &glyph)) {
			gpf_cell_draw_glyph(p, ctx, &cell, x, y, code, &glyph);
			free(glyph.bits);
			continue;
		}

		gpf_cell_draw(p, ctx, &cell, x, y, code);
	}

	if (compose)
		gpf_resolved_clear(&accent);

	if (shown)
		gpf_cell_grid(p, ctx, &cell, cols, (shown + cols - 1) / cols);
}

int gpf_deps_input(gp_event *ev)
{
	gp_pixmap *p = gp_widget_pixmap_get(gui.deps);
	uint32_t codes[GPF_DEPS_MAX];
	struct gpf_cell cell;
	struct gpf_resolved res;
	unsigned int cols, rows, cnt, idx;
	int cell_idx;

	if (!gui.font)
		return 0;

	if (ev->type == GP_EV_REL && ev->code == GP_EV_REL_WHEEL)
		return deps_scroll(ev->val < 0 ? 1 : -1);

	if (ev->type != GP_EV_KEY || ev->code != GP_EV_KEY_DOWN)
		return 0;

	if (ev->key.key != GP_BTN_LEFT && ev->key.key != GP_BTN_TOUCH)
		return 0;

	if (deps_grid(p, &cell, &cols, &rows))
		return 0;

	deps_list(codes, GPF_DEPS_MAX, &cnt, cols * rows);

	cell_idx = gpf_cell_at(&cell, cols, rows, ev->st->cursor_x,
	                       ev->st->cursor_y);
	if (cell_idx < 0)
		return 0;

	idx = cell_idx;

	if (idx >= GPF_MIN(cnt - gui.deps_off, cols * rows))
		return 0;

	idx += gui.deps_off;

	/* the table keeps its page, it is not what was clicked */
	gpf_gui_select_keep(codes[idx]);

	/*
	 * An empty slot is a letter the font has not got, and clicking it is
	 * how it gets one: the glyph is created from the base and the accent.
	 */
	if (gpf_resolve(gui.font, gui.variant, codes[idx], &res))
		gpf_gui_create_glyph(codes[idx]);
	else
		gpf_resolved_clear(&res);

	return 1;
}

int gpf_strip_input(gp_event *ev)
{
	gp_pixmap *p = gp_widget_pixmap_get(gui.strip);
	unsigned int cell_w, id;

	if (!gui.font)
		return 0;

	if (!p || ev->type != GP_EV_KEY || ev->code != GP_EV_KEY_DOWN)
		return 0;

	if (ev->key.key != GP_BTN_LEFT && ev->key.key != GP_BTN_TOUCH)
		return 0;

	cell_w = GPF_MAX(1u, p->w / GPF_VARIANTS);

	id = ev->st->cursor_x / cell_w;

	if (id >= GPF_VARIANTS)
		return 0;

	gpf_gui_set_variant(id);

	return 1;
}
