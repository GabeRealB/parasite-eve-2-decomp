#!/usr/bin/env python3
"""Hold what the editor deployed of a project's build to the checksums the ninja build is held to.

    check_project_build.py DEPLOYED CHECKSUMS --executable NAME --stamp OUT

DEPLOYED is the folder the editor's build writes a configuration's images to (`<project>/build/<config>/`, by
`pe2-cli project build`), CHECKSUMS the release's `configs/<release>/checksum.sha`, whose images are named as the ninja
build writes them (`build/<release>/out/<name>`). A package the editor deploys as `<name>.pe2pkg` is the ninja build's
`<name>`, and its executable `<name>.exe` is the release's (`--executable`). It fails on an image whose SHA-256 differs
and on one the checksums do not know, and says how many of the checksums' images the editor built; OUT is written once
everything deployed matches.
"""
from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path


def checksums(path: Path) -> dict[str, str]:
    """Each image's SHA-256, by the name the ninja build gives it."""
    found = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.strip():
            digest, image = line.split(maxsplit=1)
            found[Path(image.strip()).name] = digest
    return found


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("deployed", type=Path)
    ap.add_argument("checksums", type=Path)
    ap.add_argument("--executable", required=True, help="the release's executable, as the checksums name it")
    ap.add_argument("--stamp", type=Path, required=True)
    args = ap.parse_args()
    expected = checksums(args.checksums)
    if not args.deployed.is_dir():
        print(f"{args.deployed}: the editor deployed nothing (did its build run?)", file=sys.stderr)
        return 1
    matched, differ, unknown = [], [], []
    for image in sorted(args.deployed.iterdir()):
        if image.suffix not in (".pe2pkg", ".exe"):
            continue                                    # the compile database and what it refers to
        name = args.executable if image.suffix == ".exe" else image.stem
        if name not in expected:
            unknown.append(image.name)
        elif hashlib.sha256(image.read_bytes()).hexdigest() != expected[name]:
            differ.append(image.name)
        else:
            matched.append(name)
    for image in differ:
        print(f"{image}: FAILED", file=sys.stderr)
    for image in unknown:
        print(f"{image}: no checksum names it", file=sys.stderr)
    print(f"{len(matched)} of the {len(expected)} images the checksums name are built by the editor and match"
          + (f"; {len(differ)} differ" if differ else "") + (f"; {len(unknown)} unknown" if unknown else ""))
    if differ or unknown or not matched:
        return 1
    args.stamp.parent.mkdir(parents=True, exist_ok=True)
    args.stamp.write_text("\n".join(matched) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
