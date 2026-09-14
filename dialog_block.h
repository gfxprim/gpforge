/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file dialog_block.h
 * @brief The block picker dialog.
 *
 * The block picker, which is how a block that the font does not have yet is
 * added to it.  It lists every unicode block that is not on the font's list
 * already.
 */
#ifndef GPFORGE_DIALOG_BLOCK_H
#define GPFORGE_DIALOG_BLOCK_H

#include "unicode_blocks.h"

/**
 * @brief Runs the block picker.
 *
 * Tells the user so, and returns NULL, when the font has every block already.
 *
 * @return The chosen block, or NULL when the dialog was cancelled.  The block
 *         is valid until the next call.
 */
const struct gpf_ucode_block *gpf_dialog_block_add(void);

#endif /* GPFORGE_DIALOG_BLOCK_H */
