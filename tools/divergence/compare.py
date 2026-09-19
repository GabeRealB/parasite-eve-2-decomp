#!/usr/bin/env python3
"""Compare two attempts by expression identity, compiler decisions and output."""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from dataclasses import replace
import json
from pathlib import Path

from attribute import Compile
from evidence import digest
from localize import report
from scratch import Attempt
from report_paths import public_text
from rtl import REG


def correspond(before, after):
    """Unique expression/context correspondence; never equate numeric UIDs.

    Repeated indistinguishable expressions are deliberately left unresolved.
    This is syntactic correspondence, not a proof of semantic equivalence.
    """
    left = [n for n in before.values() if n.active]
    right = [n for n in after.values() if n.active]
    def indexed(nodes):
        result = defaultdict(list)
        for i, node in enumerate(nodes):
            result[node.signature].append(node)
        return result
    a, b = indexed(left), indexed(right)
    pairs = []
    for signature in a.keys() & b.keys():
        if len(a[signature]) == len(b[signature]) == 1:
            pairs.append((a[signature][0], b[signature][0], "unique_expression"))
    matched_a = {x.uid for x, _, _ in pairs}
    matched_b = {y.uid for _, y, _ in pairs}
    def context(nodes, used):
        result = defaultdict(list)
        for i, n in enumerate(nodes):
            if n.uid not in used:
                key = (nodes[i-1].signature if i else "START", n.signature,
                       nodes[i+1].signature if i+1 < len(nodes) else "END")
                result[key].append(n)
        return result
    ca, cb = context(left, matched_a), context(right, matched_b)
    for key in ca.keys() & cb.keys():
        if len(ca[key]) == len(cb[key]) == 1:
            pairs.append((ca[key][0], cb[key][0], "unique_context"))
    return sorted(pairs, key=lambda p: p[0].order)


def pseudo_correspondence(pairs):
    candidates, reverse = defaultdict(set), defaultdict(set)
    for a, b, _ in pairs:
        ar, br = REG.findall(a.expression), REG.findall(b.expression)
        if len(ar) != len(br):
            continue
        for (am, ap), (bm, bp) in zip(ar, br):
            ap, bp = int(ap), int(bp)
            if am == bm and ap >= 76 and bp >= 76:
                candidates[ap].add(bp)
                reverse[bp].add(ap)
    return {a: next(iter(bs)) for a, bs in candidates.items()
            if len(bs) == 1 and len(reverse[next(iter(bs))]) == 1}


def compare(before: Attempt, after: Attempt, before_trace=None, after_trace=None):
    if digest(before.target) != digest(after.target):
        raise ValueError("attempts have different targets; comparison requires identical target assembly")
    a, b = Compile.load(before), Compile.load(after)
    ra, rb = report(before, before_trace), report(after, after_trace)
    passes = []
    for name in a.snapshots.keys() & b.snapshots.keys():
        na = [n for n in a.snapshots[name].values() if n.active]
        nb = [n for n in b.snapshots[name].values() if n.active]
        sa, sb = [n.signature for n in na], [n.signature for n in nb]
        pairs = correspond(a.snapshots[name], b.snapshots[name])
        passes.append({"pass": name, "same_expression_sequence": sa == sb,
                       "same_expression_multiset": Counter(sa) == Counter(sb),
                       "before_instructions": len(na), "after_instructions": len(nb),
                       "corresponding": len(pairs),
                       "unresolved_before": len(na) - len(pairs), "unresolved_after": len(nb) - len(pairs)})
    passes.sort(key=lambda row: list(a.snapshots).index(row["pass"]))
    pairs = correspond(a.snapshots.get("lreg", {}), b.snapshots.get("lreg", {}))
    pseudos = pseudo_correspondence(pairs)
    changes = []
    import dumps
    oa, ob = dumps.allocation_order(a.texts.get("greg", "")), dumps.allocation_order(b.texts.get("greg", ""))
    ca, cb = dumps.conflicts(a.texts.get("greg", "")), dumps.conflicts(b.texts.get("greg", ""))
    for p, q in sorted(pseudos.items()):
        def state(c, number, order, conflicts):
            stats = c.pseudos.get(number)
            return {"refs": stats.refs if stats else None, "span": stats.span if stats else None,
                    "local_home": c.local_homes.get(number), "final_home": c.final_homes.get(number),
                    "rank": order.index(number) if number in order else None,
                    "initial_hard_conflicts": [r for r in conflicts[number] if r < 76] if number in conflicts else None}
        old, new = state(a, p, oa, ca), state(b, q, ob, cb)
        changed = {key: {"before": old[key], "after": new[key]} for key in old if old[key] != new[key]}
        if changed:
            changes.append({"before_pseudo": p, "after_pseudo": q, "changes": changed})
    def sites(data):
        return {e["target_index"] for e in data["divergences"] if e["target_index"] is not None}
    first = next((r["pass"] for r in passes if not r["same_expression_sequence"]), None)
    return {"schema": 1, "before": str(before), "after": str(after),
            "before_identity": ra["identity"], "after_identity": rb["identity"],
            "distance": {"before": ra["score"].get("distance"), "after": rb["score"].get("distance")},
            "assembly_identical": ra["identity"]["assembly_sha256"] == rb["identity"]["assembly_sha256"],
            "families": {"before": ra["families"], "after": rb["families"]},
            "resolved_target_sites": sorted(sites(ra) - sites(rb)),
            "new_target_sites": sorted(sites(rb) - sites(ra)),
            "first_changed_expression_sequence": first, "passes": passes,
            "pseudo_correspondence": pseudos, "allocation_changes": changes,
            "limitations": ["Correspondence is based on unique expression/context shapes; ambiguous values remain unresolved.",
                            "Anonymous expression comparison can hide changed cross-instruction dataflow; it is not semantic equivalence.",
                            "Dump boundaries can contain several compiler transformations."]}


def render(data):
    lines = [f"{data['before']} -> {data['after']}: distance {data['distance']['before']} -> {data['distance']['after']}",
             f"Assembly identical: {data['assembly_identical']}",
             f"First changed expression sequence: {data['first_changed_expression_sequence'] or 'none in available dumps'}",
             f"Resolved target sites: {data['resolved_target_sites']}; new: {data['new_target_sites']}"]
    for row in data["allocation_changes"]:
        lines.append(f"r{row['before_pseudo']} -> r{row['after_pseudo']}: " + "; ".join(f"{k} {v['before']} -> {v['after']}" for k, v in row["changes"].items()))
    lines.extend("Limitation: " + x for x in data["limitations"])
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scratch", type=Path)
    parser.add_argument("before")
    parser.add_argument("after")
    parser.add_argument("--function")
    parser.add_argument("--before-trace", type=Path)
    parser.add_argument("--after-trace", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    try:
        data = compare(replace(Attempt.discover(args.scratch, args.before), function=args.function),
                       replace(Attempt.discover(args.scratch, args.after), function=args.function), args.before_trace, args.after_trace)
    except (ValueError, OSError) as error:
        parser.exit(2, f"comparison unavailable: {public_text(str(error))}\n")
    print(json.dumps(data, indent=2) if args.json else render(data))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
