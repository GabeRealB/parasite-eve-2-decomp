"""Bring a room's C in line with its instance: the module includes where their blocks are, no room data written out.

For each unit of the room:

1. every module include (with its `ROOM_PART` selector), the declarations include, and every C definition of an
   object the room's instance holds - with its doc comment, the cell-macro pair around a grid table, and its
   forward declarations - are removed;
2. what stays is located by address: the unit's C data definitions and the objects its includes define (generated
   models, clips and banks under build/include, and shared `.inc.c` sources), each in its section;
3. each module part is included before the first thing that stays which follows it in address order, in its
   section, or after the last one before it; parts at one place are in address order;
4. the declarations include goes after the last header include before the unit's first definition.

An object that leaves the room's data has to be given back its C definition first: the tool never writes C data.
A lost definition shows in the build and in `check_order`.

`check_order` compares a built unit's `.data` and `.rodata` symbols with their addresses in the symbol map and names
the first object out of place - the mistake a misplaced include makes, before the checksum shows it.
"""
from __future__ import annotations

import re
import subprocess
from pathlib import Path

from . import _units, c_files, images, read

ROOT = Path(__file__).resolve().parents[3]
DEF = re.compile(r"^((?:static\s+)?(?:const\s+)?[\w]+[\s*]+(\w+)\s*((?:\[[^\]]*\])*)\s*"
                 r"(?:(?:SECTION\([^)]*\)|__attribute__\(\(\w+\([^)]*\)\)\))\s*)?=\s*)", re.M)
FORWARD = re.compile(r"^(?:static|extern)\s+(?:const\s+)?\w+[\s*]+(\w+)\s*(?:\[[^\]]*\])*\s*;[^\n]*\n", re.M)
GENERATED_DEF = re.compile(r"^(?:static )?(?:const )?[\w*]+ (\w+)(?:\[\d+\])*(?: SECTION\([^)]*\))? =", re.M)


def _statement_end(s: str, i: int) -> int:
    if s[i] == "{":
        depth = 0
        for j in range(i, len(s)):
            if s[j] == "{":
                depth += 1
            elif s[j] == "}":
                depth -= 1
                if depth == 0:
                    return s.index(";", j) + 1
    return s.index(";", i) + 1


def _with_comment(s: str, start: int) -> int:
    lines = s[:start].split("\n")
    k = len(lines) - 1
    while k > 0 and lines[k - 1].lstrip().startswith("///"):
        k -= 1
    return len("\n".join(lines[:k])) + (1 if k else 0)


def _sources(rec: dict) -> list[Path]:
    group, pkg = rec["group"], rec["package_name"]
    return sorted(Path(p) for p in {*map(str, (ROOT / "src" / "rooms" / pkg).glob("*.c")),
                                    *map(str, (ROOT / "src" / "mapui" / pkg).glob("*.c")),
                                    *map(str, (ROOT / "src" / "stages").glob(f"*/rooms/{group.rsplit('/', 1)[1]}/*.c"))})


def _parts(files: dict[str, str]) -> list[tuple[str, str, bool]]:
    """(first object, module, selected) of every module part."""
    out = []
    for f, text in files.items():
        if f == "declarations.inc":
            continue
        firsts = re.findall(r"#define ROOM_IS_(\w+) 1", text)
        if firsts:
            out += [(first, f[:-4], True) for first in firsts]
        else:
            out.append((GENERATED_DEF.findall(text)[0], f[:-4], False))
    return out


def sync(name: str, assets: dict, embedded: dict, assets_root: Path, modules: set[str]) -> list[str]:
    """Rewrite the room's units; returns problems (and writes nothing for a unit that has one)."""
    rec = assets[name]
    group, pkg = rec["group"], rec["package_name"]
    room = read(name, assets, assets_root)
    objects = {s for objs in room["modules"].values() for s in objs}
    im, _area = images(name, assets, embedded, assets_root)
    files = c_files(name, assets, embedded, assets_root)
    parts = _parts(files)
    units = _units(pkg)

    def where(a: int) -> tuple[str, str] | None:
        return next(((u, sec) for st, en, u, sec in units if st <= a - im.base < en), None)

    decl_selected = "#define ROOM_IS_declarations_" in files["declarations.inc"]
    module_include = re.compile(r'^(?:#define ROOM_PART \w+\n)?#include "assets/' + re.escape(group) + r'/(' +
                                "|".join(sorted(modules | {"declarations", "effects"})) + r')\.inc"\n', re.M)
    included = re.compile(r'^#include "((?:assets/[\w/]+\.inc)|(?:[\w./]+\.inc\.c))"\n', re.M)
    problems = []
    for path in _sources(rec):
        unit = path.stem
        s = path.read_text(encoding="utf-8")
        cut = [[m.start(), m.end(), ""] for m in module_include.finditer(s)]
        for m in DEF.finditer(s):
            if m.group(2) in objects:
                st, en = _with_comment(s, m.start()), _statement_end(s, m.end())
                cut.append([st, en, ""])
                if "_table.inc" in s[st:en]:            # the cell-macro pair around a grid table
                    before = re.search(r"((?:///[^\n]*\n)*#define \w+\(halfwordOffset\)[^\n]*\n\n?)$", s[:st])
                    after = re.match(r"\n?#undef \w+\n", s[en:])
                    if before:
                        cut.append([before.start(), st, ""])
                    if after:
                        cut.append([en, en + after.end(), ""])
        cut += [[m.start(), m.end(), ""] for m in FORWARD.finditer(s) if m.group(1) in objects]
        # what stays, by address: C definitions and the objects of standalone asset includes
        staying = []
        for m in DEF.finditer(s):
            n = m.group(2)
            if n not in objects:
                if n in im.addresses and where(im.addresses[n]) and where(im.addresses[n])[0] == unit:
                    end = _statement_end(s, m.end())
                    end += s[end:end + 1] == "\n"
                    staying.append((im.addresses[n], where(im.addresses[n])[1], _with_comment(s, m.start()), end))
        for m in included.finditer(s):                    # data an include defines: generated, or a shared source
            target = m.group(1)
            if module_include.match(m.group(0)):
                continue
            gen = (ROOT / "build" / "include" / target) if target.startswith("assets/") else (path.parent / target)
            if not gen.exists():
                continue
            text = gen.read_text(errors="surrogateescape")
            for n in set(GENERATED_DEF.findall(text)) | {d.group(2) for d in DEF.finditer(text)}:
                if n in im.addresses and where(im.addresses[n]) and where(im.addresses[n])[0] == unit:
                    staying.append((im.addresses[n], where(im.addresses[n])[1], m.start(), m.end()))
        mine = [p for p in parts if where(im.addresses[p[0]]) and where(im.addresses[p[0]])[0] == unit]
        unit_objects = {n for n, a in im.addresses.items() if where(a) and where(a)[0] == unit and n in objects}
        if not mine and not unit_objects:
            continue
        inserts = []
        for first, module, selected in mine:
            a = im.addresses[first]
            sec = where(a)[1]
            later = [(x, st) for x, sc, st, _en in staying if sc == sec and x > a]
            earlier = [(x, en) for x, sc, _st, en in staying if sc == sec and x < a]
            if later:
                at = min(later)[1]
            elif earlier:
                at = max(earlier)[1]
            else:
                at = None
            text = (f"#define ROOM_PART {first}\n" if selected else "") + f'#include "assets/{group}/{module}.inc"\n'
            inserts.append((at, a, text))
        if any(at is None for at, _a, _t in inserts):        # no datum stays in the section: where the first part was
            olds = [st for st, _en, _r in cut]
            fallback = min(olds) if olds else None
            if fallback is None:
                problems.append(f"{path}: nowhere to include the room's modules")
                continue
            inserts = [(fallback if at is None else at, a, t) for at, a, t in inserts]
        if problems:
            continue
        for at, _a, text in sorted(inserts):
            cut.append([at, at, text])
        # a run of includes at one place reads as a block: a blank line before and after it
        places = sorted({at for at, _a, _t in inserts})
        for at in places:
            cut.append([at, at, "\n"]) if at and s[at - 2:at] != "\n\n" else None
        cut.sort(key=lambda c: (c[0], c[1] != c[0]))
        out, pos = [], 0
        for st, en, repl in cut:
            if st < pos:
                if repl:
                    out.append(repl)
                pos = max(pos, en)
                continue
            out.append(s[pos:st])
            out.append(repl)
            pos = en
        s = "".join(out) + s[pos:]
        if unit_objects:                                  # the declarations, after the headers
            first = re.search(r"^(?!#|//|/\*| \*)[A-Za-z_][^\n;]*[=(]", s, re.M)
            incs = [m for m in re.finditer(r'^#include (?!"assets/)[^\n]*\n', s, re.M)
                    if not first or m.start() < first.start()]
            at = incs[-1].end()
            sel = f"#define ROOM_PART declarations_{unit}\n" if decl_selected else ""
            s = s[:at] + f'\n{sel}#include "assets/{group}/declarations.inc"\n' + s[at:]
        s = re.sub(r"\n{3,}", "\n\n", s)
        path.write_text(s, encoding="utf-8")
    return problems


def check_order(name: str, assets: dict, embedded: dict, assets_root: Path) -> list[str]:
    """The first object of each built unit's .data and .rodata that is not in address order."""
    rec = assets[name]
    im, _area = images(name, assets, embedded, assets_root)
    out = []
    for path in _sources(rec):
        obj = next((d / f"{path.stem}.c.o" for d in (ROOT / "build" / "USA" / "src" / f / rec["package_name"]
                                                      for f in ("rooms", "mapui")) if (d / f"{path.stem}.c.o").exists()),
                   ROOT / "build" / "USA" / "src" / "rooms" / rec["package_name"] / f"{path.stem}.c.o")
        if not obj.exists():
            continue
        table = subprocess.run(["mips-linux-gnu-objdump", "-t", str(obj)], capture_output=True, text=True).stdout
        for sec in (".data", ".rodata"):
            got = sorted((int(m.group(1), 16), m.group(2)) for line in table.splitlines()
                         for m in [re.match(r"([0-9a-f]{8}) .{7} " + re.escape(sec) + r"\s+[0-9a-f]{8} (\w+)$", line)]
                         if m and m.group(2) in im.addresses)
            names = [n for _o, n in got]
            want = sorted(names, key=im.addresses.get)
            if names != want:
                i = next(k for k in range(len(names)) if names[k] != want[k])
                out.append(f"{obj.name} {sec}: {names[i]} is where {want[i]} belongs")
    return out
