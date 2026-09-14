/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file dialog_save.h
 * @brief The unsaved changes dialog.
 *
 * "Discard them?" was the wrong question: the answer to unsaved work is
 * usually to save it, so the dialog offers that first and keeps cancel as the
 * way out.
 */
#ifndef GPFORGE_DIALOG_SAVE_H
#define GPFORGE_DIALOG_SAVE_H

/**
 * @brief What to do about unsaved changes.
 *
 * None of them is zero: the answer is the dialog's return value, and zero is
 * "still running" to gp_dialog_run().
 */
enum gpf_save_answer {
	/** Save the font, then go on. */
	GPF_SAVE_SAVE = 1,
	/** Throw the changes away and go on. */
	GPF_SAVE_DISCARD,
	/** Do not go on. */
	GPF_SAVE_CANCEL,
};

/**
 * @brief Asks what to do about unsaved changes.
 *
 * @param title The dialog title, which says what asked.
 * @return The answer, GPF_SAVE_CANCEL when the dialog could not be shown or
 *         was closed without one.
 */
enum gpf_save_answer gpf_dialog_save(const char *title);

#endif /* GPFORGE_DIALOG_SAVE_H */
