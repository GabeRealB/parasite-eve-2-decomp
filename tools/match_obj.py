import argparse
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO

from elftools.elf.elffile import ELFFile
from elftools.elf.sections import Section
from Levenshtein import distance


@dataclass
class ObjMatch:
    path: Path
    offset: int
    length: int
    score: float


def match_obj(path: Path, data: bytes, data_start: int, matches: list[ObjMatch]):
    with open(path, "rb") as f:
        elffile = ELFFile(f)
        text = elffile.get_section_by_name(".text")
        if text is None:
            return
        assert isinstance(text, Section)

        offset = 0
        section_data = text.data()
        if data_start % text.data_alignment != 0:
            offset = text.data_alignment - (data_start % text.data_alignment)

        best_score = 0.0
        best_offset = data_start + offset
        while offset < len(data):
            remaining = len(data) - offset
            if remaining < len(section_data):
                break

            data_slice = data[offset : len(section_data)]
            dist = distance(section_data, data_slice)
            score = (len(section_data) - dist) / len(section_data)
            if score > best_score:
                best_score = score
                best_offset = data_start + offset

            offset += text.data_alignment

        matches.append(ObjMatch(path, best_offset, len(section_data), best_score))


def iterate_dir(dir: Path, data: bytes, data_start: int, matches: list[ObjMatch]):
    for entry in dir.iterdir():
        if entry.is_dir():
            iterate_dir(entry, data, data_start, matches)
        elif entry.suffix == ".o":
            match_obj(entry, data, data_start, matches)


def main():
    parser = argparse.ArgumentParser(
        description="Find the best matching elf object in a file section"
    )
    parser.add_argument("exe", type=argparse.FileType("rb"))
    parser.add_argument("dir", type=Path)
    parser.add_argument("start", type=str)
    parser.add_argument("end", type=str)
    args = parser.parse_args()

    exe: BinaryIO = args.exe
    dir: Path = args.dir
    byte_start = int(args.start, 0)
    byte_end = int(args.end, 0)

    exe.seek(byte_start)
    exe_data = exe.read(byte_end - byte_start)
    matchs: list[ObjMatch] = []

    iterate_dir(dir, exe_data, byte_start, matchs)

    matchs.sort(key=lambda x: x.length, reverse=True)
    matchs.sort(key=lambda x: x.score, reverse=True)
    for match in matchs:
        if match.score < 0.1:
            continue
        print(
            f"{match.path} (offset=0x{match.offset:x}, length=0x{match.length:x}, score={match.score})"
        )


if __name__ == "__main__":
    main()
