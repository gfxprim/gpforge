/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/*
 * Writes a font built in memory, reads it back and compares, which pins the
 * ink spec and keeps the reader and the writer in agreement.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "font.h"
#include "format.h"
#include "resolve.h"
#include "test_util.h"

static int glyph_cmp(const struct gpf_glyph *a, const struct gpf_glyph *b)
{
	if (a->code != b->code || a->kind != b->kind)
		return 1;

	if (a->has_advance != b->has_advance || a->has_bearing != b->has_bearing ||
	    a->has_shift != b->has_shift)
		return 1;

	if (a->has_shift &&
	    (a->shift_x != b->shift_x || a->shift_y != b->shift_y))
		return 1;

	if (a->has_advance && a->advance != b->advance)
		return 1;

	if (a->kind == GPF_GLYPH_COMPOSE) {
		return a->base != b->base || a->accent != b->accent ||
		       a->dx != b->dx || a->dy != b->dy;
	}

	if (a->has_bearing &&
	    (a->bearing_x != b->bearing_x || a->bearing_y != b->bearing_y))
		return 1;

	if (a->width != b->width || a->height != b->height)
		return 1;

	if (a->width && memcmp(a->bits, b->bits, a->width * a->height))
		return 1;

	return 0;
}

static void font_cmp(struct gpf_font *a, struct gpf_font *b)
{
	struct gpf_family_meta *ma = &a->meta;
	struct gpf_family_meta *mb = &b->meta;
	unsigned int i;
	size_t j;

	CHECK(!strcmp(ma->family, mb->family), "family '%s' != '%s'",
	      ma->family, mb->family);
	CHECK(ma->size == mb->size, "size %i != %i", ma->size, mb->size);
	CHECK(ma->ascent == mb->ascent, "ascent %i != %i", ma->ascent, mb->ascent);
	CHECK(ma->descent == mb->descent, "descent %i != %i", ma->descent, mb->descent);
	CHECK(ma->em == mb->em, "em %i != %i", ma->em, mb->em);
	CHECK(ma->x_height == mb->x_height, "x_height %i != %i",
	      ma->x_height, mb->x_height);
	CHECK(ma->cap_height == mb->cap_height, "cap_height %i != %i",
	      ma->cap_height, mb->cap_height);
	CHECK(ma->underline_pos == mb->underline_pos, "underline_pos %i != %i",
	      ma->underline_pos, mb->underline_pos);
	CHECK(ma->default_glyph == mb->default_glyph, "default_glyph %u != %u",
	      ma->default_glyph, mb->default_glyph);

	if (gp_vec_len(a->blocks) != gp_vec_len(b->blocks)) {
		FAIL("%zu declared blocks, read back %zu",
		     gp_vec_len(a->blocks), gp_vec_len(b->blocks));
	} else {
		for (j = 0; j < gp_vec_len(a->blocks); j++) {
			CHECK(a->blocks[j].min == b->blocks[j].min &&
			      a->blocks[j].max == b->blocks[j].max &&
			      !strcmp(a->blocks[j].name, b->blocks[j].name),
			      "declared block %zu differs", j);
		}
	}

	if (gp_vec_len(a->authors) != gp_vec_len(b->authors)) {
		FAIL("%zu authors, read back %zu",
		     gp_vec_len(a->authors), gp_vec_len(b->authors));
	} else {
		for (j = 0; j < gp_vec_len(a->authors); j++) {
			CHECK(!strcmp(a->authors[j].years, b->authors[j].years) &&
			      !strcmp(a->authors[j].name, b->authors[j].name) &&
			      !strcmp(a->authors[j].email, b->authors[j].email),
			      "author %zu differs", j);
		}
	}

	for (i = 0; i < GPF_VARIANTS; i++) {
		struct gpf_variant *va = a->variants[i], *vb = b->variants[i];

		if (!va != !vb) {
			FAIL("variant %s present in one font only",
			     gpf_variant_name(i));
			continue;
		}

		if (!va)
			continue;

		CHECK(va->spacing_mono == vb->spacing_mono,
		      "%s spacing differs", gpf_variant_name(i));
		CHECK(va->derive_embolden == vb->derive_embolden,
		      "%s derive differs", gpf_variant_name(i));
		CHECK(va->advance == vb->advance, "%s advance %i != %i",
		      gpf_variant_name(i), va->advance, vb->advance);

		if (gpf_variant_glyphs(va) != gpf_variant_glyphs(vb)) {
			FAIL("%s has %zu glyphs, read back %zu",
			     gpf_variant_name(i), gpf_variant_glyphs(va),
			     gpf_variant_glyphs(vb));
			continue;
		}

		for (j = 0; j < gpf_variant_glyphs(va); j++) {
			if (glyph_cmp(&va->glyphs[j], &vb->glyphs[j])) {
				FAIL("%s U+%04X differs after a round trip",
				     gpf_variant_name(i), va->glyphs[j].code);
			}
		}
	}
}

static char *slurp(const char *path)
{
	FILE *f = fopen(path, "r");
	static char buf[1 << 20];
	size_t len;

	if (!f)
		return NULL;

	len = fread(buf, 1, sizeof(buf) - 1, f);
	buf[len] = 0;

	fclose(f);

	return buf;
}

static struct gpf_font *build(void)
{
	struct gpf_font *font = gpf_font_new();
	struct gpf_family_meta *m = &font->meta;
	struct gpf_variant *mono, *regular;
	struct gpf_glyph *glyph;

	static const char *a_rows[] = {
		"..##..",
		".#..#.",
		"#....#",
		"######",
		"#....#",
		NULL,
	};
	/* a descender, ink below the baseline */
	static const char *y_rows[] = {
		"#...#",
		".#.#.",
		"..#..",
		".#...",
		NULL,
	};
	/* ink entirely below the baseline, a negative bearing_y */
	static const char *under_rows[] = {
		"#####",
		NULL,
	};
	/* ink left of the origin, a negative bearing_x */
	static const char *hash_rows[] = {
		".#.#.",
		"#####",
		".#.#.",
		NULL,
	};
	/* ink floating above the baseline */
	static const char *quote_rows[] = {
		"#.#",
		"#.#",
		NULL,
	};
	/* ink crossing the advance */
	static const char *wide_rows[] = {
		"########",
		NULL,
	};

	snprintf(m->family, sizeof(m->family), "Test Font");
	m->size = 8;
	m->ascent = 6;
	m->descent = 2;
	m->em = 6;
	m->x_height = 3;
	m->cap_height = 5;
	m->ch_width = 6;
	m->underline_pos = -2;
	m->underline_thickness = 1;
	m->strike_pos = 2;
	m->strike_thickness = 1;
	m->overline_pos = 6;
	m->overline_thickness = 1;
	m->default_glyph = '?';
	snprintf(m->license, sizeof(m->license), "GPL-2.0-or-later");

	/* in the order written, a name with spaces, a range and a list of years */
	gpf_authors_add(&font->authors, "2019-2024", "Jane Q. Doe",
	                "jane@example.org");
	gpf_authors_add(&font->authors, "2021,2023", "John Doe",
	                "john@example.org");

	/* two blocks the font means to cover, one of them still empty */
	gpf_font_block_add(font, 0x0370, 0x03ff, "Greek and Coptic");
	gpf_font_block_add(font, 0x2500, 0x257f, "Box Drawing");

	mono = gpf_font_variant(font, GPF_MONO, 1);
	mono->advance = 6;

	/* a space, no ink at all */
	glyph = gpf_variant_glyph_add(mono, ' ');
	glyph->kind = GPF_GLYPH_INK;
	glyph->has_advance = 1;
	glyph->has_bearing = 1;
	glyph->advance = 6;

	add_ink(mono, '#', hash_rows, -1, 4, 6);
	add_ink(mono, 'A', a_rows, 0, 5, 6);
	add_ink(mono, '_', under_rows, 0, -1, 6);
	add_ink(mono, 'y', y_rows, 0, 2, 6);
	add_ink(mono, 0x2018, quote_rows, 1, 5, 6);
	add_ink(mono, 0x2501, wide_rows, 0, 3, 6);

	regular = gpf_font_variant(font, GPF_REGULAR, 1);
	regular->spacing_mono = 0;

	/* metrics only, the advance */
	glyph = gpf_variant_glyph_add(regular, 'A');
	glyph->kind = GPF_GLYPH_METRICS;
	glyph->has_advance = 1;
	glyph->advance = 5;

	/* metrics only, translated ink */
	glyph = gpf_variant_glyph_add(regular, 'y');
	glyph->kind = GPF_GLYPH_METRICS;
	glyph->has_advance = 1;
	glyph->has_shift = 1;
	glyph->advance = 4;
	glyph->shift_x = -1;
	glyph->shift_y = 1;

	/* a composition */
	glyph = gpf_variant_glyph_add(regular, 0x00c1);
	glyph->kind = GPF_GLYPH_COMPOSE;
	glyph->base = 'A';
	glyph->accent = 0x00b4;
	glyph->dx = 0;
	glyph->dy = 3;

	/* a redrawn glyph */
	add_ink(regular, 'i', quote_rows, 0, 5, 3);

	return font;
}

static void test_round_trip(const char *dir1, const char *dir2)
{
	char err[GPF_ERR_MAX] = "";
	struct gpf_font *font = build();
	struct gpf_font *read;

	if (gpf_font_write(font, dir1, err, sizeof(err))) {
		FAIL("write: %s", err);
		return;
	}

	read = gpf_font_read(dir1, err, sizeof(err));
	if (!read) {
		FAIL("read: %s", err);
		return;
	}

	font_cmp(font, read);

	if (gpf_font_write(read, dir2, err, sizeof(err))) {
		FAIL("rewrite: %s", err);
		return;
	}

	gpf_font_free(font);
	gpf_font_free(read);
}

static void test_files_identical(const char *dir1, const char *dir2)
{
	static const char *names[] = {"family", "mono", "regular", NULL};
	char path[4096];
	char *first, *second;
	unsigned int i;

	for (i = 0; names[i]; i++) {
		snprintf(path, sizeof(path), "%s/%s", dir1, names[i]);
		first = slurp(path);

		if (!first) {
			FAIL("%s is missing", path);
			continue;
		}

		first = strdup(first);

		snprintf(path, sizeof(path), "%s/%s", dir2, names[i]);

		second = slurp(path);

		if (!second)
			FAIL("%s is missing", path);
		else if (strcmp(first, second))
			FAIL("%s differs after a rewrite", names[i]);

		free(first);
	}
}

/*
 * The ink spec, pinned against hand written text.
 */
static void test_golden(const char *dir)
{
	char path[4096];
	const char *mono;

	static const char *expect_a =
		"glyph U+0041 'A'\n"
		"   |....####....|\n"
		"   |..##....##..|\n"
		"   |##........##|\n"
		"   |############|\n"
		"  _|##........##|\n"
		"    ^-----------^\n";

	/* the baseline mark is on a blank row, the ink is below it */
	static const char *expect_under =
		"glyph U+005F '_'\n"
		"  _|..........|\n"
		"   |..........|\n"
		"   |##########|\n"
		"    ^-----------^\n";

	/* the origin tick sits inside the ink */
	static const char *expect_hash =
		"glyph U+0023 '#'\n"
		"   |..##..##..|\n"
		"   |##########|\n"
		"   |..##..##..|\n"
		"  _|..........|\n"
		"    --^-----------^\n";

	/* an empty glyph still shows its cell */
	static const char *expect_space =
		"glyph U+0020 ' '\n"
		"  _|............|\n"
		"    ^-----------^\n";

	snprintf(path, sizeof(path), "%s/mono", dir);

	mono = slurp(path);
	if (!mono) {
		FAIL("cannot read %s", path);
		return;
	}

	CHECK(strstr(mono, expect_a), "'A' is not written as expected");
	CHECK(strstr(mono, expect_under), "'_' is not written as expected");
	CHECK(strstr(mono, expect_space), "' ' is not written as expected");
	CHECK(strstr(mono, expect_hash), "'#' is not written as expected");
}

static void rm_dir(const char *dir)
{
	static const char *names[] = {"family", "mono", "regular", "bold",
	                              "bold-mono", NULL};
	char path[4096];
	unsigned int i;

	for (i = 0; names[i]; i++) {
		snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
		unlink(path);
	}

	rmdir(dir);
}

/*
 * Removing a block takes its glyphs with it, in every variant.
 */
static void test_block_clear(void)
{
	struct gpf_font *font = build();
	struct gpf_variant *mono = gpf_font_variant(font, GPF_MONO, 1);
	struct gpf_variant *regular = gpf_font_variant(font, GPF_REGULAR, 1);
	size_t mono_cnt = gpf_variant_glyphs(mono);
	size_t regular_cnt = gpf_variant_glyphs(regular);
	unsigned int cnt;

	/* the two glyphs the build puts above U+2000 */
	cnt = gpf_font_block_clear(font, 0x2000, 0x2fff);

	CHECK(cnt == 2, "cleared %u glyphs, expected 2", cnt);
	CHECK(gpf_variant_glyphs(mono) == mono_cnt - 2,
	      "mono has %zu glyphs, expected %zu", gpf_variant_glyphs(mono),
	      mono_cnt - 2);
	CHECK(!gpf_variant_glyph(mono, 0x2018), "U+2018 survived the clear");
	CHECK(!gpf_variant_glyph(mono, 0x2501), "U+2501 survived the clear");

	/* ASCII is untouched */
	CHECK(gpf_variant_glyph(mono, 'A'), "the clear took 'A' as well");
	CHECK(gpf_variant_glyphs(regular) == regular_cnt,
	      "the clear touched regular");

	cnt = gpf_font_block_clear(font, 0x0041, 0x0041);

	CHECK(cnt == 2, "clearing 'A' hit %u glyphs, expected 2 (mono, regular)",
	      cnt);

	gpf_font_free(font);
}

/*
 * An overlay that only moves the ink or changes the advance is written as
 * numbers, and one that matches what the variant derives is not written at
 * all.  Editing materializes glyphs as ink, so this is what keeps an
 * advance change from storing a copy of the parent's ink.
 */
static void test_smallest_form(const char *dir)
{
	char err[GPF_ERR_MAX] = "";
	struct gpf_font *font = gpf_font_new();
	struct gpf_variant *mono, *regular;
	struct gpf_glyph *glyph;
	char path[4096];
	const char *text;

	static const char *rows[] = {"##", "#.", NULL};

	font->meta.ascent = 4;
	font->meta.descent = 1;

	mono = gpf_font_variant(font, GPF_MONO, 1);
	mono->advance = 6;

	add_ink(mono, 'A', rows, 0, 3, 6);
	add_ink(mono, 'B', rows, 0, 3, 6);
	add_ink(mono, 'C', rows, 0, 3, 6);

	regular = gpf_font_variant(font, GPF_REGULAR, 1);
	regular->spacing_mono = 0;

	/* the same ink, a tighter cell */
	glyph = add_ink(regular, 'A', rows, 0, 3, 4);
	/* the same ink, nudged */
	glyph = add_ink(regular, 'B', rows, 1, 3, 6);
	/* the same everything */
	glyph = add_ink(regular, 'C', rows, 0, 3, 6);

	(void)glyph;

	if (gpf_font_write(font, dir, err, sizeof(err))) {
		FAIL("write: %s", err);
		return;
	}

	snprintf(path, sizeof(path), "%s/regular", dir);

	text = slurp(path);
	if (!text) {
		FAIL("cannot read %s", path);
		return;
	}

	CHECK(strstr(text, "glyph U+0041 'A'\nadvance 4\n"),
	      "a tighter cell was not written as an advance");
	CHECK(strstr(text, "glyph U+0042 'B'\nshift 1,0\n"),
	      "a nudge was not written as a shift");
	CHECK(!strstr(text, "U+0043"),
	      "an entry that says nothing was written anyway");
	CHECK(!strstr(text, "|"), "an overlay stored a copy of the parent's ink");

	gpf_font_free(font);
}

/*
 * A letter that happens to be its base with an accent on it is written as it
 * was drawn.  The editor composes a letter once, when it is created from an
 * empty slot, and from then on it is a drawing like any other — nothing looks
 * at ink and decides it is really a composition.
 */
static void test_accented_stays_drawn(const char *dir)
{
	char err[GPF_ERR_MAX] = "";
	struct gpf_font *font = gpf_font_new();
	struct gpf_variant *mono;
	char path[4096];
	const char *text;

	/* A, an acute, and the two of them drawn as one glyph */
	static const char *a_rows[] = {"####", "#..#", "####", "#..#", NULL};
	static const char *acute_rows[] = {".#", "#.", NULL};
	static const char *a_acute_rows[] = {
		"..#.",
		".#..",
		"....",
		"####",
		"#..#",
		"####",
		"#..#",
		NULL,
	};

	font->meta.ascent = 8;
	font->meta.descent = 1;

	mono = gpf_font_variant(font, GPF_MONO, 1);
	mono->advance = 6;

	add_ink(mono, 'A', a_rows, 0, 4, 6);
	add_ink(mono, 0x00b4, acute_rows, 1, 7, 6);
	add_ink(mono, 0x00c1, a_acute_rows, 0, 7, 6);

	if (gpf_font_write(font, dir, err, sizeof(err))) {
		FAIL("write: %s", err);
		return;
	}

	snprintf(path, sizeof(path), "%s/mono", dir);

	text = slurp(path);
	if (!text) {
		FAIL("cannot read %s", path);
		return;
	}

	CHECK(!strstr(text, "compose"),
	      "a hand drawn letter was turned into a composition");
	CHECK(strstr(text, "glyph U+00C1 'Á'\n   |"),
	      "a hand drawn letter was not written as ink");

	gpf_font_free(font);

	/* and it renders the same as the ink it was detected from */
	font = gpf_font_read(dir, err, sizeof(err));
	if (!font) {
		FAIL("read: %s", err);
		return;
	}

	mono = gpf_font_variant(font, GPF_MONO, 0);

	{
		struct gpf_resolved res;

		if (gpf_resolve(font, GPF_MONO, 0x00c1, &res)) {
			FAIL("the composition does not resolve");
		} else {
			CHECK(ink_eq(&res.glyph, a_acute_rows),
			      "the composition does not render as it was drawn");
			gpf_resolved_clear(&res);
		}
	}

	gpf_font_free(font);
}

/*
 * A font started from the properties alone: the metadata as given, an empty
 * mono variant with the cell asked for, and a directory that reads back as
 * the same thing.
 */
static void test_create(const char *dir)
{
	char err[GPF_ERR_MAX] = "";
	struct gpf_family_meta meta;
	struct gpf_font *font, *read;
	struct gpf_variant *mono;

	gpf_family_meta_guess(&meta, "", 16, 12, 4, 9);

	CHECK(!strcmp(meta.family, "New"), "an empty family is '%s'",
	      meta.family);
	CHECK(meta.em == 16, "the em starts at %i, not at the box", meta.em);
	CHECK(meta.overline_pos == 13,
	      "the overline is at %i, not above the ascent", meta.overline_pos);

	font = gpf_font_create(&meta, 9);
	if (!font) {
		FAIL("gpf_font_create() failed");
		return;
	}

	CHECK(gpf_family_meta_same(&font->meta, &meta),
	      "the font does not carry the metadata");

	mono = font->variants[GPF_MONO];

	CHECK(mono && mono->spacing_mono && mono->advance == 9 &&
	      !gpf_variant_glyphs(mono), "the mono variant is not an empty cell");

	if (gpf_font_write(font, dir, err, sizeof(err))) {
		FAIL("write: %s", err);
		gpf_font_free(font);
		return;
	}

	read = gpf_font_read(dir, err, sizeof(err));
	if (!read) {
		FAIL("read: %s", err);
		gpf_font_free(font);
		return;
	}

	CHECK(gpf_family_meta_same(&read->meta, &meta),
	      "the metadata did not survive the write");
	CHECK(read->variants[GPF_MONO] &&
	      read->variants[GPF_MONO]->advance == 9,
	      "the mono cell did not survive the write");

	gpf_font_free(read);
	gpf_font_free(font);
}

int main(void)
{
	char dir1[] = "/tmp/gpforge-test-XXXXXX";
	char dir2[256];

	setvbuf(stdout, NULL, _IONBF, 0);

	if (!mkdtemp(dir1)) {
		perror("mkdtemp");
		return 1;
	}

	snprintf(dir2, sizeof(dir2), "%s-rewrite", dir1);

	test_block_clear();
	test_smallest_form(dir2);
	test_accented_stays_drawn(dir2);
	test_create(dir2);
	test_round_trip(dir1, dir2);
	test_files_identical(dir1, dir2);
	test_golden(dir1);

	rm_dir(dir1);
	rm_dir(dir2);

	if (failures) {
		printf("%u failures\n", failures);
		return 1;
	}

	printf("unit: ok\n");

	return 0;
}
