#!/usr/bin/env python3
"""Claim an overlay and report whether any of it can actually land here.

Run by the list driver before it spends a worker on an overlay. Setting one up
costs a 235MB asm copy and four submodule clones, and an overlay whose
remaining functions are all shared bodies produces nothing: the landing filter
refuses them ("not landing X here: body is not in src/..."), so the whole
worktree is built and torn down for no result.

A difficulty bound is the same problem again. Under --max-difficulty most
overlays hold nothing the sweep will pick: only 51 of the 121 in
actors_sweep_order.txt have work under 0.3, so the other 70 were each claimed,
given a worktree, split, and released seconds later having matched nothing.
Applying the bound here skips them before any of that is paid for.

Exit 0 and print the claim on success; exit 1 (having relinquished) when the
claim fails or nothing claimed is landable; exit 2 when this helper itself
could not decide, which the caller must treat as "carry on", never as "skip".
Fails open throughout: if the overlay's source directory cannot be resolved,
the claim stands and the worker proceeds.

It lives in tools/ rather than local/ because tools/vacuum_overlay_list.sh
depends on it. A committed script calling a gitignored helper fails on every
other checkout - and fails *silently*, because a missing helper looks exactly
like "nothing landable here" and the driver would skip every overlay.
"""
from __future__ import annotations
import json, pathlib, re, subprocess, sys, time

ROOT = pathlib.Path(__file__).resolve().parent.parent


def orch(*args: str) -> tuple[int, str]:
    p = subprocess.run([sys.executable, "tools/vacuum_orch.py", *args],
                       cwd=ROOT, capture_output=True, text=True)
    return p.returncode, p.stdout.strip()


def src_dir(overlay: str) -> pathlib.Path | None:
    if "/lib/" in overlay:
        d = ROOT / "src" / overlay.split("/", 1)[0] / "lib"
    else:
        hits = list((ROOT / "src").glob(f"*/{overlay}"))
        d = hits[0] if hits else None
    return d if d and d.is_dir() else None


def landable(overlay: str, funcs: list[str]) -> list[str]:
    d = src_dir(overlay)
    if d is None:
        return funcs                      # fail open
    text = "\n".join(p.read_text(errors="replace") for p in d.glob("*.c"))
    slots = set(re.findall(r"INCLUDE_ASM\([^)]*,\s*(\w+)\)", text))
    return [f for f in funcs if f in slots]


def solved_set() -> set:
    """Bodies already matched in another overlay - the pick skips these.

    tools/vacuum.sh excludes them from every pick (`--exclude-file "$solved"`),
    so an overlay whose remaining functions are all duplicates yields nothing.
    Without the same exclusion here that overlay passed the filter, got a full
    worktree and split, and the inner vacuum then reported "0 function(s) to
    attempt (of N claimed; the rest are duplicates already matched elsewhere)".

    The index rebuild costs ~8s, so it is cached briefly beside the orchestrator
    state - machine-local, and not under local/, which tools/ must not depend on.
    Fails open (empty set) so a cache or index problem never skips real work.
    """
    try:
        gitdir = pathlib.Path(subprocess.run(
            ["git", "rev-parse", "--git-common-dir"], cwd=ROOT,
            capture_output=True, text=True, check=True).stdout.strip())
        if not gitdir.is_absolute():
            gitdir = ROOT / gitdir
        cache = gitdir / "vacuum-solved-cache.txt"
        if cache.is_file() and time.time() - cache.stat().st_mtime < 300:
            return set(cache.read_text().split())
        out = subprocess.run(
            [sys.executable, "tools/overlay_dup_index.py", "solved", "--rebuild"],
            cwd=ROOT, capture_output=True, text=True, timeout=900)
        if out.returncode != 0:
            return set()
        tmp = cache.with_suffix(".tmp")
        tmp.write_text(out.stdout)
        tmp.replace(cache)
        return set(out.stdout.split())
    except Exception:
        return set()


def asm_dir(overlay: str) -> pathlib.Path | None:
    """Where this overlay's unmatched .s files live, for scoring."""
    base = ROOT / "asm" / "USA"
    if "/lib/" in overlay:
        fam, unit = overlay.split("/lib/", 1)
        d = base / fam / "nonmatchings" / "lib" / unit
    else:
        hits = list(base.glob(f"*/nonmatchings/{overlay}"))
        d = hits[0] if hits else None
    return d if d and d.is_dir() else None


def under_bound(overlay: str, funcs: list[str], bound: str) -> list[str]:
    """Those of `funcs` the scorer puts at or below `bound`.

    Fails open: any trouble resolving or running the scorer returns `funcs`
    unchanged, so a scoring problem costs a wasted worktree rather than
    silently skipping an overlay that had work in it.
    """
    d = asm_dir(overlay)
    if d is None:
        return funcs
    # Ask for scores rather than passing --max-score, and apply the bound here.
    # score_functions.py exits non-zero both when nothing meets the bound and
    # when it genuinely failed, so a --max-score run cannot tell "skip this
    # overlay" from "something went wrong" - and those need opposite handling.
    # Unfiltered, a non-zero exit means only the latter.
    p = subprocess.run([sys.executable, "tools/score_functions.py",
                        "--scores", str(d)],
                       cwd=ROOT, capture_output=True, text=True)
    if p.returncode != 0 or not p.stdout.strip():
        return funcs                      # could not score - fail open
    try:
        lim = float(bound)
    except ValueError:
        return funcs
    ok = {ln.split("\t")[1] for ln in p.stdout.splitlines()
          if "\t" in ln and float(ln.split("\t")[0]) <= lim}
    return [f for f in funcs if f in ok]


def main() -> int:
    overlay, session, pid = sys.argv[1], sys.argv[2], sys.argv[3]
    bound = sys.argv[4] if len(sys.argv) > 4 else ""
    rc, out = orch("claim-overlay", "--session", session, "--pid", pid,
                   "--cli", "agent", "--overlay", overlay)
    if rc != 0:
        print(f"{overlay}: claim refused", file=sys.stderr)
        return 1
    try:
        claim = json.loads(out)
    except Exception:
        # Not a decision about the overlay - this helper could not do its job.
        # Exit 2 so the caller falls back instead of silently skipping work.
        print(f"{overlay}: unparseable claim: {out[:120]}", file=sys.stderr)
        return 2
    funcs = claim.get("functions", [])
    keep = landable(overlay, funcs)
    why = "landable here (shared or promoted bodies)"
    if keep:
        solved = solved_set()
        if solved:
            keep = [f for f in keep if f not in solved]
            why = "left after duplicates already matched elsewhere"
    if keep and bound:
        keep = under_bound(overlay, keep, bound)
        why = f"under the {bound} difficulty bound"
    if not keep:
        orch("relinquish-overlay", "--session", session)
        print(f"{overlay}: {len(funcs)} claimed, 0 {why} "
              f"- skipped before setup", file=sys.stderr)
        return 1
    print(json.dumps({"overlay": overlay, "claimed": len(funcs),
                      "landable": len(keep), "functions": keep}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
