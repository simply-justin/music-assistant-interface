"""Generates strings.gen.hpp/.cpp from strings.csv (id + one column per language).

Adding a language = adding a column. A missing translation falls back to English at build time.
"""
import csv
import pathlib
import sys


def c_str(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def main(src: str, out_dir: str) -> None:
    with open(src, encoding="utf-8", newline="") as f:
        rows = list(csv.reader(f))
    header, rows = rows[0], [r for r in rows[1:] if r and r[0].strip()]
    langs = header[1:]
    assert header[0] == "id" and langs and langs[0] == "en", "first columns must be id,en"
    ids = [r[0].strip() for r in rows]
    assert len(set(ids)) == len(ids), "duplicate string id"
    for r in rows:
        assert all(c.isalnum() or c == "_" for c in r[0]), f"bad id {r[0]!r}"

    out = pathlib.Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    hpp = [
        "// Generated from strings.csv by gen_strings.py. Do not edit.",
        "#pragma once",
        "#include <cstdint>",
        "namespace i18n {",
        "enum class Lang : uint8_t { " + ", ".join(langs) + " };",
        f"constexpr int kLangCount = {len(langs)};",
        "enum class Str : uint16_t {",
        *[f"    {i}," for i in ids],
        "};",
        f"constexpr int kStrCount = {len(ids)};",
        "extern const char* const kLangCodes[kLangCount];",
        "extern const char* const kStrings[kLangCount][kStrCount];",
        "}  // namespace i18n",
    ]
    cpp = [
        "// Generated from strings.csv by gen_strings.py. Do not edit.",
        '#include "strings.gen.hpp"',
        "namespace i18n {",
        "const char* const kLangCodes[kLangCount] = {" + ", ".join(c_str(l) for l in langs) + "};",
        "const char* const kStrings[kLangCount][kStrCount] = {",
    ]
    for col, lang in enumerate(langs, start=1):
        cells = [(r[col] if col < len(r) and r[col] else r[1]) for r in rows]
        cpp.append(f"    {{  // {lang}")
        cpp += [f"        {c_str(c)}," for c in cells]
        cpp.append("    },")
    cpp += ["};", "}  // namespace i18n"]

    # Only rewrite on change, so builds stay incremental.
    for name, lines in (("strings.gen.hpp", hpp), ("strings.gen.cpp", cpp)):
        p, text = out / name, "\n".join(lines) + "\n"
        if not p.exists() or p.read_text(encoding="utf-8") != text:
            p.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
