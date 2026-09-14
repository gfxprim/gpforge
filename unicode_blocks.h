/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file unicode_blocks.h
 * @brief The BMP part of unicode block table and the blocks of a font.
 *
 * The global table lists every unicode block, a font's block list is made of
 * copies of its entries with the coverage filled in.
 */
#ifndef GPFORGE_UNICODE_BLOCKS_H
#define GPFORGE_UNICODE_BLOCKS_H

#include <stdint.h>

#include "font.h"

/**
 * @brief A unicode block description.
 */
struct gpf_ucode_block {
	/** @brief First code point in the block. */
	uint32_t min;
	/** @brief Last code point in the block. */
	uint32_t max;
	/** @brief Human readable name of the block. */
	const char *name;
	/**
	 * @brief Number of glyphs the font has in this block.
	 *
	 * Filled in a font's block list only, always zero in gpf_ucode_blocks.
	 */
	unsigned int coverage;
};

/**
 * @brief A NULL name terminated array of unicode blocks.
 */
extern const struct gpf_ucode_block gpf_ucode_blocks[];

/**
 * @brief Looks up the preview sample text for a unicode block.
 *
 * @param name A unicode block name, as in gpf_ucode_blocks.
 * @return A sample text in UTF-8, or NULL for a block we have no sample for,
 *         which the preview fills with the font's own glyphs instead.
 */
const char *gpf_ucode_block_sample(const char *name);

/**
 * @brief Builds a list of blocks for a font.
 *
 * The blocks the font has glyphs in, the ones declared in the font family and
 * Basic Latin when that leaves the list empty.
 *
 * @param font A font.
 * @return A gp_vec of gpf_ucode_block structures sorted by codepoints.
 */
struct gpf_ucode_block *gpf_blocks_build(struct gpf_font *font);

/**
 * @brief Looks up a block by a codepoint.
 *
 * @param blocks A gp_vec of unicode blocks.
 * @param code A codepoint to lookup a block for.
 * @return An index into the list, or -1 when the codepoint is in none of its
 *         blocks.
 */
int gpf_blocks_find(const struct gpf_ucode_block *blocks, uint32_t code);

/**
 * @brief Counts number of codepoints in a unicode block.
 *
 * @param block A unicode block.
 * @return A number of codepoints in a unicode block.
 */
static inline uint32_t gpf_block_glyphs(const struct gpf_ucode_block *block)
{
	return block->max - block->min + 1;
}

#endif /* GPFORGE_UNICODE_BLOCKS_H */
