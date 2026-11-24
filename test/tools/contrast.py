#!/usr/bin/env python3
"""WCAG 2.2 contrast check of the colour tokens in components/ui/theme.cpp.

Reads the palettes straight from the source, prints the worst ratio per pair in each theme and
exits with 1 when a pair is below its requirement (4.5:1 text, 3:1 non-text).
Usage: python3 test/tools/contrast.py
"""
import pathlib, re, sys

SRC = pathlib.Path(__file__).resolve().parents[2] / "components/ui/theme.cpp"
code = SRC.read_text()
FIELDS = ("bg side surface elevated hairline border text text2 text3 primary on_primary field_bg field_text "
          "warning danger_text danger_fill live").split()


def palette(name):
    body = re.search(name + r"\{([^}]*)\}", code).group(1)
    return dict(zip(FIELDS, (int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{6})", body))))


def accents():
    body = re.search(r"kAccents\[\] = \{(.*?)\};", code, re.S).group(1)
    pairs = re.findall(r"\{(0x[0-9A-Fa-f]+|0), (0x[0-9A-Fa-f]+|0)\}", body)
    return [(int(d, 16), int(l, 16)) for d, l in pairs[1:]]  # index 0 = neutral (the primary)


def lum(rgb):
    def ch(v):
        v /= 255
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = rgb >> 16 & 255, rgb >> 8 & 255, rgb & 255
    return 0.2126 * ch(r) + 0.7152 * ch(g) + 0.0722 * ch(b)


def ratio(a, b):
    la, lb = sorted((lum(a), lum(b)), reverse=True)
    return (la + 0.05) / (lb + 0.05)


failed = False
print(f"{'pair':52} {'dark':>7} {'light':>7}  need")
themes = {"dark": palette("kDark"), "light": palette("kLight")}
acc = accents()
surfaces = ("bg", "side", "surface", "elevated")
rows = []
for fg, need in (("text", 4.5), ("text2", 4.5), ("text3", 4.5), ("warning", 4.5), ("danger_text", 4.5)):
    rows.append((f"{fg} on bg / side / surface / elevated", need,
                 {t: min(ratio(p[fg], p[s]) for s in surfaces) for t, p in themes.items()}))
rows.append(("white on danger_fill (destructive button)", 4.5, {t: ratio(0xFFFFFF, p["danger_fill"]) for t, p in themes.items()}))
rows.append(("white on live (LIVE tag)", 4.5, {t: ratio(0xFFFFFF, p["live"]) for t, p in themes.items()}))
rows.append(("field_text on field_bg", 4.5, {t: ratio(p["field_text"], p["field_bg"]) for t, p in themes.items()}))
rows.append(("on_primary on primary (neutral accent)", 4.5, {t: ratio(p["on_primary"], p["primary"]) for t, p in themes.items()}))
rows.append(("label on each accent", 4.5, {
    "dark": min(ratio(0x000000, d) for d, _ in acc), "light": min(ratio(0xFFFFFF, l) for _, l in acc)}))
rows.append(("accent vs page, non-text", 3.0, {
    "dark": min(min(ratio(d, themes["dark"][s]) for s in surfaces) for d, _ in acc),
    "light": min(min(ratio(l, themes["light"][s]) for s in surfaces) for _, l in acc)}))
# `border` is decorative (about 1.5:1): every control that uses it carries a stronger cue, checked here.
rows.append(("toggle off: knob (text2) vs its track (elevated)", 3.0,
             {t: ratio(p["text2"], p["elevated"]) for t, p in themes.items()}))
rows.append(("text field edge (text3) vs field fill (surface)", 3.0,
             {t: ratio(p["text3"], p["surface"]) for t, p in themes.items()}))

for name, need, r in rows:
    bad = [t for t in r if r[t] < need]
    failed |= bool(bad)
    print(f"{name:52} {r['dark']:7.2f} {r['light']:7.2f}  {need}{'  FAIL ' + ','.join(bad) if bad else ''}")
sys.exit(1 if failed else 0)
