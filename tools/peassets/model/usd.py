"""Write a model (codec.py's form) and its bindings as USD.

A model is one skinned mesh bound rigidly to a skeleton: each part is a joint with its rest transform, each vertex
belongs to one part with weight 1, so faces spanning parts need nothing special. Materials are GeomSubsets bound to
Material prims; a textured material names its slot in an input and leaves the texture open.

Exact values sit beside display values so `read_model` can recover the model: `texel` UVs (in the slot's texels),
`normalIndex`, part-local `sourceNormals`, uniform `shading`, `part` and `colorSource` (none, face or corner: which
of `displayColor` is the face's). Faces wind clockwise about their normals, so the mesh is `leftHanded`. Vertices are not duplicated: each is its point taken back through its part's bind
transform and rounded. The game's Y axis points down; the root rotates 180 degrees about X so models show upright
without altering the data.

A binding is a layer over the model: it connects each slot's material to its texture image and adds `st`, the UVs
normalized to that image.

A skeleton states its joints and their `bindTransforms` only: the game binds every mesh in the rest pose, so each
joint's rest transform is its bind transform relative to its parent's, and is read back that way (`_read_skeleton`).
One value per joint, which is the one Blender builds its armature from; a clip animates every joint, so nothing needs
the `restTransforms` fallback. A rig is a skeleton of its own file (`write_rig`), its joints by name. A model on a
rig does not hold its skeleton: `/Model/Rig/Skeleton` references the rig's file, so the model is read, and encoded,
with the rig's joints.
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


def _joint_paths(parts, names=None):
    """Each joint's path, its ancestors' names and its own (`part<i>` without names)."""
    paths = {}

    def path(i):
        if i not in paths:
            p = parts[i]["parent"]
            own = names[i] if names else f"part{i}"
            paths[i] = own if p == i else f"{path(p)}/{own}"
        return paths[i]

    return [path(i) for i in range(len(parts))]


def _define_skeleton(stage, at: str, parts, names=None):
    skel = UsdSkel.Skeleton.Define(stage, at)
    skel.CreateJointsAttr(Vt.TokenArray(_joint_paths(parts, names)))
    skel.CreateBindTransformsAttr(Vt.Matrix4dArray([_matrix(*w) for w in _worlds(parts)]))
    return skel


def _read_skeleton(skel, path) -> list[dict]:
    """The parts (parent, rotation, translation) a skeleton's joints and bind transforms give: each joint's rest
    transform is its bind transform relative to its parent's (USD composes row vectors: bind = rest * parent bind)."""
    joints = [str(j) for j in skel.GetJointsAttr().Get() or []]
    binds = skel.GetBindTransformsAttr().Get() or []
    if not joints or len(binds) != len(joints):
        raise ValueError(f"{path}: the skeleton needs its joints and a bind transform for each")
    index = {j: i for i, j in enumerate(joints)}
    parts = []
    for i, (j, bind) in enumerate(zip(joints, binds)):
        parent = index[j.rpartition("/")[0]] if "/" in j else i
        m = bind if parent == i else bind * binds[parent].GetInverse()
        parts.append({"parent": parent, "rotation": [[m[c][r] for c in range(3)] for r in range(3)],
                      "translation": [round(m[3][r]) for r in range(3)], "vertices": []})
    return parts


def write_rig(parts: list[dict], names: list[str] | None, path: Path, label: str) -> None:
    """A rig's file: a Skeleton, the default prim, with its joints named by `names` and the parts' rest pose."""
    stage = Usd.Stage.CreateNew(str(path))
    UsdGeom.SetStageUpAxis(stage, UsdGeom.Tokens.y)
    UsdGeom.SetStageMetersPerUnit(stage, 0.001)
    skel = _define_skeleton(stage, "/Rig", parts, names)
    stage.SetDefaultPrim(skel.GetPrim())
    skel.GetPrim().SetCustomDataByKey("source", label)
    stage.GetRootLayer().Save()


def read_rig(path: Path) -> list[dict]:
    """The parts (parent, rotation, translation) of a `write_rig` file."""
    stage = Usd.Stage.Open(str(path))
    skel = UsdSkel.Skeleton(stage.GetDefaultPrim())
    if not skel:
        raise ValueError(f"{path}: not a rig (a Skeleton default prim)")
    return _read_skeleton(skel, path)


def write_model(model: dict, path: Path, label: str, rig_file: str | None = None) -> None:
    """Write `model`; on a rig, `rig_file` is the rig's file relative to `path`, which the skeleton references (the
    model's parts have to be the rig's)."""
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
    if rig_file:
        skel = UsdSkel.Skeleton.Define(stage, "/Model/Rig/Skeleton")
        skel.GetPrim().GetReferences().AddReference(rig_file)
    else:
        skel = _define_skeleton(stage, "/Model/Rig/Skeleton", parts)

    points, joint = [], []
    for i, p in enumerate(parts):
        for v in p["vertices"]:
            points.append(_apply(worlds[i], v)); joint.append(i)
    loose = model.get("loose_vertices", [])
    for v in loose:
        points.append([float(x) for x in v]); joint.append(0)

    mesh = UsdGeom.Mesh.Define(stage, "/Model/Rig/Mesh")
    mesh.CreatePointsAttr(Vt.Vec3fArray([Gf.Vec3f(*p) for p in points]))
    counts, indices, normals, nidx, colors, texel, shading, partof, source = [], [], [], [], [], [], [], [], []
    n_local = model["normals"]
    for f in model["faces"]:
        counts.append(len(f["corners"]))
        shading.append(f["shading"]); partof.append(f.get("part", -1))
        source.append("corner" if any("color" in c for c in f["corners"]) else "face" if "color" in f else "none")
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
    mesh.CreateOrientationAttr(UsdGeom.Tokens.leftHanded)      # the game's faces wind clockwise about their normals
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
    pv.CreatePrimvar("colorSource", Sdf.ValueTypeNames.TokenArray, UsdGeom.Tokens.uniform).Set(Vt.TokenArray(source))
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


def _solve(R, t, p):
    """The x with R x + t = p, for a 3x3 R (Cramer's rule; rest rotations are Q12, not exactly orthonormal)."""
    b = [p[i] - t[i] for i in range(3)]
    det = lambda m: (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                     + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]))
    d = det(R)
    return [det([[b[r] if c == k else R[r][c] for c in range(3)] for r in range(3)]) / d for k in range(3)]


def read_model(path: Path) -> dict:
    """The model a `write_model` file holds, edited or not; raises ValueError where the file cannot be one."""
    stage = Usd.Stage.Open(str(path))
    rp = stage.GetPrimAtPath("/Model")
    skel = UsdSkel.Skeleton(stage.GetPrimAtPath("/Model/Rig/Skeleton"))
    mesh = UsdGeom.Mesh(stage.GetPrimAtPath("/Model/Rig/Mesh"))
    if not (rp and skel and mesh):
        raise ValueError(f"{path}: not a model (needs /Model, /Model/Rig/Skeleton and /Model/Rig/Mesh)")
    model: dict = {}
    for key in ("effect", "reflection"):
        attr = rp.GetAttribute(key)
        if attr and attr.HasAuthoredValue():
            model[key] = attr.Get()
    model["slots"] = list(rp.GetAttribute("slots").Get() or [])

    parts = _read_skeleton(skel, path)
    worlds = _worlds(parts)

    points = mesh.GetPointsAttr().Get()
    binder = UsdSkel.BindingAPI(mesh.GetPrim())
    joint = list(binder.GetJointIndicesPrimvar().Get())
    loose = mesh.GetPrim().GetAttribute("looseVertexCount")
    loose = loose.Get() if loose and loose.HasAuthoredValue() else 0
    owned = len(points) - loose
    if any(joint[k] > joint[k + 1] for k in range(owned - 1)):
        raise ValueError(f"{path}: points are not grouped by part in part order")
    for k in range(owned):
        parts[joint[k]]["vertices"].append([round(x) for x in _solve(*worlds[joint[k]], list(points[k]))])
    if mesh.GetOrientationAttr().Get() != UsdGeom.Tokens.leftHanded:
        raise ValueError(f"{path}: faces must wind as the game's do (orientation leftHanded), in their stored corner order")
    model["parts"] = parts
    if loose:
        model["loose_vertices"] = [[round(x) for x in p] for p in points[owned:]]
    model["normals"] = [[float(x) for x in n] for n in mesh.GetPrim().GetAttribute("sourceNormals").Get()]

    pv = UsdGeom.PrimvarsAPI(mesh)
    get = lambda name: pv.GetPrimvar(name).Get() if pv.HasPrimvar(name) else None
    counts, indices = mesh.GetFaceVertexCountsAttr().Get(), mesh.GetFaceVertexIndicesAttr().Get()
    shading, partof, source = get("shading"), get("part"), get("colorSource")
    nidx, colors, texel = get("normalIndex"), get("displayColor"), get("texel")
    material_of = [None] * len(counts)
    materials = {}
    for subset in UsdGeom.Subset.GetAllGeomSubsets(mesh):
        mat, _ = UsdShade.MaterialBindingAPI(subset.GetPrim()).ComputeBoundMaterial()
        mi = int(subset.GetPrim().GetName().removeprefix("material"))
        inputs = {i.GetBaseName(): i.Get() for i in mat.GetInputs()}
        materials[mi] = {"transparent": bool(inputs["transparent"])}
        for key in ("slot", "blend"):
            if key in inputs:
                materials[mi][key] = str(inputs[key])
        for f in subset.GetIndicesAttr().Get():
            material_of[f] = mi
    if sorted(materials) != list(range(len(materials))) or None in material_of:
        raise ValueError(f"{path}: every face needs a material, numbered from material0")
    model["materials"] = [materials[i] for i in range(len(materials))]

    faces, k = [], 0
    rgb = lambda c: [round(x * 255) for x in c]
    for fi, n in enumerate(counts):
        mat = model["materials"][material_of[fi]]
        face = {"material": material_of[fi], "shading": str(shading[fi]), "corners": []}
        if partof[fi] >= 0:
            face["part"] = partof[fi]
        if face["shading"] == "flat":
            face["normal"] = nidx[k]
        if source[fi] == "face":
            face["color"] = rgb(colors[k])
        for j in range(n):
            c = {"vertex": indices[k + j]}
            if face["shading"] == "smooth":
                c["normal"] = nidx[k + j]
            if source[fi] == "corner":
                c["color"] = rgb(colors[k + j])
            if "slot" in mat:
                c["uv"] = list(texel[k + j])
            face["corners"].append(c)
        faces.append(face)
        k += n
    model["faces"] = faces
    return model


def write_binding(model: dict, textures: dict, path: Path, model_file: str) -> None:
    """A layer over `model_file` that fills its slots. `textures` maps a slot to {"file": image path relative to
    this layer, "size": (width, height) in texels, "origin": (u, v) of the image within the slot's texels}."""
    stage = Usd.Stage.CreateNew(str(path))
    UsdGeom.SetStageUpAxis(stage, UsdGeom.Tokens.y)         # a layer's own metadata: the model's units and axis
    UsdGeom.SetStageMetersPerUnit(stage, 0.001)
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
