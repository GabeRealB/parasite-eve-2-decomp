#!/usr/bin/env python3
"""Locate the files one matching attempt produced.

A matching session builds candidate after candidate in a scratch directory, and
every tool here works from one of those attempts: the normalised disassembly of
both sides, the compiler's own assembly with its uid annotations, and the dumps
written beside the preprocessed source. This module knows where those are so
the rest does not have to.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re


@dataclass(frozen=True)
class Attempt:
    """One candidate build inside a scratch directory."""

    directory: Path
    stem: str
    function: str | None = None

    @property
    def source(self) -> Path:
        return self.directory / f"{self.stem}.c"

    @property
    def preprocessed(self) -> Path:
        return self.directory / f"{self.stem}.i"

    @property
    def cc1_assembly(self) -> Path:
        """Compiler output, with uid and pattern annotations."""
        return self.directory / f"{self.stem}.s"

    @property
    def ours(self) -> Path:
        """Normalised disassembly of what this attempt assembled to."""
        return self.directory / f"{self.stem}_object_dump_normalized.s"

    @property
    def target(self) -> Path:
        """Normalised disassembly of the bytes being matched."""
        return self.directory / "target_object_dump_normalized.s"

    @property
    def score(self) -> Path:
        return self.directory / f"{self.stem}.score.json"

    def dump(self, pass_name: str) -> Path:
        return self.directory / f"{self.stem}.i.{pass_name}"

    @property
    def complete(self) -> bool:
        return self.ours.is_file() and self.target.is_file()

    @classmethod
    def discover(cls, directory: Path | str, stem: str | None = None) -> "Attempt":
        """The named attempt, or the most recently built one that is complete.

        Sessions number their candidates rather than overwriting them, so the
        newest complete build is almost always the one a question is about.
        """
        directory = Path(directory)
        if stem:
            if Path(stem).name != stem:
                raise ValueError("attempt stem must be a filename without a directory")
            attempt = cls(directory, stem)
            if not attempt.complete:
                raise FileNotFoundError(f"missing target or candidate assembly for {attempt}")
            return attempt
        candidates = [
            cls(directory, path.name[: -len("_object_dump_normalized.s")])
            for path in directory.glob("*_object_dump_normalized.s")
            if path.name != "target_object_dump_normalized.s"
        ]
        candidates = [a for a in candidates if a.complete]
        if not candidates:
            raise FileNotFoundError(f"no completed attempt in {directory}")
        return max(candidates, key=lambda a: (a.ours.stat().st_mtime, _number(a.stem)))

    def __str__(self) -> str:
        return f"{self.directory.name}/{self.stem}"


def _number(stem: str) -> int:
    match = re.search(r"(\d+)$", stem)
    return int(match.group(1)) if match else -1
