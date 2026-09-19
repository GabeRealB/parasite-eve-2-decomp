"""Executable objectives anchored to the fixed target, not unstable pseudo IDs."""
from __future__ import annotations

import re

from align import align
from attribute import Compile
from common import parse_asm_file
import dumps


KINDS = {"distance_le", "family_count_le", "instruction", "rtl", "allocation_before"}


def validate(specs):
    if not isinstance(specs, list):
        raise ValueError("constraints must be a list")
    required = {"distance_le": ("value",), "family_count_le": ("family", "value"),
                "instruction": ("target_index", "pattern"), "rtl": ("target_index", "pass", "pattern"),
                "allocation_before": ("before_target", "after_target")}
    for spec in specs:
        if not isinstance(spec, dict) or spec.get("kind") not in KINDS:
            raise ValueError(f"unknown constraint: {spec!r}")
        missing = [k for k in required[spec["kind"]] if k not in spec]
        if missing:
            raise ValueError(f"constraint missing {missing}")
        for key in ("value", "target_index", "before_target", "after_target"):
            if key in spec and (type(spec[key]) is not int or spec[key] < 0):
                raise ValueError(f"constraint {key} must be a nonnegative integer")
        if "pattern" in spec:
            try:
                re.compile(spec["pattern"])
            except (re.error, TypeError) as error:
                raise ValueError(f"invalid constraint pattern: {error}") from error
        if spec.get("pass", "rtl") not in dumps.PASSES:
            raise ValueError(f"unknown dump pass: {spec['pass']}")
    return specs


def evaluate(attempt, data, specs):
    validate(specs)
    compile_ = Compile.load(attempt)
    pairs = {p.target.index: p for p in align(parse_asm_file(attempt.target), compile_.ours) if p.target}
    def site(index):
        pair = pairs.get(index)
        return pair.ours if pair and pair.ours and pair.confidence == "shape" else None
    def node(index, pass_name):
        insn = site(index)
        emitted = compile_.at(insn.index) if insn else None
        return compile_.snapshots.get(pass_name, {}).get(emitted.uid) if emitted else None
    def definition(index):
        n = node(index, "lreg")
        m = re.match(r"\(set\s+\(reg[/\w]*:\w+\s+(\d+)", n.expression) if n else None
        return int(m[1]) if m and int(m[1]) >= 76 else None
    rows = []
    for spec in specs:
        kind = spec["kind"]
        value = met = None
        if kind == "distance_le":
            value = data["score"].get("distance")
            met = value <= spec["value"] if value is not None else None
        elif kind == "family_count_le":
            value = data["families"].get(spec["family"], 0)
            met = value <= spec["value"]
        elif kind == "instruction":
            insn = site(spec["target_index"])
            value = insn.text if insn else None
            met = bool(re.search(spec["pattern"], value)) if value is not None else None
        elif kind == "rtl":
            n = node(spec["target_index"], spec["pass"])
            value = n.expression if n else None
            met = bool(re.search(spec["pattern"], value)) if value is not None else None
        elif kind == "allocation_before":
            a, b = definition(spec["before_target"]), definition(spec["after_target"])
            order = dumps.allocation_order(compile_.texts.get("greg", ""))
            if a in order and b in order and a != b:
                value = {"before_pseudo": a, "after_pseudo": b, "ranks": [order.index(a), order.index(b)]}
                met = order.index(a) < order.index(b)
        rows.append({"constraint": spec, "status": "unknown" if met is None else "met" if met else "unmet", "observed": value})
    return rows
