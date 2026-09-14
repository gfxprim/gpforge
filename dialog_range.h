/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file dialog_range.h
 * @brief The codepoint range dialog.
 *
 * The codepoint range dialog, used to delete a part of a block rather than all
 * of it.  Both fields start at the block being browsed, so the common case —
 * "this block, from here to there" — is two edits.
 */
#ifndef GPFORGE_DIALOG_RANGE_H
#define GPFORGE_DIALOG_RANGE_H

#include <stdint.h>

/**
 * @brief Asks for a codepoint range.
 *
 * @param title The dialog title.
 * @param min The first codepoint, the prefilled value in and the answer out.
 * @param max The last codepoint, the prefilled value in and the answer out.
 * @return Zero when the user accepted a valid range, min not above max,
 *         non-zero otherwise; min and max are left alone then.
 */
int gpf_dialog_range(const char *title, uint32_t *min, uint32_t *max);

#endif /* GPFORGE_DIALOG_RANGE_H */
