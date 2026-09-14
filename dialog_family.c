/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */

#include <stddef.h>

#include <gfxprim.h>

#include "gui.h"
#include "dialog_family.h"

static const char *dialog_json =
"{\n"
" \"info\": {\"version\": 1, \"license\": \"GPL-2.0-or-later\"},\n"
" \"layout\": {\n"
"  \"widgets\": [\n"
"   {\n"
"    \"type\": \"frame\",\n"
"    \"uid\": \"title\",\n"
"    \"widget\": {\n"
"     \"rows\": 6,\n"
"     \"widgets\": [\n"
/* the grids fill column by column, so the labels come first */
"      {\"cols\": 2, \"rows\": 3, \"uniform\": false, \"border\": \"none\",\n"
"       \"widgets\": [\n"
"        {\"type\": \"label\", \"text\": \"family\", \"halign\": \"right\"},\n"
"        {\"type\": \"label\", \"text\": \"license\", \"halign\": \"right\"},\n"
"        {\"type\": \"label\", \"text\": \"default glyph U+\",\n"
"         \"halign\": \"right\"},\n"
"        {\"type\": \"tbox\", \"uid\": \"family\", \"len\": 24,\n"
"         \"halign\": \"fill\", \"focused\": true, \"on_event\": \"check\"},\n"
"        {\"type\": \"tbox\", \"uid\": \"license\", \"len\": 24,\n"
"         \"halign\": \"fill\"},\n"
"        {\"type\": \"tbox\", \"uid\": \"default_glyph\", \"len\": 6,\n"
"         \"tattr\": \"mono\", \"halign\": \"left\"}\n"
"       ]\n"
"      },\n"
"      {\"cols\": 2, \"uniform\": true, \"border\": \"none\",\n"
"       \"align\": \"fill\",\n"
"       \"widgets\": [\n"
"        {\"type\": \"frame\", \"title\": \"Line box\", \"align\": \"fill\",\n"
"         \"widget\": {\"cols\": 2, \"rows\": 5, \"border\": \"none\",\n"
"          \"widgets\": [\n"
"           {\"type\": \"label\", \"text\": \"size\", \"halign\": \"right\"},\n"
"           {\"type\": \"label\", \"text\": \"ascent\", \"halign\": \"right\"},\n"
"           {\"type\": \"label\", \"text\": \"descent\", \"halign\": \"right\"},\n"
"           {\"type\": \"label\", \"text\": \"line gap\", \"halign\": \"right\"},\n"
"           {\"type\": \"label\", \"text\": \"mono advance\", \"halign\": \"right\"},\n"
"           {\"type\": \"spinner\", \"uid\": \"size\", \"on_event\": \"check\"},\n"
"           {\"type\": \"spinner\", \"uid\": \"ascent\", \"on_event\": \"check\"},\n"
"           {\"type\": \"spinner\", \"uid\": \"descent\", \"on_event\": \"check\"},\n"
"           {\"type\": \"spinner\", \"uid\": \"line_gap\"},\n"
"           {\"type\": \"spinner\", \"uid\": \"advance\"}\n"
"          ]\n"
"         }\n"
"        },\n"
"        {\"type\": \"frame\", \"title\": \"Letters\", \"align\": \"fill\",\n"
"         \"widget\": {\"cols\": 3, \"rows\": 4, \"border\": \"none\",\n"
"          \"widgets\": [\n"
"           {\"type\": \"label\", \"text\": \"em\", \"halign\": \"right\"},\n"
"           {\"type\": \"label\", \"text\": \"x-height\", \"halign\": \"right\"},\n"
"           {\"type\": \"label\", \"text\": \"cap height\", \"halign\": \"right\"},\n"
"           {\"type\": \"label\", \"text\": \"digit width\", \"halign\": \"right\"},\n"
"           {\"type\": \"spinner\", \"uid\": \"em\"},\n"
"           {\"type\": \"spinner\", \"uid\": \"x_height\", \"on_event\": \"check\"},\n"
"           {\"type\": \"spinner\", \"uid\": \"cap_height\", \"on_event\": \"check\"},\n"
"           {\"type\": \"spinner\", \"uid\": \"ch_width\"},\n"
"           {\"type\": \"label\", \"text\": \"\"},\n"
"           {\"type\": \"button\", \"label\": \"measure\", \"uid\": \"x_height_m\",\n"
"            \"on_event\": \"measure_x_height\"},\n"
"           {\"type\": \"button\", \"label\": \"measure\", \"uid\": \"cap_height_m\",\n"
"            \"on_event\": \"measure_cap_height\"},\n"
"           {\"type\": \"button\", \"label\": \"measure\", \"uid\": \"ch_width_m\",\n"
"            \"on_event\": \"measure_ch_width\"}\n"
"          ]\n"
"         }\n"
"        }\n"
"       ]\n"
"      },\n"
"      {\"type\": \"frame\", \"title\": \"Rules\", \"halign\": \"fill\",\n"
"       \"widget\": {\"cols\": 3, \"rows\": 4, \"border\": \"none\",\n"
"        \"widgets\": [\n"
"         {\"type\": \"label\", \"text\": \"\"},\n"
"         {\"type\": \"label\", \"text\": \"underline\", \"halign\": \"right\"},\n"
"         {\"type\": \"label\", \"text\": \"strikethrough\", \"halign\": \"right\"},\n"
"         {\"type\": \"label\", \"text\": \"overline\", \"halign\": \"right\"},\n"
"         {\"type\": \"label\", \"text\": \"above baseline\"},\n"
"         {\"type\": \"spinner\", \"uid\": \"underline_pos\"},\n"
"         {\"type\": \"spinner\", \"uid\": \"strike_pos\"},\n"
"         {\"type\": \"spinner\", \"uid\": \"overline_pos\"},\n"
"         {\"type\": \"label\", \"text\": \"thickness\"},\n"
"         {\"type\": \"spinner\", \"uid\": \"underline_thickness\"},\n"
"         {\"type\": \"spinner\", \"uid\": \"strike_thickness\"},\n"
"         {\"type\": \"spinner\", \"uid\": \"overline_thickness\"}\n"
"        ]\n"
"       }\n"
"      },\n"
"      {\"type\": \"frame\", \"title\": \"Authors\", \"halign\": \"fill\",\n"
"       \"widget\": {\"rows\": 2, \"border\": \"none\", \"halign\": \"fill\",\n"
"        \"widgets\": [\n"
"         {\"type\": \"table\", \"uid\": \"authors\", \"min_rows\": 3,\n"
"          \"halign\": \"fill\", \"col_ops\": \"authors_ops\",\n"
"          \"header\": [\n"
"           {\"label\": \"years\", \"id\": \"years\", \"min_size\": 9},\n"
"           {\"label\": \"name\", \"id\": \"name\", \"min_size\": 16,\n"
"            \"fill\": 1},\n"
"           {\"label\": \"email\", \"id\": \"email\", \"min_size\": 16,\n"
"            \"fill\": 1}\n"
"          ]\n"
"         },\n"
"         {\"cols\": 5, \"border\": \"none\", \"halign\": \"fill\",\n"
"          \"cfill\": \"0, 1, 1, 0, 0\",\n"
"          \"widgets\": [\n"
"           {\"type\": \"tbox\", \"uid\": \"author_years\", \"len\": 9,\n"
"            \"max_len\": 31, \"help\": \"2024\"},\n"
"           {\"type\": \"tbox\", \"uid\": \"author_name\", \"len\": 12,\n"
"            \"max_len\": 63, \"help\": \"name\", \"halign\": \"fill\"},\n"
"           {\"type\": \"tbox\", \"uid\": \"author_email\", \"len\": 12,\n"
"            \"max_len\": 63, \"help\": \"email\", \"halign\": \"fill\"},\n"
"           {\"type\": \"button\", \"label\": \"add\", \"uid\": \"author_add\",\n"
"            \"on_event\": \"author_add\"},\n"
"           {\"type\": \"button\", \"label\": \"remove\",\n"
"            \"uid\": \"author_del\", \"on_event\": \"author_del\"}\n"
"          ]\n"
"         }\n"
"        ]\n"
"       }\n"
"      },\n"
"      {\"type\": \"label\", \"uid\": \"note\", \"text\": \"\", \"width\": 40,\n"
"       \"halign\": \"fill\"},\n"
"      {\"cols\": 2,\n"
"       \"halign\": \"fill\",\n"
"       \"cpadf\": \"1, 1, 1\",\n"
"       \"cfill\": \"0, 0\",\n"
"       \"border\": \"none\",\n"
"       \"uniform\": true,\n"
"       \"widgets\": [\n"
"        {\"type\": \"button\", \"halign\": \"fill\", \"label\": \"Cancel\",\n"
"         \"btype\": \"cancel\", \"on_event\": \"cancel\"},\n"
"        {\"type\": \"button\", \"halign\": \"fill\", \"label\": \"OK\",\n"
"         \"btype\": \"ok\", \"uid\": \"ok\", \"on_event\": \"ok\"}\n"
"       ]\n"
"      }\n"
"     ]\n"
"    }\n"
"   }\n"
"  ]\n"
" }\n"
"}\n";

/*
 * The numbers, by the uid of the spinner that edits them.  A zero thickness is
 * "the font does not say", which the export leaves out.
 */
static const struct field {
	const char *uid;
	size_t off;
	int min, max;
} fields[] = {
#define FIELD(name, min, max) \
	{#name, offsetof(struct gpf_family_meta, name), min, max}
	FIELD(size, 1, 255),
	FIELD(ascent, 1, 255),
	FIELD(descent, 0, 255),
	FIELD(line_gap, 0, 255),
	FIELD(em, 1, 255),
	FIELD(x_height, 0, 255),
	FIELD(cap_height, 0, 255),
	FIELD(ch_width, 0, 255),
	FIELD(underline_pos, -255, 255),
	FIELD(underline_thickness, 0, 255),
	FIELD(strike_pos, -255, 255),
	FIELD(strike_thickness, 0, 255),
	FIELD(overline_pos, -255, 255),
	FIELD(overline_thickness, 0, 255),
#undef FIELD
};

#define FIELDS (sizeof(fields) / sizeof(fields[0]))

static int field_get(const struct gpf_family_meta *meta,
                     const struct field *field)
{
	return *(const int *)((const char *)meta + field->off);
}

static void field_set(struct gpf_family_meta *meta, const struct field *field,
                      int val)
{
	*(int *)((char *)meta + field->off) = val;
}

static gp_htable *uids;

/* the caller's copy of the authors, which the table shows and the buttons edit */
static struct gpf_author **authors;

/* what add or remove last had to say, shown until the next add or remove */
static const char *author_msg;

/*
 * A font is being created rather than edited.  Whatever is open is not the
 * font this is about then, so there is nothing to measure.
 */
static int creating;

static gp_widget *spinner(const char *uid)
{
	return gp_widget_by_uid(uids, uid, GP_WIDGET_SPINNER);
}

static int spinner_val(const char *uid)
{
	gp_widget *widget = spinner(uid);

	return widget ? gp_widget_int_val_get(widget) : 0;
}

static const char *tbox_text(const char *uid)
{
	gp_widget *widget = gp_widget_by_uid(uids, uid, GP_WIDGET_TBOX);

	return widget ? gp_widget_tbox_text(widget) : "";
}

/*
 * What is wrong with the numbers, if anything.  Only a nameless family is
 * refused; the rest is said and left to the user, since a font that breaks a
 * rule on purpose is still a font.
 */
static void check(void)
{
	gp_widget *note = gp_widget_by_uid(uids, "note", GP_WIDGET_LABEL);
	gp_widget *ok = gp_widget_by_uid(uids, "ok", GP_WIDGET_BUTTON);
	int ascent = spinner_val("ascent");
	int size = spinner_val("size");
	const char *msg = "";

	if (size < ascent || size > ascent + spinner_val("descent"))
		msg = "the size is not within the line box";

	if (spinner_val("x_height") > ascent)
		msg = "the x-height is above the ascent";

	if (spinner_val("cap_height") > ascent)
		msg = "the cap height is above the ascent";

	if (author_msg)
		msg = author_msg;

	if (!tbox_text("family")[0])
		msg = "the family needs a name";

	if (note)
		gp_widget_label_set(note, msg);

	if (ok)
		gp_widget_disabled_set(ok, !tbox_text("family")[0]);
}

static int check_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		check();

	return 0;
}

static void measure(enum gpf_measure what, const char *uid)
{
	gp_widget *widget = spinner(uid);
	int val;

	if (!widget || creating || gpf_measure(gui.font, gui.variant, what, &val))
		return;

	gp_widget_int_val_set(widget, val);

	check();
}

static int measure_x_height_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		measure(GPF_MEASURE_X_HEIGHT, "x_height");

	return 0;
}

static int measure_cap_height_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		measure(GPF_MEASURE_CAP_HEIGHT, "cap_height");

	return 0;
}

static int measure_ch_width_on_event(gp_widget_event *ev)
{
	if (ev->type == GP_WIDGET_EVENT_WIDGET)
		measure(GPF_MEASURE_CH_WIDTH, "ch_width");

	return 0;
}

enum authors_col {
	AUTHORS_YEARS,
	AUTHORS_NAME,
	AUTHORS_EMAIL,
};

static int authors_seek_row(gp_widget *self, int op, unsigned int pos)
{
	gp_widget_table_priv *priv = gp_widget_table_priv_get(self);
	size_t len = gp_vec_len(*authors);

	switch (op) {
	case GP_TABLE_ROW_RESET:
		priv->row_idx = 0;
	break;
	case GP_TABLE_ROW_ADVANCE:
		priv->row_idx += pos;
	break;
	case GP_TABLE_ROW_MAX:
		return len;
	}

	return priv->row_idx < len;
}

static int authors_get_cell(gp_widget *self, gp_widget_table_cell *cell,
                            unsigned int col)
{
	gp_widget_table_priv *priv = gp_widget_table_priv_get(self);
	const struct gpf_author *author = &(*authors)[priv->row_idx];

	switch (col) {
	case AUTHORS_YEARS:
		cell->text = author->years;
	break;
	case AUTHORS_NAME:
		cell->text = author->name;
	break;
	case AUTHORS_EMAIL:
		cell->text = author->email;
	break;
	}

	return 1;
}

static void tbox_set(const char *uid, const char *text)
{
	gp_widget *widget = gp_widget_by_uid(uids, uid, GP_WIDGET_TBOX);

	if (widget)
		gp_widget_tbox_set(widget, text);
}

/* the author selected in the table, filled in the fields to be corrected */
static int authors_on_event(gp_widget_event *ev)
{
	const struct gpf_author *author;
	unsigned int row;

	if (ev->type != GP_WIDGET_EVENT_WIDGET ||
	    !gp_widget_table_sel_has(ev->self))
		return 0;

	row = gp_widget_table_sel_get(ev->self);
	if (row >= gp_vec_len(*authors))
		return 0;

	author = &(*authors)[row];

	tbox_set("author_years", author->years);
	tbox_set("author_name", author->name);
	tbox_set("author_email", author->email);

	return 0;
}

static const gp_widget_table_col_ops authors_ops = {
	.seek_row = authors_seek_row,
	.get_cell = authors_get_cell,
	.on_event = authors_on_event,
	.col_map = {
		{.id = "years", .idx = AUTHORS_YEARS},
		{.id = "name", .idx = AUTHORS_NAME},
		{.id = "email", .idx = AUTHORS_EMAIL},
		{}
	}
};

static gp_widget *authors_table(void)
{
	return gp_widget_by_uid(uids, "authors", GP_WIDGET_TABLE);
}

static int author_add_on_event(gp_widget_event *ev)
{
	const char *years = tbox_text("author_years");
	const char *name = tbox_text("author_name");
	const char *email = tbox_text("author_email");
	gp_widget *table = authors_table();

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	author_msg = gpf_author_invalid(years, name, email);

	if (!author_msg && gpf_authors_add(authors, years, name, email))
		author_msg = "out of memory";

	if (!author_msg) {
		tbox_set("author_years", "");
		tbox_set("author_name", "");
		tbox_set("author_email", "");
	}

	if (table)
		gp_widget_table_refresh(table);

	check();

	return 0;
}

static int author_del_on_event(gp_widget_event *ev)
{
	gp_widget *table = authors_table();

	if (ev->type != GP_WIDGET_EVENT_WIDGET || !table)
		return 0;

	author_msg = NULL;

	if (!gp_widget_table_sel_has(table))
		author_msg = "select an author to remove";
	else
		gpf_authors_del(authors, gp_widget_table_sel_get(table));

	gp_widget_table_refresh(table);

	check();

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

static const gp_widget_json_addr addrs[] = {
	{.id = "author_add", .on_event = author_add_on_event},
	{.id = "author_del", .on_event = author_del_on_event},
	{.id = "authors_ops", .addr = (void *)&authors_ops},
	{.id = "cancel", .on_event = cancel_on_event},
	{.id = "check", .on_event = check_on_event},
	{.id = "measure_cap_height", .on_event = measure_cap_height_on_event},
	{.id = "measure_ch_width", .on_event = measure_ch_width_on_event},
	{.id = "measure_x_height", .on_event = measure_x_height_on_event},
	{.id = "ok", .on_event = ok_on_event},
	{}
};

/*
 * A measure button the variant cannot answer is greyed out, rather than
 * pressed to no effect.
 */
static void measure_enable(const char *uid, enum gpf_measure what)
{
	gp_widget *widget = gp_widget_by_uid(uids, uid, GP_WIDGET_BUTTON);
	int val, none;

	if (!widget)
		return;

	none = creating || gpf_measure(gui.font, gui.variant, what, &val);

	gp_widget_disabled_set(widget, none);
}

/*
 * The mono cell, which is asked for when a font is created and only shown when
 * one is edited.
 */
static void load_advance(const int *advance)
{
	gp_widget *widget = spinner("advance");
	struct gpf_variant *mono;

	if (!widget)
		return;

	if (advance) {
		gp_widget_int_set(widget, 1, 255, *advance);
		return;
	}

	mono = gui.font ? gui.font->variants[GPF_MONO] : NULL;

	gp_widget_int_set(widget, 0, 255, mono ? mono->advance : 0);
	gp_widget_disabled_set(widget, 1);
}

static void load(const struct gpf_family_meta *meta, const int *advance)
{
	gp_widget *widget;
	size_t i;

	for (i = 0; i < FIELDS; i++) {
		widget = spinner(fields[i].uid);
		if (widget) {
			gp_widget_int_set(widget, fields[i].min, fields[i].max,
			                  field_get(meta, &fields[i]));
		}
	}

	widget = gp_widget_by_uid(uids, "family", GP_WIDGET_TBOX);
	if (widget)
		gp_widget_tbox_set(widget, meta->family);

	widget = gp_widget_by_uid(uids, "license", GP_WIDGET_TBOX);
	if (widget)
		gp_widget_tbox_set(widget, meta->license);

	widget = gp_widget_by_uid(uids, "default_glyph", GP_WIDGET_TBOX);
	if (widget) {
		if (meta->default_glyph) {
			gp_widget_tbox_printf(widget, "%04X",
			                      meta->default_glyph);
		}
		gp_widget_tbox_filter_set(widget, GP_TBOX_FILTER_HEX);
	}

	measure_enable("x_height_m", GPF_MEASURE_X_HEIGHT);
	measure_enable("cap_height_m", GPF_MEASURE_CAP_HEIGHT);
	measure_enable("ch_width_m", GPF_MEASURE_CH_WIDTH);

	load_advance(advance);
}

static void store(struct gpf_family_meta *meta, int *advance)
{
	size_t i;

	for (i = 0; i < FIELDS; i++) {
		if (spinner(fields[i].uid))
			field_set(meta, &fields[i], spinner_val(fields[i].uid));
	}

	snprintf(meta->family, sizeof(meta->family), "%s", tbox_text("family"));
	snprintf(meta->license, sizeof(meta->license), "%s",
	         tbox_text("license"));

	/* an empty field is no default glyph, which the writer leaves out */
	meta->default_glyph = strtoul(tbox_text("default_glyph"), NULL, 16);

	if (advance && spinner("advance"))
		*advance = spinner_val("advance");
}

int gpf_dialog_family(struct gpf_family_meta *meta,
                      struct gpf_author **edit_authors, int *advance)
{
	gp_dialog dialog = {};
	gp_widget_json_callbacks callbacks = {
		.default_priv = &dialog,
		.addrs = addrs,
	};
	gp_widget *frame;
	int ret = 1;

	/* editing needs a font to edit */
	if (!advance && !gui.font)
		return 1;

	creating = !!advance;
	authors = edit_authors;
	author_msg = NULL;

	dialog.layout = gp_dialog_layout_load("gpforge_family", &callbacks,
	                                      dialog_json, &uids);
	if (!dialog.layout)
		return 1;

	frame = gp_widget_by_uid(uids, "title", GP_WIDGET_FRAME);
	if (frame)
		gp_widget_frame_title_set(frame, creating ? "New font" :
		                                            "Font properties");

	load(meta, advance);
	check();

	if (gp_dialog_run(&dialog) == GP_DIALOG_YES) {
		store(meta, advance);
		ret = 0;
	}

	gp_htable_free(uids);
	uids = NULL;
	gp_widget_free(dialog.layout);
	authors = NULL;

	return ret;
}
