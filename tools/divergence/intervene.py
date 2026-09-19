#!/usr/bin/env python3
"""Run isolated compiler counterfactuals; experimental output is never a source match."""
from __future__ import annotations

import argparse
import difflib
import gzip
import importlib.util
import json
from pathlib import Path
import re
import shutil
import sys

from evidence import ROOT, COMPILER, SUPPORTED, digest
from experiments import run_process
from report_paths import public_data, public_path, public_text


def tracer():
    spec = importlib.util.spec_from_file_location("divergence_trace_support", ROOT / "tools/trace_gcc.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def execute(input_path, output, function, intervention, timeout=60, target=None):
    if digest(COMPILER) != SUPPORTED:
        raise ValueError("unsupported compiler binary; intervention ABI is pinned")
    if not shutil.which("gdb"):
        raise ValueError("gdb is required")
    if not function:
        raise ValueError("a function is required to scope writes")
    if timeout <= 0:
        raise ValueError("timeout must be positive")
    if target and not Path(target).is_file():
        raise ValueError("target object is missing")
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=False)
    content = Path(input_path).read_bytes()
    if str(input_path).endswith(".gz"):
        content = gzip.decompress(content)
    source = output / "input.i"
    source.write_bytes(content)
    support = tracer()
    manifest = {"schema": 1, "compiler_sha256": SUPPORTED, "input_sha256": digest(source),
                "input_origin": public_path(input_path), "flags": support.FLAGS,
                "function": function, "intervention": intervention, "stages": {},
                "acceptance": "Compiler intervention only. Reproduce with ordinary C and the project build before accepting a match."}
    def save():
        (output / "manifest.json").write_text(json.dumps(public_data(manifest), indent=2) + "\n")
    save()
    symbols = support.symbols(COMPILER)
    observer = ROOT / "tools/gcc_trace_gdb.py"
    hook = Path(__file__).with_name("_gdb_intervene.py").resolve()
    manifest.update(observer_sha256=digest(observer), intervention_sha256=digest(hook))
    for phase in ("baseline", "control", "intervention"):
        directory = output / phase
        directory.mkdir()
        assembly = directory / "output.s"
        command = [str(COMPILER), *support.FLAGS, "-o", str(assembly), str(source)]
        if phase != "baseline":
            config = {"symbols": symbols, "events": str(directory / "events.jsonl"),
                      "function": function, "uids": [intervention["uid"]] if "uid" in intervention else [],
                      "control": phase == "control", "intervention": intervention}
            config_file = directory / "config.json"
            config_file.write_text(json.dumps(config))
            script = directory / "run.gdb"
            script.write_text("set pagination off\nset confirm off\nset disable-randomization off\npython\nimport json\n"
                              f"CONFIG = json.load(open({str(config_file)!r}))\n"
                              f"exec(compile(open({str(observer)!r}).read(), {str(observer)!r}, 'exec'))\n"
                              f"exec(compile(open({str(hook)!r}).read(), {str(hook)!r}, 'exec'))\nend\nrun\n")
            command = ["gdb", "-nx", "-batch", "-x", str(script), "--args", *command]
        result = run_process(command, ROOT, directory / "run.log", timeout)
        result["assembly_sha256"] = digest(assembly)
        for dump in output.glob("input.i.*"):
            shutil.copy2(dump, directory / dump.name)
        if phase != "baseline":
            events_file = directory / "events.jsonl"
            events = [json.loads(line) for line in events_file.read_text().splitlines()] if events_file.exists() else []
            result["interventions"] = [e for e in events if e["event"] == "intervention"]
            errors = [e for e in events if e["event"] == "error"]
            if errors or not any(e["event"] == "exit" and e.get("code") == 0 for e in events):
                result.update(status="failed", errors=errors or ["compiler did not exit normally; inspect run.log"])
            if not result["interventions"]:
                result.update(status="not_applied", reason="selected decision was never eligible")
        manifest["stages"][phase] = result
        save()
        if result["status"] != "complete":
            return manifest
        if phase == "control":
            manifest["control_identical"] = assembly.read_bytes() == (output / "baseline/output.s").read_bytes()
            save()
            if not manifest["control_identical"]:
                manifest["error"] = "no-op observation changed output; intervention aborted"
                save()
                return manifest
    before = (output / "baseline/output.s").read_text().splitlines(keepends=True)
    after = (output / "intervention/output.s").read_text().splitlines(keepends=True)
    (output / "output.diff").write_text("".join(difflib.unified_diff(before, after, "baseline", "intervention")))
    manifest["assembly_changed"] = before != after
    if target:
        manifest["target_sha256"] = digest(target)
        for phase in ("baseline", "intervention"):
            directory = output / phase
            raw = (directory / "output.s").read_text()
            raw = re.sub(r"(?m)^([ \t]*#?\.set[ \t]+(?:no)?volatile)[ \t]+#.*$", r"\1", raw)
            raw = re.sub(r"(?m)^([ \t]*li\.[ds][ \t]+[^#]*?)\s*#.*$", r"\1", raw)
            sanitized = directory / "assembler.s"
            sanitized.write_text(raw)
            obj = directory / "output.o"
            cmd = [sys.executable, str(ROOT / "tools/maspsx/maspsx.py"), "--aspsx-version=2.77", "--run-assembler", "--expand-div",
                   "-EL", "-O2", "-march=r3000", "-mtune=r3000", "-no-pad-sections", "-G0",
                   "-I", str(ROOT / "include"), "-I", str(ROOT / "build"), "-o", str(obj), str(sanitized)]
            assembly_run = run_process(cmd, ROOT, directory / "assemble.log", timeout)
            manifest["stages"][phase]["assemble"] = assembly_run
            if assembly_run["status"] == "complete":
                score = directory / "score.json"
                score_run = run_process([sys.executable, str(ROOT / "tools/claude-decomp-env/dist.py"), str(Path(target).resolve()), str(obj),
                                         "--stack-diffs", "--json", str(score)], ROOT, directory / "score.log", timeout)
                manifest["stages"][phase]["scoring"] = score_run
                if score_run["status"] == "complete" and score.exists():
                    manifest["stages"][phase]["score"] = json.loads(score.read_text())
    save()
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="retained .i or .i.gz input")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--function", required=True)
    parser.add_argument("--kind", choices=("global_order", "schedule_ready"), required=True)
    parser.add_argument("--before", type=int, help="pseudo to allocate first")
    parser.add_argument("--after", type=int, help="pseudo to allocate later")
    parser.add_argument("--uid", type=int, help="ready instruction to prefer within its equal-priority group")
    parser.add_argument("--pass", dest="phase", choices=("sched1", "sched2"), default="sched1")
    parser.add_argument("--timeout", type=float, default=60)
    parser.add_argument("--target-object", type=Path, help="also assemble and independently score both outputs")
    args = parser.parse_args()
    intervention = {"kind": args.kind}
    if args.kind == "global_order":
        if args.before is None or args.after is None or args.before == args.after:
            parser.error("global_order needs two different --before/--after pseudos")
        intervention.update(before=args.before, after=args.after)
    else:
        if args.uid is None:
            parser.error("schedule_ready needs --uid")
        intervention.update(uid=args.uid, **{"pass": args.phase})
    try:
        data = execute(args.input, args.output, args.function, intervention, args.timeout, args.target_object)
    except (OSError, ValueError) as error:
        parser.exit(2, f"intervention unavailable: {public_text(str(error))}\n")
    print(json.dumps(public_data(data), indent=2))
    success = data.get("control_identical") and data.get("stages", {}).get("intervention", {}).get("status") == "complete"
    if args.target_object:
        success = success and all("score" in data["stages"].get(phase, {}) for phase in ("baseline", "intervention"))
    return 0 if success else 1


if __name__ == "__main__":
    raise SystemExit(main())
