# The gpforge font format

A bitmap font is a **directory of UTF-8 text files**.  One file holds the
family-wide metadata, one file holds each style variant, and a variant other
than the base stores only what it changes — so a glyph is drawn once and
reused by every variant that does not need its own.

## 1. The directory

```
HaxorNarrow-18/
    family        family-wide metadata           required
    mono          base variant, every glyph      required
    regular       diff against mono              optional, proportional
    bold-mono     diff against mono              optional, emboldened
    bold          diff against regular           optional, bold proportional
```

The file name *is* the variant name; there are no others.  A variant file that
does not exist is a variant the font does not have, which is not an error — a
font with no proportional work done has no `regular`.

## 2. Lines

* Line oriented, at most 1024 bytes per line, UTF-8.
* A line is `keyword value`, keywords lowercase, fields separated by spaces or
  tabs.  Leading and trailing whitespace is insignificant.
* An **unknown keyword is an error.**  A reader must not skip what it does not
  understand.
* `#` starts a comment that runs to the end of the line, but only at the start
  of a line or after whitespace — so it does not cut into a value.  Lines that
  begin with `|`, `_` or `^` are ink rows and are never touched, `#` being
  the ink.
* Codepoints are written `U+` followed by hex digits, any length, no padding
  required.  A writer emits at least four digits, uppercase.
* Comments **before the first keyword** of a file are preserved when the file
  is rewritten.  Every other comment is not: variant files are regenerated
  wholesale.

## 3. `family`

```
family         Haxor Narrow
size           18
ascent         18
descent        5
line_gap       0

em             22
x_height       12
cap_height     15
ch_width       10

underline      -2 1
strikethrough  6 1
overline       19 1

default_glyph  U+003F
license        GPL-2.0-or-later

author         2019-2024 Cyril Hrubis <metan@ucw.cz>

block          U+0370 U+03FF Greek and Coptic
```

| keyword | value | meaning |
|---|---|---|
| `family` | text | the family name, as a consumer looks it up |
| `size` | int | the pixel size the font calls itself |
| `ascent`, `descent` | int | the line box, both positive, from the baseline |
| `line_gap` | int | extra leading between lines |
| `em` | int | the size the font is meant to be **read** as |
| `x_height`, `cap_height` | int | height of `x` and of `H` above the baseline |
| `ch_width` | int | the advance of `0` |
| `underline` | int int | position relative to the baseline, thickness |
| `strikethrough` | int int | position relative to the baseline, thickness |
| `overline` | int int | position relative to the baseline, thickness |
| `default_glyph` | `U+XXXX` | drawn for a codepoint the font has not got |
| `license` | text | SPDX identifier |
| `author` | `years name <email>` | one line per author, see below |
| `block` | `U+MIN U+MAX name` | a unicode block the font means to cover |

All numeric keys may be absent, in which case they are zero and a consumer
falls back to its own guess.

The three rules are each a position above the baseline and a thickness, so an
underline's position is negative and an overline's is above the `ascent` —
which is where the accents already are, and drawing over them is no overline.

**`em` is a decision, not a measurement.** It is what a layout gets for `1em`,
and it is neither the line box nor any advance: HaxorNarrow-18 is a 23 pixel
box that stands for 22, HaxorTiny is a 7 pixel box that stands for 12 because
it is meant to be shown at twice the size. An importer can only start it at
the box.

**`author` lines are the copyright holders**, one per line and kept in the
order they are written. The first field is the copyright years — digits, `-`
for a range and `,` for a list, no spaces, so `2024`, `2019-2024` or
`2019,2023` — the last is the email in angle brackets, and the name is
everything in between, spaces included. All three are required.

**`block` lines are an intention, not a fact.** A block that has glyphs in it
belongs to the font whether it is listed or not; these are the ones that are
still empty, so that a font begun for a new script remembers what it is aiming
at. The range is authoritative, the name is there to read.

Nothing here describes a bounding box. The drawing surface is
`ascent + descent` tall and as wide as the glyph's own advance.

## 4. Variant files

The header comes first, before any glyph block:

```
variant   regular
parent    mono
spacing   proportional
derive    embolden
advance   10
```

| keyword | value | |
|---|---|---|
| `variant` | variant name | must match the file name |
| `parent` | variant name | must be the lattice parent below; absent in `mono` |
| `spacing` | `mono` or `proportional` | |
| `derive` | `embolden` | optional |
| `advance` | int | the cell of a monospace variant, see below |

The lattice is fixed — a variant is exactly one parent plus an optional
derivation, and no other arrangement is legal:

```
mono                        base, every glyph stored
├── regular     parent mono                      proportional
│   └── bold    parent regular, derive embolden  proportional bold
└── bold-mono   parent mono,    derive embolden  mono bold
```

`bold` hangs off `regular` because the proportional metrics have to reach it.

The header `advance` is the advance a monospace variant is drawn to. It is not
inherited as a metric — every ink block carries its own — but it is the cell
the embolden smear is clamped to, and the advance an editor starts a new glyph
at.

## 5. Glyph blocks

The rest of a variant file is glyph blocks, **sorted by codepoint**, separated
by blank lines — a block ends at a blank line or at the next `glyph`. A block
starts with

```
glyph U+0041 'A'
```

The quoted character is present when the codepoint is printable — not a C0 or
C1 control, not DEL, not above U+10FFFF — and is decoration; the codepoint is
what counts.

A block is one of exactly **three** kinds, and it never mixes them. An ink
block may not carry metric lines and a numbers block may not carry rows, so no
field has two sources and there is no precedence to resolve.

**Ink** — every metric is implied by it:

```
glyph U+0041 'A'
   |......####......|
   |....##....##....|
   |....##....##....|
   |..##........##..|
   |..##........##..|
   |..##........##..|
   |##............##|
   |##............##|
   |################|
   |##............##|
   |##............##|
   |##............##|
   |##............##|
   |##............##|
  _|##............##|
    ^-------------------^
```

**Numbers** — no ink of its own, inherits the parent's:

```
glyph U+0041 'A'
advance 9
```

| keyword | value | |
|---|---|---|
| `advance` | int | overrides the inherited advance |
| `shift` | `dx,dy` | moves the inherited ink, positive right and up |

The shift is relative to wherever the parent has the ink, so the glyph follows
the parent when the parent is redrawn: a dot drawn on top of a letter in `mono`
is on top of it in every variant that shifts it.

**Composition** — the ink is built from two other glyphs:

```
glyph U+00C1 'Á'
compose U+0041 U+00B4 at 0,3
```

The base, the accent, and the offset the accent is drawn at, `dx` positive
right and `dy` positive up. The metrics are the base's. A reader must
understand it, and a writer keeps one it read for as long as it is still the
smallest form that is true. Nothing creates one: gpforge draws an accented
letter once and stores the drawing, and editing a composed glyph turns it into
ink.

## 6. The ink

* One pixel is **two characters**, `##` set and `..` clear, which makes pixels
  roughly square in a terminal.
* `|` delimits the ink columns. Every row of a block starts and ends at the
  same column and has the same width; at most 256 rows.
* **`_` in the left gutter marks the last row above the baseline.** It appears
  exactly once per block, so an ink block always contains at least one row
  above the baseline, blank-padded when the ink starts below it.
* The **ruler** on the last line is `^`, then dashes, then `^`, laid out in the
  same two-character grid as the pixels. The left tick is the origin, x = 0;
  the right tick is the advance. It may begin with dashes rather than the
  first tick, which is how a negative `bearing_x` is written.

Everything else follows:

| metric | comes from |
|---|---|
| ink, width, height | the `##` pixels, trimmed to the ink |
| `bearing_x` | columns between the origin tick and the first ink column |
| `bearing_y` | rows above the `_` mark |
| `advance` | distance between the two ticks |

The ink box is **trimmed**: blank rows and columns around the ink are not part
of the glyph, and a reader must trim them so that two spellings of the same
drawing produce the same glyph.

An empty glyph — a space — is a single blank row as wide as the advance:

```
glyph U+0020 ' '
  _|....................|
    ^-------------------^
```

`_` is a glyph whose ink is entirely below the baseline, a negative
`bearing_y`, which reads as blank rows between the baseline mark and the ink:

```
glyph U+005F '_'
  _|....................|
   |....................|
   |####################|
    ^-------------------^
```

## 7. Resolving a glyph

What a consumer does to turn the files into one face, `mono` is the answer for
itself: its entry, or the glyph is not in the font.

**`regular`** — its own entry if it has one, otherwise `mono`'s glyph. A
numbers entry takes the ink from `mono` and applies its own numbers.

**`bold-mono`** — the same against `mono`, with `derive embolden` applied to
ink that came up from `mono`.

**Embolden** smears the ink one pixel right: every set pixel also sets the
pixel to its right. Since the ink box is trimmed, its rightmost column always
has ink, so the box grows one pixel wider. A gap of two pixels or more
survives, a one pixel gap is filled in. The `bearing_x` and `advance` do not
change. **In a monospace variant the smear stops at the cell**: when
`bearing_x + width + 1` would pass the cell the glyph keeps its rightmost
column instead of growing. The cell is the `advance` in the `mono` header,
and only when `mono` is monospaced. Emboldened glyphs are not stored, so
every consumer must reproduce this exactly.

**`bold`** is decided per glyph:

```
1. bold has an ink or compose entry      -> use it, metrics included
2. regular overrides the INK             -> embolden(regular's ink)
3. otherwise                             -> resolve(bold-mono)
```

For 2 and 3 the advance and bearings come from `resolve(regular)`, adjusted by
a numbers entry in `bold` when there is one. **The advance grows by what the
bold ink is wider than regular's**, so bold keeps the gap regular leaves on the
right:

* step 2 always, a proportional advance was fitted to the narrower ink;
* step 3 only when `regular`'s advance differs from `mono`'s, ink coming up
  from `bold-mono` was made to fit the monospace cell, which is room only as
  long as `regular` keeps the cell's advance.

A hand drawn glyph in step 1 carries its own ruler.

A composition inherited from `regular` is the exception to 2 and 3: it is built
here out of `bold`'s own parts and takes its metrics from its base.

Step 2 asks whether `regular` overrides the **ink**, not whether it has an
entry: almost every entry in `regular` is a numbers block, and those must fall
through to step 3 or `bold-mono`'s hand fixes are ignored.  An ink entry whose
ink is identical to `mono`'s does not override it either, it is the same glyph
as the numbers block a writer stores it as. The fallback is
directional — `bold` may read `bold-mono`, never the reverse.

## 8. What a writer stores

An entry in a variant other than `mono` is written in the smallest form that is
true, compared against what the variant would resolve to without it:

| the entry, against the derived glyph | written as |
|---|---|
| identical | nothing at all |
| same ink, different numbers | a numbers block |
| different ink | an ink block |

A writer that skips this stores a copy of the parent's ink every time an
advance changes. Glyph blocks are written in codepoint order, one blank line
between them, and a variant file with no glyphs left is still a valid file.
