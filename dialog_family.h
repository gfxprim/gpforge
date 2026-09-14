/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file dialog_family.h
 * @brief The font properties dialog.
 *
 * The family file's metadata, less the declared blocks, which have buttons of
 * their own.  It edits a copy and the caller journals it, so all of it is one
 * Ctrl+Z.
 *
 * The three metrics the importer measures have a button that measures them
 * again off the variant being edited.  The em has none, because it is a
 * decision and not a measurement.
 *
 * The authors are a table with add and remove below it.  Selecting a row fills
 * the fields in, so an author is corrected by removing the row and adding it
 * back.  The list is a copy as well and goes into the same journal entry.
 *
 * It is the new font dialog as well: the same numbers are what a font starts
 * from, plus the advance of the mono cell, which a font that has glyphs cannot
 * change here — every glyph carries its own, so it is shown and greyed out.
 */
#ifndef GPFORGE_DIALOG_FAMILY_H
#define GPFORGE_DIALOG_FAMILY_H

#include "font.h"

/**
 * @brief Runs the font properties dialog.
 *
 * With advance it is the new font dialog and asks for the mono advance as
 * well; without it it edits gui.font, and fails when there is none.
 *
 * @param meta The metadata the dialog starts from, overwritten with what the
 *             user entered.
 * @param authors The author list, a gp_vec the dialog edits in place whatever
 *                the answer, so the caller passes a copy.
 * @param advance The mono advance, both in and out, or NULL when editing an
 *                existing font.
 * @return Zero when the user accepted the dialog, non-zero when it was
 *         cancelled; meta and advance are left alone then.
 */
int gpf_dialog_family(struct gpf_family_meta *meta,
                      struct gpf_author **authors, int *advance);

#endif /* GPFORGE_DIALOG_FAMILY_H */
