#!/usr/bin/env python3
"""Stage 0's files, read from a release's disc as its declarations say.

A disc's `STAGE0.HED` lists each file's sector in `STAGE0.CDF`, and a file is its chunks, one for each of the slots its
declaration lists, in order. A package is stored LZSS-compressed.
"""
from __future__ import annotations

import sys
import tomllib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "peassets"))
from format import FILE_CHUNK_HEADER_SIZE, FileChunkEndFlag, SECTOR_SIZE, parse_file_chunk_header, unpack_chunk_payload  # noqa: E402
from lzss import decode_lzss, trim_lzss  # noqa: E402

from declarations import DECLARATIONS, Declarations, uid_of  # noqa: E402

HED_STREAMS = 3 * 0x28
HED_ENTRY = 8
HED_END = 0xFFFFFFFF


class Stage0:
    def __init__(self, declarations: Declarations, rom: Path, release: str) -> None:
        self.declarations = declarations
        layout = tomllib.loads((DECLARATIONS / "disc" / release / "stage0.toml").read_text(encoding="utf-8"))
        self.folder = rom / release / layout["at"][0]["disc"]
        self.files = [self.folder / name for name in layout["at"][0]["slots"]]
        # Each file group's number, as many ten thousands of its files' ids, by the group's uid.
        self.groups: dict[str, int] = {uid_of(group["asset"])[0]: group["id"] for group in layout.get("groups", [])}
        self.sectors: dict[int, int] = {}
        hed = self.files[0].read_bytes()
        for at in range(HED_STREAMS, len(hed), HED_ENTRY):
            file = int.from_bytes(hed[at:at + 4], "little")
            if file == HED_END:
                break
            self.sectors[file] = int.from_bytes(hed[at + 4:at + 8], "little")
        self.cdf = self.files[1].read_bytes()
        self.read: dict[tuple[int, str], bytes] = {}

    def slot(self, file: int, name: str) -> bytes:
        """A file's slot, as it is loaded: a package inflated."""
        if (file, name) not in self.read:
            found = self.declarations.stage0_file(file)
            if found is None or file not in self.sectors:
                raise SystemExit(f"no stage 0 file {file}")
            slots = [slot["name"] for slot in found[0].table["slots"]]
            if name not in slots:
                raise SystemExit(f"{found[0].path} has no slot {name}")
            chunk = self.chunks(file)[slots.index(name)]
            self.read[file, name] = decode_lzss(trim_lzss(chunk)) if found[0].table["slots"][slots.index(name)]["type"] == "package" else chunk
        return self.read[file, name]

    def chunks(self, file: int) -> list[bytes]:
        """A file's chunks' payloads, in order."""
        at, out = self.sectors[file] * SECTOR_SIZE, []
        while True:
            header = parse_file_chunk_header(self.cdf[at:at + FILE_CHUNK_HEADER_SIZE])
            if header is None:
                raise SystemExit(f"file {file}: no chunk at {at:#x}")
            out.append(unpack_chunk_payload(self.cdf[at:at + header.chunk_size], header.sector_len))
            at += header.chunk_size
            if header.end_flag != FileChunkEndFlag.Continue:
                return out
