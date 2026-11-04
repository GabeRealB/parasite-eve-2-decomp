#!/usr/bin/env python3
"""The libraries packages compile and link against, made from what the rest of the build reads.

`main`, `gameplay` and `psyq` are not packages of their own yet: `include/` is what they are, which every overlay still in
`src/` compiles against. So what a package includes of them is made from there into `packages/<library>/include`, as much
as packages reach, with its includes of others named by package (`PE2_INCLUDE`) and each named one bound (`PE2_BIND`).
What they include of `include/decomp/` is core's, a package written by hand (`packages/core`) whose one public file is
`pe2.h`: such an include becomes `pe2.h`, and one of what core does not give stops the build. `ninja_config.py` does this
at every run, as `build.ninja` needs the files to exist; nothing here is edited.

    gen_package_library.py symbols ELF OUT --nm NM    what an image publishes to link against
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PACKAGES = Path("packages")
GENERATED = ("gameplay", "main", "psyq")
# What core gives through `pe2.h`, of what `include/decomp/` holds; the rest of it no package has.
THROUGH_PE2 = frozenset({"common.h", "types.h"})
# What cpp searches after a file's own folder, as the build passes it.
SEARCHED = ("include", "include/psyq", "include/decomp")
INCLUDE = re.compile(r'^(\s*#\s*include\s+)(?:"([^"\n]+)"|<([^>\n]+)>)', re.M)
# What the assembler reads, named inside the code's own strings rather than by an include.
ASSEMBLED = re.compile(r'\.include\s+\\*"([^"\\]+)')
ASKED = re.compile(r"\bPE2_INCLUDE\(\s*(\w+)\s*,\s*([^)\s]+)\s*\)")
SYMBOL = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
GUARD = re.compile(r"^#ifndef (\w+)\n#define \1\n", re.M)


def place_of(file: Path) -> tuple[str, str] | None:
    """The package of a file under `include/`, and its path within that package's public files."""
    parts = file.relative_to(ROOT).parts
    if parts[0] != "include":
        return None
    if len(parts) == 2:
        return "core", parts[1]
    if parts[1] == "decomp":
        return "core", "/".join(parts[2:])
    return parts[1], "/".join(parts[2:])


def found(including: Path | None, spelled: str, quoted: bool) -> Path | None:
    """The file an include names, as cpp finds it."""
    if quoted and including is not None:
        beside = (including.parent / spelled).resolve()
        if beside.is_file():
            return beside
    for folder in SEARCHED:
        candidate = ROOT / folder / spelled
        if candidate.is_file():
            return candidate.resolve()
    return None


def asked(package: str, inside: str) -> Path | None:
    """The file `PE2_INCLUDE(package, inside)` comes to."""
    if package == "core":
        return found(None, inside, False)
    candidate = ROOT / "include" / package / inside
    return candidate.resolve() if candidate.is_file() else None


def rewritten(source: Path, reached: list[Path]) -> str:
    """A file's text, what it reaches outside its package named by package; what it reaches of a library is noted."""
    text = source.read_text(encoding="latin-1", newline="")
    own, _ = place_of(source)
    bound: set[str] = set()
    macros = False

    def include(match: re.Match) -> str:
        spelled, quoted = (match[2], True) if match[2] else (match[3], False)
        file = found(source, spelled, quoted)
        place = place_of(file) if file is not None else None
        if place is None:
            return match[0]
        package, inside = place
        if package == "core":
            if inside not in THROUGH_PE2:
                raise SystemExit(f"{source}: {inside} is core's, and core gives packages only pe2.h")
            nonlocal macros
            macros = True
            return ""
        if package in GENERATED:
            reached.append(file)
        if package == own:
            near = Path(inside).relative_to(Path(place_of(source)[1]).parent, walk_up=True).as_posix()
            return f'{match[1]}"{near}"'
        # Each binding stands above the first include it serves, after the lines before that include.
        first = package not in bound
        bound.add(package)
        binding = f"PE2_BIND({package}, package)\n" if first else ""
        directive = match[1].lstrip("\n")
        return f"{match[1][:len(match[1]) - len(directive)]}{binding}{directive}PE2_INCLUDE({package}, {inside})"

    # What an include taken out leaves is no more than one blank line.
    text = re.sub(r"\n{3,}", "\n\n", INCLUDE.sub(include, text))
    for name in ASSEMBLED.findall(text):
        file = found(source, name, True)
        if file is not None and place_of(file) is not None:
            reached.append(file)
    if not bound and not macros:
        return text
    # What uses the macros includes what defines them, whatever included it first; in a header, inside its guard.
    head = '#include "pe2.h"\n'
    guard = GUARD.search(text)
    if guard is None:
        return head + text
    return text[:guard.end()] + "\n" + head + text[guard.end():]


def written(path: Path, text: str) -> None:
    """`text` at `path`, its time unchanged where it holds that already: the compiler's depfiles name these."""
    if path.is_file() and path.read_text(encoding="latin-1", newline="") == text:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="latin-1", newline="")


def make(tracked: list[Path]) -> None:
    """The libraries, as much as the packages `tracked` reach."""
    pending: list[Path] = []
    for folder in tracked:
        for file in sorted(path for path in folder.rglob("*") if path.suffix in (".c", ".h")):
            for package, inside in ASKED.findall(file.read_text(encoding="latin-1")):
                source = asked(package, inside) if package in GENERATED else None
                if source is not None:
                    pending.append(source)
    made: set[Path] = set()
    seen: set[Path] = set()
    while pending:
        source = pending.pop()
        if source in seen:
            continue
        seen.add(source)
        package, inside = place_of(source)
        target = PACKAGES / package / "include" / inside
        written(target, rewritten(source, pending))
        made.add(target)
    for package in GENERATED:
        public = PACKAGES / package / "include"
        for stale in sorted(path for path in public.rglob("*") if path.is_file() and path not in made):
            stale.unlink()


def symbols(elf: Path, out: Path, nm: str) -> None:
    listed = subprocess.run([nm, str(elf)], capture_output=True, text=True, check=True).stdout
    fields = (line.split() for line in listed.splitlines())
    named = {field[2]: field[0] for field in fields if len(field) == 3 and SYMBOL.fullmatch(field[2])}
    written(out, "".join(f"PROVIDE({name} = 0x{address});\n" for name, address in named.items()))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("step", choices=("symbols",))
    parser.add_argument("elf", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("--nm", required=True)
    args = parser.parse_args()
    symbols(args.elf, args.out, args.nm)


if __name__ == "__main__":
    sys.exit(main())
