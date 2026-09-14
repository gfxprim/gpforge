/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file compose.h
 * @brief Accented letters: the decomposition table and accent composition.
 *
 * The table is generated from the unicode decompositions.  It says which
 * letters are accented forms of which letter — that is what the editor works
 * on, the letters that have to follow when a letter is edited — and, for
 * drawing a new one, what to draw it from and which accent to put on it.
 *
 * The composition is what draws a new letter: the accent taken off a letter
 * that has it, put on the base at gpf_accent_offset().
 */
#ifndef GPFORGE_COMPOSE_H
#define GPFORGE_COMPOSE_H

#include <stdint.h>

#include "font.h"
#include "resolve.h"

/**
 * @brief Accent location in the glyph rectangle.
 */
enum gpf_accent_place {
	/** @brief Centred over the ink, one blank row above it. */
	GPF_ACCENT_ABOVE,
	/** @brief Centred under the ink, right below it. */
	GPF_ACCENT_BELOW,
	/** @brief Right aligned with the ink, right below it. */
	GPF_ACCENT_OGONEK,
};

/**
 * @brief A glyph decomposition.
 */
struct gpf_decomposition {
	/** @brief A unicode code point of the accented letter. */
	uint32_t code;
	/** @brief The letter this is an accented form of, e.g. 'i' for 'í'. */
	uint32_t base_code;
	/**
	 * @brief What a new one is drawn from.
	 *
	 * The same letter except for an accent above an i or a j: those are
	 * drawn on the dotless letter, or the dot would sit under the accent.
	 */
	uint32_t draw_base;
	/**
	 * @brief The combining mark unicode decomposes the letter into.
	 *
	 * Two accents, because a font can carry two, and neither is where the
	 * accent usually comes from — gpf_accent_ink() takes that off a letter
	 * that has it.  A bitmap font rarely draws the combining mark; it is
	 * the first fallback.
	 */
	uint32_t mark;
	/**
	 * @brief The spacing accent, the one that gets typed.
	 *
	 * Drawn to fill an advance and wider than the one inside a letter, it
	 * is the second fallback.
	 */
	uint32_t accent;
	/** @brief Where the accent is placed in the glyph rectangle. */
	enum gpf_accent_place place;
};

/**
 * @brief A decomposition table.
 *
 * Zero terminated, sorted by the codepoint.
 */
extern const struct gpf_decomposition gpf_decompositions[];

/**
 * @brief Returns all accented forms of a letter.
 *
 * The accented forms of a letter, sorted by the codepoint: `A` has `À Á Â Ã Ä
 * Å Ā Ă Ą Ǎ`, `i` has `ì í î ï ĩ ī ĭ į ǐ`.  These are the letters the update
 * box copies an edit of the letter into.
 *
 * @param base_code A base code to lookup the accented variants for.
 * @param codes An array to fill the accented letters into.
 * @param max The size of the codes array.
 *
 * @return How many codes were written into codes, at most max.
 */
unsigned int gpf_decompose_accented(uint32_t base_code, uint32_t *codes,
                                    unsigned int max);

/**
 * @brief Looks up all glyphs for a given accent.
 *
 * The letters an accent goes on, sorted by the codepoint: the caron — the
 * combining U+030C or the spacing U+02C7, either names it — has `Č Ď Ě Ľ Ň Ř
 * Š Ť Ž` and their lower case.  These are the letters the accented letters
 * strip shows composed while the mark is selected.
 *
 * @param accent An accent to lookup the accented variants for.
 * @param codes An array to fill the accented letters into.
 * @param max The size of the codes array.
 *
 * @return How many codes were written into codes, at most max.
 */
unsigned int gpf_decompose_by_accent(uint32_t accent, uint32_t *codes,
                                     unsigned int max);

/**
 * @brief Looks up the base letter of an accented letter.
 *
 * The letter whose accented forms to work with when this glyph is selected:
 * the glyph itself, or the letter it is an accented form of.  Selecting `í`
 * and selecting `i` show the same family.
 *
 * @param code A codepoint to lookup the base variant for.
 * @return A base codepoint, if found, the code passed as argument otherwise.
 */
uint32_t gpf_decompose_base(uint32_t code);

/**
 * @brief Looks up the decomposition of an accented letter.
 *
 * @param code A codepoint to look up.
 * @return The decomposition of a codepoint, or NULL when it is not one.
 */
const struct gpf_decomposition *gpf_decompose(uint32_t code);

/**
 * @brief The default offset for an accent on a base.
 *
 * The starting point a hand adjustment moves from.  The accent is kept inside
 * the font's box: a letter that already reaches the ascent leaves no room
 * above it, and an accent drawn outside the box is drawn into the line above.
 * It is pushed down (or up) until it fits, which is what a bitmap font does at
 * small sizes anyway.
 *
 * @param font A font, its ascent and descent are the box; NULL for no clamp.
 * @param base The base glyph.
 * @param accent The accent glyph.
 * @param place Where the accent goes.
 * @param dx Where to store the offset for the accent ink, positive right.
 * @param dy Where to store the offset for the accent ink, positive up.
 */
void gpf_accent_offset(const struct gpf_font *font,
                       const struct gpf_glyph *base,
                       const struct gpf_glyph *accent,
                       enum gpf_accent_place place, int *dx, int *dy);

/**
 * @brief Composes the base ink with the accent ink.
 *
 * The metrics come from the base.
 *
 * @param dst Where to store the composed glyph into.
 * @param base A base glyph.
 * @param accent An accent glyph.
 * @param dx Offset for the accent ink, positive right.
 * @param dy Offset for the accent ink, positive up.
 * @return Non-zero on an allocation failure.
 */
int gpf_compose(struct gpf_glyph *dst, const struct gpf_glyph *base,
                const struct gpf_glyph *accent, int dx, int dy);

/**
 * @brief Subtracts the base ink from a glyph.
 *
 * Fails when the base does not sit inside the glyph pixel for pixel, or when
 * nothing is left after the subtraction.
 *
 * @param glyph An accented letter.
 * @param base The letter it is drawn from.
 * @param rem Where to store what is left, the accent.
 * @return Zero on success, non-zero otherwise.
 */
int gpf_compose_remainder(const struct gpf_glyph *glyph,
                          const struct gpf_glyph *base, struct gpf_glyph *rem);

/**
 * @brief The letter a new accented letter is drawn from.
 *
 * The decomposition's gpf_decomposition::draw_base, and for an i or a j that
 * the font has no dotless form of, the letter with its dot taken off.
 *
 * @param font A font.
 * @param id A glyph variant id e.g. GPF_BOLD.
 * @param decomp A decomposition description.
 * @param res Where to store the base, free it with gpf_resolved_clear().
 *
 * @return Non-zero when the font has no letter to draw it from.
 */
int gpf_draw_base(struct gpf_font *font, enum gpf_variant_id id,
                  const struct gpf_decomposition *decomp,
                  struct gpf_resolved *res);

/**
 * @brief The accent of a decomposition, as this font draws it.
 *
 * Attempts to resolve accent ink from existing letters. E.g. caron can be
 * resolved from 'Č' minus 'C'. The accent is subtracted from whichever glyph
 * carries it. Where it goes on the new letter is gpf_accent_offset().
 *
 * Falls back to the combining mark and then to the spacing accent, for a font
 * that has no letter to take it from.
 *
 * @param font A font.
 * @param id A glyph variant id e.g. GPF_BOLD.
 * @param decomp A decomposition description.
 * @param out Where to store the result to.
 *
 * @return Non-zero when the font has no way to draw this accent.
 */
int gpf_accent_ink(struct gpf_font *font, enum gpf_variant_id id,
                   const struct gpf_decomposition *decomp,
                   struct gpf_glyph *out);

#endif /* GPFORGE_COMPOSE_H */
