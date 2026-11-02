#!/usr/bin/env python3
"""What a package's slots come to as C, as the editor's tokenizers make it (`extensions/packages/src/tokenizers.rs`).

A tokenizer is declared for a type: what it makes of the whole value and the values derived from it, each with the args
it reads. The entries and what they ask for are the editor's. What they read is not: this build knows no asset's editable
form, so a model or an image is what the disc stores for it, and only what no bytes hold comes from its declaration.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass, field
from typing import Callable

from declarations import Declaration, Declarations, uid_of
from gen_model_source import definition
from stage0 import Stage0

Args = dict[str, list[str]]
RECORD = 0x24
# A template's type, where the two differ.
TYPES = {"texture": "image"}
BLENDS = ("mix", "add", "subtract", "add_quarter")
DEFAULT_BLEND = "mix"
WORDS_PER = {"indexed_4bpp": 4, "indexed_8bpp": 2}
BITS = {"indexed_4bpp": 4, "indexed_8bpp": 8}
TEXTURELESS = {"colored"}


def hexadecimal(number: int) -> str:
    return f"0x{number:X}"


def one(args: Args, name: str) -> str:
    """An arg's one value."""
    if name not in args:
        raise ValueError(f"it needs {name}")
    if len(args[name]) != 1:
        raise ValueError(f"{name} takes one value")
    return args[name][0]


@dataclass
class Reader:
    """What tokenizers read: the declarations, and a release's disc."""

    declarations: Declarations
    disc: Stage0
    release: str

    def stored(self, held: Declaration) -> tuple[bytes, int]:
        """The bytes an asset is stored as, and the address they are at."""
        place = held.stored(self.release)
        if place is None:
            raise ValueError(f"{held.path} is not stored on {self.release}'s disc")
        if "offset" not in place.place:
            raise ValueError(f"{held.path} is stored as a slot of its own, which only the editor reads")
        file = self.disc.slot(place.files[0], place.slot)
        return file[place.offset:place.offset + place.size], place.address

    def member(self, prefab: Declaration, address: str) -> Declaration:
        """The model a prefab's member holds."""
        fill = prefab.table.get("groups", {})
        for step in address.split("/"):
            if step not in fill:
                raise ValueError(f"it has no member {address}")
            fill = fill[step]
        if "model" not in fill:
            raise ValueError(f"{address} holds no model")
        return self.declarations.named(fill["model"]["slot"])

    def placed(self, placement: dict) -> tuple[int, int]:
        """Where a placement (`slot`, `at`) puts what it holds, in VRAM halfwords."""
        x, y = self.declarations.named(placement["slot"]).table["rect"][:2]
        at = placement.get("at", [0, 0])
        return x + at[0], y + at[1]


def model_definition(reader: Reader, held: Declaration, args: Args) -> str:
    stored, address = reader.stored(held)
    return definition(stored, address, one(args, "symbol")).replace("\n", " ")


def record_field(reader: Reader, held: Declaration, at: int) -> str:
    stored, _ = reader.stored(held)
    return str(struct.unpack_from("<i", stored, len(stored) - RECORD + at)[0])


def texture_of(reader: Reader, material: Declaration) -> tuple[Declaration, dict, str]:
    """A material's image, its cell, and the palette it is read through."""
    if material.table.get("shader") in TEXTURELESS:
        raise ValueError("it has no texture")
    main = material.table.get("main")
    if main is None:
        raise ValueError("it has no texture")
    _, output = uid_of(main)
    if output is None or not output.startswith("cells/"):
        raise ValueError("no cell is named")
    cell, _, palette = output.removeprefix("cells/").partition("@")
    image = reader.declarations.named(main)
    held = image.table.get("cells", {}).get(cell)
    if held is None:
        raise ValueError(f"no cell {cell}")
    palette = palette or held.get("palette")
    if not palette:
        raise ValueError(f"{cell} has no palette")
    return image, held, palette


def material_tpage(reader: Reader, material: Declaration, _: Args) -> str:
    image, cell, _ = texture_of(reader, material)
    bits = BITS.get(image.table.get("mode"), 16)
    if bits == 16:
        raise ValueError("a 16 bit image is no texture of a face")
    page_x, page_y = reader.placed(image.table["page"])
    x, y = cell["rect"][:2]
    page_x, page_y = (page_x + x * bits // 16) // 64, (page_y + y) // 256
    blend = BLENDS.index(material.table.get("blend", DEFAULT_BLEND))
    return hexadecimal(page_x | page_y << 4 | blend << 5 | (bits == 8) << 7)


def material_clut(reader: Reader, material: Declaration, _: Args) -> str:
    image, _, palette = texture_of(reader, material)
    placement = image.table.get("palettes", {}).get(palette)
    if placement is None:
        raise ValueError(f"no palette {palette}")
    x, y = reader.placed(placement)
    return hexadecimal(x // 16 | y << 6)


def image_words(reader: Reader, image: Declaration, _: Args) -> str:
    stored, _ = reader.stored(image)
    if len(stored) % 4:
        raise ValueError(f"{len(stored)} bytes are no words")
    words = struct.unpack(f"<{len(stored) // 4}I", stored)
    return ", ".join(hexadecimal(word) if word > 0x7FFFFFFF else str(word) for word in words)


def image_size(image: Declaration) -> list[int]:
    if "size" not in image.table:
        raise ValueError("it has no size")
    return image.table["size"]


def image_width_words(_: Reader, image: Declaration, __: Args) -> str:
    return str(image_size(image)[0] // WORDS_PER.get(image.table.get("mode"), 1))


def image_height(_: Reader, image: Declaration, __: Args) -> str:
    return str(image_size(image)[1])


def of_member(make: Callable[[Reader, Declaration, Args], str]) -> Callable[[Reader, Declaration, Args], str]:
    """A prefab's member is a model, as the prefab holds it."""
    return lambda reader, prefab, args: make(reader, reader.member(prefab, one(args, "member")), args)


def model_buffer(reader: Reader, held: Declaration, _: Args) -> str:
    return record_field(reader, held, 4)


def model_seam(reader: Reader, held: Declaration, _: Args) -> str:
    return record_field(reader, held, 8)


@dataclass(frozen=True)
class Entry:
    make: Callable[[Reader, Declaration, Args], str]
    args: tuple[str, ...] = ()


@dataclass(frozen=True)
class Tokenizer:
    derived: dict[str, Entry] = field(default_factory=dict)
    whole: Entry | None = None


TOKENIZERS = {
    "model": Tokenizer(
        whole=Entry(model_definition, ("symbol",)),
        derived={"buffer": Entry(model_buffer), "seam": Entry(model_seam)},
    ),
    "prefab": Tokenizer(
        whole=Entry(of_member(model_definition), ("symbol", "member")),
        derived={"buffer": Entry(of_member(model_buffer), ("member",)), "seam": Entry(of_member(model_seam), ("member",))},
    ),
    "material": Tokenizer(derived={"tpage": Entry(material_tpage), "clut": Entry(material_clut)}),
    "image": Tokenizer(
        whole=Entry(image_words),
        derived={"width_words": Entry(image_width_words), "height": Entry(image_height)},
    ),
}


def type_of(declaration: Declaration) -> str:
    return TYPES.get(declaration.template, declaration.template or "nothing")


def tokens(reader: Reader, node: Declaration, derived: str | None, args: Args) -> str:
    """What a node's tokenizer makes of what is asked."""
    tokenizer = TOKENIZERS.get(type_of(node))
    if tokenizer is None:
        raise ValueError(f"nothing tokenizes a {type_of(node)}")
    entry = tokenizer.derived.get(derived) if derived else tokenizer.whole
    if entry is None:
        if derived is None:
            raise ValueError("it has no value as a whole")
        listed = ", ".join(tokenizer.derived)
        raise ValueError(f"it derives no {derived}; it derives {listed}" if listed else f"it derives no {derived}")
    missing = next((arg for arg in entry.args if arg not in args), None)
    if missing:
        raise ValueError(f"it needs {missing}")
    return entry.make(reader, node, args)
