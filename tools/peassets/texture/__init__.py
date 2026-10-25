"""Textures: one texture page of pixels, stored as indexed PNGs, and the image and palette chunks built from them.

A texture is `bpp` 4 or 8 (indexed) or 16 (direct color), `width` texels by `height` rows, in one or more
`variants`. Each variant is a complete PNG beside its texture's other files, `<local name>.png` for the variant
`default` and `<local name>.<variant>.png` for any other (files.variant_file): indexed for 4 and 8 bits, its PLTE the
variant's palette in stored order (16 or 256 entries, unused ones included), RGBA for 16 bits. All variants of a
texture have the same indices; the game holds the pixels once and swaps the palette.

A file (a `file` record) lists its chunks in order. An image or palette chunk is built from textures, so it is
declared there, by the sha1 of its stored bytes and what it uploads where; sounds, packages and the like are
assets of their own:

    contents = [
        { chunk = "palette", sha1 = "...", palettes = [{ texture = "texture", variant = "default",
                                                        region = "engine/vram/weapon_palette" }] },
        { chunk = "image", sha1 = "...", pages = [{ texture = "texture", region = "engine/vram/weapon_sheet" }] },
        { chunk = "sounds" },
        { chunk = "package" },
    ]

A page or palette is placed by naming a VRAM region (a `vram_region`: a rectangle of VRAM, groups/engine/vram.toml),
plus an `offset` in it where the region is larger than what is placed; a region may be `within` another, as part of it. A page holds one texture or one `atlas`, a record listing its `sprites`,
textures each at a texel `at` of its own depth (the UI sheet); a texture whose palettes an executable uploads names
that embedded palette as `palettes_from` (the UI font's), or a table of variants and their own (the security room's lamps). A file may name another group's chunks (`asset`) and
textures by path; a palette a file uploads for another file's resident page is a variant of that page's texture (the
landing pad's fireball palettes recolor the effect atlas the boot files upload), decoded with the page. An image chunk's entry may also state `slots`, where its page table has room for more pages than it holds, a page
`truncated = true`, where its last strip stores one byte less, and any chunk its `sector_len`, where the file gives
it less than the whole 0x800-byte sector (chunks.py says what each means).

    color.py         the PS1 color <-> 8-bit RGBA, one-to-one
    chunks.py        image and palette chunks: their content, read, and encoded exactly before LZSS
    png.py           a variant's PNG, written and read back against its texture
    declaration.py   the schema of textures, files and VRAM regions, and the chunk records a file implies
    files.py         a declared file decoded to its textures, and its chunks encoded from them

    python3 tools/peassets/texture decode|encode|check [--assets assets/USA] [--only FILE] [--out DIR]
"""
