"""Declare the sound banks of the packages a project builds: each bank, and the instruments, scripts and samples it
binds - each of those once, under the first bank binding it.

    sounds.py declare --registry REGISTRY.json --cli PE2-CLI --roms DIR [--release USA]

A bank is the `sounds` slot of a file whose `package` slot names a package (`res://packages/...`); it is declared beside
that package's other assets, in `<group>/sounds.toml` (`pe/antibody/sounds/level_1`, `weapons/m93r/sounds/bank`). What
the game stores as one image is four kinds of node, each the pe2.sounds extension's:

- a sound: a sample of the bank's pool, a WAV its node imports into the body the bank stores (`sound.wav`), at the rate
  its banks mostly play it at (`tools/peassets/sound/pitch.py`) and in the encoder mode that reproduces its body;
- an instrument: a program, its layers playing its own samples (`samples`, in order of first use);
- a sound script: what a sound id starts, its voices playing its own instruments (`instruments`, in order of first use);
- a sound bank: its `instruments` at their program numbers, its `scripts` in its image's order, its `samples` in its
  pool's, read off the disc as its chunk (`from.<release>`).

An instrument or script is the same as another where its file and what its list names are; a sample, where its body
is. One shared is named after the first bank binding it, by its place there: `<bank>_instrument_<program>`,
`<bank>_script_<n>`, `<bank>_sample_<n>` (without `<bank>_` in a group of one bank). Nothing is named from what the code
calls a sound; a script is known by the slots that start it.

The banks are read through the plan the declarations make (`pe2-cli plan observe`), so what is declared is what a
project's extraction reads. Rerunnable: each `sounds.toml` is written whole, its uids kept by path.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
from multiprocessing import Pool
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "buildsteps"))
sys.path.insert(0, str(HERE.parent / "peassets"))
import declarations as declared  # noqa: E402
import init  # noqa: E402
import plan as plans  # noqa: E402
import toml_out  # noqa: E402
from sound import adpcm, codec, pitch  # noqa: E402

PACKAGES = "res://packages/"
HEADER = ("# Declarations: sound banks as the disc stores them, and the instruments, scripts and samples they bind - each "
          "once,\n# under the first bank binding it (tools/editor_project/sounds.py).\n")


def banks_of(decl) -> list[dict]:
    """Each bank: the file holding it, the group its package's assets are in, and its name there."""
    found = []
    for path, d in sorted(decl.by_path.items()):
        table = d.table
        slots = table.get("slots", [])
        if table.get("template") != "file" or not any(s.get("name") == "sounds" and s.get("type") == "sound_bank"
                                                      for s in slots):
            continue
        packages = [s["package"]["path"] for s in slots
                    if s.get("type") == "package" and str(s.get("package", {}).get("path", "")).startswith(PACKAGES)]
        if not packages:
            continue
        group = packages[0][len(PACKAGES):].removesuffix(".node")
        package = group.rsplit("/", 1)[-1]
        file = path.rsplit("/", 1)[-1]
        name = "" if file == package else file.removeprefix(f"{package}_")
        found.append({"file": path, "group": group, "name": name, "from": table.get("from", {})})
    for bank in found:
        alone = sum(1 for b in found if b["group"] == bank["group"]) == 1
        bank["name"] = bank["name"] or "bank"
        bank["prefix"] = "" if alone else f"{bank['name']}_"
        bank["path"] = f"{bank['group']}/sounds/{bank['name']}"
    return found


def read_banks(decl, registry: dict, banks: list[dict], release: str, cli: Path, roms: Path, scratch: Path) -> None:
    """Each bank's bytes and sha1 (`blob`, `sha1`), read through the plan its file's declaration makes."""
    made = plans.make(decl, registry, [release])
    containers = {(release, int(id.rsplit("/", 1)[-1])): (line["in"]["disc"], id)
                  for id, line in made.ops.items() if line["op"] == "container"}
    places = {}
    for path, d in decl.by_path.items():
        for at in d.table.get("from", {}).get(release, {}).get("at", []):
            if "disc" not in at and "slot" not in at and "stage" in at and "file" in at:
                places[(release, at["stage"], at.get("folder"), at["file"])] = path
    for bank in banks:
        at = dict(bank["from"][release]["at"][0], slot="sounds")
        bank["at"] = at
        bank["read"] = plans.chunk(made, release, at, decl.by_path, places, containers)
    wanted = {b["read"] for b in banks}
    lines = {id: line for id, line in made.ops.items() if line["op"] in ("disc", "container") or id in wanted}
    head = {"plan": plans.VERSION, "requires": [], "releases": [release],
            "inputs": {release: {"look_in": plans.LOOK_IN, "discs": made.inputs[release]}}}
    survey = scratch / "sounds.plan.jsonl"
    survey.write_text("\n".join(json.dumps(line, sort_keys=True) for line in [head] + plans.ordered(lines)) + "\n",
                      encoding="utf-8")
    kept = scratch / "kept"
    observed = subprocess.run([str(cli), "plan", "observe", str(survey), "--roms", str(roms), "--keep", str(kept),
                               "--keep-kinds", "chunk"], check=True, capture_output=True, text=True).stdout
    seen = {o["id"]: o for o in map(json.loads, filter(None, observed.split("\n")))}
    for bank in banks:
        o = seen.get(bank["read"], {})
        if not o.get("sha1"):
            raise SystemExit(f"{bank['file']}: its bank is not read ({o.get('error')})")
        bank["sha1"] = o["sha1"]
        bank["blob"] = (kept / o["sha1"]).read_bytes()


def canonical(item) -> str:
    return json.dumps(item, sort_keys=True)


def first_use(items) -> list:
    out = []
    for i in items:
        if i not in out:
            out.append(i)
    return out


def identities(bank: dict) -> tuple[list[str], list[str], list[str]]:
    """What makes each sample, instrument (by program) and script (by entry, in image order) of a bank the same as
    another's: a sample its body; an instrument its file and its samples; a script its file, its instruments, and what
    each voice reaches past its program's layers (the interpreter counts a voice's layer from its program's first,
    unbounded, into the next programs')."""
    decoded = bank["decoded"]
    samples = []
    for i, item in enumerate(decoded["pool"]):
        if "sample" not in item:
            raise SystemExit(f"{bank['file']}: pool item {i} is raw bytes, which no node holds yet")
        samples.append(hashlib.sha1(item["sample"]).hexdigest())
    instruments = []
    for program in decoded["programs"]:
        own = first_use(layer["sample"] for layer in program["layers"])
        layers = [dict(layer, sample=own.index(layer["sample"])) for layer in program["layers"]]
        instruments.append(canonical({"volume": program["volume"], "pan": program["pan"], "layers": layers,
                                      "samples": [samples[s] for s in own]}))
    flat = [(p, n) for p, program in enumerate(decoded["programs"]) for n in range(len(program["layers"]))]
    chunks = decoded["chunks"]
    scripts = []
    for chunk in chunks:
        if chunk["kind"] != "entry":
            continue
        voices = [c for c in chunk["commands"] if c["op"] == "voice"]
        if any(v.get("bank") for v in voices):
            raise SystemExit(f"{bank['file']}: a voice plays another bank's program, which no node binds yet")
        own = first_use(v["program"] for v in voices)
        commands, reached = [], []
        for c in chunk["commands"]:
            c = dict(c)
            if c["op"] == "voice":
                for field in ("adsr", "envelope"):
                    if c[field] is not None:
                        c[field] = {k: v for k, v in chunks[c[field]].items() if k != "kind"}
                at = sum(len(p["layers"]) for p in decoded["programs"][:c["program"]]) + c["layer"]
                if c["layer"] >= len(decoded["programs"][c["program"]]["layers"]):
                    reached.append(None if at >= len(flat) else [instruments[flat[at][0]], flat[at][1]])
                c["program"] = own.index(c["program"])
            commands.append(c)
        scripts.append(canonical({"controls": chunk["controls"], "commands": commands, "reached": reached,
                                  "instruments": [instruments[p] for p in own]}))
    return samples, instruments, scripts


def mode_of(body: bytes) -> str:
    return adpcm.decode(body)[1]


def named(banks: list[dict]) -> dict:
    """Each sample, instrument and script by what makes it the same: its path, named after the first bank binding it,
    and that bank."""
    nodes: dict[tuple[str, str], dict] = {}
    for bank in banks:
        samples, instruments, scripts = bank["identities"]
        for kind, keys in (("sample", samples), ("instrument", instruments), ("script", scripts)):
            for n, key in enumerate(keys):
                nodes.setdefault((kind, key), {"path": f"{bank['group']}/sounds/{bank['prefix']}{kind}_{n}",
                                               "bank": bank, "at": n})
    return nodes


def node_uid(path: str, taken: set[str]) -> str:
    salt = 0
    while True:
        uid = init.uid_text(int.from_bytes(hashlib.sha1(f"sound:{path}:{salt}".encode()).digest()[:8], "big") >> 1)
        if uid not in taken:
            return uid
        salt += 1


def table_text(name: str, fields: dict) -> str:
    """A declaration's table: its fields, then its `import` and each release's `from` as sections."""
    sections = []
    own = {k: v for k, v in fields.items() if k not in ("import", "from")}
    sections.append(f"[{toml_out.key(name)}]\n" + "\n".join(f"{toml_out.key(k)} = {toml_out.value(v)}" for k, v in own.items()))
    if "import" in fields:
        sections.append(f"[{toml_out.key(name)}.import]\n"
                        + "\n".join(f"{toml_out.key(k)} = {toml_out.value(v)}" for k, v in fields["import"].items()))
    for release, placed in fields.get("from", {}).items():
        # A placement on one line, as the other declarations write it.
        line = lambda k, v: (f"at = [{toml_out.value(v[0])}]" if k == "at" and len(v) == 1  # noqa: E731
                             else f"{toml_out.key(k)} = {toml_out.value(v)}")
        sections.append(f"[{toml_out.key(name)}.from.{release}]\n" + "\n".join(line(k, v) for k, v in placed.items()))
    return "\n\n".join(sections)


def declare(root: Path, banks: list[dict], nodes: dict, release: str) -> int:
    decl = declared.Declarations(root)
    kept = {path: d.uid for path, d in decl.by_path.items()}
    taken = set(decl.by_uid)
    uids = {}

    def uid(path: str) -> str:
        if path not in uids:
            uids[path] = kept.get(path) or node_uid(path, taken)
            taken.add(uids[path])
        return uids[path]

    def ref(path: str) -> dict:
        return {"uid": uid(path), "path": f"res://{path}.node"}

    groups: dict[str, dict[str, dict]] = {}
    for bank in banks:
        samples, instruments, scripts = bank["identities"]
        decoded = bank["decoded"]
        node = lambda kind, key: nodes[(kind, key)]["path"]  # noqa: E731
        groups.setdefault(bank["group"], {})[bank["name"]] = {
            "uid": uid(bank["path"]), "template": "sound_bank",
            "instruments": [ref(node("instrument", k)) for k in instruments],
            "scripts": [ref(node("script", k)) for k in scripts],
            "samples": [ref(node("sample", k)) for k in samples],
            "from": {release: {"at": [bank["at"]], "sha1": bank["sha1"]}},
        }
        for n, key in enumerate(instruments):
            if nodes[("instrument", key)]["bank"] is not bank:
                continue
            program = decoded["programs"][n]
            own = first_use(layer["sample"] for layer in program["layers"])
            path = node("instrument", key)
            groups[bank["group"]][path.rsplit("/", 1)[-1]] = {
                "uid": uid(path), "template": "instrument", "samples": [ref(node("sample", samples[s])) for s in own]}
        entries = [c for c in decoded["chunks"] if c["kind"] == "entry"]
        for n, key in enumerate(scripts):
            if nodes[("script", key)]["bank"] is not bank:
                continue
            own = first_use(c["program"] for c in entries[n]["commands"] if c["op"] == "voice")
            path = node("script", key)
            groups[bank["group"]][path.rsplit("/", 1)[-1]] = {
                "uid": uid(path), "template": "sound_script",
                "instruments": [ref(node("instrument", instruments[p])) for p in own]}
        for n, key in enumerate(samples):
            found = nodes[("sample", key)]
            if found["bank"] is not bank:
                continue
            fields = {"uid": uid(found["path"]), "template": "sound", "rate": found["rate"]}
            if found["mode"] != "normal":
                fields["mode"] = found["mode"]
            fields["import"] = {"importer": "pe2.sounds@wav", "source": "sound.wav"}
            fields["from"] = {release: {"sha1": key}}
            groups[bank["group"]][found["path"].rsplit("/", 1)[-1]] = fields
    order = {"sound_bank": 0, "sound_script": 1, "instrument": 2, "sound": 3}
    for group, tables in groups.items():
        def sort_key(item):
            name, fields = item
            tail = name.rsplit("_", 1)
            return (order[fields["template"]], tail[0], int(tail[1]) if len(tail) == 2 and tail[1].isdigit() else -1)
        body = "\n\n".join(table_text(name, fields) for name, fields in sorted(tables.items(), key=sort_key))
        out = root / group / "sounds.toml"
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(HEADER + "\n" + body + "\n", encoding="utf-8")
    return len(groups)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)
    d = sub.add_parser("declare")
    d.add_argument("--registry", type=Path, required=True, help="what a project's extensions offer (pe2-cli project registry)")
    d.add_argument("--cli", type=Path, required=True, help="pe2-cli")
    d.add_argument("--roms", type=Path, required=True, help="the folder holding the release's discs")
    d.add_argument("--release", default="USA")
    d.add_argument("--declarations", type=Path, default=declared.DECLARATIONS)
    d.add_argument("--jobs", type=int, default=None)
    args = ap.parse_args()
    root = args.declarations
    registry = json.loads(args.registry.read_text(encoding="utf-8"))
    decl = declared.Declarations(root)
    banks = [b for b in banks_of(decl) if b["from"].get(args.release)]
    (ROOT / "build").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=ROOT / "build", prefix="sounds-") as scratch:
        read_banks(decl, registry, banks, args.release, args.cli, args.roms, Path(scratch))
    for bank in banks:
        bank["decoded"] = codec.decode(bank["blob"])
        bank["identities"] = identities(bank)
    nodes = named(banks)
    bodies = {key: item["sample"] for bank in banks for key, item in zip(bank["identities"][0], bank["decoded"]["pool"])}
    octaves = pitch.octaves_of(bank["blob"] for bank in banks)
    keys = sorted(bodies)
    with Pool(args.jobs) as pool:
        modes = dict(zip(keys, pool.map(mode_of, [bodies[k] for k in keys])))
    for key in keys:
        found = nodes[("sample", key)]
        found["mode"] = modes[key]
        found["rate"] = pitch.rate(octaves.get(key, 0))
    written = declare(root, banks, nodes, args.release)
    counts = {kind: sum(1 for k in nodes if k[0] == kind) for kind in ("instrument", "script", "sample")}
    print(f"{len(banks)} banks declared in {written} groups: {counts['instrument']} instruments, {counts['script']} "
          f"scripts, {counts['sample']} samples")
    return 0


if __name__ == "__main__":
    sys.exit(main())
