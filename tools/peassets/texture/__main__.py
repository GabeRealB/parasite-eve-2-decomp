"""python3 tools/peassets/texture decode|encode|check [--assets assets/USA] [--only FILE] [--out DIR]

decode   write each declared file's textures, every variant as a PNG, and each atlas's layout as TOML
encode   encode each declared file's image and palette chunks from those PNGs into --out
check    encode them and compare with the stored chunks before LZSS
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

PEASSETS = Path(__file__).resolve().parents[1]
if __package__ in (None, ""):                   # run as a directory: the package is texture, under peassets
    sys.path[0] = str(PEASSETS)
    __package__ = "texture"

from texture.files import (check_file, declared_atlases, declared_files, decode_atlas, decode_file,  # noqa: E402
                           encode_file)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=("decode", "encode", "check"))
    ap.add_argument("--assets", type=Path, default=PEASSETS.parents[1] / "assets/USA")
    ap.add_argument("--only")
    ap.add_argument("--out", type=Path, help="where encode writes the chunks")
    args = ap.parse_args(argv)
    import asset_data
    failed = 0
    for name in declared_files(asset_data.ASSETS):
        if args.only and name != args.only:
            continue
        if args.command == "decode":
            for path in decode_file(name, asset_data.ASSETS, args.assets):
                print(path.relative_to(args.assets))
        elif args.command == "encode":
            if args.out is None:
                ap.error("encode needs --out")
            args.out.mkdir(parents=True, exist_ok=True)
            for chunk, data in encode_file(name, asset_data.ASSETS, args.assets).items():
                path = args.out / f"{chunk}.{asset_data.ASSETS[chunk]['type']}"
                path.write_bytes(data)
                print(path)
        else:
            problems = check_file(name, asset_data.ASSETS, args.assets)
            print(f"{name}: " + ("chunks rebuild from the textures" if not problems else "; ".join(problems)))
            failed += bool(problems)
    if args.command == "decode":
        for name in declared_atlases(asset_data.ASSETS):
            if not args.only or name == args.only:
                print(decode_atlas(name, asset_data.ASSETS, args.assets).relative_to(args.assets))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
