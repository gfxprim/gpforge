/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/*
 * The lint checks.  A font small enough to be wrong on purpose: every check
 * here is something the corpus is clean of, which is what makes a finding
 * worth showing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utils/gp_vec.h>

#include "font.h"
#include "lint.h"
#include "test_util.h"

static const char *box[] = {
	"####",
	"#..#",
	"####",
	NULL,
};

static struct gpf_font *build(struct gpf_variant **mono)
{
	struct gpf_font *font = gpf_font_new();

	font->meta.ascent = 8;
	font->meta.descent = 2;
	font->meta.x_height = 5;
	font->meta.cap_height = 7;

	*mono = gpf_font_variant(font, GPF_MONO, 1);

	(*mono)->spacing_mono = 1;
	(*mono)->advance = 6;

	return font;
}

/*
 * The first finding for one glyph, which is what the editor shows, and what
 * the whole font report is made of.
 */
static int findings_for(struct gpf_font *font, enum gpf_variant_id id,
                        uint32_t code, struct gpf_lint_finding *out)
{
	return gpf_lint_glyph(font, id, code, out);
}

static void test_clean(void)
{
	struct gpf_variant *mono;
	struct gpf_font *font = build(&mono);
	struct gpf_lint_finding *findings;

	add_ink(mono, 'A', box, 1, 7, 6);

	findings = gpf_lint(font);

	CHECK(findings && !gp_vec_len(findings), "a clean font has %zu findings",
	      findings ? gp_vec_len(findings) : 0);

	gp_vec_free(findings);
	gpf_font_free(font);
}

/*
 * A monospace cell is a contract: ink outside it overlaps the next glyph, and
 * an advance of its own is not a thing a monospace glyph has.
 */
static void test_mono_cell(void)
{
	struct gpf_variant *mono;
	struct gpf_font *font = build(&mono);
	struct gpf_lint_finding finding;

	/* four wide at bearing 3 ends at 7, the cell is 6 */
	add_ink(mono, 'A', box, 3, 7, 6);

	CHECK(!findings_for(font, GPF_MONO, 'A', &finding),
	      "ink out of the cell was not found");
	CHECK(finding.sev == GPF_LINT_ERROR, "ink out of the cell is a %s",
	      gpf_lint_severity_name(finding.sev));

	add_ink(mono, 'B', box, 1, 7, 8);

	CHECK(!findings_for(font, GPF_MONO, 'B', &finding),
	      "an advance of its own was not found");
	CHECK(finding.sev == GPF_LINT_ERROR, "a wrong advance is a %s",
	      gpf_lint_severity_name(finding.sev));

	/* negative bearings walk into the previous cell */
	add_ink(mono, 'C', box, -1, 7, 6);

	CHECK(!findings_for(font, GPF_MONO, 'C', &finding),
	      "ink left of the origin was not found");

	gpf_font_free(font);
}

/*
 * The heights are measured off the font itself, so a letter that does not
 * reach them was drawn one pixel out.
 */
static void test_heights(void)
{
	struct gpf_variant *mono;
	struct gpf_font *font = build(&mono);
	struct gpf_lint_finding finding;

	/* x-height is 5, this one tops out at 6 */
	add_ink(mono, 'x', box, 1, 6, 6);

	CHECK(!findings_for(font, GPF_MONO, 'x', &finding),
	      "an x-height of 6 against 5 was not found");
	CHECK(finding.sev == GPF_LINT_WARN, "an x-height is a %s",
	      gpf_lint_severity_name(finding.sev));

	add_ink(mono, 'H', box, 1, 7, 6);

	CHECK(findings_for(font, GPF_MONO, 'H', &finding),
	      "a cap at the cap height says: %s", finding.msg);

	gpf_font_free(font);
}

/*
 * Ink above the ascent overlaps the line above it, which is what an accent
 * that does not fit does.
 */
static void test_ascent(void)
{
	struct gpf_variant *mono;
	struct gpf_font *font = build(&mono);
	struct gpf_lint_finding finding;

	add_ink(mono, 0x00c1, box, 1, 9, 6);

	CHECK(!findings_for(font, GPF_MONO, 0x00c1, &finding),
	      "ink above the ascent was not found");
	CHECK(finding.sev == GPF_LINT_WARN, "ink above the ascent is a %s",
	      gpf_lint_severity_name(finding.sev));

	gpf_font_free(font);
}

/*
 * A composition whose parts the font has lost renders as nothing, and the
 * entry is the only thing left that says what it was.
 */
static void test_compose(void)
{
	struct gpf_variant *mono;
	struct gpf_font *font = build(&mono);
	struct gpf_glyph *glyph;
	struct gpf_lint_finding finding;

	glyph = gpf_variant_glyph_add(mono, 0x00c1);

	glyph->kind = GPF_GLYPH_COMPOSE;
	glyph->base = 'A';
	glyph->accent = 0x0301;

	CHECK(!findings_for(font, GPF_MONO, 0x00c1, &finding),
	      "a composition with no parts was not found");
	CHECK(finding.sev == GPF_LINT_ERROR, "a broken composition is a %s",
	      gpf_lint_severity_name(finding.sev));

	gpf_font_free(font);
}

/*
 * Cell art is drawn to fill the cell and to touch the next one, so the
 * geometry rules do not apply to it.
 */
static void test_cell_art(void)
{
	struct gpf_variant *mono;
	struct gpf_font *font = build(&mono);
	struct gpf_lint_finding finding;

	add_ink(mono, 0x2500, box, 3, 7, 6);

	CHECK(findings_for(font, GPF_MONO, 0x2500, &finding),
	      "box drawing was linted: %s", finding.msg);

	gpf_font_free(font);
}

/*
 * The report is every glyph in every variant, sorted so that walking it goes
 * forwards through the font.
 */
static void test_report(void)
{
	struct gpf_variant *mono;
	struct gpf_font *font = build(&mono);
	struct gpf_lint_finding *findings;
	size_t i;

	add_ink(mono, 'A', box, 3, 7, 6);
	add_ink(mono, 'B', box, 1, 7, 8);
	add_ink(mono, 'x', box, 1, 6, 6);

	findings = gpf_lint(font);

	CHECK(findings && gp_vec_len(findings) >= 3, "the report has %zu findings",
	      findings ? gp_vec_len(findings) : 0);

	for (i = 1; findings && i < gp_vec_len(findings); i++) {
		CHECK(findings[i - 1].id < findings[i].id ||
		      (findings[i - 1].id == findings[i].id &&
		       findings[i - 1].code <= findings[i].code),
		      "finding %zu is out of order", i);
	}

	gp_vec_free(findings);
	gpf_font_free(font);
}

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	test_clean();
	test_mono_cell();
	test_heights();
	test_ascent();
	test_compose();
	test_cell_art();
	test_report();

	if (failures) {
		printf("%u failures\n", failures);
		return 1;
	}

	printf("lint: ok\n");

	return 0;
}
