#!/usr/bin/env python3
"""Record compiler hypotheses and reproducible build observations in a scratch.

Before editing: ./attempt.py plan base_1.c --parent base.c --hypothesis '...'
    --expect '...' --pass-name lreg
After inspecting dumps: ./attempt.py conclude base_1.c --result '...'
    --next '...'
build.sh records observations automatically, including duplicate assembly.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import uuid

from diagnose import fingerprint

PASSES = ('rtl', 'jump', 'cse', 'addressof', 'loop', 'cse2', 'bp', 'flow',
          'combine', 'sched', 'lreg', 'greg', 'sched2', 'jump2', 'dbr')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None


def read_jsonl(path):
    rows = []
    if path.is_file():
        for line in path.read_text().splitlines():
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError:
                continue  # A killed writer may leave an incomplete last line.
    return rows


def append(path, row):
    row = {'at': datetime.now(timezone.utc).isoformat(), **row}
    incomplete = False
    if path.is_file() and path.stat().st_size:
        with path.open('rb') as old:
            old.seek(-1, 2)
            incomplete = old.read(1) != b'\n'
    with path.open('a') as out:
        if incomplete:
            out.write('\n')
        out.write(json.dumps(row, sort_keys=True) + '\n')


def session(scratch):
    path = scratch / 'session.json'
    if path.is_file():
        return json.loads(path.read_text())
    row = {'id': uuid.uuid4().hex, 'started_at': datetime.now(timezone.utc).isoformat()}
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(row) + '\n')
    temporary.replace(path)
    return row


def normalized_rtl(path):
    if not path.is_file():
        return None
    # Source filenames/line numbers change when base_N.c changes; they are not
    # compiler decisions. Keep instruction IDs, operands and allocation details.
    text = re.sub(r'"[^"\n]*\.(?:i|c|h)"\) \d+', '"SOURCE") LINE', path.read_text())
    return re.sub(r'"[^"\n]*\.(?:i|c|h)"', '"SOURCE"', text)


def record(source, project, compiler=None, flags='', failure=0):
    scratch = source.parent
    session(scratch)
    previous = read_jsonl(scratch / 'attempts.jsonl')
    prior = read_jsonl(scratch / 'PRIOR_ATTEMPTS.jsonl')
    plans = read_jsonl(scratch / 'experiments.jsonl')
    plan = next((p for p in reversed(plans) if p.get('event') == 'plan' and p.get('source') == source.name), {})
    row = {'source': source.name, 'source_sha256': digest(source), 'failure': failure,
           'project': str(project),
           'hypothesis': plan.get('hypothesis'), 'expected': plan.get('expected'),
           'pass_name': plan.get('pass_name'), 'parent': plan.get('parent'),
           'compiler_sha256': digest(compiler) if compiler else None, 'flags': flags,
           'preprocessed_sha256': digest(source.with_suffix('.i')),
           'build_script_sha256': digest(scratch / 'build.sh'),
           'target_sha256': digest(scratch / 'target.o')}
    if not failure:
        score_file = source.with_suffix('.score.json')
        if score_file.is_file():
            row.update(json.loads(score_file.read_text()))
        asm = source.with_name(source.stem + '_object_dump_normalized.s')
        obj = source.with_suffix('.o')
        row['assembly_sha256'] = fingerprint(obj) if obj.is_file() else digest(asm)
        duplicate = next((r for r in previous + prior if r.get('assembly_sha256') == row['assembly_sha256']
                          and row['assembly_sha256'] and r.get('target_sha256') == row['target_sha256']), None)
        row['duplicate_of'] = duplicate.get('source') if duplicate else None
        if duplicate:
            print(f"Repeated assembly: {source.name} reproduces {duplicate['source']}; choose a different hypothesis.")
        parent = scratch / plan.get('parent', 'base.c')
        row['first_changed_pass'] = None
        for pass_name in PASSES:
            old = normalized_rtl(parent.with_suffix('.i.' + pass_name))
            new = normalized_rtl(source.with_suffix('.i.' + pass_name))
            if old is not None and new is not None and old != new:
                row['first_changed_pass'] = pass_name
                break
        if row['first_changed_pass']:
            print('First changed dump vs parent: .' + row['first_changed_pass'] + ' (check operands; IDs can also change).')
    append(scratch / 'attempts.jsonl', row)
    if not plan and source.name != 'base.c':
        print(f"Record a hypothesis with ./attempt.py plan {source.name} --parent base.c --hypothesis '...' --expect '...' --pass-name lreg")
    unique = [r for r in previous + [row] if not r.get('failure') and not r.get('duplicate_of')]
    if len(unique) >= 10:
        scores = [r.get('score', 0) for r in unique]
        best_at = max(range(len(scores)), key=scores.__getitem__)
        if len(unique) - best_at - 1 >= 10:
            print('REASSESS: ten distinct builds without a score gain. Change hypothesis or seed; record what the dumps ruled out.')
    if len(previous) + 1 >= 40:
        print('SESSION BUDGET: 40 builds including failures and repeats. Preserve findings and stop unless an explicit larger budget was authorized.')
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    plan = sub.add_parser('plan')
    plan.add_argument('source', type=Path)
    plan.add_argument('--parent', required=True)
    plan.add_argument('--hypothesis', required=True)
    plan.add_argument('--expect', required=True)
    plan.add_argument('--pass-name', choices=PASSES, required=True)
    done = sub.add_parser('conclude')
    done.add_argument('source', type=Path)
    done.add_argument('--result', required=True)
    done.add_argument('--next', required=True)
    build = sub.add_parser('record')
    build.add_argument('source', type=Path)
    build.add_argument('--project', type=Path, required=True)
    build.add_argument('--compiler', type=Path)
    build.add_argument('--flags', default='')
    build.add_argument('--failure', type=int, default=0)
    args = parser.parse_args()
    source = args.source.absolute()
    if args.command == 'record':
        record(source, args.project, args.compiler, args.flags, args.failure)
    else:
        session(source.parent)
        row = {'event': args.command, 'source': source.name, 'source_sha256': digest(source)}
        if args.command == 'plan':
            row.update(parent=args.parent, hypothesis=args.hypothesis, expected=args.expect, pass_name=args.pass_name)
        else:
            row.update(result=args.result, next=args.next)
        append(source.parent / 'experiments.jsonl', row)


if __name__ == '__main__':
    main()
