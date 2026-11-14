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
    # The faces may read the texture deeper than its image is stored, where the material says (`bits`).
    bits = material.table.get("bits", BITS.get(image.table.get("mode"), 16))
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
    return "{ " + ", ".join(hexadecimal(word) if word > 0x7FFFFFFF else str(word) for word in words) + " }"


def image_size(image: Declaration) -> list[int]:
    if "size" not in image.table:
        raise ValueError("it has no size")
    return image.table["size"]


def image_width_words(_: Reader, image: Declaration, __: Args) -> str:
    return str(image_size(image)[0] // WORDS_PER.get(image.table.get("mode"), 1))


def image_height(_: Reader, image: Declaration, __: Args) -> str:
    return str(image_size(image)[1])


def image_placed(reader: Reader, image: Declaration) -> tuple[int, int]:
    """Where an image is in VRAM, in halfwords: its `page` placement."""
    if "page" not in image.table:
        raise ValueError("it is placed nowhere")
    return reader.placed(image.table["page"])


def image_page(reader: Reader, image: Declaration) -> tuple[int, int]:
    """The texture page an image is in: its VRAM origin, in halfwords."""
    x, y = image_placed(reader, image)
    return x // 64 * 64, y // 256 * 256


def image_page_x(reader: Reader, image: Declaration, _: Args) -> str:
    return str(image_page(reader, image)[0])


def image_page_y(reader: Reader, image: Declaration, _: Args) -> str:
    return str(image_page(reader, image)[1])


def image_depth(_: Reader, image: Declaration, __: Args) -> str:
    """A texture page's depth as the GPU is told it: 0 for 4 bits, 1 for 8, 2 for 16."""
    return {4: "0", 8: "1"}.get(BITS.get(image.table.get("mode"), 16), "2")


def image_palette(reader: Reader, image: Declaration) -> tuple[int, int]:
    """Where an image's `default` palette is in VRAM, else its first's."""
    palettes = image.table.get("palettes", {})
    if not palettes:
        raise ValueError("it has no palettes")
    return reader.placed(palettes.get("default", next(iter(palettes.values()))))


def image_palette_x(reader: Reader, image: Declaration, _: Args) -> str:
    return str(image_palette(reader, image)[0])


def image_palette_y(reader: Reader, image: Declaration, _: Args) -> str:
    return str(image_palette(reader, image)[1])


def image_cell(reader: Reader, image: Declaration, args: Args) -> tuple[int, int, int, int]:
    """A cell's rectangle in its page's texels: where the image sits in the page, and the cell in the image."""
    name = one(args, "cell")
    cell = image.table.get("cells", {}).get(name)
    if cell is None:
        raise ValueError(f"it has no cell {name}")
    x, y, w, h = cell["rect"]
    (placed_x, placed_y), (page_x, page_y) = image_placed(reader, image), image_page(reader, image)
    return (placed_x - page_x) * (16 // BITS.get(image.table.get("mode"), 16)) + x, (placed_y - page_y) + y, w, h


def of_cell(at: int) -> Callable[[Reader, Declaration, Args], str]:
    return lambda reader, image, args: str(image_cell(reader, image, args)[at])


def list_count(_: Reader, listed: Declaration, __: Args) -> str:
    """A list's items."""
    return str(len(listed.table.get("items", [])))


def list_step(reader: Reader, listed: Declaration, _: Args) -> str:
    """What each item's id is more than the one before it, the same for each: what code finding an item by its index
    from the first's adds. A list of fewer than two has none, and is 0."""
    ids = []
    for at, reference in enumerate(listed.table.get("items", [])):
        node, _, args = reached(reader.declarations, reference, ())
        text = tokens(reader, node, "id", args)
        if not text.strip().lstrip("-").isdigit():
            raise ValueError(f"item {at}'s id is {text}, no number")
        ids.append(int(text))
    if len(ids) < 2:
        return "0"
    step = ids[1] - ids[0]
    for before, after in zip(ids, ids[1:]):
        if after - before != step:
            raise ValueError(f"its items' ids are not evenly apart: {before} to {after} is not {step}")
    return str(step)


def group_file_id(reader: Reader, group: Declaration, args: Args) -> str:
    """A file of a group's id: the group's number in its stage's layout, as many ten thousands, and the file's in the
    group."""
    name = one(args, "file")
    file = next((file for file in group.table.get("files", []) if file.get("name") == name), None)
    if file is None:
        raise ValueError(f"it has no file {name}")
    if group.uid not in reader.disc.groups:
        raise ValueError(f"stage 0 of {reader.release} lists no {group.path}")
    return str(reader.disc.groups[group.uid] * 10000 + file["id"])


def file_stream_id(_: Reader, file: Declaration, args: Args) -> str:
    """A stream of a file's id, as its record has it: a movie in a view's is the view, a cutscene's 100 and its place
    among the file's cutscenes (its movies in no view), a scene's its own."""
    name = one(args, "stream")
    cutscenes = 0
    for stream in file.table.get("streams", []):
        movie = stream.get("type") == "movie"
        stream_id = stream.get("view", 100 + cutscenes) if movie else stream["id"]
        if stream.get("name") == name:
            return str(stream_id)
        if movie and "view" not in stream:
            cutscenes += 1
    raise ValueError(f"it has no stream {name}")


def of_member(make: Callable[[Reader, Declaration, Args], str]) -> Callable[[Reader, Declaration, Args], str]:
    """A prefab's member is a model, as the prefab holds it."""
    return lambda reader, prefab, args: make(reader, reader.member(prefab, one(args, "member")), args)


def clip_name(path: str) -> str:
    """A clip's C name, from its path: its steps after the first, in words (`characters/aya/handgun/walk` ->
    `_gAyaHandgunWalkClip`)."""
    words = [word for step in path.split("/")[1:] for word in step.split("_")]
    return "_g" + "".join(word[:1].upper() + word[1:] for word in words) + "Clip"


SET_SIZE = 0x28
ANGLE_STEP = 8


def _signed(value: int, bits: int) -> int:
    value &= (1 << bits) - 1
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def clip_definition(name: str, stored: bytes, address: int) -> str:
    """A clip's arrays and its `AnimationSet`, as one line of C, its arrays named after it: what the disc stores, read
    as its pointers lay it out."""
    at = len(stored) - SET_SIZE
    words = struct.unpack_from("<10I", stored, at)
    records_at, starts_at = words[0] - address, words[1] - address
    banks = {kind: words[2 + kind] - address for kind in range(8) if words[2 + kind]}
    ends = sorted([*banks.values(), records_at])
    out, named = [], []
    for kind in sorted(banks, key=banks.get):
        start = banks[kind]
        end = ends[ends.index(start) + 1]
        if kind == 1:
            rows = ["{ " + ", ".join(map(str, pose)) + " }" for pose in struct.iter_unpack("<6h", stored[start:end])]
            out.append(f"static AnimationPackedPose {name}Bank1[] = {{ {', '.join(rows)} }};")
        else:
            rows = ["{ " + ", ".join(str(_signed(word >> shift, bits)) for shift, bits in ((0, 11), (11, 10), (21, 11)))
                    + " }" for word, in struct.iter_unpack("<I", stored[start:end])]
            out.append(f"static AnimationPackedRotation {name}Bank{kind}[] = {{ {', '.join(rows)} }};")
        named.append(f"[{kind}] = {name}Bank{kind}")
    records = [f"{{ {offset}, {frames}, 0x{flags:02X} }}"
               for offset, frames, flags in struct.iter_unpack("<HBB", stored[records_at:starts_at])]
    out.append(f"static AnimationRecord {name}Records[] = {{ {', '.join(records)} }};")
    starts = struct.unpack_from(f"<{(at - starts_at) // 2}H", stored, starts_at)
    out.append(f"static u16 {name}TrackStarts[] = {{ {', '.join(map(str, starts))} }};")
    out.append(f"static AnimationSet {name} = {{ .records = {name}Records, .trackStartIndices = {name}TrackStarts, "
               f".poseBanks = {{ {', '.join(named)} }} }};")
    return " ".join(out)


def bank_whole(reader: Reader, bank: Declaration, args: Args) -> str:
    """A bank as the game stores it: every clip it stores, then `AnimationSet* <symbol>[capacity]` naming each slot's.
    Its slots are its schema's, each filled by the nearest bank of its chain (itself, then its `base`s) binding it; it
    stores its `order`, then what its slots reach that the order leaves out."""
    chain = [bank]
    while "base" in chain[-1].table:
        if len(chain) > 32:
            raise ValueError("its bases go round")
        chain.append(reader.declarations.named(chain[-1].table["base"]))
    schema = next((reader.declarations.named(b.table["schema"]) for b in chain if "schema" in b.table), None)
    if schema is None:
        raise ValueError("no bank it is laid over names a schema")
    filled = []
    for slot in (name for name in schema.table.get("names", []) if name):
        clip = next((b.table["clips"][slot] for b in chain if slot in b.table.get("clips", {})), None)
        if clip is not None:
            filled.append((slot, uid_of(clip)[0]))
    stored = [uid_of(clip)[0] for clip in bank.table.get("order", [])]
    stored += [clip for clip in dict.fromkeys(clip for _, clip in filled) if clip not in stored]
    names, out = {}, []
    for clip in stored:
        declaration = reader.declarations.named(clip)
        names[clip] = clip_name(declaration.path)
        out.append(clip_definition(names[clip], *reader.stored(declaration)))
    prefix, capacity = schema.table.get("prefix", ""), schema.table["capacity"]
    entries = ", ".join(f"[{prefix}{slot.upper()}] = &{names[clip]}" for slot, clip in filled)
    out.append(f"AnimationSet* {one(args, 'symbol')}[{capacity}] = {{ {entries} }};")
    return " ".join(out)


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
    "animation_bank": Tokenizer(whole=Entry(bank_whole, ("symbol",))),
    "list": Tokenizer(derived={"count": Entry(list_count), "step": Entry(list_step)}),
    "file_group": Tokenizer(derived={"id": Entry(group_file_id, ("file",))}),
    "file": Tokenizer(derived={"id": Entry(file_stream_id, ("stream",))}),
    "material": Tokenizer(derived={"tpage": Entry(material_tpage), "clut": Entry(material_clut)}),
    "image": Tokenizer(
        whole=Entry(image_words),
        derived={
            "width_words": Entry(image_width_words),
            "height": Entry(image_height),
            "page_x": Entry(image_page_x),
            "page_y": Entry(image_page_y),
            "depth": Entry(image_depth),
            "palette_x": Entry(image_palette_x),
            "palette_y": Entry(image_palette_y),
            **{name: Entry(of_cell(at), ("cell",)) for at, name in enumerate(("u", "v", "w", "h"))},
        },
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


def led_on(node: Declaration, output: str | None) -> tuple[Declaration, str | None, Args]:
    """What an output of a node leads to: a member of a prefab's group is a model read from the prefab, a file of a
    group (`files/<name>`) the group read as that file."""
    if output is None:
        return node, None, {}
    if type_of(node) == "prefab" and node.table.get("groups") is not None:
        return node, "model", {"member": [output]}
    if type_of(node) == "file_group" and output.startswith("files/"):
        name = output.removeprefix("files/")
        if not any(file.get("name") == name for file in node.table.get("files", [])):
            raise ValueError(f"{node.path} has no file {name}")
        return node, "file", {"file": [name]}
    raise ValueError(f"{node.path} has no output {output}")


def reached(declarations: Declarations, reference: str | dict, path: tuple[str, ...]) -> tuple[Declaration, str | None, Args]:
    """Where a reference and a sub-path come to: a node, what it reads as here, and the args it is read by. The
    reference's output leads on (`led_on`); then a cell of an image is the image told which (`cell/<name>`), a stream of
    a file the file told which (`streams/<name>`), and an item of a list (`items/<index>`) goes on from what the item
    names."""
    node, reads_as, args = led_on(declarations.named(reference), uid_of(reference)[1])
    if not path:
        return node, reads_as, args
    step, rest = path[:2], path[2:]
    if reads_as is None and len(step) == 2 and type_of(node) == "image" and step[0] == "cell" and not rest:
        if step[1] not in node.table.get("cells", {}):
            raise ValueError(f"{node.path} has no cell {step[1]}")
        return node, "image", {"cell": [step[1]]}
    if reads_as is None and len(step) == 2 and type_of(node) == "file" and step[0] == "streams" and not rest:
        if not any(stream.get("name") == step[1] for stream in node.table.get("streams", [])):
            raise ValueError(f"{node.path} has no stream {step[1]}")
        return node, "file", {"stream": [step[1]]}
    if reads_as is None and len(step) == 2 and type_of(node) == "list" and step[0] == "items":
        items = node.table.get("items", [])
        if not step[1].isdigit() or int(step[1]) >= len(items):
            raise ValueError(f"{node.path} has {len(items)} items, and no item {step[1]}")
        return reached(declarations, items[int(step[1])], rest)
    raise ValueError(f"nothing goes on from {node.path}")
