"""A model as a viewer draws it: vertices in the rest pose and textured faces, from the codec.

    model, placement = from_source(package_bytes, load, source_offset)   any model the overlays declare (tmd or model)
    vertices = posed(model)                                              world positions, one per vertex index
    faces, textures, pairs, parts = faces_of(model, placement)

A face is its corners' vertex indices in rim order (a quad's stored Z order v0 v1 v2 v3 drawn as v0 v1 v3 v2). Its
texture is None, or the index of its [texture page, CLUT] pair followed by one (u, v) per corner in the same order;
the page word leaves out the blend rate, which is the material's. Y points down, as in the game.
"""
from __future__ import annotations

from . import codec, tmd

TPAGE_MODE = {4: 0, 8: 1, 16: 2}


def from_source(data: bytes, load: int, source_offset: int) -> tuple[dict, dict]:
    """Decode the model whose `TmdSource` record is at `source_offset`: its arrays through its record."""
    src = tmd.read_source(data, load, source_offset)
    head = src["head"]
    return codec.decode(data[head:source_offset + tmd.TMD_SOURCE_SIZE], load + head)


def worlds(parts: list[dict]) -> list[tuple[list[list[float]], list[float]]]:
    """Each part's rest transform in model space, (rotation rows, translation), composed up its parents."""
    out: list = [None] * len(parts)

    def get(i: int):
        if out[i] is None:
            p = parts[i]
            rot = [[float(x) for x in row] for row in p["rotation"]]
            tr = [float(x) for x in p["translation"]]
            if p["parent"] != i:
                prot, ptr = get(p["parent"])
                rot = [[sum(prot[r][k] * rot[k][c] for k in range(3)) for c in range(3)] for r in range(3)]
                tr = [sum(prot[r][k] * tr[k] for k in range(3)) + ptr[r] for r in range(3)]
            out[i] = (rot, tr)
        return out[i]

    for i in range(len(parts)):
        get(i)
    return out


def posed(model: dict) -> list[tuple[float, float, float]]:
    """Every vertex in the rest pose, in vertex-index order: each part's in turn, then any loose ones as stored."""
    out = []
    for (rot, tr), part in zip(worlds(model["parts"]), model["parts"]):
        for v in part["vertices"]:
            out.append(tuple(sum(rot[r][k] * v[k] for k in range(3)) + tr[r] for r in range(3)))
    out += [tuple(float(c) for c in v) for v in model.get("loose_vertices", ())]
    return out


def page_pair(slot: dict) -> list[int]:
    """The [texture page, CLUT] words of a slot's raw placement, the page without its blend rate."""
    (px, py), (cx, cy) = slot["page"], slot["palette"]
    return [TPAGE_MODE[slot["bits"]] << 7 | (py & 0x100) >> 4 | (px & 0x3FF) >> 6, cy << 6 | (cx >> 4)]


def faces_of(model: dict, placement: dict):
    """(faces, textures, pairs, parts) as the module docstring describes them."""
    faces, textures, pairs, parts = [], [], [], []
    for f in model["faces"]:
        faces.append([c["vertex"] for c in f["corners"]])
        parts.append(f.get("part", -1))
        slot = model["materials"][f["material"]].get("slot")
        if slot is None:
            textures.append(None)
            continue
        pair = page_pair(placement["slots"][slot])
        if pair not in pairs:
            pairs.append(pair)
        textures.append([pairs.index(pair)] + [v for c in f["corners"] for v in c["uv"]])
    return faces, textures, pairs, parts
