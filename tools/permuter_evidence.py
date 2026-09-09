"""Retain permutation experiments before the next search cleans its directory.

Search distances are only leads. Rebuild a paired baseline and the best output
with the scratch compiler/scorer, then review source behavior before reuse.
"""
from __future__ import annotations

import difflib
from datetime import datetime, timezone
import gzip
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time
import uuid


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None


def read_rows(path):
    rows = []
    if path.is_file():
        for line in path.read_text().splitlines():
            try:
                rows.append(json.loads(line))
            except ValueError:
                pass
    return rows


def keep_file(source, dest):
    if not source.is_file() or source.is_symlink():
        return
    dest.parent.mkdir(parents=True, exist_ok=True)
    if source.suffix == '.i':
        dest.with_name(dest.name + '.gz').write_bytes(gzip.compress(source.read_bytes(), mtime=0))
    else:
        shutil.copy2(source, dest)


def keep_build(source, directory):
    """Include RTL and object differences, not just a percentage and a C file."""
    for path in source.parent.iterdir():
        if path.name == source.name or path.name.startswith((source.stem + '.', source.stem + '_')):
            keep_file(path, directory / path.name)


def capture(perm_dir, scratch, seed, repo):
    """Copy every complete, source-distinct output; never depend on a live search dir."""
    run_id = uuid.uuid4().hex[:16]
    directory = scratch / 'PERMUTER_EVIDENCE' / run_id
    directory.mkdir(parents=True)
    keep_build(seed, directory / 'seed')
    for name in ('base.c', 'compile.sh', 'settings.toml', 'target.s', 'target.o'):
        keep_file(perm_dir / name, directory / 'search' / name)
    records = read_rows(scratch / 'attempts.jsonl')
    baseline = next((r for r in reversed(records) if r.get('source') == seed.name), {})
    outputs, seen = [], set()
    for path in perm_dir.glob('output-*/source.c'):
        try:
            distance = int((path.parent / 'score.txt').read_text().strip())
            if distance < 0 or path.parent.name.split('-')[1] != str(distance) or not path.stat().st_size:
                continue
        except (OSError, ValueError, IndexError):
            continue  # A killed writer may have left an incomplete output.
        source_hash = digest(path)
        if source_hash in seen:
            continue
        seen.add(source_hash)
        output = directory / path.parent.name
        for name in ('source.c', 'score.txt', 'diff.txt'):
            keep_file(path.parent / name, output / name)
        outputs.append({'path': str(output.relative_to(scratch)), 'source_sha256': source_hash,
                        'search_distance': distance, 'validation': 'not rebuilt',
                        'source_review': 'pending'})
    outputs.sort(key=lambda o: (o['search_distance'], o['path']))
    record = {'id': run_id, 'path': str(directory.relative_to(scratch)), 'seed': seed.name,
              'seed_build': baseline, 'outputs': outputs, 'analysis_at_capture': 'unresolved',
              'tool_sha256': {name: digest(repo / name) for name in (
                  'tools/linux/gcc-2.8.1-psx/cc1', 'tools/maspsx/maspsx.py',
                  'tools/maspsx/maspsx/__init__.py', 'tools/permuter_evidence.py',
                  'tools/vacuum_permute.py', 'tools/prepare_permuter.py',
                  'tools/decomp-permuter-sizeof.patch', 'tools/decomp-permuter-objdump.patch',
                  'tools/decomp-permuter/perm_pycparser/c_parser.py',
                  'tools/decomp-permuter/perm_pycparser/yacctab.py',
                  'tools/decomp-permuter/src/scorer.py', 'tools/decomp-permuter/src/randomizer.py',
                  'tools/claude-decomp-env/dist.py', 'permute.sh')}}
    save(scratch, record)
    return record


def save(scratch, record):
    directory = scratch / record['path']
    # Hash the retained payload too, including the exact compiler script/flags.
    record['files'] = {str(p.relative_to(directory)): digest(p)
                       for p in directory.rglob('*') if p.is_file() and p.name != 'manifest.json'}
    (directory / 'manifest.json').write_text(json.dumps(record, indent=2) + '\n')


def validate(scratch, record, deadline, run_bounded):
    """Two bounded builds. Every raw output remains available if either fails."""
    if not record['outputs']:
        return
    directory = scratch / record['path']
    best = record['outputs'][0]
    parent = scratch / f"base_perm_{record['id']}_parent.c"
    candidate = scratch / f"base_perm_{record['id']}.c"
    shutil.copy2(directory / 'search/base.c', parent)
    shutil.copy2(scratch / best['path'] / 'source.c', candidate)
    record.update(parent=parent.name, candidate=candidate.name)
    (directory / 'source.diff').write_text(''.join(difflib.unified_diff(
        parent.read_text().splitlines(keepends=True), candidate.read_text().splitlines(keepends=True),
        fromfile=parent.name, tofile=candidate.name)))
    observations = []
    for source in (parent, candidate):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            record['validation'] = 'rebuild budget exhausted'
            break
        with (scratch / 'experiments.jsonl').open('a') as out:
            out.write(json.dumps({'at': datetime.now(timezone.utc).isoformat(), 'event': 'plan',
                                  'source': source.name, 'source_sha256': digest(source),
                                  'parent': record['seed'] if source == parent else parent.name,
                                  'hypothesis': 'permuter source normalization' if source == parent else
                                                'permuter transformation improves distance; mechanism unresolved',
                                  'expected': 'same generated code as seed' if source == parent else
                                              'lower distance than paired baseline',
                                  'pass_name': None, 'origin': 'permuter-verification'}) + '\n')
        try:
            proc = run_bounded(['bash', str(scratch / 'build.sh'), source.name], cwd=scratch,
                               text=True, timeout=min(60, remaining))
            (directory / (source.stem + '.build.log')).write_text(proc.stdout + proc.stderr)
            row = next((r for r in reversed(read_rows(scratch / 'attempts.jsonl'))
                        if r.get('source') == source.name), {})
            if proc.returncode or row.get('failure') or row.get('source_sha256') != digest(source):
                record['validation'] = f'{source.name}: rebuild failed or observation missing'
                break
            observations.append(row)
        except (subprocess.TimeoutExpired, OSError) as exc:
            record['validation'] = f'{source.name}: {exc}'
            break
        finally:
            keep_build(source, directory / 'rebuilds')
    record['builds'] = observations
    # Normalization can itself match; retain it even if the second build failed.
    record['exact'] = next((r['source'] for r in reversed(observations) if r.get('distance') == 0), None)
    if len(observations) == 2:
        before, after = observations
        original = record['seed_build']
        record['normalization'] = {'distinct_assembly': original.get('assembly_sha256') != before.get('assembly_sha256'),
                                   'seed_distance': original.get('distance'), 'baseline_distance': before.get('distance')}
        same_inputs = all(before.get(k) is not None and before.get(k) == after.get(k)
                          for k in ('compiler_sha256', 'flags', 'target_sha256', 'build_script_sha256'))
        distinct = (bool(before.get('assembly_sha256') and after.get('assembly_sha256'))
                    and before['assembly_sha256'] != after['assembly_sha256'])
        improved = same_inputs and distinct and after.get('distance', float('inf')) < before.get('distance', 0)
        record.update(validation='improved' if improved else 'improvement not reproduced',
                      comparable_inputs=same_inputs, distinct_assembly=distinct)
        best['validation'] = record['validation']
        if improved:
            record['improvement'] = {'candidate': candidate.name, 'parent': parent.name,
                                     'before': before['distance'], 'after': after['distance'],
                                     'score': after['score'], 'evidence': record['path']}
        elif (all(original.get(k) is not None and original.get(k) == before.get(k)
                  for k in ('compiler_sha256', 'flags', 'target_sha256', 'build_script_sha256'))
              and before.get('distance', float('inf')) < original.get('distance', 0)):
            record['improvement'] = {'candidate': parent.name, 'parent': record['seed'],
                                     'before': original['distance'], 'after': before['distance'],
                                     'score': before['score'], 'evidence': record['path']}
            record['validation'] = 'baseline normalization improved; permutation gain not reproduced'
        for suffix in ('_object_dump_normalized.s', '.s'):
            old, new = scratch / (parent.stem + suffix), scratch / (candidate.stem + suffix)
            if old.is_file() and new.is_file():
                (directory / ('assembly.diff' if suffix.startswith('_') else 'compiler.diff')).write_text(
                    ''.join(difflib.unified_diff(old.read_text().splitlines(keepends=True),
                                                new.read_text().splitlines(keepends=True),
                                                fromfile=old.name, tofile=new.name)))
    save(scratch, record)


def prepare_followup(scratch, report):
    """A durable, initially unresolved deliverable and an eight-build allowance."""
    runs = report.get('experiments', [])
    if not any(r.get('outputs') for r in runs):
        return
    notes = scratch / 'PERMUTER_ANALYSIS.md'
    with notes.open('a') as out:
        out.write('\n## Permuter discoveries\n\nStatus: unresolved; investigation pending.\n\n')
        for run in runs:
            out.write(f"- `{run['path']}`: {run.get('validation', 'not rebuilt')}; "
                      f"seed `{run['seed']}`, {len(run['outputs'])} retained outputs.\n")
            if run.get('improvement'):
                pair = run['improvement']
                out.write(f"  Paired distance: {pair['before']} → {pair['after']}; "
                          f"`{pair['parent']}` → `{pair['candidate']}`.\n")
        out.write('\nRecord the minimal source change, earliest meaningful RTL divergence, observed compiler '
                  'decision, prediction made before a counterfactual build, actual result, and evidence paths. '
                  'Separate observations, hypotheses and supported mechanisms. An unresolved result is valid.\n')
    state = scratch / 'PERMUTER_FOLLOWUP.json'
    if not state.is_file():
        state.write_text(json.dumps({'status': 'active', 'build_budget': 8, 'builds_used': 0,
                                    'start_attempt': len(read_rows(scratch / 'attempts.jsonl'))}, indent=2) + '\n')
