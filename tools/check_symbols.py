#!/usr/bin/env python3
"""Check every symbol the splat configs declare against the images they describe.

    check_symbols.py [--root DIR] [--image NAME ...] [--verbose] [--strict]

Each config (`configs/USA/{main,gameplay,title}.yaml` and every generated
overlay config) lists symbol files; together they declare names at addresses.
An image's address range is read from its linked ELF, so the build must have
run. A declaration belongs to the image whose range holds its address: the
config's own image, or - for an import - an image that can be resident beside
it, which is any image whose range does not overlap its own, except that title
and gameplay never run together. An import into a region several such images
share (another family's load slot) has no single owner: it belongs to that
*slot*, the set of images covering the address.

Checks, in order:

0. layout: images that are resident together must not overlap. Main is resident
   under everything; title and gameplay replace each other; a family's
   overlays replace each other.
1. range: a declaration outside the config's own image and outside every image
   resident with it names nothing that image can reach.
2. declared: every symbol splat had to invent for a reference (listed in the
   image's `undefined_*_auto` files) must be declared under that name - a
   reference to an address nobody named, or to a name the owning image spells
   differently, is reported.
3. names: one name means one thing. A name declared at two addresses, or at one
   address in two different images, is reported.
4. addresses: where several names are declared at one address, the address is
   *unique* if only one image in the project covers it - there is one
   definition there, and every name for it must be the same - or *ambiguous* if
   several images overlap there (a family's overlays share a load address,
   title and gameplay share theirs), in which case names only have to agree
   within each image.

Exit status is 1 with --strict when anything is reported.
"""
from __future__ import annotations

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

import yaml
from elftools.elf.constants import SH_FLAGS
from elftools.elf.elffile import ELFFile

DECL = re.compile(r'^\s*([A-Za-z_.$][\w.$]*)\s*=\s*0x([0-9A-Fa-f]+)\s*;(.*)$')
CORE = {'main': 'SLUS_010.42', 'gameplay': 'gameplay', 'title': 'title'}
NEVER_TOGETHER = {frozenset(('title', 'gameplay'))}
ALWAYS = 'SLUS_010.42'


class Decl:
    __slots__ = ('image', 'owner', 'name', 'addr', 'attrs', 'where')

    def __init__(self, image, name, addr, attrs, where):
        self.image, self.name, self.addr, self.attrs, self.where = image, name, addr, attrs, where
        self.owner = None


def image_range(elf: Path) -> tuple[int, int]:
    with open(elf, 'rb') as f:
        lo, hi = None, None
        for s in ELFFile(f).iter_sections():
            # main's executable header is allocated at address 0 but is not
            # part of the loaded image.
            if not s['sh_flags'] & SH_FLAGS.SHF_ALLOC or s['sh_size'] == 0 or s['sh_addr'] < 0x80000000:
                continue
            a, b = s['sh_addr'], s['sh_addr'] + s['sh_size']
            lo = a if lo is None else min(lo, a)
            hi = b if hi is None else max(hi, b)
    return lo, hi


def load_configs(root: Path) -> dict[str, dict]:
    """image name -> {sym files, undefined files}."""
    out = {}
    paths = [root / f'configs/USA/{c}.yaml' for c in CORE] + sorted((root / 'configs/USA/generated').glob('*.yaml'))
    for p in paths:
        opts = yaml.safe_load(p.read_text())['options']
        image = CORE.get(p.stem, p.stem)
        syms = opts.get('symbol_addrs_path') or []
        out[image] = {
            'config': p.relative_to(root),
            'syms': [syms] if isinstance(syms, str) else syms,
            'undef': [opts[k] for k in ('undefined_funcs_auto_path', 'undefined_syms_auto_path') if opts.get(k)],
        }
    return out


def parse_decls(root: Path, image: str, files: list[str]) -> list[Decl]:
    decls = []
    for f in files:
        path = root / f
        if not path.is_file():
            continue
        for n, line in enumerate(path.read_text(errors='replace').splitlines(), 1):
            m = DECL.match(line)
            if not m:
                continue
            attrs = dict(re.findall(r'\b(\w+):(\S+)', m.group(3).split('//', 1)[-1])) if '//' in m.group(3) else {}
            decls.append(Decl(image, m.group(1), int(m.group(2), 16), attrs, f'{f}:{n}'))
    return decls


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--root', default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument('--image', action='append', default=[], help='report only findings involving these images')
    ap.add_argument('--verbose', '-v', action='store_true', help='list every finding, not only counts and a sample')
    ap.add_argument('--strict', action='store_true')
    a = ap.parse_args()
    root = Path(a.root).resolve()

    configs = load_configs(root)
    ranges = {}
    for image in configs:
        elf = root / 'build/USA/out' / f'{image}.elf'
        if not elf.is_file():
            sys.exit(f'{elf} is missing; build first')
        ranges[image] = image_range(elf)

    def covering(addr):
        return [i for i, (lo, hi) in ranges.items() if lo <= addr < hi]

    def overlaps(x, y):
        return ranges[x][0] < ranges[y][1] and ranges[y][0] < ranges[x][1]

    resident = {i: {j for j in ranges if j != i and (j == ALWAYS or i == ALWAYS or not overlaps(i, j))
                    and frozenset((i, j)) not in NEVER_TOGETHER} for i in ranges}

    def owner_of(image, addr):
        lo, hi = ranges[image]
        if lo <= addr < hi:
            return image
        cands = sorted(j for j in covering(addr) if j in resident[image])
        if not cands:
            return None
        if len(cands) == 1:
            return cands[0]
        return f'slot[{cands[0]}..{cands[-1]}]({len(cands)})'

    findings: dict[str, list[tuple[set, str]]] = defaultdict(list)

    seen = set()

    def report(check, images, msg):
        if (check, msg) not in seen:
            seen.add((check, msg))
            findings[check].append((set(images), msg))

    # 0. layout: main and the core images are resident with everything that
    # loads beside them, so their ranges must stay clear of each other.
    for other in ('gameplay', 'title'):
        if overlaps(ALWAYS, other):
            lo = max(ranges[ALWAYS][0], ranges[other][0])
            hi = min(ranges[ALWAYS][1], ranges[other][1])
            report('layout', [ALWAYS, other], f'{ALWAYS} [0x{ranges[ALWAYS][0]:08X}, 0x{ranges[ALWAYS][1]:08X}) '
                   f'overlaps {other} [0x{ranges[other][0]:08X}, 0x{ranges[other][1]:08X}) over [0x{lo:08X}, 0x{hi:08X})')

    # 1. range, and each declaration's owning image. A symbol file shared by
    # a family is read once per config, so its findings are merged by line.
    decls: list[Decl] = []
    by_image: dict[str, list[Decl]] = {}
    out_of_range = defaultdict(list)
    for image, cfg in configs.items():
        ds = parse_decls(root, image, cfg['syms'])
        by_image[image] = ds
        for d in ds:
            d.owner = owner_of(image, d.addr)
            if d.owner is None:
                out_of_range[(d.where, d.name, d.addr)].append(image)
        decls += ds
    for (w, name, addr), images in sorted(out_of_range.items()):
        whom = images[0] if len(images) == 1 else f'{len(images)} images reading it'
        report('range', images, f'{w}: {name} = 0x{addr:08X} is outside {whom} and every image resident with it')

    # 2. declared: references splat had to write out for the linker.
    for image, cfg in configs.items():
        names = {d.name for d in by_image[image]}
        for f in cfg['undef']:
            path = root / f
            if not path.is_file():
                continue
            for line in path.read_text().splitlines():
                m = DECL.match(line)
                if not m or m.group(1) in names:
                    continue
                name, addr = m.group(1), int(m.group(2), 16)
                owner = owner_of(image, addr)
                known = sorted({d.name for d in by_image.get(owner, []) if d.addr == addr and d.owner == owner})
                if owner is None:
                    report('declared', [image], f'{image}: {name} = 0x{addr:08X} is referenced but lies in no image it can reach')
                elif known:
                    report('declared', [image, owner], f'{image}: references {name} = 0x{addr:08X}, which {owner} declares as {", ".join(known)}')
                else:
                    report('declared', [image, owner], f'{image}: references {name} = 0x{addr:08X} in {owner}, where nothing is declared')

    # 3. names: one name, one (image, address).
    meaning = defaultdict(set)
    where = defaultdict(list)
    for d in decls:
        if d.owner:
            meaning[d.name].add((d.owner, d.addr))
            where[d.name].append(d)
    for name, ms in sorted(meaning.items()):
        if len(ms) > 1:
            owners = sorted({o for o, _ in ms})
            desc = ', '.join(f'{o}@0x{ad:08X}' for o, ad in sorted(ms)[:4]) + (f', ... ({len(ms)} in all)' if len(ms) > 4 else '')
            report('names', owners, f'{name} names {len(ms)} things: {desc}')

    # 4. addresses: several names at one address.
    at = defaultdict(list)
    for d in decls:
        if d.owner:
            at[d.addr].append(d)
    for addr, ds in sorted(at.items()):
        names = {d.name for d in ds}
        if len(names) < 2:
            continue
        cover = covering(addr)
        if len(cover) == 1 and len({d.owner for d in ds}) == 1:
            report('addresses', [cover[0]] + [d.image for d in ds],
                   f'0x{addr:08X} is unique to {cover[0]} but declared as {", ".join(sorted(names))}')
            continue
        per_owner = defaultdict(set)
        for d in ds:
            per_owner[d.owner].add(d.name)
        for owner, ns in sorted(per_owner.items()):
            if len(ns) > 1:
                report('addresses', [owner], f'0x{addr:08X} is ambiguous ({len(cover)} images) and {owner} alone '
                       f'declares it as {", ".join(sorted(ns))}')

    wanted = set(a.image)
    total = 0
    for check in ('layout', 'range', 'declared', 'names', 'addresses'):
        items = [msg for imgs, msg in findings[check] if not wanted or imgs & wanted]
        total += len(items)
        print(f'{check}: {len(items)}')
        for msg in items if a.verbose else items[:5]:
            print(f'  {msg}')
        if not a.verbose and len(items) > 5:
            print(f'  ... {len(items) - 5} more (--verbose)')
    sys.exit(1 if a.strict and total else 0)


if __name__ == '__main__':
    main()
