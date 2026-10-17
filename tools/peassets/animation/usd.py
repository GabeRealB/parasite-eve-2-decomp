"""Write a clip (codec.py's form) as a USD skeletal animation, and read it back.

The clip is a `UsdSkelAnimation`: per-frame joint samples to look at (Blender plays them on a skeleton bound to it),
and the exact keys beside them, which `read_clip` reads:

    keyCounts int[]      keys per track
    keyRotations int3[]  each key's Euler angles, 4096 units per turn (tracks in order, keys in order)
    keyTranslations int3[]  each key's translation, (0, 0, 0) on a track without translation
    keyFrames int[]      frames to blend into each key
    keyCues int[]        each key's cue bits
    keyVariants int[]    each key's pose variant (codec.py)
    translated bool[]    per track: stored with translation
    loops int[]          per track: the key a finished track jumps back to, -1 when it ends

Display samples: a track reaches key k after the frames of keys 0..k and holds its last key; angles blend linearly,
as the game blends them, and turn into quaternions by the game's order (M = Rx Ry Rz). A track without translation
keeps its joint's rest translation, which the clip's rig gives (zero without one). Joints are named by the rig's
joint paths (root/pelvis/...; part0/part1/... where it names none), as its file does; 30 frames per second. The
file also references the rig (`/Preview/Rig`, playing `/Clip`), so it opens as the rig moving.
"""
from __future__ import annotations

import math
from pathlib import Path

from pxr import Gf, Sdf, Usd, UsdGeom, UsdSkel, Vt

FPS = 30
TURN = 4096
EXACT = ("keyCounts", "keyRotations", "keyTranslations", "keyFrames", "keyCues", "keyVariants", "translated", "loops")


def joint_paths(parents: list[int], names: list[str] | None = None) -> list[str]:
    paths: dict[int, str] = {}

    def path(i: int) -> str:
        if i not in paths:
            p = parents[i]
            own = names[i] if names else f"part{i}"
            paths[i] = own if p == i else f"{path(p)}/{own}"
        return paths[i]

    return [path(i) for i in range(len(parents))]


def _quaternion(rotation: list[float]) -> Gf.Quatf:
    """M = Rx Ry Rz (column vectors), angles in 4096 units per turn."""
    qx, qy, qz = (Gf.Rotation(axis, a * 360.0 / TURN).GetQuat()
                  for axis, a in zip((Gf.Vec3d.XAxis(), Gf.Vec3d.YAxis(), Gf.Vec3d.ZAxis()), rotation))
    q = qx * qy * qz
    return Gf.Quatf(q.GetReal(), *q.GetImaginary())


def _sample(keys: list[dict], frame: int, field: str) -> list[float]:
    t = 0
    previous = keys[0][field]
    for key in keys:
        if frame <= t + key["frames"]:
            f = (frame - t) / key["frames"]
            return [a + (b - a) * f for a, b in zip(previous, key[field])]
        t += key["frames"]
        previous = key[field]
    return list(previous)


def write_clip(clip: dict, path: Path, label: str, rig: list[dict] | None = None, names: list[str] | None = None,
               rig_file: str | None = None) -> None:
    """Write `clip`; `rig` is its rig's joints (parent, rest translation), for the joint paths, with the rig's joint
    `names`, and the rest translations of tracks without translation; `rig_file` is the rig's file relative to
    `path`."""
    tracks = clip["tracks"]
    parents = [p["parent"] for p in rig] if rig else list(range(len(tracks)))
    if rig and len(rig) != len(tracks):
        raise ValueError(f"{len(tracks)} tracks for a rig of {len(rig)} joints")
    stage = Usd.Stage.CreateNew(str(path))
    UsdGeom.SetStageUpAxis(stage, UsdGeom.Tokens.y)             # the units and axis of the models and rigs
    UsdGeom.SetStageMetersPerUnit(stage, 0.001)
    anim = UsdSkel.Animation.Define(stage, "/Clip")
    stage.SetDefaultPrim(anim.GetPrim())
    anim.GetPrim().SetCustomDataByKey("source", label)
    length = max(sum(k["frames"] for k in t["keys"]) for t in tracks)
    stage.SetStartTimeCode(0)
    stage.SetEndTimeCode(length)
    stage.SetTimeCodesPerSecond(FPS)
    stage.SetFramesPerSecond(FPS)
    anim.CreateJointsAttr(Vt.TokenArray(joint_paths(parents, names)))
    anim.CreateScalesAttr(Vt.Vec3hArray([Gf.Vec3h(1, 1, 1)] * len(tracks)))
    rest = [p["translation"] for p in rig] if rig else [[0, 0, 0]] * len(tracks)
    rotations, translations = anim.CreateRotationsAttr(), anim.CreateTranslationsAttr()
    for frame in range(length + 1):
        rotations.Set(Vt.QuatfArray([_quaternion(_sample(t["keys"], frame, "rotation")) for t in tracks]), frame)
        translations.Set(Vt.Vec3fArray([Gf.Vec3f(*(_sample(t["keys"], frame, "translation") if "translation" in t["keys"][0]
                                                   else rest[i])) for i, t in enumerate(tracks)]), frame)

    keys = [k for t in tracks for k in t["keys"]]
    prim = anim.GetPrim()
    values = {
        "keyCounts": (Sdf.ValueTypeNames.IntArray, [len(t["keys"]) for t in tracks]),
        "keyRotations": (Sdf.ValueTypeNames.Int3Array, [Gf.Vec3i(*k["rotation"]) for k in keys]),
        "keyTranslations": (Sdf.ValueTypeNames.Int3Array, [Gf.Vec3i(*k.get("translation", (0, 0, 0))) for k in keys]),
        "keyFrames": (Sdf.ValueTypeNames.IntArray, [k["frames"] for k in keys]),
        "keyCues": (Sdf.ValueTypeNames.IntArray, [k.get("cue", 0) for k in keys]),
        "keyVariants": (Sdf.ValueTypeNames.IntArray, [k.get("variant", 0) for k in keys]),
        "translated": (Sdf.ValueTypeNames.BoolArray, ["translation" in t["keys"][0] for t in tracks]),
        "loops": (Sdf.ValueTypeNames.IntArray, [t.get("loop", -1) for t in tracks]),
    }
    for name in EXACT:
        kind, value = values[name]
        prim.CreateAttribute(name, kind, custom=True).Set(value)
    if rig_file:
        UsdSkel.Root.Define(stage, "/Preview").AddRotateXOp().Set(180.0)      # upright, as model/usd.py shows models
        skel = UsdSkel.Skeleton.Define(stage, "/Preview/Rig")
        skel.GetPrim().GetReferences().AddReference(rig_file)
        UsdSkel.BindingAPI.Apply(skel.GetPrim()).CreateAnimationSourceRel().SetTargets([anim.GetPath()])
    stage.GetRootLayer().Save()


def read_clip(path: Path) -> dict:
    """The clip a `write_clip` file holds; raises ValueError where the file cannot be one."""
    stage = Usd.Stage.Open(str(path))
    prim = stage.GetDefaultPrim()
    if not prim or not prim.IsA(UsdSkel.Animation) or any(not prim.GetAttribute(n) for n in EXACT):
        raise ValueError(f"{path}: not a clip (a SkelAnimation default prim with {', '.join(EXACT)})")
    v = {n: list(prim.GetAttribute(n).Get()) for n in EXACT}
    if len(v["translated"]) != len(v["keyCounts"]) or len(v["loops"]) != len(v["keyCounts"]):
        raise ValueError(f"{path}: per-track arrays disagree in length")
    tracks, k = [], 0
    for count, translated, loop in zip(v["keyCounts"], v["translated"], v["loops"]):
        keys = []
        for _ in range(count):
            key = {"rotation": list(v["keyRotations"][k])}
            if translated:
                key["translation"] = list(v["keyTranslations"][k])
            key["frames"] = v["keyFrames"][k]
            if v["keyCues"][k]:
                key["cue"] = v["keyCues"][k]
            if v["keyVariants"][k]:
                key["variant"] = v["keyVariants"][k]
            keys.append(key)
            k += 1
        track = {"keys": keys}
        if loop >= 0:
            track["loop"] = loop
        tracks.append(track)
    if k != len(v["keyFrames"]):
        raise ValueError(f"{path}: keyCounts cover {k} keys, the key arrays hold {len(v['keyFrames'])}")
    return {"tracks": tracks}
