"""The game's text encodings: which character each byte of a text, or each index of a glyph script, stands for.

    UI         the UI fonts and the texts drawn with them (main's textDrawString): Windows-1252 from 0x20, read off
               the fonts' glyphs, except that 0x7B..0x7E are the controller's buttons where ASCII has { | } ~. A
               byte Windows-1252 leaves undefined (0x81, 0x8D, 0x8F, 0x90, 0x9D) and 0x7F stand for no character.
    CREDITS    the credits fonts' glyph indices, each face's 60 glyphs in the order the credits first use them
    CAPTION    the load-screen caption font's glyph scripts (gTextCaptionGlyphCells, bootload.c's caption streams):
               an index per glyph, A-Z, a-z, 1-9 and 0, then : ; ' , . ? ! and the space.

Each is {code: character}; a code it does not list has no character.
"""
from __future__ import annotations

BUTTONS = {0x7B: "○", 0x7C: "△", 0x7D: "✕", 0x7E: "□"}    # circle, triangle, cross, square


def _ui() -> dict[int, str]:
    out = {}
    for code in range(0x20, 0x100):
        if code in BUTTONS:
            out[code] = BUTTONS[code]
            continue
        try:
            char = bytes([code]).decode("cp1252")
        except UnicodeDecodeError:
            continue
        if char.isprintable() or char == " ":
            out[code] = char
    return out


UI = _ui()
CAPTION = dict(enumerate("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890:;',.?! "))
# The credits (the ending's STF document): each face's 60 glyphs in the order the credits first use them,
# "BASED ON THE NOVEL ..." - index 5 is the space, 12 the apostrophe.
CREDITS = dict(enumerate("BASED ONTHVL'PRIKWUXCMYGFJZQ12.o9-(,)&amplingsudrectbyhvkfz0"))
ENCODINGS = {"ui": UI, "caption": CAPTION, "credits": CREDITS}
