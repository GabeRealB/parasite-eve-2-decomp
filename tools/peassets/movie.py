"""Movies: the `movie` asset type.

A movie is a stream that a stage folder's streaming list names (format.StreamingListMovieEntry). The room plays it by
its stream id - the view it plays in, or 100 and up for a scene - and, where several share an id, by its sub id. A
declaration locates it there:

    folder = "<the stage folder, as asset_data.TREE names it>"
    stream = <stream id>
    sub = <sub id>                                 where it is not 0

extract_movies.py writes a declared movie under its name (`<name>.mp4` and `.json` in its group, `raw/movie/<name>.str`);
an undeclared one keeps the stem of its place on the disc. Its frame size is what the folder's entry states - the size
the game reads (stream.c) - in the extraction's `stage<N>/<folder>/streaming.json`, which every extraction mode writes,
so nothing that sizes memory by a movie needs the movie itself extracted.
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any


def _folder(rec: dict[str, Any], tree: dict) -> tuple[int, dict] | None:
    """(stage, folder node) of the folder a movie's declaration names."""
    return next(((stage, t["folders"][rec["folder"]]) for stage, t in tree.items()
                 if rec.get("folder") in t.get("folders", {})), None)


def located(assets: dict[str, dict[str, Any]], tree: dict) -> dict[tuple[int, int, int, int], str]:
    """{(stage, folder id, stream id, sub id): name} of the declared movies."""
    out = {}
    for key, rec in assets.items():
        if rec.get("type") == "movie" and (place := _folder(rec, tree)):
            out[(place[0], place[1]["id"], rec["stream"], rec.get("sub", 0))] = key
    return out


def entry(name: str, assets: dict[str, dict[str, Any]], tree: dict, assets_root: Path) -> dict[str, Any]:
    """The streaming-list entry a declared movie is, as the extraction wrote it."""
    rec = assets[name]
    stage, _node = _folder(rec, tree)
    entries = json.loads((Path(assets_root) / f"stage{stage}" / rec["folder"] / "streaming.json").read_text())
    return next(e for e in entries if e["stream_type"] == "movie" and e["stream_id"] == rec["stream"]
                and e["stream_sub_id"] == rec.get("sub", 0))


def frame_size(name: str, assets: dict[str, dict[str, Any]], tree: dict, assets_root: Path) -> tuple[int, int]:
    """(width, height) in pixels of a declared movie's frames, as its folder's entry states them."""
    e = entry(name, assets, tree, assets_root)
    return e["picture_width"], e["picture_height"]


def check(key: str, rec: dict[str, Any], assets: dict[str, dict[str, Any]], tree: dict) -> list[str]:
    """A movie names a stage folder and a stream id (a sub id where it is not 0); no two name one stream."""
    if _folder(rec, tree) is None:
        return [f"{key}: folder = a stage folder, as asset_data.TREE names it"]
    if not isinstance(rec.get("stream"), int) or not isinstance(rec.get("sub", 0), int) or rec.get("sub") == 0:
        return [f"{key}: stream = its stream id, and sub = its sub id where that is not 0"]
    return [f"{key}: {other} is the same stream" for other, o in assets.items()
            if other > key and o.get("type") == "movie" and (o.get("folder"), o.get("stream"), o.get("sub", 0))
            == (rec["folder"], rec["stream"], rec.get("sub", 0))]
