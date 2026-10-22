"""The layouts of a room's data, as the room's C declares them, and the module each kind of object belongs to.

A layout is (C type, fields) with fields (C name, kind); a scalar layout has one field named "" and its elements are
plain values. A kind is a format char (b B h H i I), ("bytes", n), ("arr", kind, n), a layout name, ("ptr", target
layout, or "ref" for a named symbol that is not followed), or ("union", member, kind): the field holds the union's
`member`.

A room's data comes in modules, each a contiguous block of the package: the header (the per-state tables and the
warps), collision geometry, view-boundary and action triggers, occluders, cameras, sprites, lights, ambient levels,
placements, surfaces, and what code alone names - area updates, vibration scripts, effect tables. A module's include
defines its objects in address order.
"""
from __future__ import annotations

# --- layouts ---------------------------------------------------------------------------------------------------
# A layout is (C type, fields) with fields (C name, kind); a scalar layout has one field named "" and its elements
# are plain values. A kind is a format char (b B h H i I), ("bytes", n), ("arr", kind, n), a layout name,
# ("ptr", target layout, or "ref" for a named symbol that is not followed), or ("union", member, kind): the field
# holds the union's `member`.
SV = ("arr", "h", 4)
LAYOUTS: dict[str, tuple[str, list]] = {
    "MATRIX": ("MATRIX", [("m", ("arr", ("arr", "h", 3), 3)), ("t", ("arr", "i", 3))]),
    "ViewCamera": ("ViewCamera", [("transform", "MATRIX"), ("screenDistance", "I")]),
    "ViewCount": ("ViewCount", [("", "h")]),
    "ViewMapPtr": ("u8*", [("", ("ptr", "ViewMap"))]),
    "ViewMap": ("u8", [("", "B")]),
    "WorldCoordRoomLighting": ("WorldCoordRoomLighting", [
        ("lights", ("ptr", "WorldCoordRoomLights")), ("ambientTable", ("ptr", "WorldCoordRoomAmbientEntry"))]),
    "WorldCoordRoomLights": ("WorldCoordRoomLights", [
        ("directionalLightCount", "i"), ("directionalLights", ("ptr", "WorldCoordLight")),
        ("pointLightCount", "i"), ("pointLights", ("ptr", "WorldCoordPointLight")),
        ("coneLightCount", "i"), ("coneLights", ("ptr", "WorldCoordSpotLight"))]),
    "LightTransform": ("", [
        ("composeStamp", "I"), ("local", "MATRIX"), ("composed", "MATRIX"), ("viewId", "h"),
        ("unknown_46", ("bytes", 4)), ("attenuation", "h"), ("parent", ("ptr", "ref"))]),
    "LightColor": ("", [("r", "h"), ("g", "h"), ("b", "h")]),
    "WorldCoordLight": ("WorldCoordLight", [
        ("transform", ("union", "lighting", "LightTransform")), ("color", "LightColor"), ("unknown_56", ("bytes", 2))]),
    "WorldCoordPointLight": ("WorldCoordPointLight", [("head", "WorldCoordLight"), ("inner", "i"), ("outer", "i")]),
    "WorldCoordSpotLight": ("WorldCoordSpotLight", [
        ("head", "WorldCoordLight"), ("axis", SV), ("inner", "i"), ("outer", "i"), ("angle", "i")]),
    "AmbientColor": ("", [("r", "h"), ("g", "h"), ("b", "h"), ("luma", "h")]),
    "WorldCoordRoomAmbientEntry": ("WorldCoordRoomAmbientEntry", [("color", "AmbientColor")]),   # a union: entry 0 also holds the count
    "WorldCollisionRoomResources": ("WorldCollisionRoomResources", [
        ("grid", ("ptr", "WorldCollisionGrid")), ("viewBoundaryTriggers", ("ptr", "WorldCollisionTrigger")),
        ("actionTriggers", ("ptr", "WorldCollisionTrigger")), ("occluders", ("ptr", "WorldCollisionOccluder"))]),
    "WorldCollisionGrid": ("WorldCollisionGrid", [
        ("viewCoord", ("ptr", "ref")), ("normals", ("ptr", "CollisionNormals")), ("vertices", ("ptr", "CollisionVerts")),
        ("faces", ("ptr", "CollisionFaces")), ("cellFaceIds", ("ptr", "CollisionTable")), ("xBias", "i"),
        ("zBias", "i"), ("cellCountX", "H"), ("cellCountZ", "H"), ("cellSize", "H"), ("faceCount", "H")]),
    # A grid's geometry is the collision asset's (gen_collision_inc.py): named here, its elements are not.
    "CollisionNormals": ("SVECTOR", [("", SV)]),
    "CollisionVerts": ("SVECTOR", [("", SV)]),
    "CollisionFaces": ("WorldCollisionGridFace", [("", ("bytes", 12))]),
    "CollisionCells": ("s16", [("", "h")]),
    "CollisionTable": ("s16*", [("", "I")]),
    "WorldCollisionTrigger": ("WorldCollisionTrigger", [
        ("next", ("ptr", "ref")), ("prevLink", ("ptr", "ref")), ("coord", ("ptr", "ref")),
        ("origin", SV), ("vertices", ("arr", SV, 4)), ("normal", SV), ("facingNormal", SV), ("radius", "H"),
        ("control", "H"), ("parameter0", "B"), ("parameter1", "B"), ("flags", "B"), ("hit", "b")]),
    "WorldCollisionOccluder": ("WorldCollisionOccluder", [
        ("next", ("ptr", "ref")), ("prevLink", ("ptr", "ref")), ("origin", SV), ("vertices", ("arr", SV, 4)),
        ("normal", SV), ("radius", "H"), ("flags", "B"), ("unknown_3B", "b")]),
    "SpriteView": ("SpriteView", [
        ("sources", ("union", "elements", ("ptr", "SpriteSources"))), ("batches", ("ptr", "SpriteBatch")),
        ("drawAreas", ("ptr", "SpriteDrawArea"))]),
    "SpriteSize": ("", [("w", "h"), ("h", "h")]),
    "SpriteUv": ("", [("u0", "B"), ("v0", "B")]),
    "SpriteSource": ("SpriteSource", [
        ("tpage", "H"), ("clut", "H"), ("size", ("union", "fields", "SpriteSize")), ("x0", "h"), ("y0", "h"),
        ("depth", "H"), ("uv", ("union", "fields", "SpriteUv")), ("r0", "B"), ("g0", "B"), ("b0", "B"),
        ("codeFlags", "B")]),
    "SpriteBatch": ("SpriteBatch", [
        ("firstSprite", "H"), ("spriteCount", "H"), ("hidden", "B"), ("skipCachedPackets", "B"),
        ("field_6", ("bytes", 2))]),
    "SpriteDrawArea": ("SpriteDrawArea", [("clipRect", "RECT"), ("restoreDepth", "H")]),
    "RECT": ("RECT", [("x", "h"), ("y", "h"), ("w", "h"), ("h", "h")]),
    "ActorSpawnTransform": ("ActorSpawnTransform", [
        ("yaw", ("union", "word", "i")), ("x", "i"), ("y", "i"), ("z", "i")]),
    "DirectionWarpEntry": ("DirectionWarpEntry", [
        ("player", "ActorSpawnTransform"), ("unknown_10", ("bytes", 4)), ("companion", "ActorSpawnTransform"),
        ("unknown_24", ("bytes", 4)), ("arrivalSound", "i"), ("departureSound", "i"), ("blockedSound", "i"),
        ("initialView", "B"), ("flags", "B"), ("mapFlagId", "H")]),
    "SurfacePtr": ("WorldCollisionSurfaceProperties*", [("", ("ptr", "WorldCollisionSurfaceProperties"))]),
    "WorldCollisionSurfaceProperties": ("WorldCollisionSurfaceProperties", [
        ("field_0", "B"), ("probePassThrough", "B"), ("weaponImpactEnabled", "B"), ("suppressPushback", "B"),
        ("footstepSounds", ("ptr", "WorldCollisionFootstepSounds"))]),
    "WorldCollisionFootstepSounds": ("WorldCollisionFootstepSounds", [("walk", "i"), ("run", "i"), ("scriptedJump", "i")]),
}
# Placements: gameplay's area table names each area's variants; a variant's placements may be the stage overlay's.
LAYOUTS.update({
    "AreaVariant": ("AreaVariant", [("placements", ("ptr", "AreaPlacement")), ("resources", ("ptr", "AreaResource"))]),
    "AreaPlacement": ("AreaPlacement", [
        ("entryId", "B"), ("variant", "B"), ("mode", "H"), ("x", "h"), ("y", "h"), ("z", "h"), ("yaw", "h"),
        ("fileIdLow", "B"), ("texturePageOffset", "b"), ("clutRowOffset", "b"), ("rowIndex", "B")]),
    "AreaResource": ("AreaResource", [
        ("entryId", "H"), ("fileNumber", "h"), ("fileGroupIndex", "B"), ("taskIndex", "B"), ("unknown_6", ("bytes", 2)),
        ("taskTable", ("ptr", "ref"))]),
    "AreaApplyRec": ("AreaApplyRec", [("stage", "B"), ("area", "B"), ("variant", "B"), ("policy", "B")]),
    "PadScriptLane": ("PadScriptLane", [("opcode", "B"), ("operand", "B")]),
    "PadScriptCmd": ("PadScriptCmd", [("holdCommand", ("union", "bytes", "PadScriptLane")),
                                      ("lerpCommand", ("union", "bytes", "PadScriptLane"))]),
    "PadScriptVibrationSegment": ("PadScriptVibrationSegment", [
        ("startIntensity", "B"), ("endIntensity", "B"), ("durationFrames", "B"), ("field_3", "B")]),
})
# Plain tables a room's code reads by address: effect positions, view masks, offsets, rows.
LAYOUTS.update({t: (t, [("", f)]) for t, f in (("u8", "B"), ("u16", "H"), ("s16", "h"), ("u32", "I"), ("s32", "i"))})
LAYOUTS.update({
    "SVECTOR": ("SVECTOR", [("vx", "h"), ("vy", "h"), ("vz", "h"), ("pad", "h")]),
    "VECTOR3": ("VECTOR3", [("vx", "i"), ("vy", "i"), ("vz", "i")]),
    "_AcropolisSanctuaryMosaicTile": ("_AcropolisSanctuaryMosaicTile", [
        ("extentU", "h"), ("extentV", "h"), ("originU", "h"), ("originV", "h"), ("thrown", "h"), ("holdFrames", "h"),
        ("sizeClass", "h")]),
})
GEOMETRY = {"CollisionNormals": "normals", "CollisionVerts": "verts", "CollisionFaces": "faces",
            "CollisionCells": "cells", "CollisionTable": "table"}
# A sprite view's source slot is a union: its sources, or (no sources) the view's own batch list.
LAYOUTS["SpriteSources"] = LAYOUTS["SpriteSource"]
FORMATS = {"b": 1, "B": 1, "h": 2, "H": 2, "i": 4, "I": 4}


def _align(kind) -> int:
    if isinstance(kind, str):
        return FORMATS.get(kind) or max(_align(k) for _f, k in LAYOUTS[kind][1])
    if kind[0] in ("named", "bits"):
        return FORMATS[kind[1]]
    if kind[0] in ("bytes", "text"):
        return 1
    if kind[0] in ("arr", "union"):
        return _align(kind[-2] if kind[0] == "arr" else kind[2])
    return 4


def _fields(layout: str):
    """(name, kind, offset) of a layout's fields."""
    off = 0
    for name, kind in LAYOUTS[layout][1]:
        a = _align(kind)
        off = (off + a - 1) // a * a
        yield name, kind, off
        off += size(kind)


def size(kind) -> int:
    if isinstance(kind, str):
        if kind in FORMATS:
            return FORMATS[kind]
        end = max(off + size(k) for _n, k, off in _fields(kind))
        a = _align(kind)
        return (end + a - 1) // a * a
    if kind[0] in ("bytes", "text"):
        return kind[1]
    if kind[0] in ("named", "bits"):
        return FORMATS[kind[1]]
    if kind[0] == "arr":
        return size(kind[1]) * kind[2]
    if kind[0] == "union":
        return size(kind[2])
    return 4


for _layout, _size in (("ViewCamera", 0x24), ("WorldCoordLight", 0x58), ("WorldCoordPointLight", 0x60),
                       ("WorldCoordSpotLight", 0x6C), ("WorldCollisionTrigger", 0x4C), ("WorldCollisionOccluder", 0x3C),
                       ("DirectionWarpEntry", 0x38), ("SpriteSource", 0x14), ("SpriteDrawArea", 0xA)):
    assert size(_layout) == _size, _layout


# The module each kind of object belongs to; triggers belong to the list that names them (view boundaries or action
# triggers), and an entry point names its own module.
MODULE_OF = {
    "WorldCollisionRoomResources": "header", "ViewMapPtr": "header", "ViewMap": "header", "ViewCount": "header",
    "WorldCoordRoomLighting": "header", "DirectionWarpEntry": "header",
    "WorldCollisionGrid": "geometry", **{k: "geometry" for k in GEOMETRY},
    "WorldCollisionOccluder": "occluders", "ViewCamera": "cameras",
    "SpriteView": "sprites", "SpriteSource": "sprites", "SpriteBatch": "sprites", "SpriteDrawArea": "sprites",
    "WorldCoordRoomLights": "lights", "WorldCoordLight": "lights", "WorldCoordPointLight": "lights",
    "WorldCoordSpotLight": "lights", "WorldCoordRoomAmbientEntry": "ambient",
    "AreaVariant": "placements", "AreaResource": "placements", "AreaPlacement": "placements",
    "SurfacePtr": "surfaces", "WorldCollisionSurfaceProperties": "surfaces", "WorldCollisionFootstepSounds": "surfaces",
    "AreaApplyRec": "updates", "PadScriptCmd": "vibrations", "PadScriptVibrationSegment": "vibrations",
}
TRIGGER_MODULE = {"viewBoundaryTriggers": "view_boundaries", "actionTriggers": "action_triggers"}
# Kinds the C declares as one object, not an array; an entry point may declare any object single.
SCALAR = frozenset({"WorldCoordRoomLights", "WorldCollisionFootstepSounds", "WorldCollisionSurfaceProperties",
                    "WorldCollisionGrid"})
# An entry point's kind where its module has one.
ENTRY_KIND = {"lights": "WorldCoordRoomLights", "geometry": "WorldCollisionGrid", "view_boundaries": "WorldCollisionTrigger",
              "action_triggers": "WorldCollisionTrigger", "occluders": "WorldCollisionOccluder", "updates": "AreaApplyRec",
              "cameras": "ViewCamera", "ambient": "WorldCoordRoomAmbientEntry"}


from .music import SEQUENCES  # noqa: E402

# --- values ----------------------------------------------------------------------------------------------------
import re as _re
import struct as _struct


# ("named", format, {number: name}): a number, written by name where it has one. ("bits", format, [(field, shift,
# width, {number: name} or None)]): a number as its bit fields, each by name where it has one, and `other` for bits no
# field covers where any are set; a one-bit field is a flag, written (true) only where it is set.
# Layouts whose records are written as their first field alone where every other field is zero (a marker record).
FIRST_FIELD_ALONE = {"_MistShootingGallerySpawn"}
def _bit_fields(value: int, kind) -> dict:
    fmt, fields = kind[1], kind[2]
    raw, out, covered = value & ((1 << 8 * FORMATS[fmt]) - 1), {}, 0
    for name, shift, width, names in fields:
        mask = (1 << width) - 1
        v = raw >> shift & mask
        covered |= mask << shift
        if width == 1 and names is None:
            if v:
                out[name] = True                          # a flag: written where it is set
        else:
            out[name] = (names or {}).get(v, v)
    if raw & ~covered:
        out["other"] = raw & ~covered
    return out


def number(value, kind) -> int:
    """A named or bit-field value's number, in the field's own signedness."""
    if kind[0] == "named":
        n = value if isinstance(value, int) else {v: k for k, v in kind[2].items()}[value]
    else:
        n = value.get("other", 0) if isinstance(value, dict) else value
        if isinstance(value, dict):
            for name, shift, _width, names in kind[2]:
                v = value.get(name, 0)
                n |= int(v if not isinstance(v, str) else {b: a for a, b in names.items()}[v]) << shift
    bits = 8 * FORMATS[kind[1]]
    n &= (1 << bits) - 1
    return n - (1 << bits) if kind[1].islower() and n >> (bits - 1) else n


def decode_value(data: bytes, off: int, kind, refs: list | None = None, field: str = ""):
    """The value of `kind` at `off`, pointers as addresses; `refs` collects (field, address, target kind)."""
    if isinstance(kind, str) and kind in FORMATS:
        return _struct.unpack_from("<" + kind, data, off)[0]
    if isinstance(kind, str):
        fields = list(_fields(kind))
        if fields[0][0] == "":
            return decode_value(data, off, fields[0][1], refs, field)
        return {n: decode_value(data, off + o, k, refs, n) for n, k, o in fields}
    if kind[0] in ("named", "bits"):
        return _struct.unpack_from("<" + kind[1], data, off)[0]
    if kind[0] == "bytes":
        return list(data[off:off + kind[1]])
    if kind[0] == "text":                                 # a line of text where the bytes are one, else the bytes
        raw = data[off:off + kind[1]]
        text, _nul, rest = raw.partition(b"\0")
        if _nul and not any(rest) and all(0x20 <= b < 0x7F for b in text):
            return text.decode("ascii")
        return list(raw)
    if kind[0] == "arr":
        return [decode_value(data, off + i * size(kind[1]), kind[1], refs, field) for i in range(kind[2])]
    if kind[0] == "union":
        return decode_value(data, off, kind[2], refs, field)
    if kind[0] == "word":
        v = _struct.unpack_from("<I", data, off)[0]
        if refs is not None and len(kind) > 3:           # a word that, as a pointer, leads to its target kind
            refs.append((field, v, kind[3]))
        return v
    v = _struct.unpack_from("<I", data, off)[0]
    if refs is not None:
        refs.append((field, v, kind[1]))
    return v


def _decode_refs(data: bytes, off: int, kind) -> list:
    refs: list = []
    decode_value(data, off, kind, refs)
    return refs


def named(value, kind, name_of):
    """The value with its pointers as names (`name_of(address)`)."""
    if isinstance(kind, str) and kind in FORMATS:
        return value
    if isinstance(kind, str):
        fields = list(_fields(kind))
        if fields[0][0] == "":
            return named(value, fields[0][1], name_of)
        if kind in FIRST_FIELD_ALONE and not any(value[n] for n, _k, _o in fields[1:]):
            return {fields[0][0]: named(value[fields[0][0]], fields[0][1], name_of)}
        return {n: named(value[n], k, name_of) for n, k, _o in fields}
    if kind[0] in ("bytes", "text"):
        return value
    if kind[0] == "named":
        return kind[2].get(value, value)
    if kind[0] == "bits":
        return _bit_fields(value, kind)
    if kind[0] == "arr":
        return [named(v, kind[1], name_of) for v in value]
    if kind[0] == "union":
        return named(value, kind[2], name_of)
    if kind[0] == "word":                                 # a name where something names the address, else the number
        name = name_of(value) if value >= 0x80000000 else value
        return name if isinstance(name, str) else value - (1 << 32) if value >= 0x80000000 else value
    return name_of(value)


def _filled(value, kind) -> dict:
    """A record written as its first field alone, with its other fields (zero)."""
    return {n: value.get(n, 0) for n, _k, _o in _fields(kind)} if kind in FIRST_FIELD_ALONE else value


def encode_value(value, kind, address_of) -> bytes:
    """The bytes of a value whose pointers are names (`address_of(name)`)."""
    if isinstance(kind, str) and kind in FORMATS:
        return _struct.pack("<" + kind, value)
    if isinstance(kind, str):
        fields = list(_fields(kind))
        if fields[0][0] == "":
            return encode_value(value, fields[0][1], address_of)
        out, value = bytearray(size(kind)), _filled(value, kind)
        for n, k, o in fields:
            b = encode_value(value[n], k, address_of)
            out[o:o + len(b)] = b
        return bytes(out)
    if kind[0] in ("named", "bits"):
        return _struct.pack("<" + kind[1], number(value, kind))
    if kind[0] == "bytes":
        return bytes(value)
    if kind[0] == "text":
        raw = value.encode("ascii") if isinstance(value, str) else bytes(value)
        return raw + bytes(kind[1] - len(raw))
    if kind[0] == "arr":
        return b"".join(encode_value(v, kind[1], address_of) for v in value)
    if kind[0] == "union":
        return encode_value(value, kind[2], address_of)
    if kind[0] == "word":
        return _struct.pack("<I", (value if isinstance(value, int) else address_of(value)) & 0xFFFFFFFF)
    return _struct.pack("<I", value if isinstance(value, int) else address_of(value))


def c_value(value, kind, pointer, layout: str = "") -> str:
    """A value as a C initializer; `pointer(name)` writes a named pointer. GCC 2.8.1 takes designators but not
    chained ones, so a union is `.field = { .member = ... }`."""
    if isinstance(kind, str) and kind in FORMATS:
        return f"0x{value:X}" if kind == "I" and value >= 0x80000000 else str(value)
    if isinstance(kind, str):
        fields = list(_fields(kind))
        if fields[0][0] == "":
            return c_value(value, fields[0][1], pointer, kind)
        parts, value = [], _filled(value, kind)
        for n, k, _o in fields:
            if isinstance(k, tuple) and k[0] == "word":
                v = value[n]
                member = k[2] if isinstance(v, str) else k[1]
                parts.append(f".{n} = {{ .{member} = {pointer(v) if isinstance(v, str) else v} }}")
                continue
            if isinstance(k, tuple) and k[0] == "union":
                member = k[1]
                if kind == "SpriteView" and n == "sources" and value[n] == value["batches"]:
                    member = "empty"                     # no sources: the slot keeps the view's batch list
                parts.append(f".{n} = {{ .{member} = {c_value(value[n], k[2], pointer)} }}")
            else:
                parts.append(f".{n} = {c_value(value[n], k, pointer)}")
        return "{ " + ", ".join(parts) + " }"
    if kind[0] in ("named", "bits"):
        return str(number(value, kind))
    if kind[0] == "text" and isinstance(value, str):
        return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'
    if kind[0] in ("bytes", "arr", "text"):
        inner = kind[1] if kind[0] == "arr" else "B"
        return "{ " + ", ".join(c_value(v, inner, pointer) for v in value) + " }"
    if kind[0] == "union":
        return c_value(value, kind[2], pointer)
    if value == 0:
        return "NULL"
    if isinstance(value, int):
        return f"(void*)0x{value:08X}"                   # an address nothing names (stale bytes)
    return pointer(value)


def pointer_targets(value, kind):
    """(name, target kind) of every named pointer in a value whose pointers are names."""
    if isinstance(kind, str) and kind in FORMATS:
        return
    if isinstance(kind, str):
        fields = list(_fields(kind))
        if fields[0][0] == "":
            yield from pointer_targets(value, fields[0][1])
            return
        value = _filled(value, kind)
        for n, k, _o in fields:
            if kind == "SpriteView" and n == "sources" and value[n] == value["batches"]:
                continue                                  # no sources: the slot keeps the view's batch list
            yield from pointer_targets(value[n], k)
        return
    if kind[0] in ("bytes", "text", "named", "bits"):
        return
    if kind[0] == "arr":
        for v in value:
            yield from pointer_targets(v, kind[1])
        return
    if kind[0] == "union":
        yield from pointer_targets(value, kind[2])
        return
    if kind[0] == "word":
        if isinstance(value, str) and len(kind) > 3:
            yield value, kind[3]
        return
    if isinstance(value, str) and kind[1] != "ref":
        yield value, kind[1]


# --- events ----------------------------------------------------------------------------------------------------
# A word that is a number or a pointer by name: ("word", number member, pointer member) - a name where the word is an
# address something names, else the number (event operands, an animation request's bank or clip table).
LAYOUTS.update({
    "EvsCommand": ("EvsCommand", [("opcode", "i")] + [(f"operand{i}", ("word", "value", "storage")) for i in range(5)]),
    "AnimationPlayRequest": ("AnimationPlayRequest", [
        ("source", ("word", "index", "sets")), ("animationId", "i"), ("blend", "i"), ("blendFrames", "i"),
        ("enableWorldCollision", "i")]),
    "ActorTransform": ("ActorTransform", [("pos", ("arr", "i", 4)), ("rot", ("arr", "h", 4))]),
    "ActorCommandLoc": ("", [("stage", "B"), ("area", "B")]),
    "ActorCommand": ("ActorCommand", [("context", ("union", "loc", "ActorCommandLoc")), ("command", "H")]),
    "AnimationBankCopyRequest": ("AnimationBankCopyRequest", [("sets", ("ptr", "ref")), ("count", "i")]),
    "EvsSceneKey": ("EvsSceneKey", [("group", "H"), ("streamId", "H"), ("subId", "H")]),
    "GameActorMoveAnim": ("GameActorMoveAnim", [("approachAnimId", "i"), ("arrivalAnimId", "i")]),
    "ActorMotionWalkAnim": ("ActorMotionWalkAnim", [("animationId", "i"), ("nextAnimId", "B")]),   # three bytes of padding
})
for _layout, _size in (("EvsCommand", 0x18), ("AnimationPlayRequest", 0x14), ("ActorTransform", 0x18), ("ActorCommand", 4),
                       ("EvsSceneKey", 6), ("ActorMotionWalkAnim", 8)):
    assert size(_layout) == _size, _layout
MODULE_OF.update({k: "events" for k in ("EvsCommand", "AnimationPlayRequest", "ActorTransform", "ActorCommand",
                                         "AnimationBankCopyRequest", "EvsSceneKey", "GameActorMoveAnim", "ActorMotionWalkAnim")})


# --- the stage overlay ---------------------------------------------------------------------------------------------
# Its per-area tables point into the rooms (one entry an area, the room's object of that role) and the wrappers
# gameplay reaches five of them through; its own data: the placement lists, area objects, the map, image memory, music
# and battle rewards.
AREA_TABLES = {"WorldCollisionRoomResources": "WorldCollisionRoomResources*", "ViewMapPtr": "u8**",
               "ViewCount": "ViewCount*", "WorldCoordRoomLighting": "WorldCoordRoomLighting*", "ViewCamera": "ViewCamera*",
               "SpriteView": "SpriteView*", "DirectionWarpEntry": "DirectionWarpEntry*",
               "SurfacePtr": "WorldCollisionSurfaceProperties**"}
LAYOUTS.update({f"AreaTable{k}": (ctype, [("", ("ptr", "ref"))]) for k, ctype in AREA_TABLES.items()})
LAYOUTS.update({
    "WorldCollisionStageResources": ("WorldCollisionStageResources", [("areaRooms", ("ptr", "AreaTableWorldCollisionRoomResources"))]),
    "ViewIndexTable": ("ViewIndexTable", [("viewMaps", ("ptr", "AreaTableViewMapPtr"))]),
    "ViewCountTable": ("ViewCountTable", [("viewCounts", ("ptr", "AreaTableViewCount"))]),
    "ViewCameraTable": ("ViewCameraTable", [("cameras", ("ptr", "AreaTableViewCamera"))]),
    "SpriteAreaTable": ("SpriteAreaTable", [("areaViews", ("ptr", "AreaTableSpriteView"))]),
    "AreaObjectRoom": ("AreaObjectRoom", [("places", ("word", "sentinel", "list", "AreaObjectPlace")), ("spawns", ("ptr", "ref"))]),
    "AreaObjectPlace": ("AreaObjectPlace", [
        ("flagIndex", "H"), ("kind", "H"), ("placeKeyHigh", "H"), ("state", "H"), ("x", "h"), ("y", "h"), ("z", "h"),
        ("yaw", "H")]),
    "MenuMapArea": ("MenuMapArea", [("originX", "h"), ("originZ", "h"), ("mapX", "H"), ("mapY", "H"), ("scaleX", "h"),
                                    ("scaleZ", "h"), ("page", "H")]),
    "MenuMapAreaShape": ("MenuMapAreaShape", [("model", ("ptr", "ref")), ("page", "B"), ("pairedArea", "B")]),
    "MenuMapMarker": ("MenuMapMarker", [("page", "B"), ("area", "B"), ("x", "h"), ("y", "h")]),
    "MenuMapIcon": ("MenuMapIcon", [("page", "B"), ("area", "B"), ("kind", "B"), ("condition", "B"), ("x", "h"), ("y", "h")]),
    "MenuMapAreaName": ("MenuMapAreaName", [("text", ("text", 0x20))]),
    "GfxImageSlot": ("GfxImageSlot", [("regionBase", ("ptr", "ref")), ("byteExtent", "i")]),
    "StageMusicEntry": ("StageMusicEntry", [("sequenceId", ("named", "B", SEQUENCES)), ("startMode", "B")]),
    "InventoryBattleReward": ("InventoryBattleReward", [("areaLayoutKey", "i"), ("items", ("arr", "H", 4))]),
})
for _layout, _size in (("AreaObjectRoom", 8), ("AreaObjectPlace", 16), ("MenuMapArea", 14), ("MenuMapAreaShape", 8),
                       ("MenuMapMarker", 6), ("MenuMapIcon", 8), ("MenuMapAreaName", 0x20), ("GfxImageSlot", 8),
                       ("StageMusicEntry", 2), ("InventoryBattleReward", 12)):
    assert size(_layout) == _size, _layout
MODULE_OF.update({**{f"AreaTable{k}": "tables" for k in AREA_TABLES},
                  **{k: "tables" for k in ("WorldCollisionStageResources", "ViewIndexTable", "ViewCountTable",
                                           "ViewCameraTable", "SpriteAreaTable")},
                  "AreaObjectRoom": "objects", "AreaObjectPlace": "objects",
                  **{k: "map" for k in ("MenuMapArea", "MenuMapAreaShape", "MenuMapMarker", "MenuMapIcon", "MenuMapAreaName")},
                  "GfxImageSlot": "memory", "StageMusicEntry": "music", "InventoryBattleReward": "rewards"})


# --- room-specific tables ----------------------------------------------------------------------------------------
# A jukebox's track lists (include/rooms/rooms_shared_8018055c.h): each row a sequence of the MIDI library and the
# label it is drawn with, which stays in C with the room's other UI strings.
LAYOUTS.update({
    "JukeboxTrack": ("JukeboxTrack", [("sequenceId", ("named", "i", SEQUENCES)), ("name", ("ptr", "ref"))]),
    "JukeboxTrackLists": ("JukeboxTrackLists", [("lists", ("arr", ("ptr", "JukeboxTrack"), 10))]),
})
# The shooting gallery's courses (mist_shooting_gallery_private.h): a wave script's records, by frame or marker, each
# target's spawn argument as its fields, and each target kind's score.
MIST_SHOOTING_GALLERY_TARGET_KINDS = {
    0: "bacterium", 1: "bear", 2: "monkey", 3: "rat", 4: "scorpion", 5: "snake", 6: "spider", 7: "bee", 8: "crow",
    9: "woman", 10: "yellow_target", 11: "brown_target", 12: "red_target"}
LAYOUTS.update({
    "_MistShootingGallerySpawn": ("_MistShootingGallerySpawn", [
        ("frame", ("named", "H", {0xFFF1: "wait_clear", 0xFFFF: "end"})),
        ("spawnArgLo", ("bits", "h", [("kind", 0, 4, MIST_SHOOTING_GALLERY_TARGET_KINDS),
                                      ("behavior", 12, 4, {0: "path", 1: "turned", 2: "demo"})])),
        ("spawnArgHi", ("bits", "h", [("path", 0, 8, None), ("hold", 8, 4, None), ("rotating", 12, 1, None),
                                      ("target_flag", 13, 1, None), ("near_player", 14, 1, None)])),
        ("x", "h"), ("y", "h"), ("z", "h")]),
    "_MistShootingGalleryTargetScore": ("_MistShootingGalleryTargetScore", [("points", "i"), ("name", ("ptr", "ref"))]),
})
for _layout, _size in (("JukeboxTrack", 8), ("JukeboxTrackLists", 0x28), ("_MistShootingGallerySpawn", 0xC),
                       ("_MistShootingGalleryTargetScore", 8)):
    assert size(_layout) == _size, _layout
MODULE_OF.update({"JukeboxTrack": "jukebox", "JukeboxTrackLists": "jukebox", "_MistShootingGallerySpawn": "waves",
                  "_MistShootingGalleryTargetScore": "scores"})
