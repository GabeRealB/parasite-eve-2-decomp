"""Animations: exported animation sets as authored clips, written as USD, and the banks that bind them to slots.

A clip (type `clip`) is one `AnimationSet` with its pose banks, records and track starts; a bank
(`animation_bank`) binds clips to the slots of a schema (`animation_schema`), the indices the code plays them by.
Every other package's sets are still one `animation` block, kept raw until they are declared.

    codec.py      bytes <-> clip (tracks of keys, loops, cue bits), byte-exact for all 4500 declared sets
    usd.py        a clip as a USD SkelAnimation (per-frame samples to look at, the exact keys beside them), and back
    declared.py   the clips and banks the asset table declares: decoded to USD, encoded from it, banks resolved

    python3 tools/peassets/animation decode|encode|check [--assets assets/USA] [--only NAME] [--out DIR]

Notes: local/authoring/animation.md.
"""
