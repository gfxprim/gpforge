/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/*
 * Drives the GUI on an X display: clicks, wheel steps and keys.  There is no
 * xdotool here and XTest.h is not installed either, hence the two prototypes.
 *
 * Build with: make tests/xdrive
 * Use as:     DISPLAY=:99 ./tests/xdrive click 100 200
 *             DISPLAY=:99 ./tests/xdrive wheel 100 200 down
 *             DISPLAY=:99 ./tests/xdrive key Right
 *             DISPLAY=:99 ./tests/xdrive type /tmp/font.bdf
 *             DISPLAY=:99 ./tests/xdrive drag 100 200 140 200
 *             DISPLAY=:99 ./tests/xdrive ctrl s
 */

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int XTestFakeMotionEvent(Display *, int, int, int, unsigned long);
extern int XTestFakeButtonEvent(Display *, unsigned int, int, unsigned long);
extern int XTestFakeKeyEvent(Display *, unsigned int, int, unsigned long);

static const struct {
	const char *name;
	KeySym sym;
} keys[] = {
	{"Left", XK_Left},
	{"Right", XK_Right},
	{"Up", XK_Up},
	{"Down", XK_Down},
	{"PgUp", XK_Page_Up},
	{"PgDn", XK_Page_Down},
	{"Tab", XK_Tab},
	{"Enter", XK_Return},
	{"BackSpace", XK_BackSpace},
	{"Esc", XK_Escape},
	{}
};

static void press(Display *d, KeySym sym)
{
	unsigned int code = XKeysymToKeycode(d, sym);

	XTestFakeKeyEvent(d, code, True, 0);
	XFlush(d);
	usleep(30000);
	XTestFakeKeyEvent(d, code, False, 0);
	XFlush(d);
	usleep(80000);
}

/*
 * A printable ASCII character is its own keysym, which is what lets `type`
 * take a path without carrying a table of punctuation names.  The key it
 * lands on may hold it in the shifted position — '_' lives on the '-' key —
 * so the shift is held for those.
 */
static void type_char(Display *d, char c)
{
	KeySym sym = (unsigned char)c;
	unsigned int code = XKeysymToKeycode(d, sym);
	unsigned int shift = XKeysymToKeycode(d, XK_Shift_L);

	if (!code) {
		fprintf(stderr, "xdrive: cannot type '%c'\n", c);
		return;
	}

	if (XkbKeycodeToKeysym(d, code, 0, 0) == sym) {
		press(d, sym);
		return;
	}

	XTestFakeKeyEvent(d, shift, True, 0);
	XFlush(d);
	usleep(30000);

	press(d, sym);

	XTestFakeKeyEvent(d, shift, False, 0);
	XFlush(d);
	usleep(30000);
}

static void button(Display *d, int x, int y, unsigned int btn)
{
	XTestFakeMotionEvent(d, -1, x, y, 0);
	XFlush(d);
	usleep(100000);
	XTestFakeButtonEvent(d, btn, True, 0);
	XFlush(d);
	usleep(50000);
	XTestFakeButtonEvent(d, btn, False, 0);
	XFlush(d);
	usleep(200000);
}

int main(int argc, char *argv[])
{
	Display *d = XOpenDisplay(NULL);
	unsigned int i;

	if (!d || argc < 2)
		return 1;

	if (!strcmp(argv[1], "click") && argc >= 4) {
		button(d, atoi(argv[2]), atoi(argv[3]), 1);
		goto out;
	}

	if (!strcmp(argv[1], "wheel") && argc >= 5) {
		button(d, atoi(argv[2]), atoi(argv[3]),
		       strcmp(argv[4], "up") ? 5 : 4);
		goto out;
	}

	if (!strcmp(argv[1], "drag") && argc >= 6) {
		int x0 = atoi(argv[2]), y0 = atoi(argv[3]);
		int x1 = atoi(argv[4]), y1 = atoi(argv[5]);
		int steps = 8, i;

		XTestFakeMotionEvent(d, -1, x0, y0, 0);
		XFlush(d);
		usleep(100000);
		XTestFakeButtonEvent(d, 1, True, 0);
		XFlush(d);
		usleep(80000);

		for (i = 1; i <= steps; i++) {
			XTestFakeMotionEvent(d, -1,
			                     x0 + (x1 - x0) * i / steps,
			                     y0 + (y1 - y0) * i / steps, 0);
			XFlush(d);
			usleep(40000);
		}

		XTestFakeButtonEvent(d, 1, False, 0);
		XFlush(d);
		usleep(200000);

		goto out;
	}

	/*
	 * A drag one step at a time, so that what the editor looks like in the
	 * middle of a stroke can be photographed: press, move, move, release.
	 */
	if (!strcmp(argv[1], "press") && argc >= 4) {
		XTestFakeMotionEvent(d, -1, atoi(argv[2]), atoi(argv[3]), 0);
		XFlush(d);
		usleep(50000);
		XTestFakeButtonEvent(d, 1, True, 0);
		XFlush(d);
		usleep(80000);
		goto out;
	}

	if (!strcmp(argv[1], "move") && argc >= 4) {
		XTestFakeMotionEvent(d, -1, atoi(argv[2]), atoi(argv[3]), 0);
		XFlush(d);
		usleep(80000);
		goto out;
	}

	if (!strcmp(argv[1], "release")) {
		XTestFakeButtonEvent(d, 1, False, 0);
		XFlush(d);
		usleep(80000);
		goto out;
	}

	if (!strcmp(argv[1], "ctrl") && argc >= 3) {
		KeySym sym = XStringToKeysym(argv[2]);
		unsigned int ctrl = XKeysymToKeycode(d, XK_Control_L);

		if (sym == NoSymbol) {
			fprintf(stderr, "xdrive: unknown key '%s'\n", argv[2]);
			return 1;
		}

		XTestFakeKeyEvent(d, ctrl, True, 0);
		XFlush(d);
		usleep(50000);

		press(d, sym);

		XTestFakeKeyEvent(d, ctrl, False, 0);
		XFlush(d);
		usleep(100000);

		goto out;
	}

	if (!strcmp(argv[1], "type") && argc >= 3) {
		const char *str = argv[2];

		for (i = 0; str[i]; i++)
			type_char(d, str[i]);

		goto out;
	}

	if (!strcmp(argv[1], "key") && argc >= 3) {
		for (i = 0; keys[i].name; i++) {
			if (strcmp(keys[i].name, argv[2]))
				continue;

			press(d, keys[i].sym);
			goto out;
		}

		fprintf(stderr, "xdrive: unknown key '%s'\n", argv[2]);
		return 1;
	}

	fprintf(stderr, "usage: xdrive click|wheel|key ...\n");
	return 1;
out:
	XCloseDisplay(d);

	return 0;
}
