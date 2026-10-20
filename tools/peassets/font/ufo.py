"""The parts of UFO 3 (unifiedfontobject.org) a face is written in, and read back from.

    write(path, font)   font = {"info": {...}, "lib": {...}, "groups": {...}, "kerning": {...},
                                "glyphs": [{"name", "advance", "unicode", "image", "note", "lib"}],
                                "images": {file: png}}
    font = read(path)   the same, less the images, which are never read back

Glyph files are named by the user-name-to-file-name convention (an uppercase letter is followed by "_", reserved
characters become "_"), kept unique case-insensitively.
"""
from __future__ import annotations

import plistlib
import shutil
import xml.etree.ElementTree as ET
from pathlib import Path

RESERVED = set('"*+/:<>?[\\]|') | {chr(c) for c in range(0x20)} | {"\x7f"}


def glyph_file_names(names: list[str]) -> dict[str, str]:
    out, taken = {}, set()
    for name in names:
        base = "".join("_" if c in RESERVED else c + "_" if c.isupper() else c for c in name)
        if base.startswith("."):
            base = "_" + base[1:]
        stem, n = base, 1
        while stem.lower() in taken:
            stem, n = f"{base}{n:02d}", n + 1
        taken.add(stem.lower())
        out[name] = f"{stem}.glif"
    return out


def _plist(path: Path, value) -> None:
    path.write_bytes(plistlib.dumps(value, sort_keys=False))


def _lib_element(parent: ET.Element, lib: dict) -> None:
    """A glyph's lib, as a plist dict inside <lib>."""
    lib_el = ET.SubElement(parent, "lib")
    lib_el.append(ET.fromstring(plistlib.dumps(lib, sort_keys=False).split(b"<plist version=\"1.0\">", 1)[1]
                                .rsplit(b"</plist>", 1)[0].strip()))


def _glif(glyph: dict) -> bytes:
    el = ET.Element("glyph", name=glyph["name"], format="2")
    ET.SubElement(el, "advance", width=str(glyph["advance"]))
    if glyph.get("unicode") is not None:
        ET.SubElement(el, "unicode", hex=f"{glyph['unicode']:04X}")
    if glyph.get("note"):
        ET.SubElement(el, "note").text = glyph["note"]
    if glyph.get("image"):
        image = glyph["image"]
        ET.SubElement(el, "image", fileName=image["file"], xOffset=str(image["x"]), yOffset=str(image["y"]))
    if glyph.get("lib"):
        _lib_element(el, glyph["lib"])
    ET.indent(el, "  ")
    return b"<?xml version='1.0' encoding='UTF-8'?>\n" + ET.tostring(el, encoding="utf-8") + b"\n"


def write(path: Path, font: dict) -> None:
    if path.exists():
        shutil.rmtree(path)
    (path / "glyphs").mkdir(parents=True)
    _plist(path / "metainfo.plist", {"creator": "org.pe2decomp.peassets", "formatVersion": 3, "formatVersionMinor": 0})
    _plist(path / "fontinfo.plist", font["info"])
    _plist(path / "lib.plist", font["lib"])
    _plist(path / "groups.plist", font["groups"])
    _plist(path / "kerning.plist", font["kerning"])
    _plist(path / "layercontents.plist", [["public.default", "glyphs"]])
    files = glyph_file_names([g["name"] for g in font["glyphs"]])
    _plist(path / "glyphs" / "contents.plist", files)
    for glyph in font["glyphs"]:
        (path / "glyphs" / files[glyph["name"]]).write_bytes(_glif(glyph))
    if font.get("images"):
        (path / "images").mkdir()
        for name, png in font["images"].items():
            (path / "images" / name).write_bytes(png)


def _read_glif(data: bytes) -> dict:
    el = ET.fromstring(data)
    glyph = {"name": el.get("name"), "advance": 0, "unicode": None, "image": None, "note": None, "lib": {}}
    for child in el:
        if child.tag == "advance":
            glyph["advance"] = int(float(child.get("width", "0")))
        elif child.tag == "unicode" and glyph["unicode"] is None:
            glyph["unicode"] = int(child.get("hex"), 16)
        elif child.tag == "image":
            glyph["image"] = {"file": child.get("fileName"), "x": int(float(child.get("xOffset", "0"))),
                              "y": int(float(child.get("yOffset", "0")))}
        elif child.tag == "note":
            glyph["note"] = child.text or ""
        elif child.tag == "lib":
            body = b"".join(ET.tostring(c) for c in child)
            glyph["lib"] = plistlib.loads(b'<?xml version="1.0" encoding="UTF-8"?>\n<plist version="1.0">'
                                          + body + b"</plist>")
    return glyph


def read(path: Path) -> dict:
    def plist(name: str, default):
        p = path / name
        return plistlib.loads(p.read_bytes()) if p.exists() else default
    files = plist("glyphs/contents.plist", {})
    return {"info": plist("fontinfo.plist", {}), "lib": plist("lib.plist", {}), "groups": plist("groups.plist", {}),
            "kerning": plist("kerning.plist", {}),
            "glyphs": [_read_glif((path / "glyphs" / f).read_bytes()) for f in files.values()]}
