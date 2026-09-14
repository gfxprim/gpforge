/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/**
 * @file face.h
 * @brief Compiles a gfxprim font face from an in-memory gpf font.
 *
 * Compiles a resolved variant into a gfxprim font face, so that the preview is
 * drawn by gfxprim itself and is exactly what a consumer would render.
 */
#ifndef GPFORGE_FACE_H
#define GPFORGE_FACE_H

#include <text/gp_font.h>

#include "font.h"

/**
 * @brief Builds a gfxprim font face from one variant of a gpf_font.
 *
 * @param font A font.
 * @param id A font variant to build, e.g. GPF_BOLD.
 * @return A compiled font face or NULL in case of a failure.
 */
gp_font_face *gpf_face_build(struct gpf_font *font, enum gpf_variant_id id);

/**
 * @brief Frees a font face.
 *
 * @param self A font face to free.
 */
void gpf_face_free(gp_font_face *self);

#endif /* GPFORGE_FACE_H */
