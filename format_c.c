/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <stdlib.h>
#include <string.h>

#include <utils/gp_utf.h>
#include <utils/gp_vec.h>
#include <text/gp_font.h>

#include "resolve.h"
#include "format_c.h"

/*
 * The ranges a gfxprim font is made of.  They are not unicode blocks: gfxprim
 * indexes a table by the codepoint, so a table is a contiguous range and the
 * ones that are worth having are these.  Everything outside them is dropped by
 * the export — it is the reason a font has more glyphs than the C font it
 * compiles into.
 *
 * The ASCII table is first because gfxprim looks it up by position.
 */
struct range {
	uint32_t min, max;
	/* the C symbol suffix, NULL for the ASCII table */
	const char *suffix;
	const char *ucode;
};

static const struct range ranges[] = {
	{0x0020, 0x007f, NULL, "GP_UCODE_LATIN_BASIC"},
	{0x00a1, 0x017e, "latin_ext", "GP_UCODE_LATIN_SUP | GP_UCODE_LATIN_EXT_A"},
	{0x0384, 0x03ce, "greek", "GP_UCODE_GREEK"},
	{0x0400, 0x045f, "cyrilic", "GP_UCODE_CYRILIC"},
	{0x2018, 0x2037, "punctuation", "GP_UCODE_PUNCTUATION"},
	{0x2070, 0x208e, "subsuper", "GP_UCODE_SUB_SUPER"},
	{0x2190, 0x21ff, "arrows", "GP_UCODE_ARROWS"},
	{0x2500, 0x257f, "box", "GP_UCODE_BOX"},
	{0x3041, 0x3096, "hiragana", "GP_UCODE_HIRAGANA"},
	{0x30a0, 0x30ff, "katakana", "GP_UCODE_KATAKANA"},
	{}
};

/* the C symbol a variant's tables and face are named with */
static const char *var_id(enum gpf_variant_id id)
{
	switch (id) {
	case GPF_MONO:
		return "font";
	case GPF_REGULAR:
		return "font_regular";
	case GPF_BOLD_MONO:
		return "font_bold_mono";
	default:
		return "font_bold";
	}
}

static void sym(char *buf, size_t len, enum gpf_variant_id id,
                const struct range *range, const char *what)
{
	if (range->suffix)
		snprintf(buf, len, "%s_%s_%s", var_id(id), range->suffix, what);
	else
		snprintf(buf, len, "%s_%s", var_id(id), what);
}

/*
 * A glyph is five bytes of metrics and then the bitmap, rows byte aligned and
 * the whole thing padded to four bytes — struct gp_glyph, as the compiled in
 * fonts store it.
 */
static size_t glyph_size(const struct gpf_glyph *glyph)
{
	size_t rows = ((size_t)glyph->width + 7) / 8;

	return (5 + rows * glyph->height + 3) & ~(size_t)3;
}

/*
 * The glyph record as the bytes that end up in the array, so that two glyphs
 * are the same record exactly when these compare equal.
 */
static uint8_t *glyph_record(const struct gpf_glyph *glyph)
{
	unsigned int row_bytes = (glyph->width + 7) / 8;
	uint8_t *rec = calloc(1, glyph_size(glyph));
	unsigned int x, y, i;
	size_t pos = 5;

	if (!rec)
		return NULL;

	rec[0] = glyph->width;
	rec[1] = glyph->height;
	rec[2] = glyph->bearing_x;
	rec[3] = glyph->bearing_y;
	rec[4] = glyph->advance;

	for (y = 0; y < glyph->height; y++) {
		for (x = 0; x < row_bytes; x++) {
			for (i = 0; i < 8; i++) {
				if (8 * x + i >= glyph->width)
					break;

				if (gpf_glyph_pixel(glyph, 8 * x + i, y))
					rec[pos] |= 0x80 >> i;
			}

			pos++;
		}
	}

	/* the padding is left zeroed, it keeps the next record aligned */
	return rec;
}

/* a codepoint that points to a record and the variants it does so in */
struct glyph_ref {
	uint32_t code;
	/* a bit per variant */
	unsigned int variants;
};

static void emit_ref(FILE *f, const struct glyph_ref *ref)
{
	char utf[5] = "";
	const char *sep = "";
	unsigned int id;

	utf[gp_to_utf8(ref->code, utf)] = 0;

	if (ref->code < 0x7f)
		fprintf(f, "\t/* '%s'", utf);
	else
		fprintf(f, "\t/* 0x%0x '%s'", ref->code, utf);

	for (id = 0; id < GPF_VARIANTS; id++) {
		if (!(ref->variants & (1u << id)))
			continue;

		fprintf(f, "%s %s", sep, gpf_variant_name(id));
		sep = " |";
	}

	fprintf(f, " */\n");
}

/*
 * A record with a comment above it for each codepoint that points to it,
 * naming the variants it does so in.
 */
static void emit_record(FILE *f, const struct glyph_ref *refs,
                        const uint8_t *rec, size_t size)
{
	size_t pos, i;

	for (i = 0; i < gp_vec_len(refs); i++)
		emit_ref(f, &refs[i]);

	/* the bearings are signed */
	fprintf(f, "\t%2i, %2i, %2i, %2i, %2i,\n", rec[0], rec[1],
	        (int8_t)rec[2], (int8_t)rec[3], rec[4]);

	if (size > 5)
		fprintf(f, "\t");

	for (pos = 5; pos < size; pos++)
		fprintf(f, "0x%02x,%s", rec[pos], pos + 1 < size ? " " : "\n");
}

static int range_has_glyphs(struct gpf_font *font, enum gpf_variant_id id,
                            const struct range *range)
{
	uint32_t code;

	for (code = range->min; code <= range->max; code++) {
		struct gpf_resolved res;

		if (gpf_resolve(font, id, code, &res))
			continue;

		gpf_resolved_clear(&res);
		return 1;
	}

	return 0;
}

/*
 * The glyph array of the family.  Every face indexes the same one, so that a
 * glyph a variant inherits unchanged — all of `regular` that is not redrawn,
 * the bold that embolds to what `bold-mono` does — is stored once.
 */
struct glyph_store {
	uint8_t *data;
	/* where each record starts in data */
	size_t *starts;
	/* the codepoints that point to each record, a vector per record */
	struct glyph_ref **refs;
};

/*
 * The index of a record the array already holds, or -1.  A linear
 * search, which is a few milliseconds for the largest corpus font with all
 * four variants and runs once per export.
 */
static ssize_t find_record(const struct glyph_store *store,
                           const uint8_t *rec, size_t size)
{
	size_t i, cnt = gp_vec_len(store->starts);

	for (i = 0; i < cnt; i++) {
		size_t start = store->starts[i];
		size_t end = i + 1 < cnt ? store->starts[i + 1] : gp_vec_len(store->data);

		if (end - start == size && !memcmp(store->data + start, rec, size))
			return i;
	}

	return -1;
}

/*
 * The offset of a glyph record in the array, appending it when it is not there
 * yet — H and the Cyrillic Н, a letter and the Greek one, a mono glyph and the
 * regular that inherits it, are the same bytes.  GP_NOGLYPH on allocation
 * failure.
 */
static gp_glyph_offset store_record(struct glyph_store *store,
                                    enum gpf_variant_id id, uint32_t code,
                                    const uint8_t *rec, size_t size)
{
	struct glyph_ref ref = {.code = code, .variants = 1u << id};
	ssize_t i = find_record(store, rec, size);
	size_t len = gp_vec_len(store->data), j;
	struct glyph_ref *refs;
	uint8_t *tmp;

	if (i >= 0) {
		refs = store->refs[i];

		for (j = 0; j < gp_vec_len(refs); j++) {
			if (refs[j].code == code) {
				refs[j].variants |= ref.variants;
				return store->starts[i];
			}
		}

		if (!GP_VEC_APPEND(store->refs[i], ref))
			return GP_NOGLYPH;

		return store->starts[i];
	}

	refs = gp_vec_new(0, sizeof(struct glyph_ref));
	if (!refs)
		return GP_NOGLYPH;

	if (!GP_VEC_APPEND(refs, ref) || !GP_VEC_APPEND(store->refs, refs)) {
		gp_vec_free(refs);
		return GP_NOGLYPH;
	}

	if (!GP_VEC_APPEND(store->starts, len))
		return GP_NOGLYPH;

	tmp = gp_vec_expand(store->data, size);
	if (!tmp)
		return GP_NOGLYPH;

	store->data = tmp;

	memcpy(store->data + len, rec, size);

	return len;
}

static void emit_glyphs(FILE *f, const struct glyph_store *store)
{
	size_t i, cnt = gp_vec_len(store->starts);

	fprintf(f, "static uint8_t %s_glyphs[] = {\n", var_id(GPF_MONO));

	for (i = 0; i < cnt; i++) {
		size_t start = store->starts[i];
		size_t end = i + 1 < cnt ? store->starts[i + 1] : gp_vec_len(store->data);

		emit_record(f, store->refs[i], store->data + start, end - start);
	}

	fprintf(f, "};\n\n");
}

/*
 * The offsets of a variant, one table per range it has glyphs in, with the
 * records they point to added to the store.  Sets tables[] to which ranges the
 * variant has glyphs in; returns non-zero on allocation failure.
 */
static int variant_offsets(struct gpf_font *font,
                           enum gpf_variant_id id, struct glyph_store *store,
                           int *tables, gp_glyph_offset **offsets)
{
	uint32_t code;
	unsigned int i;

	for (i = 0; ranges[i].max; i++) {
		size_t cnt = ranges[i].max - ranges[i].min + 1;

		tables[i] = range_has_glyphs(font, id, &ranges[i]);
		if (!tables[i])
			continue;

		offsets[i] = malloc(cnt * sizeof(gp_glyph_offset));
		if (!offsets[i])
			return 1;

		for (code = ranges[i].min; code <= ranges[i].max; code++) {
			gp_glyph_offset *off = &offsets[i][code - ranges[i].min];
			struct gpf_resolved res;
			uint8_t *rec;
			size_t size;

			*off = GP_NOGLYPH;

			if (gpf_resolve(font, id, code, &res))
				continue;

			size = glyph_size(&res.glyph);
			rec = glyph_record(&res.glyph);
			gpf_resolved_clear(&res);

			if (!rec)
				return 1;

			*off = store_record(store, id, code, rec, size);
			free(rec);

			if (*off == GP_NOGLYPH)
				return 1;
		}
	}

	return 0;
}

/*
 * The variant whose offsets table a face points to for a range: an earlier one
 * with the very same table, which is the case for a range nobody redrew, or the
 * variant itself.
 */
static enum gpf_variant_id offsets_owner(int tables[][16],
                                         gp_glyph_offset *offsets[][16],
                                         enum gpf_variant_id id, unsigned int i)
{
	size_t size = (ranges[i].max - ranges[i].min + 1) * sizeof(gp_glyph_offset);
	unsigned int prev;

	for (prev = 0; prev < id; prev++) {
		if (tables[prev][i] && !memcmp(offsets[prev][i], offsets[id][i], size))
			return prev;
	}

	return id;
}

static void emit_offsets(FILE *f, enum gpf_variant_id id,
                         const struct range *range,
                         const gp_glyph_offset *offsets)
{
	size_t cnt = range->max - range->min + 1, i;
	char name[128];

	sym(name, sizeof(name), id, range, "offsets");

	fprintf(f, "static gp_glyph_offset %s[] = {", name);

	for (i = 0; i < cnt; i++) {
		if (!(i % 8))
			fprintf(f, "\n\t");

		if (offsets[i] == GP_NOGLYPH)
			fprintf(f, "GP_NOGLYPH,");
		else
			fprintf(f, "    0x%04x,", offsets[i]);

		if (i % 8 != 7)
			fprintf(f, " ");
	}

	fprintf(f, "\n};\n\n");
}

static void emit_face(FILE *f, struct gpf_font *font, enum gpf_variant_id id,
                      const char *font_id, const int *tables,
                      const enum gpf_variant_id *owners)
{
	struct gpf_family_meta *m = &font->meta;
	struct gpf_variant *variant = font->variants[id];
	int mono = (id == GPF_MONO || id == GPF_BOLD_MONO);
	int bold = (id == GPF_BOLD || id == GPF_BOLD_MONO);
	unsigned int max_width = 0, max_advance = 0, cnt = 0, i;
	uint32_t code;
	char name[128];

	(void)variant;

	for (i = 0; ranges[i].max; i++) {
		if (!tables[i])
			continue;

		cnt++;

		for (code = ranges[i].min; code <= ranges[i].max; code++) {
			struct gpf_resolved res;
			unsigned int width;

			if (gpf_resolve(font, id, code, &res))
				continue;

			width = res.glyph.bearing_x + res.glyph.width;

			if (width > max_width)
				max_width = width;

			if ((unsigned int)res.glyph.advance > max_advance)
				max_advance = res.glyph.advance;

			gpf_resolved_clear(&res);
		}
	}

	fprintf(f, "static struct gp_font_face %s = {\n", var_id(id));

	if (id == GPF_MONO)
		fprintf(f, "\t.family_name = \"%s\",\n", font_id);
	else
		fprintf(f, "\t.family_name = \"%s_%s\",\n", font_id, var_id(id) + 5);

	fprintf(f, "\t.style = %s%s,\n", mono ? "GP_FONT_MONO" : "GP_FONT_REGULAR",
	        bold ? " | GP_FONT_BOLD" : "");

	fprintf(f, "\t.ascent = %i,\n", m->ascent);
	fprintf(f, "\t.descent = %i,\n", m->descent);
	fprintf(f, "\t.max_glyph_width = %u,\n", max_width);
	fprintf(f, "\t.max_glyph_advance = %u,\n", max_advance);

	if (m->em)
		fprintf(f, "\t.em = %i,\n", m->em);
	if (m->x_height)
		fprintf(f, "\t.x_height = %i,\n", m->x_height);
	if (m->cap_height)
		fprintf(f, "\t.cap_height = %i,\n", m->cap_height);
	if (m->ch_width)
		fprintf(f, "\t.ch_width = %i,\n", m->ch_width);
	if (m->line_gap)
		fprintf(f, "\t.line_gap = %i,\n", m->line_gap);
	if (m->underline_pos)
		fprintf(f, "\t.underline_pos = %i,\n", m->underline_pos);
	if (m->underline_thickness)
		fprintf(f, "\t.underline_thickness = %i,\n", m->underline_thickness);
	if (m->strike_pos)
		fprintf(f, "\t.strike_pos = %i,\n", m->strike_pos);
	if (m->strike_thickness)
		fprintf(f, "\t.strike_thickness = %i,\n", m->strike_thickness);
	if (m->overline_pos)
		fprintf(f, "\t.overline_pos = %i,\n", m->overline_pos);
	if (m->overline_thickness)
		fprintf(f, "\t.overline_thickness = %i,\n", m->overline_thickness);

	fprintf(f, "\t.glyph_bitmap_format = GP_FONT_BITMAP_1BPP,\n");
	fprintf(f, "\t.glyph_tables = %u,\n", cnt);
	fprintf(f, "\t.glyphs = {\n");

	for (i = 0; ranges[i].max; i++) {
		if (!tables[i])
			continue;

		fprintf(f, "\t\t{\n");

		fprintf(f, "\t\t\t.glyphs = %s_glyphs,\n", var_id(GPF_MONO));

		sym(name, sizeof(name), owners[i], &ranges[i], "offsets");
		fprintf(f, "\t\t\t.offsets = %s,\n", name);

		fprintf(f, "\t\t\t.min_glyph = 0x%04x,\n", ranges[i].min);
		fprintf(f, "\t\t\t.max_glyph = 0x%04x,\n", ranges[i].max);
		fprintf(f, "\t\t},\n");
	}

	fprintf(f, "\t}\n};\n\n");
}

/*
 * Which faces are worth emitting.  The monospace pair always is — a bold that
 * nobody drew is the emboldened one, which is what the compiled in fonts have
 * had all along.  The proportional pair only when the font really has a
 * proportional variant, or `regular` would be a second copy of `mono`.
 */
static int face_wanted(struct gpf_font *font, enum gpf_variant_id id)
{
	if (id == GPF_MONO || id == GPF_BOLD_MONO)
		return 1;

	return !!font->variants[GPF_REGULAR] || !!font->variants[id];
}

/* what the export leaves behind, which is worth knowing */
static void count_dropped(struct gpf_font *font, unsigned int *kept,
                          unsigned int *dropped)
{
	uint32_t *codes = gpf_font_codes(font);
	size_t i;
	unsigned int j;

	if (!codes)
		return;

	for (i = 0; i < gp_vec_len(codes); i++) {
		int in = 0;

		for (j = 0; ranges[j].max; j++) {
			if (codes[i] >= ranges[j].min && codes[i] <= ranges[j].max)
				in = 1;
		}

		if (in)
			(*kept)++;
		else
			(*dropped)++;
	}

	gp_vec_free(codes);
}

int gpf_export_c(struct gpf_font *font, const char *font_id, const char *name,
                 FILE *f, char *err, size_t err_len)
{
	int tables[GPF_VARIANTS][16] = {};
	gp_glyph_offset *offsets[GPF_VARIANTS][16] = {};
	enum gpf_variant_id owners[GPF_VARIANTS][16] = {};
	struct glyph_store store = {
		.data = gp_vec_new(0, 1),
		.starts = gp_vec_new(0, sizeof(size_t)),
		.refs = gp_vec_new(0, sizeof(struct glyph_ref *)),
	};
	char blocks[512] = "";
	unsigned int kept = 0, dropped = 0;
	unsigned int id, i;
	int faces = 0, ret = 1;

	if (!font->variants[GPF_MONO]) {
		snprintf(err, err_len, "the font has no mono variant to export");
		goto out;
	}

	if (!store.data || !store.starts || !store.refs)
		goto oom;

	if (font->meta.license[0])
		fprintf(f, "// SPDX-License-Identifier: %s\n", font->meta.license);

	if (font->authors) {
		fprintf(f, "/*\n");
		for (i = 0; i < gp_vec_len(font->authors); i++) {
			fprintf(f, " * Copyright (C) %s %s <%s>\n",
			        font->authors[i].years,
			        font->authors[i].name,
			        font->authors[i].email);
		}
		fprintf(f, " */\n");
	}

	fprintf(f, "/* Generated file, do not touch */\n\n");
	fprintf(f, "#include <text/gp_font.h>\n\n");

	for (id = 0; id < GPF_VARIANTS; id++) {
		if (!face_wanted(font, id))
			continue;

		if (variant_offsets(font, id, &store, tables[id], offsets[id]))
			goto oom;
	}

	emit_glyphs(f, &store);

	for (id = 0; id < GPF_VARIANTS; id++) {
		for (i = 0; ranges[i].max; i++) {
			if (!tables[id][i])
				continue;

			owners[id][i] = offsets_owner(tables, offsets, id, i);

			if (owners[id][i] == id)
				emit_offsets(f, id, &ranges[i], offsets[id][i]);
		}
	}

	for (id = 0; id < GPF_VARIANTS; id++) {
		if (!face_wanted(font, id))
			continue;

		emit_face(f, font, id, font_id, tables[id], owners[id]);

		faces++;
	}

	/* the blocks the family has, named the way gfxprim names them */
	for (i = 0; ranges[i].max; i++) {
		if (!tables[GPF_MONO][i])
			continue;

		if (blocks[0])
			strncat(blocks, " | ", sizeof(blocks) - strlen(blocks) - 1);

		strncat(blocks, ranges[i].ucode, sizeof(blocks) - strlen(blocks) - 1);
	}

	fprintf(f, "const gp_font_family __attribute__((visibility (\"hidden\")))"
	           " font_family_%s = {\n", font_id);
	fprintf(f, "\t.family_name = \"%s\",\n", name);
	fprintf(f, "\t.ucode_blocks = %s,\n", blocks);
	fprintf(f, "\t.fonts = {\n");

	for (id = 0; id < GPF_VARIANTS; id++) {
		if (!face_wanted(font, id))
			continue;

		fprintf(f, "\t\t&%s,\n", var_id(id));
	}

	fprintf(f, "\t\tNULL\n\t}\n};\n");

	count_dropped(font, &kept, &dropped);

	fprintf(stderr, "%s: %u glyphs, %u faces", name, kept, faces);

	if (dropped) {
		fprintf(stderr, ", %u outside the exported ranges dropped",
		        dropped);
	}

	fprintf(stderr, "\n");

	ret = faces ? 0 : 1;
	goto out;
oom:
	snprintf(err, err_len, "out of memory");
out:
	for (id = 0; id < GPF_VARIANTS; id++) {
		for (i = 0; ranges[i].max; i++)
			free(offsets[id][i]);
	}

	for (i = 0; store.refs && i < gp_vec_len(store.refs); i++)
		gp_vec_free(store.refs[i]);

	gp_vec_free(store.refs);
	gp_vec_free(store.starts);
	gp_vec_free(store.data);
	return ret;
}
