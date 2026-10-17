"""Write a model (codec.py's form) and its bindings as USD.

A model is one skinned mesh bound rigidly to a skeleton: each part is a joint with its rest transform, each vertex
belongs to one part with weight 1, so faces spanning parts need nothing special. Materials are GeomSubsets bound to
Material prims; a textured material names its slot in an input and leaves the texture open.

Exact values sit beside display values so an importer can recover the model: `texel` UVs (in the slot's texels),
`normalIndex`, part-local `sourceNormals`, uniform `shading` and `part`. The game's Y axis points down; the root
rotates 180 degrees about X so models show upright without altering the data.

A binding is a layer over the model: it connects each slot's material to its texture image and adds `st`, the UVs
normalized to that image.
"""
from __future__ import annotations

from pathlib import Path

from pxr import Gf, Sdf, Usd, UsdGeom, UsdShade, UsdSkel, Vt


def _worlds(parts):
    """Rest-pose model-space transform of each part as (rotation rows, translation), composed up the parents."""
    out = [None] * len(parts)

    def get(i):
        if out[i] is None:
            p = parts[i]
            R = [[float(x) for x in row] for row in p["rotation"]]
            t = [float(x) for x in p["translation"]]
            if p["parent"] != i:
                PR, Pt = get(p["parent"])
                R = [[sum(PR[r][k] * R[k][c] for k in range(3)) for c in range(3)] for r in range(3)]
                t = [sum(PR[r][k] * t[k] for k in range(3)) + Pt[r] for r in range(3)]
            out[i] = (R, t)
        return out[i]

    for i in range(len(parts)):
        get(i)
    return out


def _apply(Rt, v):
    R, t = Rt
    return [sum(R[r][k] * v[k] for k in range(3)) + t[r] for r in range(3)]


def _matrix(R, t):
    """Column-vector (v' = R v + t) to USD's row-vector convention."""
    return Gf.Matrix4d(R[0][0], R[1][0], R[2][0], 0, R[0][1], R[1][1], R[2][1], 0,
                       R[0][2], R[1][2], R[2][2], 0, t[0], t[1], t[2], 1)


def _joint_paths(parts):
    names = {}

    def name(i):
        if i not in names:
            p = parts[i]["parent"]
            names[i] = f"part{i}" if p == i else f"{name(p)}/part{i}"
        return names[i]

    return [name(i) for i in range(len(parts))]


def write_model(model: dict, path: Path, label: str) -> None:
    stage = Usd.Stage.CreateNew(str(path))
    UsdGeom.SetStageUpAxis(stage, UsdGeom.Tokens.y)
    UsdGeom.SetStageMetersPerUnit(stage, 0.001)
    root = UsdGeom.Xform.Define(stage, "/Model")
    root.AddRotateXOp().Set(180.0)
    stage.SetDefaultPrim(root.GetPrim())
    rp = root.GetPrim()
    rp.SetCustomDataByKey("source", label)
    if "effect" in model:
        rp.CreateAttribute("effect", Sdf.ValueTypeNames.Int).Set(model["effect"])
    if "reflection" in model:
        rp.CreateAttribute("reflection", Sdf.ValueTypeNames.Token).Set(model["reflection"])
    rp.CreateAttribute("slots", Sdf.ValueTypeNames.TokenArray).Set(Vt.TokenArray(model["slots"]))

    parts = model["parts"]
    worlds = _worlds(parts)
    skel_root = UsdSkel.Root.Define(stage, "/Model/Rig")
    skel = UsdSkel.Skeleton.Define(stage, "/Model/Rig/Skeleton")
    skel.CreateJointsAttr(Vt.TokenArray(_joint_paths(parts)))
    skel.CreateBindTransformsAttr(Vt.Matrix4dArray([_matrix(*w) for w in worlds]))
    skel.CreateRestTransformsAttr(Vt.Matrix4dArray([_matrix([[float(x) for x in r] for r in p["rotation"]],
                                                            [float(x) for x in p["translation"]]) for p in parts]))

    points, joint = [], []
    for i, p in enumerate(parts):
        for v in p["vertices"]:
            points.append(_apply(worlds[i], v)); joint.append(i)
    loose = model.get("loose_vertices", [])
    for v in loose:
        points.append([float(x) for x in v]); joint.append(0)

    mesh = UsdGeom.Mesh.Define(stage, "/Model/Rig/Mesh")
    mesh.CreatePointsAttr(Vt.Vec3fArray([Gf.Vec3f(*p) for p in points]))
    counts, indices, normals, nidx, colors, texel, shading, partof = [], [], [], [], [], [], [], []
    n_local = model["normals"]
    for f in model["faces"]:
        counts.append(len(f["corners"]))
        shading.append(f["shading"]); partof.append(f.get("part", -1))
        for c in f["corners"]:
            v = c["vertex"]; indices.append(v)
            ni = c.get("normal", f.get("normal", -1))
            nidx.append(ni)
            if 0 <= ni < len(n_local):
                R = worlds[joint[v]][0]; n = n_local[ni]
                normals.append(Gf.Vec3f(*[sum(R[r][k] * n[k] for k in range(3)) for r in range(3)]))
            else:
                normals.append(None)
            col = c.get("color", f.get("color", [255, 255, 255]))
            colors.append(Gf.Vec3f(*[x / 255 for x in col]))
            texel.append(Gf.Vec2i(*c.get("uv", [0, 0])))
    mesh.CreateFaceVertexCountsAttr(Vt.IntArray(counts))
    mesh.CreateFaceVertexIndicesAttr(Vt.IntArray(indices))
    mesh.CreateDoubleSidedAttr(True)
    pv = UsdGeom.PrimvarsAPI(mesh)
    if any(n is not None for n in normals):
        k = 0
        for cnt in counts:                  # corners with no source normal get their face's normal, for display
            if any(normals[k + j] is None for j in range(cnt)):
                a, b, c = (Gf.Vec3f(*points[indices[k + j]]) for j in range(3))
                gn = Gf.Cross(b - a, c - a)
                gn = gn.GetNormalized() if gn.GetLength() > 0 else Gf.Vec3f(0, 1, 0)
                for j in range(cnt):
                    if normals[k + j] is None:
                        normals[k + j] = gn
            k += cnt
        pv.CreatePrimvar("normals", Sdf.ValueTypeNames.Normal3fArray, UsdGeom.Tokens.faceVarying).Set(Vt.Vec3fArray(normals))
        pv.CreatePrimvar("normalIndex", Sdf.ValueTypeNames.IntArray, UsdGeom.Tokens.faceVarying).Set(Vt.IntArray(nidx))
    pv.CreatePrimvar("displayColor", Sdf.ValueTypeNames.Color3fArray, UsdGeom.Tokens.faceVarying).Set(Vt.Vec3fArray(colors))
    if model["slots"]:
        pv.CreatePrimvar("texel", Sdf.ValueTypeNames.Int2Array, UsdGeom.Tokens.faceVarying).Set(Vt.Vec2iArray(texel))
    pv.CreatePrimvar("shading", Sdf.ValueTypeNames.TokenArray, UsdGeom.Tokens.uniform).Set(Vt.TokenArray(shading))
    pv.CreatePrimvar("part", Sdf.ValueTypeNames.IntArray, UsdGeom.Tokens.uniform).Set(Vt.IntArray(partof))
    mesh.GetPrim().CreateAttribute("sourceNormals", Sdf.ValueTypeNames.Float3Array).Set(Vt.Vec3fArray([Gf.Vec3f(*n) for n in n_local]))
    if loose:
        mesh.GetPrim().CreateAttribute("looseVertexCount", Sdf.ValueTypeNames.Int).Set(len(loose))

    binder = UsdSkel.BindingAPI.Apply(mesh.GetPrim())
    binder.CreateSkeletonRel().SetTargets([skel.GetPath()])
    binder.CreateJointIndicesPrimvar(constant=False, elementSize=1).Set(Vt.IntArray(joint))
    binder.CreateJointWeightsPrimvar(constant=False, elementSize=1).Set(
        Vt.FloatArray([0.0 if i >= len(points) - len(loose) else 1.0 for i in range(len(points))]))
    binder.CreateGeomBindTransformAttr(Gf.Matrix4d(1))
    UsdSkel.BindingAPI.Apply(skel_root.GetPrim()).CreateSkeletonRel().SetTargets([skel.GetPath()])

    UsdShade.MaterialBindingAPI.Apply(mesh.GetPrim())
    for mi, mat in enumerate(model["materials"]):
        faces_of = [i for i, f in enumerate(model["faces"]) if f["material"] == mi]
        if not faces_of:
            continue
        mprim = UsdShade.Material.Define(stage, f"/Model/Materials/material{mi}")
        mprim.CreateInput("transparent", Sdf.ValueTypeNames.Bool).Set(mat["transparent"])
        if "blend" in mat:
            mprim.CreateInput("blend", Sdf.ValueTypeNames.Token).Set(mat["blend"])
        if "slot" in mat:
            mprim.CreateInput("slot", Sdf.ValueTypeNames.Token).Set(mat["slot"])
        shader = UsdShade.Shader.Define(stage, f"/Model/Materials/material{mi}/Surface")
        shader.CreateIdAttr("UsdPreviewSurface")
        shader.CreateInput("roughness", Sdf.ValueTypeNames.Float).Set(1.0)
        if mat["transparent"]:
            shader.CreateInput("opacity", Sdf.ValueTypeNames.Float).Set(0.5)
        mprim.CreateSurfaceOutput().ConnectToSource(shader.ConnectableAPI(), "surface")
        subset = UsdGeom.Subset.Define(stage, f"/Model/Rig/Mesh/material{mi}")
        subset.CreateElementTypeAttr(UsdGeom.Tokens.face)
        subset.CreateFamilyNameAttr("materialBind")
        subset.CreateIndicesAttr(Vt.IntArray(faces_of))
        UsdShade.MaterialBindingAPI.Apply(subset.GetPrim()).Bind(mprim)
    stage.GetRootLayer().Save()


def write_binding(model: dict, textures: dict, path: Path, model_file: str) -> None:
    """A layer over `model_file` that fills its slots. `textures` maps a slot to {"file": image path relative to
    this layer, "size": (width, height) in texels, "origin": (u, v) of the image within the slot's texels}."""
    stage = Usd.Stage.CreateNew(str(path))
    stage.GetRootLayer().subLayerPaths.append(model_file)
    stage.SetDefaultPrim(stage.GetPrimAtPath("/Model")) if stage.GetPrimAtPath("/Model") else None
    st = []
    for f in model["faces"]:
        mat = model["materials"][f["material"]]
        t = textures.get(mat.get("slot"))
        for c in f["corners"]:
            if t is None:
                st.append(Gf.Vec2f(0, 0)); continue
            u, v = c.get("uv", [0, 0])
            st.append(Gf.Vec2f((u - t["origin"][0] + 0.5) / t["size"][0], 1 - (v - t["origin"][1] + 0.5) / t["size"][1]))
    mesh = UsdGeom.Mesh(stage.OverridePrim("/Model/Rig/Mesh"))
    UsdGeom.PrimvarsAPI(mesh).CreatePrimvar("st", Sdf.ValueTypeNames.TexCoord2fArray, UsdGeom.Tokens.faceVarying).Set(Vt.Vec2fArray(st))
    for mi, mat in enumerate(model["materials"]):
        t = textures.get(mat.get("slot"))
        if t is None:
            continue
        base = f"/Model/Materials/material{mi}"
        reader = UsdShade.Shader.Define(stage, f"{base}/stReader")
        reader.CreateIdAttr("UsdPrimvarReader_float2")
        reader.CreateInput("varname", Sdf.ValueTypeNames.Token).Set("st")
        tex = UsdShade.Shader.Define(stage, f"{base}/Texture")
        tex.CreateIdAttr("UsdUVTexture")
        tex.CreateInput("file", Sdf.ValueTypeNames.Asset).Set(t["file"])
        tex.CreateInput("sourceColorSpace", Sdf.ValueTypeNames.Token).Set("sRGB")
        tex.CreateInput("wrapS", Sdf.ValueTypeNames.Token).Set("clamp")
        tex.CreateInput("wrapT", Sdf.ValueTypeNames.Token).Set("clamp")
        tex.CreateInput("st", Sdf.ValueTypeNames.Float2).ConnectToSource(reader.ConnectableAPI(), "result")
        surface = UsdShade.Shader(stage.OverridePrim(f"{base}/Surface"))
        surface.CreateInput("diffuseColor", Sdf.ValueTypeNames.Color3f).ConnectToSource(tex.ConnectableAPI(), "rgb")
    stage.GetRootLayer().Save()
