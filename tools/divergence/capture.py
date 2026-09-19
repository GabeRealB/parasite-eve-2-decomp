#!/usr/bin/env python3
"""Retain what a matching session learned, so a later one can use it.

A session that gives up is archived in full - every candidate, the scores, the
hypotheses and their outcomes. A session that *succeeds* is deleted: the scratch
is removed and the archive is cleared, leaving the matched C, an attempt count
in the commit subject, and whatever prose the agent chose to write down. That is
backwards for a corpus, because the cases worth learning from are the ones that
worked, and the pairing worth learning from - the divergence that remained, and
the source change that closed it - only exists while both halves are on disk.

This writes a durable record of a session: what the near-misses looked like,
what the last one still diverged on, and how the source differed from the one
that matched. Records are data rather than code and are kept outside the
repository tree by default.

    python3 tools/divergence/capture.py record --func F --scratch DIR
    python3 tools/divergence/capture.py import-giveups
    python3 tools/divergence/capture.py list
"""

from __future__ import annotations

import argparse
import difflib
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import uuid
from datetime import datetime, timezone

sys.path.insert(0, str(Path(__file__).resolve().parent))
from localize import report  # noqa: E402
from scratch import Attempt  # noqa: E402
from evidence import digest, identity, journal_rows
from report_paths import public_path, public_text
from dumps import PASSES

REPOSITORY = Path(__file__).resolve().parent.parent.parent
DEFAULT_STORE = REPOSITORY / "local" / "divergence" / "records"

SCHEMA = 2


def _now() -> str:
    return datetime.now(timezone.utc).isoformat()


def _commit() -> str:
    """The tree a record was taken against.

    A record ages: the headers a candidate was written against get renamed, and
    a source that compiled when it was stored may not compile later. Recording
    the commit is what lets a reader tell a stale candidate from a wrong one.
    """
    try:
        return subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=REPOSITORY,
            capture_output=True,
            text=True,
            check=True,
        ).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""


def _attempts(directory: Path) -> list[Attempt]:
    found = [
        Attempt(directory, path.name[: -len("_object_dump_normalized.s")])
        for path in sorted(directory.glob("*_object_dump_normalized.s"))
        if path.name != "target_object_dump_normalized.s"
    ]
    return [a for a in found if a.source.is_file()]


def _score_of(attempt: Attempt) -> dict:
    if not attempt.score.is_file():
        return {}
    try:
        return json.loads(attempt.score.read_text())
    except json.JSONDecodeError:
        return {}


def _diff(before: str, after: str) -> list[str]:
    return list(
        difflib.unified_diff(
            before.splitlines(), after.splitlines(), "near-miss", "matched", lineterm="", n=3
        )
    )


def record(
    func: str, scratch: Path, store: Path, matched_source: Path | None = None, note: str = ""
) -> Path:
    """Write one session's record.

    The interesting pairing is between the last attempt that still diverged and
    the source that finally matched, so both are kept along with the divergence
    the near-miss had. Where a session ended without a match, the last attempt
    stands alone and the record is still worth keeping.
    """
    scratch = Path(scratch)
    attempts = _attempts(scratch)
    if not attempts:
        raise FileNotFoundError(f"no completed attempt in {scratch}")

    scored = [(a, _score_of(a)) for a in attempts]
    matching = [a for a, s in scored if s.get("distance") == 0]
    near_misses = [(a, s) for a, s in scored if s.get("distance") not in (0, None)]
    last_near_miss = (
        max(near_misses, key=lambda pair: pair[1].get("score", 0))[0] if near_misses else None
    )

    winner_text = ""
    if matched_source and Path(matched_source).is_file():
        winner_text = Path(matched_source).read_text()
    elif matching:
        winner_text = matching[-1].source.read_text()

    builds = journal_rows(scratch / "attempts.jsonl")
    verified = next((a for a in matching if a.source.read_text() == winner_text and any(
        row.get("source") == a.source.name and row.get("source_sha256") == digest(a.source)
        and row.get("distance") == 0 and not row.get("failure") for row in builds)), None)
    pairing = "closest_scored_attempt; not necessarily the causal parent"
    if verified:
        built = next((row for row in reversed(builds) if row.get("source") == verified.source.name
                      and row.get("source_sha256") == digest(verified.source) and not row.get("failure")), {})
        parent = next((a for a, _ in near_misses if a.source.name == built.get("parent")), None)
        if parent:
            last_near_miss, pairing = parent, "recorded_parent"

    entry = {
        "schema": SCHEMA,
        "function": func,
        "recorded_at": _now(),
        "commit": _commit(),
        "matched": verified is not None,
        "match_basis": "retained distance-zero attempt and source hash verified in build journal; integration unverified" if verified else "unverified or no matched source",
        "pairing": pairing,
        "identities": {a.stem: identity(a) for a in attempts},
        "attempts": len(attempts),
        "note": note,
        "scores": [
            {"stem": a.stem, **s} for a, s in scored if s
        ],
        "near_miss": None,
        "matched_source": winner_text or None,
        "fix": None,
    }

    if last_near_miss is not None:
        try:
            entry["near_miss"] = {
                "stem": last_near_miss.stem,
                "source": last_near_miss.source.read_text(),
                "divergence": report(last_near_miss),
            }
        except Exception as error:  # a partial attempt is still worth recording
            entry["near_miss"] = {
                "stem": last_near_miss.stem,
                "source": last_near_miss.source.read_text(),
                "divergence_error": public_text(str(error)),
            }
        if winner_text:
            entry["fix"] = _diff(last_near_miss.source.read_text(), winner_text)

    store = Path(store)
    store.mkdir(parents=True, exist_ok=True)
    name = f"{Path(func).name}.{uuid.uuid4().hex[:12]}"
    path = store / f"{name}.json"
    artifacts = {scratch / name for name in ("target.o", "target_object_dump_normalized.s", "attempts.jsonl", "experiments.jsonl")}
    for a in attempts:
        artifacts.update([a.source, a.preprocessed, a.cc1_assembly, a.ours, a.score,
                          scratch / f"{a.stem}.o", scratch / f"{a.stem}.diagnosis.json"])
        artifacts.update(a.dump(p) for p in PASSES)
    artifacts = sorted(p for p in artifacts if p.is_file() and not p.is_symlink())
    archive = store / f"{name}.tar.gz"
    with tarfile.open(archive, "x:gz") as bundle:
        for artifact in artifacts:
            bundle.add(artifact, arcname=artifact.name, recursive=False)
    entry["artifacts"] = {"archive": archive.name, "sha256": digest(archive),
                          "files": {p.name: digest(p) for p in artifacts}}
    path.write_text(json.dumps(entry, indent=2))
    return path


def import_giveups(giveups: Path, store: Path, limit: int | None = None) -> list[Path]:
    """Backfill records from the sessions that were archived rather than cleared.

    These carry no fix, since nothing matched, but they carry the shape of a
    hard divergence and the candidates that got closest, which is what a later
    retry wants to start from.
    """
    written: list[Path] = []
    for directory in sorted(Path(giveups).iterdir()):
        if limit is not None and len(written) >= limit:
            break
        meta_path = directory / "meta.json"
        if not directory.is_dir() or not meta_path.is_file():
            continue
        try:
            meta = json.loads(meta_path.read_text())
        except json.JSONDecodeError:
            continue
        candidates = sorted(directory.glob("seed_*.c")) + sorted(directory.glob("base*.c"))
        entry = {
            "schema": SCHEMA,
            "function": meta.get("func", directory.name),
            "recorded_at": _now(),
            "commit": "",
            "matched": False,
            "attempts": meta.get("attempts"),
            "note": "imported from an archived give-up; sources may predate later renames",
            "scores": [
                {
                    "stem": meta.get("seed_name", ""),
                    "score": meta.get("score"),
                    "penalties": meta.get("penalties", {}),
                }
            ],
            "near_miss": {
                "stem": meta.get("seed_name", ""),
                "source": candidates[0].read_text() if candidates else None,
                "divergence": None,
            },
            "matched_source": None,
            "fix": None,
            "overlay": meta.get("overlay"),
            "asm_file": meta.get("asm_file"),
            "c_file": meta.get("c_file"),
        }
        store.mkdir(parents=True, exist_ok=True)
        path = store / f"{Path(entry['function']).name}.imported.json"
        if path.exists():
            continue
        path.write_text(json.dumps(entry, indent=2))
        written.append(path)
    return written


def load_all(store: Path) -> list[dict]:
    entries: list[dict] = []
    store = Path(store)
    if not store.is_dir():
        return entries
    for path in sorted(store.glob("*.json")):
        try:
            entries.append(json.loads(path.read_text()))
        except json.JSONDecodeError:
            continue
    return entries


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--store", type=Path, default=DEFAULT_STORE)
    sub = parser.add_subparsers(dest="command", required=True)

    one = sub.add_parser("record", help="record one session")
    one.add_argument("--func", required=True)
    one.add_argument("--scratch", type=Path, required=True)
    one.add_argument("--matched-source", type=Path, help="the source that finally matched")
    one.add_argument("--note", default="")

    backfill = sub.add_parser("import-giveups", help="backfill from archived give-ups")
    backfill.add_argument("--giveups", type=Path, default=REPOSITORY / "tools" / "giveups")
    backfill.add_argument("--limit", type=int)

    sub.add_parser("list", help="list what is stored")

    args = parser.parse_args()

    if args.command == "record":
        path = record(args.func, args.scratch, args.store, args.matched_source, args.note)
        print(f"wrote {public_path(path)}")
    elif args.command == "import-giveups":
        written = import_giveups(args.giveups, args.store, args.limit)
        print(f"wrote {len(written)} records to {public_path(args.store)}")
    else:
        entries = load_all(args.store)
        for entry in entries:
            best = max((s.get("score") or 0 for s in entry.get("scores", [])), default=0)
            state = "matched" if entry.get("matched") else f"best {best:.3f}%"
            fix = " +fix" if entry.get("fix") else ""
            print(f"{entry['function']:<44} {state}{fix}")
        print(f"\n{len(entries)} records in {public_path(args.store)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
