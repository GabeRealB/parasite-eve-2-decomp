"""python3 tools/peassets/animation decode|encode|check [--assets assets/USA] [--only NAME] [--out DIR]
python3 tools/peassets/animation scene MODEL [--assets assets/USA] [--out FILE] [--blender PATH]

decode    write each declared clip as USD (.usda) in its group's directory
encode    encode each declared clip from its USD, and each bank from its declaration, into --out
check     encode them and compare with their bytes in their packages
scene     write a .blend with the declared model MODEL (textured, through its binding) and every clip of its rig as
          an action (blender_scene.py), by default build/previews/<MODEL>.blend; Blender is --blender, else
          $PE2_BLENDER, else `blender` (a Windows blender.exe from WSL is given Windows paths)
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

PEASSETS = Path(__file__).resolve().parents[1]
if __package__ in (None, ""):                   # run as a directory: the package is animation, under peassets
    sys.path[0] = str(PEASSETS)
    __package__ = "animation"

from animation import declared  # noqa: E402
from animation.declared import declared as declared_names  # noqa: E402


def scene(model: str, assets: dict, assets_root: Path, out: Path, blender: str) -> int:
    """Run blender_scene.py on `model`'s binding (else its USD) and every clip of its rig."""
    from model import declared as models
    rig = assets.get(model, {}).get("rig")
    if rig is None:
        raise SystemExit(f"{model}: not a declared model on a rig")
    binding = next((b for b, r in sorted(assets.items()) if r.get("type") == "binding" and r.get("model") == model), None)
    shown = models.source_path(binding or model, assets, assets_root)           # a binding is named like its file
    clips = [declared.source_path(c, assets, assets_root) for c in declared_names(assets) if assets[c].get("rig") == rig]
    windows = blender.lower().endswith(".exe") and os.path.exists("/proc/sys/fs/binfmt_misc/WSLInterop")
    host = (lambda p: subprocess.run(["wslpath", "-w", str(Path(p).resolve())], capture_output=True, text=True,
                                     check=True).stdout.strip()) if windows else (lambda p: str(Path(p).resolve()))
    out.parent.mkdir(parents=True, exist_ok=True)
    script = Path(__file__).with_name("blender_scene.py")
    run = subprocess.run([blender, "-b", "--factory-startup", "-P", host(script), "--", host(out), host(shown),
                          *map(host, clips)], capture_output=True, text=True)
    if run.returncode or not out.is_file():
        print(run.stdout[-2000:], run.stderr[-2000:], file=sys.stderr)
        return 1
    print(f"{out}: {model} and {len(clips)} clips of {rig}")
    return 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=("decode", "encode", "check", "scene"))
    ap.add_argument("model", nargs="?", help="scene: the declared model")
    ap.add_argument("--assets", type=Path, default=PEASSETS.parents[1] / "assets/USA")
    ap.add_argument("--only", help="one clip or bank")
    ap.add_argument("--out", type=Path, help="encode: where to write (build/animation); scene: the .blend")
    ap.add_argument("--blender", default=os.environ.get("PE2_BLENDER", "blender"), help="scene: Blender's executable")
    args = ap.parse_args(argv)
    import asset_data
    assets, embedded = asset_data.ASSETS, asset_data.EMBEDDED_ASSETS
    if args.command == "scene":
        if not args.model:
            ap.error("scene needs a declared model")
        out = args.out or PEASSETS.parents[1] / "build/previews" / f"{args.model}.blend"
        return scene(args.model, assets, args.assets, out, args.blender)
    args.out = args.out or PEASSETS.parents[1] / "build/animation"
    names = declared_names(assets) if args.command == "decode" else declared_names(assets) + declared.placed_banks(assets, embedded)
    failed = 0
    for name in names:
        if args.only and name != args.only:
            continue
        if args.command == "decode":
            print(declared.decode(name, assets, embedded, args.assets).relative_to(args.assets))
        elif args.command == "encode":
            args.out.mkdir(parents=True, exist_ok=True)
            bank = assets[name]["type"] == "animation_bank"
            data = declared.bank_bytes(name, assets, embedded) if bank else declared.encode(name, assets, embedded, args.assets)
            path = args.out / f"{name}{'.bank' if bank else '.clip'}"
            path.parent.mkdir(parents=True, exist_ok=True)           # a grouped asset's name is a path
            path.write_bytes(data)
            print(path)
        else:
            problems = declared.check(name, assets, embedded, args.assets)
            print(f"{name}: " + ("rebuilds" if not problems else "; ".join(problems)))
            failed += bool(problems)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
