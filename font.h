/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file font.h
 * @brief The in-memory font model.
 *
 * A font is a family of four variants: mono, regular, bold-mono and bold.  Each
 * variant holds a sparse array of glyphs sorted by unicode codepoint, which
 * matches the on-disk order so that saving reshuffles nothing.
 */
#ifndef GPFORGE_FONT_H
#define GPFORGE_FONT_H

#include <stdint.h>
#include <stddef.h>

#include <utils/gp_vec.h>

/** @brief Size of the name buffers, including the terminating zero. */
#define GPF_NAME_MAX 64
/** @brief Size of the author years buffer, including the terminating zero. */
#define GPF_YEARS_MAX 32

/** @brief Returns the smaller of two values, evaluates them twice. */
#define GPF_MIN(a, b) ((a) < (b) ? (a) : (b))
/** @brief Returns the larger of two values, evaluates them twice. */
#define GPF_MAX(a, b) ((a) > (b) ? (a) : (b))

/**
 * @brief Font face variant.
 */
enum gpf_variant_id {
	/** @brief Monospace glyphs, the base variant. */
	GPF_MONO,
	/** @brief Proportional glyphs, "regular" on disk. */
	GPF_REGULAR,
	/** @brief Bold monospace glyphs. */
	GPF_BOLD_MONO,
	/** @brief Bold glyphs. */
	GPF_BOLD,
	/** @brief Number of font variants. */
	GPF_VARIANTS,
	/** @brief What gpf_variant_parent() returns for the base variant. */
	GPF_NO_PARENT = GPF_VARIANTS,
};

/**
 * @brief A glyph entry type.
 *
 * What a glyph entry stored in a variant carries: the ink itself, numbers
 * only with the ink derived from the parent, or a composition.
 */
enum gpf_glyph_kind {
	/**
	 * @brief Glyph with ink.
	 *
	 * The complete glyph is stored as ink. The advance and both
	 * bearings are valid.
	 */
	GPF_GLYPH_INK,
	/**
	 * @brief Glyph with metrics.
	 *
	 * Metrics are stored on disk, the ink comes from the parent.
	 */
	GPF_GLYPH_METRICS,
	/**
	 * @brief Glyph is composed from base + accent.
	 *
	 * Ink is base + accent, metrics come from the base.
	 */
	GPF_GLYPH_COMPOSE,
};

/**
 * @brief A glyph.
 */
struct gpf_glyph {
	/**
	 * @brief Unicode code point.
	 */
	uint32_t code;
	/**
	 * @brief enum gpf_glyph_kind
	 */
	uint8_t kind;

	/**
	 * @brief The entry carries an advance.
	 *
	 * For GPF_GLYPH_METRICS this and has_shift say which numbers the
	 * block carried, for GPF_GLYPH_INK this and has_bearing are always
	 * set.
	 */
	uint8_t has_advance;
	/** @brief The entry carries both bearings, see has_advance. */
	uint8_t has_bearing;
	/** @brief The entry moves the inherited ink, GPF_GLYPH_METRICS only. */
	uint8_t has_shift;

	/**
	 * @brief Glyph was modified flag.
	 *
	 * Edited since the font was last written. Runtime only.
	 */
	uint8_t modified;

	/**
	 * @brief Glyph advance.
	 *
	 * Stored per glyph in every variant; in a monospace variant it has
	 * to match gpf_variant::advance, which the lint checks.
	 */
	int advance;
	/**
	 * @brief Glyph bearing x.
	 *
	 * Columns from the origin to the leftmost ink column, may be negative.
	 */
	int bearing_x;
	/**
	 * @brief Glyph bearing y.
	 *
	 * Height of the top ink row above the baseline, positive up.
	 */
	int bearing_y;

	/**
	 * @brief Glyph ink width.
	 */
	unsigned int width;
	/**
	 * @brief Glyph ink height.
	 */
	unsigned int height;
	/**
	 * @brief Glyph ink.
	 *
	 * Ink, one byte per pixel, row major, trimmed so that the first and
	 * last row and column are non empty. An empty glyph has width and
	 * height zero and bits NULL.
	 */
	uint8_t *bits;

	/**
	 * @brief Base glyph, GPF_GLYPH_COMPOSE only.
	 */
	uint32_t base;
	/**
	 * @brief Accent to be combined with the base, GPF_GLYPH_COMPOSE only.
	 */
	uint32_t accent;
	/** @brief Accent offset, positive right and up, compose only. */
	int dx, dy;

	/**
	 * @brief How far the inherited ink is moved, positive right and up.
	 *
	 * A shift and not a place: the ink follows the parent's when the
	 * parent is redrawn taller or wider.  GPF_GLYPH_METRICS only.
	 */
	int shift_x, shift_y;
};

/**
 * @brief A font variant.
 */
struct gpf_variant {
	/** @brief Font variant e.g. GPF_REGULAR, GPF_MONO, etc. */
	enum gpf_variant_id id;
	/** @brief Every glyph has the same advance. */
	int spacing_mono;
	/** @brief Glyphs not drawn here are emboldened from the parent. */
	int derive_embolden;
	/**
	 * @brief Advance for all glyphs.
	 *
	 * Monospace variants only, 0 when unset.
	 */
	int advance;
	/**
	 * @brief Comments above the first glyph block.
	 *
	 * A gp_vec string, may be NULL.
	 */
	char *comments;

	/**
	 * @brief Glyphs.
	 *
	 * A gp_vec sorted by the codepoint.
	 */
	struct gpf_glyph *glyphs;
};

/**
 * @brief A declared unicode block.
 *
 * A unicode block the font means to cover. A block with glyphs in it needs no
 * declaration, this is how a block that is still empty stays on the font's
 * list — it is an intention, which is exactly what a font being started for a
 * new script has.
 */
struct gpf_block_decl {
	/** @brief The first codepoint of the block. */
	uint32_t min;
	/** @brief The last codepoint of the block, inclusive. */
	uint32_t max;
	/** @brief The block name. */
	char name[GPF_NAME_MAX];
};

/**
 * @brief A font author, one `author` line of the family file.
 */
struct gpf_author {
	/** @brief Copyright years, e.g. "2024", "2019-2024" or "2019,2023". */
	char years[GPF_YEARS_MAX];
	/** @brief The author's name. */
	char name[GPF_NAME_MAX];
	/** @brief The author's email, without the angle brackets. */
	char email[GPF_NAME_MAX];
};

/**
 * @brief Font family meta data.
 */
struct gpf_family_meta {
	/** @brief Font family name. */
	char family[GPF_NAME_MAX];
	/** @brief SPDX license name. */
	char license[GPF_NAME_MAX];

	/** @brief Font size. */
	int size;
	/** @brief Font ascent. */
	int ascent;
	/** @brief Font descent. */
	int descent;
	/** @brief Gap between two lines of text. */
	int line_gap;

	/** @brief Em size. */
	int em;
	/** @brief The x-height. */
	int x_height;
	/** @brief Capital letter height. */
	int cap_height;
	/** @brief The advance of the digit zero. */
	int ch_width;

	/** @brief Underline position. */
	int underline_pos;
	/** @brief Underline thickness. */
	int underline_thickness;
	/** @brief Strikethrough position. */
	int strike_pos;
	/** @brief Strikethrough thickness. */
	int strike_thickness;
	/** @brief Overline position. */
	int overline_pos;
	/** @brief Overline thickness. */
	int overline_thickness;

	/** @brief The unicode code point of the default glyph. */
	uint32_t default_glyph;
};

/**
 * @brief A font family.
 */
struct gpf_font {
	/** @brief Font meta data. */
	struct gpf_family_meta meta;

	/**
	 * @brief Font comments.
	 *
	 * Comments at the top of the family file, a gp_vec string, may be NULL.
	 */
	char *comments;

	/**
	 * @brief Declared unicode blocks.
	 *
	 * A gp_vec of declared blocks, sorted by the first codepoint, may
	 * be NULL.
	 */
	struct gpf_block_decl *blocks;

	/**
	 * @brief Font authors.
	 *
	 * A gp_vec of authors in the order they are written in, may be NULL.
	 */
	struct gpf_author *authors;

	/** @brief Variants indexed by gpf_variant_id, NULL when missing. */
	struct gpf_variant *variants[GPF_VARIANTS];
};

/**
 * @brief Returns the name of a variant.
 *
 * @param id A variant id.
 * @return The on-disk name, e.g. "bold-mono", NULL for an invalid id.
 */
const char *gpf_variant_name(enum gpf_variant_id id);

/**
 * @brief Looks up a variant by its on-disk name.
 *
 * @param name A variant name, e.g. "regular".
 * @param id Set to the variant id on success.
 * @return Zero on success, non-zero for an unknown name.
 */
int gpf_variant_by_name(const char *name, enum gpf_variant_id *id);

/**
 * @brief Returns the variant a variant derives its missing glyphs from.
 *
 * Regular and bold-mono derive from mono, bold derives from regular.
 *
 * @param id A variant id.
 * @return The parent variant id, GPF_NO_PARENT for mono.
 */
enum gpf_variant_id gpf_variant_parent(enum gpf_variant_id id);

/**
 * @brief Allocates an empty font.
 *
 * The font has no variants and zeroed metadata apart from the line
 * thicknesses, which are set to one.
 *
 * @return A new font or NULL on an allocation failure.
 */
struct gpf_font *gpf_font_new(void);

/**
 * @brief Frees a font with all its variants and glyphs.
 *
 * @param self A font, may be NULL.
 */
void gpf_font_free(struct gpf_font *self);

/**
 * @brief Fills in metadata for a font that is only being started.
 *
 * Sets what is asked for and guesses the rest, so that nothing is zero; the
 * guesses are to be overwritten as the font is drawn.
 *
 * @param meta The metadata to fill in.
 * @param family A family name, "New" is used when NULL or empty.
 * @param size Font size.
 * @param ascent Font ascent.
 * @param descent Font descent.
 * @param advance The mono advance, used as the ch_width.
 */
void gpf_family_meta_guess(struct gpf_family_meta *meta, const char *family,
                           int size, int ascent, int descent, int advance);

/**
 * @brief Creates a new font.
 *
 * The font has the metadata and an empty mono variant of the advance, the
 * other variants appear when something is drawn in them.
 *
 * @param meta Font metadata, copied.
 * @param advance The advance of the mono variant.
 * @return A new font or NULL on an allocation failure.
 */
struct gpf_font *gpf_font_create(const struct gpf_family_meta *meta,
                                 int advance);

/**
 * @brief Compares two sets of font metadata.
 *
 * Strings are compared up to their terminating zero, what is past the end of
 * a string does not count.
 *
 * @param a Font metadata.
 * @param b Font metadata.
 * @return Non-zero when both carry the same values.
 */
int gpf_family_meta_same(const struct gpf_family_meta *a,
                         const struct gpf_family_meta *b);

/**
 * @brief Returns a font variant.
 *
 * A newly allocated variant is empty, has spacing_mono set for the mono
 * variants and derive_embolden set for the bold ones.
 *
 * @param self A font.
 * @param id A variant id.
 * @param create Allocate the variant when it is missing.
 * @return The variant, NULL when it is missing and create is not set, on an
 *         invalid id or on an allocation failure.
 */
struct gpf_variant *gpf_font_variant(struct gpf_font *self,
                                     enum gpf_variant_id id, int create);

/**
 * @brief Returns the number of glyph entries in a variant.
 *
 * @param self A font variant.
 * @return The length of the glyphs array.
 */
static inline size_t gpf_variant_glyphs(const struct gpf_variant *self)
{
	return gp_vec_len(self->glyphs);
}

/**
 * @brief Returns every codepoint the font mentions, in any variant.
 *
 * @param self A font.
 * @return A sorted gp_vec of unique codepoints, free it with gp_vec_free(),
 *         NULL on an allocation failure.
 */
uint32_t *gpf_font_codes(struct gpf_font *self);

/**
 * @brief Clears the modified flag of all glyphs in all variants.
 *
 * Forgets which glyphs were edited, which is what writing the font makes true.
 *
 * @param self A font.
 */
void gpf_font_clear_modified(struct gpf_font *self);

/**
 * @brief Returns how far the ink of the whole font reaches.
 *
 * This is not always the ascent and the descent: an accent that does not fit
 * over a capital sticks out of the box, and a cell that is drawn as tall as
 * the box would cut it off. The result is never smaller than the box.
 *
 * @param self A font.
 * @param top Set to the highest ink row, positive up from the baseline.
 * @param bottom Set to the lowest ink row, positive up from the baseline.
 */
void gpf_font_ink_extent(struct gpf_font *self, int *top, int *bottom);

/**
 * @brief Declares a unicode block.
 *
 * Declaring an already declared block is a no-op.
 *
 * @param self A font.
 * @param min The first codepoint of the block.
 * @param max The last codepoint of the block.
 * @param name The block name, may be NULL.
 * @return Zero on success, non-zero on an allocation failure.
 */
int gpf_font_block_add(struct gpf_font *self, uint32_t min, uint32_t max,
                       const char *name);

/**
 * @brief Removes a block declaration.
 *
 * Does not touch any glyphs: a block that has glyphs stays on the font's list
 * regardless.
 *
 * @param self A font.
 * @param min The first codepoint of the block, an undeclared one is a no-op.
 */
void gpf_font_block_del(struct gpf_font *self, uint32_t min);

/**
 * @brief Deletes every glyph in a range, in all variants.
 *
 * @param self A font.
 * @param min The first codepoint of the range.
 * @param max The last codepoint of the range, inclusive.
 * @return The number of glyph entries deleted.
 */
unsigned int gpf_font_block_clear(struct gpf_font *self, uint32_t min,
                                  uint32_t max);

/**
 * @brief Counts glyph entries in a range, in all variants.
 *
 * @param self A font.
 * @param min The first codepoint of the range.
 * @param max The last codepoint of the range, inclusive.
 * @return The number of glyph entries in the range.
 */
unsigned int gpf_font_range_glyphs(struct gpf_font *self, uint32_t min,
                                   uint32_t max);

/**
 * @brief Checks whether a block is declared.
 *
 * @param self A font.
 * @param min The first codepoint of the block.
 * @return Non-zero when a block starting at min is declared.
 */
int gpf_font_block_declared(struct gpf_font *self, uint32_t min);

/**
 * @brief Checks the fields of an author.
 *
 * The years are digits, '-' for a range and ',' for a list, and start with a
 * digit; the name is not empty; the email has an '@' and no whitespace or
 * angle brackets; and all of them fit struct gpf_author.
 *
 * @param years Copyright years, e.g. "2019-2024".
 * @param name The author's name.
 * @param email The author's email, without the angle brackets.
 * @return NULL when the author is valid, what is wrong with it otherwise.
 */
const char *gpf_author_invalid(const char *years, const char *name,
                               const char *email);

/**
 * @brief Appends an author to a gp_vec of authors.
 *
 * @param authors A gp_vec of authors, allocated when it is NULL.
 * @param years Copyright years, e.g. "2019-2024".
 * @param name The author's name.
 * @param email The author's email, without the angle brackets.
 * @return Non-zero on an allocation failure or when gpf_author_invalid()
 *         refuses the author.
 */
int gpf_authors_add(struct gpf_author **authors, const char *years,
                    const char *name, const char *email);

/**
 * @brief Removes an author from a gp_vec of authors.
 *
 * @param authors A gp_vec of authors.
 * @param idx The index of the author to remove, out of range is a no-op.
 */
void gpf_authors_del(struct gpf_author **authors, size_t idx);

/**
 * @brief Copies a gp_vec of authors.
 *
 * @param authors A gp_vec of authors, may be NULL.
 * @return A new gp_vec, empty for NULL, or NULL on an allocation failure.
 */
struct gpf_author *gpf_authors_dup(const struct gpf_author *authors);

/**
 * @brief Compares two gp_vecs of authors, NULL being the same as empty.
 *
 * @param a A gp_vec of authors, may be NULL.
 * @param b A gp_vec of authors, may be NULL.
 * @return Non-zero when they hold the same authors in the same order.
 */
int gpf_authors_same(const struct gpf_author *a, const struct gpf_author *b);


/**
 * @brief Looks up a glyph in a variant.
 *
 * @param self A font variant.
 * @param code A unicode codepoint.
 * @return The glyph or NULL when the variant does not have it. The pointer is
 *         invalidated by adding or removing glyphs.
 */
struct gpf_glyph *gpf_variant_glyph(struct gpf_variant *self, uint32_t code);

/**
 * @brief Adds a glyph to a variant.
 *
 * The new glyph is zeroed apart from the code, i.e. an empty GPF_GLYPH_INK.
 *
 * @param self A font variant.
 * @param code A unicode codepoint.
 * @return The new glyph, the existing one when the variant already has it, or
 *         NULL on an allocation failure. The pointer is invalidated by adding
 *         or removing glyphs.
 */
struct gpf_glyph *gpf_variant_glyph_add(struct gpf_variant *self, uint32_t code);

/**
 * @brief Removes a glyph from a variant.
 *
 * Reverting an overlay is exactly this.
 *
 * @param self A font variant.
 * @param code A unicode codepoint, a missing one is a no-op.
 */
void gpf_variant_glyph_del(struct gpf_variant *self, uint32_t code);

/**
 * @brief Frees the ink and zeroes the glyph, keeping only the code.
 *
 * @param self A glyph.
 */
void gpf_glyph_clear(struct gpf_glyph *self);

/**
 * @brief Stores the glyph ink.
 *
 * Trims empty rows and columns and adjusts the bearings. An empty bitmap
 * produces an empty glyph with zero bearings.
 *
 * @param self A glyph.
 * @param bits The ink, one byte per pixel, w * h, row major, copied.
 * @param w The bitmap width.
 * @param h The bitmap height.
 * @param bearing_x The bearing x of the bitmap left column.
 * @param bearing_y The bearing y of the bitmap top row.
 * @return Zero on success, non-zero on an allocation failure.
 */
int gpf_glyph_set_bits(struct gpf_glyph *self, const uint8_t *bits,
                       unsigned int w, unsigned int h,
                       int bearing_x, int bearing_y);

/**
 * @brief Returns a glyph ink pixel.
 *
 * @param self A glyph.
 * @param x The column in the trimmed ink.
 * @param y The row in the trimmed ink.
 * @return Non-zero when the pixel is inked, zero outside of the ink.
 */
static inline int gpf_glyph_pixel(const struct gpf_glyph *self,
                                  unsigned int x, unsigned int y)
{
	if (x >= self->width || y >= self->height)
		return 0;

	return self->bits[y * self->width + x];
}

#endif /* GPFORGE_FONT_H */
