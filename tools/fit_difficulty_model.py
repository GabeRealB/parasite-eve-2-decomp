#!/usr/bin/env python3
"""Refit the `score_functions.py` difficulty model on this repo's own history.

Every match lands as a `matched <function> <attempts>` commit and every give-up
as a line in `tools/difficult_functions`, so the project has been recording a
labelled difficulty corpus since the first match. This tool reads it back,
joins each observation to the function's assembly metrics, fits the logistic
model and (with ``--write``) drops the coefficients into `score_functions.py`.

Target: **P(the function does not match on the first attempt)**. A give-up
counts as a positive whatever its attempt count - stopping at 63% after 39
tries is not a one-shot match.

Two joins are needed because matched functions get renamed by later naming
passes while `asm/` is regenerated under the current names: by name first, then
by the vram embedded in a `func_<addr>` / `func_<overlay>_<addr>` name.

    python3 tools/fit_difficulty_model.py            # report only
    python3 tools/fit_difficulty_model.py --write    # update score_functions.py
"""

from __future__ import annotations

import argparse
import collections
import re
import subprocess
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import score_functions as sf  # noqa: E402

ASM_ROOT = Path("asm/USA")
DIFFICULT = Path("tools/difficult_functions")
SCORER = Path("tools/score_functions.py")
MATCHED = re.compile(r"^matched (\S+) (\d+)$")
VRAM = re.compile(r"/\* [0-9A-F]+ ([0-9A-F]{8}) [0-9A-F]{8} \*/")
HEX8 = re.compile(r"([0-9A-F]{8})")

# The features the scorer computes, in the order the coefficients are stored.
FEATURES = ("instructions", "branches", "jumps", "labels")


def scan_asm() -> tuple[dict, dict]:
    """Metrics for every function in asm/, keyed by name and by (family, vram)."""
    by_name: dict[str, dict] = {}
    by_vram: dict[tuple[str, str], list[dict]] = collections.defaultdict(list)
    for path in ASM_ROOT.rglob("*.s"):
        parts = path.parts
        if "matchings" not in parts and "nonmatchings" not in parts:
            continue
        score = sf.analyze_function(str(path))
        if score is None or score.instruction_count == 0:
            continue
        text = path.read_text(errors="replace")
        vram = VRAM.search(text)
        rec = {
            "name": path.stem,
            "family": parts[2],
            "instructions": score.instruction_count,
            "branches": score.branch_count,
            "jumps": score.jump_count,
            "labels": score.label_count,
        }
        by_name.setdefault(path.stem, rec)
        if vram:
            by_vram[(parts[2], vram.group(1))].append(rec)
    return by_name, by_vram


def history() -> list[dict]:
    """`matched <fn> <attempts>` commits, with the src/ family they touched."""
    out = subprocess.run(
        ["git", "log", "--numstat", "--pretty=format:C\t%s"],
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    recs: list[dict] = []
    cur = None
    for line in out.splitlines():
        if line.startswith("C\t"):
            cur = None
            m = MATCHED.match(line[2:])
            if m:
                cur = {"name": m.group(1), "attempts": int(m.group(2)), "family": None}
                recs.append(cur)
        elif cur is not None and "\t" in line:
            _, _, path = line.split("\t", 2)
            if path.startswith("src/") and cur["family"] is None:
                cur["family"] = path.split("/")[1]
    return recs


def giveups() -> list[dict]:
    recs = []
    if DIFFICULT.is_file():
        for line in DIFFICULT.read_text().split("\n"):
            parts = line.split()
            if parts:
                recs.append(
                    {
                        "name": parts[0],
                        "attempts": int(parts[1]) if len(parts) > 1 else 0,
                        "family": None,
                        "gaveup": True,
                    }
                )
    return recs


def join(recs, by_name, by_vram):
    rows, missed = [], collections.Counter()
    for rec in recs:
        hit = by_name.get(rec["name"])
        if hit is None:
            families = [rec["family"]] if rec["family"] else []
            families += ["main", "gameplay", "actors", "rooms", "title", "weapons"]
            for addr in HEX8.findall(rec["name"].upper()):
                for family in families:
                    if (family, addr) in by_vram:
                        hit = by_vram[(family, addr)][0]
                        break
                if hit:
                    break
        if hit is None:
            missed[rec["family"] or "?"] += 1
            continue
        row = dict(hit)
        row["attempts"] = rec["attempts"]
        row["hard"] = 1 if (rec.get("gaveup") or rec["attempts"] > 1) else 0
        rows.append(row)
    return rows, missed


def fit(X: np.ndarray, y: np.ndarray, l2: float = 1e-3):
    """L2-penalised logistic regression by Newton-Raphson. X is standardised."""
    n, k = X.shape
    design = np.column_stack([np.ones(n), X])
    beta = np.zeros(k + 1)
    penalty = np.eye(k + 1) * l2
    penalty[0, 0] = 0.0
    for _ in range(100):
        p = 1.0 / (1.0 + np.exp(-design @ beta))
        w = np.clip(p * (1 - p), 1e-9, None)
        grad = design.T @ (y - p) - penalty @ beta
        hess = design.T @ (design * w[:, None]) + penalty
        step = np.linalg.solve(hess, grad)
        beta += step
        if np.abs(step).max() < 1e-10:
            break
    return beta[0], beta[1:]


def auc(scores: np.ndarray, y: np.ndarray) -> float:
    order = np.argsort(scores)
    ranks = np.empty(len(scores), float)
    ranks[order] = np.arange(1, len(scores) + 1)
    for value in np.unique(scores):
        tie = scores == value
        if tie.sum() > 1:
            ranks[tie] = ranks[tie].mean()
    pos, neg = y.sum(), (1 - y).sum()
    if pos == 0 or neg == 0:
        return float("nan")
    return float((ranks[y == 1].sum() - pos * (pos + 1) / 2) / (pos * neg))


def cv_auc(X, y, folds=5, seed=0):
    rng = np.random.default_rng(seed)
    idx = rng.permutation(len(y))
    out = []
    for f in range(folds):
        test = idx[f::folds]
        train = np.setdiff1d(idx, test)
        mu, sd = X[train].mean(0), X[train].std(0)
        sd[sd == 0] = 1.0
        b0, b = fit((X[train] - mu) / sd, y[train])
        out.append(auc((X[test] - mu) / sd @ b + b0, y[test]))
    return float(np.mean(out)), float(np.std(out))


def transform(rows, log: bool) -> np.ndarray:
    X = np.array([[r[f] for f in FEATURES] for r in rows], float)
    return np.log1p(X) if log else X


def predict(mu, sd, coef, intercept, log, counts) -> float:
    x = np.array(counts, float)
    if log:
        x = np.log1p(x)
    return float(1 / (1 + np.exp(-((x - mu) / sd @ coef + intercept))))


def emit(mu, sd, coef, intercept, log, rows, auc) -> str:
    def arr(values):
        return "\n".join(f"        np.float64({float(v)!r})," for v in values)

    return f'''# --- fitted model (tools/fit_difficulty_model.py) ---
# Logistic regression on {len(rows)} observations mined from this repo:
# every `matched <fn> <attempts>` commit plus every give-up in
# tools/difficult_functions, joined to that function's assembly metrics.
# Target: P(the function does not match on the first attempt); a give-up is a
# positive whatever its attempt count. 5-fold CV AUC {auc:.3f}. Adding stack
# size, jump-table, float/GTE, callee or memory-op counts moves that by less
# than the fold noise: the difficulty a static count cannot see is the
# difficulty that is left.
#
# Counts are log1p'd. Calls genuinely predict *easier* (glue code), so `jumps`
# fits negative, and on raw counts that extrapolates to nonsense - a
# 214-instruction, 46-call room function scored below a 2-instruction leaf
# stub. The log holds the effect down: raw counts leave 58 functions of >=100
# instructions ranked easier than that stub, log1p leaves none, for ~0.01 AUC.
LOG_FEATURES = {log}
MEANS = np.array(
    [
{arr(mu)}
    ]
)
STDS = np.array(
    [
{arr(sd)}
    ]
)
COEFFICIENTS = np.array(
    [
{arr(coef)}
    ]
)
INTERCEPT = {float(intercept)!r}
# --- end fitted model ---'''


def write_constants(block: str) -> None:
    text = SCORER.read_text()
    start = text.index("# --- fitted model (tools/fit_difficulty_model.py) ---")
    end = text.index("# --- end fitted model ---") + len("# --- end fitted model ---")
    SCORER.write_text(text[:start] + block + text[end:])
    print(f"wrote {SCORER}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--write", action="store_true", help="update score_functions.py")
    ap.add_argument(
        "--raw",
        action="store_true",
        help="fit on raw counts: ~0.01 better AUC, but see the sanity check below",
    )
    args = ap.parse_args()

    by_name, by_vram = scan_asm()
    recs = history() + giveups()
    rows, missed = join(recs, by_name, by_vram)
    print(f"{len(rows)} of {len(recs)} observations joined to asm/ (unjoined: {dict(missed)})")
    by_family = collections.Counter(r["family"] for r in rows)
    print("  by family:", dict(by_family.most_common()))
    y = np.array([r["hard"] for r in rows], float)
    print(f"  positives (not one-shot): {int(y.sum())} of {len(y)}")

    for log in (False, True):
        X = transform(rows, log)
        m, s = cv_auc(X, y)
        print(f"  5-fold CV AUC, {'log1p' if log else 'raw   '} features: {m:.3f} +/- {s:.3f}")
    old = np.array([sf.decompilation_difficulty_score(*[r[f] for f in FEATURES]) for r in rows])
    print(f"  5-fold CV AUC, model in tree            : {auc(old, y):.3f} (in-sample, no CV)")

    log = not args.raw
    X = transform(rows, log)
    mu, sd = X.mean(0), X.std(0)
    sd[sd == 0] = 1.0
    intercept, coef = fit((X - mu) / sd, y)
    print("\ncoefficients (standardised):")
    for name, c in zip(FEATURES, coef):
        print(f"  {name:13s} {c:+.3f}")
    print(f"  {'intercept':13s} {intercept:+.3f}")

    stub = predict(mu, sd, coef, intercept, log, [2, 0, 0, 0])
    silly = sum(
        1
        for r in by_name.values()
        if r["instructions"] >= 100
        and predict(mu, sd, coef, intercept, log, [r[f] for f in FEATURES]) < stub
    )
    print(f"\nsanity: a 2-instruction leaf stub scores {stub:.3f}; {silly} function(s)")
    print("        of >=100 instructions score below it (want 0)")

    block = emit(mu, sd, coef, intercept, log, rows, cv_auc(X, y)[0])
    if args.write:
        write_constants(block)
    else:
        print("\n" + block)
    return 0


if __name__ == "__main__":
    sys.exit(main())
