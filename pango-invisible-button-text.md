# Pango backend: `StringFormatFlagsLineLimit` does not follow GDI+, and blanks Mono button labels

Upstream ticket: https://gitlab.winehq.org/mono/libgdiplus/-/work_items/6
Downstream: Launchpad [#2069473](https://bugs.launchpad.net/ubuntu/+source/libgdiplus/+bug/2069473)

> An earlier version of this report said that GDI+ always draws the first line
> under `LineLimit`. That was wrong, as the maintainer pointed out: it only holds
> for text that is a single line. The measurements below replace it.

## Summary

With the Pango text renderer, `GdipDrawString` and `GdipMeasureString` decide
which lines fit under `StringFormatFlagsLineLimit` differently from GDI+. In the
common case - a label drawn into a rectangle exactly `Font.Height` tall - the
label is dropped entirely. Mono's WinForms does exactly that for every button,
so button labels come out blank (`MessageBox.Show ("a")` shows an empty OK
button with Noto Sans).

## What GDI+ does

Measured with the native `gdiplus.dll` (1.1.7601.17514, from Windows 7 SP1) run
under Wine, using the **same font file as libgdiplus** (Noto Sans 8.25pt,
`GdipGetFontHeight` = 14.982), so that only the behaviour differs. Heights are
`k × GdipGetFontHeight`; `lines`/`chars` come from `GdipMeasureString` with
`LineLimit`, `drawn` says whether `GdipDrawString` with `LineLimit` painted
anything. The probe that produced this is `linelimit-probe/probe.c`; one source,
built for both libraries.

```
case           k       h  lines  chars  drawn
"OK"        0.50    7.49      1      2      1     <- single line: always kept
"OK"        F.Ht   15.00      1      2      1
"OK\n"      0.50    7.49      0      0      0     <- a line break, even a final one,
"OK\n"      F.Ht   15.00      1      3      1        and the rule below applies
"A\nB\nC"   0.99   14.83      0      0      0
"A\nB\nC"   1.01   15.13      1      2      1
"A\nB\nC"   2.01   30.11      2      4      1
"A\nB\nC"   2.99   44.80      2      4      1
"A\nB\nC"   3.01   45.10      3      5      1
wrapping    0.99   14.83      0      0      0     <- wrapping counts as several lines
wrapping    2.01   30.11      2     22      1
```

So, with `LineLimit`:

- GDI+ keeps `floor(height / GdipGetFontHeight)` lines - none if the rectangle is
  shorter than one line (this is what the maintainer saw);
- except that text which is a single line, with no line break and no wrapping,
  is always kept, at any height.

The same pattern holds with a second font (Tahoma), and matches what I measured
earlier with .NET Framework on real Windows 10 (`"OK"` drawn at every height;
`"A\nB\nC"` drawn as `floor(h / font.GetHeight())` lines, none below one line).

## What libgdiplus does

`gdip_pango_setup_layout` compares each line against **Pango's logical line
box** instead of the font's line spacing. Pango rounds ascent and descent up
separately, so its line box is taller: 16 against 14.982 for Noto Sans 8.25pt.
And it has no single-line exception. Against the same probe, 43 of 98 cases
differ from GDI+; for example:

```
case           k       h  lines  chars  drawn
"OK"        0.50    7.49      0      0      0     GDI+: 1 2 1
"OK"        F.Ht   15.00      0      0      0     GDI+: 1 2 1   <- the button label
"A\nB\nC"   F.Ht   15.00      0      0      0     GDI+: 1 2 1
"A\nB\nC"   2.01   30.11      1      2      1     GDI+: 2 4 1
```

A second, smaller discrepancy: when lines are trimmed at a line break, the kept
text ends in `\n` and Pango lays out an empty line after it. That empty line was
counted by `MeasureString` and shifted vertically centered text.

## Why Mono's buttons are hit

`System.Windows.Forms.ButtonBase` sets `LineLimit` unconditionally, and
`ThemeWin32Classic.ButtonBase_DrawText` draws into
`Rectangle.Inflate (ClientRectangle, -4, -4)` with
`Height = Math.Max (Font.Height, ...)`. For MessageBox's 86x23 buttons that is a
15 pixel tall rectangle; `Font.Height` is `ceil (14.982)` = 15. GDI+ keeps the
label (it is one line, and one line of 14.982 fits anyway); libgdiplus compares
against 16 and drops it.

Whether you see it depends on the font: it happens when Pango's line box is
taller than `max (Font.Height, 15)`. Noto Sans, what fontconfig returns for
"Microsoft Sans Serif" on Kubuntu, is one pixel over; DejaVu Sans and Liberation
Sans are not.

## Regression

The trimming was changed to its current form in `0a27652` (2018-04-01, "Add
initial tests for GdipMeasureString and fix Pango text backend to pass them").
Building its parent and it with the reproduction: parent draws the label, `0a27652`
does not. Present in 6.0 through 6.2 and `main`.

## Fix

In `gdip_pango_setup_layout`:

1. With `LineLimit`, keep `floor (height / GdipGetFontHeight)` lines, and keep a
   text that is a single line (no line break, one layout line) regardless.
2. When trimming at a line break, leave the break out of the layout so Pango does
   not add an empty line, but still count it as fitted in `MeasureString`
   (GDI+ reports `chars` = 2 for `"A\n"`).
3. `pango_MeasureString` no longer re-applies `LineLimit` against Pango's line
   box; the layout it gets has already been trimmed.

With this, the number of lines kept and whether anything is drawn match native
GDI+ in **all 98** cases of the probe, and the MessageBox button labels are drawn.
The fitted character count matches in 89; the other 9 are all the wrapping case,
where GDI+ and Pango break the line at different characters (13 against 11 for
the first line). That difference is there before this change as well and has
nothing to do with `LineLimit`.

## Test

`tests/testtext.c`: `test_draw_string_line_limit` checks the rules above with
heights expressed in multiples of `GdipGetFontHeight` (so it does not depend on
the font): a single line is drawn at `Font.Height` and at half a line; `"OK\n"`
and `"A\nB\nC"` draw nothing below one line; `"A\nB\nC"` measures 1, 2 and 3
lines (2, 4 and 5 characters) at 1.5, 2.5 and 3.5 lines. It fails on `427daeb`
and passes with the fix. The other C tests give the same results as before.

## Environment

- libgdiplus `main` at `427daeb`, Pango build
- Native GDI+: `gdiplus.dll` 1.1.7601.17514 (Windows 7 SP1, x86) under Wine 10.0
- Earlier cross-check: Windows 10, .NET Framework 4.8
