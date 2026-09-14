/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file format_bdf.h
 * @brief BDF import, and BDF export.
 */
#ifndef GPFORGE_BDF_H
#define GPFORGE_BDF_H

#include "font.h"

/**
 * @brief Reads a BDF font.
 *
 * @param path A path to the BDF font file.
 * @param err A buffer to write an error into.
 * @param err_len The size of the err buffer.
 * @return A parsed font, or NULL with err filled in on a failure.
 */
struct gpf_font *gpf_bdf_read(const char *path, char *err, size_t err_len);

/**
 * @brief Writes a BDF font.
 *
 * Writes one variant of the font.  A BDF file is one face, so which variant
 * it is is the caller's decision, and the face is the resolved one.
 *
 * @param font A font to be exported to BDF.
 * @param id A variant id to be exported to BDF, e.g. GPF_BOLD.
 * @param f A FILE to write the BDF font to.
 * @param err A buffer to write an error into.
 * @param err_len The size of the err buffer.
 * @return Zero on success, non-zero on a failure with err filled in.
 */
int gpf_bdf_write(struct gpf_font *font, enum gpf_variant_id id, FILE *f,
                  char *err, size_t err_len);

#endif /* GPFORGE_BDF_H */
