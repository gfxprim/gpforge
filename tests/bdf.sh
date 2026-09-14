#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
#
# The BDF export against the BDF importer, over the corpus: every variant of
# every font is exported as a BDF, read back, and the resolved face of the
# import is compared against the face it was written from.
#
# A BDF holds one face, so the variant it lands in on the way back is the one
# its SPACING says — mono for a monospaced export, regular for a proportional
# one — and that is what the comparison asks for.  What has to be identical is
# every glyph's ink, bearings and advance; the provenance cannot be, since a
# BDF has nowhere to say that a glyph was derived and everything in it is
# drawn.
#
# Skips cleanly when the corpus is not there.

FONTS=${FONTS:-$HOME/Devel/fonts}
CLI=./gpforge-cli

if [ ! -d "$FONTS" ]; then
	echo "bdf: no corpus in $FONTS, skipping"
	exit 0
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

fail=0
cnt=0

# the provenance says where a glyph came from, which the export flattens
strip='s/^\(U+[0-9A-F]*\) [a-z-]* /\1 /'

for bdf in "$FONTS"/*.bdf; do
	[ -e "$bdf" ] || continue

	name=$(basename "$bdf" .bdf)
	cnt=$((cnt + 1))
	bad=0

	for variant in mono regular bold-mono bold; do
		# a proportional face comes back as regular, a mono one as mono
		case "$variant" in
		mono|bold-mono) back=mono ;;
		*)              back=regular ;;
		esac

		if ! $CLI bdf "$bdf" $variant > "$tmp/out.bdf" 2> "$tmp/err"; then
			echo "FAIL $name: export $variant"
			cat "$tmp/err"
			bad=1
			break
		fi

		$CLI face "$bdf" $variant | sed "$strip" > "$tmp/a.face"

		if ! $CLI face "$tmp/out.bdf" $back | sed "$strip" > "$tmp/b.face"; then
			echo "FAIL $name: the exported $variant does not read back"
			bad=1
			break
		fi

		if ! diff -u "$tmp/a.face" "$tmp/b.face" > "$tmp/diff"; then
			echo "FAIL $name: the $variant face changed through a BDF"
			head -20 "$tmp/diff"
			bad=1
			break
		fi
	done

	if [ "$bad" != 0 ]; then
		fail=$((fail + 1))
		continue
	fi

	glyphs=$(grep -c '^STARTCHAR ' "$tmp/out.bdf")

	echo "ok   $name: 4 variants, $glyphs glyphs"
done

if [ "$fail" != 0 ]; then
	echo "bdf: $fail of $cnt fonts failed"
	exit 1
fi

echo "bdf: $cnt fonts ok"
