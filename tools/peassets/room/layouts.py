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
    if kind[0] == "bytes":
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
    if kind[0] == "bytes":
        return kind[1]
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


# --- values ----------------------------------------------------------------------------------------------------
import re as _re
import struct as _struct


def decode_value(data: bytes, off: int, kind, refs: list | None = None, field: str = ""):
    """The value of `kind` at `off`, pointers as addresses; `refs` collects (field, address, target kind)."""
    if isinstance(kind, str) and kind in FORMATS:
        return _struct.unpack_from("<" + kind, data, off)[0]
    if isinstance(kind, str):
        fields = list(_fields(kind))
        if fields[0][0] == "":
            return decode_value(data, off, fields[0][1], refs, field)
        return {n: decode_value(data, off + o, k, refs, n) for n, k, o in fields}
    if kind[0] == "bytes":
        return list(data[off:off + kind[1]])
    if kind[0] == "arr":
        return [decode_value(data, off + i * size(kind[1]), kind[1], refs, field) for i in range(kind[2])]
    if kind[0] == "union":
        return decode_value(data, off, kind[2], refs, field)
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
        return {n: named(value[n], k, name_of) for n, k, _o in fields}
    if kind[0] == "bytes":
        return value
    if kind[0] == "arr":
        return [named(v, kind[1], name_of) for v in value]
    if kind[0] == "union":
        return named(value, kind[2], name_of)
    return name_of(value)


def encode_value(value, kind, address_of) -> bytes:
    """The bytes of a value whose pointers are names (`address_of(name)`)."""
    if isinstance(kind, str) and kind in FORMATS:
        return _struct.pack("<" + kind, value)
    if isinstance(kind, str):
        fields = list(_fields(kind))
        if fields[0][0] == "":
            return encode_value(value, fields[0][1], address_of)
        out = bytearray(size(kind))
        for n, k, o in fields:
            b = encode_value(value[n], k, address_of)
            out[o:o + len(b)] = b
        return bytes(out)
    if kind[0] == "bytes":
        return bytes(value)
    if kind[0] == "arr":
        return b"".join(encode_value(v, kind[1], address_of) for v in value)
    if kind[0] == "union":
        return encode_value(value, kind[2], address_of)
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
        parts = []
        for n, k, _o in fields:
            if isinstance(k, tuple) and k[0] == "union":
                member = k[1]
                if kind == "SpriteView" and n == "sources" and value[n] == value["batches"]:
                    member = "empty"                     # no sources: the slot keeps the view's batch list
                parts.append(f".{n} = {{ .{member} = {c_value(value[n], k[2], pointer)} }}")
            else:
                parts.append(f".{n} = {c_value(value[n], k, pointer)}")
        return "{ " + ", ".join(parts) + " }"
    if kind[0] in ("bytes", "arr"):
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
        for n, k, _o in fields:
            if kind == "SpriteView" and n == "sources" and value[n] == value["batches"]:
                continue                                  # no sources: the slot keeps the view's batch list
            yield from pointer_targets(value[n], k)
        return
    if kind[0] == "bytes":
        return
    if kind[0] == "arr":
        for v in value:
            yield from pointer_targets(v, kind[1])
        return
    if kind[0] == "union":
        yield from pointer_targets(value, kind[2])
        return
    if isinstance(value, str) and kind[1] != "ref":
        yield value, kind[1]
