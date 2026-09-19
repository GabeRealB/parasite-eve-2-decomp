"""Evidence identity and trace validation shared by diagnosis and experiments."""
from __future__ import annotations

import hashlib
import json
import shlex
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
COMPILER = ROOT / "tools/linux/gcc-2.8.1-psx/cc1"
SUPPORTED = "60d886cd75bbd7855fc7909224a15401de76bff21af8a629c2060290a073f5fd"


def digest(path):
    path = Path(path)
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None


def read_json(path, default=None):
    path = Path(path)
    if not path.is_file():
        return {} if default is None else default
    return json.loads(path.read_text())


def journal_rows(path):
    """A truncated journal line cannot establish provenance; retain other rows."""
    if not Path(path).is_file():
        return []
    rows = []
    for line in Path(path).read_text().splitlines():
        try:
            row = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(row, dict):
            rows.append(row)
    return rows


def identity(attempt):
    return {"source_sha256": digest(attempt.source), "input_sha256": digest(attempt.preprocessed),
            "target_sha256": digest(attempt.target), "assembly_sha256": digest(attempt.ours),
            "compiler_sha256": digest(COMPILER)}


def artifact_provenance(attempt):
    """Check fields actually recorded by this harness, without inventing hashes."""
    rows = journal_rows(attempt.directory / "attempts.jsonl")
    built = next((r for r in reversed(rows) if r.get("source") == attempt.source.name), None)
    if not built:
        return {"status": "unverified", "reason": "No build journal entry for this source."}
    reasons = []
    if built.get("failure"):
        reasons.append("latest recorded build failed; older outputs may remain")
    for key, path in (("source_sha256", attempt.source), ("preprocessed_sha256", attempt.preprocessed),
                      ("target_sha256", attempt.directory / "target.o"), ("compiler_sha256", COMPILER)):
        if built.get(key) and built[key] != digest(path):
            reasons.append(f"{key} disagrees with latest recorded build")
    score = read_json(attempt.score)
    if "distance" in built and score.get("distance") != built["distance"]:
        reasons.append("score disagrees with latest recorded build")
    return {"status": "mismatch" if reasons else "journal_fields_agree", "reasons": reasons,
            "limitation": "The journal does not hash every RTL dump or annotated assembly file."}


def load_trace(directory, attempt, function=None):
    if directory is None:
        return [], {"status": "absent"}
    directory = Path(directory)
    manifest = read_json(directory / "manifest.json")
    reasons = []
    if not manifest.get("trace_complete") or not manifest.get("assembly_identical"):
        reasons.append("trace is incomplete or changed ordinary compiler output")
    if not attempt.preprocessed.is_file() or manifest.get("input_sha256") != digest(attempt.preprocessed):
        reasons.append("trace input does not match this attempt's preprocessed input")
    if manifest.get("compiler_sha256") != SUPPORTED or digest(COMPILER) != SUPPORTED:
        reasons.append("trace compiler is not the supported bundled binary")
    if not (directory / "events.jsonl").is_file():
        reasons.append("events.jsonl is missing")
    baseline = directory / "baseline.s"
    if not baseline.is_file():
        reasons.append("trace baseline.s is missing")
    else:
        from dumps import emitted
        expected = [(e.uid, e.insn.text, e.pattern) for e in emitted(attempt.cc1_assembly.read_text())] if attempt.cc1_assembly.is_file() else []
        actual = [(e.uid, e.insn.text, e.pattern) for e in emitted(baseline.read_text())]
        if not expected or expected != actual:
            reasons.append("trace baseline instruction/UID stream differs from this attempt; rebuild coherent artifacts before joining by UID")
        if manifest.get("baseline_assembly_sha256") != digest(baseline):
            reasons.append("trace baseline hash disagrees with its manifest")
    flags_status = "unverified"
    journal = attempt.directory / "attempts.jsonl"
    if journal.is_file():
        builds = journal_rows(journal)
        last = next((r for r in reversed(builds) if not r.get("failure") and r.get("source") == attempt.source.name and r.get("source_sha256") == digest(attempt.source)), None)
        if last and last.get("flags"):
            def options(flags):
                if isinstance(flags, str) and "|" in flags:
                    parts = [p for p in flags.split("|") if "-mcpu=" in p]
                    if len(parts) != 1:
                        return None
                    flags = parts[0]
                values = shlex.split(flags) if isinstance(flags, str) else list(flags or [])
                # This scratch build appends -G0 outside its recorded CC_FLAGS.
                return [v for v in values if v not in ("-da", "-dp", "-quiet", "-w", "-G0")]
            if options(last["flags"]) != options(manifest.get("flags")):
                reasons.append("trace compiler flags disagree with the recorded attempt")
            else:
                flags_status = "verified"
    if reasons:
        return [], {"status": "rejected", "reasons": reasons, "path": str(directory)}
    rows = [json.loads(line) for line in (directory / "events.jsonl").read_text().splitlines() if line.strip()]
    if any(r.get("event") == "error" for r in rows):
        return [], {"status": "rejected", "reasons": ["trace contains an observer error"]}
    functions = {r.get("name") for r in rows if r.get("event") == "function"}
    if function and function not in functions:
        return [], {"status": "rejected", "reasons": ["requested function is absent from trace"]}
    if not function and len(functions) > 1:
        return [], {"status": "rejected", "reasons": ["multiple functions in trace; select --function"]}
    active = function is None
    selected = []
    for number, row in enumerate(rows, 1):
        if row.get("event") == "function":
            active = function is None or row.get("name") == function
        if active:
            selected.append({**row, "event_index": number})
    return selected, {"status": "verified_input", "path": str(directory),
                      "flags": manifest.get("flags"),
                      "flags_status": flags_status,
                      "limitation": "Input/compiler and annotated UID stream verified. Flags without an original journal remain unverified."}
