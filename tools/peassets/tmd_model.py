"""A PE2 model (a `TmdSource` with its arrays and stream) as a self-contained authored model, and back.

    model, slots = decode(blob, address)    blob = the model's bytes, arrays through record; address = its first byte
    blob = encode(model, slots, address)    byte-identical for every one of the 790 declared models

The model holds parts (bones and their vertices), normals, materials and faces described by rendering properties:
shading (unlit/flat/smooth), face or corner colors, transparency and blend mode, a texture slot by name, and
model-level `effect` (an owner effect slot) and `reflection`. Everything PS1-specific is derived: opcodes, command
runs, the seam group and its transform pass (faces whose corners span parts), depth references, GPU code bytes,
array layout and buffer sizes. `slots` maps each slot to its raw placement (color depth, page, palette), which a
binding replaces with texture assets. Notes: local/authoring/model.md.
"""
import struct



# --- element layouts ---

MODIFIERS = 0x4000 | 0x8000 | 0x10000 | 0x20000

# base opcode -> (corners, normals per face: 0/1/corners, color words, textured, cached refs)
LAYOUT = {
    0x04: (3, 0, 1, False, False), 0x44: (4, 0, 1, False, False),
    0x20: (3, 3, 1, False, False), 0x22: (3, 3, 1, False, False),
    0x40: (4, 1, 1, False, False), 0x60: (4, 4, 1, False, False), 0x62: (4, 4, 1, False, False),
    0x18: (3, 1, 0, True, False), 0x1A: (3, 1, 0, True, False), 0x1C: (3, 0, 0, True, False), 0x1E: (3, 0, 0, True, False),
    0x38: (3, 3, 0, True, False), 0x3A: (3, 3, 0, True, False), 0x30: (3, 3, 1, True, False),
    0x58: (4, 1, 0, True, False), 0x5A: (4, 1, 0, True, False), 0x5C: (4, 0, 0, True, False), 0x5E: (4, 0, 0, True, False),
    0x78: (4, 4, 0, True, False), 0x7A: (4, 4, 0, True, False), 0x70: (4, 4, 1, True, False),
    0x156: (4, 0, 4, True, False),
    0x21: (3, 0, 0, False, True), 0x31: (3, 0, 0, True, True), 0x39: (3, 0, 0, True, True), 0x3B: (3, 0, 0, True, True),
    0x79: (4, 0, 0, True, True), 0x7B: (4, 0, 0, True, True),
}
PASS = {0xC8: 0, 0xC0: 1}   # transform pass: (vertex, normal) + two cache destinations [+ color]


def halves(words, n):
    out = []
    for w in words:
        out += [w & 0xFFFF, w >> 16]
    return out[:n], out[n:]


def pack_halves(hs):
    hs = list(hs) + [0] * (len(hs) % 2)
    return [hs[i] | (hs[i + 1] << 16) for i in range(0, len(hs), 2)]


def decode_element(op, words):
    base = op & ~MODIFIERS
    if base in PASS:
        (v, n), _ = halves(words[:1], 2)
        dw = words[2] if PASS[base] else words[1]          # 0xC0 carries its color before the destinations
        f = {"pass": True, "v": v // 8, "n": n // 8, "dest": (dw & 0xFFFF, dw >> 16)}
        if PASS[base]: f["color"] = words[1]
        return f
    corners, nn, ncol, tex, cached = LAYOUT[base]
    nrefs = corners + (nn if not cached else 0)
    rw = (nrefs + 1) // 2
    hs, spare = halves(words[:rw], nrefs)
    f = {}
    if cached:
        f["cache"] = [h // 4 for h in hs]; bad = [h for h in hs if h % 4]
    else:
        f["v"] = [h // 8 for h in hs[:corners]]; f["n"] = [h // 8 for h in hs[corners:]]
        bad = [h for h in hs if h % 8]
    f["_spare"] = spare
    if bad: f["_misaligned"] = bad
    cw = words[rw:rw + ncol]
    if ncol: f["color"] = cw
    if tex:
        t = words[rw + ncol:]
        f["uv"] = [t[0] & 0xFFFF, t[1] & 0xFFFF, t[2] & 0xFFFF] + ([t[2] >> 16] if corners == 4 else [])
        f["clut"], f["tpage"] = t[0] >> 16, t[1] >> 16
        if corners == 3 and t[2] >> 16: f["_uvspare"] = t[2] >> 16
    return f


def encode_element(op, f):
    base = op & ~MODIFIERS
    if base in PASS:
        w = [(f["v"] * 8) | ((f["n"] * 8) << 16)]
        if PASS[base]: w.append(f["color"])
        w.append(f["dest"][0] | (f["dest"][1] << 16))
        return w
    corners, nn, ncol, tex, cached = LAYOUT[base]
    hs = [c * 4 for c in f["cache"]] if cached else [v * 8 for v in f["v"]] + [n * 8 for n in f["n"]]
    words = pack_halves(hs + f.get("_spare", []))
    words = words[:(len(hs) + 1) // 2]
    if len(hs) % 2: words[-1] |= (f["_spare"][0] if f.get("_spare") else 0) << 16
    words += f.get("color", [])
    if tex:
        uv = f["uv"]
        words += [uv[0] | (f["clut"] << 16), uv[1] | (f["tpage"] << 16),
                  uv[2] | ((uv[3] if corners == 4 else f.get("_uvspare", 0)) << 16)]
    return words


# --- the raw layer ---

GROUP_END, STREAM_END = 0xFFFFFFFE, 0xFFFFFFFF
RECORD_SIZE = 0x24
BONE_SIZE = 36


def decode_raw(data: bytes, load: int, src: int) -> dict:
    flag, half, pre, parts, counts, verts, norms, skel, stream = struct.unpack_from("<4i5I", data, src)
    off = lambda va: va - load
    m = {"bufferHalfBytes": half, "preXformRegionBytes": pre, "flag": flag}
    m["bones"] = [struct.unpack_from("<9h2x3ii", data, off(skel) + BONE_SIZE * i) for i in range(parts)]
    m["counts"] = list(struct.unpack_from(f"<{parts}I", data, off(counts))) if counts else None
    nv = (norms - verts) // 8
    m["verts"] = [struct.unpack_from("<4h", data, off(verts) + 8 * i) for i in range(nv)]
    nn = (stream - norms) // 8
    m["normals"] = [struct.unpack_from("<4h", data, off(norms) + 8 * i) for i in range(nn)]
    # stream: groups of commands; a group ends at GROUP_END, the stream at STREAM_END
    groups, cur, o = [], [], off(stream)
    while True:
        w = struct.unpack_from("<I", data, o)[0]
        if w == STREAM_END:
            groups.append(cur); o += 4; break
        if w == GROUP_END:
            groups.append(cur); cur = []; o += 4; continue
        slot, dims = struct.unpack_from("<II", data, o + 4)
        n, st = dims >> 16, dims & 0xFFFF
        els = [list(struct.unpack_from(f"<{st}I", data, o + 12 + 4 * st * e)) for e in range(n)]
        cur.append({"op": w, "slot": slot, "stride": st, "elements": els})
        o += 12 + 4 * n * st
    m["groups"] = groups
    # where things are, to check the layout rule
    m["_at"] = dict(skel=off(skel), counts=off(counts) if counts else None, verts=off(verts), norms=off(norms),
                    stream=off(stream), stream_end=o, record=src)
    return m


def layout(m: dict, head: int) -> dict:
    """Offsets by the layout rule: skeleton, counts, vertices, normals, stream, record - contiguous."""
    at = {"skel": head}
    at["counts"] = at["skel"] + BONE_SIZE * len(m["bones"])
    at["verts"] = at["counts"] + 4 * len(m["counts"] or [])
    at["norms"] = at["verts"] + 8 * len(m["verts"])
    at["stream"] = at["norms"] + 8 * len(m["normals"])
    size = 0
    for gi, g in enumerate(m["groups"]):
        for c in g:
            size += 12 + 4 * c["stride"] * len(c["elements"])
        size += 4  # GROUP_END, or STREAM_END after the last group
    at["record"] = at["stream"] + size
    return at


def encode_raw(m: dict, load: int, head: int) -> bytes:
    at = layout(m, head)
    out = bytearray()
    for b in m["bones"]:
        out += struct.pack("<9h2x3ii", *b)
    if m["counts"] is not None:
        out += struct.pack(f"<{len(m['counts'])}I", *m["counts"])
    for v in m["verts"]:
        out += struct.pack("<4h", *v)
    for n in m["normals"]:
        out += struct.pack("<4h", *n)
    for gi, g in enumerate(m["groups"]):
        for c in g:
            out += struct.pack("<III", c["op"], c["slot"], (len(c["elements"]) << 16) | c["stride"])
            for e in c["elements"]:
                out += struct.pack(f"<{c['stride']}I", *e)
        out += struct.pack("<I", STREAM_END if gi == len(m["groups"]) - 1 else GROUP_END)
    va = lambda o: load + o
    out += struct.pack("<4i5I", m["flag"], m["bufferHalfBytes"], m["preXformRegionBytes"], len(m["bones"]),
                       va(at["counts"]) if m["counts"] is not None else 0, va(at["verts"]), va(at["norms"]),
                       va(at["skel"]), va(at["stream"]))
    return bytes(out)




# --- the general layer ---

# base opcode <-> (corners, textured, shading, color, semi, seam)
SEMANTICS = {
    0x04: (3, False, "unlit", "face", False, False), 0x44: (4, False, "unlit", "face", False, False),
    0x20: (3, False, "smooth", "face", False, False), 0x22: (3, False, "smooth", "face", True, False),
    0x40: (4, False, "flat", "face", False, False),
    0x60: (4, False, "smooth", "face", False, False), 0x62: (4, False, "smooth", "face", True, False),
    0x18: (3, True, "flat", None, False, False), 0x1A: (3, True, "flat", None, True, False),
    0x1C: (3, True, "unlit", None, False, False), 0x1E: (3, True, "unlit", None, True, False),
    0x38: (3, True, "smooth", None, False, False), 0x3A: (3, True, "smooth", None, True, False),
    0x30: (3, True, "smooth", "face", False, False),
    0x58: (4, True, "flat", None, False, False), 0x5A: (4, True, "flat", None, True, False),
    0x5C: (4, True, "unlit", None, False, False), 0x5E: (4, True, "unlit", None, True, False),
    0x78: (4, True, "smooth", None, False, False), 0x7A: (4, True, "smooth", None, True, False),
    0x70: (4, True, "smooth", "face", False, False),
    0x156: (4, True, "unlit", "corner", True, False),
    0x21: (3, False, "smooth", "corner", False, True),
    0x39: (3, True, "smooth", None, False, True), 0x3B: (3, True, "smooth", None, True, True),
    0x79: (4, True, "smooth", None, False, True), 0x7B: (4, True, "smooth", None, True, True),
}
OPCODE = {v: k for k, v in SEMANTICS.items()}
ROUTINE = {0: 0, 0x8000: 1, 0x10000: 2, 0x20000: 3}
ROUTINE_BITS = {v: k for k, v in ROUTINE.items()}
CODE = {0x04: 0x20, 0x44: 0x28, 0x20: 0x30, 0x22: 0x32, 0x40: 0x38, 0x60: 0x38, 0x62: 0x3A, 0x30: 0x34, 0x70: 0x3C, 0x156: 0x3C}
PACKET = {3: {False: (28, 8), True: (40, 12)}, 4: {False: (36, 8), True: (52, 12)}}
# bytes of the primitive each family builds in the object's buffer (layered faces build two)
PRIM = {0x04: 20, 0x44: 24, 0x20: 28, 0x22: 28, 0x40: 36, 0x60: 36, 0x62: 36, 0x1C: 32, 0x1E: 32, 0x5C: 40, 0x5E: 40,
        0x18: 40, 0x1A: 40, 0x30: 40, 0x38: 40, 0x3A: 40, 0x58: 52, 0x5A: 52, 0x70: 52, 0x78: 52, 0x7A: 52, 0x156: 52,
        0x21: 28, 0x39: 40, 0x3B: 40, 0x79: 52, 0x7B: 52}


def part_starts(m):
    s = [0]
    for n in m["counts"]: s.append(s[-1] + n)
    return s


def to_general(m):
    starts = part_starts(m)
    part_of = lambda v: next(i for i in range(len(starts) - 1) if starts[i] <= v < starts[i + 1])
    g = {"parts": [{"parent": b[12], "rotation": [list(b[0:3]), list(b[3:6]), list(b[6:9])], "translation": list(b[9:12]),
                    "vertices": [list(v[:3]) for v in m["verts"][starts[i]:starts[i + 1]]]} for i, b in enumerate(m["bones"])],
         "loose_vertices": [list(v[:3]) for v in m["verts"][starts[-1]:]],
         "normals": [list(n[:3]) for n in m["normals"]], "faces": [],
         }
    # the pass: corner -> (normal, pass opcode, color) keyed by (vertex, destination)
    passes = {}
    for grp in m["groups"]:
        for c in grp:
            for e in c["elements"]:
                f = decode_element(c["op"], e)
                if f.get("pass"): passes[(f["v"], f["dest"][0])] = (f["n"], c["op"], f.get("color"))
    seam_index = len(m["groups"]) - 2
    for gi, grp in enumerate(m["groups"]):
        off = 0
        for c in grp:
            base = c["op"] & ~MODIFIERS
            if base in PASS: continue
            corners, textured, shading, color, semi, seam = SEMANTICS[base]
            for e in c["elements"]:
                f = decode_element(c["op"], e)
                face = {"corners": [], "shading": shading, "blend": "semi" if semi else "opaque",
                        "layer": bool(c["op"] & 0x4000), "routine": ROUTINE[c["op"] & 0x38000]}
                if textured: face["texture"] = {"clut": f["clut"], "tpage": f["tpage"]}
                if color == "face": face["color"] = f["color"][0] & 0xFFFFFF
                verts = f["cache"] if seam else f["v"]
                for k, v in enumerate(verts):
                    corner = {"v": v}
                    if textured: corner["uv"] = f["uv"][k]
                    if seam:
                        size, stride = PACKET[corners][textured]
                        dest = off + (4 if c["op"] & 0x4000 else 8) + stride * k
                        n, pop, pcol = passes[(v, dest)]
                        corner["n"] = n
                        if pop == 0x200C8: corner["routine"] = 1
                        if pcol is not None: corner["color"] = pcol
                    elif shading == "smooth": corner["n"] = f["n"][k]
                    if color == "corner" and not seam: corner["color"] = f["color"][k] & 0xFFFFFF
                    face["corners"].append(corner)
                if shading == "flat": face["n"] = f["n"][0]
                if seam:
                    off += PACKET[corners][textured][0] * (2 if c["op"] & 0x4000 else 1)
                else:
                    face["part"] = gi
                g["faces"].append(face)
    return g


def to_raw(g):
    """Rebuild the raw model: per-part groups (own faces, then the pass), the seam group, an empty end group."""
    counts = [len(p["vertices"]) for p in g["parts"]]
    starts = [0]
    for n in counts: starts.append(starts[-1] + n)
    part_of = lambda v: next(i for i in range(len(starts) - 1) if starts[i] <= v < starts[i + 1])
    m = {"flag": 0,
         "bones": [tuple(p["rotation"][0] + p["rotation"][1] + p["rotation"][2] + p["translation"] + [p["parent"]]) for p in g["parts"]],
         "counts": counts, "verts": [tuple(v + [0]) for p in g["parts"] for v in p["vertices"]] + [tuple(v + [0]) for v in g["loose_vertices"]],
         "normals": [tuple(n + [0]) for n in g["normals"]]}
    groups = [[] for _ in g["parts"]] + [[], []]
    seam_g = groups[-2]
    passes = [[] for _ in g["parts"]]   # per part: (vertex, dest, pass opcode, normal, color)
    off = 0

    def add(group, op, words):
        if group and group[-1]["op"] == op and group[-1]["_open"]: group[-1]["elements"].append(words)
        else: group.append({"op": op, "slot": 0, "stride": len(words), "elements": [words], "_open": True})

    for face in g["faces"]:
        corners = len(face["corners"]); textured = "texture" in face
        seam = "part" not in face
        color = "corner" if any("color" in c for c in face["corners"]) else ("face" if "color" in face else None)
        base = OPCODE[(corners, textured, face["shading"], color, face["blend"] == "semi", seam)]
        op = base | (0x4000 if face["layer"] else 0) | ROUTINE_BITS[face["routine"]]
        f = {}
        vs = [c["v"] for c in face["corners"]]
        if seam: f["cache"] = vs
        else:
            f["v"] = vs
            f["n"] = [c["n"] for c in face["corners"]] if face["shading"] == "smooth" else ([face["n"]] if face["shading"] == "flat" else [])
        if color == "face": f["color"] = [(CODE[base] << 24) | face["color"]]
        if color == "corner" and not seam:
            f["color"] = [((CODE[base] << 24) if k == 0 else 0) | c["color"] for k, c in enumerate(face["corners"])]
        if textured:
            f["uv"] = [c["uv"] for c in face["corners"]]; f["clut"] = face["texture"]["clut"]; f["tpage"] = face["texture"]["tpage"]
        words = encode_element(op, f)
        if seam:
            add(seam_g, op, words)
            size, stride = PACKET[corners][textured]
            for k, c in enumerate(face["corners"]):
                if face["layer"]:
                    dest, pop = (off + 4 + stride * k, off + 4 + stride * k + size), 0x40C8
                else:
                    dest, pop = (off + 8 + stride * k, off + 4 + stride * k), (0x200C8 if c.get("routine") else (0xC0 if "color" in c else 0xC8))
                passes[part_of(c["v"])].append((c["v"], dest, pop, c["n"], c.get("color")))
            off += size * (2 if face["layer"] else 1)
        else:
            add(groups[face["part"]], op, words)
    for pi, plist in enumerate(passes):
        for gcmd in groups[pi]: gcmd["_open"] = False
        for v, dest, pop, n, col in sorted(plist, key=lambda t: (t[2], t[0], t[1])):
            f = {"pass": True, "v": v, "n": n, "dest": dest}
            if col is not None: f["color"] = col
            add(groups[pi], pop, encode_element(pop, f))
    for grp in groups:
        for c in grp: c.pop("_open", None)
    m["groups"] = groups
    size = lambda gs: sum(PRIM[c["op"] & ~MODIFIERS] * (2 if c["op"] & 0x4000 else 1) * len(c["elements"])
                          for grp in gs for c in grp if (c["op"] & ~MODIFIERS) not in PASS)
    m["preXformRegionBytes"] = size([seam_g])
    m["bufferHalfBytes"] = size(groups)
    return m


# --- the authored model ---

BLEND = ["mix", "add", "subtract", "add_quarter"]       # GPU semi-transparency modes, B/2+F/2, B+F, B-F, B+F/4
EFFECT_SEAM_FACES, EFFECT_SEAM_PASS = 1, 3               # effect slots that also mark seam faces / the seam pass


def generalize(g):
    faces = g["faces"]
    effects = {f["routine"] for f in faces if "part" in f} or {0}
    layers = {f["layer"] for f in faces} or {False}
    assert len(effects) == 1 and len(layers) == 1, "effect or reflection not uniform"
    effect, reflection = effects.pop(), layers.pop()
    model = {"parts": [{"parent": p["parent"], "rotation": [[x / 4096 for x in row] for row in p["rotation"]],
                        "translation": p["translation"], "vertices": p["vertices"]} for p in g["parts"]],
             "normals": [[x / 4096 for x in n] for n in g["normals"]],
             "slots": [], "materials": [], "faces": []}
    binding = {"slots": {}}
    if g["loose_vertices"]: model["loose_vertices"] = g["loose_vertices"]
    if effect: model["effect"] = effect
    if reflection: model["reflection"] = "environment"
    slots, mats = {}, {}
    rgb = lambda c: [(c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF][::-1]   # stored as 0xBBGGRR
    for f in faces:
        tex = f.get("texture"); abr = None
        if tex:
            tp, cl = tex["tpage"], tex["clut"]
            key = (tp & ~0x60, cl)
            if key not in slots:
                name = f"texture{len(model['slots'])}"
                slots[key] = name
                model["slots"].append(name)
                binding["slots"][name] = {"bits": 8 if (tp >> 7) & 3 == 1 else 4,
                                          "page": [(tp & 0xF) * 64, ((tp >> 4) & 1) * 256],
                                          "palette": [(cl & 0x3F) * 16, cl >> 6]}
            abr = (tp >> 5) & 3
        mkey = (slots[key] if tex else None, f["blend"] == "semi", abr)
        if mkey not in mats:
            mats[mkey] = len(model["materials"])
            mat = {"transparent": mkey[1]}
            if tex: mat["slot"] = mkey[0]; mat["blend"] = BLEND[abr]
            model["materials"].append(mat)
        out = {"material": mats[mkey], "shading": f["shading"], "corners": []}
        if "part" in f: out["part"] = f["part"]
        if "n" in f: out["normal"] = f["n"]
        if "color" in f: out["color"] = rgb(f["color"])
        for c in f["corners"]:
            oc = {"vertex": c["v"]}
            if "n" in c: oc["normal"] = c["n"]
            if "color" in c: oc["color"] = rgb(c["color"])
            if "uv" in c: oc["uv"] = [c["uv"] & 0xFF, c["uv"] >> 8]
            out["corners"].append(oc)
        if len(out["corners"]) == 4:                      # stored Z order (0,1,2,3) -> rim order (0,1,3,2)
            cs = out["corners"]; out["corners"] = [cs[0], cs[1], cs[3], cs[2]]
        model["faces"].append(out)
    return model, binding


def specialize(model, binding):
    effect = model.get("effect", 0); layer = model.get("reflection") == "environment"
    unrgb = lambda c: c[0] | (c[1] << 8) | (c[2] << 16)
    g = {"parts": [{"parent": p["parent"], "rotation": [[round(x * 4096) for x in row] for row in p["rotation"]],
                    "translation": p["translation"], "vertices": p["vertices"]} for p in model["parts"]],
         "loose_vertices": model.get("loose_vertices", []),
         "normals": [[round(x * 4096) for x in n] for n in model["normals"]], "faces": []}
    for f in model["faces"]:
        if len(f["corners"]) == 4:                        # rim order back to stored Z order
            cs = f["corners"]; f = dict(f, corners=[cs[0], cs[1], cs[3], cs[2]])
        mat = model["materials"][f["material"]]
        seam = "part" not in f
        out = {"shading": f["shading"], "blend": "semi" if mat["transparent"] else "opaque", "layer": layer,
               "routine": effect if (not seam or effect == EFFECT_SEAM_FACES) else 0, "corners": []}
        if not seam: out["part"] = f["part"]
        if "slot" in mat:
            t = binding["slots"][mat["slot"]]
            tp = (t["page"][0] // 64) | ((t["page"][1] // 256) << 4) | (BLEND.index(mat["blend"]) << 5) | ((1 if t["bits"] == 8 else 0) << 7)
            out["texture"] = {"tpage": tp, "clut": (t["palette"][0] // 16) | (t["palette"][1] << 6)}
        if "normal" in f: out["n"] = f["normal"]
        if "color" in f: out["color"] = unrgb(f["color"])
        for c in f["corners"]:
            oc = {"v": c["vertex"]}
            if "normal" in c: oc["n"] = c["normal"]
            if "color" in c: oc["color"] = unrgb(c["color"])
            if "uv" in c: oc["uv"] = c["uv"][0] | (c["uv"][1] << 8)
            if seam and effect == EFFECT_SEAM_PASS: oc["routine"] = 1
            out["corners"].append(oc)
        g["faces"].append(out)
    return g


def decode(blob: bytes, address: int) -> tuple[dict, dict]:
    """The authored model and its raw slot placements, from the model's bytes (arrays through its record)."""
    m = globals()["decode_raw"](blob, address, len(blob) - RECORD_SIZE)
    return generalize(to_general(m))


def encode(model: dict, slots: dict, address: int) -> bytes:
    return encode_raw(to_raw(specialize(model, slots)), address, 0)
