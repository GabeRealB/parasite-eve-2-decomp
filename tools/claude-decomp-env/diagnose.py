#!/usr/bin/env python3
"""Conservative MIPS object diagnostics, independent of the exact match score.

Block numbering follows layout, not graph isomorphism. Unknown indirect jumps,
multiple functions, or unsupported relocations yield 'unknown', never a match.
Even matching topology and predicates are not a proof of semantic equivalence.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class Elf:
    """Read ELF32 MIPS sections and REL relocations without extra dependencies."""

    def __init__(self, path: Path):
        data = path.read_bytes()
        if data[:5] != b'\x7fELF\x01' or data[5] not in (1, 2):
            raise ValueError('expected ELF32')
        self.order = '<' if data[5] == 1 else '>'
        header = struct.unpack_from(self.order + 'HHIIIIIHHHHHH', data, 16)
        if header[0:2] != (1, 8):
            raise ValueError('expected relocatable MIPS object')
        offset, entsize, count, names_index = header[5], header[10], header[11], header[12]
        raw = [struct.unpack_from(self.order + '10I', data, offset + i * entsize)
               for i in range(count)]
        names = data[raw[names_index][4]:raw[names_index][4] + raw[names_index][5]]
        self.sections = []
        for row in raw:
            name = names[row[0]:].split(b'\0', 1)[0].decode()
            self.sections.append({'name': name, 'type': row[1], 'flags': row[2], 'size': row[5],
                                  'data': data[row[4]:row[4]+row[5]] if row[1] != 8 else b'',
                                  'link': row[6], 'info': row[7], 'entsize': row[9]})
        self.symbols = {}
        for i, section in enumerate(self.sections):
            if section['type'] != 2:
                continue
            strings = self.sections[section['link']]['data']
            symbols = []
            for at in range(0, len(section['data']), section['entsize']):
                name, value, size, info, _, index = struct.unpack_from(
                    self.order + 'IIIBBH', section['data'], at)
                symbols.append({'name': strings[name:].split(b'\0', 1)[0].decode(),
                                'value': value, 'size': size, 'type': info & 15,
                                'section': index})
            self.symbols[i] = symbols

    def relocations(self, section_index):
        result = {}
        for section in self.sections:
            if section['info'] != section_index or section['type'] not in (9, 4):
                continue
            if section['type'] == 4:
                raise ValueError('RELA relocations are not supported')
            for at in range(0, len(section['data']), section['entsize']):
                offset, info = struct.unpack_from(self.order + 'II', section['data'], at)
                result[offset] = (info & 255, self.symbols[section['link']][info >> 8])
        return result


def fingerprint(path: Path) -> str:
    """Code, allocated data and relocations, excluding debug/source filenames."""
    elf = Elf(path)
    rows = []
    for index, section in enumerate(elf.sections):
        if not section['flags'] & 2:  # SHF_ALLOC
            continue
        relocations = []
        for at, (kind, symbol) in sorted(elf.relocations(index).items()):
            target = symbol['section']
            name = elf.sections[target]['name'] if 0 < target < len(elf.sections) else symbol['name']
            relocations.append((at, kind, name, symbol['value']))
        rows.append((section['name'], section['size'], section['data'].hex(), relocations))
    return hashlib.sha256(json.dumps(rows).encode()).hexdigest()


def image(path: Path) -> dict:
    elf = Elf(path)
    text_index = next(i for i, s in enumerate(elf.sections) if s['name'] == '.text')
    data = elf.sections[text_index]['data']
    functions = {s['value'] for symbols in elf.symbols.values() for s in symbols
                 if s['section'] == text_index and s['type'] == 2}
    if len(functions) > 1 or (functions and functions != {0}):
        raise ValueError('diagnostics require a single function starting at text offset zero')
    sizes = {s['size'] for symbols in elf.symbols.values() for s in symbols
             if s['section'] == text_index and s['type'] == 2 and s['size']}
    if len(sizes) == 1:
        data = data[:sizes.pop()]
    words = list(struct.unpack(elf.order + 'I' * (len(data) // 4), data))
    relocs = elf.relocations(text_index)
    tables = {}
    for index, section in enumerate(elf.sections):
        if not section['name'].startswith('.rodata'):
            continue
        entries = {}
        for at, (kind, symbol) in elf.relocations(index).items():
            if kind == 2 and symbol['section'] == text_index:
                entries[at] = struct.unpack_from(elf.order + 'I', section['data'], at)[0] + symbol['value']
        for at in sorted(entries):
            if at - 4 in entries:
                continue
            targets = []
            cursor = at
            while cursor in entries:
                targets.append(entries[cursor])
                cursor += 4
            tables[(index, at)] = targets

    def switch_targets(pc, reg):
        # Track table addresses/loads locally through the usual GCC dispatch.
        # A bare nearby rodata reference is insufficient to resolve a jump.
        values = {}
        for at in range(max(0, pc - 64), pc, 4):
            word = words[at // 4]
            op, rs, rt, rd, fn = word >> 26, (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, word & 63
            imm = word & 65535
            imm = imm if imm < 32768 else imm - 65536
            relocation = relocs.get(at)
            value = None
            if relocation and relocation[0] == 6:
                sym = relocation[1]
                key = (sym['section'], sym['value'] + imm)
                if key in tables:
                    value = ('load' if op == 35 else 'address', key)
            elif op in (9, 13):
                value = values.get(rs) if imm == 0 else None
            elif op == 0 and fn in (33, 37):
                value = values.get(rs) or values.get(rt)
            elif op == 35 and (values.get(rs) or (None,))[0] == 'address' and imm == 0:
                value = ('load', values[rs][1])
            if op == 0 and fn in (8, 9):
                values.clear()
            elif op == 0:
                values[rd] = value
            elif op in (8, 9, 10, 11, 12, 13, 14, 15, 32, 33, 34, 35, 36, 37, 38):
                values[rt] = value
        value = values.get(reg)
        if value and value[0] == 'load':
            return tables[value[1]]
        raise ValueError(f'unresolved indirect jump at {pc:#x}')

    # Omit assembler padding after a terminal return, but retain its delay slot.
    while len(words) > 2 and words[-1] == 0 and words[-2] == 0:
        words.pop()
    leaders = {0}
    transfers, calls = [], []
    predicates = []
    delays = []
    opcounts = Counter()
    stack_accesses = 0
    for i, word in enumerate(words):
        pc = i * 4
        op, rs, rt, fn = word >> 26, (word >> 21) & 31, (word >> 16) & 31, word & 63
        opcounts[f'{op}:{fn if op == 0 else 0}'] += 1
        stack_accesses += int(rs == 29 and op in (32, 33, 35, 36, 37, 40, 41, 43))
        target, kind, predicate = None, None, None
        if op in (4, 5, 6, 7, 20, 21, 22, 23) or (op == 1 and rt in (0, 1, 2, 3)):
            imm = word & 65535
            target = pc + 4 + (imm if imm < 32768 else imm - 65536) * 4
            kind = 'branch'
            predicate = (op, rt if op == 1 else None)
            if op == 4 and rs == rt:
                kind, predicate = 'jump', None
            predicates.append((pc, predicate, rs, rt))
        elif op in (2, 3):
            target = (word & 0x3ffffff) * 4
            relocation = relocs.get(pc)
            if relocation:
                if relocation[0] != 4:
                    raise ValueError('unsupported jump relocation')
                sym = relocation[1]
                target = (target + sym['value']) if sym['section'] == text_index else f"{sym['name']}+{target}"
            kind = 'call' if op == 3 else 'jump'
        elif op == 0 and fn in (8, 9):
            if fn == 9:
                kind, target = 'call', 'indirect'
            elif rs == 31:
                kind, target = 'return', 'return'
            else:
                kind, target = 'switch', switch_targets(pc, rs)
        elif op in (16, 17, 18) and rs == 8 or op == 1:
            raise ValueError(f'unsupported branch at {pc:#x}')
        if kind is None:
            continue
        if i + 1 >= len(words):
            raise ValueError('missing delay slot')
        delay = words[i + 1]
        delayop = delay >> 26
        if delayop in (1, 2, 3, 4, 5, 6, 7, 20, 21, 22, 23) or (delayop == 0 and delay & 63 in (8, 9)):
            raise ValueError('control transfer in delay slot')
        delays.append((pc, delay))
        if kind == 'call':
            calls.append((pc, target))
            continue
        destinations = target if isinstance(target, list) else [target]
        for destination in destinations:
            if isinstance(destination, int):
                if destination % 4 or not 0 <= destination < len(words) * 4:
                    raise ValueError('branch destination outside the function')
                leaders.add(destination)
        if i + 2 < len(words):
            leaders.add(pc + 8)
        transfers.append((pc, kind, target, predicate))
    if any(pc + 4 in leaders for pc, _ in delays):
        raise ValueError('branch enters a delay slot')
    ordered = sorted(leaders)
    def block(at):
        return sum(at >= start for start in ordered) - 1
    def destination(value):
        return block(value) if isinstance(value, int) else value
    edges, conditions = [], []
    terminated = set()
    for pc, kind, target, predicate in transfers:
        source = block(pc)
        terminated.add(source)
        targets = [destination(v) for v in target] if isinstance(target, list) else destination(target)
        edges.append((source, kind, targets, block(pc + 8) if kind == 'branch' and pc + 8 < len(words)*4 else None))
        conditions.append((source, predicate))
    for b in range(len(ordered) - 1):
        if b not in terminated:
            edges.append((b, 'fallthrough', b + 1, None))
    return {'instructions': len(words), 'blocks': len(ordered), 'edges': sorted(edges),
            'conditions': conditions, 'condition_registers': [(block(p), r, t) for p, _, r, t in predicates],
            'calls': [(block(p), t) for p, t in calls], 'delay_slots': [w for _, w in delays],
            'opcounts': dict(opcounts), 'stack_accesses': stack_accesses,
            'address_halves': sum(w >> 26 == 15 for w in words)}


def compare(target: Path, candidate: Path) -> dict:
    result = {'schema': 1, 'target_sha256': sha256(target), 'object_sha256': sha256(candidate),
              'topology': 'unknown', 'semantic_equivalence': 'not established'}
    try:
        a, b = image(target), image(candidate)
        result.update(target=a, candidate=b, topology='match' if a['edges'] == b['edges'] and a['blocks'] == b['blocks'] else 'different',
                      predicates_match=a['conditions'] == b['conditions'],
                      calls_match=None if any(t == 'indirect' for _, t in a['calls'] + b['calls']) else a['calls'] == b['calls'],
                      condition_registers_match=a['condition_registers'] == b['condition_registers'],
                      delay_slot_words_match=a['delay_slots'] == b['delay_slots'])
        delta = Counter(b['opcounts'])
        delta.subtract(a['opcounts'])
        result['opcode_delta'] = {k: v for k, v in delta.items() if v}
    except (ValueError, IndexError, KeyError, StopIteration, struct.error) as exc:
        result['reason'] = str(exc) or 'unsupported object layout'
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target', type=Path)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = compare(args.target, args.candidate)
    if args.source:
        report['source_sha256'] = sha256(args.source)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print('Structure: ' + report['topology'] + ' (diagnostic only; exact verification still required)')
    if report['topology'] != 'unknown':
        print(f"  blocks={report['target']['blocks']}/{report['candidate']['blocks']} "
              f"instructions={report['target']['instructions']}/{report['candidate']['instructions']} "
              f"predicates_match={report['predicates_match']} calls_match={report['calls_match']}")
    else:
        print('  ' + report['reason'])


if __name__ == '__main__':
    main()
