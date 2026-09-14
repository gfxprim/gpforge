/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file undo.h
 * @brief The undo journal.
 *
 * One journal for the whole family, one entry per stroke, per command, per
 * metric change, per overlay created or reverted. An entry is a before and an
 * after snapshot of a single glyph entry in a single variant, either of which
 * may be "the entry did not exist" — which is what creating an overlay and
 * reverting one are.
 *
 * The other kind of entry is a change to the family metadata, a before and an
 * after snapshot of struct gpf_family_meta and of the font authors.
 */
#ifndef GPFORGE_UNDO_H
#define GPFORGE_UNDO_H

#include "font.h"

struct gpf_undo;

/**
 * @brief Creates a new undo.
 *
 * @return A pointer to newly allocated and initialized undo or NULL in a case
 * of a failure.
 */
struct gpf_undo *gpf_undo_new(void);

/**
 * @brief Frees undo.
 *
 * @param self An undo journal to be freed.
 */
void gpf_undo_free(struct gpf_undo *self);

/**
 * @brief Starts an undo group.
 *
 * Entries recorded between a group begin and end are undone and redone
 * together.
 *
 * E.g. deleting a range of glyphs is one action to the user, however many
 * glyphs and variants it touches.
 *
 * @param self An undo journal.
 */
void gpf_undo_group_begin(struct gpf_undo *self);

/**
 * @brief Ends an undo group.
 *
 * Entries recorded between a group begin and end are undone and redone
 * together.
 *
 * E.g. deleting a range of glyphs is one action to the user, however many
 * glyphs and variants it touches.
 *
 * @param self An undo journal.
 */
void gpf_undo_group_end(struct gpf_undo *self);

/**
 * @brief Snapshots a glyph before it is changed.
 *
 * Called before every glyph edit. A second call without a commit in between
 * replaces the snapshot, so a stroke that starts over is not half recorded.
 *
 * @param self An undo journal.
 * @param font A font to snapshot the glyph from.
 * @param id A glyph variant e.g. GPF_BOLD.
 * @param code A unicode codepoint of the glyph to snapshot.
 *
 * @return Non-zero on allocation failure.
 */
int gpf_undo_begin(struct gpf_undo *self, struct gpf_font *font,
                   enum gpf_variant_id id, uint32_t code);

/**
 * @brief Finishes a snapshot and starts a snapshot for a different glyph.
 *
 * If the pending snapshot is already for this glyph and variant it's a no-op.
 * Otherwise commits whatever was pending first with gpf_undo_commit(), then
 * starts a new glyph snapshot with gpf_undo_begin().
 *
 * @param self An undo journal.
 * @param font A font to snapshot the glyph from.
 * @param id A glyph variant e.g. GPF_BOLD.
 * @param code A unicode codepoint of the glyph to snapshot.
 *
 * @return Non-zero on allocation failure.
 */
int gpf_undo_retarget(struct gpf_undo *self, struct gpf_font *font,
                      enum gpf_variant_id id, uint32_t code);

/**
 * @brief Finishes an undo on a glyph started by gpf_undo_begin().
 *
 * Records what the glyph became. An edit that changed nothing is dropped.
 *
 * @param self An undo journal.
 * @param font A font.
 */
void gpf_undo_commit(struct gpf_undo *self, struct gpf_font *font);

/**
 * @brief Sets the font metadata and authors and saves the old ones into the
 *        undo journal.
 *
 * Sets the family metadata and the authors, as one journal entry.
 *
 * The call is a no-op if both are the same as the font's.
 *
 * @param self An undo journal.
 * @param font A font family.
 * @param meta New font family metadata.
 * @param authors New gp_vec of authors, copied, may be NULL for none.
 *
 * @return Non-zero on an allocation failure.
 */
int gpf_undo_family(struct gpf_undo *self, struct gpf_font *font,
                    const struct gpf_family_meta *meta,
                    const struct gpf_author *authors);

/**
 * @brief Undo type.
 */
enum gpf_undo_what {
	/** @brief The journal had nothing at that end. */
	GPF_UNDO_NONE,
	/**
	 * @brief A glyph changed.
	 *
	 * Id and code say which.
	 */
	GPF_UNDO_GLYPH,
	/** @brief The family metadata or the authors changed. */
	GPF_UNDO_FAMILY,
};

/**
 * @brief Undoes the last entry, or group of entries.
 *
 * @param self An undo journal.
 * @param font A font to apply the undo on.
 * @param id When glyph was undone this carries the glyph variant e.g. GPF_BOLD.
 * @param code When glyph was undone this carries the glyph unicode codepoint.
 *
 * @return What was undone, GPF_UNDO_NONE if there was nothing to undo. A group
 *         that carried a glyph returns GPF_UNDO_GLYPH.
 */
enum gpf_undo_what gpf_undo(struct gpf_undo *self, struct gpf_font *font,
                            enum gpf_variant_id *id, uint32_t *code);

/**
 * @brief Redoes the next entry, or group of entries.
 *
 * @param self An undo journal.
 * @param font A font to apply the redo on.
 * @param id When glyph was redone this carries the glyph variant e.g. GPF_BOLD.
 * @param code When glyph was redone this carries the glyph unicode codepoint.
 *
 * @return What was redone, GPF_UNDO_NONE if there was nothing to redo. A group
 *         that carried a glyph returns GPF_UNDO_GLYPH.
 */
enum gpf_undo_what gpf_redo(struct gpf_undo *self, struct gpf_font *font,
                            enum gpf_variant_id *id, uint32_t *code);

/**
 * @brief Marks the current state as the one written to the file.
 *
 * Undo and redo then clear the modified flag of glyphs they bring back to
 * that state.
 *
 * @param self An undo journal.
 */
void gpf_undo_saved(struct gpf_undo *self);

/**
 * @brief Returns if the font is in the state written to the file.
 *
 * That is the state at the last gpf_undo_saved(), or the one the journal was
 * created with.
 *
 * @param self An undo journal.
 * @return True if the journal is at the saved state.
 */
int gpf_undo_is_saved(struct gpf_undo *self);

/**
 * @brief Returns if undo is possible.
 *
 * @param self An undo journal, may be NULL.
 * @return True if undo is possible.
 */
int gpf_undo_can_undo(struct gpf_undo *self);

/**
 * @brief Returns if redo is possible.
 *
 * @param self An undo journal, may be NULL.
 * @return True if redo is possible.
 */
int gpf_undo_can_redo(struct gpf_undo *self);

#endif /* GPFORGE_UNDO_H */
