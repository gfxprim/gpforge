/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file lint.h
 * @brief The lint checks.
 *
 * Linter checks for the glyphs. E.g. ink that leaves a monospace cell, an
 * advance that disagrees with the variant, a glyph the base variant has not
 * got.
 */
#ifndef GPFORGE_LINT_H
#define GPFORGE_LINT_H

#include "font.h"

#define GPF_LINT_MSG_MAX 72

/** @brief Linter severity. */
enum gpf_lint_severity {
	/** @brief The font renders wrong. */
	GPF_LINT_ERROR,
	/** @brief Probably a slip. May be deliberate. */
	GPF_LINT_WARN,
};

/** @brief A linter finding. */
struct gpf_lint_finding {
	/** @brief Variant of the glyph the finding is for. */
	enum gpf_variant_id id;
	/** @brief A unicode code point the finding is for. */
	uint32_t code;
	/** @brief Severity. */
	enum gpf_lint_severity sev;
	/** @brief Linter message. */
	char msg[GPF_LINT_MSG_MAX];
};

/**
 * @brief Runs linter on a font.
 *
 * Every finding in the font, as a gp_vec sorted by variant and then by
 * codepoint. Free it with gp_vec_free().
 *
 * @param font A font to run linter for.
 * @return A gp_vec of linter findings, NULL on an allocation failure.
 */
struct gpf_lint_finding *gpf_lint(struct gpf_font *font);

/**
 * @brief Runs a linter for a single glyph.
 *
 * The first finding for one glyph, or non zero when it has none.
 *
 * @param font A font to run the linter for.
 * @param id A glyph variant to run the linter for.
 * @param code A unicode code point of the glyph.
 * @param out Where to store the finding, if there was one.
 * @return Zero if a finding was stored into out, non-zero otherwise.
 */
int gpf_lint_glyph(struct gpf_font *font, enum gpf_variant_id id, uint32_t code,
                   struct gpf_lint_finding *out);

/*
 * How many findings of each severity the whole font has, counted over what
 * gpf_lint() returns — so the same numbers `gpforge-cli lint` prints.
 */
void gpf_lint_count(struct gpf_font *font, unsigned int *errors,
                    unsigned int *warnings);

/**
 * @brief Returns severity name.
 *
 * @param sev A severity level.
 * @return A name for the severity level.
 */
const char *gpf_lint_severity_name(enum gpf_lint_severity sev);

#endif /* GPFORGE_LINT_H */
