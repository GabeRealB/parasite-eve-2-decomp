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
import fcntl, json, os, pathlib, re, subprocess, sys, time

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

    tools/vacuum.sh excludes them from every pick, so an overlay holding only
    those yields nothing: without the same exclusion here it got a full worktree
    and split before the inner vacuum reported "0 function(s) to attempt".
    """
    def produce():
        r = subprocess.run(
            [sys.executable, "tools/overlay_dup_index.py", "solved", "--rebuild"],
            cwd=ROOT, capture_output=True, text=True, timeout=900)
        return r.stdout if r.returncode == 0 else None
    text = _locked_cache("vacuum-solved-cache.txt", 600.0, produce)
    return set(text.split()) if text else set()


def difficult_names() -> set:
    """Functions tools/difficult_functions has parked.

    score_functions.py drops these, so an overlay whose every function is parked
    simply has no rows in the score table - which reads identically to "the
    table does not cover this overlay" and so fell through the fail-open. Read
    the list directly; absence is not evidence.
    """
    try:
        return {l.split()[0]
                for l in (ROOT / "tools/difficult_functions").read_text().splitlines()
                if l.strip()}
    except OSError:
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


def _locked_cache(name: str, ttl: float, produce) -> str | None:
    """Read a cached file, or let exactly one process regenerate it.

    Without the lock every caller that arrives while the value is being computed
    starts its own copy. claim_filter runs on every claim, the duplicate index
    takes ~9s and the scorer ~3s, and 136 concurrent copies of the two starved a
    16-core box to a standstill (cpu pressure 92%, no landings for over an hour).
    """
    try:
        gitdir = pathlib.Path(subprocess.run(
            ["git", "rev-parse", "--git-common-dir"], cwd=ROOT,
            capture_output=True, text=True, check=True).stdout.strip())
        if not gitdir.is_absolute():
            gitdir = ROOT / gitdir
    except Exception:
        return None
    cache = gitdir / name

    def fresh():
        try:
            if time.time() - cache.stat().st_mtime < ttl:
                return cache.read_text()
        except OSError:
            pass
        return None

    hit = fresh()
    if hit is not None:
        return hit
    with open(gitdir / (name + ".lock"), "w") as lf:
        try:
            fcntl.flock(lf, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            fcntl.flock(lf, fcntl.LOCK_EX)      # wait for the winner
            fcntl.flock(lf, fcntl.LOCK_UN)
            return fresh()
        hit = fresh()
        if hit is not None:
            return hit
        text = produce()
        if text is None:
            return None
        tmp = cache.with_suffix(".tmp")
        tmp.write_text(text)
        tmp.replace(cache)
        return text


def scores() -> tuple[dict, set]:
    """(function -> difficulty, overlays present), from one cached run.

    This used to be a score_functions.py invocation per claimed overlay. That is
    a fresh interpreter, a numpy import and a directory walk each time, and with
    workers claiming continuously they piled up faster than they finished.
    """
    def produce():
        dirs = subprocess.run(
            [sys.executable, "tools/decomp_overlay.py", "list-nonmatchings"],
            cwd=ROOT, capture_output=True, text=True).stdout.split()
        if not dirs:
            return None
        r = subprocess.run([sys.executable, "tools/score_functions.py", "--scores", *dirs],
                           cwd=ROOT, capture_output=True, text=True, timeout=900)
        return r.stdout if r.returncode == 0 and r.stdout.strip() else None

    text = _locked_cache("vacuum-scores-cache.tsv", 600.0, produce)
    if not text:
        return {}, set()
    out, seen = {}, set()
    for line in text.splitlines():
        parts = line.split("\t")
        if len(parts) == 3:
            try:
                out[parts[1]] = float(parts[0])
                seen.add(parts[2])
            except ValueError:
                pass
    return out, seen


def under_bound(overlay: str, funcs: list[str], bound: str) -> list[str]:
    """Those of `funcs` at or below `bound`. Fails open on any trouble."""
    try:
        lim = float(bound)
    except ValueError:
        return funcs
    sc, _covered = scores()
    if not sc:
        return funcs                        # no table at all - fail open
    known = [f for f in funcs if f in sc]
    if known:
        return [f for f in known if sc[f] <= lim]
    # No rows for any of them. The table is built from every directory
    # decomp_overlay.py lists, and asm_dir() only resolves inside those, so a
    # resolvable overlay was definitely scanned: no rows means nothing here is
    # scorable at all - all data symbols or jump tables, which is what
    # "All functions are marked as difficult or are data sections" reports after
    # a worktree has already been built. Skip it.
    #
    # The narrow cost is an overlay whose functions appeared since the last
    # refresh (<=10 min): it is passed over this run and picked up by the next
    # driver, against a wasted worktree every time if we guessed the other way.
    return [] if asm_dir(overlay) is not None else funcs


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
        hard = difficult_names()
        if hard:
            keep = [f for f in keep if f not in hard]
            why = "left that is not already parked in difficult_functions"
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
