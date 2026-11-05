#!/usr/bin/env python3
"""The declarations, read: what the build needs of them, with nothing but the standard library.

A declaration is a table of a `<group>.toml` holding a `uid`, the asset `<group>/<key>`, and `disc/` holds the disc's
layout rather than assets (the editor's `project::declarations` says all of it). This reads as much as a package's
slots need followed: what a reference names, what a prefab's member holds, and where on the disc something is stored.

    python3 tools/buildsteps/declarations.py    check them: every reference names something, no uid twice
"""
from __future__ import annotations

import re
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
# What the build makes for the package being built, which core's `pe2.h` includes in quotes.
MADE = frozenset({"pe2_options.h", "pe2_values.h", "pe2_server.h"})
INCLUDED = re.compile(r'^[ \t]*#[ \t]*include[ \t]*(?:"([^"\n]+)"|<([^>\n]+)>)', re.M)


def includes(text: str) -> Iterator[tuple[str, bool, int]]:
    """Every `#include` written, whichever branch it is in: what it names, whether in angle brackets, its line."""
    for found in INCLUDED.finditer(text):
        yield found[1] or found[2], found[2] is not None, text.count("\n", 0, found.start()) + 1


def normal(path: str) -> str:
    """A path with its `.` and `..` steps taken."""
    out: list[str] = []
    for step in path.split("/"):
        if step == ".." and out and out[-1] != "..":
            out.pop()
        elif step not in ("", "."):
            out.append(step)
    return "/".join(out)


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
    """A declaration as a target reads it: its own values, then what it says under `target.<target>` - a table merged
    into the table it has, a list appended to the list it has, any other value replacing. What it says for other
    targets is not there."""
    return merged({key: value for key, value in table.items() if key != "target"}, table.get("target", {}).get(target, {}))


def merged(into: dict, more: dict) -> dict:
    """`more` over `into`: tables merged, lists appended, other values replacing."""
    out = dict(into)
    for key, value in more.items():
        held = out.get(key)
        if isinstance(value, dict) and isinstance(held, dict):
            out[key] = merged(held, value)
        elif isinstance(value, list) and isinstance(held, list):
            out[key] = held + value
        else:
            out[key] = value
    return out


# What a package's files are for, which a declaration says of each kind: `private`, its own build's; `public`, its own
# and, as far as an image, what links it.
VISIBILITIES = ("private", "public")


@dataclass(frozen=True)
class Package:
    """A package a target builds. Its path is the folder its files are in, from the repository's root."""

    declaration: Declaration
    target: str
    # What others include it by (`<engine/fs.h>`) and its objects are named by: its folder's name, after the name of the
    # package its folder is in, if one is (`aya/mist`); folders that are no package's do not count (`Declarations.names`).
    name: str

    @property
    def path(self) -> str:
        return self.declaration.path

    @property
    def public(self) -> Path | None:
        """The folder of what others may include of it (`headers.public`), if it has one: one, which its name stands
        for (`<engine/fs.h>`)."""
        folder = self.headers.get("public")
        if folder is not None and not isinstance(folder, str):
            raise SystemExit(f"{self.path}: headers.public is one folder, which its name stands for")
        return Path(self.path) / folder if folder is not None else None

    @property
    def private(self) -> list[Path]:
        """The folders its own files' quoted includes are found in after their own (`headers.private`)."""
        return [Path(self.path) / folder for folder in self.headers.get("private", [])]

    @property
    def headers(self) -> dict:
        return self.kind("headers")

    def kind(self, key: str) -> dict:
        """What it declares of a kind of file, by visibility."""
        found = self.fields.get(key, {})
        if not isinstance(found, dict) or any(visibility not in VISIBILITIES for visibility in found):
            raise SystemExit(f"{self.path}: {key} is declared by visibility: {', '.join(f'{key}.{one}' for one in VISIBILITIES)}")
        return found

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

    def listed(self, key: str, visibilities: tuple[str, ...] = VISIBILITIES) -> list[Path]:
        """The files it declares of a kind, of those visibilities: globs from its folder, in order, a `!` leaving out
        what it matches."""
        folder = Path(self.path)
        kept: set[Path] = set()
        declared = self.kind(key)
        for pattern in (pattern for visibility in visibilities for pattern in declared.get(visibility, [])):
            if pattern.startswith("!"):
                kept -= set(folder.glob(pattern[1:]))
            else:
                kept |= set(folder.glob(pattern))
        return sorted(kept)

    def units(self, visibilities: tuple[str, ...] = VISIBILITIES) -> list[Path]:
        """Its sources (`sources`), which its target compiles by their kind: C, or assembly (`.s`)."""
        return self.listed("sources", visibilities)

    def objects(self, visibilities: tuple[str, ...] = VISIBILITIES) -> list[Path]:
        """What its target links as it is (`objects`)."""
        return self.listed("objects", visibilities)

    def object_of(self, unit: Path, root: Path) -> Path:
        """Where a unit's object is made, named by the package and the unit's place in it, as a link script names it
        (`*/title/src/package.o`) in every build."""
        return root / self.name / unit.relative_to(self.path).with_suffix(".o")

    def makes_image(self) -> bool:
        """Whether its target makes it an image, which others link against by the symbols it publishes: it has a link
        script, or, built elsewhere, publishes symbols (`symbols`)."""
        return "link_script" in self.fields or "symbols" in self.fields

    def makes_objects(self) -> bool:
        """Whether it gives what links it objects: public sources compiled, or public objects as they are."""
        return bool(self.kind("sources").get("public") or self.kind("objects").get("public"))

    def declared(self, declarations: Declarations) -> list[Package]:
        """What it depends on, as its declaration says (`dependencies`) for its target."""
        return [declarations.package(declarations.named(reference), self.target) for reference in self.fields.get("dependencies", [])]

    def exposed(self, declarations: Declarations) -> list[Package]:
        """What of what it depends on its public headers include, which whatever includes them needs too."""
        if self.public is None or not self.public.is_dir():
            return []
        named = {package.name: package for package in self.declared(declarations)}
        found: dict[str, Package] = {}
        for header in sorted(self.public.rglob("*.h")):
            for spelled, angled, _ in includes(header.read_text(encoding="latin-1")):
                dependency = declarations.spelled(spelled) if angled else None
                if dependency is not None and dependency[0].name in named:
                    found[dependency[0].name] = named[dependency[0].name]
        return list(found.values())

    def reachable(self, declarations: Declarations) -> list[Package]:
        """What its units can include: what it depends on, what each of those exposes, and theirs, each once."""
        found: dict[str, Package] = {}
        pending = self.declared(declarations)
        while pending:
            dependency = pending.pop(0)
            if dependency.name in found or dependency.name == self.name:
                continue
            found[dependency.name] = dependency
            pending += dependency.exposed(declarations)
        return list(found.values())

    def closure(self, declarations: Declarations) -> set[str]:
        """The names of everything it depends on, however far."""
        found: set[str] = set()
        pending = self.declared(declarations)
        while pending:
            dependency = pending.pop(0)
            if dependency.name not in found:
                found.add(dependency.name)
                pending += dependency.declared(declarations)
        return found

    def linked(self, declarations: Declarations) -> tuple[list[Package], list[Package]]:
        """What its image links: the libraries whose objects it takes, and the images it links against.

        Everything it depends on, and theirs, as far as an image: an image is linked against by its symbols, and what
        it depends on is its own, so a library reached through it is not taken again (Psy-Q's members are the
        executable's)."""
        libraries: dict[str, Package] = {}
        images: dict[str, Package] = {}
        covered: set[str] = set()
        pending = self.declared(declarations)
        while pending:
            dependency = pending.pop(0)
            if dependency.name in libraries or dependency.name in images or dependency.name == self.name:
                continue
            if dependency.makes_image():
                images[dependency.name] = dependency
                covered |= dependency.closure(declarations)
                continue
            libraries[dependency.name] = dependency
            pending += dependency.declared(declarations)
        objects = [library for name, library in libraries.items() if name not in covered and library.makes_objects()]
        return objects, list(images.values())

    def include_flags(self, deps: Path) -> str:
        """Where its units' includes are found: what they write in quotes in its own folders (`-iquote`), what they
        write in angle brackets in `deps`, which holds each package it can reach under that package's name."""
        own = [*([self.public] if self.public is not None else []), *self.private]
        return " ".join([*(f"-iquote {folder.as_posix()}" for folder in own), f"-I{deps.as_posix()}"])

    def resolved(self, declarations: Declarations, file: Path, spelled: str, angled: bool, at: str) -> Path | None:
        """The file an include in one of its files names, by the rules both builds hold every package's files to: in
        quotes, a file of the same package - of the same public folder, from a public header - or one the build makes
        (`pe2_options.h`); in angle brackets, a public file of a package its declaration names. A file not there is
        the compiler's to say: it knows which branch of a conditional is taken."""
        if not angled:
            if spelled in MADE:
                return None
            public = self.public
            in_public = public is not None and file.is_relative_to(public)
            folders = [file.parent] if in_public else [file.parent, *([public] if public is not None else []), *self.private]
            within = public if in_public else Path(self.path)
            for folder in folders:
                candidate = Path(normal((folder / spelled).as_posix()))
                if candidate.is_file():
                    if not candidate.is_relative_to(within):
                        kind = "a public file" if in_public else "a file"
                        raise SystemExit(f'{at}: "{spelled}" is not {kind} of {self.name}')
                    return candidate
            raise SystemExit(f'{at}: "{spelled}" is no file of {self.name}; another package\'s is included in angle brackets')
        dependency = declarations.spelled(spelled)
        if dependency is None:
            raise SystemExit(f"{at}: <{spelled}> names no package")
        package, inside = dependency
        if package.name not in {declared.name for declared in self.declared(declarations)}:
            raise SystemExit(f"{at}: <{spelled}> is {package.name}'s, which {self.name} does not depend on")
        found = package.public / inside if package.public is not None else None
        return found if found is not None and found.is_file() else None

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
            self.package(declaration, target)
            for declaration in self.by_path.values()
            if declaration.template == "package" and "link_script" in declaration.table.get("target", {}).get(target, {})
        ]

    def compiled(self, target: str = "psx") -> list[Package]:
        """Every package and library a target has something to make of: sources to compile or objects to link."""
        return [
            package
            for declaration in self.by_path.values()
            if declaration.template in ("package", "library")
            for package in [self.package(declaration, target)]
            if package.kind("sources") or package.kind("objects")
        ]

    def published(self, target: str = "psx") -> list[Path]:
        """What is published to link against on a target: each library's symbols."""
        return [
            Path(declaration.path) / declaration.table["target"][target]["symbols"]
            for declaration in self.by_path.values()
            if "symbols" in declaration.table.get("target", {}).get(target, {})
        ]

    def every_package(self, target: str = "psx") -> list[Package]:
        """Every package and library."""
        return [self.package(declaration, target) for declaration in self.by_path.values() if declaration.template in ("package", "library")]

    def package(self, declaration: Declaration, target: str = "psx") -> Package:
        """A package or library as a target reads it."""
        return Package(declaration, target, self.names()[declaration.path])

    def names(self) -> dict[str, str]:
        """Every package's name, by its path: its folder's name, after the name of the nearest package whose folder
        holds it. A package nests in packages, not in folders: `packages/a/aya` and `packages/b/aya` are both `aya`,
        and `packages/aya/akropolis` is `aya/akropolis`."""
        if not hasattr(self, "_names"):
            paths = sorted(path for path, declaration in self.by_path.items() if declaration.template in ("package", "library"))
            self._names: dict[str, str] = {}
            for path in paths:
                outer = max((other for other in paths if path.startswith(other + "/")), key=len, default=None)
                last = path.rsplit("/", 1)[-1]
                self._names[path] = f"{self._names[outer]}/{last}" if outer is not None else last
        return self._names

    def public(self) -> dict[str, Path]:
        """What each package and library others include gives them, by its name (`<engine/fs.h>`)."""
        return {package.name: package.public for package in self.every_package() if package.public is not None}

    def spelled(self, spelled: str) -> tuple[Package, str] | None:
        """The package an include in angle brackets names, and the path within its public folder: the longest name
        the include starts with."""
        found = None
        for package in self.every_package():
            if spelled.startswith(package.name + "/") and (found is None or len(package.name) > len(found[0].name)):
                found = (package, spelled[len(package.name) + 1:])
        return found

    def owner_of(self, file: Path) -> Package | None:
        """The package a file is of: the one whose folder holds it nearest."""
        owners = [package for package in self.every_package() if file.is_relative_to(package.path)]
        return max(owners, key=lambda package: len(package.path), default=None)

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
        """What is wrong with them as a whole: a uid twice, a reference naming no declaration, or two packages' public
        files at one place (a package with a public folder inside one that has one)."""
        found = [f"{uid} is declared twice" for uid in self.repeated]
        by_name: dict[str, str] = {}
        for path, name in self.names().items():
            if name in by_name:
                found.append(f"{by_name[name]} and {path} are both named {name}, in the same package or none")
            by_name.setdefault(name, path)
        public = self.public()
        for name in public:
            for outer in public:
                if name != outer and name.startswith(outer + "/"):
                    found.append(f"the package {name}: its public files and {outer}'s are both <{name}/...>")
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
