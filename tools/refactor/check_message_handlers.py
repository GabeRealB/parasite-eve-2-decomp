#!/usr/bin/env python3
"""Check the shape of every task-message handler.

A `TaskMessageEntry` table holds an unprototyped callback, `s32 (*)()`, so the
compiler accepts any function in it. `taskMessageDispatch` nevertheless calls
every handler the same way: the receiving task, the message id and two payload
words. This tool enforces what the type cannot:

    <s32 or void> handler(Task*, s32, <payload>, <payload>)

Four parameters, a `Task*` first and an `s32` second. The payload types are the
handler's own. A handler returning `void` is listed, not failed: a handful only
match that way, and dispatch then forwards whatever the result register holds,
so no sender may read a result from such a message.

    check_message_handlers.py            # exit 1 on a misshapen handler
    check_message_handlers.py --void     # also list the void handlers
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cref  # noqa: E402
import clang.cindex as ci  # noqa: E402


def scan(job):
    rel, args, root = job
    tu = cref.parse_tu(rel, args, root)
    out = []
    if tu is None:
        return out
    for cur in tu.cursor.get_children():
        if cur.kind != ci.CursorKind.VAR_DECL or "TaskMessageEntry" not in cur.type.spelling:
            continue
        for c in cur.walk_preorder():
            if c.kind != ci.CursorKind.DECL_REF_EXPR or c.referenced is None:
                continue
            fn = c.referenced
            if fn.kind != ci.CursorKind.FUNCTION_DECL:
                continue
            params = [a.type.get_canonical().spelling for a in fn.get_arguments()]
            ret = fn.result_type.get_canonical().spelling
            problems = []
            if len(params) != 4:
                problems.append(f"{len(params)} parameter(s), not 4")
            if params[:1] and params[0] not in ("Task *", "struct Task *"):
                problems.append(f"first parameter is {params[0]}, not Task*")
            if params[1:2] and params[1] != "int":
                problems.append(f"second parameter is {params[1]}, not s32")
            if ret not in ("int", "void"):
                problems.append(f"returns {ret}")
            where = f"{cref.relpath(c.location.file.name, root)}:{c.location.line}"
            out.append((fn.spelling, where, tuple(problems), ret == "void"))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--version", default=cref.DEFAULT_VERSION)
    ap.add_argument("-j", "--jobs", type=int, default=int(os.environ.get("PE2_JOBS") or 0) or os.cpu_count() or 8)
    ap.add_argument("--void", action="store_true", help="list the handlers that return void")
    args = ap.parse_args()
    root = cref.repo_root()
    db = cref.load_db(root, args.version)
    units = sorted(db)
    from multiprocessing import Pool
    rows = set()
    with Pool(args.jobs) as pool:
        for out in pool.imap_unordered(scan, [(u, db[u], root) for u in units], chunksize=4):
            rows.update(out)
    handlers = {name for name, _w, _p, _v in rows}
    bad = sorted((w, n, p) for n, w, p, _v in rows if p)
    void = sorted({n for n, _w, _p, v in rows if v})
    for where, name, problems in bad:
        print(f"{where}: {name}: {'; '.join(problems)}")
    if args.void:
        for n in void:
            print(f"void: {n}")
    print(f"{len(handlers)} handler(s) in message tables: {len({n for _w, n, _p in bad})} misshapen, {len(void)} return void")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
