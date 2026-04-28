import argparse
from pathlib import Path


def iter_dir(dir: Path, pkg_list: list[tuple[Path, bytes]]):
    for entry in dir.iterdir():
        if entry.is_dir():
            iter_dir(entry, pkg_list)
        elif entry.is_file() and entry.suffix == ".pe2pkg":
            with open(entry, "rb") as f:
                pkg = f.read()
                for _, old_pkg in pkg_list:
                    if pkg == old_pkg:
                        break
                else:
                    pkg_list.append((entry, pkg))


def main():
    parser = argparse.ArgumentParser(description="Find all unique pkgs")
    parser.add_argument("dir", type=Path, help="Directory to search")
    args = parser.parse_args()

    pkg_list: list[tuple[Path, bytes]] = []
    iter_dir(args.dir, pkg_list)

    print(f"Found {len(pkg_list)} pkgs:")
    for entry, pkg in pkg_list:
        print(f"\t{entry}")
        # print(f"\t{entry}: 0x{len(pkg) // 1024:x} KB")


if __name__ == "__main__":
    main()
