"""Declare the rooms' dialogues: each a node of its own, its text in its stage's string table, its glyphs in the dialogue
faces.

    dialogue.py declare --registry REGISTRY.json --cli PE2-CLI --roms DIR [--release USA]

A dialogue is a data resource of a room's bundle (`pe2.game@bundle`), a CAP file (doc/CAP_FORMAT.md): the `pe2.dialogue`
extension's `dialogue` node, which makes it again of its structure, its text and its faces. Each is declared once,
whatever bundles hold it (the night rooms of Dryfield hold their day rooms', the Akropolis rooms one story call): at
the room holding it, `<room>/dialogue_<i>` by its place among the bundle's data resources (`capSelectLoadedFile`);
where several rooms hold it, at what they are in (`stages/shared` across stages), named after the first; under the name the asset table gave it where
it gave one (`stages/akropolis/story_calls`). Every bundle holding it names it.

Its atlas page is the image its room's file uploads (`<room>/dialogue_<i>_atlas`), found by the sha1 the asset table
records of the atlas the dialogue generates; where two dialogues share an image, each lays out a column of it. The
choices the asset table makes for cells whose picture several characters share go with the dialogue's reading
(`from.<release>.glyphs`).

Written: each dialogue in its room's group file (its `dialogue_<i>` sections replaced), each stage's string table
(`stages/<stage>/strings.toml`, `dialogue`), the faces and the cast (`ui/dialogue.toml`, the cast from
`tools/peassets/cap_plates.tsv`), and each bundle's references. Rerunnable: uids follow from paths.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
import tomllib
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "buildsteps"))
sys.path.insert(0, str(HERE.parent / "peassets"))
import bundle as bundles  # noqa: E402
import declarations as declared  # noqa: E402
import init  # noqa: E402
import plan as plans  # noqa: E402
import toml_out  # noqa: E402

PLATES = HERE.parent / "peassets" / "cap_plates.tsv"
UI = "ui/dialogue"
FACES = {"glyphs": ("Parasite Eve II Dialogue", "Regular"), "bold": ("Parasite Eve II Dialogue", "Bold"),
         "speakers": ("Parasite Eve II Speakers", "Regular")}
HEADER = "# Declarations: {what} (tools/editor_project/dialogue.py).\n"
SECTION = re.compile(r"^\[\[?([^\].]+)")


def uid_for(kind: str, path: str) -> str:
    return init.uid_text(int.from_bytes(hashlib.sha1(f"{kind}:{path}".encode()).digest()[:8], "big") >> 1)


def ref(path: str, kind: str) -> dict:
    return {"uid": uid_for(kind, path), "path": f"res://{path}.node"}


def read_bundles(decl, registry: dict, release: str, cli: Path, roms: Path, scratch: Path) -> dict[str, bytes]:
    """Each bundle node's chunk, read through the plan its declarations make - without the dialogues, which this
    declares."""
    import types
    without = types.SimpleNamespace(by_path={p: d for p, d in decl.by_path.items() if d.table.get("template") != "dialogue"})
    made = plans.make(without, registry, [release])
    containers = {(release, int(id.rsplit("/", 1)[-1])): (line["in"]["disc"], id)
                  for id, line in made.ops.items() if line["op"] == "container"}
    places = {}
    for path, d in decl.by_path.items():
        for at in d.table.get("from", {}).get(release, {}).get("at", []):
            if "disc" not in at and "slot" not in at and "stage" in at and "file" in at:
                places[(release, at["stage"], at.get("folder"), at["file"])] = path
    reads = {}
    for path, d in sorted(decl.by_path.items()):
        if d.table.get("template") == "bundle" and d.table.get("from", {}).get(release):
            at = d.table["from"][release]["at"][0]
            reads[path] = plans.chunk(made, release, at, decl.by_path, places, containers)
    wanted = set(reads.values())
    lines = {id: line for id, line in made.ops.items() if line["op"] in ("disc", "container") or id in wanted}
    head = {"plan": plans.VERSION, "requires": [], "releases": [release],
            "inputs": {release: {"look_in": plans.LOOK_IN, "discs": made.inputs[release]}}}
    survey = scratch / "bundles.plan.jsonl"
    survey.write_text("\n".join(json.dumps(line, sort_keys=True) for line in [head] + plans.ordered(lines)) + "\n",
                      encoding="utf-8")
    kept = scratch / "kept"
    observed = subprocess.run([str(cli), "plan", "observe", str(survey), "--roms", str(roms), "--keep", str(kept),
                               "--keep-kinds", "chunk"], check=True, capture_output=True, text=True).stdout
    seen = {o["id"]: o for o in map(json.loads, filter(None, observed.split("\n")))}
    out = {}
    for path, id in reads.items():
        o = seen.get(id, {})
        if not o.get("sha1"):
            raise SystemExit(f"{path}: its bundle is not read ({o.get('error')})")
        out[path] = (kept / o["sha1"]).read_bytes()
    return out


def room_of(bundle: str) -> str:
    """The room a bundle node is of: what holds its file group (`<room>/files/bundle`)."""
    return bundle.rsplit("/", 2)[0]


def common(paths: list[str]) -> str:
    parts = [p.split("/") for p in paths]
    out = []
    for steps in zip(*parts):
        if len(set(steps)) != 1:
            break
        out.append(steps[0])
    found = "/".join(out)
    return found.removesuffix("/rooms")


def asset_names() -> tuple[dict, dict]:
    """The asset table's dialogues by sha1 - their names, the glyph choices, and the atlas each generates (its sha1 and
    column)."""
    import asset_data
    assets = asset_data.ASSETS
    generated = {}
    for name, record in assets.items():
        if record.get("type") == "font_atlas":
            by = record.get("generated_by")
            for column, dialogue in enumerate(by if isinstance(by, list) else [by]):
                generated[dialogue] = (record["sha1"], column)
    caps = {}
    for name, record in assets.items():
        if record.get("type") == "cap":
            caps[record["sha1"]] = {"name": name, "glyphs": record.get("glyphs", {}), "atlas": generated.get(name)}
    return caps, assets


def atlas_images(decl, release: str) -> dict[str, str]:
    """Each image node a file's slot uploads, by the sha1 of that slot's chunk."""
    files = {}
    for path, d in decl.by_path.items():
        placed = d.table.get("from", {}).get(release, {})
        if d.table.get("template") == "file":
            for at in placed.get("at", []):
                for slot, sha1 in placed.get("chunks", {}).items():
                    files[(at.get("stage"), at.get("folder"), at.get("file"), slot)] = sha1
    out = {}
    for path, d in decl.by_path.items():
        for at in d.table.get("from", {}).get(release, {}).get("at", []):
            key = (at.get("stage"), at.get("folder"), at.get("file"), at.get("slot"))
            if key in files and d.table.get("template") == "texture":
                out.setdefault(files[key], path)
    return out


def section_text(name: str, fields: dict, placed: dict) -> str:
    lines = [f"[{name}]"] + [f"{toml_out.key(k)} = {toml_out.value(v)}" for k, v in fields.items()]
    out = "\n".join(lines)
    for release, where in placed.items():
        body = [f"at = [{toml_out.value(where['at'][0])}]"] + [f"{toml_out.key(k)} = {toml_out.value(v)}"
                                                               for k, v in where.items() if k != "at"]
        out += f"\n\n[{name}.from.{release}]\n" + "\n".join(body)
    return out + "\n"


def replace_sections(file: Path, keep_header: str, names: set[str] | re.Pattern, sections: list[str]) -> None:
    """A group file with its sections of those names (and their subtables) replaced by `sections`, at its end."""
    text = file.read_text(encoding="utf-8") if file.exists() else keep_header
    out, skipping = [], False
    for line in text.split("\n"):
        found = SECTION.match(line)
        if found:
            name = found[1]
            skipping = bool(names.match(name)) if isinstance(names, re.Pattern) else name in names
        if not skipping:
            out.append(line)
    body = "\n".join(out).rstrip("\n") + "\n"
    for section in sections:
        body += "\n" + section
    tomllib.loads(body)
    file.parent.mkdir(parents=True, exist_ok=True)
    file.write_text(body, encoding="utf-8")


def declare(root: Path, decl, chunks: dict[str, bytes], release: str) -> None:
    caps, _assets = asset_names()
    images = atlas_images(decl, release)
    faces = {face: ref(f"{UI}/{face}", "font") for face in FACES}
    cast = ref(f"{UI}/cast", "cast")
    # Each dialogue once, with every bundle holding it.
    held: dict[str, list[tuple[str, int, int]]] = {}
    for path in sorted(chunks):
        data_index = 0
        for meta, payload in bundles.resources(chunks[path]):
            if meta["kind"] != 3:
                continue
            if payload[:4] == b"CAP2":
                held.setdefault(hashlib.sha1(payload).hexdigest(), []).append((path, meta["slot"], data_index))
            data_index += 1
    named: dict[str, str] = {}
    by_group: dict[Path, list[str]] = {}
    tables: dict[str, str] = {}
    references: dict[tuple[str, int], str] = {}
    for sha1, users in sorted(held.items(), key=lambda kv: kv[1][0]):
        record = caps.get(sha1, {})
        first_bundle, first_slot, first_index = users[0]
        rooms = sorted({room_of(b) for b, _, _ in users})
        if record.get("name", "").startswith("stages/"):
            path = record["name"]
        elif len(rooms) == 1:
            path = f"{rooms[0]}/dialogue_{first_index}"
        elif "/rooms/" in common(rooms):
            # A room's, held by its day and night versions.
            path = f"{common(rooms)}/dialogue_{first_index}"
        else:
            # A stage's, held by several of its rooms - or held across stages, the stages' shared one.
            held_in = common(rooms)
            held_in = "stages/shared" if held_in == "stages" else held_in
            room = room_of(first_bundle).split("/rooms/", 1)[1].replace("/", "_")
            path = f"{held_in}/{room}_dialogue_{first_index}"
        if path in named.values():
            raise SystemExit(f"{path}: named twice")
        named[sha1] = path
        for bundle, slot, _ in users:
            references[(bundle, slot)] = path
        stage = path.split("/")[1]
        table = f"stages/{stage}/strings/dialogue"
        prefix = path.split("/", 2)[2].removeprefix("rooms/")
        fields = {"uid": uid_for("dialogue", path), "template": "dialogue", "strings": ref(table, "strings"),
                  "cast": cast, "faces": faces}
        atlas = record.get("atlas")
        if atlas and atlas[0] in images:
            image = images[atlas[0]]
            fields["page"] = {"uid": decl.by_path[image].uid, "path": f"res://{image}.node"}
            if atlas[1]:
                fields["column"] = atlas[1]
        elif atlas:
            raise SystemExit(f"{path}: its atlas ({atlas[0]}) is uploaded by no file declared")
        at = dict(decl.by_path[first_bundle].table["from"][release]["at"][0], resource=first_slot)
        placed = {"at": [at]}
        if record.get("glyphs"):
            placed["glyphs"] = {str(k): v for k, v in sorted(record["glyphs"].items())}
        placed["prefix"] = prefix
        group, name = path.rsplit("/", 1)
        by_group.setdefault(root / f"{group}.toml", []).append(section_text(name, fields, {release: placed}))
        tables[table] = stage
    for file, sections in by_group.items():
        replace_sections(file, HEADER.format(what="the rooms' dialogues"), re.compile(r"(\w+_)?dialogue_\d+$|story_calls$"),
                         sections)
    for table, stage in tables.items():
        group, name = table.rsplit("/", 1)
        section = section_text(name, {"uid": uid_for("strings", table), "template": "string_table"}, {})
        replace_sections(root / f"{group}.toml", HEADER.format(what=f"the text of {stage}'s dialogues, by key"),
                         {name}, [section])
    # The faces and the cast.
    speakers = {}
    for line in PLATES.read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            fields = line.split("\t")
            speakers[fields[1]] = [int(n) for n in fields[2].split(",")] if len(fields) > 2 and fields[2] else []
    sections = [section_text(face, {"uid": faces[face]["uid"], "template": "font", "family": family, "style": style}, {})
                for face, (family, style) in FACES.items()]
    sections.append(section_text("cast", {"uid": cast["uid"], "template": "cast", "speakers": speakers}, {}))
    replace_sections(root / f"{UI}.toml", HEADER.format(what="the dialogues' faces and cast"),
                     set(FACES) | {"cast"}, sections)
    # Each bundle names the dialogues it holds.
    for bundle in sorted({b for b, _ in references}):
        d = decl.by_path[bundle]
        file = root / f"{bundle.rsplit('/', 1)[0]}.toml"
        text = file.read_text(encoding="utf-8")
        for resource in d.table["resources"]:
            if resource["type"] != "dialogue":
                continue
            path = references[(bundle, resource["index"])]
            old = resource["dialogue"]
            new = ref(path, "dialogue")
            text = text.replace(f'dialogue = {{ uid = "{old["uid"]}", path = "{old["path"]}" }}',
                                f'dialogue = {{ uid = "{new["uid"]}", path = "{new["path"]}" }}')
        tomllib.loads(text)
        file.write_text(text, encoding="utf-8")
    print(f"{len(named)} dialogues declared, of {sum(len(u) for u in held.values())} resources in {len(chunks)} bundles; "
          f"{len(tables)} string tables")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)
    d = sub.add_parser("declare")
    d.add_argument("--registry", type=Path, required=True, help="what a project's extensions offer (pe2-cli project registry)")
    d.add_argument("--cli", type=Path, required=True, help="pe2-cli")
    d.add_argument("--roms", type=Path, required=True, help="the folder holding the release's discs")
    d.add_argument("--release", default="USA")
    d.add_argument("--declarations", type=Path, default=declared.DECLARATIONS)
    args = ap.parse_args()
    registry = json.loads(args.registry.read_text(encoding="utf-8"))
    decl = declared.Declarations(args.declarations)
    (ROOT / "build").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=ROOT / "build", prefix="dialogue-") as scratch:
        chunks = read_bundles(decl, registry, args.release, args.cli, args.roms, Path(scratch))
    declare(args.declarations, decl, chunks, args.release)
    return 0


if __name__ == "__main__":
    os.environ.setdefault("PYTHONHASHSEED", "0")
    sys.exit(main())
