#!/usr/bin/env python3
"""Pick a ≥95% scratch seed and run decomp-permuter until score 0 or timeout.

Intended as a vacuum *post-step* after the agent gives up. Prefers an unpinned
seed (register-asm pins shrink the search). Uses structural diagnostics to select several candidates within one time budget.
Always records selection and skip reasons, retains full-context discoveries,
and reports verified improvements for a bounded compiler investigation.

Exit codes:
  0  candidate for follow-up: verified HIT/IMPROVEMENT, or unverified REVIEW
  1  ran without a discovery
  2  skipped (PERMUTER_SKIP=...)
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import signal
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

import permuter_evidence as evidence

REPO_ROOT = Path(__file__).resolve().parent.parent
MIN_SCORE_DEFAULT = 95.0
UNPINNED_WINDOW = 1.0
REGISTER_ASM_RE = re.compile(
    r'\bregister\s+([^;]*?)\s+(?:asm|__asm__)\s*\(\s*"[^"]*"\s*\)',
    re.S,
)


PENALTY_KEYS = ("stack", "branch", "regs", "reorder", "insert", "delete")


@dataclass
class Seed:
    path: Path
    score: float
    pinned: bool
    penalties: Optional[dict] = None
    diagnosis: Optional[dict] = None
    assembly_sha256: Optional[str] = None
    permuter_gain: bool = False

    @property
    def source_hash(self):
        return hashlib.sha256(self.path.read_bytes()).hexdigest()

    @property
    def barriers(self):
        # Count asm helpers too, and ignore mentions in comments/string literals.
        source = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                        ' ', self.path.read_text(), flags=re.S)
        return len(re.findall(r'\b(?:SOFT_BARRIER|SCHED_BARRIER|SOFT_USE_REG|(?:SOFT_)?TOUCH_REG(?:_USE)?|asm|__asm(?:__)?)\s*\(', source))


def parse_penalties(fields: list[str]) -> Optional[dict]:
    out: dict[str, int] = {}
    for field in fields:
        if "=" not in field:
            continue
        key, _, raw = field.partition("=")
        if key not in PENALTY_KEYS:
            continue
        try:
            out[key] = int(raw)
        except ValueError:
            return None
    return out or None


def rejected_sources(scratch: Path) -> dict[str, str]:
    """Semantic/source review is separate from a successful compiler score."""
    rejected = {}
    for name in ('PRIOR_SEEDS.json', 'meta.json'):
        path = scratch / name
        if path.is_file():
            rejected.update(json.loads(path.read_text()).get('rejected_sources', {}))
    for row in evidence.read_rows(scratch / 'experiments.jsonl'):
        if row.get('event') == 'reject' and row.get('source_sha256'):
            rejected[row['source_sha256']] = row['reason']
    return rejected


def parse_match_log(scratch: Path) -> list[Seed]:
    logs = [scratch / 'prior_match_log.txt', scratch / 'match_log.txt']
    if not any(log.is_file() for log in logs):
        return []
    observations = {}
    aliases = {}
    rejected = rejected_sources(scratch)
    metadata = scratch / 'meta.json'
    if not metadata.is_file():
        metadata = scratch / 'PRIOR_SEEDS.json'
    if metadata.is_file():
        meta = json.loads(metadata.read_text())
        aliases = {r.get('source_sha256'): r for r in [meta] + meta.get('alternates', [])}
    journal = scratch / 'attempts.jsonl'
    if journal.is_file():
        for line in journal.read_text().splitlines():
            try:
                row = json.loads(line)
                observations[row['source']] = row
            except (ValueError, KeyError):
                continue
    seeds: dict[str, Seed] = {}
    lines = [line for log in logs if log.is_file()
             for line in log.read_text(encoding='utf-8', errors='replace').splitlines()]
    for line in lines:
        parts = line.split()
        if len(parts) < 2:
            continue
        name = Path(parts[0]).name
        if not re.fullmatch(r"[\w.-]+\.c", name):
            continue
        try:
            score = float(parts[1].rstrip("%"))
        except ValueError:
            continue
        path = scratch / name
        if not path.is_file():
            continue
        if hashlib.sha256(path.read_bytes()).hexdigest() in rejected:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        observation = observations.get(name)
        if observation:
            if observation.get('failure') or observation.get('source_sha256') != hashlib.sha256(path.read_bytes()).hexdigest():
                continue
            score = observation.get('score', score)
        alias = aliases.get(hashlib.sha256(path.read_bytes()).hexdigest(), {})
        parent = observations.get((observation or {}).get('parent'), {})
        parent_path = scratch / parent.get('source', '')
        permuter_gain = bool(observation and parent and not parent.get('failure')
                             and observation.get('parent', '').startswith('base_perm_')
                             and parent_path.is_file()
                             and parent.get('source_sha256') == hashlib.sha256(parent_path.read_bytes()).hexdigest()
                             and all(observation.get(k) is not None and observation.get(k) == parent.get(k)
                                     for k in ('compiler_sha256', 'flags', 'target_sha256'))
                             and observation.get('distance', float('inf')) < parent.get('distance', 0))
        diagnosis = None
        sidecar = path.with_suffix('.diagnosis.json')
        if sidecar.is_file():
            try:
                diagnosis = json.loads(sidecar.read_text())
                if diagnosis.get('source_sha256') != hashlib.sha256(path.read_bytes()).hexdigest():
                    diagnosis = None
            except (ValueError, OSError):
                pass
        seeds[name] = Seed(
                path=path,
                score=score,
                pinned=bool(REGISTER_ASM_RE.search(text)),
                penalties=parse_penalties(parts[2:]),
                diagnosis=diagnosis,
                assembly_sha256=(observation or alias).get('assembly_sha256'),
                permuter_gain=permuter_gain or alias.get('permuter_gain', False),
            )
    return list(seeds.values())


def eligibility(seed: Seed, min_score: float) -> tuple[bool, str]:
    if seed.pinned:
        return False, 'register-pinned source; unpin and rebuild as a separate experiment'
    if seed.score >= 100:
        return False, 'score is already zero; port and run full verification'
    if seed.score < min_score - 5:
        return False, f'score below alternate floor {min_score - 5:g}%'
    diagnosis = seed.diagnosis or {}
    target = seed.path.parent / 'target.o'
    candidate = seed.path.with_suffix('.o')
    for path, key in ((target, 'target_sha256'), (candidate, 'object_sha256')):
        if not path.is_file() or diagnosis.get(key) != hashlib.sha256(path.read_bytes()).hexdigest():
            diagnosis = {}
            break
    if diagnosis.get('topology') == 'different':
        return False, 'block connections differ; inspect .jump/.jump2'
    if diagnosis.get('topology') == 'match':
        if not diagnosis.get('predicates_match') or not diagnosis.get('calls_match'):
            return False, 'branch predicates or call targets differ or are unresolved'
        # Structure matching is evidence for a trial, not semantic validation.
        return True, 'block connections, predicates and call targets match; bounded allocation/scheduling trial'
    penalties = seed.penalties
    if seed.score >= min_score and penalties and all(penalties.get(k) == 0 for k in ('branch', 'insert', 'delete')):
        return True, 'legacy register/scheduling-only penalties; structure unavailable'
    return False, 'structure unknown; rebuild or investigate indirect control flow before search'


def distinct_assembly(seeds: list[Seed]) -> list[Seed]:
    """Prefer a compact source for each observed object; keep unobserved sources distinct."""
    groups = {}
    for seed in seeds:
        groups.setdefault(seed.assembly_sha256 or seed.source_hash, []).append(seed)
    result = []
    for group in groups.values():
        seed = min(group, key=lambda s: (s.pinned, -round(s.score, 6), s.barriers,
                                        s.path.stat().st_size, s.path.name))
        seed.permuter_gain = any(s.permuter_gain for s in group)
        result.append(seed)
    return result


def search_candidates(seeds: list[Seed], min_score: float, limit: int) -> list[Seed]:
    """A bounded, source-distinct set, including lower-score alternatives."""
    pool = distinct_assembly([s for s in seeds if s.score >= min_score - 5 and not s.pinned])
    if not pool or max(s.score for s in pool) < min_score:
        return []
    first = max(pool, key=lambda s: s.score)
    chosen = [first]
    pool.remove(first)
    # Give the retained source-shape and permuter alternatives an actual search
    # slot, instead of spending the budget on another copy of the same object.
    for choices in (
        [s for s in pool if s.score >= first.score - UNPINNED_WINDOW and s.barriers < first.barriers],
        [s for s in pool if s.permuter_gain],
    ):
        choices = [s for s in choices if s in pool]
        if choices and len(chosen) < limit:
            candidate = max(choices, key=lambda s: (s.score, -s.barriers))
            chosen.append(candidate)
            pool.remove(candidate)
    axes = [lambda s: sum((s.penalties or {}).get(k, 10**6) for k in ('branch', 'insert', 'delete')),
            lambda s: s.barriers, lambda s: (s.penalties or {}).get('regs', 10**6),
            lambda s: (s.penalties or {}).get('reorder', 10**6)]
    for cost in axes:
        if len(chosen) >= limit or not pool:
            break
        candidate = min(pool, key=lambda s: (cost(s), -s.score, s.source_hash))
        chosen.append(candidate)
        pool.remove(candidate)
    return chosen


def format_penalties(penalties: Optional[dict]) -> str:
    if not penalties:
        return ""
    return " ".join(f"{k}={penalties.get(k, 0)}" for k in PENALTY_KEYS)


def pick_seed(seeds: list[Seed], min_score: float) -> Optional[Seed]:
    eligible = [s for s in seeds if s.score >= min_score]
    if not eligible:
        return None
    best = max(s.score for s in eligible)
    unpinned = [s for s in eligible if not s.pinned and s.score >= best - UNPINNED_WINDOW]
    pool = unpinned or eligible
    pool.sort(key=lambda s: (-round(s.score, 6), s.barriers, s.path.stat().st_size, s.path.name))
    return pool[0]


def strip_register_asm(src: str) -> str:
    """Drop GNU `register ... asm("reg")` pins. Leave `__asm__` / GTE macros."""
    return REGISTER_ASM_RE.sub(r"\1", src)


def _python() -> str:
    venv = REPO_ROOT / "venv" / "bin" / "python"
    return str(venv) if venv.is_file() else sys.executable


def _nproc_jobs(requested: int) -> int:
    if requested > 0:
        return requested
    return max(1, min(os.cpu_count() or 4, 8))


def run_bounded(cmd, *, timeout, **kwargs):
    """A timeout also terminates compiler/search descendants, not just the shell."""
    with subprocess.Popen(cmd, start_new_session=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, **kwargs) as process:
        try:
            stdout, stderr = process.communicate(timeout=timeout)
        except subprocess.TimeoutExpired as exc:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            exc.stdout, exc.stderr = process.communicate()
            raise
        return subprocess.CompletedProcess(cmd, process.returncode, stdout, stderr)


def run_permuter(
    func: str,
    scratch: Path,
    asm_file: Path,
    seed: Path,
    timeout: int,
    jobs: int,
) -> tuple[str, Optional[dict]]:
    """Search, retain every discovery, then rebuild a pair within this seed's budget."""
    perm_dir = REPO_ROOT / "permuter" / func
    started = time.monotonic()
    deadline = started + timeout
    setup = run_bounded(
        ["./permute.sh", "--clean", func, str(asm_file), str(seed)],
        cwd=REPO_ROOT,
        text=True,
        timeout=max(1, min(60, timeout)),
    )
    timeout = timeout - (time.monotonic() - started)
    if timeout <= 0:
        return 'budget exhausted during setup', None
    if setup.returncode != 0:
        return (
            f"setup failed: {(setup.stderr or setup.stdout).strip()[-400:]}",
            None,
        )

    cmd = [
        _python(),
        str(REPO_ROOT / "tools" / "decomp-permuter" / "permuter.py"),
        f"-j{jobs}",
        "--better-only",
        "--stop-on-zero",
        "--algorithm",
        "levenshtein",
        str(perm_dir),
    ]
    # Private TMPDIR that goes away with the run: the permuter leaves a scratch
    # file per candidate compile behind, and a shared /tmp fills up (12G in one
    # long search here). permute.sh's cleaner does not cover this call path.
    # Reserve time for paired verification. Search and verification still share
    # the router's total wall-clock budget; copying evidence never gets skipped.
    search_timeout = max(0.1, timeout - min(30, timeout / 3))
    log = ''
    with tempfile.TemporaryDirectory(prefix=f"permuter-{func}-") as tmpdir:
        env = {**os.environ, "TMPDIR": tmpdir}
        try:
            proc = run_bounded(cmd, cwd=REPO_ROOT, text=True, env=env, timeout=search_timeout)
            log = (proc.stdout or '') + (proc.stderr or '')
            status = f'search exit {proc.returncode}'
        except subprocess.TimeoutExpired as exc:
            chunks = [exc.stdout or '', exc.stderr or '']
            log = '\n'.join(c.decode(errors='replace') if isinstance(c, bytes) else c for c in chunks)
            status = f'search timeout after {search_timeout:.1f}s'
    # The next seed uses --clean on the same search directory. Copy first,
    # including full declaration context, even on a nonzero exit or timeout.
    record = evidence.capture(perm_dir, scratch, seed, REPO_ROOT)
    (scratch / record['path'] / 'search.log').write_text(log)
    evidence.validate(scratch, record, deadline, run_bounded)
    evidence.save(scratch, record)
    return status, record


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--func', required=True)
    parser.add_argument('--scratch', required=True, type=Path)
    parser.add_argument('--asm', default='')
    parser.add_argument('--min-score', type=float, default=MIN_SCORE_DEFAULT)
    parser.add_argument('--timeout', type=int, default=360, help='Total budget across rebuilds, setup and all searches')
    parser.add_argument('--jobs', type=int, default=0)
    parser.add_argument('--max-seeds', type=int, default=3)
    parser.add_argument('--setup-only', action='store_true', help='Report selection and eligibility without compiling/searching')
    args = parser.parse_args(argv)
    if args.timeout < 1 or not 1 <= args.max_seeds <= 6:
        parser.error('timeout must be positive; max-seeds must be 1..6')
    scratch = args.scratch.absolute()
    if not scratch.is_dir():
        print(f'PERMUTER_SKIP=no scratch at {scratch}')
        return 2
    # A matching agent may already have run the router. Neither a second caller
    # nor setup-only inspection should erase its discoveries or reset its budget.
    if not args.setup_only and (scratch / 'PERMUTER_FOLLOWUP.json').is_file():
        print(f'PERMUTER_SKIP=existing investigation; read {scratch / "PERMUTER_ANALYSIS.md"}')
        return 2
    report = {'func': args.func, 'budget_seconds': args.timeout, 'candidates': [],
              'experiments': [], 'status': 'pending'}
    def finish(code, status):
        report['status'] = status
        if not args.setup_only:
            evidence.prepare_followup(scratch, report)
        report_name = 'PERMUTER_SETUP' if args.setup_only else 'PERMUTER'
        (scratch / (report_name + '.json')).write_text(json.dumps(report, indent=2) + '\n')
        lines = [f'status={status}']
        lines += [f"{r['seed']}: {r.get('status', '')} {r.get('reason', '')}" for r in report['candidates']]
        lines += [f"Evidence: {r['path']} — {r.get('validation', 'not rebuilt')}" for r in report['experiments']]
        if report.get('candidate'):
            lines += [f"Follow-up candidate: {report['candidate']}", 'Read PERMUTER_ANALYSIS.md; investigate within eight further builds.']
        (scratch / (report_name + '.txt')).write_text('\n'.join(lines) + '\n')
        if args.setup_only:
            for line in lines[1:]:
                print('PERMUTER_CANDIDATE=' + line)
        print(('PERMUTER_MISS=' if code == 1 else 'PERMUTER_SKIP=' if code == 2 else 'PERMUTER_DONE=') + status)
        # Standalone router calls also retain discoveries beyond scratch cleanup
        # and successful-match removal of tools/giveups/<func>.
        if any(r.get('outputs') for r in report['experiments']):
            archived = subprocess.run([sys.executable, str(REPO_ROOT / 'tools/archive_giveup.py'),
                                       '--func', args.func, '--scratch', str(scratch), '--permuter-findings'],
                                      cwd=REPO_ROOT, capture_output=True, text=True)
            print(archived.stdout.strip())
            if archived.returncode:
                print('PERMUTER_ARCHIVE_FAILED=retain scratch; ' + archived.stderr.strip(), file=sys.stderr)
        return code
    seeds = parse_match_log(scratch)
    selected = search_candidates(seeds, args.min_score, args.max_seeds)
    if not selected:
        report['candidates'] = [{'seed': s.path.name, 'score': s.score, 'status': 'skipped',
                                 'reason': eligibility(s, args.min_score)[1]} for s in seeds]
        return finish(2, 'no unpinned seed reaches the primary score threshold')
    deadline = time.monotonic() + args.timeout
    asm_file = Path(args.asm).absolute() if args.asm else None
    if not args.setup_only and asm_file is None:
        try:
            loc = subprocess.run([sys.executable, str(REPO_ROOT / 'tools/decomp_overlay.py'), 'find', args.func, '--json'],
                                 cwd=REPO_ROOT, capture_output=True, text=True, timeout=min(30, args.timeout))
            asm_file = REPO_ROOT / json.loads(loc.stdout)['asm_file']
        except (ValueError, KeyError, subprocess.TimeoutExpired):
            return finish(2, 'cannot resolve target assembly')
    if not args.setup_only and not asm_file.is_file():
        return finish(2, 'target assembly is missing')
    ran = False
    for index, seed in enumerate(selected):
        entry = {'seed': seed.path.name, 'source_sha256': seed.source_hash, 'score': seed.score}
        report['candidates'].append(entry)
        left = deadline - time.monotonic()
        if left < 1:
            entry.update(status='skipped', reason='total budget exhausted')
            continue
        if not args.setup_only:
            # Archived scores/diagnostics describe old headers and compiler inputs.
            # Rebuild each selected source before deciding whether to search it.
            try:
                build = run_bounded(['bash', str(scratch / 'build.sh'), seed.path.name], cwd=scratch,
                                    text=True, timeout=max(1, min(60, left)))
                (scratch / (seed.path.stem + '.permute-build.log')).write_text(build.stdout + build.stderr)
                if build.returncode:
                    entry.update(status='skipped', reason=f'candidate rebuild failed ({build.returncode})')
                    continue
                seed = next(s for s in parse_match_log(scratch) if s.path == seed.path)
                entry['score'] = seed.score
                score_file = seed.path.with_suffix('.score.json')
                if score_file.is_file() and json.loads(score_file.read_text()).get('distance') == 0:
                    report['winner'] = str(seed.path)
                    report.update(candidate=seed.path.name, seed=seed.path.name)
                    entry.update(status='score zero on rebuild', reason='port and full verification required')
                    finish(0, 'score zero found; port and full build verification required')
                    print(f'PERMUTER_SEED={seed.path} score={seed.score:.3f}% pinned=0')
                    print(f'PERMUTER_HIT={seed.path}')
                    return 0
            except (subprocess.TimeoutExpired, StopIteration, OSError) as exc:
                entry.update(status='skipped', reason=f'candidate rebuild unavailable: {exc}')
                continue
        allowed, reason = eligibility(seed, args.min_score)
        entry.update(reason=reason, diagnosis=seed.diagnosis, status='eligible' if allowed else 'skipped')
        if not allowed or args.setup_only:
            continue
        remaining = deadline - time.monotonic()
        if remaining < 1:
            entry.update(status='skipped', reason='total budget exhausted')
            continue
        budget = max(1, int(remaining / (len(selected) - index)))
        print(f'PERMUTER_SEED={seed.path} score={seed.score:.3f}% pinned=0')
        print(f'PERMUTER_RUN timeout={budget}s jobs={_nproc_jobs(args.jobs)}')
        ran = True
        try:
            status, experiment = run_permuter(args.func, scratch, asm_file, seed.path, budget, _nproc_jobs(args.jobs))
        except (subprocess.TimeoutExpired, OSError) as exc:
            status, experiment = f'setup/search failed: {exc}', None
        entry.update(status=status, budget_seconds=budget)
        if experiment:
            report['experiments'].append({k: v for k, v in experiment.items() if k not in ('files', 'tool_sha256', 'builds', 'seed_build')})
        if experiment and experiment.get('exact'):
            winner = scratch / experiment['exact']
            report['winner'] = str(winner)
            report.update(candidate=winner.name, seed=seed.path.name)
            finish(0, 'score zero found; port and full build verification required')
            print(f'PERMUTER_HIT={winner}')
            return 0
    improved = [r for r in report['experiments'] if r.get('improvement')]
    if improved:
        best = min(improved, key=lambda r: r['improvement']['after'])
        report.update(candidate=best['improvement']['candidate'], seed=best['seed'])
        finish(0, 'distance improvement verified; review source behavior and compiler mechanism before reuse')
        print(f"PERMUTER_SEED={scratch / best['seed']}")
        print(f"PERMUTER_IMPROVEMENT={scratch / report['candidate']}")
        return 0
    leads = [r for r in report['experiments'] if r.get('outputs') and r.get('candidate')]
    if leads:
        # A short rebuild budget or changed preprocessing must not suppress the
        # handoff of a possible match. Label it explicitly, never as a verified hit.
        best = min(leads, key=lambda r: r['outputs'][0]['search_distance'])
        report.update(candidate=best['candidate'], seed=best['seed'])
        finish(0, 'search discovery needs review; improvement not verified')
        print(f"PERMUTER_REVIEW={scratch / report['candidate']}")
        return 0
    return finish(1 if ran else 2, 'search finished without a discovery' if ran else 'setup-only' if args.setup_only else 'all candidates skipped')


if __name__ == '__main__':
    raise SystemExit(main())
