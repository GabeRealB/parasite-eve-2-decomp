#!/usr/bin/env python3
"""Claim an overlay and report whether any of it can actually land here.

Run by the list driver before it spends a worker on an overlay. Setting one up
costs a 235MB asm copy and four submodule clones, and an overlay whose
remaining functions are all shared bodies produces nothing: the landing filter
refuses them ("not landing X here: body is not in src/..."), so the whole
worktree is built and torn down for no result.

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
import json, pathlib, re, subprocess, sys

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


def main() -> int:
    overlay, session, pid = sys.argv[1], sys.argv[2], sys.argv[3]
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
    if not keep:
        orch("relinquish-overlay", "--session", session)
        print(f"{overlay}: {len(funcs)} claimed, 0 landable here "
              f"(shared or promoted bodies) - skipped before setup", file=sys.stderr)
        return 1
    print(json.dumps({"overlay": overlay, "claimed": len(funcs),
                      "landable": len(keep), "functions": keep}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
