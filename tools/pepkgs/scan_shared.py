import argparse
import json
from multiprocessing import shared_memory
from pathlib import Path

from Levenshtein import distance


def main():
    parser = argparse.ArgumentParser(description="Scans a shared memory map for pkgs")
    parser.add_argument("name", type=str, help="Memory map name")
    parser.add_argument("dir", type=Path, help="Assets directory")
    args = parser.parse_args()

    asset_path = args.dir
    pkg_path = asset_path / "OVR"
    map_path = pkg_path / "map.json"
    with open(map_path, "r") as f:
        map_data = json.load(f)

    address_map = {}
    for name, entry in map_data.items():
        ovr_name = entry["ovr_name"]
        load_address = entry["load_address"]
        info = {
            "name": name,
            "ovr_name": ovr_name,
        }

        if load_address not in address_map:
            address_map[load_address] = [info]
        else:
            address_map[load_address].append(info)

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
    load_address_str = f"0x{int.from_bytes(header_data[0x8:0xC], byteorder='little'):X}"
    print(
        f"\tloadAddr  (0x8-0xC): 0x{int.from_bytes(header_data[0x8:0xC], byteorder='little'):X}"
    )

    candidate_bytes = {}
    for name, entry in map_data.items():
        ovr_name = entry["ovr_name"]
        if "DUPLICATE" in ovr_name:
            continue

        if load_address_str == entry["load_address"]:
            with open(asset_path / name, "rb") as f:
                candidate_bytes[ovr_name] = f.read()[:CHUNK_DATA_SIZE]

    candidate_scores = {}
    for candidate, candidate_bytes in candidate_bytes.items():
        score = distance(asset_data, candidate_bytes)
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
