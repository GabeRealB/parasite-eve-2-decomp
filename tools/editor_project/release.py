"""Declare a release whose discs have another's structure: where each asset is on its discs, and its bytes' sha1.

    release.py declare EUR --like USA --images DISC1.cue DISC2.cue --registry REGISTRY.json [--cli PE2-CLI]

A release made from another - Europe from the USA's - holds the same files of the same chunks in the same order, much
of it the same bytes and the rest translated or tuned: its code, its sounds, its text. So each asset is where the other
release has it, and what is left to find is whether its bytes are the same.

- Its disc layouts (`disc/<release>/disk<N>.toml`, `stage<N>.toml`) are the other's, with its images (name, size,
  sha1), its creation dates and its discs' file names - read off the images, through `dumpsxiso`'s layout, which says
  the rest is the same, or refuses.
- The reads the other release's declarations make (the plan they make, its host operations) are made of both releases'
  discs, each read again of this release's where the other read it (`pe2-cli plan observe`); what the other release
  reads is held to what it declares, so the survey is proven before it is used.
- An asset whose every read is the same in both takes the other release's placement as it is. One whose read differs
  takes this release's sha1; a run of a package (a mesh, a clip, a model) is looked for in this release's package, and
  moves where it is found. What can be placed neither way - an image whose chunks differ - is left out and listed.

It writes each asset's `[<asset>.from.<release>]` after the other release's, as text (the files' comments stay), and
replaces what it wrote before.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import tomllib
import xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "buildsteps"))
import declarations as declared  # noqa: E402
import init  # noqa: E402
import plan as plans  # noqa: E402
import toml_out  # noqa: E402

DUMPSXISO = ROOT / "tools" / "linux" / "mkpsxiso" / "dumpsxiso"
# What a plan reads of the discs: the host's operations, which the survey makes of both releases.
READS = {"disc", "container", "slot", "chunk", "stored_stream", "movie_clips", "clip", "inflate", "slice"}
# The reads whose bytes are an asset's own, as a placement says.
OWN = {"slot", "chunk", "stored_stream", "clip", "slice"}
# The extensions' operations that hold the declarations they are given to the bytes they read: made of this release's
# discs with the other's declarations, what they make says whether those hold for this release too.
CHECKED = {"texture", "atlas"}


def sha1_file(path: Path) -> str:
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 22), b""):
            h.update(block)
    return h.hexdigest()


def disc_layout(cue: Path, scratch: Path) -> dict:
    """What `dumpsxiso` says of a disc: its track, identifiers, default attributes, its files in disc order and the
    dummy sectors after them. The files it dumps go with the scratch folder."""
    out = scratch / cue.stem
    out.mkdir()
    subprocess.run([str(DUMPSXISO), "-q", "-x", str(out / "files"), "-s", str(out / "layout.xml"), str(cue.absolute())],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    track = ET.parse(out / "layout.xml").getroot().find("track")
    shutil.rmtree(out / "files", ignore_errors=True)
    files = [(f.attrib["name"], f.attrib.get("type", "data")) for f in track.find("directory_tree") if f.tag == "file"]
    dummy = [int(d.attrib.get("sectors", 150)) for d in track.find("directory_tree") if d.tag == "dummy"]
    number = lambda v: int(v) if v.lstrip("-").isdigit() else v  # noqa: E731
    return {
        "track": {"type": track.attrib.get("type"), "xa_edc": track.attrib.get("xa_edc") == "true",
                  "new_type": track.attrib.get("new_type") == "true"},
        "identifiers": dict(track.find("identifiers").attrib),
        "default_attributes": {k: number(v) for k, v in track.find("default_attributes").attrib.items()},
        "files": files,
        "dummy_sectors": dummy[0] if dummy else None,
    }


def replace_line(text: str, key: str, value: str) -> str:
    """A top-level `key = ...` line set to `key = value`."""
    found = re.search(rf"^{re.escape(key)} = .*$", text, re.M)
    if found is None:
        raise SystemExit(f"no {key} to set")
    return text[:found.start()] + f"{key} = {value}" + text[found.end():]


def declare_layouts(decl, like: str, release: str, cues: list[Path], root: Path) -> dict:
    """The release's disc and stage layouts, as the other's with its own images. Returns each disc's file names, the
    other release's by this one's (`{(disc, name): name}`)."""
    taken = set(decl.by_uid)
    folder = root / "disc" / release
    folder.mkdir(parents=True, exist_ok=True)
    names: dict[tuple[str, str], str] = {}
    stage_uids = {}
    for path in sorted(p for p in decl.by_path if p.startswith(f"disc/{like}/stage")):
        stage = path.rsplit("/", 1)[1]
        uid = init.derived_uid(f"disc/{release}/{stage}", taken)
        taken.add(uid)
        stage_uids[decl.by_path[path].uid] = (uid, f"res://disc/{release}/{stage}.node", f"res://disc/{like}/{stage}.node")
        text = (root / "disc" / like / f"{stage}.toml").read_text(encoding="utf-8")
        (folder / f"{stage}.toml").write_text(replace_line(text, "uid", toml_out.string(uid)), encoding="utf-8")
    with tempfile.TemporaryDirectory(dir=ROOT / "build", prefix="release-") as scratch:
        for n, cue in enumerate(cues, 1):
            disc = f"disk{n}"
            theirs = decl.by_path[f"disc/{like}/{disc}"].table
            ours = disc_layout(cue, Path(scratch))
            for key in ("track", "default_attributes", "dummy_sectors"):
                if theirs.get(key) != ours[key]:
                    raise SystemExit(f"{cue.name}: its {key} is {ours[key]}, {like}'s {theirs.get(key)}: not the same discs")
            their_files = [(s["name"], s["type"]) for s in theirs["slots"] if s["type"] != "license"]
            if [t for _, t in their_files] != [t for _, t in ours["files"]]:
                raise SystemExit(f"{cue.name}: its files are {ours['files']}, {like}'s {their_files}")
            for (their, _), (our, _) in zip(their_files, ours["files"]):
                names[(disc, their)] = our
                if their != our and not (their.startswith("SLUS_") or their.startswith("PE_DISK")):
                    print(f"{disc}: {their} is {our}")
            image = cue.with_suffix(".bin")
            text = (root / "disc" / like / f"{disc}.toml").read_text(encoding="utf-8")
            uid = init.derived_uid(f"disc/{release}/{disc}", taken)
            taken.add(uid)
            text = replace_line(text, "uid", toml_out.string(uid))
            text = replace_line(text, "image", toml_out.value({"name": image.name, "cue": cue.name,
                                                              "size": image.stat().st_size, "sha1": sha1_file(image)}))
            identifiers = dict(theirs["identifiers"])
            identifiers.update(ours["identifiers"])
            text = replace_line(text, "identifiers", toml_out.value(identifiers))
            for (_, name), our in ((k, v) for k, v in names.items() if k[0] == disc):
                text = text.replace(f'name = "{name}"', f'name = "{our}"')
            for their_uid, (uid, path, their_path) in stage_uids.items():
                text = text.replace(f'uid = "{their_uid}", path = "{their_path}"', f'uid = "{uid}", path = "{path}"')
            (folder / f"{disc}.toml").write_text(text, encoding="utf-8")
    return names


def twin(line: dict, like: str, release: str, names: dict, discs: dict) -> dict:
    """A read of the other release, made of this one's discs: its id and what it takes renamed, a disc's file by this
    release's name."""
    def renamed(id: str) -> str:
        out = id.replace(f":{like}/", f":{release}/")
        for (disc, their), our in names.items():
            out = out.replace(f"/{disc}/{their}", f"/{disc}/{our}")
        return out
    if line["op"] == "disc":
        return discs[renamed(line["id"])]
    out = {"id": renamed(line["id"]), "op": line["op"], "args": copy.deepcopy(line.get("args", {}))}
    if line.get("in"):
        out["in"] = {port: [renamed(i) for i in ids] if isinstance(ids, list) else renamed(ids)
                     for port, ids in line["in"].items()}
    disc = out.get("in", {}).get("disc")
    if line["op"] in ("slot", "movie_clips", "clip") and disc:
        their_disc = disc.rsplit("/", 1)[1]
        name = out["args"]["name"]
        out["args"]["name"] = next((o for (d, t), o in names.items() if d == their_disc and t == name), name)
    return out


def survey_plan(decl, registry: dict, like: str, release: str, names: dict) -> tuple[plans.Plan, dict]:
    """The reads the other release's declarations make, and each again of this release's discs, held to nothing.
    Returns the plan the other release's declarations make, and the survey's lines by id."""
    made = plans.make(decl, registry, [like])
    # Every disc file an asset is placed in, read whole: what an assembled movie file is held to.
    for path, d in decl.by_path.items():
        for at in d.table.get("from", {}).get(like, {}).get("at", []):
            for name in at.get("slots", []) if "disc" in at else []:
                made.op(f"slot:{like}/{at['disc']}/{name}", "slot", {"disc": f"disc:{like}/{at['disc']}"}, {"name": name})
    discs = {}
    for path, d in decl.by_path.items():
        parts = path.split("/")
        if len(parts) == 3 and parts[0] == "disc" and parts[1] == release and d.table.get("template") == "disc":
            image = d.table["image"]
            discs[f"disc:{release}/{parts[2]}"] = {
                "id": f"disc:{release}/{parts[2]}", "op": "disc",
                "args": {"image": {"name": image["name"], "size": image["size"], "sha1": image["sha1"]},
                         "release": release, "slots": [[s["name"], s["type"]] for s in d.table.get("slots", [])]}}
    # Each chunk of every file whose chunks are declared one by one.
    for path, d in decl.by_path.items():
        for at in d.table.get("from", {}).get(like, {}).get("at", [])[:1] if d.table.get("from", {}).get(like, {}).get("chunks") else []:
            slots = [s["name"] for s in d.table.get("slots", [])]
            container = f"container:{like}/{at['stage']}"
            where = "-" if at.get("folder") is None else str(at["folder"])
            for index, _ in enumerate(slots):
                made.op(f"chunk:{like}/{at['stage']}/{where}/{at['file']}/{index}", "chunk",
                        {"disc": made.ops[container]["in"]["disc"], "container": container},
                        {"folder": at.get("folder"), "file": at["file"], "index": index})
    lines = {}
    for id, line in made.ops.items():
        checked = id.split(":", 1)[0] in CHECKED
        if line["op"] not in READS and not checked:
            continue
        if line["op"] in READS:
            lines[id] = {k: v for k, v in line.items() if k != "expect"}
        theirs = twin({k: v for k, v in line.items() if k != "expect"}, like, release, names, discs)
        if checked:
            theirs["args"] = as_release(theirs["args"], like, release, names)
        lines[theirs["id"]] = theirs
    return made, lines


def as_release(item, like: str, release: str, names: dict):
    """An extension's settings made of the other release's declarations for this one: each declaration placed where
    the other release has it (`from.<release>` as the other's), each disc's file by this release's name, and the
    release it reads this one - so that what it holds the declarations to holds them to this release's bytes."""
    if isinstance(item, list):
        return [as_release(v, like, release, names) for v in item]
    if not isinstance(item, dict):
        return item
    out = {k: as_release(v, like, release, names) for k, v in item.items()}
    if out.get("release") == like:
        out["release"] = release
    if "disc" in out and isinstance(out.get("slot"), str):
        out["slot"] = names.get((out["disc"], out["slot"]), out["slot"])
    froms = out.get("from")
    if isinstance(froms, dict) and like in froms:
        out["from"] = {**froms, release: as_release(copy.deepcopy(froms[like]), like, release, names)}
    return out


def write_survey(lines: dict, made: plans.Plan, release: str, decl, path: Path) -> None:
    inputs = {r: {"look_in": plans.LOOK_IN, "discs": discs} for r, discs in made.inputs.items()}
    inputs[release] = {"look_in": plans.LOOK_IN, "discs": [
        {"name": d.table["image"]["name"], "size": d.table["image"]["size"], "sha1": d.table["image"]["sha1"]}
        for p, d in sorted(decl.by_path.items()) if p.startswith(f"disc/{release}/disk")]}
    order = [l for l in plans.ordered(lines)]
    head = {"plan": plans.VERSION, "requires": [], "inputs": inputs}
    path.write_text("\n".join(json.dumps(l, sort_keys=True) for l in [head] + order) + "\n", encoding="utf-8")


def checks_of(made: plans.Plan, path: str) -> list[str]:
    """The extensions' operations an asset's files come from that check what they are given (`CHECKED`)."""
    roots = [r.split("#", 1)[0] for r in made.emits.get(path, {}).values()]
    roots += [id for id in made.ops if id.split(":", 1)[-1] == path]
    seen, out, stack = set(), [], roots
    while stack:
        id = stack.pop()
        if id in seen or id not in made.ops:
            continue
        seen.add(id)
        if id.split(":", 1)[0] in CHECKED:
            out.append(id)
        for ids in (made.ops[id].get("in") or {}).values():
            stack.extend(ids if isinstance(ids, list) else [ids])
    return sorted(out)


def reads_of(made: plans.Plan, path: str) -> list[str]:
    """The reads an asset's files and checks come from: what its emits and its own operations take, all the way down."""
    roots = list(made.emits.get(path, {}).values())
    roots += [id for id in made.ops if id.split(":", 1)[-1] == path]
    seen, out, stack = set(), [], [r.split("#", 1)[0] for r in roots]
    while stack:
        id = stack.pop()
        if id in seen or id not in made.ops:
            continue
        seen.add(id)
        line = made.ops[id]
        if line["op"] in OWN:
            out.append(id)
        for ids in (line.get("in") or {}).values():
            stack.extend(ids if isinstance(ids, list) else [ids])
    return sorted(out)


def placed(decl, made: plans.Plan, like: str, release: str, names: dict, seen: dict, kept: Path) -> tuple[dict, Counter, list]:
    """Each asset's placement in the release, by path; how many were placed each way; what could not be."""
    out, counts, left = {}, Counter(), []
    rename = lambda id: twin(made.ops[id], like, release, names, {})["id"]  # noqa: E731
    sha1 = lambda id: seen.get(id, {}).get("sha1")  # noqa: E731
    for path, d in sorted(decl.by_path.items()):
        theirs = d.table.get("from", {}).get(like)
        if not theirs or (path.startswith("disc/") and path.count("/") == 2):
            continue
        ours = as_release(copy.deepcopy(theirs), like, release, names)
        for at in ours.get("at", []):
            if "disc" in at and "slots" in at:
                at["slots"] = [names.get((at["disc"], s), s) for s in at["slots"]]
        reads = reads_of(made, path)
        if not reads and "sha1" in theirs:
            # A movie file, held to its sha1 whole: its slots read whole.
            reads = [f"slot:{like}/{at['disc']}/{s}" for at in theirs.get("at", []) for s in at.get("slots", [])]
        if any(sha1(r) is None for r in reads):
            left.append((path, "the other release's reads failed"))
            continue
        how = None
        if all(sha1(r) == sha1(rename(r)) for r in reads):
            how = "the same"
        checks = checks_of(made, path)
        if how is None and checks:
            # What the extension holds the declarations to: made of this release's bytes, they held.
            failed = [seen.get(rename(c), {}).get("error", "not made") for c in checks
                      if rename(c) not in seen or "error" in seen[rename(c)]]
            if failed:
                left.append((path, failed[0].split("\n")[0][:160]))
                continue
            how = "checked"
        if how is None and "offset" in theirs:
            rebased = rebase(theirs, reads, made, seen, rename, kept)
            if rebased is None:
                left.append((path, "its run is not the other's rebased"))
                continue
            ours["address"], ours["sha1"] = rebased
            how = "rebased"
        if how is None:
            sha1s = {sha1(rename(r)) for r in reads}
            if reads and all(made.ops[r]["op"] in ("slot", "chunk", "stored_stream", "clip") for r in reads) \
                    and len(sha1s) == 1 and None not in sha1s and "sha1" in theirs and "palettes" not in theirs:
                ours["sha1"] = sha1s.pop()
                how = "read anew"
        if how is None and not reads and "sha1" not in theirs:
            how = "the same"
        if how is None:
            left.append((path, "what it is made of differs"))
            continue
        if "chunks" in theirs:
            # A file's chunks, each its own: this release's.
            at = theirs["at"][0]
            slots = [s["name"] for s in d.table.get("slots", [])]
            where = "-" if at.get("folder") is None else str(at["folder"])
            chunks = {}
            for slot, declared_sha1 in theirs["chunks"].items():
                id = f"chunk:{like}/{at['stage']}/{where}/{at['file']}/{slots.index(slot)}"
                if sha1(id) != declared_sha1:
                    raise SystemExit(f"{path}: {slot}: {like} declares {declared_sha1}, the survey read {sha1(id)}")
                chunks[slot] = sha1(rename(id))
            if chunks != theirs["chunks"] and how == "the same":
                how = "read anew"
            ours["chunks"] = chunks
        counts[how] += 1
        out[path] = ours
    return out, counts, left


def rebase(theirs: dict, reads: list, made: plans.Plan, seen: dict, rename, kept: Path):
    """A package's run as this release has it, where its package is loaded elsewhere: at the same offset, every word
    the same but its pointers, each moved by one distance - the packages' load addresses apart. Its address and sha1
    there; none where it is otherwise."""
    # Its own run: of its package, at its offset - not another's it is read with (a model's mesh).
    at = theirs["at"][0]
    where = "-" if at.get("folder") is None else str(at["folder"])
    place = f"/{at['stage']}/{where}/{at['file']}/"
    run = next((r for r in reads if made.ops[r]["op"] == "slice" and place in r
                and r.endswith(f"@{theirs['offset']}+{theirs['size']}")), None)
    if run is None or seen.get(rename(run), {}).get("sha1") is None:
        return None
    a = (kept / seen[run]["sha1"]).read_bytes()
    b = (kept / seen[rename(run)]["sha1"]).read_bytes()
    if len(a) != len(b) or len(a) % 4:
        return None
    count = len(a) // 4
    pointer = lambda w: 0x80000000 <= w < 0x80800000  # noqa: E731
    apart = {(y - x) & 0xFFFFFFFF for x, y in zip(struct.unpack(f"<{count}I", a), struct.unpack(f"<{count}I", b))
             if x != y and pointer(x) and pointer(y)}
    others = any(x != y and not (pointer(x) and pointer(y))
                 for x, y in zip(struct.unpack(f"<{count}I", a), struct.unpack(f"<{count}I", b)))
    if others or len(apart) != 1:
        return None
    distance = apart.pop()
    return (theirs["address"] + distance) & 0xFFFFFFFF, seen[rename(run)]["sha1"]


BLOCK = re.compile(r"^\[(?P<key>.+)\.from\.(?P<release>[A-Za-z0-9_]+)\][ \t]*(?:#.*)?$")


def write_placements(root: Path, like: str, release: str, ours: dict) -> int:
    """Each placement written after the other release's, as a block of the same shape; the release's blocks written
    before removed first. How many were written."""
    written = 0
    for file in sorted(root.rglob("*.toml")):
        group = file.relative_to(root).with_suffix("").as_posix()
        if group.startswith("disc/"):
            continue
        lines = file.read_text(encoding="utf-8").split("\n")
        out, at = [], 0
        while at < len(lines):
            found = BLOCK.match(lines[at])
            end = at + 1
            if found:
                while end < len(lines) and lines[end].strip() and not lines[end].startswith("["):
                    end += 1
            if found and found["release"] == release:
                at = end + (1 if end < len(lines) and not lines[end].strip() else 0)
                continue
            out.extend(lines[at:end])
            if found and found["release"] == like:
                key = found["key"].strip('"')
                placed = ours.get(f"{group}/{key}")
                if placed is not None:
                    out.append("")
                    out.extend(block(lines[at:end], found["key"], release, placed))
                    written += 1
            at = end
        text = "\n".join(out)
        if text != file.read_text(encoding="utf-8"):
            file.write_text(text, encoding="utf-8")
    return written


def block(theirs: list[str], key: str, release: str, ours: dict) -> list[str]:
    """This release's block, as the other's text: its header, and each value that differs written anew in the other's
    line, the rest as they are."""
    head = f"[{key}.from.{release}]"
    text = "\n".join(theirs[1:])
    table = tomllib.loads(text)
    if set(table) != set(ours):
        raise SystemExit(f"{key}: the block's keys {sorted(table)} are not the placement's {sorted(ours)}")
    # Its statements, each the lines a key's value takes.
    statements, pending = [], []
    for line in theirs[1:]:
        pending.append(line)
        try:
            tomllib.loads("\n".join(pending))
        except tomllib.TOMLDecodeError:
            continue
        statements.append(pending)
        pending = []
    if pending:
        raise SystemExit(f"{key}: a value left open")
    out = [head]
    for statement in statements:
        value = tomllib.loads("\n".join(statement))
        name = next(iter(value), None)
        if name in ours and value[name] != ours[name]:
            out.append(f"{name} = " + inline(ours[name]))
        else:
            out.extend(statement)
    return out


def inline(item) -> str:
    """A value on one line, as the declarations write placements."""
    if isinstance(item, dict):
        return "{ " + ", ".join(f"{k} = {inline(v)}" for k, v in item.items()) + " }"
    if isinstance(item, list):
        return "[" + ", ".join(inline(v) for v in item) + "]"
    return toml_out.value(item)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)
    d = sub.add_parser("declare")
    d.add_argument("release")
    d.add_argument("--like", required=True)
    d.add_argument("--images", type=Path, nargs="+", required=True, help="the release's discs' cue sheets, in order")
    d.add_argument("--registry", type=Path, required=True, help="what a project's extensions offer (pe2-cli project registry)")
    d.add_argument("--cli", type=Path, required=True, help="pe2-cli")
    d.add_argument("--declarations", type=Path, default=declared.DECLARATIONS)
    args = ap.parse_args()
    root = args.declarations
    registry = json.loads(args.registry.read_text(encoding="utf-8"))
    names = declare_layouts(declared.Declarations(root), args.like, args.release, args.images, root)
    decl = declared.Declarations(root)
    made, lines = survey_plan(decl, registry, args.like, args.release, names)
    with tempfile.TemporaryDirectory(dir=ROOT / "build", prefix="release-") as scratch:
        scratch = Path(scratch)
        survey = scratch / "survey.plan.jsonl"
        write_survey(lines, made, args.release, decl, survey)
        roms = args.images[0].parent
        observed = subprocess.run([str(args.cli), "plan", "observe", str(survey), "--roms", str(roms),
                                   "--keep", str(scratch / "kept"), "--keep-kinds", "inflate,slice"],
                                  check=True, capture_output=True, text=True).stdout
        seen = {o["id"]: o for o in map(json.loads, filter(None, observed.split("\n")))}
        wrong = [(id, line["expect"]["sha1"], seen.get(id, {}).get("sha1") or seen.get(id, {}).get("error"))
                 for id, line in made.ops.items() if line["op"] in OWN and line.get("expect")
                 and seen.get(id, {}).get("sha1") != line["expect"]["sha1"]]
        if wrong:
            for id, want, got in wrong[:20]:
                print(f"{id}: {args.like} declares {want}, the survey read {got}")
            raise SystemExit(f"{len(wrong)} of {args.like}'s reads are not what it declares: the survey is not proven")
        ours, counts, left = placed(decl, made, args.like, args.release, names, seen, scratch / "kept")
    written = write_placements(root, args.like, args.release, ours)
    print(f"{args.release}: {written} assets placed - {counts['the same']} as {args.like}, {counts['checked']} held by "
          f"what reads them, {counts['read anew']} read anew, {counts['rebased']} runs rebased; {len(left)} left out")
    for path, why in left:
        print(f"  {path}: {why}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
