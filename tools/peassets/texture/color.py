"""The PS1's 15-bit color plus its bit 15, mapped one-to-one onto 8-bit RGBA:

    0x0000                     alpha 0     transparent
    bit 15 clear               alpha 255
    bit 15 set, color != 0    alpha 128   semi-transparent where the primitive blends
    0x8000                     alpha 255   black that draws
"""
from __future__ import annotations


def color_to_rgba(c: int) -> tuple[int, int, int, int]:
    e = lambda v: v << 3 | v >> 2
    alpha = 0 if c == 0 else 128 if c & 0x8000 and c & 0x7FFF else 255
    return e(c & 31), e(c >> 5 & 31), e(c >> 10 & 31), alpha


def rgba_to_color(r: int, g: int, b: int, a: int) -> int:
    if a == 0:
        return 0
    if any(v != (v >> 3) << 3 | v >> 5 for v in (r, g, b)) or a not in (128, 255):
        raise ValueError(f"({r}, {g}, {b}, {a}) is not a PS1 color")
    c = r >> 3 | (g >> 3) << 5 | (b >> 3) << 10
    return c | (0x8000 if a == 128 or c == 0 else 0)
