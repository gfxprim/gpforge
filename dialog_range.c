/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <gfxprim.h>

#include "gui.h"
#include "dialog_range.h"

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
"      {\"cols\": 4,\n"
"       \"widgets\": [\n"
"        {\"type\": \"label\", \"text\": \"from\"},\n"
"        {\"type\": \"tbox\", \"uid\": \"min\", \"len\": 5, \"tattr\": \"mono\",\n"
"         \"focused\": true},\n"
"        {\"type\": \"label\", \"text\": \"to\"},\n"
"        {\"type\": \"tbox\", \"uid\": \"max\", \"len\": 5, \"tattr\": \"mono\"}\n"
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
"        {\"type\": \"button\", \"halign\": \"fill\", \"label\": \"Delete\",\n"
"         \"btype\": \"rem\", \"on_event\": \"ok\"}\n"
"       ]\n"
"      }\n"
"     ]\n"
"    }\n"
"   }\n"
"  ]\n"
" }\n"
"}\n";

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

static const gp_widget_json_addr addrs[] = {
	{.id = "cancel", .on_event = cancel_on_event},
	{.id = "ok", .on_event = ok_on_event},
	{}
};

static int field(gp_htable *uids, const char *uid, uint32_t *val)
{
	gp_widget *widget = gp_widget_by_uid(uids, uid, GP_WIDGET_TBOX);
	const char *text;
	unsigned long code;
	char *end;

	if (!widget)
		return 1;

	text = gp_widget_tbox_text(widget);

	code = strtoul(text, &end, 16);

	if (end == text || *end)
		return 1;

	*val = code;

	return 0;
}

int gpf_dialog_range(const char *title, uint32_t *min, uint32_t *max)
{
	gp_dialog dialog = {};
	gp_htable *uids = NULL;
	gp_widget_json_callbacks callbacks = {
		.default_priv = &dialog,
		.addrs = addrs,
	};
	gp_widget *frame, *widget;
	int ret = 1;

	dialog.layout = gp_dialog_layout_load("gpforge_range", &callbacks,
	                                      dialog_json, &uids);
	if (!dialog.layout)
		return 1;

	frame = gp_widget_by_uid(uids, "title", GP_WIDGET_FRAME);
	if (frame)
		gp_widget_frame_title_set(frame, title);

	widget = gp_widget_by_uid(uids, "min", GP_WIDGET_TBOX);
	if (widget) {
		gp_widget_tbox_printf(widget, "%04X", *min);
		gp_widget_tbox_filter_set(widget, GP_TBOX_FILTER_HEX);
		gp_widget_tbox_clear_on_input(widget);
	}

	widget = gp_widget_by_uid(uids, "max", GP_WIDGET_TBOX);
	if (widget) {
		gp_widget_tbox_printf(widget, "%04X", *max);
		gp_widget_tbox_filter_set(widget, GP_TBOX_FILTER_HEX);
		gp_widget_tbox_clear_on_input(widget);
	}

	if (gp_dialog_run(&dialog) == GP_DIALOG_YES) {
		uint32_t from = *min, to = *max;

		if (!field(uids, "min", &from) && !field(uids, "max", &to) &&
		    from <= to) {
			*min = from;
			*max = to;
			ret = 0;
		}
	}

	gp_htable_free(uids);
	gp_widget_free(dialog.layout);

	return ret;
}
