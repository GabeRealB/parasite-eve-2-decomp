"""python3 tools/peassets/model decode|encode|check [--assets assets/USA] [--only NAME] [--usdc] [--out DIR]
python3 tools/peassets/model preview FAMILY_OR_PACKAGE [--assets assets/USA] [--out build/models]

decode    write each rig, derived from its models, and each declared model and its bindings as USD (.usda; .usdc
          too with --usdc)
encode    encode each declared model from its USD and binding into --out, as <name>.model
check     encode them and compare with the models in their packages
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
from model.declared import check, declared, decode, decode_rig, encode, rigs  # noqa: E402


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=("decode", "encode", "check", "preview"))
    ap.add_argument("name", nargs="?", help="preview: a manifest family or a package")
    ap.add_argument("--assets", type=Path, default=PEASSETS.parents[1] / "assets/USA")
    ap.add_argument("--only", help="decode: one declared model")
    ap.add_argument("--usdc", action="store_true", help="decode: also write the binary form")
    ap.add_argument("--out", type=Path, default=PEASSETS.parents[1] / "build/models", help="preview, encode: where to write")
    args = ap.parse_args(argv)
    if args.command == "preview":
        if not args.name:
            ap.error("preview needs a family or a package")
        written = preview.write(args.name, args.assets, args.out)
        print(f"wrote {len(written)} model(s) under {args.out}/")
        return 0
    import asset_data
    assets, embedded = asset_data.ASSETS, asset_data.EMBEDDED_ASSETS
    failed = 0
    if args.command == "decode":
        for rig in rigs(assets):
            print(f"{rig} -> {decode_rig(rig, assets, embedded, args.assets).relative_to(args.assets)}")
    for name in declared(assets):
        if args.only and name != args.only:
            continue
        if args.command == "decode":
            out = decode(name, assets, embedded, args.assets, args.usdc)
            print(f"{name} -> {out.relative_to(args.assets)}")
        elif args.command == "encode":
            args.out.mkdir(parents=True, exist_ok=True)
            path = args.out / f"{name}.model"
            path.parent.mkdir(parents=True, exist_ok=True)           # a grouped asset's name is a path
            path.write_bytes(encode(name, assets, embedded, args.assets))
            print(path)
        else:
            problems = check(name, assets, embedded, args.assets)
            print(f"{name}: " + ("rebuilds from its USD and binding" if not problems else "; ".join(problems)))
            failed += bool(problems)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
