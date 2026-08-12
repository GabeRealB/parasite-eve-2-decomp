import argparse
import json
from multiprocessing import shared_memory
from pathlib import Path

from Levenshtein import distance


def _room_pkgs_by_load_addr(stages: dict) -> dict[str, list[tuple[str, str]]]:
    """load_addr → [(store_name, path), …] unique by path."""
    out: dict[str, list[tuple[str, str]]] = {}
    seen: set[str] = set()

    def consider(path: str | None, load_addr: str) -> None:
        if not path or path in seen:
            return
        seen.add(path)
        stem = Path(path).stem
        out.setdefault(load_addr, []).append((stem, path))

    for stage in stages.values():
        if not isinstance(stage, dict):
            continue
        files = stage.get("files")
        if isinstance(files, dict):
            for chunks in files.values():
                if not isinstance(chunks, dict):
                    continue
                for ent in chunks.values():
                    if isinstance(ent, dict) and ent.get("type") == "room_pkg":
                        consider(ent.get("path"), ent.get("load_addr") or "0x0")
            continue
        folders = stage.get("folders")
        if not isinstance(folders, dict):
            continue
        for folder_files in folders.values():
            if not isinstance(folder_files, dict):
                continue
            for chunks in folder_files.values():
                if not isinstance(chunks, dict):
                    continue
                for ent in chunks.values():
                    if isinstance(ent, dict) and ent.get("type") == "room_pkg":
                        consider(ent.get("path"), ent.get("load_addr") or "0x0")
    return out


def main():
    parser = argparse.ArgumentParser(description="Scans a shared memory map for pkgs")
    parser.add_argument("name", type=str, help="Memory map name")
    parser.add_argument("dir", type=Path, help="Assets directory")
    args = parser.parse_args()

    asset_path = args.dir
    with open(asset_path / "stages.json", "r") as f:
        stages = json.load(f)
    by_addr = _room_pkgs_by_load_addr(stages)

    memory = shared_memory.SharedMemory(name=args.name, create=False)
    assert memory.buf is not None
    ram_memory = memory.buf[0:0x200000]

    SECTOR_BUFFER = 0x0006C4D8
    SECTOR_BUFFER_SIZE = 0x800
    CHUNK_HEADER_SIZE = 0x10
    CHUNK_DATA_SIZE = SECTOR_BUFFER_SIZE - CHUNK_HEADER_SIZE

    sector_data = ram_memory[
        SECTOR_BUFFER : SECTOR_BUFFER + SECTOR_BUFFER_SIZE
    ].tobytes()
    header_data = sector_data[:CHUNK_HEADER_SIZE]
    asset_data = sector_data[CHUNK_HEADER_SIZE:]
    print("Chunk:")
    match int.from_bytes(header_data[0x0:0x1], byteorder="little"):
        case 0:
            print("\tchunkType (0x0-0x1): Pkg")
        case 1:
            print("\tchunkType (0x0-0x1): Image")
        case 2:
            print("\tchunkType (0x0-0x1): Clut")
        case 4:
            print("\tchunkType (0x0-0x1): Cap2")
        case 5:
            print("\tchunkType (0x0-0x1): Background")
        case 6:
            print("\tchunkType (0x0-0x1): Music")
        case 7:
            print("\tchunkType (0x0-0x1): Ascii")
        case x:
            print(f"\tchunkType (0x0-0x1): Unknown ({x})")
    print(
        f"\tendFlag   (0x1-0x2): {int.from_bytes(header_data[0x1:0x2], byteorder='little')}"
    )
    print(
        f"\tfield_02  (0x2-0x4): {int.from_bytes(header_data[0x2:0x4], byteorder='little')}"
    )
    print(
        f"\tchunkSize (0x4-0x8): {int.from_bytes(header_data[0x4:0x8], byteorder='little')}"
    )
    load_addr_str = f"0x{int.from_bytes(header_data[0x8:0xC], byteorder='little'):X}"
    print(
        f"\tloadAddr  (0x8-0xC): 0x{int.from_bytes(header_data[0x8:0xC], byteorder='little'):X}"
    )

    candidate_bytes = {}
    for store_name, rel in by_addr.get(load_addr_str, []):
        pkg_file = asset_path / rel
        if not pkg_file.exists():
            continue
        with open(pkg_file, "rb") as f:
            candidate_bytes[store_name] = f.read()[:CHUNK_DATA_SIZE]

    candidate_scores = {}
    for candidate, cand_bytes in candidate_bytes.items():
        score = distance(asset_data, cand_bytes)
        candidate_scores[candidate] = 1.0 - (score / CHUNK_DATA_SIZE)

    candidate_scores = dict(
        sorted(candidate_scores.items(), key=lambda item: item[1], reverse=True)
    )
    print()
    print("Scores:")
    prev_score = 0
    for candidate, score in candidate_scores.items():
        if prev_score == 1 and score != 1:
            break
        prev_score = score
        print(f"\t{candidate}: {score * 100:.2f}%")

    ram_memory.release()
    memory.close()


if __name__ == "__main__":
    main()
