/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/*
 * The command line half of gpforge: BDF import and export, the C export, and
 * a canonical dump used to compare an imported font against a font loaded
 * back from a directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "font.h"
#include "resolve.h"
#include "lint.h"
#include "format.h"
#include "format_bdf.h"
#include "format_c.h"


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

static void dump_glyph(const struct gpf_glyph *glyph)
{
	unsigned int x, y;

	printf("U+%04X ", glyph->code);

	switch (glyph->kind) {
	case GPF_GLYPH_INK:
		printf("ink adv %i brg %i,%i size %ux%u\n",
		       glyph->advance, glyph->bearing_x, glyph->bearing_y,
		       glyph->width, glyph->height);

		for (y = 0; y < glyph->height; y++) {
			printf("\t");
			for (x = 0; x < glyph->width; x++)
				putchar(gpf_glyph_pixel(glyph, x, y) ? '#' : '.');
			putchar('\n');
		}
	break;
	case GPF_GLYPH_METRICS:
		printf("metrics");
		if (glyph->has_advance)
			printf(" adv %i", glyph->advance);
		if (glyph->has_shift)
			printf(" shift %i,%i", glyph->shift_x, glyph->shift_y);
		putchar('\n');
	break;
	case GPF_GLYPH_COMPOSE:
		printf("compose U+%04X U+%04X at %i,%i\n",
		       glyph->base, glyph->accent, glyph->dx, glyph->dy);
	break;
	}
}

static void dump(struct gpf_font *font)
{
	struct gpf_family_meta *m = &font->meta;
	unsigned int i;
	size_t j;

	printf("family %s\n", m->family);
	printf("size %i ascent %i descent %i line_gap %i\n",
	       m->size, m->ascent, m->descent, m->line_gap);
	printf("em %i x_height %i cap_height %i ch_width %i\n",
	       m->em, m->x_height, m->cap_height, m->ch_width);
	printf("underline %i %i strikethrough %i %i overline %i %i\n",
	       m->underline_pos, m->underline_thickness,
	       m->strike_pos, m->strike_thickness,
	       m->overline_pos, m->overline_thickness);
	printf("default_glyph U+%04X\n", m->default_glyph);
	printf("license %s\n", m->license);

	for (j = 0; j < gp_vec_len(font->authors); j++) {
		printf("author %s %s <%s>\n", font->authors[j].years,
		       font->authors[j].name, font->authors[j].email);
	}

	for (i = 0; i < GPF_VARIANTS; i++) {
		struct gpf_variant *variant = font->variants[i];

		if (!variant)
			continue;

		printf("variant %s spacing %s derive %s advance %i glyphs %zu\n",
		       gpf_variant_name(variant->id),
		       variant->spacing_mono ? "mono" : "proportional",
		       variant->derive_embolden ? "embolden" : "none",
		       variant->advance, gpf_variant_glyphs(variant));

		for (j = 0; j < gpf_variant_glyphs(variant); j++)
			dump_glyph(&variant->glyphs[j]);
	}
}

/*
 * Dumps a resolved face, which is what a consumer of the font would see.
 */
static int face(struct gpf_font *font, const char *name)
{
	enum gpf_variant_id id;
	uint32_t *vec;
	size_t i;

	if (gpf_variant_by_name(name, &id)) {
		fprintf(stderr, "unknown variant '%s'\n", name);
		return 1;
	}

	vec = gpf_font_codes(font);
	if (!vec)
		return 1;

	for (i = 0; i < gp_vec_len(vec); i++) {
		struct gpf_resolved res;
		unsigned int x, y;

		if (gpf_resolve(font, id, vec[i], &res))
			continue;

		printf("U+%04X %s adv %i brg %i,%i size %ux%u\n",
		       vec[i], gpf_provenance_name(res.prov),
		       res.glyph.advance, res.glyph.bearing_x,
		       res.glyph.bearing_y, res.glyph.width, res.glyph.height);

		for (y = 0; y < res.glyph.height; y++) {
			printf("\t");
			for (x = 0; x < res.glyph.width; x++)
				putchar(gpf_glyph_pixel(&res.glyph, x, y) ? '#' : '.');
			putchar('\n');
		}

		gpf_resolved_clear(&res);
	}

	gp_vec_free(vec);

	return 0;
}

static void usage(const char *name)
{
	fprintf(stderr,
	        "usage: %s import <font.bdf> <dir>\n"
	        "       %s write  <dir> <out-dir>\n"
	        "       %s dump   <font.bdf|dir>\n"
	        "       %s face   <font.bdf|dir> <variant>\n"
	        "       %s lint   <font.bdf|dir>\n"
	        "       %s c      <font.bdf|dir> <font_id> <family-name>\n"
	        "       %s bdf    <font.bdf|dir> <variant>\n",
	        name, name, name, name, name, name, name);
}

int main(int argc, char *argv[])
{
	char err[GPF_ERR_MAX] = "";
	struct gpf_font *font;

	if (argc < 3) {
		usage(argv[0]);
		return 1;
	}

	if (!strcmp(argv[1], "dump")) {
		font = load(argv[2], err, sizeof(err));
		if (!font) {
			fprintf(stderr, "%s\n", err);
			return 1;
		}

		dump(font);
		gpf_font_free(font);

		return 0;
	}

	if (!strcmp(argv[1], "c")) {
		if (argc < 5) {
			usage(argv[0]);
			return 1;
		}

		font = load(argv[2], err, sizeof(err));
		if (!font) {
			fprintf(stderr, "%s\n", err);
			return 1;
		}

		if (gpf_export_c(font, argv[3], argv[4], stdout, err, sizeof(err))) {
			fprintf(stderr, "%s\n", err);
			gpf_font_free(font);
			return 1;
		}

		gpf_font_free(font);

		return 0;
	}

	if (!strcmp(argv[1], "bdf")) {
		enum gpf_variant_id id;

		if (argc < 4) {
			usage(argv[0]);
			return 1;
		}

		if (gpf_variant_by_name(argv[3], &id)) {
			fprintf(stderr, "unknown variant '%s'\n", argv[3]);
			return 1;
		}

		font = load(argv[2], err, sizeof(err));
		if (!font) {
			fprintf(stderr, "%s\n", err);
			return 1;
		}

		if (gpf_bdf_write(font, id, stdout, err, sizeof(err))) {
			fprintf(stderr, "%s\n", err);
			gpf_font_free(font);
			return 1;
		}

		gpf_font_free(font);

		return 0;
	}

	if (!strcmp(argv[1], "lint")) {
		struct gpf_lint_finding *findings;
		unsigned int errors = 0, warns = 0;
		size_t i;

		font = load(argv[2], err, sizeof(err));
		if (!font) {
			fprintf(stderr, "%s\n", err);
			return 1;
		}

		findings = gpf_lint(font);

		for (i = 0; findings && i < gp_vec_len(findings); i++) {
			struct gpf_lint_finding *f = &findings[i];

			if (f->sev == GPF_LINT_ERROR)
				errors++;
			else
				warns++;

			printf("%-10s U+%04X  %-5s  %s\n",
			       gpf_variant_name(f->id), f->code,
			       gpf_lint_severity_name(f->sev), f->msg);
		}

		printf("%u errors, %u warnings\n", errors, warns);

		gp_vec_free(findings);
		gpf_font_free(font);

		return errors ? 1 : 0;
	}

	if (!strcmp(argv[1], "face")) {
		int ret;

		if (argc < 4) {
			usage(argv[0]);
			return 1;
		}

		font = load(argv[2], err, sizeof(err));
		if (!font) {
			fprintf(stderr, "%s\n", err);
			return 1;
		}

		ret = face(font, argv[3]);

		gpf_font_free(font);

		return ret;
	}

	if (!strcmp(argv[1], "import") || !strcmp(argv[1], "write")) {
		if (argc < 4) {
			usage(argv[0]);
			return 1;
		}

		font = load(argv[2], err, sizeof(err));
		if (!font) {
			fprintf(stderr, "%s\n", err);
			return 1;
		}

		if (gpf_font_write(font, argv[3], err, sizeof(err))) {
			fprintf(stderr, "%s\n", err);
			gpf_font_free(font);
			return 1;
		}

		gpf_font_free(font);

		return 0;
	}

	usage(argv[0]);

	return 1;
}
