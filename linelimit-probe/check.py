#!/usr/bin/env python3
"""Check the output of probe.c.

  check.py rule OUTPUT       does every row follow the LineLimit rule below?
  check.py diff A B          compare two outputs row by row (same font blocks)

The rule (what native GDI+ was measured to do):
  - text that is a single line, with no line break and no wrapping
    ("1/button", "1/near") is always kept: 1 line, drawn;
  - anything else keeps min(floor(h / GdipGetFontHeight), number of lines)
    lines, and draws nothing when that is 0.
Rows whose h is within 0.5% of a whole number of lines are reported but not
judged: right at the boundary the result depends on rounding inside GDI+.
"""
import math
import sys


def load(path):
    blocks, font = {}, None
    for line in open(path, encoding="utf-8", errors="replace"):
        if line.startswith("font "):
            font = line.split('"')[1]
            n = sum(1 for f in blocks if f.split("#")[0] == font)
            font = font if n == 0 else f"{font}#{n + 1}"
            fh = float(line.split("GdipGetFontHeight=")[1].split()[0])
            blocks[font] = {"fh": fh, "rows": []}
            continue
        f = line.split()
        if font and len(f) == 7 and f[0] != "case":
            name, k, h, lines, chars, drawn, nolimit = f
            blocks[font]["rows"].append((name, k, float(h), int(lines), int(chars), int(drawn), int(nolimit)))
    return blocks


def rule(path):
    bad = judged = 0
    for font, b in load(path).items():
        fh, rows = b["fh"], b["rows"]
        total = {}
        for r in rows:
            total[r[0]] = max(total.get(r[0], 0), r[3])
        for name, k, h, lines, chars, drawn, nolimit in rows:
            q = h / fh
            if abs(q - round(q)) < 0.005 * max(1, round(q)) and round(q) >= 1:
                continue
            want = 1 if name.startswith("1/") else min(math.floor(q), total[name])
            judged += 1
            if lines != want or drawn != (1 if want else 0):
                bad += 1
                print(f"{font}: {name} k={k} h={h}: lines={lines} drawn={drawn}, rule says lines={want}")
    print(f"{judged} rows judged, {bad} break the rule")
    return bad == 0


def diff(a_path, b_path):
    a, b = load(a_path), load(b_path)
    differ = compared = 0
    for font in a:
        if font not in b:
            print(f"font {font!r} only in {a_path}")
            continue
        rb = {(r[0], r[1]): r for r in b[font]["rows"]}
        for r in a[font]["rows"]:
            s = rb.get((r[0], r[1]))
            compared += 1
            if s is None or (r[3], r[4], r[5], r[6]) != (s[3], s[4], s[5], s[6]):
                differ += 1
                print(f"{font}: {r[0]} k={r[1]}: lines/chars/drawn/noLL {r[3:]} vs {s[3:] if s else '-'}")
    print(f"{compared} rows compared, {differ} differ")
    return differ == 0


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "rule":
        ok = rule(sys.argv[2])
    elif len(sys.argv) == 4 and sys.argv[1] == "diff":
        ok = diff(sys.argv[2], sys.argv[3])
    else:
        sys.exit(__doc__)
    sys.exit(0 if ok else 1)
