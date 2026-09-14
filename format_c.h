/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file format_c.h
 * @brief The gfxprim C font export.
 *
 * Writes the font as a compiled-in gfxprim font.
 */
#ifndef GPFORGE_EXPORT_H
#define GPFORGE_EXPORT_H

#include <stdio.h>

#include "font.h"

/**
 * @brief Exports the font into a C source.
 *
 * Writes the C font to f.  font_id names the C symbols and the faces, name is
 * the family name gfxprim looks fonts up by — "haxor_narrow_18" and
 * "haxor-narrow-18".  Returns zero on success.
 *
 * @param font A font.
 * @param font_id A font id, the suffix of the C symbols and the face names.
 * @param name A font family name as stored in the C structure.
 * @param f A file to write the font into.
 * @param err A buffer to write an error into.
 * @param err_len A length of the err buffer.
 * @return Zero on success, non-zero otherwise.
 */
int gpf_export_c(struct gpf_font *font, const char *font_id, const char *name,
                 FILE *f, char *err, size_t err_len);

#endif /* GPFORGE_EXPORT_H */
