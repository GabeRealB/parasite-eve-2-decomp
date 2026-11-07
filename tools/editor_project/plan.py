"""A game layer's extraction plan, from the declarations: what the editor runs to fill the layer's bundles from the
discs (`pe2-cli project extract`), and checks them by without the discs (`pe2-cli project verify`).

One JSON line each: first what the plan is, then its operations - each by an id naming what it reads, taking what
operations before it made - then what each bundle is given. It holds where things are on the discs, how large and
their sha1s, never their bytes. An asset no extractor makes is read off every place its declaration gives - a disc's
slot, a file's chunk, a stream stored in a stage, a clip of a disc's movie file - each held to the declared sha1,
and its bundle given the first release's first as `native.<format>`. (What extractors make joins the plan as their
operations are written.)
"""
from __future__ import annotations

import json

import nodes
import toml_out

VERSION = 1
# What extractors made, now read as any asset is: a movie is its clip or its stored stream.
READ = frozenset({"pe2.movies@movie"})
# What render's operations make of a run of a package: a mesh, a clip, and a model, checked.
MESH, CLIP, MODEL = "pe2.render@mesh", "pe2.render@clip", "pe2.render@model"
# A texture: its texels and colors from the chunks it reads, and its source written from them.
IMAGE = "pe2.images@image"
# An atlas and the faces it holds: one read of its pages and their tables makes all of their files.
ATLAS, FACE = "pe2.fonts@font_atlas", "pe2.fonts@font"
PLANNED = READ | {MESH, CLIP, MODEL, IMAGE, ATLAS, FACE}


def target(reference) -> str | None:
    """The path a reference names: a `slot` beside what it places, or the reference alone."""
    for _ in range(2):
        if isinstance(reference, dict) and "slot" in reference:
            reference = reference["slot"]
    path = reference.get("path") if isinstance(reference, dict) else None
    if not isinstance(path, str) or not path.startswith("res://") or not path.endswith(".node"):
        return None
    return path[len("res://"):-len(".node")]


def references(item):
    """Every path a table names, anywhere in it."""
    if isinstance(item, dict):
        if isinstance(item.get("path"), str):
            found = target(item)
            if found:
                yield found
        for value in item.values():
            yield from references(value)
    elif isinstance(item, list):
        for value in item:
            yield from references(value)


def declared(table: dict) -> dict:
    """A declaration as an operation is given it: without where it is on the discs."""
    return {k: v for k, v in table.items() if k != "from"}


# Where a plan's discs are looked for: the project's unmanaged mount of them.
LOOK_IN = "res://roms"


class Plan:
    def __init__(self, releases: list[str], extensions: dict[str, str] | None = None) -> None:
        self.releases = releases
        # Each release's discs, in disc order, by name, size and sha1: what the plan reads.
        self.inputs: dict[str, list[dict]] = {release: [] for release in releases}
        # Each extension's version as the registry says it (`0.1.0 <build>`): what the plan requires of those it runs.
        self.extensions = extensions or {}
        self.ops: dict[str, dict] = {}
        self.emits: dict[str, dict[str, str]] = {}
        self.checks: set[str] = set()
        self.optional: dict[str, set[str]] = {}

    def op(self, id: str, op: str, takes: dict | None = None, args: dict | None = None, sha1: str | None = None,
           after: list[str] | None = None) -> str:
        line = {"id": id, "op": op}
        if takes:
            line["in"] = takes
        if after:
            line["after"] = after
        if args is not None:
            line["args"] = args
        if sha1:
            line["expect"] = {"sha1": sha1}
        known = self.ops.get(id)
        if known is not None and known != line:
            # Held to a sha1 by one reader and not another: one read, held to it.
            same = {k: v for k, v in known.items() if k != "expect"} == {k: v for k, v in line.items() if k != "expect"}
            if not same or ("expect" in known and "expect" in line):
                raise ValueError(f"{id} is read as two things: {known} and {line}")
            line = known if "expect" in known else line
        self.ops[id] = line
        return id

    def requires(self) -> list[dict]:
        """The extensions whose operations it runs, each at the versions compatible with the one it was made by
        (`^<major>.<minor>`)."""
        used = sorted({line["op"].split("@")[0] for line in self.ops.values() if "@" in line["op"]})
        if any(line["op"] == "source" for line in self.ops.values()):
            used = sorted(set(used) | {"pe2.images"})
        out = []
        for id in used:
            version = self.extensions.get(id, "").split(" ")[0]
            parts = version.split(".")
            if len(parts) < 2:
                raise ValueError(f"the registry gives no version of {id}, whose operations the plan runs")
            out.append({"id": id, "version": f"^{parts[0]}.{parts[1]}"})
        return out

    def check(self, id: str) -> None:
        self.checks.add(id)

    def emit(self, node: str, file: str, id: str | list[str], optional: bool = False) -> None:
        """A file of a bundle, made by an operation - or by any of several, in preference order, making the same bytes
        of different releases."""
        if isinstance(id, list):
            id = id[0] if len(id) == 1 else id
        self.emits.setdefault(node, {})[file] = id
        if optional:
            self.optional.setdefault(node, set()).add(file)

    def text(self) -> str:
        """The plan's lines: what it is, its operations - discs, containers, then reads, each in id order - and the
        bundles, by path."""
        inputs = {release: {"look_in": LOOK_IN, "discs": discs} for release, discs in self.inputs.items()}
        head = {"plan": VERSION, "requires": self.requires(), "releases": list(self.inputs), "inputs": inputs}
        emits = []
        for node, files in sorted(self.emits.items()):
            line = {"emit": node, "files": files}
            if node in self.optional:
                line["optional"] = sorted(self.optional[node])
            emits.append(line)
        lines = [head] + ordered(self.ops) + emits
        lines += [{"check": id} for id in sorted(self.checks)]
        return "".join(json.dumps(line, sort_keys=True, separators=(",", ":")) + "\n" for line in lines)


def chunk(plan: Plan, release: str, at: dict, by_path: dict, places: dict, containers: dict, sha1=None) -> str:
    """A file's chunk, by its slot's place among the slots its file asset declares."""
    disc, container = containers[(release, at["stage"])]
    folder = at.get("folder")
    file_asset = places[(release, at["stage"], folder, at["file"])]
    slots = [s["name"] for s in by_path[file_asset].table.get("slots", [])]
    index = slots.index(at["slot"])
    where = "-" if folder is None else str(folder)
    id = f"chunk:{release}/{at['stage']}/{where}/{at['file']}/{index}"
    args = {"folder": folder, "file": at["file"], "index": index}
    return plan.op(id, "chunk", {"disc": disc, "container": container}, args, sha1)


def stored(plan: Plan, path: str, placed: dict, release: str, by_path: dict, places: dict, containers: dict) -> str:
    """A run of the package a declaration names, inflated, held to its sha1."""
    at = placed["at"][0]
    package = chunk(plan, release, at, by_path, places, containers)
    inflated = plan.op(f"inflate:{package}", "inflate", {"bytes": package})
    offset, size = placed["offset"], placed["size"]
    if "sha1" not in placed:
        raise ValueError(f"{path}: {release} has no sha1")
    return plan.op(f"slice:{inflated}@{offset}+{size}", "slice", {"bytes": inflated}, {"offset": offset, "size": size}, placed["sha1"])


def joints_of(table: dict, by_path: dict) -> list | None:
    named = target(table.get("skeleton")) if table.get("skeleton") else None
    return by_path[named].table.get("joints", []) if named else None


def run_groups(table: dict, releases) -> tuple[list[str], list[list[str]]]:
    """The releases declaring a package's run, and them grouped by what they declare it: its bytes, and where it is
    loaded - the same bytes loaded elsewhere being another mesh, its pointers resolved there."""
    declaring = [r for r in releases if table.get("from", {}).get(r)]
    groups = by_what_they_declare({r: (table["from"][r].get("sha1"), table["from"][r].get("address"))
                                   for r in declaring})
    return declaring, groups


def run_op(kind: str, path: str, table: dict, release: str, releases) -> str:
    """The operation making a mesh, a clip or a model of a release: the first group's plain, another's by its first
    release (`<kind>:<release>:<path>`)."""
    _, groups = run_groups(table, releases)
    for n, group in enumerate(groups):
        if release in group:
            return f"{kind}:{path}" if n == 0 else f"{kind}:{group[0]}:{path}"
    raise KeyError(f"{path}: {release} declares no run")


def rendered(plan: Plan, path: str, table: dict, node_type: str, by_path: dict, releases, places, containers) -> None:
    """A mesh, a clip or a model: each release's run of its package held to its sha1; made once for each group of
    releases declaring it alike, of any of their runs."""
    declaring, groups = run_groups(table, releases)
    if not declaring:
        return
    runs = {r: stored(plan, path, table["from"][r], r, by_path, places, containers) for r in declaring}
    made_by = {}
    for group in groups:
        run, placed = any_of([runs[r] for r in group]), table["from"][group[0]]
        id = run_op(node_type_kind(node_type), path, table, group[0], releases)
        for release in group:
            made_by[release] = id
        if node_type == MESH:
            args = {"address": placed["address"], "material_slots": placed.get("material_slots", [])}
            joints = joints_of(table, by_path)
            if joints is not None:
                args["joints"] = joints
            plan.op(id, "pe2.render@extract_mesh", {"stored": run}, args)
        elif node_type == CLIP:
            args = {"address": placed["address"], "joints": joints_of(table, by_path)}
            plan.op(id, "pe2.render@extract_clip", {"stored": run}, args)
        else:
            mesh_path = target(table["mesh"])
            mesh_table = by_path[mesh_path].table
            materials = {slot: target(ref) for slot, ref in table.get("materials", {}).items()}
            material_tables = {m: declared(by_path[m].table) for m in materials.values() if m}
            images = {i: declared(by_path[i].table) for t in material_tables.values() for i in references(t)
                      if by_path.get(i) and by_path[i].table.get("template") in ("texture", "background", "font_texture")}
            slots = {v: declared(by_path[v].table) for t in images.values() for v in references(t)
                     if by_path.get(v) and by_path[v].table.get("template") == "vram_slot"}
            args = {"address": placed["address"], "model": {"materials": table.get("materials", {})},
                    "materials": material_tables, "images": images, "vram_slots": slots}
            joints = joints_of(mesh_table, by_path)
            if joints is not None:
                args["joints"] = joints
            takes = {"stored": run, "mesh": run_op("mesh", mesh_path, mesh_table, group[0], releases)}
            plan.check(plan.op(id, "pe2.render@check_model", takes, args))
    if node_type == MESH:
        emit_by_release(plan, path, "mesh.usda", groups, made_by)
    elif node_type == CLIP:
        emit_by_release(plan, path, "clip.usda", groups, made_by)


def node_type_kind(node_type: str) -> str:
    return {MESH: "mesh", CLIP: "clip"}.get(node_type, "model")


def ordered(ops: dict[str, dict]) -> list[dict]:
    """The operations, each after what it takes, in id order where nothing says otherwise."""
    done: set[str] = set()
    out: list[dict] = []
    def place(id: str) -> None:
        if id in done:
            return
        done.add(id)
        for taken in ops[id].get("in", {}).values():
            for t in taken if isinstance(taken, list) else [taken]:
                for one in t["any"] if isinstance(t, dict) else [t]:
                    place(one)
        for t in ops[id].get("after", []):
            place(t)
        out.append(ops[id])
    for id in sorted(ops):
        place(id)
    return out


def any_of(ids: list[str]):
    """What an operation takes at a place: one operation, or any of several making the same bytes of different
    releases, in preference order."""
    unique = list(dict.fromkeys(ids))
    return unique[0] if len(unique) == 1 else {"any": unique}


def by_what_they_declare(made: dict) -> list[list[str]]:
    """The releases grouped by what they declare an asset to be (`made`: release -> what it declares, in release
    order; none being unlike any other), in release order."""
    groups: list[tuple[object, list[str]]] = []
    for release, declared in made.items():
        group = next((g for key, g in groups if declared is not None and key == declared), None)
        if group is None:
            groups.append((declared, [release]))
        else:
            group.append(release)
    return [g for _, g in groups]


def emit_by_release(plan: Plan, path: str, name: str, groups: list[list[str]], made_by: dict,
                    optional: bool = False) -> None:
    """A file each group of releases makes alike (`by_what_they_declare`, `made_by`: release -> its operation): the
    first group's the bundle's file, made by any of its releases; each other release's its own, `<release>/<name>`,
    made by it first, else by the others of its group."""
    for n, group in enumerate(groups):
        ids = [made_by[r] for r in group]
        if n == 0:
            plan.emit(path, name, ids, optional)
            continue
        for release in group:
            own = made_by[release]
            plan.emit(path, f"{release}/{name}", [own] + [i for i in ids if i != own], optional)


def palette_sources(by_path: dict) -> tuple[dict, dict, dict]:
    """Where each image palette's colors are, by image and palette name; where each named run no image owns is, by VRAM
    slot and place - the first file in path order that uploads it, its slots before their resources - and each file
    by where it is (the last there)."""
    named: dict[tuple, dict] = {}
    runs: dict[tuple, dict] = {}
    files_at: dict[tuple, str] = {}
    for path, d in by_path.items():
        table = d.table
        if table.get("template") != "file":
            continue
        def note(slot: str, resource, in_pages: bool, entries) -> None:
            for e in entries:
                source = {"file": path, "slot": slot, "in_pages": in_pages}
                if resource is not None:
                    source["resource"] = resource
                part = e.get("palette")
                output = part.get("output") if isinstance(part, dict) else None
                if part and isinstance(output, str) and output.startswith("palettes/"):
                    named.setdefault((target(part), output[len("palettes/"):]), source)
                elif part is None and e.get("slot") and e.get("at") is not None:
                    runs.setdefault((target(e["slot"]), tuple(e["at"])), source)
        for slot in table.get("slots", []):
            note(slot["name"], None, bool(slot.get("pages")), slot.get("palettes", []))
            for r in slot.get("resources", []):
                note(slot["name"], r["index"], False, r.get("palettes", []))
        for release, placed in table.get("from", {}).items():
            for at in placed.get("at", []):
                if "stage" in at and "file" in at:
                    files_at[(release, at["stage"], at.get("folder"), at["file"])] = path
    return named, runs, files_at


def place(at: dict, slot: str | None = None) -> dict:
    """A chunk's place, as read: its stage, folder, file and slot - or a disc's slot."""
    out = {k: at[k] for k in ("disc", "stage", "folder", "file") if k in at}
    out["slot"] = slot if slot is not None else at["slot"]
    return out


def read_at(plan: Plan, release: str, at: dict, by_path: dict, places: dict, containers: dict, discs: dict) -> str:
    """What a place holds: a disc's slot, or a file's chunk."""
    if "disc" in at:
        disc = discs[(release, at["disc"])]
        return plan.op(f"slot:{release}/{at['disc']}/{at['slot']}", "slot", {"disc": disc}, {"name": at["slot"]})
    return chunk(plan, release, at, by_path, places, containers)


def texture_reads(path: str, table: dict, by_path: dict, release: str, palettes) -> tuple[list[dict], dict]:
    """What a release's texture reads - its first page's chunk, the file's later image chunks holding the rest of it,
    its palettes' sources - and the settings its extraction is given."""
    named, runs, files_at = palettes
    placed = table["from"][release]
    at = placed["at"][0]
    reads = [place(at)]
    file_path = files_at.get((release, at.get("stage", 0), at.get("folder"), at.get("file", 0)))
    file = by_path[file_path].table if file_path else {}
    images = {path: table}
    slots = file.get("slots", [])
    names = [s["name"] for s in slots]
    if at.get("slot") in names:
        for slot in slots[names.index(at["slot"]) + 1:]:
            pages = slot.get("pages", [])
            if any(target(e.get("image")) == path for e in pages if e.get("image")):
                reads.append(place(at, slot["name"]))
            for e in pages:
                other = target(e.get("image")) if e.get("image") else None
                if other and other in by_path:
                    images[other] = by_path[other].table
    files = {file_path: file} if file_path else {}
    vram, given_palettes, given_runs = {}, [], []
    for name, palette in table.get("palettes", {}).items():
        if not isinstance(palette, dict) or not palette.get("slot") or palette.get("at") is None:
            continue
        slot = target(palette["slot"])
        vram[slot] = by_path[slot].table
        source = named.get((path, name))
        if source is not None:
            given_palettes.append({"palette": name, **source})
        else:
            source = runs.get((slot, tuple(palette["at"])))
            if source is None:
                continue
            given_runs.append({"vram": slot, "at": list(palette["at"]), **source})
        files[source["file"]] = by_path[source["file"]].table
        source_at = by_path[source["file"]].table["from"][release]["at"][0]
        read = place(source_at, source["slot"])
        if read not in reads:
            reads.append(read)
    args = {
        "path": path, "release": release, "images": images, "files": files, "vram": vram,
        "palettes": given_palettes, "runs": given_runs, "reads": reads,
        "places": [{"stage": at.get("stage", 0), "folder": at.get("folder"), "file": at.get("file", 0), "path": file_path}]
        if file_path else [],
    }
    return reads, args


def textured(plan: Plan, path: str, table: dict, registry: dict, by_path: dict, releases, places, containers, palettes,
             discs):
    """A texture: its native files from the chunks it reads, given what it reads of the other declarations; then its
    source, written and checked to import back. Made once for each group of releases declaring it alike, of any of
    their chunks at each place - the extraction holding them to the first's declarations."""
    declaring = [r for r in releases if table.get("from", {}).get(r)]
    if not declaring:
        return
    read = {r: texture_reads(path, table, by_path, r, palettes) for r in declaring}
    as_declared = lambda r: json.dumps({k: table["from"][r].get(k) for k in ("sha1", "palettes", "offset", "size")},  # noqa: E731
                                       sort_keys=True) + str(len(read[r][0]))
    groups = by_what_they_declare({r: as_declared(r) for r in declaring})
    declared, _ = nodes.stamped_node(path, table, registry, releases)
    source = table.get("import", {}).get("source", "source.ora")
    made_by = {}
    for n, group in enumerate(groups):
        reads, args = read[group[0]]
        chunks = [any_of([read_at(plan, r, read[r][0][k], by_path, places, containers, discs) for r in group])
                  for k in range(len(reads))]
        own = "" if n == 0 else f"{group[0]}:"
        texture = plan.op(f"texture:{own}{path}", "pe2.images@extract_texture", {"chunks": chunks}, json_safe(args))
        # As TOML text: its tables' order - a texture's palettes, its cells - is what its source is written in.
        made = plan.op(f"source:{own}{path}", "source", {"native": texture},
                       {"node": path, "uid": table["uid"], "declared": toml_out.document(declared)})
        for release in group:
            made_by[release] = made
    emit_by_release(plan, path, source, groups, made_by)


def atlased(plan: Plan, path: str, table: dict, by_path: dict, releases, places, containers, discs) -> None:
    """An atlas: its pages and its faces' tables read, and from them its layout, what lies under its glyphs where
    anything does, and each face's UFO - one operation for all of them, as each face is read through the atlas. Made
    once for each group of releases declaring it and its faces alike, of any of their chunks at each place."""
    faces, encodings = {}, {}
    for entry in table.get("faces", []):
        face = target(entry["font"])
        faces[face] = by_path[face].table
        if faces[face].get("encoding"):
            encoding = target(faces[face]["encoding"])
            encodings[encoding] = by_path[encoding].table
    declaring = [r for r in releases if table.get("from", {}).get(r)
                 and all(f.get("from", {}).get(r) for f in faces.values())]
    if not declaring:
        return

    def reads_of(release: str) -> list[dict]:
        placed = table["from"][release]
        reads = [place(sheet["at"][0]) for sheet in placed.get("pages", [placed])]
        for face_table in faces.values():
            read = place(face_table["from"][release]["at"][0])
            if read not in reads:
                reads.append(read)
        return reads
    read = {r: reads_of(r) for r in declaring}
    as_declared = lambda r: json.dumps([table["from"][r]] + [f["from"][r] for f in faces.values()],  # noqa: E731
                                       sort_keys=True, default=str) + str(len(read[r]))
    groups = by_what_they_declare({r: as_declared(r) for r in declaring})
    made_by = {}
    for n, group in enumerate(groups):
        chunks = [any_of([read_at(plan, r, read[r][k], by_path, places, containers, discs) for r in group])
                  for k in range(len(read[group[0]]))]
        args = {"atlas": path, "release": group[0], "atlases": {path: table}, "faces": faces, "encodings": encodings,
                "reads": read[group[0]]}
        own = "" if n == 0 else f"{group[0]}:"
        made = plan.op(f"atlas:{own}{path}", "pe2.fonts@extract_atlas", {"chunks": chunks}, json_safe(args))
        for release in group:
            made_by[release] = made
    emit_by_release(plan, path, "layout.toml", groups, made_by)
    emit_by_release(plan, path, "under.png", groups, made_by, optional=True)
    for face in faces:
        name = face.rsplit("/", 1)[-1]
        prefix = f"#{face}/{name}.ufo/"
        emit_by_release(plan, face, f"{name}.ufo/", groups, {r: id + prefix for r, id in made_by.items()})


def json_safe(item):
    """A value as JSON holds it, as a TOML table reads back: no nulls."""
    if isinstance(item, dict):
        return {k: json_safe(v) for k, v in item.items() if v is not None}
    if isinstance(item, list):
        return [json_safe(v) for v in item]
    return item


def make(decl, registry: dict, releases: list[str]) -> Plan:
    plan = Plan(releases, registry.get("extensions", {}))
    by_path = dict(sorted(decl.by_path.items()))
    discs: dict[tuple[str, str], str] = {}
    containers: dict[tuple[str, int], tuple[str, str]] = {}
    for release in releases:
        for path, d in by_path.items():
            parts = path.split("/")
            if len(parts) != 3 or parts[0] != "disc" or parts[1] != release:
                continue
            table = d.table
            if table.get("template") == "disc":
                image = table["image"]
                slots = [[s["name"], s["type"]] for s in table.get("slots", [])]
                disc = {"name": image["name"], "size": image["size"], "sha1": image["sha1"]}
                plan.inputs[release].append(disc)
                args = {"image": disc, "release": release, "slots": slots}
                discs[(release, parts[2])] = plan.op(f"disc:{release}/{parts[2]}", "disc", args=args)
    for release in releases:
        for path, d in by_path.items():
            parts = path.split("/")
            if len(parts) != 3 or parts[0] != "disc" or parts[1] != release or d.table.get("template") != "stage":
                continue
            stage = int(parts[2].removeprefix("stage"))
            at = d.table["at"][0]
            disc = discs[(release, at["disc"])]
            cdf = "STAGE0.CDF" if stage == 0 else at["slots"][0]
            id = plan.op(f"container:{release}/{stage}", "container", {"disc": disc}, {"stage": stage, "cdf": cdf})
            containers[(release, stage)] = (disc, id)
    palettes = palette_sources(by_path)
    # A file asset by where it is: the last declared there, as the editor's extraction found them.
    places: dict[tuple, str] = {}
    for path, d in by_path.items():
        for release, placed in d.table.get("from", {}).items():
            for at in placed.get("at", []):
                if "disc" not in at and "slot" not in at and "stage" in at and "file" in at:
                    places[(release, at["stage"], at.get("folder"), at["file"])] = path
    for path, d in by_path.items():
        table = d.table
        template = table.get("template")
        if path.startswith("disc/") and path.count("/") == 2:
            continue
        node_type = registry["templates"].get(template, {}).get("node_type", "")
        if template == "stream" or (nodes.extracted(node_type, table) and node_type not in PLANNED):
            continue
        if node_type in (MESH, CLIP, MODEL):
            rendered(plan, path, table, node_type, by_path, releases, places, containers)
            continue
        if node_type == ATLAS:
            atlased(plan, path, table, by_path, releases, places, containers, discs)
            continue
        if node_type == FACE:
            continue  # made with its atlas
        if node_type == IMAGE and nodes.extracted(node_type, table):
            textured(plan, path, table, registry, by_path, releases, places, containers, palettes, discs)
            continue
        made_by, declared_as = {}, {}
        for release in releases:
            placed = table.get("from", {}).get(release)
            if not placed:
                continue
            sha1 = placed.get("sha1")
            for at in placed.get("at", []):
                reads = []
                folder = at.get("folder")
                where = "-" if folder is None else str(folder)
                if "disc" not in at and ("slot" in at or "stream" in at):
                    if sha1 is None:
                        raise ValueError(f"{path}: {release} gives no sha1")
                    disc, container = containers[(release, at["stage"])]
                    if "stream" in at:
                        id = f"stream:{release}/{at['stage']}/{where}/{at['stream']}"
                        args = {"folder": folder, "index": at["stream"]}
                        reads.append(plan.op(id, "stored_stream", {"disc": disc, "container": container}, args, sha1))
                    else:
                        file_asset = places[(release, at["stage"], folder, at["file"])]
                        slots = [s["name"] for s in by_path[file_asset].table.get("slots", [])]
                        index = slots.index(at["slot"])
                        id = f"chunk:{release}/{at['stage']}/{where}/{at['file']}/{index}"
                        args = {"folder": folder, "file": at["file"], "index": index}
                        reads.append(plan.op(id, "chunk", {"disc": disc, "container": container}, args, sha1))
                elif "disc" in at and "clip" in at:
                    if sha1 is None:
                        raise ValueError(f"{path}: {release} gives no sha1")
                    disc = discs[(release, at["disc"])]
                    file = f"{release}/{at['disc']}/{at['slot']}"
                    clips = plan.op(f"clips:{file}", "movie_clips", {"disc": disc}, {"name": at["slot"]})
                    args = {"name": at["slot"], "index": at["clip"]}
                    reads.append(plan.op(f"clip:{file}/{at['clip']}", "clip", {"disc": disc, "clips": clips}, args, sha1))
                elif "disc" in at:
                    names = ([at["slot"]] if "slot" in at else []) + at.get("slots", [])
                    if names and sha1 is None:
                        raise ValueError(f"{path}: {release} gives no sha1")
                    for name in names:
                        disc = discs[(release, at["disc"])]
                        reads.append(plan.op(f"slot:{release}/{at['disc']}/{name}", "slot", {"disc": disc}, {"name": name}, sha1))
                if reads and release not in made_by:
                    made_by[release], declared_as[release] = reads[0], sha1
        if made_by:
            name = f"native.{table.get('format', 'bin')}"
            emit_by_release(plan, path, name, by_what_they_declare(declared_as), made_by)
    # A disc's movie file, joined from the movies it holds as the bundles were given them: held to the disc's.
    for path, d in by_path.items():
        table = d.table
        if table.get("template") != "stream":
            continue
        for release in releases:
            placed = table.get("from", {}).get(release)
            if not placed or not placed.get("sha1"):
                continue
            files = {f"clip:{release}/{at['disc']}/{at['slot']}/" for at in placed.get("at", []) if "disc" in at}
            after = sorted(id for id in plan.ops if any(id.startswith(f) for f in files))
            id = f"assembled:{path}" if release == next(r for r in releases if table.get("from", {}).get(r)) \
                else f"assembled:{release}:{path}"
            plan.check(plan.op(id, "assembled", None, {"node": path}, placed["sha1"], after))
    return plan
