#!/usr/bin/env python3
"""What a package compiles from, in two steps around the first pass, as the editor's `extensions/packages/src/gather.rs`.

    gen_package_values.py options PACKAGE OUT --release USA --config usa --target psx
    gen_package_values.py values PACKAGE OUT --release USA [--rom rom] FIRST...

`options` reads text, following includes whichever branch they are in, as what it finds decides what the preprocessor is
given: it writes `pe2_options.h`. The build then preprocesses each unit with PE2_FIRST_PASS (`packages/core/pe2.h`), which
leaves the live code with the slots' macros as written, and `values` reads that: it writes `pe2_values.h`, each slot and
value made only where it is read, an error being a macro that fires at the use. Tokens are `package_tokenizers.py`'s.

Each writes OUT only where it differs, and OUT.d, as the rules restat: a declaration changing nothing recompiles nothing.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from declarations import Declaration, Declarations, Package, uid_of  # noqa: E402
from package_tokenizers import Args, Reader, tokens, type_of  # noqa: E402
from stage0 import Stage0  # noqa: E402

KNOWN = frozenset({
    "PE2_BIND", "PE2_BIND_ARG", "PE2_EDITOR_LABEL", "PE2_INCLUDE", "PE2_VALUE", "PE2_OPTION_DEF", "PE2_OPTION",
    "PE2_OPTION_IS", "PE2_CONFIG", "PE2_CONFIG_IS",
})
CONFIG_KEYS = ("name", "release", "target")
OPTION_TYPES = ("int", "bool", "string", "enum")
UNSET = "pe2_unset_"
# The most a line of a macro takes: `mcpp` stops near 65,000 bytes, so a long value is a chain of macros.
LINE = 32_000

SKIPPED = re.compile(r'"(?:\\.|[^"\\])*"' r"|'(?:\\.|[^'\\])*'" r"|//[^\n]*|/\*.*?\*/" r"|\bPE2_[A-Z_]+\b", re.S)
BESIDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.M)


@dataclass(frozen=True)
class Use:
    name: str
    args: tuple[str, ...]
    line: int


def arguments(text: str, opening: int) -> tuple[tuple[str, ...], int] | None:
    """A call's arguments, from its opening parenthesis; and where it ends."""
    at, depth, start, args = opening + 1, 0, opening + 1, []
    while at < len(text):
        byte = text[at]
        if byte in "\"'":
            at += 1
            while at < len(text) and text[at] != byte:
                at += 2 if text[at] == "\\" else 1
        elif byte == "(":
            depth += 1
        elif byte == ")" and depth == 0:
            args.append(text[start:at].strip())
            return tuple(args), at + 1
        elif byte == ")":
            depth -= 1
        elif byte == "," and depth == 0:
            args.append(text[start:at].strip())
            start = at + 1
        at += 1
    return None


def uses(text: str) -> list[Use]:
    """Every use of a macro in a text, comments and strings left out."""
    found, at = [], 0
    while True:
        match = SKIPPED.search(text, at)
        if match is None:
            return found
        at = match.end()
        word = match.group()
        if word not in KNOWN:
            continue
        opening = at + len(text[at:at + 64]) - len(text[at:at + 64].lstrip())
        if text[opening:opening + 1] != "(":
            continue
        read = arguments(text, opening)
        if read is not None:
            found.append(Use(word, read[0], text.count("\n", 0, match.start()) + 1))
            at = read[1]


def failing(name: str, why: str) -> str:
    """A macro firing an error where it is used."""
    why = why.replace("\\", "\\\\").replace('"', '\\"')
    return f'#define {name} _Pragma("GCC error \\"{why}\\"") pe2_error_{name}\n'


def defined(name: str, text: str) -> str:
    """A macro of any length: long ones a chain of macros, cut between tokens."""
    if len(text) <= LINE:
        return f"#define {name} {text}\n"
    out, pieces, rest = "", [], text
    while rest:
        cut = len(rest)
        if cut > LINE:
            space = rest.rfind(" ", 0, LINE)
            cut = space + 1 if space >= 0 else LINE
        pieces.append(f"{name}_part{len(pieces)}")
        out += f"#define {pieces[-1]} {rest[:cut]}\n"
        rest = rest[cut:]
    return out + f"#define {name} {' '.join(pieces)}\n"


# --- options ------------------------------------------------------------------------------------------------------


@dataclass
class Option:
    type: str
    members: tuple[str, ...]
    at: str


@dataclass
class Options:
    defined: dict[str, Option] = field(default_factory=dict)
    read: list[tuple[str, str | None, str]] = field(default_factory=list)
    config: list[tuple[str, str]] = field(default_factory=list)
    wrong: list[str] = field(default_factory=list)

    def take(self, found: Use, at: str) -> None:
        name, args = found.name, found.args
        if name == "PE2_OPTION_DEF" and len(args) >= 2:
            if args[1] not in OPTION_TYPES:
                self.wrong.append(f"{at}: {args[0]}: there is no option type {args[1]}")
            if args[0] in self.defined:
                self.wrong.append(f"{at}: {args[0]} is defined already, at {self.defined[args[0]].at}")
            else:
                self.defined[args[0]] = Option(args[1], args[2:], at)
        elif name == "PE2_OPTION" and len(args) == 1:
            self.read.append((args[0], None, at))
        elif name == "PE2_OPTION_IS" and len(args) == 2:
            self.read.append((args[0], args[1], at))
        elif name in ("PE2_CONFIG", "PE2_CONFIG_IS") and args:
            self.config.append((args[0], at))

    def value_of(self, name: str, fields: dict) -> str | None:
        """An option's value for a package, as C reads it; none where it has none."""
        if name not in fields.get("option", {}):
            return None
        value, option = fields["option"][name], self.defined[name]
        wrong = SystemExit(f"{option.at}: {name} is {{}}, and this package gives it {value}")
        if option.type == "bool":
            if not isinstance(value, bool):
                raise SystemExit(wrong.code.format("a bool"))
            return str(int(value))
        if option.type == "int":
            if not isinstance(value, int) or isinstance(value, bool):
                raise SystemExit(wrong.code.format("an int"))
            return str(value)
        if option.type == "string":
            if not isinstance(value, str):
                raise SystemExit(wrong.code.format("a string"))
            return json.dumps(value)
        if value not in option.members:
            raise SystemExit(wrong.code.format("one of its members"))
        return str(option.members.index(value))

    def check(self, fields: dict) -> None:
        """The first thing wrong with what was read."""
        if self.wrong:
            raise SystemExit(self.wrong[0])
        for name, member, at in self.read:
            if name not in self.defined:
                raise SystemExit(f"{at}: {name} has no PE2_OPTION_DEF")
            if member is not None and member not in self.defined[name].members:
                raise SystemExit(f"{at}: {name} has no member {member}; it has {', '.join(self.defined[name].members)}")
        for key, at in self.config:
            if key not in CONFIG_KEYS:
                raise SystemExit(f"{at}: a config has no {key}; it has {', '.join(CONFIG_KEYS)}")
        for name in self.defined:
            self.value_of(name, fields)

    def header(self, fields: dict, config: dict[str, str]) -> str:
        """What `pe2.h` includes in both passes."""
        self.check(fields)
        header, unwritten = "", ""
        for name, option in sorted(self.defined.items()):
            value = self.value_of(name, fields)
            # With no value it divides by zero wherever `#if` evaluates it, and is an error wherever it is expanded.
            unset = f"(0/{UNSET}{name})"
            if value is None:
                unwritten += failing(f"{UNSET}{name}", f"{name} has no value on this package")
            header += f"#define pe2_option_{name} {value if value is not None else unset}\n"
            if option.type == "enum":
                for at, member in enumerate(option.members):
                    held = unset if value is None else str(int(value == str(at)))
                    header += f"#define pe2_option_{name}_is_{member} {held}\n"
        for key in CONFIG_KEYS:
            header += f'#define pe2_config_{key} "{config[key]}"\n#define pe2_config_{key}_is_{config[key]} 1\n'
        # Past the first pass: there, an undefined name in `#if` is what divides by zero.
        return header + f"#ifndef PE2_FIRST_PASS\n{unwritten}#endif\n"


def gathered(package: Package, declarations: Declarations) -> tuple[Options, list[Path]]:
    """What a package's units and what they include say of options, every file read once."""
    public = declarations.public()
    options, seen = Options(), []

    def read(file: Path) -> None:
        if file in seen:
            return
        seen.append(file)
        text = file.read_text(encoding="latin-1")
        for found in uses(text):
            options.take(found, f"{file.as_posix()}:{found.line}")
            if found.name == "PE2_INCLUDE" and len(found.args) == 2 and found.args[0] in public:
                inside = public[found.args[0]] / found.args[1]
                if inside.is_file():
                    read(inside)
        for name in BESIDE.findall(text):
            beside = file.parent / name
            if beside.is_file():
                read(beside)

    for unit in package.units():
        read(unit)
    return options, seen


# --- values -------------------------------------------------------------------------------------------------------


@dataclass
class Slot:
    type: str | None = None
    args: dict[tuple[tuple[str, ...], str], list[str]] = field(default_factory=dict)


@dataclass(frozen=True, order=True)
class Read_:
    binding: str
    path: tuple[str, ...]
    derived: str | None

    @staticmethod
    def of(args: tuple[str, ...]) -> Read_ | None:
        if not args:
            return None
        path = args[1:]
        derived = path[-1][1:] if path and path[-1].startswith("_") else None
        return Read_(args[0], path[:-1] if derived else path, derived)

    def name(self) -> str:
        """The macro it reads, as `PE2_VALUE` pastes it."""
        joined = "".join(f"_{word}" for word in self.path)
        return f"pe2_value_{self.binding}{joined}" + (f"__{self.derived}" if self.derived else "")


def slots_of(texts: list[str]) -> tuple[dict[str, Slot], set[Read_]]:
    """What a first pass left says of a package's slots."""
    slots: dict[str, Slot] = {}
    reads: set[Read_] = set()
    for text in texts:
        for found in uses(text):
            if not found.args or not found.args[0]:
                continue
            name, rest = found.args[0], found.args[1:]
            if found.name == "PE2_BIND":
                slot = slots.setdefault(name, Slot())
                slot.type = slot.type or (rest[0] if rest else None)
            elif found.name == "PE2_VALUE":
                slots.setdefault(name, Slot())
                read = Read_.of(found.args)
                if read:
                    reads.add(read)
            elif found.name == "PE2_BIND_ARG":
                at = next((index for index, word in enumerate(rest) if word.startswith("_")), None)
                if at is not None:
                    slots.setdefault(name, Slot()).args[rest[:at], rest[at][1:]] = list(rest[at + 1:])
    return slots, reads


def reached(declarations: Declarations, reference: str | dict, path: tuple[str, ...]) -> tuple[Declaration, str | None, Args]:
    """Where a reference and a sub-path come to: a node, what it reads as here, and the args it is read by. The one output
    leading on is a member of a prefab's group, a model read from the prefab."""
    node = declarations.named(reference)
    _, output = uid_of(reference)
    if path:
        raise ValueError(f"nothing goes on from {node.path}")
    if output is None:
        return node, None, {}
    fill = node.table.get("groups") if type_of(node) == "prefab" else None
    if fill is None:
        raise ValueError(f"{node.path} has no output {output}")
    return node, "model", {"member": [output]}


def slot_problem(declarations: Declarations, bound: dict, name: str, kind: str | None) -> str | None:
    """Why a slot cannot be filled as it declares."""
    if name not in bound:
        return f"{name}: nothing fills it on this package"
    try:
        node, reads_as, _ = reached(declarations, bound[name], ())
    except (KeyError, ValueError) as why:
        return f"{name}: {why}"
    own = reads_as or type_of(node)
    return None if own == kind else f"{name} is declared {kind}, and what fills it is {own}"


def said(binding: str, why: str) -> str:
    """What is wrong with a read, as the source's author is told it."""
    if why.startswith("it needs "):
        return f"{binding} needs PE2_BIND_ARG({binding}, _{why.removeprefix('it needs ')}, <value>)"
    return f"{binding}: {why}"


def values(slots: dict[str, Slot], reads: set[Read_], package: Package, reader: Reader) -> str:
    """`pe2_values.h`."""
    header = ""
    for name, slot in sorted(slots.items()):
        wrong = None if slot.type == "package" else slot_problem(reader.declarations, package.bound, name, slot.type)
        header += failing(f"pe2_bind_{name}", wrong) if wrong else f"#define pe2_bind_{name}\n"
    written: dict[str, Read_] = {}
    for read in sorted(reads, key=lambda read: (read.binding, read.path, read.derived or "")):
        name = read.name()
        if name in written:
            header += failing(name, f"{written[name]} and {read} are one name")
            continue
        written[name] = read
        try:
            slot = slots[read.binding]
            if slot.type is None:
                raise ValueError("it has no PE2_BIND")
            if read.binding not in package.bound:
                raise ValueError("nothing fills it on this package")
            node, _, led = reached(reader.declarations, package.bound[read.binding], read.path)
            args: Args = {arg: given for (path, arg), given in slot.args.items() if path == read.path}
            header += defined(name, tokens(reader, node, read.derived, {**args, **led}))
        except (KeyError, ValueError) as why:
            header += failing(name, said(read.binding, str(why)))
    return header


# --- the steps ----------------------------------------------------------------------------------------------------


def written(path: Path, text: str, inputs: list[Path]) -> None:
    """`text` at `path`, its time unchanged where it holds that already, and what it read."""
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.is_file() or path.read_text(encoding="latin-1") != text:
        path.write_text(text, encoding="latin-1")
    depfile = Path(f"{path}.d")
    depfile.write_text(f"{path.as_posix()}: " + " ".join(Path(file).as_posix() for file in inputs) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("step", choices=("options", "values"))
    parser.add_argument("package")
    parser.add_argument("out", type=Path)
    parser.add_argument("first", type=Path, nargs="*")
    parser.add_argument("--release", default="USA")
    parser.add_argument("--config")
    parser.add_argument("--target", default="psx")
    parser.add_argument("--rom", type=Path, default=Path("rom"))
    args = parser.parse_args()

    declarations = Declarations()
    package = next((one for one in declarations.packages(args.target) if one.path == args.package), None)
    if package is None:
        raise SystemExit(f"{args.package}: no package of that path is declared")
    if args.step == "options":
        options, read = gathered(package, declarations)
        config = {"name": args.config or args.release.lower(), "release": args.release, "target": args.target}
        written(args.out, options.header(package.declaration.table, config), [*read, *declarations.files])
    else:
        disc = Stage0(declarations, args.rom, args.release)
        slots, reads = slots_of([file.read_text(encoding="latin-1") for file in args.first])
        header = values(slots, reads, package, Reader(declarations, disc, args.release))
        written(args.out, header, [*declarations.files, *disc.files])


if __name__ == "__main__":
    main()
