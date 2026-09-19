#!/usr/bin/env python3
"""Find what has been done before about a divergence of this shape.

The step from "this is a global-allocation divergence on one pseudo" to "change
the source like *this*" is not derivable from the dumps, and pretending
otherwise would produce confident guesses. What is available instead is the
project's own record: sessions that faced the same shape of divergence, and the
accumulated prose about the compiler's behaviour. This ranks both and shows
them; it never decides.

    python3 tools/divergence/retrieve.py <scratch directory>
    python3 tools/divergence/retrieve.py --family register --pass greg
"""

from __future__ import annotations

import argparse
import importlib.util
import math
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from capture import DEFAULT_STORE, load_all  # noqa: E402
from localize import report  # noqa: E402
from scratch import Attempt  # noqa: E402

REPOSITORY = Path(__file__).resolve().parent.parent.parent

# The permuter's penalty categories, in the vocabulary the families use, so a
# record that predates the classifier can still be compared with one that does
# not.
PENALTY_FAMILIES = {
    "regs": "register",
    "reorder": "scheduling",
    "insert": "insertion",
    "delete": "deletion",
    "stack": "stack",
    "branch": "branch",
}

# Terms that pull the relevant prose out of the accumulated notes. The compiler
# pass a divergence was decided in is a much better query than the family alone,
# because the notes are written about passes.
TERMS = {
    "register": ["register", "allocation", "allocator", "home"],
    "scheduling": ["schedule", "scheduling", "reorder", "ordering"],
    "delay_slot": ["delay", "slot", "branch"],
    "opcode": ["combine", "pattern", "width"],
    "immediate": ["constant", "immediate", "cse"],
    "displacement": ["offset", "field", "struct"],
    "symbol": ["symbol", "rodata", "relocation"],
    "stack": ["stack", "frame", "spill"],
    "insertion": ["extra", "spill", "reload"],
    "deletion": ["missing", "merged", "cross-jump"],
    "mixed": [],
    "hazard": ["hazard", "latency", "nop"],
    "branch": ["predicate", "jump", "branch"],
    "call": ["argument", "prototype", "call"],
}


def _learn():
    """Load the section-ranking search over the accumulated notes.

    That search already exists and is tuned for this corpus; duplicating it here
    would mean two rankings of the same text that drift apart.
    """
    path = REPOSITORY / "tools" / "learn.py"
    if not path.is_file():
        return None
    spec = importlib.util.spec_from_file_location("learn", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def profile_of(entry: dict) -> dict[str, float]:
    """A record's divergence shape, as family -> share of the difference."""
    divergence = (entry.get("near_miss") or {}).get("divergence")
    if divergence and divergence.get("families"):
        counts = {k: float(v) for k, v in divergence["families"].items()}
    else:
        counts = {}
        for scored in entry.get("scores", []):
            for penalty, value in (scored.get("penalties") or {}).items():
                family = PENALTY_FAMILIES.get(penalty)
                if family and value:
                    counts[family] = counts.get(family, 0.0) + float(value)
            if counts:
                break
    total = sum(counts.values())
    return {k: v / total for k, v in counts.items()} if total else {}


def similarity(left: dict[str, float], right: dict[str, float]) -> float:
    if not left or not right:
        return 0.0
    keys = set(left) | set(right)
    dot = sum(left.get(k, 0.0) * right.get(k, 0.0) for k in keys)
    norm = math.sqrt(sum(v * v for v in left.values())) * math.sqrt(
        sum(v * v for v in right.values())
    )
    return dot / norm if norm else 0.0


def rank_records(target: dict[str, float], store: Path, limit: int) -> list[tuple[float, dict]]:
    scored = []
    for entry in load_all(store):
        value = similarity(target, profile_of(entry))
        if value > 0:
            scored.append((value, entry))
    # A record that carries a fix is worth more than one that only carries a
    # near-miss, so equal shapes are ordered by whether anything was resolved.
    scored.sort(key=lambda pair: (pair[0], bool(pair[1].get("matched") and pair[1].get("fix"))), reverse=True)
    return scored[:limit]


def rank_notes(families: list[str], passes: list[str], limit: int) -> list[tuple[str, str]]:
    learn = _learn()
    if learn is None:
        return []
    terms = [t for family in families for t in TERMS.get(family, [])] + passes
    if not terms:
        return []
    text = learn.DOC.read_text(errors="replace") if learn.DOC.is_file() else ""
    ranked = sorted(
        ((learn.score(title, body, terms), title, body) for title, body in learn.sections(text)),
        key=lambda row: -row[0],
    )
    return [(title, body) for value, title, body in ranked[:limit] if value > 0]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("scratch", nargs="?", help="a scratch directory to take the shape from")
    parser.add_argument("--family", action="append", default=[], help="query a family directly")
    parser.add_argument("--pass", dest="passes", action="append", default=[])
    parser.add_argument("--store", type=Path, default=DEFAULT_STORE)
    parser.add_argument("--limit", type=int, default=5)
    parser.add_argument("--notes", type=int, default=3, help="how many notes to list")
    parser.add_argument("--show-fixes", action="store_true", help="print retained source diffs and their pairing basis")
    args = parser.parse_args()

    families = list(args.family)
    passes = list(args.passes)
    target: dict[str, float] = {}
    function = None

    if args.scratch:
        data = report(Attempt.discover(args.scratch))
        function = data["attempt"]
        total = sum(data["families"].values()) or 1
        target = {k: v / total for k, v in data["families"].items()}
        families += list(data["families"])
        passes += [e["decided_by"] for e in data["divergences"] if e["decided_by"]]
    elif families:
        target = {family: 1.0 / len(families) for family in families}
    else:
        parser.error("give a scratch directory or at least one --family")

    families = sorted(set(families))
    passes = sorted(set(passes))
    header = f"shape: {', '.join(families)}"
    if passes:
        header += f" observed by {', '.join('.' + p for p in passes)}"
    print(f"{function + ': ' if function else ''}{header}\n")

    ranked = rank_records(target, args.store, args.limit)
    if ranked:
        print("past sessions with a similar shape:")
        for value, entry in ranked:
            best = max((s.get("score") or 0 for s in entry.get("scores", [])), default=0)
            state = "matched" if entry.get("matched") else f"best {best:.3f}%"
            marker = " [has source comparison]" if entry.get("fix") else ""
            print(f"  {value:.2f}  {entry['function']:<44} {state}{marker}")
            if args.show_fixes and entry.get("fix"):
                print("    pairing: " + entry.get("pairing", "legacy record; causal pairing unverified"))
                print("\n".join(entry["fix"]))
        print()
    else:
        print("no stored session has a comparable shape yet\n")

    notes = rank_notes(families, passes, args.notes)
    if notes:
        print("notes on this kind of divergence:")
        for title, _ in notes:
            print(f"  {title.strip()}")
        print("\nread one in full with: python3 tools/learn.py --show 1 <terms>")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
