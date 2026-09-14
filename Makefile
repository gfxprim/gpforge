# SPDX-License-Identifier: GPL-2.0-or-later
# a packager's CFLAGS replace the optimisation, the warnings stay
CFLAGS ?= -O2 -g
CFLAGS += -W -Wall -Wextra -std=gnu11

CFLAGS += -MMD -MP

CFLAGS += $(shell pkg-config --cflags gfxprim gfxprim-widgets)
LDLIBS = $(shell pkg-config --libs gfxprim)
LDLIBS_GUI = $(shell pkg-config --libs gfxprim-widgets)

OBJS=font.o format.o format_bdf.o embolden.o compose.o resolve.o face.o edit.o undo.o lint.o format_c.o

BINS=gpforge-cli gpforge

all: $(BINS)

gpforge-cli: gpforge-cli.o $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

GUI_OBJS=gpforge.o canvas.o browser.o preview.o cell.o unicode_blocks.o dialog_block.o dialog_family.o dialog_range.o dialog_save.o

gpforge: $(GUI_OBJS) $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS_GUI) $(LDLIBS)

tests/unit: tests/unit.o $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

tests/resolve: tests/resolve.o $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

tests/edit: tests/edit.o $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

tests/lint: tests/lint.o $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

tests/export_cmp: tests/export_cmp.o $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS) -ldl

tests/xdrive: tests/xdrive.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lX11 -l:libXtst.so.6

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -I. -c $< -o $@

doc:
	doxygen

FONTS?=../fonts

test: $(BINS) tests/unit tests/resolve tests/edit tests/lint tests/export_cmp
	./tests/unit
	./tests/resolve
	./tests/edit
	./tests/lint
	FONTS=$(FONTS) ./tests/roundtrip.sh
	FONTS=$(FONTS) ./tests/bdf.sh
	FONTS=$(FONTS) ./tests/gui.sh
	FONTS=$(FONTS) ./tests/export.sh

lint: gpforge-cli
	@for f in $(FONTS)/*.bdf; do \
		printf '%-28s ' "$$(basename $$f)"; \
		./gpforge-cli lint "$$f" | tail -1; \
	done

-include $(OBJS:.o=.d) $(GUI_OBJS:.o=.d) gpforge-cli.d tests/unit.d tests/resolve.d tests/edit.d tests/lint.d tests/export_cmp.d

install: $(BINS)
	install -d $(DESTDIR)/etc/gp_apps/gpforge/
	install -m 644 layout.json -t $(DESTDIR)/etc/gp_apps/gpforge/
	install -d $(DESTDIR)/usr/bin/
	install $(BINS) -t $(DESTDIR)/usr/bin/
	install -d $(DESTDIR)/usr/share/applications/
	install -m 644 gpforge.desktop -t $(DESTDIR)/usr/share/applications/

clean:
	rm -f *.o *.d tests/*.o tests/*.d $(BINS) tests/unit tests/resolve tests/edit tests/lint tests/export_cmp tests/xdrive

.PHONY: all test lint install clean
