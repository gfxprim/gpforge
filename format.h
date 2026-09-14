/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file format.h
 * @brief Native font format.
 *
 * \includedoc FONT_FORMAT.md
 */
#ifndef GPFORGE_FORMAT_H
#define GPFORGE_FORMAT_H

#include "font.h"

#define GPF_ERR_MAX 256

/**
 * @brief Writes a font into a directory.
 *
 * Writes the font into a directory, creating it when needed. Only variants
 * that exist in the model are written.
 *
 * @param self A font to write.
 * @param dir A directory to write the font into.
 * @param err A buffer to write an error into.
 * @param err_len The size of the err buffer.
 * @return Zero on success, non-zero on a failure with err filled in.
 */
int gpf_font_write(struct gpf_font *self, const char *dir,
                   char *err, size_t err_len);

/**
 * @brief Loads a font from a directory.
 *
 * @param dir A directory to read the font from.
 * @param err A buffer to write an error into.
 * @param err_len The size of the err buffer.
 * @return A font, or NULL with err filled in on a failure.
 */
struct gpf_font *gpf_font_read(const char *dir, char *err, size_t err_len);

int gpf_code_printable(uint32_t code);

#endif /* GPFORGE_FORMAT_H */
