/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <gfxprim.h>

#include "unicode_blocks.h"
#include "gui.h"
#include "dialog_block.h"

static const char *dialog_json =
"{\n"
" \"info\": {\"version\": 1, \"license\": \"GPL-2.0-or-later\"},\n"
" \"layout\": {\n"
"  \"widgets\": [\n"
"   {\n"
"    \"type\": \"frame\",\n"
"    \"uid\": \"title\",\n"
"    \"widget\": {\n"
"     \"rows\": 2,\n"
"     \"widgets\": [\n"
"      {\"cols\": 2,\n"
"       \"widgets\": [\n"
"        {\"type\": \"spinbutton\", \"desc\": \"blocks\", \"uid\": \"block\",\n"
"         \"focused\": true, \"on_event\": \"block\"},\n"
"        {\"type\": \"tbox\", \"uid\": \"start\", \"len\": 5,\n"
"         \"tattr\": \"mono\", \"on_event\": \"start\"}\n"
"       ]\n"
"      },\n"
"      {\"cols\": 2,\n"
"       \"halign\": \"fill\",\n"
"       \"cpadf\": \"1, 1, 1\",\n"
"       \"cfill\": \"0, 0\",\n"
"       \"border\": \"none\",\n"
"       \"uniform\": true,\n"
"       \"widgets\": [\n"
"        {\"type\": \"button\", \"halign\": \"fill\", \"label\": \"Cancel\",\n"
"         \"btype\": \"cancel\", \"on_event\": \"cancel\"},\n"
"        {\"type\": \"button\", \"halign\": \"fill\", \"label\": \"Add\",\n"
"         \"btype\": \"add\", \"on_event\": \"ok\"}\n"
"       ]\n"
"      }\n"
"     ]\n"
"    }\n"
"   }\n"
"  ]\n"
" }\n"
"}\n";

/* the blocks the font does not have, and which of them is selected */
static struct gpf_ucode_block *candidates;
static size_t selected;

/* the block selector and the hex field showing where it starts */
static gp_widget *block_widget;
static gp_widget *start_widget;

static void update_start(void)
{
	if (!start_widget || selected >= gp_vec_len(candidates))
		return;

	gp_widget_tbox_printf(start_widget, "%04X", candidates[selected].min);
}

/*
 * The list is long, so a block can be reached by typing where it starts
 * instead of spinning to it.
 */
static int start_on_event(gp_widget_event *ev)
{
	unsigned long code;
	size_t i;
	char *end;

	switch (ev->type) {
	case GP_WIDGET_EVENT_NEW:
		gp_widget_tbox_filter_set(ev->self, GP_TBOX_FILTER_HEX);
		gp_widget_tbox_clear_on_input(ev->self);
		return 0;
	case GP_WIDGET_EVENT_WIDGET:
		if (ev->sub_type != GP_WIDGET_TBOX_TRIGGER)
			return 0;
	break;
	default:
		return 0;
	}

	code = strtoul(gp_widget_tbox_text(ev->self), &end, 16);

	if (!*end) {
		for (i = 0; i < gp_vec_len(candidates); i++) {
			if (candidates[i].min <= code)
				selected = i;
		}
	}

	if (block_widget)
		gp_widget_redraw(block_widget);

	update_start();

	return 0;
}

static int block_on_event(gp_widget_event *ev)
{
	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	update_start();

	return 0;
}

static int ok_on_event(gp_widget_event *ev)
{
	gp_dialog *dialog = ev->self->priv;

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	dialog->retval = GP_DIALOG_YES;

	return 0;
}

static int cancel_on_event(gp_widget_event *ev)
{
	gp_dialog *dialog = ev->self->priv;

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	dialog->retval = GP_DIALOG_NO;

	return 0;
}

static const char *blocks_get_choice(gp_widget *self, size_t idx)
{
	static char buf[80];

	(void)self;

	if (idx >= gp_vec_len(candidates))
		return "";

	snprintf(buf, sizeof(buf), "%s  U+%04X-U+%04X", candidates[idx].name,
	         candidates[idx].min, candidates[idx].max);

	return buf;
}

static size_t blocks_get(gp_widget *self, enum gp_widget_choice_op op)
{
	(void)self;

	switch (op) {
	case GP_WIDGET_CHOICE_OP_SEL:
		return selected;
	case GP_WIDGET_CHOICE_OP_CNT:
		return gp_vec_len(candidates);
	}

	return 0;
}

static void blocks_set(gp_widget *self, size_t sel)
{
	(void)self;

	selected = sel;
}

static const gp_widget_choice_ops blocks_ops = {
	.get_choice = blocks_get_choice,
	.get = blocks_get,
	.set = blocks_set,
};

static const gp_widget_choice_desc blocks_desc = {
	.ops = &blocks_ops,
};

static const gp_widget_json_addr addrs[] = {
	{.id = "block", .on_event = block_on_event},
	{.id = "blocks", .addr = (void *)&blocks_desc},
	{.id = "cancel", .on_event = cancel_on_event},
	{.id = "ok", .on_event = ok_on_event},
	{.id = "start", .on_event = start_on_event},
	{}
};

/*
 * Every unicode block that is not on the font's list already.
 */
static int build_candidates(void)
{
	const struct gpf_ucode_block *have = gpf_gui_blocks();
	unsigned int i;

	gp_vec_free(candidates);

	candidates = gp_vec_new(0, sizeof(struct gpf_ucode_block));
	if (!candidates || !have)
		return 1;

	for (i = 0; gpf_ucode_blocks[i].name; i++) {
		const struct gpf_ucode_block *ub = &gpf_ucode_blocks[i];
		size_t j;
		int found = 0;

		for (j = 0; j < gp_vec_len(have); j++) {
			if (have[j].min == ub->min)
				found = 1;
		}

		if (found)
			continue;

		if (!GP_VEC_APPEND(candidates, *ub))
			return 1;
	}

	return 0;
}

const struct gpf_ucode_block *gpf_dialog_block_add(void)
{
	gp_dialog dialog = {};
	gp_htable *uids = NULL;
	gp_widget_json_callbacks callbacks = {
		.default_priv = &dialog,
		.addrs = addrs,
	};
	gp_widget *frame;
	const struct gpf_ucode_block *ret = NULL;

	selected = 0;

	if (build_candidates())
		return NULL;

	if (!gp_vec_len(candidates)) {
		gp_dialog_msg_run(GP_DIALOG_MSG_INFO, "Add block",
		                  "The font has every unicode block already.");
		return NULL;
	}

	dialog.layout = gp_dialog_layout_load("gpforge_block", &callbacks,
	                                      dialog_json, &uids);
	if (!dialog.layout)
		return NULL;

	frame = gp_widget_by_uid(uids, "title", GP_WIDGET_FRAME);
	if (frame)
		gp_widget_frame_title_set(frame, "Add block");

	block_widget = gp_widget_by_uid(uids, "block", GP_WIDGET_SPINBUTTON);
	start_widget = gp_widget_by_uid(uids, "start", GP_WIDGET_TBOX);

	gp_htable_free(uids);

	update_start();

	if (gp_dialog_run(&dialog) == GP_DIALOG_YES &&
	    selected < gp_vec_len(candidates))
		ret = &candidates[selected];

	gp_widget_free(dialog.layout);

	block_widget = NULL;
	start_widget = NULL;

	return ret;
}
