/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <utils/gp_utf.h>
#include <utils/gp_vec.h>
#include <utils/gp_vec_str.h>

#include "resolve.h"
#include "format.h"

#define LINE_MAX_LEN 1024

int gpf_code_printable(uint32_t code)
{
	if (code < 0x20 || code == 0x7f)
		return 0;

	/* C1 controls */
	if (code >= 0x80 && code < 0xa0)
		return 0;

	if (code > 0x10ffff)
		return 0;

	return 1;
}

/* ------------------------------------------------------------------ write */

static void write_glyph_header(FILE *f, const struct gpf_glyph *glyph)
{
	char buf[5];

	fprintf(f, "glyph U+%04X", glyph->code);

	if (gpf_code_printable(glyph->code)) {
		buf[gp_to_utf8(glyph->code, buf)] = 0;
		fprintf(f, " '%s'", buf);
	}

	fputc('\n', f);
}

static void write_ink(FILE *f, const struct gpf_glyph *glyph)
{
	int top = glyph->bearing_y > 1 ? glyph->bearing_y : 1;
	int bottom = glyph->bearing_y - (int)glyph->height;
	int rows = top - (bottom < 0 ? bottom : 0);
	int left = glyph->bearing_x < 0 ? glyph->bearing_x : 0;
	int right = glyph->bearing_x + (int)glyph->width;
	int ink_top = top - glyph->bearing_y;
	int cols, len, origin, advance, x, y, i;

	/*
	 * An empty glyph still has to show its cell, so it's drawn as a
	 * single blank row spanning the advance.
	 */
	if (!glyph->width)
		right = left + (glyph->advance > 0 ? glyph->advance : 1);

	cols = right - left;
	if (cols < 1)
		cols = 1;

	for (y = 0; y < rows; y++) {
		fputs(y == top - 1 ? "  _|" : "   |", f);

		for (x = 0; x < cols; x++) {
			int ix = left + x - glyph->bearing_x;
			int iy = y - ink_top;
			int set = 0;

			if (ix >= 0 && iy >= 0)
				set = gpf_glyph_pixel(glyph, ix, iy);

			fputs(set ? "##" : "..", f);
		}

		fputs("|\n", f);
	}

	origin = -left;
	advance = glyph->advance - left;

	len = 2 * (right > glyph->advance ? right - left : advance) + 1;

	fputs("    ", f);

	for (i = 0; i < len; i++)
		fputc(i == 2 * origin || i == 2 * advance ? '^' : '-', f);

	fputc('\n', f);
}

/*
 * An overlay entry is written in the smallest form that is true: an entry
 * that only moves the ink or changes the advance is numbers, and one that
 * matches what the variant would derive is not written at all.  Editing a
 * glyph materializes it as ink, so without this every advance change would
 * store a duplicate of the parent's ink.
 */
static void write_glyph(FILE *f, struct gpf_font *font, enum gpf_variant_id id,
                        const struct gpf_glyph *glyph)
{
	struct gpf_glyph numbers;

	switch (gpf_entry_form(font, id, glyph, &numbers)) {
	case GPF_ENTRY_NONE:
		return;
	case GPF_ENTRY_METRICS:
		write_glyph_header(f, glyph);

		if (numbers.has_advance)
			fprintf(f, "advance %i\n", numbers.advance);

		if (numbers.has_shift) {
			fprintf(f, "shift %i,%i\n",
			        numbers.shift_x, numbers.shift_y);
		}
		return;
	case GPF_ENTRY_INK:
	break;
	}

	write_glyph_header(f, glyph);

	switch (glyph->kind) {
	case GPF_GLYPH_INK:
		write_ink(f, glyph);
	break;
	case GPF_GLYPH_METRICS:
		if (glyph->has_advance)
			fprintf(f, "advance %i\n", glyph->advance);
		if (glyph->has_shift) {
			fprintf(f, "shift %i,%i\n",
			        glyph->shift_x, glyph->shift_y);
		}
	break;
	case GPF_GLYPH_COMPOSE:
		fprintf(f, "compose U+%04X U+%04X at %i,%i\n",
		        glyph->base, glyph->accent, glyph->dx, glyph->dy);
	break;
	}
}

static int write_variant(struct gpf_font *font, struct gpf_variant *variant,
                         const char *dir, char *err, size_t err_len)
{
	char path[4096];
	FILE *f;
	size_t i;
	enum gpf_variant_id parent = gpf_variant_parent(variant->id);

	snprintf(path, sizeof(path), "%s/%s", dir, gpf_variant_name(variant->id));

	f = fopen(path, "w");
	if (!f) {
		snprintf(err, err_len, "%s: %s", path, strerror(errno));
		return 1;
	}

	if (variant->comments)
		fputs(variant->comments, f);

	fprintf(f, "%-9s %s\n", "variant", gpf_variant_name(variant->id));

	if (parent != GPF_NO_PARENT)
		fprintf(f, "%-9s %s\n", "parent", gpf_variant_name(parent));

	fprintf(f, "%-9s %s\n", "spacing",
	        variant->spacing_mono ? "mono" : "proportional");

	if (variant->derive_embolden)
		fprintf(f, "%-9s %s\n", "derive", "embolden");

	if (variant->advance)
		fprintf(f, "%-9s %i\n", "advance", variant->advance);

	for (i = 0; i < gpf_variant_glyphs(variant); i++) {
		long pos = ftell(f);

		fputc('\n', f);

		write_glyph(f, font, variant->id, &variant->glyphs[i]);

		/* an entry that says nothing is not written, blank line and all */
		if (ftell(f) == pos + 1) {
			fflush(f);

			if (ftruncate(fileno(f), pos))
				break;

			fseek(f, pos, SEEK_SET);
		}
	}

	if (fclose(f)) {
		snprintf(err, err_len, "%s: %s", path, strerror(errno));
		return 1;
	}

	return 0;
}

static int write_family(struct gpf_font *font, const char *dir,
                        char *err, size_t err_len)
{
	struct gpf_family_meta *m = &font->meta;
	char path[4096];
	FILE *f;
	size_t i;

	snprintf(path, sizeof(path), "%s/family", dir);

	f = fopen(path, "w");
	if (!f) {
		snprintf(err, err_len, "%s: %s", path, strerror(errno));
		return 1;
	}

	if (font->comments)
		fputs(font->comments, f);

	fprintf(f, "%-14s %s\n", "family", m->family);
	fprintf(f, "%-14s %i\n", "size", m->size);
	fprintf(f, "%-14s %i\n", "ascent", m->ascent);
	fprintf(f, "%-14s %i\n", "descent", m->descent);
	fprintf(f, "%-14s %i\n", "line_gap", m->line_gap);
	fputc('\n', f);
	fprintf(f, "%-14s %i\n", "em", m->em);
	fprintf(f, "%-14s %i\n", "x_height", m->x_height);
	fprintf(f, "%-14s %i\n", "cap_height", m->cap_height);
	fprintf(f, "%-14s %i\n", "ch_width", m->ch_width);
	fputc('\n', f);
	fprintf(f, "%-14s %i %i\n", "underline",
	        m->underline_pos, m->underline_thickness);
	fprintf(f, "%-14s %i %i\n", "strikethrough",
	        m->strike_pos, m->strike_thickness);
	fprintf(f, "%-14s %i %i\n", "overline",
	        m->overline_pos, m->overline_thickness);

	if (m->default_glyph || m->license[0])
		fputc('\n', f);

	if (m->default_glyph)
		fprintf(f, "%-14s U+%04X\n", "default_glyph", m->default_glyph);

	if (m->license[0])
		fprintf(f, "%-14s %s\n", "license", m->license);

	if (gp_vec_len(font->authors))
		fputc('\n', f);

	for (i = 0; i < gp_vec_len(font->authors); i++) {
		fprintf(f, "%-14s %s %s <%s>\n", "author",
		        font->authors[i].years, font->authors[i].name,
		        font->authors[i].email);
	}

	/*
	 * The blocks the font means to cover.  A block that has glyphs needs
	 * no line here, this is for the ones that are still empty.
	 */
	if (gp_vec_len(font->blocks))
		fputc('\n', f);

	for (i = 0; i < gp_vec_len(font->blocks); i++) {
		fprintf(f, "%-14s U+%04X U+%04X %s\n", "block",
		        font->blocks[i].min, font->blocks[i].max,
		        font->blocks[i].name);
	}

	if (fclose(f)) {
		snprintf(err, err_len, "%s: %s", path, strerror(errno));
		return 1;
	}

	return 0;
}

int gpf_font_write(struct gpf_font *self, const char *dir,
                   char *err, size_t err_len)
{
	unsigned int i;

	if (mkdir(dir, 0755) && errno != EEXIST) {
		snprintf(err, err_len, "%s: %s", dir, strerror(errno));
		return 1;
	}

	if (write_family(self, dir, err, err_len))
		return 1;

	for (i = 0; i < GPF_VARIANTS; i++) {
		if (!self->variants[i])
			continue;

		if (write_variant(self, self->variants[i], dir, err, err_len))
			return 1;
	}

	return 0;
}

/* ------------------------------------------------------------------- read */

struct parser {
	const char *path;
	unsigned int line;
	char *err;
	size_t err_len;
};

static int perr(struct parser *p, const char *fmt, ...)
{
	char msg[GPF_ERR_MAX];
	va_list va;

	va_start(va, fmt);
	vsnprintf(msg, sizeof(msg), fmt, va);
	va_end(va);

	snprintf(p->err, p->err_len, "%s:%u: %s", p->path, p->line, msg);

	return 1;
}

static char *skip_space(char *str)
{
	while (*str == ' ' || *str == '\t')
		str++;

	return str;
}

/*
 * Strips the trailing newline and whitespace and cuts a comment off.  A '#'
 * only starts a comment at the start of a line or after a whitespace, which
 * keeps it out of a glyph annotation, and ink rows are left alone since
 * '#' is also the ink.
 */
static void strip_line(char *line)
{
	size_t len = strlen(line);
	char *start = skip_space(line);
	char *i;

	while (len && (line[len-1] == '\n' || line[len-1] == '\r' ||
	               line[len-1] == ' ' || line[len-1] == '\t'))
		line[--len] = 0;

	if (*start == '|' || *start == '_' || *start == '^')
		return;

	for (i = line; *i; i++) {
		if (*i != '#')
			continue;

		if (i == line || i[-1] == ' ' || i[-1] == '\t') {
			*i = 0;
			while (i > line && (i[-1] == ' ' || i[-1] == '\t'))
				*--i = 0;
			return;
		}
	}
}

static int line_is_comment(const char *line)
{
	const char *str = skip_space((char *)line);

	return *str == '#';
}

/*
 * Splits a header line into a key and the rest.
 */
static char *split_key(char *line, char **key)
{
	char *val;

	*key = skip_space(line);

	val = *key;
	while (*val && *val != ' ' && *val != '\t')
		val++;

	if (*val)
		*val++ = 0;

	return skip_space(val);
}

static int parse_int(struct parser *p, const char *str, int *val)
{
	char *end;
	long res;

	if (!*str)
		return perr(p, "expected a number");

	errno = 0;
	res = strtol(str, &end, 10);

	if (errno || *end)
		return perr(p, "invalid number '%s'", str);

	*val = res;

	return 0;
}

static int parse_two_ints(struct parser *p, char *str, int *a, int *b)
{
	char *second = str;

	while (*second && *second != ' ' && *second != '\t')
		second++;

	if (!*second)
		return perr(p, "expected two numbers");

	*second++ = 0;

	if (parse_int(p, str, a))
		return 1;

	return parse_int(p, skip_space(second), b);
}

static int parse_code(struct parser *p, const char *str, uint32_t *code)
{
	char *end;
	unsigned long res;

	if (str[0] != 'U' || str[1] != '+')
		return perr(p, "expected U+XXXX, got '%s'", str);

	errno = 0;
	res = strtoul(str + 2, &end, 16);

	if (errno || (*end && *end != ' ' && *end != '\t'))
		return perr(p, "invalid codepoint '%s'", str);

	*code = res;

	return 0;
}

/*
 * A growable vector of the lines of a single glyph block.
 */
struct block {
	/* a gp_vec of the lines of a single glyph block */
	char **lines;
	unsigned int first_line;
	uint32_t code;
};

static int block_add(struct block *self, const char *line)
{
	char *str = strdup(line);

	if (!str)
		return 1;

	if (!self->lines) {
		self->lines = gp_vec_new(0, sizeof(char *));
		if (!self->lines) {
			free(str);
			return 1;
		}
	}

	if (!GP_VEC_APPEND(self->lines, str)) {
		free(str);
		return 1;
	}

	return 0;
}

static void block_free(struct block *self)
{
	size_t i;

	for (i = 0; i < gp_vec_len(self->lines); i++)
		free(self->lines[i]);

	gp_vec_free(self->lines);
	self->lines = NULL;
}

static int row_ink_start(struct parser *p, const char *line, int *start)
{
	const char *bar = strchr(line, '|');

	if (!bar)
		return perr(p, "ink row without a '|'");

	*start = bar - line + 1;

	return 0;
}

static int parse_ink(struct parser *p, struct gpf_variant *variant,
                         struct block *block, size_t rows_cnt,
                         char **rows, const char *ruler, int marker)
{
	struct gpf_glyph *glyph;
	uint8_t *bits;
	int start = 0, cols = 0, first, last, left, top;
	size_t y;
	int x;

	if (!ruler)
		return perr(p, "ink without a ruler");

	if (marker < 0)
		return perr(p, "ink without a '_' baseline mark");

	if (row_ink_start(p, rows[0], &start))
		return 1;

	cols = (int)strlen(rows[0]) - start - 1;

	if (cols < 0 || cols % 2)
		return perr(p, "malformed ink row");

	cols /= 2;

	first = strchr(ruler, '^') - ruler;
	last = strrchr(ruler, '^') - ruler;

	if ((first - start) % 2 || (last - first) % 2)
		return perr(p, "ruler is not aligned with the ink");

	left = -(first - start) / 2;
	top = marker + 1;

	bits = calloc(1, (size_t)cols * rows_cnt + 1);
	if (!bits)
		return perr(p, "out of memory");

	for (y = 0; y < rows_cnt; y++) {
		int rstart;

		if (row_ink_start(p, rows[y], &rstart)) {
			free(bits);
			return 1;
		}

		if (rstart != start || (int)strlen(rows[y]) != start + 2 * cols + 1) {
			free(bits);
			return perr(p, "ink rows are not aligned");
		}

		if (rows[y][start + 2 * cols] != '|') {
			free(bits);
			return perr(p, "ink row without a closing '|'");
		}

		for (x = 0; x < cols; x++) {
			char a = rows[y][start + 2 * x];
			char b = rows[y][start + 2 * x + 1];

			if (a != b || (a != '#' && a != '.')) {
				free(bits);
				return perr(p, "expected '##' or '..' in an ink row");
			}

			bits[y * cols + x] = (a == '#');
		}
	}

	glyph = gpf_variant_glyph_add(variant, block->code);
	if (!glyph) {
		free(bits);
		return perr(p, "out of memory");
	}

	glyph->kind = GPF_GLYPH_INK;
	glyph->has_advance = 1;
	glyph->has_bearing = 1;
	glyph->advance = (last - first) / 2;

	if (gpf_glyph_set_bits(glyph, bits, cols, rows_cnt, left, top)) {
		free(bits);
		return perr(p, "out of memory");
	}

	free(bits);

	return 0;
}

static int parse_glyph_block(struct parser *parser, struct gpf_variant *variant,
                             struct block *block)
{
	/* the block is parsed after it has been read, so keep our own line */
	struct parser bp = *parser;
	struct parser *p = &bp;
	char *rows[256];
	size_t rows_cnt = 0;
	char *ruler = NULL;
	int marker = -1;
	struct gpf_glyph *glyph;
	size_t i;
	int has_numbers = 0;

	p->line = block->first_line;

	for (i = 0; i < gp_vec_len(block->lines); i++) {
		char *line = block->lines[i];
		char *start = skip_space(line);

		if (*start == '|' || *start == '_') {
			if (rows_cnt >= 256)
				return perr(p, "ink too tall");

			if (*start == '_')
				marker = rows_cnt;

			rows[rows_cnt++] = line;
			continue;
		}

		/* a ruler starts with dashes when the origin is inside it */
		if (*start == '^' || *start == '-') {
			if (ruler)
				return perr(p, "more than one ruler");

			ruler = line;
			continue;
		}

		has_numbers = 1;
	}

	if (rows_cnt && has_numbers)
		return perr(p, "a glyph block mixes ink with numbers");

	if (rows_cnt)
		return parse_ink(p, variant, block, rows_cnt, rows, ruler, marker);

	if (ruler)
		return perr(p, "a ruler without ink");

	glyph = gpf_variant_glyph_add(variant, block->code);
	if (!glyph)
		return perr(p, "out of memory");

	glyph->kind = GPF_GLYPH_METRICS;

	for (i = 0; i < gp_vec_len(block->lines); i++) {
		char *key, *val;

		p->line = block->first_line + i + 1;

		val = split_key(block->lines[i], &key);

		if (!strcmp(key, "advance")) {
			if (parse_int(p, val, &glyph->advance))
				return 1;
			glyph->has_advance = 1;
			continue;
		}

		if (!strcmp(key, "shift")) {
			char *comma = strchr(val, ',');

			if (!comma)
				return perr(p, "shift is 'dx,dy'");

			*comma++ = 0;

			if (parse_int(p, val, &glyph->shift_x))
				return 1;

			if (parse_int(p, skip_space(comma), &glyph->shift_y))
				return 1;

			glyph->has_shift = 1;
			continue;
		}

		if (!strcmp(key, "compose")) {
			char *accent, *at;

			glyph->kind = GPF_GLYPH_COMPOSE;

			accent = strchr(val, ' ');
			if (!accent)
				return perr(p, "compose needs a base and an accent");
			*accent++ = 0;

			if (parse_code(p, val, &glyph->base))
				return 1;

			accent = skip_space(accent);

			at = strstr(accent, " at ");
			if (!at)
				return perr(p, "compose needs an 'at dx,dy'");
			*at = 0;
			at += 4;

			if (parse_code(p, accent, &glyph->accent))
				return 1;

			at = skip_space(at);

			{
				char *comma = strchr(at, ',');

				if (!comma)
					return perr(p, "compose offset is 'dx,dy'");

				*comma++ = 0;

				if (parse_int(p, at, &glyph->dx))
					return 1;

				if (parse_int(p, skip_space(comma), &glyph->dy))
					return 1;
			}

			continue;
		}

		return perr(p, "unknown key '%s' in a glyph block", key);
	}

	return 0;
}

static int append_comment(char **dst, const char *line)
{
	char *str = *dst;

	if (!str) {
		str = gp_vec_str_new();
		if (!str)
			return 1;

		*dst = str;
	}

	str = gp_vec_str_append(str, line);
	if (!str)
		return 1;

	*dst = str;

	str = gp_vec_str_append(str, "\n");
	if (!str)
		return 1;

	*dst = str;

	return 0;
}

/*
 * An author line is `years name <email>`: the years are the first field, the
 * email the last, and the name is everything in between, spaces and all.
 */
static int parse_author(struct parser *p, struct gpf_font *font, char *val)
{
	char *name, *email, *end;
	const char *msg;

	name = strpbrk(val, " \t");
	if (!name)
		return perr(p, "author needs years, a name and an <email>");

	*name++ = 0;
	name = skip_space(name);

	email = strrchr(name, '<');
	end = name + strlen(name) - 1;

	if (!email || *end != '>')
		return perr(p, "author needs an <email> at the end");

	*email++ = 0;
	*end = 0;

	end = name + strlen(name);
	while (end > name && (end[-1] == ' ' || end[-1] == '\t'))
		*--end = 0;

	msg = gpf_author_invalid(val, name, email);
	if (msg)
		return perr(p, "%s", msg);

	if (gpf_authors_add(&font->authors, val, name, email))
		return perr(p, "out of memory");

	return 0;
}

static int read_family(struct gpf_font *font, const char *dir,
                       char *err, size_t err_len)
{
	struct gpf_family_meta *m = &font->meta;
	char path[4096];
	char line[LINE_MAX_LEN];
	struct parser p = {.path = path, .err = err, .err_len = err_len};
	FILE *f;
	int seen_key = 0;

	snprintf(path, sizeof(path), "%s/family", dir);

	f = fopen(path, "r");
	if (!f) {
		snprintf(err, err_len, "%s: %s", path, strerror(errno));
		return 1;
	}

	while (fgets(line, sizeof(line), f)) {
		char *key, *val;

		p.line++;

		if (!seen_key && line_is_comment(line)) {
			char *nl = strchr(line, '\n');

			if (nl)
				*nl = 0;

			if (append_comment(&font->comments, line))
				goto err;

			continue;
		}

		strip_line(line);

		if (!*skip_space(line))
			continue;

		seen_key = 1;

		val = split_key(line, &key);

		if (!strcmp(key, "family")) {
			snprintf(m->family, sizeof(m->family), "%s", val);
			continue;
		}

		if (!strcmp(key, "license")) {
			snprintf(m->license, sizeof(m->license), "%s", val);
			continue;
		}

		if (!strcmp(key, "size")) {
			if (parse_int(&p, val, &m->size))
				goto err;
			continue;
		}

		if (!strcmp(key, "ascent")) {
			if (parse_int(&p, val, &m->ascent))
				goto err;
			continue;
		}

		if (!strcmp(key, "descent")) {
			if (parse_int(&p, val, &m->descent))
				goto err;
			continue;
		}

		if (!strcmp(key, "line_gap")) {
			if (parse_int(&p, val, &m->line_gap))
				goto err;
			continue;
		}

		if (!strcmp(key, "em")) {
			if (parse_int(&p, val, &m->em))
				goto err;
			continue;
		}

		if (!strcmp(key, "x_height")) {
			if (parse_int(&p, val, &m->x_height))
				goto err;
			continue;
		}

		if (!strcmp(key, "cap_height")) {
			if (parse_int(&p, val, &m->cap_height))
				goto err;
			continue;
		}

		if (!strcmp(key, "ch_width")) {
			if (parse_int(&p, val, &m->ch_width))
				goto err;
			continue;
		}

		if (!strcmp(key, "underline")) {
			if (parse_two_ints(&p, val, &m->underline_pos,
			                   &m->underline_thickness))
				goto err;
			continue;
		}

		if (!strcmp(key, "strikethrough")) {
			if (parse_two_ints(&p, val, &m->strike_pos,
			                   &m->strike_thickness))
				goto err;
			continue;
		}

		if (!strcmp(key, "overline")) {
			if (parse_two_ints(&p, val, &m->overline_pos,
			                   &m->overline_thickness))
				goto err;
			continue;
		}

		if (!strcmp(key, "block")) {
			uint32_t min, max;
			char *second, *name;

			second = strchr(val, ' ');
			if (!second) {
				perr(&p, "block needs a range");
				goto err;
			}

			*second++ = 0;
			second = skip_space(second);

			name = strchr(second, ' ');

			if (name)
				*name++ = 0;

			if (parse_code(&p, val, &min))
				goto err;

			if (parse_code(&p, second, &max))
				goto err;

			if (gpf_font_block_add(font, min, max,
			                       name ? skip_space(name) : "")) {
				perr(&p, "out of memory");
				goto err;
			}

			continue;
		}

		if (!strcmp(key, "default_glyph")) {
			if (parse_code(&p, val, &m->default_glyph))
				goto err;
			continue;
		}

		if (!strcmp(key, "author")) {
			if (parse_author(&p, font, val))
				goto err;
			continue;
		}

		perr(&p, "unknown key '%s'", key);
		goto err;
	}

	fclose(f);

	return 0;
err:
	fclose(f);
	return 1;
}

static int read_variant(struct gpf_font *font, enum gpf_variant_id id,
                        const char *dir, char *err, size_t err_len)
{
	char path[4096];
	char line[LINE_MAX_LEN];
	struct parser p = {.path = path, .err = err, .err_len = err_len};
	struct gpf_variant *variant;
	struct block block = {0};
	FILE *f;
	int in_glyph = 0, seen_key = 0;

	snprintf(path, sizeof(path), "%s/%s", dir, gpf_variant_name(id));

	f = fopen(path, "r");
	if (!f) {
		if (errno == ENOENT)
			return 0;

		snprintf(err, err_len, "%s: %s", path, strerror(errno));
		return 1;
	}

	variant = gpf_font_variant(font, id, 1);
	if (!variant) {
		fclose(f);
		snprintf(err, err_len, "out of memory");
		return 1;
	}

	while (fgets(line, sizeof(line), f)) {
		char *key, *val, *start;

		p.line++;

		if (!seen_key && !in_glyph && line_is_comment(line)) {
			char *nl = strchr(line, '\n');

			if (nl)
				*nl = 0;

			if (append_comment(&variant->comments, line))
				goto err;

			continue;
		}

		strip_line(line);
		start = skip_space(line);

		if (!*start) {
			if (in_glyph) {
				if (parse_glyph_block(&p, variant, &block))
					goto err;

				block_free(&block);
				in_glyph = 0;
			}
			continue;
		}

		if (!strncmp(start, "glyph ", 6)) {
			if (in_glyph) {
				if (parse_glyph_block(&p, variant, &block))
					goto err;

				block_free(&block);
			}

			in_glyph = 1;
			block.first_line = p.line;

			if (parse_code(&p, skip_space(start + 6), &block.code))
				goto err;

			continue;
		}

		if (in_glyph) {
			if (block_add(&block, line)) {
				perr(&p, "out of memory");
				goto err;
			}
			continue;
		}

		seen_key = 1;

		val = split_key(line, &key);

		if (!strcmp(key, "variant")) {
			enum gpf_variant_id got;

			if (gpf_variant_by_name(val, &got)) {
				perr(&p, "unknown variant '%s'", val);
				goto err;
			}

			if (got != id) {
				perr(&p, "variant '%s' in a file named '%s'",
				     val, gpf_variant_name(id));
				goto err;
			}

			continue;
		}

		if (!strcmp(key, "parent")) {
			enum gpf_variant_id got;

			if (gpf_variant_by_name(val, &got)) {
				perr(&p, "unknown parent '%s'", val);
				goto err;
			}

			if (got != gpf_variant_parent(id)) {
				perr(&p, "'%s' cannot be a parent of '%s'",
				     val, gpf_variant_name(id));
				goto err;
			}

			continue;
		}

		if (!strcmp(key, "spacing")) {
			if (!strcmp(val, "mono"))
				variant->spacing_mono = 1;
			else if (!strcmp(val, "proportional"))
				variant->spacing_mono = 0;
			else {
				perr(&p, "unknown spacing '%s'", val);
				goto err;
			}
			continue;
		}

		if (!strcmp(key, "derive")) {
			if (strcmp(val, "embolden")) {
				perr(&p, "unknown derive '%s'", val);
				goto err;
			}
			variant->derive_embolden = 1;
			continue;
		}

		if (!strcmp(key, "advance")) {
			if (parse_int(&p, val, &variant->advance))
				goto err;
			continue;
		}

		perr(&p, "unknown key '%s'", key);
		goto err;
	}

	if (in_glyph) {
		if (parse_glyph_block(&p, variant, &block))
			goto err;
	}

	block_free(&block);
	fclose(f);

	return 0;
err:
	block_free(&block);
	fclose(f);
	return 1;
}

struct gpf_font *gpf_font_read(const char *dir, char *err, size_t err_len)
{
	struct gpf_font *font = gpf_font_new();
	unsigned int i;

	if (!font) {
		snprintf(err, err_len, "out of memory");
		return NULL;
	}

	if (read_family(font, dir, err, err_len))
		goto err;

	for (i = 0; i < GPF_VARIANTS; i++) {
		if (read_variant(font, i, dir, err, err_len))
			goto err;
	}

	return font;
err:
	gpf_font_free(font);
	return NULL;
}
