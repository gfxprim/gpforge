/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <gfxprim.h>

#include "gui.h"
#include "dialog_save.h"

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
"      {\"type\": \"label\", \"uid\": \"msg\", \"text\": \"\"},\n"
"      {\"cols\": 3,\n"
"       \"halign\": \"fill\",\n"
"       \"cpadf\": \"1, 1, 1, 1\",\n"
"       \"cfill\": \"0, 0, 0\",\n"
"       \"border\": \"none\",\n"
"       \"uniform\": true,\n"
"       \"widgets\": [\n"
"        {\"type\": \"button\", \"halign\": \"fill\", \"label\": \"Cancel\",\n"
"         \"btype\": \"cancel\", \"on_event\": \"cancel\"},\n"
"        {\"type\": \"button\", \"halign\": \"fill\", \"label\": \"Discard\",\n"
"         \"btype\": \"rem\", \"on_event\": \"discard\"},\n"
"        {\"type\": \"button\", \"halign\": \"fill\", \"label\": \"Save\",\n"
"         \"btype\": \"save\", \"on_event\": \"save\", \"focused\": true}\n"
"       ]\n"
"      }\n"
"     ]\n"
"    }\n"
"   }\n"
"  ]\n"
" }\n"
"}\n";

static int answer(gp_widget_event *ev, enum gpf_save_answer val)
{
	gp_dialog *dialog = ev->self->priv;

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	/* zero is "still running" to gp_dialog_run(), so no answer is zero */
	dialog->retval = val;

	return 0;
}

static int save_on_event(gp_widget_event *ev)
{
	return answer(ev, GPF_SAVE_SAVE);
}

static int discard_on_event(gp_widget_event *ev)
{
	return answer(ev, GPF_SAVE_DISCARD);
}

static int cancel_on_event(gp_widget_event *ev)
{
	return answer(ev, GPF_SAVE_CANCEL);
}

static const gp_widget_json_addr addrs[] = {
	{.id = "cancel", .on_event = cancel_on_event},
	{.id = "discard", .on_event = discard_on_event},
	{.id = "save", .on_event = save_on_event},
	{}
};

enum gpf_save_answer gpf_dialog_save(const char *title)
{
	gp_dialog dialog = {};
	gp_htable *uids = NULL;
	gp_widget_json_callbacks callbacks = {
		.default_priv = &dialog,
		.addrs = addrs,
	};
	gp_widget *widget;
	long ret;

	dialog.layout = gp_dialog_layout_load("gpforge_save", &callbacks,
	                                      dialog_json, &uids);
	if (!dialog.layout)
		return GPF_SAVE_CANCEL;

	widget = gp_widget_by_uid(uids, "title", GP_WIDGET_FRAME);
	if (widget)
		gp_widget_frame_title_set(widget, title);

	widget = gp_widget_by_uid(uids, "msg", GP_WIDGET_LABEL);
	if (widget)
		gp_widget_label_set(widget, "The font has unsaved changes.");

	ret = gp_dialog_run(&dialog);

	gp_htable_free(uids);
	gp_widget_free(dialog.layout);

	if (ret != GPF_SAVE_SAVE && ret != GPF_SAVE_DISCARD)
		return GPF_SAVE_CANCEL;

	return ret;
}
