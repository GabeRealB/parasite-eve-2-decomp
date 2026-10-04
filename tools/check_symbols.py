#!/usr/bin/env python3
"""Check every symbol the splat configs declare against the images they describe.

    check_symbols.py [--root DIR] [--image NAME ...] [--verbose] [--strict]
                     [--assign-owners] [--renames FILE]

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
3. names: cross-image names mean one thing. A name declared at two addresses,
   or at one address in two different images, is reported. Definitions used
   only inside their owning images have image scope: included C implementations
   can reuse private names. Each such declaration must resolve to a section-backed
   definition at the declared address in its own ELF; an import is never exempt.
4. addresses: where several names are declared at one address, the address is
   *unique* if only one image in the project covers it - there is one
   definition there, and every name for it must be the same - or *ambiguous* if
   several images overlap there (a family's overlays share a load address,
   title and gameplay share theirs), in which case names only have to agree
   within each image.
5. interior: a name that C code uses although it is declared strictly inside
   another data object of the image that owns it - a field or element given a
   name of its own. Code reaching that address means the containing object's
   member, and should say so; declaring the interior address separately gives
   one object two declarations. Extents are the sized symbols of the owning
   image's ELF. A name only the split assembly still carries is a label, not a
   declaration, and is not reported.

Three kinds of memory belong to no image of ours but are real targets: the
kernel's area below the executable, the fixed buffers at the top of retail RAM,
and a development unit's RAM past 2 MB, where debug tooling the game calls into
lived. They are modelled as regions
always resident with everything. The packages one manifest entry builds from a
single source (its `slots`) count as one image, compared by file offset, since
each slot may load them at a different address.

A reference into a slot says which image it means, in its symbol-map comment:

    owner=IMAGE     exactly one image starts a symbol at the address, and the
                    reference carries that symbol's name. Rejected when the
                    owner does not define that name there, and when any other
                    image that can hold the slot starts a symbol there too.
    owner=IMAGE shared=FAMILY
                    several images start a symbol there, and this reference
                    means IMAGE's - established from the use site, since the
                    address cannot say. IMAGE must define the reference's name
                    there. The address may be imported again under another
                    image's name by a reference that means that one.
    shared=FAMILY   several images start a symbol there - a weapon's entry
                    point, a table every actor exports - and the reference
                    means whichever is loaded. Rejected when fewer than two
                    do, or when they belong to other families than written
                    (`shared=mappic+pe` where they span two).

Candidates are found by address alone, never by the reference's name, and only
a symbol's start counts: a reference landing inside an object is offered
nothing. The `owner` check lists every reference that has neither annotation,
with its candidates. `--assign-owners` writes owner= where there is one
candidate and the names agree; `--renames FILE` lists those
whose one candidate spells the name differently, to be renamed first.
Ownership does not prove runtime
reachability: retained alternate-room branches still need their load-state and
resource bounds reviewed separately.
"""
from __future__ import annotations

import argparse
import re
import sys
import tomllib
from collections import defaultdict
from pathlib import Path

import yaml
from elftools.elf.constants import SH_FLAGS
from elftools.elf.elffile import ELFFile

DECL = re.compile(r'^\s*([A-Za-z_.$][\w.$]*)\s*=\s*0x([0-9A-Fa-f]+)\s*;(.*)$')
CORE = {'main': 'SLUS_010.42', 'gameplay': 'gameplay', 'title': 'title'}
NEVER_TOGETHER = {frozenset(('title', 'gameplay'))}
ALWAYS = 'SLUS_010.42'
# Memory the game addresses that no image of ours occupies: the kernel's area
# below the executable, the fixed buffers at the top of the retail console's
# RAM (the image buffers boot points Fs_ImgBuffers at, and the work area above
# them), and the RAM a development unit had past the retail 2 MB, where debug
# tooling the game calls into was resident.
EXTERNAL = {
    'kernel': (0x80000000, 0x80010000),
    'fixed-buffers': (0x801D7000, 0x80200000),
    'devkit-ram': (0x80200000, 0x80800000),
}


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


def image_objects(elf: Path) -> list[tuple[int, int, str]]:
    """Sized data symbols of a linked image, as (address, size, name), sorted."""
    out = {}
    with open(elf, 'rb') as f:
        e = ELFFile(f)
        tab = e.get_section_by_name('.symtab')
        if tab is None:
            return []
        for sym in tab.iter_symbols():
            size, sh = sym['st_size'], sym['st_shndx']
            if not size or not isinstance(sh, int):
                continue
            if e.get_section(sh)['sh_flags'] & SH_FLAGS.SHF_EXECINSTR:
                continue
            name = sym.name.removesuffix('.NON_MATCHING')
            out[(sym['st_value'], name)] = size
    return sorted((a, sz, n) for (a, n), sz in out.items())


def image_definitions(elf: Path) -> dict[tuple[str, int], bool]:
    """Section-backed definitions mapped to whether their linkage is local.

    Excludes absolute linker aliases and imports. Distinct TUs may define the
    same local name at different addresses in one image.
    """
    with open(elf, 'rb') as f:
        e = ELFFile(f)
        tab = e.get_section_by_name('.symtab')
        if tab is None:
            return {}
        return {(s.name, s['st_value']): s['st_info']['bind'] == 'STB_LOCAL'
                for s in tab.iter_symbols()
                if isinstance(s['st_shndx'], int) and s['st_shndx'] != 0
                and s['st_info']['type'] in ('STT_FUNC', 'STT_OBJECT', 'STT_NOTYPE')}


# Symbols that mark a position rather than define something: the compiler's
# line labels and unit markers, and the linker script's section bounds.
MARKER = re.compile(r'^(LM\d+|__gnu_compiled_c|gcc2_compiled\.|.*_(TEXT|DATA|RODATA|BSS|VRAM)(_(START|END))?)$')


def image_symbols_at(elf: Path) -> dict[int, list[tuple[str, bool]]]:
    """What an image defines at each address: (name, is code), markers left out."""
    out = defaultdict(set)
    with open(elf, 'rb') as f:
        e = ELFFile(f)
        tab = e.get_section_by_name('.symtab')
        if tab is None:
            return {}
        for s in tab.iter_symbols():
            sh = s['st_shndx']
            if not isinstance(sh, int) or sh == 0 or s['st_info']['type'] not in ('STT_FUNC', 'STT_OBJECT', 'STT_NOTYPE'):
                continue
            name = s.name.removesuffix('.NON_MATCHING')
            if not name or MARKER.match(name) or name.startswith(('.', '$')):
                continue
            # An overlay links as one section holding code and data alike, so
            # the section says nothing; the symbol's own type does.
            out[s['st_value']].add((name, s['st_info']['type'] == 'STT_FUNC'))
    return {a: sorted(v) for a, v in out.items()}


def image_scoped_names(decls: list[Decl], definitions) -> set[str]:
    """Names with proven owning-image definitions and no cross-image use.

    External C linkage can join TUs inside one overlay without exporting an
    interface to other overlays. Check actual declarations, not a spelling
    convention or a list of exempt modules.
    """
    local, imported = set(), set()
    for d in decls:
        if d.owner == d.image and (d.name, d.addr) in definitions.get(d.image, ()):
            local.add(d.name)
        else:
            imported.add(d.name)
    return local - imported


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


def load_groups(root: Path) -> dict[str, str]:
    """package -> manifest entry, for entries that build several packages.

    Such an entry compiles one source into every package it lists (byte-identical
    copies, or variants differing only in their defines), so a name it declares
    in each of them is still one thing."""
    with open(root / 'configs/USA/overlays.toml', 'rb') as f:
        manifest = tomllib.load(f)
    groups = {}
    for family, body in manifest.items():
        for key, entry in (body.get('overlays') or {}).items():
            for slot in entry.get('slots') or []:
                groups[slot['package']] = f'{family}/{key}'
    return groups


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
            explicit_owner = re.search(r'\bowner=([A-Za-z0-9_]+)\b', m.group(3))
            if explicit_owner:
                attrs['owner'] = explicit_owner.group(1)
            shared = re.search(r'\bshared=([A-Za-z0-9_+]+)', m.group(3))
            if shared:
                attrs['shared'] = shared.group(1)
            decls.append(Decl(image, m.group(1), int(m.group(2), 16), attrs, f'{f}:{n}'))
    return decls


def has_explicit_owner(d: Decl, symbols_at, ranges) -> bool:
    """An owner must define this name at this address in its own image."""
    owner = d.attrs.get('owner')
    if d.attrs.get('absolute', '').lower() != 'true' or owner not in ranges or owner not in symbols_at:
        return False
    lo, hi = ranges[owner]
    here = symbols_at[owner].get(d.addr, ())
    return lo <= d.addr <= hi and (any(n == d.name for n, _ in here) or (bool(here) and d.name in slot_copy_names(owner, d.addr)))


def slot_copy_names(owner: str, addr: int) -> tuple[str, str]:
    """What a reference may call an object of one slot copy of a multi-slot source.

    The copies of one source carry the same symbol names, so a reference cannot
    use that name once an image refers to two copies. It names the package and
    the address instead, the form a placeholder takes.
    """
    return (f'D_{owner}_{addr:08X}', f'func_{owner}_{addr:08X}')


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--root', default=str(Path(__file__).resolve().parent.parent))
    ap.add_argument('--image', action='append', default=[], help='report only findings involving these images')
    ap.add_argument('--verbose', '-v', action='store_true', help='list every finding, not only counts and a sample')
    ap.add_argument('--strict', action='store_true')
    ap.add_argument('--assign-owners', action='store_true',
                    help='write owner=IMAGE onto every unowned reference that has exactly one candidate '
                         'and already carries that candidate\'s name')
    ap.add_argument('--renames', metavar='FILE',
                    help='write the unowned references whose one candidate spells the name differently, '
                         'as `old new owner where` lines, for a rename before --assign-owners')
    a = ap.parse_args()
    root = Path(a.root).resolve()

    configs = load_configs(root)
    ranges = {}
    for image in configs:
        elf = root / 'build/USA/out' / f'{image}.elf'
        if not elf.is_file():
            sys.exit(f'{elf} is missing; build first')
        ranges[image] = image_range(elf)
    ranges.update(EXTERNAL)
    groups = load_groups(root)
    slot_members: dict[str, set[str]] = {}

    def canon(owner):
        """The thing an owner stands for: a multi-package entry for its packages."""
        return groups.get(owner, owner)

    def covering(addr):
        return [i for i, (lo, hi) in ranges.items() if lo <= addr < hi]

    def overlaps(x, y):
        return ranges[x][0] < ranges[y][1] and ranges[y][0] < ranges[x][1]

    always = {ALWAYS, *EXTERNAL}
    resident = {i: {j for j in ranges if j != i and (j in always or i in always or not overlaps(i, j))
                    and frozenset((i, j)) not in NEVER_TOGETHER} for i in ranges}

    def owner_of(image, addr):
        lo, hi = ranges[image]
        # The end address itself is the image's: boundary symbols such as the
        # end of bss point one past the last byte.
        if lo <= addr <= hi:
            return image
        cands = sorted(j for j in covering(addr) if j in resident[image])
        if not cands:
            return None
        if len(cands) == 1:
            return cands[0]
        slot = f'slot[{cands[0]}..{cands[-1]}]({len(cands)})'
        slot_members[slot] = {canon(c) for c in cands}
        return slot

    symbols_at = {image: image_symbols_at(root / 'build/USA/out' / f'{image}.elf') for image in configs}

    def needs_owner(image, d):
        """Whether layout alone leaves the reference without one owner.

        That is an absolute reference into a region several images load into
        in turn: another family's slot, or the image's own - where it can only
        mean an image that replaces this one, since this one does not define it.
        """
        if d.attrs.get('absolute', '').lower() != 'true':
            return False
        if MARKER.match(d.name):
            return False  # A section bound the linker script places, not a reference.
        lo, hi = ranges[image]
        if lo <= d.addr <= hi:
            return not any(n == d.name for n, _ in symbols_at[image].get(d.addr, ()))
        return len([j for j in covering(d.addr) if j in resident[image]]) > 1

    def candidates(image, d):
        """Images that define something at the address, with what they call it.

        Decided by the address, never by the reference's own name: the name is
        what an owner is supposed to establish. A reference declared as code is
        offered only code.
        """
        lo, hi = ranges[image]
        if lo <= d.addr <= hi:
            pool = [j for j in configs if j != image and canon(j) != canon(image) and overlaps(image, j)]
        else:
            pool = [j for j in covering(d.addr) if j in resident[image] and j in configs]
        want_code = True if d.attrs.get('type') == 'func' else None
        return {(j, n, code) for j in pool for n, code in symbols_at[j].get(d.addr, ())
                if want_code is None or code == want_code}

    def family(image):
        """The family an overlay's config belongs to; a core image is its own."""
        for f in configs[image]['syms']:
            m = re.search(r'/sym/([^/]+)/', f)
            if m:
                return m.group(1)
        return image

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
    unowned: dict[tuple, dict] = {}
    for image, cfg in configs.items():
        by_image[image] = parse_decls(root, image, cfg['syms'])
    for image, ds in by_image.items():
        for d in ds:
            explicit_owner = d.attrs.get('owner')
            shared = d.attrs.get('shared')
            if explicit_owner is not None and shared is not None:
                # Both: the place is shared, and this reference means one of
                # the images that define it - which the address cannot say, and
                # the use site does (a stage table's entry for one room). The
                # same address may then be imported again, under another
                # image's name, by a reference that means that one.
                cands = candidates(image, d)
                entries = {canon(j) for j, _, _ in cands}
                fams = '+'.join(sorted({family(j) for j, _, _ in cands}))
                if not has_explicit_owner(d, symbols_at, ranges):
                    report('ownership', [image, explicit_owner],
                           f'{d.where}: {d.name} has no matching definition in owner={explicit_owner}')
                    d.owner = None
                elif len(entries) < 2:
                    report('ownership', [image, explicit_owner],
                           f'{d.where}: {d.name} is owner={explicit_owner} shared={shared}, but only that image '
                           f'starts a symbol at 0x{d.addr:08X}; drop shared=')
                    d.owner = explicit_owner
                elif fams != '+'.join(sorted(shared.split('+'))):
                    report('ownership', [image], f'{d.where}: {d.name} is shared={shared}, but the images that start '
                                                 f'a symbol there belong to {fams}')
                    d.owner = explicit_owner
                else:
                    d.owner = explicit_owner
            elif explicit_owner is not None:
                # owner= says one image, and only one, starts a symbol here.
                entries = {canon(j) for j, _, _ in candidates(image, d)}
                if not has_explicit_owner(d, symbols_at, ranges):
                    report('ownership', [image, explicit_owner],
                           f'{d.where}: {d.name} has no matching definition in owner={explicit_owner}')
                    d.owner = None
                elif entries != {canon(explicit_owner)}:
                    report('ownership', [image, explicit_owner],
                           f'{d.where}: {d.name} has owner={explicit_owner}, but {len(entries)} images start a symbol '
                           f'at 0x{d.addr:08X}; say shared= as well if the use site establishes this one')
                    d.owner = None
                else:
                    d.owner = explicit_owner
            elif shared is not None:
                # shared= says the opposite: the address is a place in a slot
                # that several images define - an entry point, a table every
                # weapon exports - and the reference means whichever is loaded.
                # It names the families so that the claim can be checked.
                cands = candidates(image, d)
                entries = {canon(j) for j, _, _ in cands}
                fams = '+'.join(sorted({family(j) for j, _, _ in cands}))
                d.owner = owner_of(image, d.addr)
                if len(entries) < 2:
                    report('ownership', [image],
                           f'{d.where}: {d.name} is shared={shared}, but {len(entries)} image(s) start a symbol at '
                           f'0x{d.addr:08X}' + ('; one image is owner=' if entries else ''))
                elif fams != '+'.join(sorted(shared.split('+'))):
                    report('ownership', [image], f'{d.where}: {d.name} is shared={shared}, but the images that start '
                                                 f'a symbol there belong to {fams}')
            else:
                d.owner = owner_of(image, d.addr)
                if needs_owner(image, d):
                    # A shared symbol file is read by every image of its
                    # family; an owner has to hold for all of them.
                    c = candidates(image, d)
                    u = unowned.setdefault((d.where, d.name, d.addr), {'images': set(), 'cands': c})
                    u['images'].add(image)
                    u['cands'] &= c
            if d.owner is None:
                out_of_range[(d.where, d.name, d.addr)].append(image)
        decls += ds
    for (w, name, addr), images in sorted(out_of_range.items()):
        whom = images[0] if len(images) == 1 else f'{len(images)} images reading it'
        report('range', images, f'{w}: {name} = 0x{addr:08X} is outside {whom} and every image resident with it')

    # 1b. owner: a reference into a slot says which image it means. The
    # candidates are the images defining something at that address; one
    # candidate is an answer, several are a question for whoever reads the code.
    assign, renames = [], []
    for (w, name, addr), u in sorted(unowned.items()):
        by_entry = defaultdict(set)
        for j, n, _ in u['cands']:
            by_entry[canon(j)].add((j, n))
        names = {n for _, n, _ in u['cands']}
        if len(by_entry) == 1 and len(names) == 1:
            owner, new, code = sorted(u['cands'])[0]
            # With every image's data defined in C, each object starts a
            # symbol, so "no other image starts one here" holds for data as it
            # does for functions.
            named = new == name or (canon(owner) != owner and name in slot_copy_names(owner, addr))
            (assign if named else renames).append((w, name, new, owner))
            what = f'one candidate: {owner}' + ('' if named else f', which calls it {new}')
        elif not u['cands']:
            what = 'no image defines anything there'
        else:
            shown = sorted(f'{j}:{n}' for j, n, _ in u['cands'])
            fams = '+'.join(sorted({family(j) for j, _, _ in u['cands']}))
            what = f'shared={fams}? {len(by_entry)} candidates: ' + ', '.join(shown[:6]) + (f', ... ({len(shown)} in all)' if len(shown) > 6 else '')
        report('owner', u['images'], f'{w}: {name} = 0x{addr:08X} has no owner; {what}')
    if a.renames:
        Path(a.renames).write_text(''.join(f'{old} {new} {owner} {w}\n' for w, old, new, owner in renames))
    if a.assign_owners:
        edits = defaultdict(dict)
        for w, name, _, owner in assign:
            f, n = w.rsplit(':', 1)
            edits[f][int(n)] = owner
        for f, lines in edits.items():
            text = (root / f).read_text().splitlines(keepends=True)
            for n, owner in lines.items():
                body = text[n - 1].rstrip('\n')
                text[n - 1] = body + (' ' if '//' in body else ' // ') + f'owner={owner}\n'
            (root / f).write_text(''.join(text))
        print(f'owner= written on {len(assign)} reference(s); {len(renames)} more wait for a rename')

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
                elif owner in EXTERNAL:
                    # Kernel and devkit memory belong to no config of ours,
                    # so nothing can be declared there.
                    continue
                elif known:
                    report('declared', [image, owner], f'{image}: references {name} = 0x{addr:08X}, which {owner} declares as {", ".join(known)}')
                else:
                    report('declared', [image, owner], f'{image}: references {name} = 0x{addr:08X} in {owner}, where nothing is declared')

    # 3. names: one cross-image name, one (image, address). The packages of one manifest
    # entry count as one image, and an import into a shared slot means the
    # same thing as a declaration by any of the slot's images at that address.
    meaning = defaultdict(set)
    absolute = defaultdict(set)
    where = defaultdict(list)
    definitions = {
        image: image_definitions(root / 'build/USA/out' / f'{image}.elf')
        for image in configs
    }
    scoped = image_scoped_names(decls, definitions)
    for d in decls:
        if d.owner:
            # An entry's packages may load into different slots; its names
            # are file offsets, the same in every copy.
            where_in = d.addr - ranges[d.owner][0] if d.owner in groups else d.addr
            scope = ''
            if d.name in scoped:
                scope = canon(d.image)
                if definitions[d.image][(d.name, d.addr)]:
                    scope += f':local@{where_in:X}'
            key = (d.name, scope)
            meaning[key].add((canon(d.owner), where_in))
            absolute[key].add((canon(d.owner), d.addr))
            where[key].append(d)
    for key, ms in sorted(meaning.items()):
        name = key[0]
        # A slot import is an absolute address, so it matches a declaration by
        # the address that declaration has, not by an entry's file offset.
        ms = {(o, ad) for o, ad in ms
              if not (o in slot_members and any(x in slot_members[o] and y == ad for x, y in absolute[key]))}
        # Two importers may see a slot through different resident sets, but an
        # import into a slot at one address names the same place either way.
        ms = {('slot' if o in slot_members else o, ad) for o, ad in ms}
        if len(ms) > 1:
            owners = sorted({o for o, _ in ms})
            desc = ', '.join(f'{o}@{"+" if o in groups.values() else ""}0x{ad:08X}' for o, ad in sorted(ms)[:4]) + (f', ... ({len(ms)} in all)' if len(ms) > 4 else '')
            report('names', set(owners) | {d.image for d in where[key]}, f'{name} names {len(ms)} things: {desc}')

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
        if len(cover) == 1 and len({canon(d.owner) for d in ds}) == 1:
            report('addresses', [cover[0]] + [d.image for d in ds],
                   f'0x{addr:08X} is unique to {cover[0]} but declared as {", ".join(sorted(names))}')
            continue
        per_owner = defaultdict(set)
        for d in ds:
            # The packages of one entry are one image for names that are file
            # offsets, but a reference that names its owner means that one
            # package: its copy of an exported function has its own name.
            per_owner[d.owner if d.attrs.get('owner') == d.owner else canon(d.owner)].add(d.name)
        for owner, ns in sorted(per_owner.items()):
            # A shared slot holds a different image at each load, so two names
            # imported into it at one address may well mean different things.
            if len(ns) > 1 and owner not in slot_members:
                report('addresses', {owner} | {d.image for d in ds}, f'0x{addr:08X} is ambiguous ({len(cover)} images) and {owner} alone '
                       f'declares it as {", ".join(sorted(ns))}')

    # 5. interior: names that point into the middle of another data object.
    used_in_c = set()
    for sub in ('src', 'include'):
        for path in (root / sub).rglob('*.[ch]'):
            text = re.sub(r'/\*.*?\*/|//[^\n]*', ' ', path.read_text(errors='replace'), flags=re.S)
            used_in_c.update(re.findall(r'\b[A-Za-z_]\w*\b', text))
    objects = {}
    declared_sizes = defaultdict(list)
    for d in decls:
        if d.owner == d.image and 'size' in d.attrs:
            try:
                declared_sizes[d.image].append((d.addr, int(d.attrs['size'], 16), d.name))
            except ValueError:
                pass
    for image in configs:
        # The ELF sizes an unsplit run up to the next label; a size a symbol
        # map declares is the object's own and may span labels inside it.
        objs = sorted(set(image_objects(root / 'build/USA/out' / f'{image}.elf')) | set(declared_sizes[image]))
        objects[image] = (objs, [o[0] for o in objs])
    import bisect
    for d in decls:
        if d.owner not in objects or d.name not in used_in_c:
            continue
        objs, starts = objects[d.owner]
        i = bisect.bisect_right(starts, d.addr) - 1
        # Several objects may start below the address; the nearest one that
        # still spans it is the container.
        while i >= 0 and objs[i][0] > d.addr - 0x10000:
            a0, size, name = objs[i]
            if a0 < d.addr < a0 + size and name != d.name:
                report('interior', {d.owner, d.image},
                       f'{d.where}: {d.name} = 0x{d.addr:08X} is {name}+0x{d.addr - a0:X} in {d.owner}')
                break
            i -= 1

    wanted = set(a.image)
    total = 0
    for check in ('layout', 'ownership', 'owner', 'range', 'declared', 'names', 'addresses', 'interior'):
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
