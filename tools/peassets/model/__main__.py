"""python3 tools/peassets/model decode [--assets assets/USA] [--only NAME] [--usdc]
python3 tools/peassets/model preview FAMILY_OR_PACKAGE [--assets assets/USA] [--out build/models]

decode    write each declared model and its bindings as USD (.usda; .usdc too with --usdc)
preview   write every model of a manifest family (kyle, weapons, ...) or one package as plain USD, to look at
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

PEASSETS = Path(__file__).resolve().parents[1]
if __package__ in (None, ""):                   # run as a directory: the package is model, under peassets
    sys.path[0] = str(PEASSETS)
    __package__ = "model"

from model import preview  # noqa: E402
from model.declared import declared, decode  # noqa: E402


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=("decode", "preview"))
    ap.add_argument("name", nargs="?", help="preview: a manifest family or a package")
    ap.add_argument("--assets", type=Path, default=PEASSETS.parents[1] / "assets/USA")
    ap.add_argument("--only", help="decode: one declared model")
    ap.add_argument("--usdc", action="store_true", help="decode: also write the binary form")
    ap.add_argument("--out", type=Path, default=PEASSETS.parents[1] / "build/models", help="preview: where to write")
    args = ap.parse_args(argv)
    if args.command == "preview":
        if not args.name:
            ap.error("preview needs a family or a package")
        written = preview.write(args.name, args.assets, args.out)
        print(f"wrote {len(written)} model(s) under {args.out}/")
        return 0
    import asset_data
    for name in declared(asset_data.ASSETS):
        if args.only and name != args.only:
            continue
        out = decode(name, asset_data.ASSETS, asset_data.EMBEDDED_ASSETS, args.assets, args.usdc)
        print(f"{name} -> {out.relative_to(args.assets)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
