#!/usr/bin/env python3
"""Walk a .lreg dump: per basic block, list insns in order with their
local-alloc insn_number, set destinations, REG_DEAD notes and calls.
usage: lregwalk.py file.i.lreg [pseudo ...]  -- restrict to blocks mentioning pseudos"""
import re, sys
text = open(sys.argv[1]).read()
want = set(sys.argv[2:])
# split into top-level rtl objects
objs = re.split(r"\n(?=\(|;;)", text)
blocks = []; cur = None
for o in objs:
    if o.startswith(";; Start of basic block") or o.startswith(";; End of basic block"):
        m = re.search(r"start of basic block (\d+)|Start of basic block (\d+)", o)
        if m:
            cur = {"n": m.group(1) or m.group(2), "insns": []}
            blocks.append(cur)
        else:
            cur = None
        continue
    m = re.match(r"\((insn|call_insn|jump_insn|code_label|barrier|note) (\d+)", o)
    if not m or cur is None: continue
    kind, uid = m.group(1), m.group(2)
    if kind == "note": continue
    dests = re.findall(r"\(set \((?:reg[/a-z]*:\w+ (\d+)|mem)", o)
    dests = [d for d in dests if d]
    deads = re.findall(r"REG_DEAD \(reg[/a-z]*:\w+ (\d+)", o)
    clob = re.findall(r"\(clobber \(reg[/a-z]*:\w+ (\d+)", o)
    srcregs = re.findall(r"\(reg[/a-z]*:\w+ (\d+)", o)
    cur["insns"].append((kind, uid, dests, deads, clob, sorted(set(srcregs))))
for b in blocks:
    regs = set()
    for ins in b["insns"]:
        regs.update(ins[5])
    if want and not (want & regs): continue
    print(f"== block {b['n']}: {len(b['insns'])} insns")
    for i, (kind, uid, dests, deads, clob, srcs) in enumerate(b["insns"], 1):
        tag = {"call_insn": "CALL", "jump_insn": "JMP", "code_label": "LABEL", "barrier": "BAR"}.get(kind, "")
        print(f"  #{i:3d} uid {uid:>5} {tag:4} set={','.join(dests) or '-':>8} dead={','.join(deads) or '-':<12} regs={' '.join(srcs)}")
