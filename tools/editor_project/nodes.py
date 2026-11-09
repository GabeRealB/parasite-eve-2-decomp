"""Each declared asset's node as the editor reads it: its `node.toml`, its stack stamped from its template.

From a declaration (`tools/buildsteps/declarations.py`) and what a project's extensions offer (`pe2-cli project
registry`), as the editor's extraction made them until now (`project::extract`, whose steps are named here):

- `asset_toml`: the declaration without its `uid`, `template` and `from` (and `format`, where it has an `[import]`),
  its references written as their uids alone (`"uid://..."`, `"uid://...#palettes/default"` for an output of one);
- the stamp: the template's stack with `{file}` named - the import's source, else the folder an extractor writes
  (a font face's `<name>.ufo`), else `native.<format>` where the asset's bytes are read off a disc, else nothing - and
  the import's other settings set on the node;
- `without_derived`, for an asset written as its import's source (an image's `source.ora`): what its importer derives
  (an image's size) left out, as the source says it;
- `settings_on_layers`: the node's fields on the layer they belong on - the first of its own operations before
  anything is taken from it, else its stored file;
- `[original.<release>] sha1`, where a release's sha1 is declared.

A disc's layout (`disc/<release>/<name>`) is its declaration without `at`, stamped with no file.
"""
from __future__ import annotations

import copy
import tomllib

RES = "res://"

# What the extensions' extractors make, by the type of node and the fields an asset declares: what is made by an
# extractor rather than read off a slot (the editor's `Extensions::extractor_for` over the extractors registered).
EXTRACTED = (
    ("pe2.images@image", ("mode",)),
    ("pe2.render@clip", ()),
    ("pe2.render@animation_schema", ()),
    ("pe2.render@animation_bank", ()),
    ("pe2.render@material", ()),
    ("pe2.render@mesh", ()),
    ("pe2.render@skeleton", ()),
    ("pe2.render@model", ()),
    ("pe2.render@prefab", ()),
    ("pe2.fonts@font", ()),
    ("pe2.fonts@font_atlas", ()),
    ("pe2.movies@movie", ()),
    ("pe2.sounds@sound_bank", ()),
    ("pe2.sounds@sound_script", ()),
    ("pe2.sounds@instrument", ()),
    ("pe2.sounds@sound", ()),
    ("pe2.sounds@reverb", ()),
    ("pe2.game@bundle", ()),
    ("pe2.dialogue@dialogue", ()),
)
# A font face's files, as its extractor writes them: in a folder named after it.
FACE = "pe2.fonts@font"


def extracted(node_type: str, table: dict) -> bool:
    return any(node_type == made and all(f in table for f in requires) for made, requires in EXTRACTED)


def is_reference(item) -> bool:
    return isinstance(item, dict) and isinstance(item.get("uid"), str) and str(item.get("path", "")).startswith(RES)


def uids_alone(item):
    """References - `{ uid, path = "res://..." [, output] }` - as their uids alone, everywhere."""
    if is_reference(item):
        output = item.get("output")
        return item["uid"] + (f"#{output}" if output else "")
    if isinstance(item, dict):
        return {k: uids_alone(v) for k, v in item.items()}
    if isinstance(item, list):
        return [uids_alone(v) for v in item]
    return item


def reads_bytes(placed: dict, releases: list[str]) -> bool:
    """Whether an asset no extractor makes is read off a disc: a chunk's slot, a stored stream, or a disc's slot."""
    for release in releases:
        for at in placed.get(release, {}).get("at", []):
            if "disc" not in at and ("slot" in at or "stream" in at):
                return True
            if "disc" in at and (at.get("slot") or at.get("slots")):
                return True
    return False


def stamped(registry: dict, template: str, file: str) -> list[dict]:
    found = registry["templates"].get(template)
    if found is None:
        raise KeyError(f"no extension gives the template {template}")
    return tomllib.loads(found["stack"].replace("{file}", file)).get("stack", [])


def import_of(stack: list[dict], host: list[str]) -> str | None:
    """The operation importing a node's stored file, where its stack begins with one taken by an operation."""
    if len(stack) < 2 or stack[0].get("op") != "file" or not isinstance(stack[0].get("file"), str):
        return None
    op = stack[1].get("op")
    return op if isinstance(op, str) and op not in host else None


def without(item, steps: list[str]) -> None:
    """A setting removed by its path, `*` standing for every key."""
    if not isinstance(item, dict) or not steps:
        return
    step, rest = steps[0], steps[1:]
    if not rest:
        item.pop(step, None)
    elif step == "*":
        for part in item.values():
            without(part, rest)
    elif step in item:
        without(item[step], rest)


def home(stack: list[dict]) -> int | None:
    taken = next((i for i, layer in enumerate(stack) if layer.get("op") in ("output", "preview")), len(stack))
    own = next((i for i, layer in enumerate(stack[:taken]) if layer.get("op") != "file"), None)
    return own if own is not None else (0 if stack else None)


def settings_on_layers(node: dict) -> dict:
    stack = node.get("stack", [])
    at = home(stack)
    if at is None:
        return node
    layer = stack[at]
    for k in [k for k in node if k != "stack"]:
        layer[k] = node.pop(k)
    return node


def asset_node(path: str, table: dict, registry: dict, releases: list[str]) -> dict:
    fields, derived = stamped_node(path, table, registry, releases)
    for setting in derived:
        without(fields, setting.split("."))
    originals = fields.pop("original", None)
    node = settings_on_layers(fields)
    if originals:
        node["original"] = originals
    return node


def stamped_node(path: str, table: dict, registry: dict, releases: list[str]) -> tuple[dict, list[str]]:
    """A node as stamped, before what its import derives is left out and its settings put on its layers - what its
    source is written from and checked against - and what its import derives, where its source is what it holds."""
    fields = copy.deepcopy(table)
    template = fields.pop("template")
    placed = fields.pop("from", {})
    fields.pop("uid", None)
    fields = uids_alone(fields)
    imported = fields.pop("import", None)
    if imported is not None:
        fields.pop("format", None)
    node_type = registry["templates"].get(template, {}).get("node_type", "")
    made = extracted(node_type, table)
    if imported and imported.get("source"):
        file = imported["source"]
    elif made:
        file = f"{path.rsplit('/', 1)[-1]}.ufo" if node_type == FACE else ""
    elif template != "stream" and reads_bytes(placed, releases):
        file = f"native.{table.get('format', 'bin')}"
    else:
        file = ""
    for k, v in (imported or {}).items():
        if k not in ("source", "importer"):
            fields[k] = v
    # What it lays after its template's stack (`layers`): an operation the project defines, an output.
    stack = stamped(registry, template, file) + fields.pop("layers", [])
    fields["stack"] = stack
    op = import_of(stack, registry["host_operations"])
    importer = registry["operations"].get(op, {}) if op else {}
    derived = importer.get("derives", []) if made and importer.get("inverse") else []
    originals = {r: {"sha1": placed[r]["sha1"]} for r in releases if placed.get(r, {}).get("sha1")}
    if originals:
        fields["original"] = originals
    return fields, derived


def layout_node(table: dict, registry: dict) -> dict:
    fields = copy.deepcopy(table)
    template = fields.pop("template")
    for k in ("at", "uid"):
        fields.pop(k, None)
    fields = uids_alone(fields)
    fields["stack"] = stamped(registry, template, "")
    return settings_on_layers(fields)
