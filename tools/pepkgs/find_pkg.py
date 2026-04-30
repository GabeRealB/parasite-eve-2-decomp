import argparse
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class GameVersionInfo:
    idx: int
    version_name: str
    version_dir: str


@dataclass
class Match:
    path: Path
    offset: int


GAME_VERSIONS = [
    GameVersionInfo(0, "USA", "USA"),
]

ASSETS_DIR = "assets"


def iter_dir(dir_path: Path, raw: bool, matches: list[Match], pattern: bytes):
    for entry in dir_path.iterdir():
        if entry.is_dir():
            iter_dir(entry, raw, matches, pattern)
        elif entry.is_file():
            if raw and entry.suffix != ".pe2pkg":
                continue

            with open(entry, "rb") as f:
                pkg_data = f.read()
                pos = pkg_data.find(pattern)
                if pos != -1:
                    matches.append(Match(path=entry, offset=pos))


def find_matches(version_idx: int, raw: bool, pattern: bytes) -> list[Match]:
    matches: list[Match] = []
    asset_path = (Path(ASSETS_DIR) / GAME_VERSIONS[version_idx].version_dir).absolute()
    if raw:
        iter_dir(asset_path, raw, matches, pattern)
    else:
        pkgs_path = asset_path / "OVR"
        iter_dir(pkgs_path, raw, matches, pattern)
    return matches


def main():
    parser = argparse.ArgumentParser(description="Find a byte sequence in a pkg")
    parser.add_argument(
        "-str",
        "--string",
        help="Interpret the sequence as a UTF-8 string",
        action="store_true",
    )
    parser.add_argument(
        "-raw",
        "--search_raw",
        help="Search in the uncompressed packages",
        action="store_true",
    )
    parser.add_argument(
        "-ver",
        "--game_version",
        help="Assume a specific version of the game",
        type=str,
    )
    parser.add_argument("sequence", type=str, help="The byte sequence to find")
    args = parser.parse_args()

    search_raw = args.search_raw
    game_version_option = 0  # USA by default
    if args.game_version is not None:
        for info in GAME_VERSIONS:
            if args.game_version.upper() == info.version_name:
                game_version_option = info.idx
                break
        else:
            print("Version not supported.")
            sys.exit(1)

    sequence: str = args.sequence
    if args.string:
        pattern = bytes(sequence, "utf-8")
    else:
        sequence = sequence.replace(" ", "")
        pattern = bytes.fromhex(sequence)

    matches = find_matches(game_version_option, search_raw, pattern)
    print("Found matches:")
    for match in matches:
        print(f"\t{match.path}: {match.offset}")


if __name__ == "__main__":
    main()
