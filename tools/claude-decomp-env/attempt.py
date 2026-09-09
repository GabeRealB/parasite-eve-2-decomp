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
import fcntl
import hashlib
import json
from pathlib import Path
import re
import shutil
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


def reserve_build(scratch):
    state = scratch / 'PERMUTER_FOLLOWUP.json'
    if not state.is_file():
        return 0
    with (scratch / '.permuter-build.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        budget = json.loads(state.read_text())
        if budget['status'] == 'complete':
            print('Permuter investigation is complete; retain its candidate and findings. Start a new session for further search.')
            return 1
        if budget.get('builds_used', 0) >= budget['build_budget']:
            print('PERMUTER BUDGET: eight further builds used. Conclude with supported evidence or unresolved questions; preserve the best candidate.')
            return 1
        # Reserve before compilation, including crashes and early shell failures.
        # A lock prevents simultaneous builds from sharing the last allowance.
        budget['builds_used'] = budget.get('builds_used', 0) + 1
        temporary = state.with_suffix('.tmp')
        temporary.write_text(json.dumps(budget, indent=2) + '\n')
        temporary.replace(state)
    return 0


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
    if len(previous) + 1 >= 40 and not (scratch / 'PERMUTER_FOLLOWUP.json').is_file():
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
    reject = sub.add_parser('reject', help='Keep a disproven source as evidence but exclude its hash from retry seeds')
    reject.add_argument('source', type=Path)
    reject.add_argument('--reason', required=True)
    check = sub.add_parser('reserve-build')
    check.add_argument('scratch', type=Path)
    permuter = sub.add_parser('conclude-permuter')
    permuter.add_argument('source', type=Path)
    permuter.add_argument('--status', required=True, choices=('supported', 'unresolved', 'not-reproduced'))
    permuter.add_argument('--result', required=True)
    permuter.add_argument('--next', required=True)
    permuter.add_argument('--prediction-source', type=Path,
                          help='Required for supported: a controlled variation planned before its successful build')
    permuter.add_argument('--evidence', type=Path, action='append', required=True,
                          help='Retained source/dump/trace or notes; may be repeated')
    build = sub.add_parser('record')
    build.add_argument('source', type=Path)
    build.add_argument('--project', type=Path, required=True)
    build.add_argument('--compiler', type=Path)
    build.add_argument('--flags', default='')
    build.add_argument('--failure', type=int, default=0)
    args = parser.parse_args()
    if args.command == 'reserve-build':
        raise SystemExit(reserve_build(args.scratch.absolute()))
    source = args.source.absolute()
    if args.command == 'record':
        record(source, args.project, args.compiler, args.flags, args.failure)
    else:
        session(source.parent)
        row = {'event': args.command, 'source': source.name, 'source_sha256': digest(source)}
        if args.command == 'plan':
            row.update(parent=args.parent, hypothesis=args.hypothesis, expected=args.expect, pass_name=args.pass_name)
        elif args.command == 'reject':
            if not source.is_file():
                parser.error(f'candidate does not exist: {source}')
            row.update(reason=args.reason)
        else:
            row.update(result=args.result, next=args.next)
        if args.command == 'conclude-permuter':
            if not source.is_file():
                parser.error(f'candidate does not exist: {source}')
            for path in args.evidence:
                if not path.is_file():
                    parser.error(f'evidence does not exist: {path}')
            if args.status == 'supported':
                prediction = args.prediction_source
                if prediction is None or prediction.absolute().parent != source.parent or not prediction.is_file():
                    parser.error('supported requires --prediction-source naming a controlled variation in this scratch')
                plans = read_jsonl(source.parent / 'experiments.jsonl')
                builds = read_jsonl(source.parent / 'attempts.jsonl')
                planned = next((p for p in reversed(plans) if p.get('event') == 'plan'
                                and p.get('source') == prediction.name and not p.get('origin')), {})
                built = next((b for b in reversed(builds) if b.get('source') == prediction.name), {})
                if (not planned.get('expected') or not built.get('at') or built.get('failure')
                        or built.get('source_sha256') != digest(prediction)
                        or planned['at'] >= built['at']):
                    parser.error('prediction must be recorded before a successful build of the retained variation')
                row.update(prediction=planned, prediction_build=built)
            state = source.parent / 'PERMUTER_FOLLOWUP.json'
            if not state.is_file():
                parser.error('no permuter follow-up is active in this scratch')
            budget = json.loads(state.read_text())
            with (source.parent / 'PERMUTER_ANALYSIS.md').open('a') as out:
                out.write(f'\n## Conclusion: {args.status}\n\n{args.result}\n\nNext: {args.next}\n\n')
                out.writelines(f"- `{p}`\n" for p in args.evidence)
            # Hash notes after writing them; a document cannot contain its own hash.
            retained = source.parent / 'PERMUTER_EVIDENCE' / 'conclusions' / uuid.uuid4().hex
            retained.mkdir(parents=True)
            citations = []
            for i, path in enumerate(args.evidence):
                copy = retained / f'{i}-{path.name}'
                shutil.copy2(path, copy)
                citations.append({'path': str(path), 'retained': str(copy.relative_to(source.parent)),
                                  'sha256': digest(copy)})
            # Counterfactual dumps are otherwise scratch-only. Preserve the
            # candidate and variation's complete build evidence automatically.
            for variant in (source, args.prediction_source):
                if variant is None:
                    continue
                for path in variant.absolute().parent.iterdir():
                    if path.is_file() and not path.is_symlink() and path.name.startswith((variant.stem + '.', variant.stem + '_')):
                        dest = retained / variant.stem / path.name
                        dest.parent.mkdir(exist_ok=True)
                        shutil.copy2(path, dest)
            row.update(status=args.status, evidence=citations)
            budget.update(status='complete', conclusion=row,
                          recorded_builds=len(read_jsonl(source.parent / 'attempts.jsonl')) - budget['start_attempt'])
            state.write_text(json.dumps(budget, indent=2) + '\n')
        append(source.parent / 'experiments.jsonl', row)


if __name__ == '__main__':
    main()
