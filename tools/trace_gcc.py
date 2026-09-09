#!/usr/bin/env python3
"""Observe the bundled GCC's actual allocation decisions through GDB.

Accepts a saved .i or .i.gz, so archived header contents can be replayed exactly.
The supported binary is deliberately pinned: the observer reads its i386 ABI
and private data layouts. It never rebuilds or replaces the matching compiler.
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
COMPILER_SHA256 = '60d886cd75bbd7855fc7909224a15401de76bff21af8a629c2060290a073f5fd'
FLAGS = '-O2 -mips1 -mcpu=3000 -w -funsigned-char -fpeephole -ffunction-cse -fpcc-struct-return -fcommon -fverbose-asm -msoft-float -mgas -fgnu-linker -quiet -gcoff -dp -G0 -da'.split()
SOURCES = ('toplev.c', 'expr.c', 'local-alloc.c', 'global.c', 'reload1.c', 'reload.c', 'sched.c',
           'cse.c', 'combine.c', 'loop.c', 'stmt.c', 'varasm.c', 'flow.c',
           'regclass.c', 'reorg.c', 'rtl.h', 'regs.h',
           'config/mips/mips.c', 'config/mips/mips.h')
HARD = ['zero', 'at', 'v0', 'v1', 'a0', 'a1', 'a2', 'a3',
        't0', 't1', 't2', 't3', 't4', 't5', 't6', 't7',
        's0', 's1', 's2', 's3', 's4', 's5', 's6', 's7',
        't8', 't9', 'k0', 'k1', 'gp', 'sp', 'fp', 'ra']


def digest(data):
    return hashlib.sha256(data).hexdigest()


def symbols(compiler):
    """Preserve STT_FILE ownership: several passes have a private reg_qty."""
    output = subprocess.check_output(['readelf', '-sW', str(compiler)], text=True)
    result, owner = {}, ''
    for line in output.splitlines():
        cols = line.split()
        if len(cols) != 8 or not cols[0].endswith(':'):
            continue
        _, value, _, kind, binding, _, _, name = cols
        if kind == 'FILE':
            owner = name
        elif kind in ('OBJECT', 'FUNC'):
            key = name if binding != 'LOCAL' else owner + ':' + name
            result[key] = int(value, 16)
    return result


def hard(reg):
    return '$' + HARD[reg] if 0 <= reg < len(HARD) else str(reg)


def summarize(events, wanted):
    lines = ['Observed compiler decisions (quantity IDs are local to each block; priorities calculated from observed inputs).',
             'Scheduler cycles run backward within each block. Actual hazard costs are delays; potential hazard costs are ranking weights, not cycles.']
    for event in events:
        kind = event['event']
        if kind == 'local_choice' and (not wanted or wanted.intersection(event['members'])):
            lines.append('local b{block} q{qty} {members}: refs={refs} span={span} '
                         'priority={priority} copy={copy_suggestions} arithmetic={suggestions} '
                         'suggested_only={suggested_only} -> {dest}'.format(
                             **event, dest=hard(event['result'])))
        elif kind == 'global_choice' and (not wanted or wanted.intersection(event['members'])):
            lines.append('global a{allocno} {members}: refs={refs} span={span} '
                         'priority={priority} calls={calls} retry={retry} '
                         'caller_save={caller_save} -> {dest}'.format(
                             **event, dest=hard(event['result'])))
        elif kind == 'reload_order':
            lines.append('reload preference: ' + ' '.join(map(hard, event['order'][:20])))
        elif kind == 'spill' and event['changes']:
            changes = [c for c in event['changes'] if not wanted or c[0] in wanted]
            if changes:
                lines.append(f"reload reserves {hard(event['reg'])}: " + ', '.join(
                    f'r{r} {hard(before)} -> {hard(after)}' for r, before, after in changes))
        elif kind == 'schedule_compare':
            lines.append(f"{event['pass']} comparator uid {event['x']['uid']} / "
                         f"{event['y']['uid']}: {event['reason']}; prefers uid {event['winner']}")
        elif kind == 'schedule_select':
            lines.append(f"{event['pass']} cycle {event['clock']}: ready "
                         f"{[r['uid'] for r in event['before']]} -> selects {event['selected']}; "
                         f"hazards={event['hazards']}")
        elif kind == 'schedule_release':
            for dep in event['dependencies']:
                before, after = dep['before'], dep['after']
                lines.append(f"{event['pass']} cycle {event['clock']} scheduled {event['scheduled']}: "
                             f"uid {before['uid']} refs {before['refs']}->{after['refs']}, "
                             f"priority {before['priority']}->{after['priority']}, tick={after['tick']}")
        elif kind == 'postreload_set':
            lines.append(f"post-reload CSE uid {event['uid']}: {event['before']} -> {event['after']}")
        elif kind == 'error':
            lines.append('TRACE ERROR: ' + event['message'])
    lines.append('A -1 disposition means no hard-register home at that stage; '
                 'reload may use memory equivalence or rematerialize a constant.')
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path, help='preprocessed .i or archived .i.gz')
    parser.add_argument('--output-dir', required=True, type=Path, help='new directory for evidence')
    parser.add_argument('--function', help='restrict observation to this function')
    parser.add_argument('--regs', type=int, nargs='+', default=[], help='filter the text report only')
    parser.add_argument('--uids', type=int, nargs='+', default=[], help='observe scheduler comparisons, dependency releases and hazard selection for these UIDs; also filters post-reload substitutions')
    args = parser.parse_args()
    compiler = ROOT / 'tools/linux/gcc-2.8.1-psx/cc1'
    if digest(compiler.read_bytes()) != COMPILER_SHA256:
        parser.error('unsupported cc1 binary; audit the ABI/layout before adding its fingerprint')
    if not (args.input.name.endswith('.i') or args.input.name.endswith('.i.gz')):
        parser.error('provide preprocessed .i or .i.gz; use dump.sh for current headers first')
    data = args.input.read_bytes()
    if args.input.name.endswith('.gz'):
        data = gzip.decompress(data)
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=False)
    source = out / 'input.i'
    source.write_bytes(data)
    config = {'symbols': symbols(compiler), 'events': str(out / 'events.jsonl'),
              'function': args.function, 'uids': args.uids}
    (out / 'config.json').write_text(json.dumps(config))
    observer = ROOT / 'tools/gcc_trace_gdb.py'
    script = ('set pagination off\nset confirm off\nset disable-randomization off\n'
              'python\nimport json\n'
              f'CONFIG = json.load(open({str(out / "config.json")!r}))\n'
              f'exec(compile(open({str(observer)!r}).read(), {str(observer)!r}, "exec"))\n'
              'end\nrun\n')
    (out / 'observe.gdb').write_text(script)
    metadata = {'compiler_sha256': COMPILER_SHA256, 'input_sha256': digest(data),
                'input_origin': str(args.input.resolve()), 'flags': FLAGS,
                'observer_sha256': digest(observer.read_bytes()), 'source_sha256': {}}
    for name in SOURCES:
        path = ROOT / 'local/gcc/gcc-2.8.1-psx' / name
        if path.exists():
            metadata['source_sha256'][name] = digest(path.read_bytes())
    (out / 'manifest.json').write_text(json.dumps(metadata, indent=2) + '\n')
    with (out / 'baseline.log').open('w') as log:
        subprocess.run([str(compiler), *FLAGS, '-o', str(out / 'baseline.s'), str(source)],
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    with (out / 'gdb.log').open('w') as log:
        run = subprocess.run(['gdb', '-nx', '-batch', '-x', str(out / 'observe.gdb'),
                              '--args', str(compiler), *FLAGS, '-o', str(out / 'traced.s'), str(source)],
                             stdout=log, stderr=subprocess.STDOUT)
    events_path = out / 'events.jsonl'
    events = [json.loads(line) for line in events_path.read_text().splitlines()] if events_path.exists() else []
    success = (run.returncode == 0 and any(e['event'] == 'function' for e in events)
               and any(e['event'] == 'exit' and e['code'] == 0 for e in events)
               and not any(e['event'] == 'error' for e in events))
    traced = out / 'traced.s'
    metadata['assembly_identical'] = traced.exists() and traced.read_bytes() == (out / 'baseline.s').read_bytes()
    metadata['trace_complete'] = success
    metadata['baseline_assembly_sha256'] = digest((out / 'baseline.s').read_bytes())
    if traced.exists():
        metadata['traced_assembly_sha256'] = digest(traced.read_bytes())
    (out / 'manifest.json').write_text(json.dumps(metadata, indent=2) + '\n')
    report = summarize(events, set(args.regs))
    (out / 'REPORT.txt').write_text(report)
    if not success or not metadata['assembly_identical']:
        print(f'Incomplete or changed-output trace. Inspect {out / "gdb.log"} and events.jsonl.', file=sys.stderr)
        return 1
    if not any(e['event'] == 'global_choice' or e['event'] == 'local_choice' for e in events):
        print('No allocations observed; check --function and whether the function needs registers.', file=sys.stderr)
    print(report, end='')
    print(f'Assembly identical with and without observation. Evidence: {out}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
