"""Models: a model's bytes as a self-contained authored model, written as USD with its bindings.

A declared model (type `model`, in a group) is placed whole by its package - arrays, stream and record - and names
its texture `slots`; a `binding` fills them with texture variants. Every other model is a `tmd` record: its face
stream, located in its package and kept raw for the build until it is declared.

    codec.py      bytes <-> authored model (layouts, raw, general and authored layers), byte-exact for all 790
    usd.py        the authored model and its bindings written as USD (UsdSkel mesh, materials per slot)
    declared.py   the models the asset table declares, decoded to USD, their bindings resolved against the files
    mesh.py       any model as a viewer draws it: posed vertices, textured faces (the asset map, the viewer)
    preview.py    every model of a family or package as plain USD, to look at
    tmd.py        model streams in packages: locating them, their TmdSource records, the names the build uses

    python3 tools/peassets/model decode [--assets assets/USA] [--only NAME] [--usdc]
    python3 tools/peassets/model preview FAMILY_OR_PACKAGE [--out build/models]

Notes: local/authoring/model.md.
"""
