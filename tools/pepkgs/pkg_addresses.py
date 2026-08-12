import argparse
import json
from pathlib import Path


def _iter_room_pkgs(stages: dict):
    """Yield (canonical_hint, path, load_addr) for room_pkg entries in stages.json."""
    for stage_name, stage in stages.items():
        if not isinstance(stage, dict):
            continue
        files = stage.get("files")
        if isinstance(files, dict):
            for file_name, chunks in files.items():
                if not isinstance(chunks, dict):
                    continue
                for chunk_key, ent in chunks.items():
                    if not isinstance(ent, dict) or ent.get("type") != "room_pkg":
                        continue
                    yield (
                        f"{stage_name}/{file_name}/{chunk_key}",
                        ent.get("path"),
                        ent.get("load_addr") or "0x0",
                    )
            continue
        folders = stage.get("folders")
        if not isinstance(folders, dict):
            continue
        for folder_name, folder_files in folders.items():
            if not isinstance(folder_files, dict):
                continue
            for file_name, chunks in folder_files.items():
                if not isinstance(chunks, dict):
                    continue
                for chunk_key, ent in chunks.items():
                    if not isinstance(ent, dict) or ent.get("type") != "room_pkg":
                        continue
                    yield (
                        f"{stage_name}/{folder_name}/{file_name}/{chunk_key}",
                        ent.get("path"),
                        ent.get("load_addr") or "0x0",
                    )


def main():
    parser = argparse.ArgumentParser(
        description="Prints out a summary of all pe2pkg load addresses"
    )
    parser.add_argument("dir", type=Path, help="Assets directory")
    args = parser.parse_args()

    stages_path = args.dir / "stages.json"
    with open(stages_path, "r") as f:
        stages = json.load(f)

    address_map: dict[str, list[dict]] = {}
    seen_paths: set[str] = set()
    for name, path, load_addr in _iter_room_pkgs(stages):
        if not path or path in seen_paths:
            continue
        seen_paths.add(path)
        info = {"name": name, "path": path}
        address_map.setdefault(load_addr, []).append(info)

    for load_addr, entries in address_map.items():
        print(load_addr)
        for entry in entries:
            print(f"\t{entry['name']} ({entry['path']})")
        print()


if __name__ == "__main__":
    main()
