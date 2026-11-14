"""The releases the build knows: each one's disc images and executables, by the names the discs give them."""
from __future__ import annotations

from dataclasses import dataclass


@dataclass
class GameVersionMetadata:
    name_disk1: str
    name_disk2: str
    version_dir: str
    exe_disk1: str
    exe_disk2: str


@dataclass
class GameVersionInfo:
    idx: int
    version_name: str
    metadata: GameVersionMetadata


GAME_VERSIONS = [
    GameVersionInfo(
        0,
        "USA",
        GameVersionMetadata(
            "SLUS-01042",
            "SLUS-01055",
            "USA",
            "SLUS_010.42",
            "SLUS_010.55",
        ),
    ),
    GameVersionInfo(
        1,
        "JAP",
        GameVersionMetadata(
            "SLPS-02480",
            "SLPS-02481",
            "JAP",
            "SLPS_024.80",
            "SLPS_024.81",
        ),
    ),
]


def named(name: str) -> GameVersionInfo:
    """A release by its name (`USA`), whatever its case."""
    for info in GAME_VERSIONS:
        if info.version_name == name.upper():
            return info
    raise SystemExit(f"no release {name}: {', '.join(v.version_name for v in GAME_VERSIONS)}")
