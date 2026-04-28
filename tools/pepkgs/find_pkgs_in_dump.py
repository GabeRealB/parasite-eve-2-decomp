from argparse import ArgumentParser, FileType
from pathlib import Path
from typing import BinaryIO


def main():
    parser = ArgumentParser()
    parser.add_argument("-d", "--dir", type=Path, help="Package directory")
    parser.add_argument(
        "-a", "--address", type=str, help="Address that must be contained"
    )
    parser.add_argument("file", type=FileType("rb"))
    args = parser.parse_args()

    dir: Path = args.dir
    file: BinaryIO = args.file
    file_data = file.read()

    address: None | int = None
    if args.address is not None:
        address = int(args.address, 0) - 0x80000000

    matches: list[tuple[Path, int, list[int]]] = []
    for entry in dir.iterdir():
        if not entry.is_file:
            pass

        with entry.open("rb") as f:
            pkg_data = f.read()
            slice_len = min(len(pkg_data), 1024)
            slice = pkg_data[:slice_len]

            start = 0x10000 + 0x597FF
            positions = []
            while True:
                pos = file_data.find(slice, start)
                if pos == -1:
                    break
                positions.append(pos)
                start = pos + 1

            if len(positions) != 0:
                matches.append((entry, len(pkg_data), positions))
    matches.sort(key=lambda x: len(str(x[0])))

    print("Found matches:")
    for path, size, positions in matches:
        for position in positions:
            if address is not None and not position <= address <= position + size:
                continue

            print(f"\t{path} ({size} B): 0x{position:x} - 0x{position + size:x}")


if __name__ == "__main__":
    main()
