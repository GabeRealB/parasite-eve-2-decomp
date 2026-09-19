"""Bounded, retained source experiments using the existing scratch build path."""
from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import time
import uuid

from constraints import evaluate, validate
from localize import report
from scratch import Attempt
from evidence import digest, artifact_provenance
from report_paths import public_path, public_text


def apply_edits(source: str, edits: list[dict]) -> str:
    """Literal, unique replacements preserve formatting and declaration context."""
    text = source
    for edit in edits:
        before, after = edit.get("before"), edit.get("after")
        if not isinstance(before, str) or not before or not isinstance(after, str):
            raise ValueError("each edit needs nonempty 'before' and string 'after'")
        if text.count(before) != 1:
            raise ValueError(f"edit must match exactly once: {before!r}")
        text = text.replace(before, after, 1)
    return text


def run_process(argv, cwd, log, timeout):
    start = time.monotonic()
    with Path(log).open("w") as output:
        try:
            process = subprocess.Popen(argv, cwd=cwd, stdout=output, stderr=subprocess.STDOUT, start_new_session=True)
        except OSError as error:
            output.write(str(error) + "\n")
            return {"status": "failed", "returncode": None, "error": public_text(str(error)), "seconds": time.monotonic() - start}
        try:
            code = process.wait(timeout=timeout)
            status = "complete" if code == 0 else "failed"
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
            code, status = None, "timeout"
    return {"status": status, "returncode": code, "seconds": time.monotonic() - start}


class Runner:
    def __init__(self, baseline: Attempt, output: Path, budget=8, timeout=60, constraints=()):
        if budget < 1 or timeout <= 0:
            raise ValueError("budget and timeout must be positive")
        self.baseline, self.output = baseline, Path(output)
        if not baseline.source.is_file() or not (baseline.directory / "build.sh").is_file():
            raise ValueError("source and scratch build.sh are required")
        if artifact_provenance(baseline)["status"] == "mismatch":
            raise ValueError("baseline artifacts disagree with its build journal; rebuild a coherent baseline before experimenting")
        self.constraints = validate(list(constraints))
        self.output.mkdir(parents=True, exist_ok=False)
        self.budget, self.timeout, self.used = budget, timeout, 0
        self.run_id = uuid.uuid4().hex[:12]
        self.target_hash = digest(baseline.target)
        self.rows = []
        self.cache = {}
        self.base_report = report(baseline)
        self.save()

    def save(self):
        rows = [{**row, **{key: public_path(row[key]) for key in ("source", "log") if key in row},
                 **({"error": public_text(row["error"])} if "error" in row else {})} for row in self.rows]
        data = {"schema": 1, "run_id": self.run_id, "baseline": str(self.baseline),
                "identity": self.base_report["identity"], "budget": self.budget, "used": self.used,
                "constraints": self.constraints, "experiments": rows,
                "acceptance": "A score gain or satisfied constraint does not establish source semantics or integration."}
        (self.output / "manifest.json").write_text(json.dumps(data, indent=2) + "\n")

    def build(self, source, label, parent=None, hypothesis=None):
        fingerprint = hashlib.sha256(source.encode()).hexdigest()
        if fingerprint in self.cache:
            return self.cache[fingerprint]
        if self.used >= self.budget:
            raise RuntimeError("experiment build budget exhausted")
        if digest(self.baseline.target) != self.target_hash:
            raise ValueError("target changed during experiment")
        self.used += 1
        stem = f"divergence_{self.run_id}_{self.used}"
        candidate = Attempt(self.baseline.directory, stem, self.baseline.function)
        with candidate.source.open("x") as out:
            out.write(source)
        row = {"label": label, "source": str(candidate.source), "source_sha256": fingerprint,
               "parent": parent or self.baseline.stem, "hypothesis": hypothesis,
               "started_at": datetime.now(timezone.utc).isoformat(), "status": "running"}
        self.rows.append(row)
        self.save()
        # Record the prediction through the existing session journal. build.sh
        # enforces its normal session/follow-up limits; this runner never resets them.
        journal = self.baseline.directory / "attempt.py"
        if journal.is_file():
            plan_log = self.output / f"{stem}.plan.log"
            plan = run_process(["python3", str(journal.resolve()), "plan", str(candidate.source.resolve()),
                                   "--parent", row["parent"] + ("" if row["parent"].endswith(".c") else ".c"),
                                   "--hypothesis", hypothesis or label, "--expect", json.dumps(self.constraints),
                                   "--pass-name", "rtl"], self.baseline.directory, plan_log, self.timeout)
            if plan["status"] != "complete":
                row.update(status="journal_failed", journal=plan, log=str(plan_log))
                self.cache[fingerprint] = row
                self.save()
                return row
        log = self.output / f"{stem}.log"
        outcome = run_process(["bash", str((self.baseline.directory / "build.sh").resolve()), str(candidate.source.resolve())],
                              self.baseline.directory, log, self.timeout)
        row.update(outcome, log=str(log))
        if outcome["status"] == "complete":
            if not candidate.complete or not candidate.score.is_file():
                row.update(status="missing_artifacts", error="build returned success without candidate assembly/score")
            else:
                try:
                    diagnosis = report(candidate)
                    if type(diagnosis["score"].get("distance")) not in (int, float):
                        raise ValueError("candidate score has no numeric distance")
                    row.update(score=diagnosis["score"], identity=diagnosis["identity"],
                               constraints=evaluate(candidate, diagnosis, self.constraints), families=diagnosis["families"])
                    (self.output / f"{stem}.report.json").write_text(json.dumps(diagnosis, indent=2) + "\n")
                except (ValueError, OSError) as error:
                    row.update(status="analysis_failed", error=str(error))
        self.cache[fingerprint] = row
        self.save()
        return row


def load_plan(path):
    plan = json.loads(Path(path).read_text())
    validate(plan.get("constraints", []))
    mutations = plan.get("mutations", [])
    if not mutations:
        raise ValueError("plan needs at least one mutation with id and edits")
    names = set()
    for mutation in mutations:
        if not mutation.get("id") or mutation["id"] in names or not mutation.get("edits"):
            raise ValueError("mutations need unique ids and nonempty edits")
        names.add(mutation["id"])
    return plan


def arguments(parser):
    parser.add_argument("scratch", type=Path)
    parser.add_argument("stem")
    parser.add_argument("plan", type=Path)
    parser.add_argument("--output", type=Path, required=True, help="new evidence directory")
    parser.add_argument("--function")
    parser.add_argument("--budget", type=int, default=8, help="maximum candidate builds, including failures")
    parser.add_argument("--timeout", type=float, default=60, help="seconds per build")


def prepare(args):
    from dataclasses import replace
    baseline = replace(Attempt.discover(args.scratch.resolve(), args.stem), function=args.function)
    plan = load_plan(args.plan)
    source = baseline.source.read_text()
    # Validate every independent edit before making a run directory or building.
    for mutation in plan["mutations"]:
        apply_edits(source, mutation["edits"])
    runner = Runner(baseline, args.output.resolve(), args.budget, args.timeout, plan.get("constraints", []))
    return runner, source, plan
