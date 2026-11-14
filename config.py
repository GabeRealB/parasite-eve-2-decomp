#!/usr/bin/env python3
"""Configure a build of the decompilation: the splat build, or an editor project's.

    python3 config.py splat [--release R] [--only SELECTOR[,SELECTOR...]]... [--fresh] [--jobs N]
    python3 config.py splat fetch-tools [--path NAME]
    python3 config.py splat extract [--release R] [--roms DIR]
    python3 config.py editor [DIR] [--config NAME] [--release R]... [--roms DIR] [--fresh] [--no-extract]
                             [--no-extensions] [--no-trust]
    python3 config.py editor build-tools [--jobs N]
    python3 config.py clean [--all]

After either, `ninja` builds and holds every image to `configs/<release>/checksum.sha`; after `splat`, `ninja objdiff`
makes what objdiff reads. Each group's work is its own (`tools/config/splat`, `tools/config/editor`), loaded only when
asked for: the editor's needs nothing but the standard library, the splat build the packages of `requirements.txt`.
"""
from __future__ import annotations

import argparse
import os
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
TOOLS = ROOT / "tools" / "config"


def splat_module():
    """The splat build's module, which works in the repository (its paths are relative to it)."""
    os.chdir(ROOT)
    sys.path.insert(0, str(TOOLS / "splat"))
    import configure
    return configure


def splat(args: argparse.Namespace) -> int:
    configure = splat_module()
    only = [name for selectors in args.only or [] for name in selectors.split(",") if name]
    configure.configure(args.release, only, args.fresh, args.jobs)
    return 0


def splat_fetch_tools(args: argparse.Namespace) -> int:
    sys.path.insert(0, str(TOOLS))
    import fetch_tools
    return fetch_tools.fetch(args.path)


def splat_extract(args: argparse.Namespace) -> int:
    roms = args.roms.absolute() if args.roms else None
    configure = splat_module()
    configure.extract(args.release, roms)
    return 0


def editor(args: argparse.Namespace) -> int:
    sys.path.insert(0, str(TOOLS / "editor"))
    import configure
    configure.configure(args.directory, args.config, args.release, args.roms, args.fresh, not args.no_extract,
                        not args.no_extensions, not args.no_trust)
    return 0


def editor_build_tools(args: argparse.Namespace) -> int:
    sys.path.insert(0, str(TOOLS / "editor"))
    import configure
    configure.build_tools(args.jobs)
    return 0


# What a configure or a build writes, and what `--all` adds: what the split writes.
CLEANED = ["build", "permuter", ".splache", "build.ninja", "rules.ninja", "matching.ninja", "objdiff.ninja",
           "objdiff.json", ".ninja_log", ".ninja_deps"]
CLEANED_ALL = ["asm", "linkers", "expected"]


def clean(args: argparse.Namespace) -> int:
    for name in CLEANED + (CLEANED_ALL if args.all else []):
        path = ROOT / name
        if path.is_dir() and not path.is_symlink():
            shutil.rmtree(path)
        elif path.exists() or path.is_symlink():
            path.unlink()
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    groups = parser.add_subparsers(dest="group", metavar="{splat,editor,clean}")

    s = groups.add_parser("splat", help="the splat build: split, generate and write build.ninja",
                          description="Fetch the toolchain, split what changed and write build.ninja: `ninja` builds "
                                      "every image and checks it, `ninja objdiff` makes objdiff.json.")
    s.add_argument("--release", default="USA", help="the release to build (default: USA)")
    s.add_argument("--only", action="append", metavar="SELECTOR[,SELECTOR...]",
                   help="split and build only these units: a family (core, weapons) or a basename (gameplay, m93r); "
                        "the checksum covers only what they build")
    s.add_argument("--fresh", action="store_true",
                   help="ignore the split cache: wipe asm/, linkers/ and build/ (with --only, the units') and split "
                        "everything selected")
    s.add_argument("--jobs", type=int, default=0, help="split workers (default: $PE2_JOBS, else every CPU)")
    s.set_defaults(run=splat)
    sc = s.add_subparsers(dest="command", metavar="{fetch-tools,extract}")
    f = sc.add_parser("fetch-tools", help="fetch the pinned tools into local/tools",
                      description="Fetch what tools/config/fetched_tools.toml pins into local/tools.")
    f.add_argument("--path", metavar="NAME", help="fetch one tool and print its folder")
    f.set_defaults(run=splat_fetch_tools)
    x = sc.add_parser("extract", help="extract what the build reads from the disc images into assets/<release>",
                      description="Extract what the build reads, from the release's disc images, into rom/<release> "
                                  "and assets/<release> (both rewritten).")
    x.add_argument("--release", default=argparse.SUPPRESS, help="the release to extract (default: USA)")
    x.add_argument("--roms", type=Path, help="the folder of the disc images (default: rom/image)")
    x.set_defaults(run=splat_extract)

    e = groups.add_parser("editor", help="an editor project, made or made active, and its build.ninja",
                          description="Make the project in DIR where there is none, trust it, make it the active one "
                                      "and write build.ninja: `ninja` has the editor build it and checks its images. "
                                      "`config.py editor build-tools` lays the editor out (cargo xtask dist).")
    e.add_argument("directory", nargs="?", type=Path, metavar="DIR", help="the project (default: project/)")
    e.add_argument("--config", help="the configuration to build (default: the first release built's)")
    e.add_argument("--release", action="append", help="a release a new project is of (default: every one declared)")
    e.add_argument("--roms", type=Path, help="where the disc images are on this machine (default: the project's roms/)")
    e.add_argument("--fresh", action="store_true",
                   help="write the game's nodes and plan anew and extract again; the project's own layer is kept")
    e.add_argument("--no-extract", action="store_true", help="write the game's nodes, but read nothing off the discs")
    e.add_argument("--no-extensions", action="store_true", help="a new project gets no extensions (pe2.psx, pe2.ffmpeg)")
    e.add_argument("--no-trust", action="store_true", help="leave the project's trust as it is")
    e.set_defaults(run=editor)
    # `editor build-tools`, a group of its own to argparse, as an optional DIR before a command cannot be told from it.
    b = groups.add_parser("editor-build-tools", prog="config.py editor build-tools",
                          description="Build the editor and its extensions and lay them out (cargo xtask dist).")
    b.add_argument("--jobs", type=int, default=0, help="cargo's jobs (default: $PE2_JOBS, else cargo's)")
    b.set_defaults(run=editor_build_tools)

    c = groups.add_parser("clean", help="remove what configuring and building wrote",
                          description="Remove build/, permuter/, the ninja files and logs, objdiff.json and .splache; "
                                      "with --all also asm/, linkers/ and expected/, what the split writes.")
    c.add_argument("--all", action="store_true", help="also what the split writes")
    c.set_defaults(run=clean)

    argv = sys.argv[1:]
    if argv[:2] == ["editor", "build-tools"]:
        argv = ["editor-build-tools", *argv[2:]]
    args = parser.parse_args(argv)
    if args.group is None:
        parser.print_help()
        return 2
    return args.run(args)


if __name__ == "__main__":
    sys.exit(main())
