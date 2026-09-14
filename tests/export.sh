#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
#
# The C export over the corpus: every UTF font is exported, compiled into a
# shared object and checked by tests/export_cmp, the mono face against the BDF
# itself and every face against the model.  See tests/export_cmp.c for what
# counts as the same glyph.
#
# Skips cleanly when the corpus is not there.

set -e

FONTS=${FONTS:-$HOME/Devel/fonts}
CC=${CC:-cc}
TMP=$(mktemp -d)

trap 'rm -rf $TMP' EXIT

if [ ! -d "$FONTS" ]; then
	echo "export: no corpus in $FONTS, skipped"
	exit 0
fi

fonts=0

for bdf in "$FONTS"/*-utf.bdf; do
	[ -e "$bdf" ] || continue

	name=$(basename "$bdf" .bdf)

	./gpforge-cli c "$bdf" font "$name" > "$TMP/font.c" 2> /dev/null

	# the family symbol is hidden, this is how the test gets at it
	echo 'const gp_font_family *export_family(void) { return &font_family_font; }' \
		>> "$TMP/font.c"

	$CC $(pkg-config --cflags gfxprim) -shared -fPIC "$TMP/font.c" \
	    -o "$TMP/font.so"

	echo "$name"

	if ! ./tests/export_cmp "$TMP/font.so" "$bdf" > "$TMP/out"; then
		sed 's/^/\t/' "$TMP/out"
		echo "export: $name FAILED"
		exit 1
	fi

	sed 's/^/\t/' "$TMP/out"

	fonts=$((fonts + 1))
done

echo "export: $fonts fonts ok"
