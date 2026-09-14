#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
#
# The corpus is the regression suite.
#
# For each BDF font: import it, compare a dump of the import against a dump of
# the font loaded back from the directory, then rewrite the directory and
# compare it byte for byte.  The first catches the format losing something,
# the second catches the reader and the writer disagreeing.
#
FONTS=${FONTS:-$HOME/Devel/fonts}
CLI=./gpforge-cli

if [ ! -d "$FONTS" ]; then
	echo "roundtrip: no corpus in $FONTS, skipping"
	exit 0
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

fail=0
cnt=0

for bdf in "$FONTS"/*.bdf; do
	[ -e "$bdf" ] || continue

	name=$(basename "$bdf" .bdf)
	dir="$tmp/$name"
	redir="$tmp/$name-rewrite"

	cnt=$((cnt + 1))

	if ! $CLI import "$bdf" "$dir"; then
		echo "FAIL $name: import"
		fail=$((fail + 1))
		continue
	fi

	# The comparison is between the resolved faces, not the stored entries:
	# the writer is allowed to store a glyph in any form that renders the
	# same, and it does — an accented glyph it recognises is written as a
	# composition.  What has to be identical is what a consumer sees.
	bad=0

	# The provenance is dropped from the comparison: it says where a glyph
	# came from, which is exactly what the rewrite is allowed to change.
	strip='s/^\(U+[0-9A-F]*\) [a-z-]* /\1 /'

	for variant in mono regular bold-mono bold; do
		$CLI face "$bdf" $variant 2>/dev/null | sed "$strip" > "$tmp/bdf.face"

		if ! $CLI face "$dir" $variant | sed "$strip" > "$tmp/dir.face"; then
			echo "FAIL $name: load"
			bad=1
			break
		fi

		if ! diff -u "$tmp/bdf.face" "$tmp/dir.face" > "$tmp/diff"; then
			echo "FAIL $name: the $variant face changed on a round trip"
			head -20 "$tmp/diff"
			bad=1
			break
		fi
	done

	if [ "$bad" != 0 ]; then
		fail=$((fail + 1))
		continue
	fi

	if ! $CLI write "$dir" "$redir"; then
		echo "FAIL $name: rewrite"
		fail=$((fail + 1))
		continue
	fi

	if ! diff -r "$dir" "$redir" > "$tmp/diff"; then
		echo "FAIL $name: a rewrite is not identical"
		head -20 "$tmp/diff"
		fail=$((fail + 1))
		continue
	fi

	# an iso8859-2 font has to land on unicode codepoints, not on its own
	case "$name" in
	*-utf) ;;
	Haxor*)
		if ! grep -q '^glyph U+0160 ' "$dir/mono"; then
			echo "FAIL $name: iso8859-2 was not mapped to unicode"
			fail=$((fail + 1))
			continue
		fi
	;;
	esac

	glyphs=$(grep -c '^glyph ' "$dir/mono" 2>/dev/null || echo 0)
	composed=$(grep -c '^compose ' "$dir/mono" 2>/dev/null || echo 0)
	size=$(du -sh "$dir" | cut -f1)

	echo "ok   $name: $glyphs glyphs, $composed composed, $size"
done

if [ "$fail" != 0 ]; then
	echo "roundtrip: $fail of $cnt fonts failed"
	exit 1
fi

echo "roundtrip: $cnt fonts ok"
