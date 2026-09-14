/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/*
 * The variant lattice, on a synthetic family small enough to reason about.
 * Every step of the three step rule for bold is exercised, in particular the
 * numbers block fall through that step 2 hinges on.
 */

#include <stdio.h>
#include <stdlib.h>

#include "font.h"
#include "embolden.h"
#include "compose.h"
#include "edit.h"
#include "resolve.h"
#include "test_util.h"

/* the base shapes */
static const char *a_rows[] = {
	"..##..",
	".#..#.",
	"#....#",
	"######",
	"#....#",
	NULL,
};

static const char *a_bold_rows[] = {
	"..###..",
	".##.##.",
	"##...##",
	"#######",
	"##...##",
	NULL,
};

/* a wide i in mono, redrawn narrow in regular */
static const char *i_mono_rows[] = {
	"#####",
	"..#..",
	"..#..",
	"#####",
	NULL,
};

static const char *i_regular_rows[] = {
	"#",
	"#",
	"#",
	"#",
	NULL,
};

static const char *i_regular_bold_rows[] = {
	"##",
	"##",
	"##",
	"##",
	NULL,
};

/* embolden mangles the crossing, so bold-mono carries a hand fix */
static const char *y_rows[] = {
	"#...#",
	".#.#.",
	"..#..",
	".#...",
	NULL,
};

static const char *y_fixed_rows[] = {
	"##..##",
	".#..#.",
	"..##..",
	".##...",
	NULL,
};

static const char *b_bold_rows[] = {
	"####",
	"#..#",
	"####",
	NULL,
};

static const char *acute_rows[] = {
	".#",
	"#.",
	NULL,
};

/* A with the acute two pixels above it */
static const char *a_acute_rows[] = {
	"...#..",
	"..#...",
	"......",
	"..##..",
	".#..#.",
	"#....#",
	"######",
	"#....#",
	NULL,
};

static struct gpf_font *build(void)
{
	struct gpf_font *font = gpf_font_new();
	struct gpf_variant *mono, *regular, *bold_mono, *bold;
	struct gpf_glyph *glyph;

	font->meta.ascent = 6;
	font->meta.descent = 2;

	mono = gpf_font_variant(font, GPF_MONO, 1);
	mono->advance = 10;

	add_ink(mono, 'A', a_rows, 0, 5, 10);
	add_ink(mono, 'B', a_rows, 0, 5, 10);
	add_ink(mono, 'i', i_mono_rows, 0, 5, 10);
	add_ink(mono, 'y', y_rows, 0, 3, 10);
	add_ink(mono, 0x00b4, acute_rows, 0, 8, 10);

	/* Á is a composition in the base */
	glyph = gpf_variant_glyph_add(mono, 0x00c1);
	glyph->kind = GPF_GLYPH_COMPOSE;
	glyph->base = 'A';
	glyph->accent = 0x00b4;
	glyph->dx = 2;
	glyph->dy = 0;

	regular = gpf_font_variant(font, GPF_REGULAR, 1);
	regular->spacing_mono = 0;

	/* a numbers block, the ink is inherited */
	glyph = gpf_variant_glyph_add(regular, 'A');
	glyph->kind = GPF_GLYPH_METRICS;
	glyph->has_advance = 1;
	glyph->advance = 8;

	/* ink, the proportional shape differs */
	add_ink(regular, 'i', i_regular_rows, 0, 5, 3);

	/* a numbers block again, this one has to fall through to bold-mono */
	glyph = gpf_variant_glyph_add(regular, 'y');
	glyph->kind = GPF_GLYPH_METRICS;
	glyph->has_advance = 1;
	glyph->advance = 8;

	bold_mono = gpf_font_variant(font, GPF_BOLD_MONO, 1);
	add_ink(bold_mono, 'y', y_fixed_rows, 0, 3, 10);

	bold = gpf_font_variant(font, GPF_BOLD, 1);
	add_ink(bold, 'B', b_bold_rows, 0, 4, 7);

	return font;
}

struct expect {
	enum gpf_variant_id id;
	uint32_t code;
	enum gpf_provenance prov;
	int advance;
	const char **rows;
	const char *desc;
};

static void check(struct gpf_font *font, const struct expect *e)
{
	struct gpf_resolved res;

	if (gpf_resolve(font, e->id, e->code, &res)) {
		FAIL("%s: U+%04X is not in %s", e->desc, e->code,
		     gpf_variant_name(e->id));
		return;
	}

	if (res.prov != e->prov) {
		FAIL("%s: U+%04X in %s is %s, expected %s", e->desc, e->code,
		     gpf_variant_name(e->id), gpf_provenance_name(res.prov),
		     gpf_provenance_name(e->prov));
	}

	if (res.glyph.advance != e->advance) {
		FAIL("%s: U+%04X in %s has advance %i, expected %i", e->desc,
		     e->code, gpf_variant_name(e->id), res.glyph.advance,
		     e->advance);
	}

	if (!ink_eq(&res.glyph, e->rows)) {
		FAIL("%s: U+%04X in %s has unexpected ink", e->desc, e->code,
		     gpf_variant_name(e->id));
		print_ink(&res.glyph);
	}

	gpf_resolved_clear(&res);
}

static void test_lattice(struct gpf_font *font)
{
	static const struct expect expects[] = {
		{GPF_MONO, 'A', GPF_PROV_STORED, 10, a_rows,
		 "the base is stored"},
		{GPF_REGULAR, 'A', GPF_PROV_INHERITED, 8, a_rows,
		 "a numbers block keeps the ink and takes the advance"},
		{GPF_BOLD_MONO, 'A', GPF_PROV_EMBOLDENED, 10, a_bold_rows,
		 "bold-mono emboldens the base and keeps the monospace cell"},
		{GPF_BOLD, 'A', GPF_PROV_EMBOLDENED, 9, a_bold_rows,
		 "step 3, the ink comes from bold-mono, regular fitted its own "
		 "advance, so bold keeps regular's gap on the right"},

		{GPF_REGULAR, 'i', GPF_PROV_STORED, 3, i_regular_rows,
		 "a redrawn proportional shape"},
		{GPF_BOLD, 'i', GPF_PROV_EMBOLDENED, 4, i_regular_bold_rows,
		 "step 2, bold emboldens regular's ink and gets back the pixel "
		 "that ate, because a proportional advance was fitted to it"},

		{GPF_BOLD_MONO, 'y', GPF_PROV_STORED, 10, y_fixed_rows,
		 "a hand fix in bold-mono"},
		{GPF_BOLD, 'y', GPF_PROV_BOLD_MONO, 9, y_fixed_rows,
		 "step 3 shares the hand fix, the advance grows by what the fix "
		 "is wider than regular's ink"},

		{GPF_BOLD, 'B', GPF_PROV_STORED, 7, b_bold_rows,
		 "step 1, hand drawn in bold"},

		{GPF_MONO, 0x00c1, GPF_PROV_COMPOSED, 10, a_acute_rows,
		 "a composition in the base"},
		{GPF_REGULAR, 0x00c1, GPF_PROV_COMPOSED, 8, a_acute_rows,
		 "a composition is inherited as a definition, so it is built "
		 "from regular's base and takes regular's advance"},
	};

	unsigned int i;

	for (i = 0; i < sizeof(expects)/sizeof(expects[0]); i++)
		check(font, &expects[i]);
}

/*
 * The composed Á emboldens as a whole in bold-mono, it is not composed out of
 * two emboldened halves.
 */
static void test_composed_embolden(struct gpf_font *font)
{
	struct gpf_resolved composed, bold;

	if (gpf_resolve(font, GPF_MONO, 0x00c1, &composed)) {
		FAIL("mono has no composed glyph");
		return;
	}

	if (gpf_resolve(font, GPF_BOLD_MONO, 0x00c1, &bold)) {
		FAIL("bold-mono has no composed glyph");
		gpf_resolved_clear(&composed);
		return;
	}

	gpf_embolden(&composed.glyph, 0);

	CHECK(composed.glyph.width == bold.glyph.width &&
	      composed.glyph.height == bold.glyph.height &&
	      !memcmp(composed.glyph.bits, bold.glyph.bits,
	              composed.glyph.width * composed.glyph.height),
	      "bold-mono does not embolden the composed result");

	gpf_resolved_clear(&composed);
	gpf_resolved_clear(&bold);
}

/*
 * A composition follows the variant's own base.  Redrawing the base in regular
 * redraws everything composed from it in regular, which is the reason accented
 * glyphs are stored as compositions at all.
 */
static void test_compose_follows_base(struct gpf_font *font)
{
	struct gpf_variant *regular = gpf_font_variant(font, GPF_REGULAR, 1);
	struct gpf_resolved res;

	/* a narrower A, drawn only in regular */
	static const char *narrow_a[] = {
		".##.",
		"#..#",
		"####",
		"#..#",
		NULL,
	};
	/* the acute sits where it sat, two rows above the shorter A */
	static const char *narrow_a_acute[] = {
		"...#",
		"..#.",
		"....",
		"....",
		".##.",
		"#..#",
		"####",
		"#..#",
		NULL,
	};

	add_ink(regular, 'A', narrow_a, 0, 4, 6);

	if (gpf_resolve(font, GPF_REGULAR, 0x00c1, &res)) {
		FAIL("the composition stopped resolving");
		return;
	}

	CHECK(ink_eq(&res.glyph, narrow_a_acute),
	      "the composition did not follow the base redrawn in regular");
	CHECK(res.glyph.advance == 6,
	      "the composition has advance %i, expected the base's 6",
	      res.glyph.advance);

	gpf_resolved_clear(&res);

	gpf_variant_glyph_del(regular, 'A');
}

/*
 * A glyph the base does not have cannot be inherited into an overlay.
 */
static void test_missing(struct gpf_font *font)
{
	struct gpf_variant *regular = gpf_font_variant(font, GPF_REGULAR, 1);
	struct gpf_glyph *glyph = gpf_variant_glyph_add(regular, 'Z');
	struct gpf_resolved res;
	unsigned int i;

	glyph->kind = GPF_GLYPH_METRICS;
	glyph->has_advance = 1;
	glyph->advance = 5;

	for (i = 0; i < GPF_VARIANTS; i++)
		CHECK(gpf_resolve(font, i, 'Z', &res),
		      "U+005A resolves in %s without any ink",
		      gpf_variant_name(i));
}

/*
 * Two glyphs composed out of each other terminate rather than recurse.
 */
static void test_compose_cycle(void)
{
	struct gpf_font *font = gpf_font_new();
	struct gpf_variant *mono = gpf_font_variant(font, GPF_MONO, 1);
	struct gpf_glyph *glyph;
	struct gpf_resolved res;

	glyph = gpf_variant_glyph_add(mono, 'X');
	glyph->kind = GPF_GLYPH_COMPOSE;
	glyph->base = 'Y';
	glyph->accent = 0x00b4;

	glyph = gpf_variant_glyph_add(mono, 'Y');
	glyph->kind = GPF_GLYPH_COMPOSE;
	glyph->base = 'X';
	glyph->accent = 0x00b4;

	CHECK(gpf_resolve(font, GPF_MONO, 'X', &res),
	      "a compose cycle resolved");

	gpf_font_free(font);
}

static void test_embolden(void)
{
	struct gpf_glyph glyph = {0};

	/* a two pixel gap survives the smear */
	static const char *rows[] = {"#..#", NULL};
	static const char *bold[] = {"##.##", NULL};

	/* a single pixel gap does not, which is why some glyphs need a fix */
	static const char *narrow[] = {"#.#", NULL};
	static const char *filled[] = {"####", NULL};

	set_ink(&glyph, rows, 0, 2);

	CHECK(!gpf_embolden(&glyph, 0), "embolden failed");
	CHECK(ink_eq(&glyph, bold), "embolden does not smear one pixel right");
	CHECK(glyph.bearing_x == 0 && glyph.bearing_y == 2,
	      "embolden moved the bearings");

	set_ink(&glyph, narrow, 0, 2);

	CHECK(!gpf_embolden(&glyph, 0), "embolden failed");
	CHECK(ink_eq(&glyph, filled), "a one pixel gap is not filled in");

	free(glyph.bits);
}

/*
 * The default offset puts the accent on the base and then inside the box: a
 * letter that reaches the ascent has no room above it, and ink above the
 * ascent is ink in the line above.
 */
static void test_accent_offset(void)
{
	struct gpf_font *font = gpf_font_new();
	struct gpf_glyph base = {0}, accent = {0};
	int dx, dy;

	static const char *tall[] = {"##", "##", "##", "##", NULL};
	static const char *dot[] = {"##", NULL};

	font->meta.ascent = 4;
	font->meta.descent = 2;

	/* the base fills the box from the baseline to the ascent */
	set_ink(&base, tall, 0, 4);
	set_ink(&accent, dot, 0, 1);

	base.advance = 4;

	gpf_accent_offset(font, &base, &accent, GPF_ACCENT_ABOVE, &dx, &dy);

	CHECK(accent.bearing_y + dy <= font->meta.ascent,
	      "the accent tops out at %i, the ascent is %i",
	      accent.bearing_y + dy, font->meta.ascent);

	gpf_accent_offset(font, &base, &accent, GPF_ACCENT_BELOW, &dx, &dy);

	CHECK(accent.bearing_y + dy - (int)accent.height + 1 >= -font->meta.descent,
	      "the accent bottoms out at %i, the descent is %i",
	      accent.bearing_y + dy - (int)accent.height + 1, -font->meta.descent);

	/*
	 * And with no box to keep it in, it sits where it was put: one clear
	 * row above the letter, which is how the corpus draws its accents.
	 */
	gpf_accent_offset(NULL, &base, &accent, GPF_ACCENT_ABOVE, &dx, &dy);

	CHECK(accent.bearing_y + dy == base.bearing_y + 1 + (int)accent.height,
	      "the unclamped accent is at %i", accent.bearing_y + dy);

	CHECK(accent.bearing_y + dy - (int)accent.height == base.bearing_y + 1,
	      "the accent is %i rows above the letter, expected one clear row",
	      accent.bearing_y + dy - (int)accent.height - base.bearing_y);

	free(base.bits);
	free(accent.bits);
	gpf_font_free(font);
}

/*
 * An accent above a j is drawn on the dotless ȷ, which hardly any font has:
 * the j with its dot taken off is drawn from instead.
 */
static void test_draw_base_dotless(void)
{
	static const char *j_rows[] = {"..#", "...", "..#", "..#", "#.#", ".#.", NULL};
	static const char *dotless_rows[] = {"..#", "..#", "#.#", ".#.", NULL};
	struct gpf_font *font = gpf_font_new();
	struct gpf_variant *mono;
	struct gpf_glyph want = {0};
	struct gpf_resolved res;

	mono = gpf_font_variant(font, GPF_MONO, 1);
	mono->advance = 4;

	add_ink(mono, 'j', j_rows, 0, 4, 4);

	set_ink(&want, dotless_rows, 0, 2);

	if (gpf_draw_base(font, GPF_MONO, gpf_decompose(0x0135), &res)) {
		FAIL("no base to draw ĵ from");
	} else {
		CHECK(res.glyph.width == want.width &&
		      res.glyph.height == want.height &&
		      res.glyph.bearing_x == want.bearing_x &&
		      res.glyph.bearing_y == want.bearing_y &&
		      !memcmp(res.glyph.bits, want.bits, want.width * want.height),
		      "ĵ is not drawn from the dotless j");
		gpf_resolved_clear(&res);
	}

	free(want.bits);
	gpf_font_free(font);
}

/*
 * A numbers block moves the parent's ink, it does not pin it: a dot drawn on
 * top of the letter in mono grows it upwards, and bold-mono's copy grows with
 * it rather than keeping the old top and sliding down.
 */
static void test_shift_follows_parent(void)
{
	static const char *z_rows[] = {"####", "..#.", ".#..", "####", NULL};
	struct gpf_font *font = gpf_font_new();
	struct gpf_variant *mono, *bold_mono;
	struct gpf_glyph *glyph, numbers;
	struct gpf_resolved before, after;

	font->meta.ascent = 8;
	font->meta.descent = 2;

	mono = gpf_font_variant(font, GPF_MONO, 1);
	mono->spacing_mono = 1;
	mono->advance = 7;
	add_ink(mono, 0x17c, z_rows, 1, 4, 7);

	/* bold-mono draws the emboldened letter a column to the left */
	bold_mono = gpf_font_variant(font, GPF_BOLD_MONO, 1);

	if (gpf_resolve(font, GPF_BOLD_MONO, 0x17c, &before)) {
		FAIL("ż is not in bold-mono");
		gpf_font_free(font);
		return;
	}

	glyph = gpf_variant_glyph_add(bold_mono, 0x17c);
	gpf_glyph_set_bits(glyph, before.glyph.bits, before.glyph.width,
	                   before.glyph.height, before.glyph.bearing_x - 1,
	                   before.glyph.bearing_y);
	glyph->kind = GPF_GLYPH_INK;
	glyph->has_advance = 1;
	glyph->has_bearing = 1;
	glyph->advance = before.glyph.advance;

	gpf_resolved_clear(&before);

	CHECK(gpf_entry_form(font, GPF_BOLD_MONO, glyph, &numbers) ==
	      GPF_ENTRY_METRICS && numbers.has_shift &&
	      numbers.shift_x == -1 && numbers.shift_y == 0,
	      "the moved ink is not stored as a shift of -1,0");

	gpf_font_simplify(font);

	/* the dot, a blank row above the letter */
	gpf_glyph_pixel_set(gpf_variant_glyph(mono, 0x17c), 2, 6, 1);

	if (gpf_resolve(font, GPF_MONO, 0x17c, &before) ||
	    gpf_resolve(font, GPF_BOLD_MONO, 0x17c, &after)) {
		FAIL("ż is gone");
		gpf_font_free(font);
		return;
	}

	CHECK(after.glyph.bearing_y == before.glyph.bearing_y,
	      "bold-mono tops out at %i, mono at %i",
	      after.glyph.bearing_y, before.glyph.bearing_y);

	gpf_resolved_clear(&before);
	gpf_resolved_clear(&after);
	gpf_font_free(font);
}

/*
 * The measured metrics come from the variant as it resolves, so a variant that
 * inherits a letter measures the parent's, and one that redraws it measures
 * its own.  A letter that is missing, or has no ink to take a height from,
 * measures nothing and leaves the value alone.
 */
static void test_measure(void)
{
	static const char *x_rows[] = {"#.#", ".#.", "#.#", NULL};
	static const char *h_rows[] = {"#.#", "#.#", "###", "#.#", "#.#", NULL};
	static const char *zero_rows[] = {"###", "#.#", "###", NULL};
	struct gpf_font *font = gpf_font_new();
	struct gpf_variant *mono, *regular;
	int val;

	mono = gpf_font_variant(font, GPF_MONO, 1);
	mono->spacing_mono = 1;
	mono->advance = 6;

	add_ink(mono, 'x', x_rows, 1, 3, 6);
	add_ink(mono, 'H', h_rows, 1, 5, 6);
	add_ink(mono, '0', zero_rows, 1, 3, 6);

	regular = gpf_font_variant(font, GPF_REGULAR, 1);
	regular->spacing_mono = 0;
	add_ink(regular, '0', zero_rows, 1, 3, 5);

	val = 0;
	CHECK(!gpf_measure(font, GPF_MONO, GPF_MEASURE_X_HEIGHT, &val) &&
	      val == 3, "x-height %i, expected 3", val);

	val = 0;
	CHECK(!gpf_measure(font, GPF_REGULAR, GPF_MEASURE_CAP_HEIGHT, &val) &&
	      val == 5, "inherited cap height %i, expected 5", val);

	val = 0;
	CHECK(!gpf_measure(font, GPF_MONO, GPF_MEASURE_CH_WIDTH, &val) &&
	      val == 6, "mono digit width %i, expected 6", val);

	val = 0;
	CHECK(!gpf_measure(font, GPF_REGULAR, GPF_MEASURE_CH_WIDTH, &val) &&
	      val == 5, "regular digit width %i, expected 5", val);

	gpf_variant_glyph_del(mono, 'H');

	val = 42;
	CHECK(gpf_measure(font, GPF_MONO, GPF_MEASURE_CAP_HEIGHT, &val),
	      "a missing H measured something");
	CHECK(val == 42, "a failed measure changed the value to %i", val);

	gpf_variant_glyph_add(mono, 'H');

	CHECK(gpf_measure(font, GPF_MONO, GPF_MEASURE_CAP_HEIGHT, &val),
	      "an empty H measured a height");
	CHECK(val == 42, "a failed measure changed the value to %i", val);

	gpf_font_free(font);
}

/*
 * Regular's ink as an ink entry identical to mono's and the same ink as a
 * numbers block are one glyph, editing makes the former and saving writes
 * the latter, so bold has to resolve the same from both.
 */
static void test_regular_ink_form(void)
{
	static const char *rows[] = {
		"#...#",
		"#####",
		NULL,
	};
	static const char *bold_rows[] = {
		"##..##",
		"######",
		NULL,
	};
	struct gpf_font *font = gpf_font_new();
	struct gpf_variant *mono, *regular, *bold_mono;
	struct gpf_glyph *glyph;
	struct gpf_resolved res;
	int pass;

	mono = gpf_font_variant(font, GPF_MONO, 1);
	mono->advance = 7;
	add_ink(mono, 'U', rows, 1, 2, 7);

	/* moved into the cell's left gap, the right one stays */
	bold_mono = gpf_font_variant(font, GPF_BOLD_MONO, 1);
	glyph = gpf_variant_glyph_add(bold_mono, 'U');
	glyph->kind = GPF_GLYPH_METRICS;
	glyph->has_shift = 1;
	glyph->shift_x = -1;

	regular = gpf_font_variant(font, GPF_REGULAR, 1);
	regular->spacing_mono = 0;

	/* moved one pixel left and one pixel narrower, as edited */
	add_ink(regular, 'U', rows, 0, 2, 6);

	for (pass = 0; pass < 2; pass++) {
		const char *form = pass ? "numbers" : "ink";

		if (gpf_resolve(font, GPF_BOLD, 'U', &res)) {
			FAIL("U+0055 is not in bold with regular as %s", form);
			break;
		}

		CHECK(ink_eq(&res.glyph, bold_rows),
		      "bold has unexpected ink with regular as %s", form);
		CHECK(res.glyph.bearing_x == 0 && res.glyph.advance == 7,
		      "bold has bearing_x %i advance %i with regular as %s, "
		      "expected 0 and 7", res.glyph.bearing_x,
		      res.glyph.advance, form);

		gpf_resolved_clear(&res);

		/* what saving does to the edited entry */
		gpf_font_simplify(font);

		glyph = gpf_variant_glyph(regular, 'U');
		CHECK(glyph && glyph->kind == GPF_GLYPH_METRICS,
		      "regular was not simplified into a numbers block");
	}

	gpf_font_free(font);
}

int main(void)
{
	struct gpf_font *font;

	setvbuf(stdout, NULL, _IONBF, 0);

	font = build();

	test_lattice(font);
	test_composed_embolden(font);
	test_compose_follows_base(font);
	test_missing(font);

	gpf_font_free(font);

	test_compose_cycle();
	test_embolden();
	test_regular_ink_form();

	test_accent_offset();
	test_draw_base_dotless();
	test_shift_follows_parent();
	test_measure();

	if (failures) {
		printf("%u failures\n", failures);
		return 1;
	}

	printf("resolve: ok\n");

	return 0;
}
