#!/usr/bin/env python3
"""Preserve every matching session and merge promising candidates across retries.

Immutable session snapshots retain sources, preprocessed inputs, observations
and notes even at equal/lower scores. A separate manifest selects a primary and
up to five alternatives from old and new candidates. --restore carries that
history and those candidates into the next scratch environment. --reindex
reconsiders existing session evidence without adding a session snapshot.

Exit 0 on write, 2 if skipped.
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import shutil
import sys
import subprocess
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

sys.path.insert(0, str(Path(__file__).resolve().parent))
import vacuum_permute as vp  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parent.parent
GIVEUPS = REPO_ROOT / "tools" / "giveups"


def _rel(path: Path) -> str:
    try:
        return str(path.relative_to(REPO_ROOT))
    except ValueError:
        return str(path)


def load_meta(dest: Path) -> Optional[dict]:
    meta = dest / "meta.json"
    if not meta.is_file():
        return None
    try:
        return json.loads(meta.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return None


def pick_any_seed(scratch: Path) -> Optional[vp.Seed]:
    seeds = vp.parse_match_log(scratch)
    if seeds:
        picked = vp.pick_seed(seeds, min_score=0.0)
        if picked is not None:
            return picked
    rejected = vp.rejected_sources(scratch)
    numbered = sorted(
        scratch.glob("base_*.c"),
        key=lambda p: p.stat().st_mtime,
        reverse=True,
    )
    for path in numbered + ([scratch / "base.c"] if (scratch / "base.c").is_file() else []):
        if path.is_file() and path.stat().st_size > 0:
            if _hash(path.read_bytes()) in rejected:
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            return vp.Seed(
                path=path,
                score=0.0,
                pinned=bool(vp.REGISTER_ASM_RE.search(text)),
            )
    return None


# Penalty dimensions a retry might want a different starting shape for.
# Instruction/address differences are diagnostic dimensions, not a cause.
ALT_DIMENSIONS = {
    "reorder": ("reorder",),
    "regs": ("regs",),
    "stack": ("stack",),
    "cfg": ("branch", "insert", "delete"),
}


def _dimension_cost(seed: vp.Seed, keys: tuple[str, ...]) -> Optional[int]:
    if not seed.penalties:
        return None
    return sum(seed.penalties.get(k, 0) for k in keys)


def pick_alternates(seeds: list[vp.Seed], primary: vp.Seed) -> list[vp.Seed]:
    """Keep useful source shapes, verified permuter gains and diagnostic alternatives."""
    unique = {s.source_hash: s for s in seeds}
    unique.pop(primary.source_hash, None)
    pool = vp.distinct_assembly([s for s in unique.values()
                                if not primary.assembly_sha256 or s.assembly_sha256 != primary.assembly_sha256])
    kept = []
    for choices in (
        [s for s in pool if not s.pinned and s.score >= primary.score - vp.UNPINNED_WINDOW and s.barriers < primary.barriers],
        [s for s in pool if not s.pinned and s.permuter_gain],
    ):
        choices = [s for s in choices if s not in kept]
        if choices:
            best = max(choices, key=lambda s: (s.score, -s.barriers))
            kept.append(best)
            pool.remove(best)
    dimensions = [lambda s, keys=keys: _dimension_cost(s, keys)
                  for keys in ALT_DIMENSIONS.values()]
    dimensions += [lambda s: s.barriers,
                   lambda s: 0 if s.diagnosis and s.diagnosis.get('topology') == 'match' else 1]
    for cost in dimensions:
        baseline = cost(primary)
        choices = [s for s in pool if cost(s) is not None
                   and baseline is not None and cost(s) < baseline]
        if choices:
            best = min(choices, key=lambda s: (cost(s), s.pinned, -s.score, s.source_hash))
            kept.append(best)
            pool.remove(best)
        if len(kept) == 5:
            break
    return kept


def session_candidates(dest: Path) -> list[vp.Seed]:
    """Reconsider archived evidence, including candidates an older shortlist dropped."""
    latest = {}
    for manifest in (dest / 'sessions').glob('*/*/manifest.json'):
        at = json.loads(manifest.read_text())['at']
        session = manifest.parent.parent.name
        if session not in latest or at > latest[session][0]:
            latest[session] = (at, manifest.parent)
    return [seed for _, directory in sorted(latest.values()) for seed in vp.parse_match_log(directory)]


def overlay_info(func: str) -> dict:
    loc = {}
    try:
        import decomp_overlay as ov

        found = ov.find_function(func)
        if found is not None:
            loc = ov.loc_to_dict(found)
    except Exception:
        pass
    return loc


def _hash(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def atomic_write(path: Path, data: bytes):
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_bytes(data)
    temporary.replace(path)


def snapshot(dest: Path, scratch: Path, legacy=False) -> Path:
    """Immutable, content-addressed observations; independent of candidate promotion."""
    session_file = scratch / 'session.json'
    if session_file.is_file():
        session = json.loads(session_file.read_text())
    else:
        session = {'id': 'legacy' if legacy else uuid.uuid4().hex}
        if not legacy:
            atomic_write(session_file, (json.dumps(session) + '\n').encode())
    files = {}
    for path in scratch.iterdir():
        if not path.is_file() or path.is_symlink():
            continue
        if (path.suffix in ('.c', '.i') or path.name.endswith(('.score.json', '.diagnosis.json'))
                or path.name in ('LEARNINGS.md', 'NOTES.md', 'RETRY_NOTES.md', 'notes.md',
                                 'BRIEF.md', 'PRIOR_SEEDS.json', 'PERMUTER.txt', 'PERMUTER.json', 'DUMP.txt',
                                 'PERMUTER_ANALYSIS.md', 'PERMUTER_FOLLOWUP.json',
                                 'attempts.jsonl', 'experiments.jsonl', 'match_log.txt', 'prior_match_log.txt', 'session.json')):
            files[path.name] = path.read_bytes()
    evidence = scratch / 'PERMUTER_EVIDENCE'
    if evidence.is_dir() and not evidence.is_symlink():
        for path in evidence.rglob('*'):
            if path.is_file() and not path.is_symlink() and not any(p.is_symlink() for p in path.parents):
                files[str(path.relative_to(scratch))] = path.read_bytes()
    repo = scratch.parent.parent
    base = repo / '.vacuum-base'
    if base.is_file():
        result = subprocess.run(['git', 'diff', base.read_text().strip(), '--',
                                 'DECOMPILATION_LEARNINGS.md', 'CODEGEN_MODEL.md'],
                                cwd=repo, capture_output=True, timeout=30)
        if result.returncode == 0 and result.stdout:
            files['learnings.patch'] = result.stdout
    manifest = {name: _hash(data) for name, data in sorted(files.items())}
    event = _hash(json.dumps(manifest, sort_keys=True).encode())[:20]
    directory = dest / 'sessions' / session['id'] / event
    if (directory / 'manifest.json').is_file():
        return directory
    directory.mkdir(parents=True, exist_ok=True)
    for name, data in files.items():
        (directory / name).parent.mkdir(parents=True, exist_ok=True)
        if name.endswith('.i'):
            (directory / (name + '.gz')).write_bytes(gzip.compress(data, mtime=0))
        else:
            (directory / name).write_bytes(data)
    atomic_write(directory / 'manifest.json', (json.dumps({
        'at': datetime.now(timezone.utc).isoformat(), 'session': session['id'],
        'scratch': str(scratch), 'files': manifest}, indent=2) + '\n').encode())
    return directory


def write_history(dest: Path):
    records = []
    for path in (dest / 'sessions').glob('*/*/manifest.json'):
        manifest = json.loads(path.read_text())
        records.append((manifest['at'], path.parent))
    lines = ['# Previous matching sessions', '',
             'Scores are historical: recompile candidates against the current headers/compiler.',
             'Read the latest hypotheses/results before choosing a new experiment.', '']
    # Include the latest snapshot of each session; full history remains on disk.
    latest = {}
    for at, directory in sorted(records):
        latest[directory.parent.name] = (at, directory)
    for at, directory in sorted(latest.values())[-8:]:
        lines += [f'## {at}', f'Files: `{_rel(directory)}`', '']
        budget = 4000
        for name in ('PERMUTER_ANALYSIS.md', 'LEARNINGS.md', 'NOTES.md', 'RETRY_NOTES.md', 'notes.md',
                     'experiments.jsonl', 'PERMUTER.txt', 'learnings.patch'):
            path = directory / name
            if path.is_file() and budget > 0:
                content = path.read_text(errors='replace')
                # Summaries stay bounded; original notes are never truncated.
                excerpt = content[-min(1500, budget):]
                budget -= len(excerpt)
                lines += [f'### {name}', excerpt, '']
    (dest / 'HISTORY.md').write_text('\n'.join(lines) + '\n')


def archive_permuter_findings(func: str, scratch: Path) -> str:
    """Keep compiler evidence after a match clears the give-up seed archive."""
    if not (scratch / 'PERMUTER_EVIDENCE').is_dir():
        return 'PERMUTER_FINDINGS_SKIP=no discoveries'
    dest = REPO_ROOT / 'tools/permuter_findings' / func
    observation = snapshot(dest, scratch)
    write_history(dest)
    return f'PERMUTER_FINDINGS_SAVED={_rel(observation)}'


def archive(func: str, scratch: Path, *, reindex=False) -> tuple[int, str]:
    if not scratch.is_dir():
        return 2, f"GIVEUP_SKIP=no scratch at {scratch}"
    dest = GIVEUPS / func
    if reindex:
        latest = (load_meta(dest) or {}).get('latest_session')
        if not latest or not (dest / latest / 'manifest.json').is_file():
            return 2, 'GIVEUP_SKIP=no session history to reindex'
        observation, current = dest / latest, []
    else:
        archive_permuter_findings(func, scratch)
        if dest.exists() and not (dest / 'sessions').exists():
            snapshot(dest, dest, legacy=True)
        dest.mkdir(parents=True, exist_ok=True)
        observation = snapshot(dest, scratch)
        current = vp.parse_match_log(scratch)
    if not current and not reindex:
        fallback = pick_any_seed(scratch)
        if fallback:
            current = [fallback]
    old = vp.parse_match_log(dest)
    prev = load_meta(dest) or {}
    rejected = vp.rejected_sources(dest)
    for manifest in (dest / 'sessions').glob('*/*/manifest.json'):
        rejected.update(vp.rejected_sources(manifest.parent))
    rejected.update(vp.rejected_sources(scratch))
    if not old and (dest / 'base.c').is_file():
        old = [vp.Seed(dest / 'base.c', float(prev.get('score', 0)),
                       bool(prev.get('pinned')), prev.get('penalties'))]
    # Same source rebuilt in this session supersedes stale score/diagnostics.
    merged = {}
    for seed in old + session_candidates(dest) + current:
        previous = merged.get(seed.source_hash)
        if previous is None or seed.assembly_sha256 or not previous.assembly_sha256:
            merged[seed.source_hash] = seed
    seeds = [s for s in merged.values() if s.source_hash not in rejected]
    primary = vp.pick_seed(seeds, 0)
    write_history(dest)
    if primary is None:
        atomic_write(dest / 'meta.json', (json.dumps({
            **prev, 'func': func, 'score': 0, 'seed_name': None,
            'alternates': [], 'rejected_sources': rejected,
            'latest_session': str(observation.relative_to(dest)),
        }, indent=2) + '\n').encode())
        return 0, f'GIVEUP_RECORDED={_rel(observation)} (notes only)'
    selected = [primary] + pick_alternates(seeds, primary)
    # Read every selected source before replacing aliases in the old archive.
    payloads = [(s, s.path.read_bytes()) for s in selected]
    rows, metadata = [], []
    for seed, data in payloads:
        name = 'seed_' + _hash(data)[:20] + '.c'
        (dest / name).write_bytes(data)
        if seed.diagnosis:
            (dest / Path(name).with_suffix('.diagnosis.json')).write_text(json.dumps(seed.diagnosis, indent=2) + '\n')
        rows.append(f'{name} {seed.score:.6f}% {vp.format_penalties(seed.penalties)}')
        metadata.append({'seed_name': name, 'score': seed.score, 'pinned': seed.pinned,
                         'penalties': seed.penalties, 'source_sha256': _hash(data),
                         'barriers': seed.barriers, 'assembly_sha256': seed.assembly_sha256,
                         'permuter_gain': seed.permuter_gain})
    atomic_write(dest / 'base.c', payloads[0][1])
    atomic_write(dest / 'match_log.txt', ('\n'.join(rows) + '\n').encode())
    loc = overlay_info(func)
    recorded_scratch = observation if reindex else scratch
    journal = recorded_scratch / 'attempts.jsonl'
    origin = json.loads((observation / 'manifest.json').read_text())['scratch'] if reindex else _rel(scratch)
    attempts = prev.get('attempts', 0) if reindex else len(current)
    if journal.is_file():
        attempts = sum(bool(line.strip()) for line in journal.read_text().splitlines())
    meta = {**metadata[0], 'func': func, 'alternates': metadata[1:], 'rejected_sources': rejected,
            'attempts': attempts,
            'archived_at': datetime.now(timezone.utc).isoformat(),
            'latest_session': str(observation.relative_to(dest)),
            'overlay': loc.get('overlay'), 'asm_file': loc.get('asm_file'),
            'c_file': loc.get('c_file'), 'scratch': origin}
    atomic_write(dest / 'meta.json', (json.dumps(meta, indent=2) + '\n').encode())
    return 0, f"GIVEUP_SAVED={_rel(dest)} score={primary.score:.3f}% seed={metadata[0]['seed_name']} sessions preserved"


def clear(func: str) -> str:
    dest = GIVEUPS / func
    if dest.is_dir():
        shutil.rmtree(dest)
        return f"GIVEUP_CLEARED={dest.relative_to(REPO_ROOT)}"
    return f"GIVEUP_SKIP=no archive for {func}"


def restore(func: str, scratch: Path):
    dest = GIVEUPS / func
    if not dest.is_dir():
        return
    if (dest / 'meta.json').is_file():
        shutil.copy2(dest / 'meta.json', scratch / 'PRIOR_SEEDS.json')
    rows = []
    for seed in vp.parse_match_log(dest):
        name = 'seed_' + seed.source_hash[:20] + '.c'
        (scratch / name).write_bytes(seed.path.read_bytes())
        rows.append(f'{name} {seed.score:.6f}% {vp.format_penalties(seed.penalties)}')
    if rows:
        (scratch / 'prior_match_log.txt').write_text('\n'.join(rows) + '\n')
    history = dest / 'HISTORY.md'
    if history.is_file():
        shutil.copy2(history, scratch / 'HISTORY.md')
    prior = []
    for path in sorted((dest / 'sessions').glob('*/*/attempts.jsonl')):
        prior.extend(path.read_text().splitlines())
    (scratch / 'PRIOR_ATTEMPTS.jsonl').write_text('\n'.join(dict.fromkeys(prior)) + '\n')


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--func", required=True)
    parser.add_argument("--scratch", type=Path, default=None)
    parser.add_argument('--restore', action='store_true', help='Copy candidates/history into an existing scratch')
    parser.add_argument('--reindex', action='store_true', help='Rebuild the retry shortlist from retained sessions without changing snapshots')
    parser.add_argument('--permuter-findings', action='store_true',
                        help='Retain compiler experiments independently of give-up seeds, including after a match')
    parser.add_argument(
        "--clear",
        action="store_true",
        help="Remove tools/giveups/<func>/ (after a successful match)",
    )
    args = parser.parse_args(argv)

    if args.clear:
        print(clear(args.func))
        return 0
    if args.reindex:
        code, msg = archive(args.func, GIVEUPS / args.func, reindex=True)
        print(msg)
        return code
    if args.scratch is None:
        print("GIVEUP_SKIP=--scratch is required unless --clear or --reindex", file=sys.stderr)
        return 2
    scratch = args.scratch if args.scratch.is_absolute() else REPO_ROOT / args.scratch
    if args.permuter_findings:
        print(archive_permuter_findings(args.func, scratch))
        return 0
    if args.restore:
        restore(args.func, scratch)
        return 0
    code, msg = archive(args.func, scratch)
    print(msg)
    return code


if __name__ == "__main__":
    raise SystemExit(main())
