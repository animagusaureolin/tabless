#!/usr/bin/env python3
"""Renames the bundled font cuts in resources/fonts/ in place, without cutting them again.

The cuts carry the product's name as a private family prefix ("lowkey Golos Text",
see tools/gen_bundled_fonts.py for why). A product rename only has to change that
prefix in the name table (IDs 1, 3, 4, 6); outlines, unitsPerEm and the glyphs added
after the cut (tools/fonts/add_return_glyph.py) stay exactly as they are.

    python tools/fonts/rename_bundled_fonts.py lowkey Tabless
"""

import sys
from pathlib import Path

from fontTools.ttLib import TTFont

FONTS = Path(__file__).resolve().parent.parent.parent / "resources" / "fonts"


def main() -> None:
    old, new = sys.argv[1], sys.argv[2]
    for path in sorted(FONTS.glob("*.ttf")):
        font = TTFont(path)
        changed = 0
        for rec in font["name"].names:
            if rec.nameID not in (1, 3, 4, 6):
                continue
            text = rec.toUnicode()
            # "lowkey Golos Text", "2.004;lowkey;lowkeyGolosText-Medium", "lowkeyGolosText-Medium"
            renamed = text.replace(old + " ", new + " ").replace(";" + old + ";", ";" + new + ";")
            if renamed.startswith(old) or (";" + old) in renamed:
                renamed = renamed.replace(old, new)
            if renamed != text:
                font["name"].setName(renamed, rec.nameID, rec.platformID, rec.platEncID, rec.langID)
                changed += 1
        if changed:
            font.save(path)
        print(f"{path.name}: {changed} name(s) -> {font['name'].getDebugName(1)}")


if __name__ == "__main__":
    main()
