"""Evidence-based mechanism selection for every divergence family.

Never infer causality from the last changed dump alone. Rules are candidate
mechanisms; observations carry their source and unresolved questions explicitly.
"""
from __future__ import annotations

from collections import Counter
from dataclasses import asdict
from pathlib import Path
import re

import dumps
import rtl
from common import reg_number
from evidence import load_trace, read_json, digest, artifact_provenance
from rules import Sources, edits


FAMILY_RULES = {
    "register": ["allocation.global", "allocation.local", "allocation.reload"],
    "scheduling": ["scheduling", "delay_slot"], "delay_slot": ["delay_slot", "assembler"],
    "opcode": ["instruction_selection", "extension"],
    "immediate": ["instruction_selection", "extension", "cse"],
    "displacement": ["address", "loop"], "symbol": ["address", "linkage"],
    "stack": ["stack", "allocation.reload"], "branch": ["branch", "loop"],
    "call": ["call"], "hazard": ["assembler", "scheduling"],
    "insertion": ["cse", "instruction_selection", "allocation.reload", "branch"],
    "deletion": ["cse", "instruction_selection", "branch", "allocation.reload"],
    "mixed": ["instruction_selection", "address", "branch", "allocation.reload"],
}
PASS_RULE = {"cse": "cse", "cse2": "cse", "combine": "instruction_selection",
             "loop": "loop", "jump": "branch", "jump2": "branch", "dbr": "delay_slot"}


def reorders(before, after, uid):
    """Actual relative order among surviving instructions, ignoring RTL changes."""
    if uid not in before or uid not in after or not before[uid].active or not after[uid].active:
        return []
    shared = [n for n in before if n in after and before[n].active and after[n].active and n != uid]
    return [n for n in shared if (before[n].order < before[uid].order) != (after[n].order < after[uid].order)]


def source_location(compile_, uid, provenance=None):
    if (provenance or artifact_provenance(compile_.attempt))["status"] == "mismatch":
        return {"source_path": str(compile_.attempt.source), "source_line": None, "mapping": "stale_artifacts"}
    for nodes in compile_.snapshots.values():
        node = nodes.get(uid)
        if not node or not node.source:
            continue
        filename, number = node.source
        # Read only this attempt's retained input/source, not arbitrary paths
        # embedded in externally supplied dumps.
        path = compile_.attempt.preprocessed if filename.endswith(".i") else compile_.attempt.source
        lines = path.read_text(errors="replace").splitlines() if path.is_file() else []
        text = lines[number - 1].strip() if 0 < number <= len(lines) else None
        source_lines = compile_.attempt.source.read_text(errors="replace").splitlines() if compile_.attempt.source.is_file() else []
        token_text = re.sub(r"\s+", "", text) if text else None
        candidates = [i + 1 for i, line in enumerate(source_lines) if token_text and re.sub(r"\s+", "", line) == token_text]
        return {"dump_filename": filename, "dump_line": number, "text": text,
                "source_path": str(compile_.attempt.source),
                "source_line": candidates[0] if len(candidates) == 1 else None,
                "mapping": "unique_line_text" if len(candidates) == 1 else "unresolved"}
    return None


class Analyzer:
    def __init__(self, compile_, trace=None, gcc_source=None):
        self.compile = compile_
        self.provenance = artifact_provenance(compile_.attempt)
        self.sources = Sources(gcc_source) if gcc_source else Sources()
        self.trace, self.trace_status = load_trace(trace, compile_.attempt, compile_.function)
        self.messages = {}
        for name, text in compile_.texts.items():
            for number, line in enumerate(text.split("\n"), 1):
                if line.lstrip().startswith("("):
                    continue
                for match in re.finditer(r"\b(?:insn|uid)\s+(\d+)\b", line, re.I):
                    self.messages.setdefault(int(match[1]), []).append({"pass": name, "line": number, "text": line.strip()})

    def one(self, attribution):
        c, a = self.compile, attribution
        finding = a.finding
        rules = list(FAMILY_RULES.get(finding.family, ["instruction_selection"]))
        observations, unknowns, features = [], [], {}
        def observe(kind, text, **values):
            observations.append({"kind": kind, "text": text, **values})
        timeline = rtl.events(c.snapshots, a.uid) if a.uid is not None else []
        if a.uid is not None:
            observe("instruction_link", f"Object instruction corresponds to compiler UID {a.uid}, pattern {a.pattern}.",
                    source=str(c.attempt.cc1_assembly), confidence="canonical_alignment")
        else:
            unknowns.append(a.note or "No supported object-to-compiler instruction correspondence.")
        for event in timeline:
            if event["event"] == "appeared" and event["from_pass"] is None:
                continue
            observe("rtl_change", f"UID {a.uid}: {event['event']} between {event['from_pass']} and {event['pass']}.",
                    source=str(c.attempt.dump(event["pass"])), **event)
        features["timeline"] = timeline
        if a.uid is not None:
            shapes = []
            for name, nodes in c.snapshots.items():
                node = nodes.get(a.uid)
                if not node or not node.active:
                    continue
                shape = {"memory_modes": re.findall(r"\(mem[/\w]*:(\w+)", node.expression),
                         "extensions": re.findall(r"\((sign_extend|zero_extend|truncate):\w+", node.expression),
                         "constants": re.findall(r"\(const_int\s+(-?\d+)\)", node.expression),
                         "symbols": re.findall(r'\(symbol_ref[^\n]*?"([^"\n]+)"', node.expression),
                         "predicates": re.findall(r"\((eq|ne|lt|le|gt|ge|ltu|leu|gtu|geu)[:\s]", node.expression),
                         "label_refs": re.findall(r"\(label_ref[/\w]*\s+(\d+)\)", node.expression)}
                if not shapes or shape != shapes[-1]["shape"]:
                    shapes.append({"pass": name, "shape": shape, "line": node.line})
            features["expression_shapes"] = shapes
            if finding.family != "register":
                for item in shapes:
                    interesting = {k: v for k, v in item["shape"].items() if v}
                    if interesting:
                        observe("expression_shape", f"At .{item['pass']}: {interesting}.",
                                source=str(c.attempt.dump(item["pass"])), line=item["line"])
            # Restrict textual diagnostics to messages naming this insn, rather
            # than attributing all optimisation messages in the function to it.
            messages = self.messages.get(a.uid, [])
            features["compiler_messages"] = messages
            for item in messages[:8]:
                observe("compiler_message", item["text"], source=str(c.attempt.dump(item["pass"])), line=item["line"])
        if finding.family == "register" and a.pseudo is not None:
            allocation = self.allocation(a, observe, unknowns)
            features["allocation"] = allocation
            rules = ["allocation.local" if allocation["local_home"] is not None else "allocation.global"]
            if (allocation["local_home"] is not None and allocation["local_home"] != allocation["final_home"]) or any(r["event"] == "spill" for r in allocation["trace"]):
                rules.append("allocation.reload")
        elif finding.family == "register":
            unknowns.append("No unique pseudo for the differing operand; do not infer a single allocation decision from a hard-register rename.")
        if a.uid is not None:
            schedules = []
            for first, last in (("combine", "sched"), ("greg", "sched2"), ("jump2", "dbr")):
                moved = reorders(c.snapshots.get(first, {}), c.snapshots.get(last, {}), a.uid)
                if moved:
                    schedules.append({"from_pass": first, "pass": last, "crossed_uids": moved})
                    observe("relative_order", f"UID {a.uid} changes relative order with {moved} between {first} and {last}.",
                            source=str(c.attempt.dump(last)), crossed_uids=moved)
            features["order_changes"] = schedules
            selections = [r for r in self.trace if r["event"].startswith("schedule_") and self.involves(r, a.uid)]
            features["scheduling_trace"] = selections
            for event in selections:
                if event["event"] == "schedule_select":
                    observe("trace_selection", f"{event['pass']} backward cycle {event['clock']}: ready {[n['uid'] for n in event['before']]}; selected {event.get('selected')}.",
                            trace_event=event["event_index"], hazards=event.get("hazards", []))
                    for hazard in event.get("hazards", []):
                        if hazard.get("uid") == a.uid and hazard["kind"] == "actual" and hazard["cost"] > 0:
                            observe("hazard_block", f"UID {a.uid} was blocked by an actual hazard of {hazard['cost']} cycles in {event['pass']} at backward cycle {event['clock']}.", trace_event=event["event_index"])
                    potentials = {h["uid"]: h["cost"] for h in event.get("hazards", []) if h["kind"] == "potential"}
                    if a.uid in potentials and event.get("selected") in potentials and event["selected"] != a.uid:
                        selected = event["selected"]
                        observe("hazard_preference", f"Potential-hazard weights: UID {a.uid}={potentials[a.uid]}, selected UID {selected}={potentials[selected]}; larger weights are preferred within the eligible priority group.", trace_event=event["event_index"])
                elif event["event"] == "schedule_compare":
                    observe("trace_comparison", f"{event['pass']}: comparator prefers UID {event['winner']} by {event['reason']}; this is not the final selection.",
                            trace_event=event["event_index"])
                elif event["event"] == "schedule_release":
                    observe("trace_release", f"{event['pass']}: scheduling UID {event['scheduled']} updates dependencies.",
                            trace_event=event["event_index"], dependencies=event.get("dependencies", []))
            if finding.family in ("scheduling", "delay_slot", "hazard") and not selections:
                unknowns.append("Dump order alone does not reveal dependency release, ready-list alternatives or hazard selection.")
            for event in timeline:
                if event["from_pass"] == "greg" and event["pass"] == "sched2" and event["event"] == "expression_changed":
                    if "(mem" in (event["before"] or "") and "(mem" not in (event["after"] or ""):
                        rules.insert(0, "allocation.reload")
                        unknowns.append("Load disappeared between greg and sched2; post-reload CSE is a candidate, not proven by the dump boundary.")
                elif event["pass"] in PASS_RULE and event["from_pass"] is not None:
                    rules.append(PASS_RULE[event["pass"]])
            late = [r for r in self.trace if r["event"] == "postreload_set" and r.get("uid") == a.uid]
            for event in late:
                observe("trace_substitution", "Post-reload CSE proposed a substitution; verify the retained sched2 expression.",
                        trace_event=event["event_index"], before=event["before"], after=event["after"])
        if finding.family == "opcode" and finding.pair.target and finding.pair.ours:
            load_modes = {"lb": (8, "signed"), "lbu": (8, "unsigned"), "lh": (16, "signed"), "lhu": (16, "unsigned"), "lw": (32, "word")}
            left, right = finding.pair.target.mnemonic, finding.pair.ours.mnemonic
            if left in load_modes and right in load_modes:
                rules.insert(0, "extension")
                observe("access_mode", f"Target load is {load_modes[left]}; this compile loads {load_modes[right]}.")
        if finding.family == "stack":
            observe("stack_operands", "Stack displacement/frame adjustment differs; stack-slot origin remains to be established.")
        if finding.family in ("stack", "displacement", "symbol", "immediate", "call", "branch"):
            observe("operand_requirement", f"Target: {finding.pair.target.text if finding.pair.target else '(absent)'}; candidate: {finding.pair.ours.text if finding.pair.ours else '(absent)'}. Configuration and value semantics must be checked before changing these operands.")
        if finding.family == "hazard" and finding.pair.ours:
            index = finding.pair.ours.index
            context = [i.text for i in c.ours[max(0, index-2):index+3]]
            observe("object_context", f"Object sequence around the nop/hazard site: {context}.", source=str(c.attempt.ours))
        if a.uid is None and finding.pair.ours is not None:
            rules.append("assembler")
        if finding.pair.target is not None and finding.pair.ours is None:
            unknowns.append("Only the target has this instruction; no target RTL exists to trace its origin.")
        if finding.pair.confidence == "positional":
            unknowns.append("Target/candidate correspondence is positional within a replacement run; operand causality is tentative.")
        rules = list(dict.fromkeys(rules))
        mechanisms = [self.sources.describe(name) for name in rules]
        proposals = edits(rules)
        location = source_location(c, a.uid, self.provenance) if a.uid is not None else None
        for proposal in proposals:
            proposal["location"] = location
            proposal["target_requirement"] = finding.pair.target.text if finding.pair.target else "remove the extra instruction without changing behavior"
        return {"status": "observed" if observations else "unresolved", "observations": observations,
                "hypotheses": mechanisms, "unknowns": list(dict.fromkeys(unknowns)),
                "proposals": proposals, "features": features, "source_location": location}

    @staticmethod
    def involves(event, uid):
        if any(n.get("uid") == uid for n in event.get("before", []) if isinstance(n, dict)):
            return True
        if event.get("x", {}).get("uid") == uid or event.get("y", {}).get("uid") == uid:
            return True
        return any(d.get("before", {}).get("uid") == uid for d in event.get("dependencies", []))

    def allocation(self, a, observe, unknowns):
        c, p = self.compile, a.pseudo
        greg = c.texts.get("greg", "")
        order, conflicts = dumps.allocation_order(greg), dumps.conflicts(greg)
        stats = c.pseudos.get(p)
        current = [r for r in self.trace if (r["event"] in ("local_choice", "global_choice") and p in r.get("members", []))
                   or (r["event"] == "spill" and any(change[0] == p for change in r.get("changes", [])))]
        target = reg_number(next(iter(a.finding.substitutions), ""))
        initial = conflicts.get(p)
        data = {"pseudo": p, "stats": asdict(stats) if stats else None,
                "local_home": c.local_homes.get(p), "final_home": c.final_homes.get(p),
                "global_rank": order.index(p) if p in order else None,
                "initial_conflicts": initial, "target_register": target, "trace": current,
                "target_initially_conflicts": target in initial if initial is not None and target is not None else None}
        observe("allocation_snapshot", f"Pseudo {p}: local home {data['local_home']}, final home {data['final_home']}, global rank {data['global_rank']} (zero based).",
                source=str(c.attempt.dump("greg")), **{k: v for k, v in data.items() if k != "trace"})
        if data["target_initially_conflicts"]:
            observe("initial_conflict", f"Target register {target} is in the printed initial conflict set; overlap at any point can exclude it.", source=str(c.attempt.dump("greg")))
        elif initial is not None:
            unknowns.append("Absence from the printed initial conflict set does not establish availability at allocation time.")
        for event in current:
            if event["event"] == "global_choice":
                blocked = target in event.get("hard_conflicts", []) if target is not None else None
                observe("global_choice", f"Observed global choice for {event['members']}: register {event['result']}; target in current hard conflicts: {blocked}.", trace_event=event["event_index"], decision=event)
            elif event["event"] == "local_choice":
                observe("local_choice", f"Observed local quantity b{event['block']}/q{event['qty']}: members {event['members']}, refs {event['refs']}, interval {event['birth']}..{event['death']}, result {event['result']}.", trace_event=event["event_index"], decision=event)
            else:
                observe("spill", f"Reload reserves register {event['reg']}; recorded home changes {event['changes']}.", trace_event=event["event_index"])
        if not current:
            unknowns.append("Exact quantity/allocno decision inputs and later evictions are not available without a matching trace.")
        return data
