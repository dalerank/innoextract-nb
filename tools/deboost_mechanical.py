#!/usr/bin/env python3
"""
Mechanical, safe textual substitutions to remove easy Boost dependencies.
Run once from repo root: python tools/deboost_mechanical.py
Only touches src/**/*.hpp and src/**/*.cpp.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"

INT_TYPES = ["uint8_t", "uint16_t", "uint32_t", "uint64_t", "int8_t", "int16_t", "int32_t", "int64_t"]


def balanced_foreach_replace(text):
    """Replace BOOST_FOREACH(decl, range) with for(decl : range), handling nested parens/templates."""
    out = []
    i = 0
    pattern = "BOOST_FOREACH("
    while True:
        idx = text.find(pattern, i)
        if idx == -1:
            out.append(text[i:])
            break
        out.append(text[i:idx])
        # find matching close paren starting after "BOOST_FOREACH("
        depth = 1
        j = idx + len(pattern)
        start_args = j
        angle_depth = 0
        comma_split = None
        while depth > 0:
            c = text[j]
            if c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
                if depth == 0:
                    break
            elif c == '<':
                angle_depth += 1
            elif c == '>':
                if angle_depth > 0:
                    angle_depth -= 1
            elif c == ',' and depth == 1 and angle_depth == 0 and comma_split is None:
                comma_split = j
            j += 1
        args_end = j  # index of matching ')'
        decl = text[start_args:comma_split].strip()
        rng = text[comma_split + 1:args_end].strip()
        out.append(f"for({decl} : {rng})")
        i = args_end + 1
    return "".join(out)


def process(path: Path):
    original = path.read_text(encoding="utf-8")
    text = original

    # cstdint
    text = text.replace("#include <boost/cstdint.hpp>", "#include <cstdint>")
    for t in INT_TYPES:
        text = re.sub(r"\bboost::" + t + r"\b", "std::" + t, text)

    # boost::size/begin/end -> std::
    uses_std_size_begin_end = False
    if "boost::size(" in text or "boost::begin(" in text or "boost::end(" in text:
        uses_std_size_begin_end = True
    text = text.replace("boost::size(", "std::size(")
    text = text.replace("boost::begin(", "std::begin(")
    text = text.replace("boost::end(", "std::end(")

    # remove now-unneeded boost range includes
    text = re.sub(r"[ \t]*#include <boost/range/size\.hpp>\n", "", text)
    text = re.sub(r"[ \t]*#include <boost/range/begin\.hpp>\n", "", text)
    text = re.sub(r"[ \t]*#include \"boost/range/begin\.hpp\"\n", "", text)
    text = re.sub(r"[ \t]*#include <boost/range/end\.hpp>\n", "", text)
    text = re.sub(r"[ \t]*#include \"boost/range/end\.hpp\"\n", "", text)

    if uses_std_size_begin_end and "#include <iterator>" not in text:
        # insert after the first #include line
        m = re.search(r"^#include .*\n", text, re.MULTILINE)
        if m:
            text = text[:m.end()] + "#include <iterator>\n" + text[m.end():]

    # BOOST_FOREACH
    if "BOOST_FOREACH" in text:
        text = balanced_foreach_replace(text)
        text = re.sub(r"[ \t]*#include <boost/foreach\.hpp>\n", "", text)

    # BOOST_STATIC_ASSERT(X); -> static_assert(X, "static assertion failed");
    text = re.sub(
        r"BOOST_STATIC_ASSERT\((.*?)\);",
        r'static_assert(\1, "static assertion failed");',
        text,
        flags=re.DOTALL,
    )
    text = re.sub(r"[ \t]*#include <boost/static_assert\.hpp>\n", "", text)

    if text != original:
        path.write_text(text, encoding="utf-8")
        print(f"updated {path.relative_to(ROOT)}")


def main():
    for path in sorted(SRC.rglob("*")):
        if path.suffix in (".hpp", ".cpp", ".h"):
            process(path)


if __name__ == "__main__":
    main()
