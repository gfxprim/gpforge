/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/*
 * Editing and the undo journal.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font.h"
#include "edit.h"
#include "undo.h"
#include "compose.h"
#include "resolve.h"
#include "test_util.h"

static const char *rows[] = {
	".##.",
	"#..#",
	"####",
	NULL,
};

static struct gpf_glyph *build_glyph(struct gpf_font **font,
                                     struct gpf_variant **mono)
{
	struct gpf_glyph *glyph;

	*font = gpf_font_new();
	*mono = gpf_font_variant(*font, GPF_MONO, 1);

	glyph = add_ink(*mono, 'A', rows, 1, 5, 6);

	return glyph;
}

/*
 * The ink box grows around a pixel set outside of it and shrinks back when it
 * is cleared, and nothing but the ink decides where it is.
 */
static void test_pixel(void)
{
	struct gpf_font *font;
	struct gpf_variant *mono;
	struct gpf_glyph *glyph = build_glyph(&font, &mono);

	CHECK(glyph->width == 4 && glyph->height == 3, "the ink box is %ux%u",
	      glyph->width, glyph->height);
	CHECK(glyph->bearing_x == 1 && glyph->bearing_y == 5,
	      "the bearings are %i,%i", glyph->bearing_x, glyph->bearing_y);

	CHECK(gpf_glyph_pixel_get(glyph, 1, 3), "the corner pixel is not set");
	CHECK(!gpf_glyph_pixel_get(glyph, 0, 5), "a pixel outside the ink is set");

	/* left of and above the ink */
	gpf_glyph_pixel_set(glyph, 0, 6, 1);

	CHECK(glyph->bearing_x == 0 && glyph->bearing_y == 6,
	      "the box did not grow, bearings are %i,%i", glyph->bearing_x,
	      glyph->bearing_y);
	CHECK(glyph->width == 5 && glyph->height == 4, "the box is %ux%u",
	      glyph->width, glyph->height);
	CHECK(gpf_glyph_pixel_get(glyph, 0, 6), "the new pixel is not set");
	CHECK(gpf_glyph_pixel_get(glyph, 1, 3), "the old ink moved");

	/* and back */
	gpf_glyph_pixel_set(glyph, 0, 6, 0);

	CHECK(glyph->bearing_x == 1 && glyph->bearing_y == 5,
	      "the box did not shrink back, bearings are %i,%i",
	      glyph->bearing_x, glyph->bearing_y);
	CHECK(glyph->width == 4 && glyph->height == 3,
	      "the box did not shrink back, it is %ux%u", glyph->width,
	      glyph->height);

	gpf_font_free(font);
}

static void test_shift_and_flip(void)
{
	struct gpf_font *font;
	struct gpf_variant *mono;
	struct gpf_glyph *glyph = build_glyph(&font, &mono);

	static const char *flipped[] = {
		".##.",
		"#..#",
		"####",
		NULL,
	};
	static const char *upside_down[] = {
		"####",
		"#..#",
		".##.",
		NULL,
	};

	gpf_glyph_shift(glyph, -1, 2);

	CHECK(glyph->bearing_x == 0 && glyph->bearing_y == 7,
	      "shift moved to %i,%i", glyph->bearing_x, glyph->bearing_y);
	CHECK(ink_eq(glyph, rows), "shift changed the ink");

	gpf_glyph_shift(glyph, 1, -2);

	/* the shape is symmetric, only the bearing moves */
	gpf_glyph_flip_h(glyph);

	CHECK(ink_eq(glyph, flipped), "flip_h changed a symmetric shape");
	CHECK(glyph->bearing_x == 6 - 1 - 4,
	      "flip_h put the ink at %i, expected %i", glyph->bearing_x,
	      6 - 1 - 4);

	gpf_glyph_flip_v(glyph);

	CHECK(ink_eq(glyph, upside_down), "flip_v did not mirror the ink");

	gpf_font_free(font);
}

/*
 * A stroke, an overlay created out of nothing and an overlay reverted all go
 * on the same journal and come back off it in order.
 */
static void test_undo(void)
{
	struct gpf_font *font;
	struct gpf_variant *mono;
	struct gpf_glyph *glyph = build_glyph(&font, &mono);
	struct gpf_undo *undo = gpf_undo_new();
	enum gpf_variant_id id;
	uint32_t code;

	/* a stroke */
	gpf_undo_begin(undo, font, GPF_MONO, 'A');
	gpf_glyph_pixel_set(glyph, 0, 6, 1);
	gpf_undo_commit(undo, font);

	CHECK(gpf_undo_can_undo(undo), "the stroke is not on the journal");

	CHECK(gpf_undo(undo, font, &id, &code), "undo did nothing");
	CHECK(id == GPF_MONO && code == 'A', "undo reports the wrong glyph");

	glyph = gpf_variant_glyph(mono, 'A');

	CHECK(ink_eq(glyph, rows), "undo did not put the ink back");
	CHECK(!gpf_undo_can_undo(undo), "the journal is not empty");
	CHECK(gpf_undo_can_redo(undo), "there is nothing to redo");

	CHECK(gpf_redo(undo, font, &id, &code), "redo did nothing");

	glyph = gpf_variant_glyph(mono, 'A');

	CHECK(gpf_glyph_pixel_get(glyph, 0, 6), "redo did not set the pixel");

	/* an edit that changes nothing is not worth an entry */
	gpf_undo_begin(undo, font, GPF_MONO, 'A');
	gpf_glyph_pixel_set(glyph, 0, 6, 1);
	gpf_undo_commit(undo, font);

	CHECK(!gpf_undo_can_redo(undo), "the redo survived a new edit");

	gpf_undo(undo, font, &id, &code);

	glyph = gpf_variant_glyph(mono, 'A');

	CHECK(ink_eq(glyph, rows),
	      "one undo did not take the stroke back, so an empty edit got an entry");

	gpf_undo_free(undo);
	gpf_font_free(font);
}

/*
 * A change to the family metadata is an entry of the same journal: it comes
 * back off it in order with the glyph edits around it, reports itself as a
 * family change so that the editor does not go anywhere, and setting what is
 * already there records nothing.
 */
static void test_undo_family(void)
{
	struct gpf_font *font;
	struct gpf_variant *mono;
	struct gpf_glyph *glyph = build_glyph(&font, &mono);
	struct gpf_undo *undo = gpf_undo_new();
	struct gpf_family_meta before, meta;
	enum gpf_variant_id id = GPF_BOLD;
	uint32_t code = 'Z';

	snprintf(font->meta.family, sizeof(font->meta.family), "Before");
	font->meta.ascent = 6;
	font->meta.x_height = 3;
	before = font->meta;

	CHECK(!gpf_undo_family(undo, font, &before, NULL), "setting the same failed");
	CHECK(!gpf_undo_can_undo(undo), "setting the same made an entry");

	/* a stroke, then the metadata, then another stroke */
	gpf_undo_begin(undo, font, GPF_MONO, 'A');
	gpf_glyph_pixel_set(glyph, 0, 6, 1);
	gpf_undo_commit(undo, font);

	meta = before;
	snprintf(meta.family, sizeof(meta.family), "After");
	meta.ascent = 7;
	meta.x_height = 4;

	CHECK(!gpf_undo_family(undo, font, &meta, NULL), "the set failed");
	CHECK(gpf_family_meta_same(&font->meta, &meta), "the set did not set");

	glyph = gpf_variant_glyph(mono, 'A');
	gpf_undo_begin(undo, font, GPF_MONO, 'A');
	gpf_glyph_pixel_set(glyph, 1, 6, 1);
	gpf_undo_commit(undo, font);

	CHECK(gpf_undo(undo, font, &id, &code) == GPF_UNDO_GLYPH,
	      "the second stroke did not come back first");
	CHECK(gpf_family_meta_same(&font->meta, &meta),
	      "undoing a stroke touched the metadata");

	id = GPF_BOLD;
	code = 'Z';

	CHECK(gpf_undo(undo, font, &id, &code) == GPF_UNDO_FAMILY,
	      "the metadata did not come back second");
	CHECK(id == GPF_BOLD && code == 'Z',
	      "a family entry reported a glyph");
	CHECK(gpf_family_meta_same(&font->meta, &before),
	      "undo did not put the metadata back");
	CHECK(!strcmp(font->meta.family, "Before"), "the family name is '%s'",
	      font->meta.family);

	glyph = gpf_variant_glyph(mono, 'A');

	CHECK(gpf_glyph_pixel_get(glyph, 0, 6),
	      "undoing the metadata took the first stroke with it");

	CHECK(gpf_redo(undo, font, &id, &code) == GPF_UNDO_FAMILY,
	      "redo did not put the metadata back");
	CHECK(gpf_family_meta_same(&font->meta, &meta),
	      "redo did not set the metadata");

	/* a new change after an undo throws the redo away, as a stroke does */
	gpf_undo(undo, font, &id, &code);

	meta.em = 12;

	gpf_undo_family(undo, font, &meta, NULL);

	CHECK(!gpf_undo_can_redo(undo), "the redo survived a new change");

	gpf_undo_free(undo);
	gpf_font_free(font);
}

/*
 * The authors are journaled with the metadata: a change to them alone is one
 * entry, and undo and redo swap the whole list.
 */
static void test_undo_authors(void)
{
	struct gpf_font *font;
	struct gpf_variant *mono;
	struct gpf_undo *undo = gpf_undo_new();
	struct gpf_author *authors = NULL;
	struct gpf_family_meta meta;
	enum gpf_variant_id id;
	uint32_t code;

	build_glyph(&font, &mono);
	meta = font->meta;

	gpf_authors_add(&font->authors, "2020", "Jane Doe", "jane@example.org");

	authors = gpf_authors_dup(font->authors);
	gpf_authors_add(&authors, "2024", "John Doe", "john@example.org");
	gpf_authors_del(&authors, 0);

	CHECK(!gpf_undo_family(undo, font, &meta, font->authors),
	      "setting the same failed");
	CHECK(!gpf_undo_can_undo(undo), "setting the same authors made an entry");

	CHECK(!gpf_undo_family(undo, font, &meta, authors), "the set failed");
	CHECK(gpf_authors_same(font->authors, authors), "the set did not set");
	CHECK(font->authors != authors, "the font took the caller's vector");

	CHECK(gpf_undo(undo, font, &id, &code) == GPF_UNDO_FAMILY,
	      "the authors did not come back");
	CHECK(gp_vec_len(font->authors) == 1 &&
	      !strcmp(font->authors[0].name, "Jane Doe"),
	      "undo did not put the authors back");

	CHECK(gpf_redo(undo, font, &id, &code) == GPF_UNDO_FAMILY,
	      "redo did not set the authors");
	CHECK(gpf_authors_same(font->authors, authors),
	      "redo set different authors");

	CHECK(gpf_authors_add(&authors, "20x4", "Bad", "bad@example.org"),
	      "added an author with bad years");
	CHECK(gpf_authors_add(&authors, "2024", "Bad", "bad.example.org"),
	      "added an author with a bad email");
	CHECK(gpf_authors_add(&authors, "2024", "", "bad@example.org"),
	      "added an author with no name");

	gp_vec_free(authors);
	gpf_undo_free(undo);
	gpf_font_free(font);
}

/*
 * Creating an overlay in a variant that has no entry, and taking it back.
 */
static void test_undo_overlay(void)
{
	struct gpf_font *font;
	struct gpf_variant *mono;
	struct gpf_variant *regular;
	struct gpf_glyph *glyph;
	struct gpf_undo *undo = gpf_undo_new();
	enum gpf_variant_id id;
	uint32_t code;

	build_glyph(&font, &mono);

	regular = gpf_font_variant(font, GPF_REGULAR, 1);

	gpf_undo_begin(undo, font, GPF_REGULAR, 'A');

	glyph = gpf_variant_glyph_add(regular, 'A');
	glyph->kind = GPF_GLYPH_METRICS;
	glyph->has_advance = 1;
	glyph->advance = 4;

	gpf_undo_commit(undo, font);

	CHECK(gpf_variant_glyph(regular, 'A'), "the overlay was not created");

	gpf_undo(undo, font, &id, &code);

	CHECK(!gpf_variant_glyph(regular, 'A'),
	      "undoing an overlay left the entry behind");
	CHECK(gpf_variant_glyph(mono, 'A'),
	      "undoing an overlay took the base glyph with it");

	gpf_redo(undo, font, &id, &code);

	CHECK(gpf_variant_glyph(regular, 'A'), "redo did not bring the overlay back");

	gpf_undo_free(undo);
	gpf_font_free(font);
}

/*
 * Deleting a range is one action to the user however many glyphs it touches,
 * so the entries come back together.
 */
static void test_undo_group(void)
{
	struct gpf_font *font;
	struct gpf_variant *mono;
	struct gpf_undo *undo = gpf_undo_new();
	enum gpf_variant_id id;
	uint32_t code;

	build_glyph(&font, &mono);

	add_ink(mono, 'B', rows, 1, 5, 6);
	add_ink(mono, 'C', rows, 1, 5, 6);

	gpf_undo_group_begin(undo);

	for (code = 'A'; code <= 'C'; code++) {
		gpf_undo_begin(undo, font, GPF_MONO, code);
		gpf_variant_glyph_del(mono, code);
		gpf_undo_commit(undo, font);
	}

	gpf_undo_group_end(undo);

	CHECK(!gpf_variant_glyphs(mono), "the range was not deleted");

	CHECK(gpf_undo(undo, font, &id, &code), "undo did nothing");

	CHECK(gpf_variant_glyphs(mono) == 3,
	      "one undo brought back %zu glyphs, expected 3",
	      gpf_variant_glyphs(mono));
	CHECK(!gpf_undo_can_undo(undo), "the group was not one entry");

	CHECK(gpf_redo(undo, font, &id, &code), "redo did nothing");
	CHECK(!gpf_variant_glyphs(mono),
	      "one redo left %zu glyphs behind", gpf_variant_glyphs(mono));

	gpf_undo_free(undo);
	gpf_font_free(font);
}


/*
 * One stroke: a pixel toggled between a begin and a commit, which is what the
 * editor does between a press and a release.
 */
static void stroke(struct gpf_undo *undo, struct gpf_font *font,
                   struct gpf_variant *variant, enum gpf_variant_id id,
                   uint32_t code, int x, int y, int set)
{
	gpf_undo_begin(undo, font, id, code);
	gpf_glyph_pixel_set(gpf_variant_glyph(variant, code), x, y, set);
	gpf_undo_commit(undo, font);
}

/*
 * A glyph is modified when it differs from the file: undo back to the state
 * that was written clears the flag, and moving away from it in either
 * direction sets it again.
 */
static void test_undo_modified(void)
{
	struct gpf_font *font;
	struct gpf_variant *mono;
	struct gpf_undo *undo = gpf_undo_new();
	enum gpf_variant_id id;
	uint32_t code;

	build_glyph(&font, &mono);

	stroke(undo, font, mono, GPF_MONO, 'A', 0, 6, 1);
	CHECK(gpf_variant_glyph(mono, 'A')->modified, "the stroke is not marked");

	CHECK(!gpf_undo_is_saved(undo), "the stroke is at the saved state");

	gpf_undo(undo, font, &id, &code);
	CHECK(!gpf_variant_glyph(mono, 'A')->modified,
	      "undo back to the loaded glyph left it marked");
	CHECK(gpf_undo_is_saved(undo), "undo back to the loaded font is not saved");

	gpf_redo(undo, font, &id, &code);
	CHECK(gpf_variant_glyph(mono, 'A')->modified, "redo is not marked");

	/* written with the stroke in */
	gpf_font_clear_modified(font);
	gpf_undo_saved(undo);

	gpf_undo(undo, font, &id, &code);
	CHECK(gpf_variant_glyph(mono, 'A')->modified,
	      "undo past the written state is not marked");

	CHECK(!gpf_undo_is_saved(undo), "undo past the written state is saved");

	gpf_redo(undo, font, &id, &code);
	CHECK(!gpf_variant_glyph(mono, 'A')->modified,
	      "redo back to the written state left it marked");
	CHECK(gpf_undo_is_saved(undo), "redo back to the written state is not saved");

	/* the written state thrown away with the redo is never coming back */
	gpf_undo(undo, font, &id, &code);
	stroke(undo, font, mono, GPF_MONO, 'A', 0, 7, 1);
	gpf_undo(undo, font, &id, &code);
	CHECK(gpf_variant_glyph(mono, 'A')->modified,
	      "the glyph matches a state that was thrown away");
	CHECK(!gpf_undo_is_saved(undo), "a thrown away state is saved");

	gpf_undo_free(undo);
	gpf_font_free(font);
}

/*
 * The three pixels the sequence test toggles, as a bitmap, so that a whole
 * state fits in one comparison.  All three are outside the ink it starts
 * with, so each one moves the box as well.
 */
static int state(struct gpf_variant *variant, uint32_t code)
{
	struct gpf_glyph *glyph = gpf_variant_glyph(variant, code);

	if (!glyph)
		return -1;

	return (gpf_glyph_pixel_get(glyph, 0, 6) ? 1 : 0) |
	       (gpf_glyph_pixel_get(glyph, 5, 6) ? 2 : 0) |
	       (gpf_glyph_pixel_get(glyph, 0, 3) ? 4 : 0);
}

/*
 * Undo and redo walk the journal, they do not toggle: a redo followed by an
 * undo has to take the same step back it just took forward, however deep into
 * the history it is.  "Edit, undo, redo, undo and the last undo does nothing"
 * is the shape this is here to catch.
 */
static void test_undo_sequence(void)
{
	struct gpf_font *font;
	struct gpf_variant *mono;
	struct gpf_undo *undo = gpf_undo_new();
	enum gpf_variant_id id;
	uint32_t code;
	int i;

	build_glyph(&font, &mono);

	CHECK(state(mono, 'A') == 0, "the glyph did not start empty of the three");

	/* edit, undo, redo, undo */
	stroke(undo, font, mono, GPF_MONO, 'A', 0, 6, 1);

	CHECK(state(mono, 'A') == 1, "the edit did not land");

	CHECK(gpf_undo(undo, font, &id, &code), "undo did nothing");
	CHECK(state(mono, 'A') == 0, "undo did not take the edit back");

	CHECK(gpf_redo(undo, font, &id, &code), "redo did nothing");
	CHECK(state(mono, 'A') == 1, "redo did not put the edit back");

	CHECK(gpf_undo(undo, font, &id, &code),
	      "the undo after a redo did nothing");
	CHECK(state(mono, 'A') == 0,
	      "the undo after a redo did not take the edit back");

	/* and the same one more time, since the second round is what broke */
	CHECK(gpf_redo(undo, font, &id, &code), "the second redo did nothing");
	CHECK(gpf_undo(undo, font, &id, &code), "the second undo did nothing");
	CHECK(state(mono, 'A') == 0, "the second undo left the edit in");

	/* three strokes, then all the way back and all the way forward */
	stroke(undo, font, mono, GPF_MONO, 'A', 0, 6, 1);
	stroke(undo, font, mono, GPF_MONO, 'A', 5, 6, 1);
	stroke(undo, font, mono, GPF_MONO, 'A', 0, 3, 1);

	CHECK(state(mono, 'A') == 7, "the three strokes did not land");

	for (i = 3; i > 0; i--) {
		CHECK(gpf_undo(undo, font, &id, &code), "undo %i did nothing", i);
		CHECK(state(mono, 'A') == (1 << (i - 1)) - 1,
		      "undo %i left %i", i, state(mono, 'A'));
	}

	CHECK(!gpf_undo_can_undo(undo), "the journal is not empty");
	CHECK(!gpf_undo(undo, font, &id, &code), "an empty journal undid something");
	CHECK(state(mono, 'A') == 0, "the undo past the end changed the glyph");

	for (i = 1; i <= 3; i++) {
		CHECK(gpf_redo(undo, font, &id, &code), "redo %i did nothing", i);
		CHECK(state(mono, 'A') == (1 << i) - 1,
		      "redo %i left %i", i, state(mono, 'A'));
	}

	CHECK(!gpf_undo_can_redo(undo), "there is something left to redo");
	CHECK(!gpf_redo(undo, font, &id, &code), "a spent journal redid something");
	CHECK(state(mono, 'A') == 7, "the redo past the end changed the glyph");

	/* zig-zag in the middle of the history */
	gpf_undo(undo, font, &id, &code);
	gpf_undo(undo, font, &id, &code);

	CHECK(state(mono, 'A') == 1, "two undos left %i", state(mono, 'A'));

	gpf_redo(undo, font, &id, &code);

	CHECK(state(mono, 'A') == 3, "the redo in the middle left %i",
	      state(mono, 'A'));

	gpf_undo(undo, font, &id, &code);

	CHECK(state(mono, 'A') == 1,
	      "the undo after a redo in the middle left %i", state(mono, 'A'));

	/* an edit here throws away the two entries above it */
	stroke(undo, font, mono, GPF_MONO, 'A', 0, 3, 1);

	CHECK(state(mono, 'A') == 5, "the new edit did not land");
	CHECK(!gpf_undo_can_redo(undo), "the redo survived a new edit");

	gpf_undo(undo, font, &id, &code);

	CHECK(state(mono, 'A') == 1, "the undo of the new edit left %i",
	      state(mono, 'A'));

	gpf_undo_free(undo);
	gpf_font_free(font);
}

/*
 * The journal is one for the whole family, so a sequence walks across glyphs
 * and variants — and says which one it moved, since the editor follows it
 * there.
 */
static void test_undo_across(void)
{
	struct gpf_font *font;
	struct gpf_variant *mono, *regular;
	struct gpf_undo *undo = gpf_undo_new();
	enum gpf_variant_id id;
	uint32_t code;

	build_glyph(&font, &mono);

	add_ink(mono, 'B', rows, 1, 5, 6);

	regular = gpf_font_variant(font, GPF_REGULAR, 1);
	add_ink(regular, 'A', rows, 1, 5, 6);

	stroke(undo, font, mono, GPF_MONO, 'A', 0, 6, 1);
	stroke(undo, font, mono, GPF_MONO, 'B', 5, 6, 1);
	stroke(undo, font, regular, GPF_REGULAR, 'A', 0, 3, 1);

	CHECK(gpf_undo(undo, font, &id, &code), "undo did nothing");
	CHECK(id == GPF_REGULAR && code == 'A',
	      "undo reported %s U+%04X", gpf_variant_name(id), code);
	CHECK(state(regular, 'A') == 0, "the wrong glyph was taken back");
	CHECK(state(mono, 'B') == 2, "an undo reached into another glyph");

	CHECK(gpf_undo(undo, font, &id, &code), "undo did nothing");
	CHECK(id == GPF_MONO && code == 'B',
	      "undo reported %s U+%04X", gpf_variant_name(id), code);
	CHECK(state(mono, 'B') == 0, "the wrong glyph was taken back");

	CHECK(gpf_redo(undo, font, &id, &code), "redo did nothing");
	CHECK(id == GPF_MONO && code == 'B',
	      "redo reported %s U+%04X", gpf_variant_name(id), code);
	CHECK(state(mono, 'B') == 2, "redo put the pixel somewhere else");
	CHECK(state(regular, 'A') == 0, "redo went one entry too far");

	CHECK(gpf_redo(undo, font, &id, &code), "redo did nothing");
	CHECK(state(regular, 'A') == 4, "the last redo did not land");

	CHECK(gpf_undo(undo, font, &id, &code), "the undo after a redo did nothing");
	CHECK(state(regular, 'A') == 0,
	      "the undo after a redo did not take the edit back");

	CHECK(state(mono, 'A') == 1, "the first stroke was taken back with it");

	gpf_undo_free(undo);
	gpf_font_free(font);
}

/*
 * A group is one step of the sequence, not one entry of it: the entries on
 * either side of it have to stay where they are however many it holds.
 */
static void test_undo_group_sequence(void)
{
	struct gpf_font *font;
	struct gpf_variant *mono;
	struct gpf_undo *undo = gpf_undo_new();
	enum gpf_variant_id id;
	uint32_t code;

	build_glyph(&font, &mono);

	add_ink(mono, 'B', rows, 1, 5, 6);

	/* one stroke, one group of two, one stroke */
	stroke(undo, font, mono, GPF_MONO, 'A', 0, 6, 1);

	gpf_undo_group_begin(undo);
	stroke(undo, font, mono, GPF_MONO, 'A', 5, 6, 1);
	stroke(undo, font, mono, GPF_MONO, 'B', 5, 6, 1);
	gpf_undo_group_end(undo);

	stroke(undo, font, mono, GPF_MONO, 'A', 0, 3, 1);

	CHECK(state(mono, 'A') == 7 && state(mono, 'B') == 2,
	      "the sequence did not land");

	CHECK(gpf_undo(undo, font, &id, &code), "undo did nothing");
	CHECK(state(mono, 'A') == 3, "the last stroke did not come back alone");

	CHECK(gpf_undo(undo, font, &id, &code), "undo did nothing");
	CHECK(state(mono, 'A') == 1 && state(mono, 'B') == 0,
	      "the group did not come back in one go");

	CHECK(gpf_undo_can_undo(undo), "the group took the stroke below it");

	CHECK(gpf_redo(undo, font, &id, &code), "redo did nothing");
	CHECK(state(mono, 'A') == 3 && state(mono, 'B') == 2,
	      "the group did not go forward in one go");

	CHECK(gpf_undo(undo, font, &id, &code), "the undo after a redo did nothing");
	CHECK(state(mono, 'A') == 1 && state(mono, 'B') == 0,
	      "the undo after a redo did not take the whole group back");

	CHECK(gpf_undo(undo, font, &id, &code), "undo did nothing");
	CHECK(state(mono, 'A') == 0, "the first stroke did not come back");
	CHECK(!gpf_undo_can_undo(undo), "the journal is not empty");

	/* and forward through all three steps again */
	CHECK(gpf_redo(undo, font, &id, &code), "redo did nothing");
	CHECK(state(mono, 'A') == 1, "the first step redid more than itself");

	CHECK(gpf_redo(undo, font, &id, &code), "redo did nothing");
	CHECK(state(mono, 'A') == 3 && state(mono, 'B') == 2,
	      "the group redid as one entry");

	CHECK(gpf_redo(undo, font, &id, &code), "redo did nothing");
	CHECK(state(mono, 'A') == 7, "the last step did not come forward");
	CHECK(!gpf_undo_can_redo(undo), "there is something left to redo");

	gpf_undo_free(undo);
	gpf_font_free(font);
}

/*
 * The accent a font draws on a letter, taken off it: pressing compose on a
 * glyph that is already drawn learns the accent this way.
 */
static void test_compose_remainder(void)
{
	struct gpf_glyph base = {0}, glyph = {0}, rem = {0};

	static const char *a[] = {"####", "#..#", "####", NULL};
	static const char *a_acute[] = {
		"..#.",
		".#..",
		"....",
		"####",
		"#..#",
		"####",
		NULL,
	};
	static const char *acute[] = {".#", "#.", NULL};

	set_ink(&base, a, 0, 3);
	set_ink(&glyph, a_acute, 0, 6);

	CHECK(!gpf_compose_remainder(&glyph, &base, &rem),
	      "the accent was not taken off the letter");
	CHECK(ink_eq(&rem, acute), "the accent came off the wrong shape");
	CHECK(rem.bearing_x == 1 && rem.bearing_y == 6,
	      "the accent came off at %i,%i", rem.bearing_x, rem.bearing_y);

	/* a glyph that does not contain the base has no accent to give */
	CHECK(gpf_compose_remainder(&base, &glyph, &rem),
	      "an accent came off a glyph that is not that base with one");

	free(base.bits);
	free(glyph.bits);
	free(rem.bits);
}

/*
 * The letters an accent goes on, which is what the compose view lists when a
 * mark is selected.  Either name of the accent finds them, since a font
 * carries the spacing one far more often than the combining mark.
 */
static void test_decompose_by_accent(void)
{
	uint32_t mark[64], spacing[64];
	unsigned int cnt, i;

	cnt = gpf_decompose_by_accent(0x030c, mark, 64);

	CHECK(cnt > 8, "the combining caron found %u letters", cnt);

	CHECK(cnt == gpf_decompose_by_accent(0x02c7, spacing, 64),
	      "the spacing caron found a different number of letters");

	for (i = 0; i < cnt; i++) {
		CHECK(mark[i] == spacing[i],
		      "the two names of the caron disagree at %u: U+%04X U+%04X",
		      i, mark[i], spacing[i]);
	}

	for (i = 0; i < cnt; i++) {
		const struct gpf_decomposition *decomp = gpf_decompose(mark[i]);

		CHECK(decomp && decomp->mark == 0x030c,
		      "U+%04X is not a caron letter", mark[i]);
	}

	for (i = 1; i < cnt; i++) {
		CHECK(mark[i-1] < mark[i], "the letters are not sorted: %u",
		      i);
	}

	/* Č is one of them, and a letter is not an accent */
	for (i = 0; i < cnt && mark[i] != 0x010c; i++);

	CHECK(i < cnt, "the caron letters do not include U+010C");

	CHECK(!gpf_decompose_by_accent('A', mark, 64),
	      "a letter was taken for an accent");
	CHECK(!gpf_decompose_by_accent(0, mark, 64),
	      "U+0000 was taken for an accent");

	/* what fits is what is asked for */
	CHECK(gpf_decompose_by_accent(0x030c, mark, 3) == 3,
	      "the count was not capped");
}

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	test_pixel();
	test_shift_and_flip();
	test_undo();
	test_undo_overlay();
	test_undo_family();
	test_undo_authors();
	test_undo_group();
	test_undo_modified();
	test_undo_sequence();
	test_undo_across();
	test_undo_group_sequence();
	test_compose_remainder();
	test_decompose_by_accent();

	if (failures) {
		printf("%u failures\n", failures);
		return 1;
	}

	printf("edit: ok\n");

	return 0;
}
