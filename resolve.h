/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file resolve.h
 * @brief The glyph variant lattice.
 */
#ifndef GPFORGE_RESOLVE_H
#define GPFORGE_RESOLVE_H

#include "font.h"

/**
 * @brief Glyph provenance.
 *
 * Where the ink of a resolved glyph came from. The editor shows this per
 * glyph, it answers "why does this glyph look like that".
 */
enum gpf_provenance {
	/** @brief The ink is stored in the variant itself. */
	GPF_PROV_STORED,
	/** @brief Built from a base glyph and an accent. */
	GPF_PROV_COMPOSED,
	/** @brief The parent's ink, the metrics may still be overridden. */
	GPF_PROV_INHERITED,
	/** @brief The parent's ink, emboldened. */
	GPF_PROV_EMBOLDENED,
	/** @brief Bold taking a hand fixed glyph from bold-mono. */
	GPF_PROV_BOLD_MONO,
};

/**
 * @brief A resolved glyph and where it was drawn from.
 */
struct gpf_resolved {
	/**
	 * @brief Freshly allocated glyph with copied/generated ink.
	 *
	 * Free it with gpf_resolved_clear().
	 */
	struct gpf_glyph glyph;
	/** @brief Where the glyph came from. */
	enum gpf_provenance prov;
	/** @brief The variant the ink came from. */
	enum gpf_variant_id ink_from;
};

/**
 * @brief Returns provenance name.
 *
 * @param prov A provenance.
 * @return A provenance name.
 */
const char *gpf_provenance_name(enum gpf_provenance prov);

/**
 * @brief Resolves a glyph variant metric and ink.
 *
 * @param font A font.
 * @param id A variant id e.g. GPF_MONO, GPF_BOLD, etc.
 * @param code An unicode codepoint to resolve.
 * @param res A resolved struct to store the result into.
 *
 * @return Zero on success, non-zero when the font has no such glyph or on an
 *         allocation failure.
 */
int gpf_resolve(struct gpf_font *font, enum gpf_variant_id id, uint32_t code,
                struct gpf_resolved *res);

/**
 * @brief Resolves a derived glyph variant metric and ink.
 *
 * What the glyph would be if this variant had no entry for it — the thing an
 * overlay entry is an override of. Used when saving to write the smallest
 * form an entry can take.
 *
 * @param font A font.
 * @param id A variant id e.g. GPF_MONO, GPF_BOLD, etc.
 * @param code An unicode codepoint to resolve.
 * @param res A resolved struct to store the result into.
 *
 * @return Zero on success, non-zero when the font has no such glyph or on an
 *         allocation failure.
 */
int gpf_resolve_derived(struct gpf_font *font, enum gpf_variant_id id,
                        uint32_t code, struct gpf_resolved *res);

/**
 * @brief Clears the resolved glyph.
 *
 * Frees the ink and clears the gpf_resolved::glyph structure.
 *
 * @param self A resolved glyph.
 */
void gpf_resolved_clear(struct gpf_resolved *self);

/**
 * @brief The smallest form an entry can be written in on save.
 */
enum gpf_entry_form {
	/** @brief Identical to what the variant would derive, no entry needed. */
	GPF_ENTRY_NONE,
	/** @brief The ink is the parent's, only numbers differ. */
	GPF_ENTRY_METRICS,
	/** @brief The ink differs, it has to be saved. */
	GPF_ENTRY_INK,
};

/**
 * @brief Computes what should be saved to disk for a glyph variant.
 *
 * Compares the entry against gpf_resolve_derived(). Only an ink entry in a
 * variant with a parent can shrink, anything else is GPF_ENTRY_INK.
 *
 * @param font A font.
 * @param id A glyph variant e.g. GPF_REGULAR, GPF_MONO, etc.
 * @param glyph A glyph entry to be saved.
 * @param out A numbers only entry, filled with the numbers that differ,
 *            meaningful for GPF_ENTRY_METRICS.
 *
 * @return What should be saved.
 */
enum gpf_entry_form gpf_entry_form(struct gpf_font *font,
                                   enum gpf_variant_id id,
                                   const struct gpf_glyph *glyph,
                                   struct gpf_glyph *out);

/**
 * @brief Simplifies a font.
 *
 * Rewrites every overlay entry into the smallest form that is true, the same
 * decision the writer makes.
 *
 * Editing materializes glyphs as ink, so this is what makes the editor
 * say "inherited" again after a metrics only edit instead of "stored".
 *
 * @param font A font.
 */
void gpf_font_simplify(struct gpf_font *font);

/*
 * The composition that defines a glyph in a variant, from the variant itself
 * or inherited from a parent, or NULL when the glyph is not composed.  A
 * composition is inherited as a definition, so that a variant which redraws
 * the base redraws everything composed from it.
 */
struct gpf_glyph *gpf_compose_def(struct gpf_font *font,
                                  enum gpf_variant_id id, uint32_t code);

/**
 * @brief The family metrics that are measured off the glyphs.
 */
enum gpf_measure {
	/** @brief The top of the 'x' ink above the baseline. */
	GPF_MEASURE_X_HEIGHT,
	/** @brief The top of the 'H' ink above the baseline. */
	GPF_MEASURE_CAP_HEIGHT,
	/** @brief The advance of the '0' glyph. */
	GPF_MEASURE_CH_WIDTH,
};

/**
 * @brief Measures a font metric.
 *
 * @param font A font.
 * @param id A glyph variant e.g. GPF_REGULAR, GPF_MONO, etc.
 * @param what Which metric to measure.
 * @param val Where to store the result to.
 * @return Zero when the val was set, non-zero when the variant doesn't have
 *         the letter, or, for the heights, when its ink is empty.
 */
int gpf_measure(struct gpf_font *font, enum gpf_variant_id id,
                enum gpf_measure what, int *val);

#endif /* GPFORGE_RESOLVE_H */
