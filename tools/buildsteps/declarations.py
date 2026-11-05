#!/usr/bin/env python3
"""The declarations, read: what the build needs of them, with nothing but the standard library.

A declaration is a table of a `<group>.toml` holding a `uid`, the asset `<group>/<key>`, and `disc/` holds the disc's
layout rather than assets (the editor's `project::declarations` says all of it). This reads as much as a package's
slots need followed: what a reference names, what a prefab's member holds, and where on the disc something is stored.

    python3 tools/buildsteps/declarations.py    check them: every reference names something, no uid twice
"""
from __future__ import annotations

import sys
import tomllib
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator

ROOT = Path(__file__).resolve().parents[2]
DECLARATIONS = ROOT / "declarations"
UID = "uid://"
RES = "res://"
NODE = ".node"


@dataclass(frozen=True)
class Declaration:
    """An asset as declared: its path, as `res://` paths name it without the `.node`, and its table."""

    path: str
    table: dict

    @property
    def uid(self) -> str:
        return self.table["uid"]

    @property
    def template(self) -> str | None:
        return self.table.get("template")

    def stored(self, release: str) -> Stored | None:
        """Where it is on a release's disc, where it says."""
        place = self.table.get("from", {}).get(release)
        return Stored(place) if place else None


@dataclass(frozen=True)
class Stored:
    """A declaration's `from.<release>`."""

    place: dict

    @property
    def files(self) -> list[int]:
        """The stage 0 files it is in, in the order declared."""
        return [at["file"] for at in self.place.get("at", []) if at.get("stage") == 0 and "file" in at]

    @property
    def slot(self) -> str:
        """The slot of those files it is in."""
        return self.place["at"][0]["slot"]

    @property
    def offset(self) -> int:
        return self.place["offset"]

    @property
    def size(self) -> int:
        return self.place["size"]

    @property
    def sha1(self) -> str | None:
        return self.place.get("sha1")

    @property
    def address(self) -> int:
        return self.place.get("address", 0)

    @property
    def symbol(self) -> str | None:
        return self.place.get("symbol")


def targeted(table: dict, target: str) -> dict:
    """A declaration as a target reads it: its own values, then what it says under `target.<target>` - a list
    appended to the list it has, any other value replacing. What it says for other targets is not there."""
    out = {key: value for key, value in table.items() if key != "target"}
    for key, value in table.get("target", {}).get(target, {}).items():
        out[key] = out[key] + value if isinstance(value, list) and isinstance(out.get(key), list) else value
    return out


@dataclass(frozen=True)
class Package:
    """A package a target builds. Its path is the folder its files are in, from the repository's root."""

    declaration: Declaration
    target: str

    @property
    def path(self) -> str:
        return self.declaration.path

    @property
    def name(self) -> str:
        return self.path.rsplit("/", 1)[-1]

    @property
    def fields(self) -> dict:
        """Its declaration as its target reads it (`targeted`)."""
        return targeted(self.declaration.table, self.target)

    @property
    def slot(self) -> str:
        return self.fields["slot"]

    @property
    def bound(self) -> dict:
        """What fills its slots, by name."""
        return self.fields.get("bind", {})

    @property
    def link_script(self) -> Path:
        return Path(self.path) / self.fields["link_script"]

    def units(self) -> list[Path]:
        """Its sources, as `sources` lists them: globs from its folder, a `!` leaving out what it matches."""
        folder = Path(self.path)
        kept: set[Path] = set()
        for pattern in self.fields.get("sources", []):
            if pattern.startswith("!"):
                kept -= set(folder.glob(pattern[1:]))
            else:
                kept |= set(folder.glob(pattern))
        return sorted(kept)

    def include_path(self, declarations: Declarations) -> list[Path]:
        """Where a unit's plain includes are found: its own public folders and `src`, then the public folders of its
        `dependencies` and of theirs, in that order. A slot (`PE2_INCLUDE`) is not found by it."""
        folders = [Path(self.path) / folder for folder in self.fields.get("public", [])]
        folders.append(Path(self.path) / "src")
        seen = {self.path}
        pending = list(self.fields.get("dependencies", []))
        while pending:
            dependency = declarations.named(pending.pop(0))
            if dependency.path in seen:
                continue
            seen.add(dependency.path)
            folders += [Path(dependency.path) / folder for folder in dependency.table.get("public", [])]
            pending += dependency.table.get("dependencies", [])
        return folders

    def read_by_preprocessor(self) -> list[Path]:
        """Every file of its own a unit can include, the units among them."""
        return sorted(path for path in Path(self.path).rglob("*") if path.suffix in (".c", ".h"))


def uid_of(reference: str | dict) -> tuple[str, str | None]:
    """A reference's uid and the output it names, if it does: `uid://a#outfit/mist/body`, or `{ uid, path, output }`."""
    if isinstance(reference, str):
        uid, _, output = reference.partition("#")
        return uid, output or None
    return reference["uid"], reference.get("output")


def references(value: object) -> Iterator[str | dict]:
    """Every reference held in a value, however deep: a `uid://` text, or a table of a `uid` and a `res://` path."""
    if isinstance(value, str):
        if value.startswith(UID):
            yield value
    elif isinstance(value, list):
        for item in value:
            yield from references(item)
    elif isinstance(value, dict):
        if isinstance(value.get("uid"), str) and str(value.get("path", "")).startswith(RES):
            yield value
            return
        for item in value.values():
            yield from references(item)


class Declarations:
    def __init__(self, root: Path = DECLARATIONS) -> None:
        self.by_path: dict[str, Declaration] = {}
        self.by_uid: dict[str, Declaration] = {}
        self.repeated: list[str] = []
        self.files = sorted(root.rglob("*.toml"))
        for file in self.files:
            group = file.relative_to(root).with_suffix("").as_posix()
            if group.startswith("disc/"):
                # A disc layout is one asset, the whole file.
                layout = tomllib.loads(file.read_text(encoding="utf-8"))
                if "uid" in layout:
                    self._add(Declaration(group, layout))
                continue
            for key, table in tomllib.loads(file.read_text(encoding="utf-8")).items():
                if isinstance(table, dict) and "uid" in table:
                    self._add(Declaration(f"{group}/{key}", table))

    def _add(self, declaration: Declaration) -> None:
        if declaration.uid in self.by_uid:
            self.repeated.append(declaration.uid)
        self.by_uid[declaration.uid] = declaration
        self.by_path[declaration.path] = declaration

    def named(self, reference: str | dict) -> Declaration:
        """What a reference names."""
        uid, _ = uid_of(reference)
        return self.by_uid[uid]

    def holding(self, reference: str | dict) -> Declaration:
        """The asset a reference comes to: where it names a member of a prefab's group (`#outfit/mist/body`), the model
        that member holds, and otherwise what it names."""
        declaration = self.named(reference)
        _, output = uid_of(reference)
        if output is None or "groups" not in declaration.table:
            return declaration
        member: dict = declaration.table["groups"]
        for step in output.split("/"):
            member = member[step]
        return self.named(member["model"]["slot"])

    def packages(self, target: str = "psx") -> list[Package]:
        """The packages a target links, which its link script says."""
        return [
            Package(declaration, target)
            for declaration in self.by_path.values()
            if declaration.template == "package" and "link_script" in declaration.table.get("target", {}).get(target, {})
        ]

    def published(self, target: str = "psx") -> list[Path]:
        """What is published to link against on a target: each library's symbols."""
        return [
            Path(declaration.path) / declaration.table["target"][target]["symbols"]
            for declaration in self.by_path.values()
            if "symbols" in declaration.table.get("target", {}).get(target, {})
        ]

    def package_names(self) -> set[str]:
        """Every package and library, as `PE2_BIND(<name>, package)` names them."""
        return {
            declaration.path.rsplit("/", 1)[-1]
            for declaration in self.by_path.values()
            if declaration.template in ("package", "library")
        }

    def public(self) -> dict[str, Path]:
        """What each package and library others include gives them, by the name `PE2_INCLUDE` binds it to."""
        return {
            declaration.path.rsplit("/", 1)[-1]: Path(declaration.path) / folder
            for declaration in self.by_path.values()
            if declaration.template in ("package", "library")
            for folder in declaration.table.get("public", [])
        }

    def stage0_file(self, file: int) -> tuple[Declaration, str] | None:
        """The stage 0 file of an id, as its group lists it: what fills it, and its name there."""
        for declaration in self.by_path.values():
            if declaration.template != "file_group":
                continue
            groups = (at.get("group") for place in declaration.table.get("from", {}).values() for at in place.get("at", []))
            if file // 10000 not in groups:
                continue
            for entry in declaration.table.get("files", []):
                if entry["id"] == file % 10000:
                    return self.named(entry["asset"]), entry["name"]
        return None

    def problems(self) -> list[str]:
        """What is wrong with them as a whole: a uid twice, or a reference naming no declaration."""
        found = [f"{uid} is declared twice" for uid in self.repeated]
        for declaration in self.by_path.values():
            for reference in references({key: value for key, value in declaration.table.items() if key != "uid"}):
                uid, _ = uid_of(reference)
                if uid not in self.by_uid:
                    found.append(f"{declaration.path}: {uid} names nothing")
        return found


def main() -> int:
    declarations = Declarations()
    problems = declarations.problems()
    print(f"{len(declarations.by_path)} declarations, {len(problems)} problems")
    for problem in problems[:20]:
        print(f"  {problem}")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
