/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 Cyril Hrubis <metan@ucw.cz>
 */
/*
 * The block table is generated from the Unicode Blocks.txt, do not edit it.
 *
 *   sed/awk equivalent: Start..End; Name, the BMP only, coverage 0.
 *
 * The preview samples and the functions below it are hand written.
 */

#include <string.h>

#include <utils/gp_vec.h>

#include "unicode_blocks.h"

const struct gpf_ucode_block gpf_ucode_blocks[] = {
	{0x0000, 0x007f, "Basic Latin", 0},
	{0x0080, 0x00ff, "Latin-1 Supplement", 0},
	{0x0100, 0x017f, "Latin Extended-A", 0},
	{0x0180, 0x024f, "Latin Extended-B", 0},
	{0x0250, 0x02af, "IPA Extensions", 0},
	{0x02b0, 0x02ff, "Spacing Modifier Letters", 0},
	{0x0300, 0x036f, "Combining Diacritical Marks", 0},
	{0x0370, 0x03ff, "Greek and Coptic", 0},
	{0x0400, 0x04ff, "Cyrillic", 0},
	{0x0500, 0x052f, "Cyrillic Supplement", 0},
	{0x0530, 0x058f, "Armenian", 0},
	{0x0590, 0x05ff, "Hebrew", 0},
	{0x0600, 0x06ff, "Arabic", 0},
	{0x0700, 0x074f, "Syriac", 0},
	{0x0750, 0x077f, "Arabic Supplement", 0},
	{0x0780, 0x07bf, "Thaana", 0},
	{0x07c0, 0x07ff, "NKo", 0},
	{0x0800, 0x083f, "Samaritan", 0},
	{0x0840, 0x085f, "Mandaic", 0},
	{0x0860, 0x086f, "Syriac Supplement", 0},
	{0x0870, 0x089f, "Arabic Extended-B", 0},
	{0x08a0, 0x08ff, "Arabic Extended-A", 0},
	{0x0900, 0x097f, "Devanagari", 0},
	{0x0980, 0x09ff, "Bengali", 0},
	{0x0a00, 0x0a7f, "Gurmukhi", 0},
	{0x0a80, 0x0aff, "Gujarati", 0},
	{0x0b00, 0x0b7f, "Oriya", 0},
	{0x0b80, 0x0bff, "Tamil", 0},
	{0x0c00, 0x0c7f, "Telugu", 0},
	{0x0c80, 0x0cff, "Kannada", 0},
	{0x0d00, 0x0d7f, "Malayalam", 0},
	{0x0d80, 0x0dff, "Sinhala", 0},
	{0x0e00, 0x0e7f, "Thai", 0},
	{0x0e80, 0x0eff, "Lao", 0},
	{0x0f00, 0x0fff, "Tibetan", 0},
	{0x1000, 0x109f, "Myanmar", 0},
	{0x10a0, 0x10ff, "Georgian", 0},
	{0x1100, 0x11ff, "Hangul Jamo", 0},
	{0x1200, 0x137f, "Ethiopic", 0},
	{0x1380, 0x139f, "Ethiopic Supplement", 0},
	{0x13a0, 0x13ff, "Cherokee", 0},
	{0x1400, 0x167f, "Unified Canadian Aboriginal Syllabics", 0},
	{0x1680, 0x169f, "Ogham", 0},
	{0x16a0, 0x16ff, "Runic", 0},
	{0x1700, 0x171f, "Tagalog", 0},
	{0x1720, 0x173f, "Hanunoo", 0},
	{0x1740, 0x175f, "Buhid", 0},
	{0x1760, 0x177f, "Tagbanwa", 0},
	{0x1780, 0x17ff, "Khmer", 0},
	{0x1800, 0x18af, "Mongolian", 0},
	{0x18b0, 0x18ff, "Unified Canadian Aboriginal Syllabics Extended", 0},
	{0x1900, 0x194f, "Limbu", 0},
	{0x1950, 0x197f, "Tai Le", 0},
	{0x1980, 0x19df, "New Tai Lue", 0},
	{0x19e0, 0x19ff, "Khmer Symbols", 0},
	{0x1a00, 0x1a1f, "Buginese", 0},
	{0x1a20, 0x1aaf, "Tai Tham", 0},
	{0x1ab0, 0x1aff, "Combining Diacritical Marks Extended", 0},
	{0x1b00, 0x1b7f, "Balinese", 0},
	{0x1b80, 0x1bbf, "Sundanese", 0},
	{0x1bc0, 0x1bff, "Batak", 0},
	{0x1c00, 0x1c4f, "Lepcha", 0},
	{0x1c50, 0x1c7f, "Ol Chiki", 0},
	{0x1c80, 0x1c8f, "Cyrillic Extended-C", 0},
	{0x1c90, 0x1cbf, "Georgian Extended", 0},
	{0x1cc0, 0x1ccf, "Sundanese Supplement", 0},
	{0x1cd0, 0x1cff, "Vedic Extensions", 0},
	{0x1d00, 0x1d7f, "Phonetic Extensions", 0},
	{0x1d80, 0x1dbf, "Phonetic Extensions Supplement", 0},
	{0x1dc0, 0x1dff, "Combining Diacritical Marks Supplement", 0},
	{0x1e00, 0x1eff, "Latin Extended Additional", 0},
	{0x1f00, 0x1fff, "Greek Extended", 0},
	{0x2000, 0x206f, "General Punctuation", 0},
	{0x2070, 0x209f, "Superscripts and Subscripts", 0},
	{0x20a0, 0x20cf, "Currency Symbols", 0},
	{0x20d0, 0x20ff, "Combining Diacritical Marks for Symbols", 0},
	{0x2100, 0x214f, "Letterlike Symbols", 0},
	{0x2150, 0x218f, "Number Forms", 0},
	{0x2190, 0x21ff, "Arrows", 0},
	{0x2200, 0x22ff, "Mathematical Operators", 0},
	{0x2300, 0x23ff, "Miscellaneous Technical", 0},
	{0x2400, 0x243f, "Control Pictures", 0},
	{0x2440, 0x245f, "Optical Character Recognition", 0},
	{0x2460, 0x24ff, "Enclosed Alphanumerics", 0},
	{0x2500, 0x257f, "Box Drawing", 0},
	{0x2580, 0x259f, "Block Elements", 0},
	{0x25a0, 0x25ff, "Geometric Shapes", 0},
	{0x2600, 0x26ff, "Miscellaneous Symbols", 0},
	{0x2700, 0x27bf, "Dingbats", 0},
	{0x27c0, 0x27ef, "Miscellaneous Mathematical Symbols-A", 0},
	{0x27f0, 0x27ff, "Supplemental Arrows-A", 0},
	{0x2800, 0x28ff, "Braille Patterns", 0},
	{0x2900, 0x297f, "Supplemental Arrows-B", 0},
	{0x2980, 0x29ff, "Miscellaneous Mathematical Symbols-B", 0},
	{0x2a00, 0x2aff, "Supplemental Mathematical Operators", 0},
	{0x2b00, 0x2bff, "Miscellaneous Symbols and Arrows", 0},
	{0x2c00, 0x2c5f, "Glagolitic", 0},
	{0x2c60, 0x2c7f, "Latin Extended-C", 0},
	{0x2c80, 0x2cff, "Coptic", 0},
	{0x2d00, 0x2d2f, "Georgian Supplement", 0},
	{0x2d30, 0x2d7f, "Tifinagh", 0},
	{0x2d80, 0x2ddf, "Ethiopic Extended", 0},
	{0x2de0, 0x2dff, "Cyrillic Extended-A", 0},
	{0x2e00, 0x2e7f, "Supplemental Punctuation", 0},
	{0x2e80, 0x2eff, "CJK Radicals Supplement", 0},
	{0x2f00, 0x2fdf, "Kangxi Radicals", 0},
	{0x2ff0, 0x2fff, "Ideographic Description Characters", 0},
	{0x3000, 0x303f, "CJK Symbols and Punctuation", 0},
	{0x3040, 0x309f, "Hiragana", 0},
	{0x30a0, 0x30ff, "Katakana", 0},
	{0x3100, 0x312f, "Bopomofo", 0},
	{0x3130, 0x318f, "Hangul Compatibility Jamo", 0},
	{0x3190, 0x319f, "Kanbun", 0},
	{0x31a0, 0x31bf, "Bopomofo Extended", 0},
	{0x31c0, 0x31ef, "CJK Strokes", 0},
	{0x31f0, 0x31ff, "Katakana Phonetic Extensions", 0},
	{0x3200, 0x32ff, "Enclosed CJK Letters and Months", 0},
	{0x3300, 0x33ff, "CJK Compatibility", 0},
	{0x3400, 0x4dbf, "CJK Unified Ideographs Extension A", 0},
	{0x4dc0, 0x4dff, "Yijing Hexagram Symbols", 0},
	{0x4e00, 0x9fff, "CJK Unified Ideographs", 0},
	{0xa000, 0xa48f, "Yi Syllables", 0},
	{0xa490, 0xa4cf, "Yi Radicals", 0},
	{0xa4d0, 0xa4ff, "Lisu", 0},
	{0xa500, 0xa63f, "Vai", 0},
	{0xa640, 0xa69f, "Cyrillic Extended-B", 0},
	{0xa6a0, 0xa6ff, "Bamum", 0},
	{0xa700, 0xa71f, "Modifier Tone Letters", 0},
	{0xa720, 0xa7ff, "Latin Extended-D", 0},
	{0xa800, 0xa82f, "Syloti Nagri", 0},
	{0xa830, 0xa83f, "Common Indic Number Forms", 0},
	{0xa840, 0xa87f, "Phags-pa", 0},
	{0xa880, 0xa8df, "Saurashtra", 0},
	{0xa8e0, 0xa8ff, "Devanagari Extended", 0},
	{0xa900, 0xa92f, "Kayah Li", 0},
	{0xa930, 0xa95f, "Rejang", 0},
	{0xa960, 0xa97f, "Hangul Jamo Extended-A", 0},
	{0xa980, 0xa9df, "Javanese", 0},
	{0xa9e0, 0xa9ff, "Myanmar Extended-B", 0},
	{0xaa00, 0xaa5f, "Cham", 0},
	{0xaa60, 0xaa7f, "Myanmar Extended-A", 0},
	{0xaa80, 0xaadf, "Tai Viet", 0},
	{0xaae0, 0xaaff, "Meetei Mayek Extensions", 0},
	{0xab00, 0xab2f, "Ethiopic Extended-A", 0},
	{0xab30, 0xab6f, "Latin Extended-E", 0},
	{0xab70, 0xabbf, "Cherokee Supplement", 0},
	{0xabc0, 0xabff, "Meetei Mayek", 0},
	{0xac00, 0xd7af, "Hangul Syllables", 0},
	{0xd7b0, 0xd7ff, "Hangul Jamo Extended-B", 0},
	{0xd800, 0xdb7f, "High Surrogates", 0},
	{0xdb80, 0xdbff, "High Private Use Surrogates", 0},
	{0xdc00, 0xdfff, "Low Surrogates", 0},
	{0xe000, 0xf8ff, "Private Use Area", 0},
	{0xf900, 0xfaff, "CJK Compatibility Ideographs", 0},
	{0xfb00, 0xfb4f, "Alphabetic Presentation Forms", 0},
	{0xfb50, 0xfdff, "Arabic Presentation Forms-A", 0},
	{0xfe00, 0xfe0f, "Variation Selectors", 0},
	{0xfe10, 0xfe1f, "Vertical Forms", 0},
	{0xfe20, 0xfe2f, "Combining Half Marks", 0},
	{0xfe30, 0xfe4f, "CJK Compatibility Forms", 0},
	{0xfe50, 0xfe6f, "Small Form Variants", 0},
	{0xfe70, 0xfeff, "Arabic Presentation Forms-B", 0},
	{0xff00, 0xffef, "Halfwidth and Fullwidth Forms", 0},
	{0xfff0, 0xffff, "Specials", 0},
	{}
};

/*
 * Sample text per unicode block, so that the preview shows the glyphs being
 * worked on.
 *
 * Right to left scripts and scripts that need shaping or combining marks are
 * left out on purpose, gp_text() draws a string glyph by glyph, left to right,
 * so a sample would show them wrong.  Those blocks fall back to the font's own
 * glyphs in the preview.
 */
static const struct {
	const char *block;
	const char *sample;
} samples[] = {
	{"Basic Latin",
		"Sphinx of black quartz, judge my vow. 0123456789 #&@?!"},
	{"Latin-1 Supplement",
		"Příliš žluťoučký kůň úpěl ódy §±¼½¾ ÀÉÎÕÜ àéîõü"},
	{"Latin Extended-A",
		"Příliš žluťoučký kůň úpěl ďábelské ódy ŁŒŠŽ łœšž"},
	{"Latin Extended-B",
		"Ș ș Ț ț Ǎ ǎ Ǐ ǐ Ǒ ǒ Ǔ ǔ Ǖ ǖ ƒ Ơ ơ Ư ư Ʒ ǯ"},
	{"IPA Extensions",
		"ðə kwɪk bɹaʊn fɒks dʒʌmps ˈoʊvɚ ðə ˈleɪzi dɔɡ"},
	{"Spacing Modifier Letters",
		"ʰ ʲ ʷ ˀ ˈ ˌ ː ˑ ˆ ˇ ˘ ˙ ˚ ˛ ˜ ˝"},
	{"Greek and Coptic",
		"Ξεσκεπάζω την ψυχοφθόρα βδελυγμία"},
	{"Cyrillic",
		"Съешь же ещё этих мягких французских булок"},
	{"Cyrillic Supplement",
		"Ԁ ԁ Ԃ ԃ Ԑ ԑ Ԛ ԛ Ԝ ԝ Ԥ ԥ Ԯ ԯ"},
	{"Armenian",
		"Հայերեն այբուբեն Աա Բբ Գգ Դդ Եե"},
	{"Georgian",
		"ქართული ენა აბგდევზთიკლმნოპ"},
	{"Latin Extended Additional",
		"Tiếng Việt có dấu Ẩ ẩ Ặ ặ Ố ố Ự ự Ỹ ỹ Ḍ ḍ Ẁ ẁ"},
	{"Greek Extended",
		"Ἀρχὴ ἥμισυ παντός ἄ ἒ ἢ ἶ ὂ ὖ ὦ ᾳ ῃ ῳ"},
	{"General Punctuation",
		"‘single’ “double” – — † ‡ • ‰ ′ ″ ‹ › …"},
	{"Superscripts and Subscripts",
		"H₂O x⁴ ⁽ⁿ⁺¹⁾ ₍ₐ₋ₓ₎ ⁰⁴⁵⁶⁷⁸⁹ ₀₁₂₃₄₅₆₇₈₉"},
	{"Currency Symbols",
		"₠ ₡ ₢ ₣ ₤ ₦ ₧ ₨ ₩ ₪ ₫ € ₭ ₮ ₱ ₲ ₴ ₵ ₹ ₺ ₽ ₿"},
	{"Letterlike Symbols",
		"℃ ℉ ℓ № ℗ ℞ ℠ ™ Ω ℧ K Å ℮ ℵ"},
	{"Number Forms",
		"⅓ ⅔ ⅕ ⅛ ⅜ ⅝ ⅞ Ⅰ Ⅱ Ⅲ Ⅳ Ⅴ Ⅹ Ⅼ Ⅽ Ⅾ Ⅿ ⅰ ⅱ ⅲ ⅳ"},
	{"Arrows",
		"← ↑ → ↓ ↔ ↕ ↖ ↗ ↘ ↙ ⇐ ⇑ ⇒ ⇓ ⇔ ⇕"},
	{"Mathematical Operators",
		"∀x ∃y: x ∈ A ∩ B ⊆ C ∑ ∏ √ ∞ ≈ ≠ ≤ ≥ ∂ ∇ ∫"},
	{"Miscellaneous Technical",
		"⌀ ⌂ ⌘ ⌚ ⌛ ⌈ ⌉ ⌊ ⌋ ⌠ ⌡ ⎡ ⎤ ⏎ ⏏ ⏻"},
	{"Box Drawing",
		"┌──┬──┐ ├──┼──┤ └──┴──┘ ╔══╦══╗ ╠══╬══╣ ╚══╩══╝"},
	{"Block Elements",
		"░▒▓█ ▀▄▌▐ ▁▂▃▄▅▆▇█"},
	{"Geometric Shapes",
		"■ □ ▪ ▫ ▲ △ ► ▶ ▼ ▽ ◀ ◆ ◇ ○ ◎ ● ◐ ◑ ◢ ◣"},
	{"Miscellaneous Symbols",
		"☀ ☁ ☂ ☃ ★ ☆ ☎ ☐ ☑ ☒ ☺ ☹ ♀ ♂ ♠ ♣ ♥ ♦ ♪ ♫ ⚠ ⚡"},
	{"Dingbats",
		"✁ ✂ ✈ ✉ ✎ ✓ ✔ ✗ ✘ ✚ ✦ ✱ ❄ ❤ ➔ ➜ ➤"},
	{"Braille Patterns",
		"⠓⠑⠇⠇⠕ ⠺⠕⠗⠇⠙ ⣿⣶⣤⣀"},
	{"CJK Symbols and Punctuation",
		"、。〃々〆〇〈〉《》「」『』【】〒〔〕"},
	{"Hiragana",
		"いろはにほへと ちりぬるを わかよたれそ"},
	{"Katakana",
		"イロハニホヘト チリヌルヲ ワカヨタレソ"},
	{"Halfwidth and Fullwidth Forms",
		"Ｆｕｌｌｗｉｄｔｈ ０１２３ ｶﾀｶﾅ ﾜｶﾖﾀﾚｿ"},
	{}
};

const char *gpf_ucode_block_sample(const char *name)
{
	unsigned int i;

	for (i = 0; samples[i].block; i++) {
		if (!strcmp(samples[i].block, name))
			return samples[i].sample;
	}

	return NULL;
}

struct gpf_ucode_block *gpf_blocks_build(struct gpf_font *font)
{
	struct gpf_ucode_block *vec = gp_vec_new(0, sizeof(struct gpf_ucode_block));
	uint32_t *codes = gpf_font_codes(font);
	unsigned int i;
	size_t j = 0;

	if (!vec || !codes) {
		gp_vec_free(vec);
		gp_vec_free(codes);
		return NULL;
	}

	/* both are sorted by the codepoint, so this is a merge */
	for (i = 0; gpf_ucode_blocks[i].name; i++) {
		const struct gpf_ucode_block *ub = &gpf_ucode_blocks[i];
		struct gpf_ucode_block block = *ub;

		while (j < gp_vec_len(codes) && codes[j] < ub->min)
			j++;

		while (j < gp_vec_len(codes) && codes[j] <= ub->max) {
			block.coverage++;
			j++;
		}

		if (!block.coverage && !gpf_font_block_declared(font, ub->min))
			continue;

		if (!GP_VEC_APPEND(vec, block)) {
			gp_vec_free(vec);
			gp_vec_free(codes);
			return NULL;
		}
	}

	gp_vec_free(codes);

	/* an empty font starts with Basic Latin, it has to start somewhere */
	if (!gp_vec_len(vec)) {
		if (!GP_VEC_APPEND(vec, gpf_ucode_blocks[0])) {
			gp_vec_free(vec);
			return NULL;
		}
	}

	return vec;
}

int gpf_blocks_find(const struct gpf_ucode_block *blocks, uint32_t code)
{
	size_t i;

	for (i = 0; i < gp_vec_len(blocks); i++) {
		if (code >= blocks[i].min && code <= blocks[i].max)
			return i;
	}

	return -1;
}
