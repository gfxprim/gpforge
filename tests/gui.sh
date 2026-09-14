#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
#
# The editor itself, driven on a headless X display.  What the model tests
# cannot reach is the wiring between the widgets and the journal, and that is
# where "edit, undo, redo, undo, and the last undo does nothing" lived: the
# redo button disabled itself while it had the focus, the focus stayed on it
# because a disabled widget cannot be focused out of, and the click that should
# have gone to undo was delivered to the redo button instead.
#
# The font is opened as a directory so that Ctrl+S writes it with no dialog in
# the way: what the editor holds can then be read back as files and diffed.
# Every step says what it expects, so a layout change shows up as a failure
# here rather than as a test that quietly stops testing anything.
#
# Skips cleanly without the corpus, without Xvfb, and when tests/xdrive cannot
# be built — it needs libX11 and libXtst.

FONTS=${FONTS:-$HOME/Devel/fonts}
BDF="$FONTS/HaxorNarrow-18-utf.bdf"

# where the buttons are in a window with no decoration, which is what a bare
# Xvfb gives us.  Re-measure them if the toolbar moves.
PIXEL_X=243; PIXEL_Y=280
PIXEL2_X=273; PIXEL2_Y=280
PIXEL3_X=243; PIXEL3_Y=320
UNDO_X=33; UNDO_Y=562
REDO_X=93; REDO_Y=562
COPY_X=178; COPY_Y=562
PASTE_X=240; PASTE_Y=562
QUEST_X=607; QUEST_Y=290
AT_X=669; AT_Y=290

if [ ! -f "$BDF" ]; then
	echo "gui: no $BDF, skipping"
	exit 0
fi

if ! command -v Xvfb > /dev/null 2>&1; then
	echo "gui: no Xvfb, skipping"
	exit 0
fi

if [ ! -x ./tests/xdrive ]; then
	if ! make tests/xdrive > /dev/null 2>&1; then
		echo "gui: cannot build tests/xdrive, skipping"
		exit 0
	fi
fi

tmp=$(mktemp -d)
xvfb_pid=
app_pid=

cleanup()
{
	[ -n "$app_pid" ] && kill "$app_pid" 2>/dev/null
	[ -n "$xvfb_pid" ] && kill "$xvfb_pid" 2>/dev/null
	rm -rf "$tmp"
}

trap cleanup EXIT

# a display of our own: this clicks at absolute coordinates, which must not
# happen on a screen somebody is looking at
for n in 97 96 95 94 93; do
	Xvfb ":$n" -screen 0 1400x1050x24 > "$tmp/xvfb.log" 2>&1 &
	xvfb_pid=$!

	sleep 1

	if kill -0 "$xvfb_pid" 2>/dev/null; then
		DISPLAY=":$n"
		break
	fi

	xvfb_pid=
done

if [ -z "$xvfb_pid" ]; then
	echo "gui: no free display for Xvfb, skipping"
	exit 0
fi

export DISPLAY

if ! ./gpforge-cli import "$BDF" "$tmp/font" > /dev/null; then
	echo "FAIL gui: the import failed"
	exit 1
fi

./gpforge "$tmp/font" > "$tmp/app.log" 2>&1 &
app_pid=$!

sleep 4

if ! kill -0 "$app_pid" 2>/dev/null; then
	echo "FAIL gui: the editor did not start"
	sed 's/^/\t/' "$tmp/app.log"
	exit 1
fi

fail=0

save()
{
	./tests/xdrive ctrl s
	sleep 2
}

click()
{
	./tests/xdrive click "$1" "$2"
	sleep 1
}

# what the editor writes, which is not always the directory it read
save
cp -r "$tmp/font" "$tmp/ref"

# expect <same|changed> <reference dir> <what was done>
expect()
{
	if diff -rq "$2" "$tmp/font" > /dev/null 2>&1; then
		have=same
	else
		have=changed
	fi

	if [ "$have" = "$1" ]; then
		echo "ok   gui: $3"
		return
	fi

	echo "FAIL gui: $3: the font is $have, expected $1"
	fail=$((fail + 1))
}

# edit, undo, redo, undo — with the buttons, which is where it broke
click $PIXEL_X $PIXEL_Y; save
expect changed "$tmp/ref" "a pixel is drawn"

click $UNDO_X $UNDO_Y; save
expect same "$tmp/ref" "the undo button takes it back"

click $REDO_X $REDO_Y; save
expect changed "$tmp/ref" "the redo button puts it back"

click $UNDO_X $UNDO_Y; save
expect same "$tmp/ref" "the undo button after a redo takes it back"

click $REDO_X $REDO_Y; save
expect changed "$tmp/ref" "the second redo"

click $UNDO_X $UNDO_Y; save
expect same "$tmp/ref" "the second undo"

# three strokes, all the way back, all the way forward and back again
click $PIXEL_X $PIXEL_Y
click $PIXEL2_X $PIXEL2_Y
click $PIXEL3_X $PIXEL3_Y
save
cp -r "$tmp/font" "$tmp/three"
expect changed "$tmp/ref" "three strokes"

click $UNDO_X $UNDO_Y
click $UNDO_X $UNDO_Y
click $UNDO_X $UNDO_Y
save
expect same "$tmp/ref" "three undos"

click $REDO_X $REDO_Y
click $REDO_X $REDO_Y
click $REDO_X $REDO_Y
save
expect same "$tmp/three" "three redos"

click $UNDO_X $UNDO_Y
click $REDO_X $REDO_Y
click $UNDO_X $UNDO_Y
click $UNDO_X $UNDO_Y
click $UNDO_X $UNDO_Y
save
expect same "$tmp/ref" "undo, redo and three undos"

# the ink of one glyph as the variant resolves it, less the header line
ink()
{
	./gpforge-cli face "$tmp/font" mono | \
		awk -v g="^U\\\\+$1 " '$0 ~ g {p = 1; next} /^U\+/ {p = 0} p'
}

# @ copied with the button, pasted over ? with the key, and the other way
click $AT_X $AT_Y
click $COPY_X $COPY_Y
click $QUEST_X $QUEST_Y
./tests/xdrive ctrl v
sleep 1
save
expect changed "$tmp/ref" "@ pasted over ?"

if [ "$(ink 003F)" = "$(ink 0040)" ]; then
	echo "ok   gui: ? is drawn as @"
else
	echo "FAIL gui: ? is not drawn as @"
	fail=$((fail + 1))
fi

click $UNDO_X $UNDO_Y; save
expect same "$tmp/ref" "the paste is one undo"

./tests/xdrive ctrl c
sleep 1
click $AT_X $AT_Y
click $PASTE_X $PASTE_Y
save
expect changed "$tmp/ref" "? pasted over @ with the button"

click $UNDO_X $UNDO_Y; save
expect same "$tmp/ref" "the button paste is one undo"

if [ "$fail" != 0 ]; then
	echo "gui: $fail steps failed"
	exit 1
fi

echo "gui: ok"
