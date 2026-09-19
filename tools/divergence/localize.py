#!/usr/bin/env python3
"""Turn a near-miss into a named compilation decision.

This is the entry point for the other stages: it aligns an attempt against its
target, classifies what differs, traces each difference back to the pass that
decided it, and explains the ones whose decision procedure is modelled. The
output is meant to replace the first half-hour of an investigation, not to
conclude it - a divergence it cannot attribute is reported as unattributed
rather than guessed at.

    python3 tools/divergence/localize.py <scratch directory> [attempt stem]
    python3 tools/divergence/localize.py <scratch directory> --json
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from dataclasses import replace

sys.path.insert(0, str(Path(__file__).resolve().parent))
from align import align  # noqa: E402
from attribute import Compile, attribute  # noqa: E402
from common import parse_asm_file  # noqa: E402
from analyze import Analyzer  # noqa: E402
from evidence import identity, read_json, digest, artifact_provenance  # noqa: E402
from families import DUMPS_FOR, classify, register_correspondence, summarise  # noqa: E402
from scratch import Attempt  # noqa: E402
from report_paths import public_data, public_text


def report(attempt: Attempt, trace=None, gcc_source=None) -> dict:
    """Everything the stages establish about one attempt, as data."""
    compile_ = Compile.load(attempt)
    alignment = align(parse_asm_file(attempt.target), compile_.ours)
    findings = classify(alignment)
    attributions = attribute(compile_, findings)
    analyzer = Analyzer(compile_, trace, gcc_source)

    entries = []
    for attribution in attributions:
        finding = attribution.finding
        analysis = analyzer.one(attribution)
        entries.append(
            {
                "id": f"t{finding.pair.target.index if finding.pair.target else 'gap'}:o{finding.pair.ours.index if finding.pair.ours else 'gap'}",
                "family": finding.family,
                "position": finding.position,
                "target_index": finding.pair.target.index if finding.pair.target else None,
                "ours_index": finding.pair.ours.index if finding.pair.ours else None,
                "alignment": finding.pair.confidence,
                "target": finding.pair.target.text if finding.pair.target else None,
                "ours": finding.pair.ours.text if finding.pair.ours else None,
                "detail": finding.detail,
                "uid": attribution.uid,
                "pattern": attribution.pattern,
                "pseudo": attribution.pseudo,
                "decided_by": attribution.decided_by or None,
                "introduced_by": attribution.introduced_by or None,
                "note": attribution.note or None,
                "explanation": [o["text"] for o in analysis["observations"]],
                "analysis": analysis,
            }
        )

    score = {}
    if attempt.score.is_file():
        try:
            score = json.loads(attempt.score.read_text())
        except json.JSONDecodeError:
            score = {}

    structural = read_json(attempt.directory / f"{attempt.stem}.diagnosis.json")
    if structural and (not digest(attempt.directory / "target.o") or not digest(attempt.directory / f"{attempt.stem}.o")
                       or structural.get("target_sha256") != digest(attempt.directory / "target.o")
                       or structural.get("object_sha256") != digest(attempt.directory / f"{attempt.stem}.o")):
        structural = {"status": "rejected", "reason": "object diagnostics are stale or objects are missing"}
    def stack_shape(stream):
        import re
        adjustments = []
        stores = []
        for insn in stream:
            if insn.mnemonic == "addiu" and insn.operands[:2] == ("sp", "sp"):
                adjustments.append({"index": insn.index, "amount": insn.operands[2]})
            if insn.mnemonic in ("sw", "sd") and len(insn.operands) == 2 and "(sp)" in insn.operands[1]:
                stores.append({"index": insn.index, "register": insn.operands[0], "address": insn.operands[1]})
        return {"adjustments": adjustments, "stack_stores": stores}
    stack = {"target": stack_shape(parse_asm_file(attempt.target)), "ours": stack_shape(compile_.ours)}
    return public_data({
        "schema": 2,
        "attempt": str(attempt),
        "function": compile_.function,
        "gcc_source": analyzer.sources.status(),
        "identity": identity(attempt),
        "provenance": artifact_provenance(attempt),
        "trace": analyzer.trace_status,
        "available_dumps": list(compile_.texts),
        "structural": structural or {"status": "absent"},
        "stack": stack,
        "integration": analyzer.sources.describe("linkage"),
        "limitations": ["No target compiler state is available; hypotheses concern this compilation.",
                        "Source availability/hashes do not prove how the prebuilt compiler was built.",
                        "Normalised text agreement is not linked-binary verification."],
        "score": score,
        "aligned_positions": len(alignment.pairs),
        "families": dict(summarise(findings)),
        "register_correspondence": {
            theirs: dict(options) for theirs, options in register_correspondence(findings).items()
        },
        "divergences": entries,
    })


def render(data: dict, limit: int = 12) -> str:
    out: list[str] = []
    score = data["score"].get("score")
    head = f"{data['attempt']}: {len(data['divergences'])} divergences"
    if score is not None:
        head += f" at {score:.3f}%"
    head += f" over {data['aligned_positions']} aligned instructions"
    out.append(head)
    source = data.get("gcc_source", {})
    if source and source["status"] != "available":
        out.append(f"GCC source {source['status']}: {source['guidance']} Run: {source['setup']}")
    if data.get("provenance", {}).get("status") == "mismatch":
        out.append("artifact provenance mismatch: " + "; ".join(data["provenance"]["reasons"]))
    out.append("dumps: " + (", ".join(data.get("available_dumps", [])) or "none"))
    if data.get("trace", {}).get("status") == "rejected":
        out.append("trace rejected: " + "; ".join(data["trace"]["reasons"]))
    structural = data.get("structural", {})
    if "topology" in structural:
        out.append(f"object topology: {structural['topology']}; predicates agree: {structural.get('predicates_match')}; calls agree: {structural.get('calls_match')}")
    if "stack" in data.get("families", {}):
        out.append("stack observations: " + json.dumps(data["stack"]))

    if not data["divergences"]:
        out.append("Normalised instruction text agrees. Verify object relocations and the linked build before accepting a match.")
        if data["score"].get("distance", 0):
            out.append("The independent score still differs: inspect normalization, relocations and data placement.")
        return "\n".join(out)

    families = ", ".join(f"{name} {n}" for name, n in sorted(
        data["families"].items(), key=lambda kv: -kv[1]
    ))
    out.append(f"families: {families}")

    correspondence = data["register_correspondence"]
    if correspondence:
        out.append("register correspondence (target -> this compile):")
        for theirs, options in sorted(correspondence.items()):
            rendered = ", ".join(f"{name} x{n}" for name, n in sorted(options.items(), key=lambda kv: -kv[1]))
            out.append(f"  {theirs} -> {rendered}")

    out.append("")
    seen_rules = set()
    for entry in data["divergences"][:limit or None]:
        left = entry["target"] or ""
        right = entry["ours"] or ""
        out.append(f"[{entry['id']} {entry['family']}; {entry['alignment']}] {left:<40} | {right}")
        if entry["detail"]:
            out.append(f"    {entry['detail']}")
        trace = []
        if entry["uid"] is not None:
            trace.append(f"insn {entry['uid']} ({entry['pattern']})")
        if entry["pseudo"] is not None:
            trace.append(f"pseudo {entry['pseudo']}")
        if entry["decided_by"]:
            trace.append(f"observed by .{entry['decided_by']}")
        if trace:
            out.append("    " + ", ".join(trace))
        if entry["note"]:
            out.append(f"    {entry['note']}")
        analysis = entry.get("analysis", {})
        for line in entry["explanation"]:
            out.append(f"    observed: {line}")
        for rule in analysis.get("hypotheses", []):
            out.append(f"    mechanism to check: {rule['id']} — {rule['title']}")
            if rule["id"] not in seen_rules:
                seen_rules.add(rule["id"])
                out.extend("      " + step for step in rule["algorithm"])
                for ref in rule["sources"]:
                    if ref["available"]:
                        out.append(f"      source: {ref['path']}:{ref['line']} ({ref['symbol']}; located)")
                    else:
                        out.append(f"      source unavailable: {ref['file']} ({ref['symbol']}; {ref['reason']})")
        for proposal in analysis.get("proposals", [])[:2]:
            out.append(f"    propose [{proposal['id']}]: {proposal['change']}")
            out.append(f"      requires: {proposal['precondition']}")
            out.append(f"      prediction: {proposal['prediction']}")
        for unknown in analysis.get("unknowns", []):
            out.append("    unresolved: " + unknown)
        out.append("")

    unattributed = [e for e in data["divergences"] if e["decided_by"] is None]
    if unattributed:
        out.append(
            f"{len(unattributed)} of {len(data['divergences'])} divergences are not attributed to "
            "a pass; their family is routing only"
        )
    dumps_to_read = sorted({d for family in data["families"] for d in DUMPS_FOR.get(family, ())})
    if dumps_to_read:
        out.append("dumps relevant to these families: " + " ".join(dumps_to_read))
    if limit and len(data["divergences"]) > limit:
        out.append(f"Showing {limit}/{len(data['divergences'])} sites; use --limit 0 or --json for all evidence and proposals.")
    return "\n".join(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("scratch")
    parser.add_argument("stem", nargs="?")
    parser.add_argument("--json", action="store_true", help="emit the report as data")
    parser.add_argument("--trace", type=Path, help="verified trace_gcc.py output directory")
    parser.add_argument("--gcc-source", type=Path, help="GCC 2.8.1 source root for algorithm citations")
    parser.add_argument("--function", help="function name when the input contains helpers")
    parser.add_argument("--limit", type=int, default=12, help="text sites to show; 0 shows all")
    args = parser.parse_args()

    try:
        attempt = replace(Attempt.discover(args.scratch, args.stem), function=args.function)
        data = report(attempt, args.trace, args.gcc_source)
    except (OSError, ValueError) as error:
        parser.exit(2, f"diagnosis unavailable: {public_text(str(error))}\n")
    print(json.dumps(data, indent=2) if args.json else render(data, args.limit))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
