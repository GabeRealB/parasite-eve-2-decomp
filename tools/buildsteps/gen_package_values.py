#!/usr/bin/env python3
"""What a package compiles from, in two steps around the first pass, as the editor's `extensions/packages/src/gather.rs`.

    gen_package_values.py options PACKAGE OUT --release USA --config usa --target psx     PACKAGE: its name (a variant's own)
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
from declarations import Declaration, Declarations, Package, includes, references, uid_of  # noqa: E402
from package_tokenizers import Args, Reader, reached, tokens, type_of  # noqa: E402
from option_types import Types, spelled  # noqa: E402
from stage0 import Stage0  # noqa: E402

KNOWN = frozenset({
    "PE2_BIND", "PE2_BIND_ARG", "PE2_BIND_SELF", "PE2_EDITOR_LABEL", "PE2_VALUE", "PE2_QUERY",
    "PE2_OPTION_DEF", "PE2_OPTION", "PE2_OPTION_IS", "PE2_OPTION_AT", "PE2_OPTION_COUNT", "PE2_CONFIG", "PE2_CONFIG_IS", "PE2_TYPE",
})
# What declares a package's slots and options, and how the editor shows them: all of it in one of the package's units,
# whose order is the order the editor lists them in.
DECLARING = frozenset({"PE2_BIND", "PE2_BIND_ARG", "PE2_BIND_SELF", "PE2_OPTION_DEF", "PE2_EDITOR_LABEL"})
# The keys a query asks by.
QUERIES = ("contains",)
CONFIG_KEYS = ("name", "release", "target")
OPTION_TYPES = ("int", "bool", "string", "enum")
UNSET = "pe2_unset_"
# The most a line of a macro takes, as a preprocessor may limit one, so a long value is a chain of macros.
LINE = 32_000

# A known name right after `#define` is its definition, not a use of it; in the body of a macro taking parameters, a
# use is of them, which only where it is expanded say anything.
DEFINED = re.compile(r"#[ \t]*define[ \t]+$")
TAKING = re.compile(r"[ \t]*#[ \t]*define[ \t]+\w+\(")
SKIPPED = re.compile(r'"(?:\\.|[^"\\])*"' r"|'(?:\\.|[^'\\])*'" r"|//[^\n]*|/\*.*?\*/" r"|\bPE2_[A-Z_]+\b", re.S)


@dataclass(frozen=True)
class Use:
    name: str
    args: tuple[str, ...]
    line: int
    offset: int = field(default=0, compare=False)


def arguments(text: str, opening: int) -> tuple[tuple[str, ...], int] | None:
    """A call's arguments, from its opening parenthesis; and where it ends. A comma in braces is an initializer's."""
    at, depth, start, args = opening + 1, 0, opening + 1, []
    while at < len(text):
        byte = text[at]
        if byte in "\"'":
            at += 1
            while at < len(text) and text[at] != byte:
                at += 2 if text[at] == "\\" else 1
        elif byte in "({":
            depth += 1
        elif byte == ")" and depth == 0:
            args.append(text[start:at].strip())
            return tuple(args), at + 1
        elif byte in ")}":
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
        if word not in KNOWN or DEFINED.search(text, text.rfind("\n", 0, match.start()) + 1, match.start()):
            continue
        if TAKING.match(text, logical_line(text, match.start())):
            continue
        opening = at + len(text[at:at + 64]) - len(text[at:at + 64].lstrip())
        if text[opening:opening + 1] != "(":
            continue
        read = arguments(text, opening)
        if read is not None:
            found.append(Use(word, read[0], text.count("\n", 0, match.start()) + 1, match.start()))
            at = read[1]


def logical_line(text: str, at: int) -> int:
    """Where the line holding `at` starts, lines a backslash continues taken as one."""
    start = text.rfind("\n", 0, at) + 1
    while start > 1 and text[start - 2] == "\\":
        start = text.rfind("\n", 0, start - 1) + 1
    return start


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
    members: tuple[str, ...]   # an enum's members; for the other types, the default written after the type, if any
    at: str


@dataclass
class Options:
    types: Types
    defined: dict[str, Option] = field(default_factory=dict)
    read: list[tuple[str, str | None, str]] = field(default_factory=list)
    at_read: list[tuple[tuple[str, ...], str]] = field(default_factory=list)
    count_read: list[tuple[str, str]] = field(default_factory=list)
    config: list[tuple[str, str]] = field(default_factory=list)
    wrong: list[str] = field(default_factory=list)

    def take(self, found: Use, at: str, text: str) -> None:
        name, args = found.name, found.args
        if name == "PE2_TYPE" and len(args) == 1:
            self.types.register(args[0], text, found.offset, at)
        elif name == "PE2_OPTION_AT" and len(args) >= 2:
            self.at_read.append((args, at))
        elif name == "PE2_OPTION_COUNT" and len(args) == 1:
            self.count_read.append((args[0], at))
        elif name == "PE2_OPTION_DEF" and len(args) >= 2:
            if args[1] != "enum" and len(args) > 3:
                self.wrong.append(f"{at}: {args[0]}: an option has one default")
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

    def shape_of(self, name: str):
        """An option's shape when it is of a primitive or a registered type, or a fixed array of one."""
        option = self.defined[name]
        return None if option.type in OPTION_TYPES else spelled(option.type)

    def shaped_value(self, name: str, fields: dict) -> object | None:
        """A typed option's value: its default merged with what its package sets; none where it has no default."""
        option, shape = self.defined[name], self.shape_of(name)
        default = self.types.parsed(shape, option.members[0], f"{option.at}: {name}") if option.members else None
        given = fields.get("option", {}).get(name)
        if given is None:
            return default
        return self.types.merged(shape, default, given, f"{option.at}: {name}, as this package sets it")

    def default_of(self, name: str) -> str | None:
        """The default a source gives an option after its type, as C reads it."""
        option = self.defined[name]
        if self.shape_of(name) is not None:
            value = self.shaped_value(name, {})
            return None if value is None else self.types.initializer(value)
        if option.type == "enum" or not option.members:
            return None
        written = option.members[0]
        wrong = SystemExit(f"{option.at}: {name} is {option.type}, and its default is {written}")
        if option.type == "int":
            negative, digits = (True, written[1:]) if written.startswith("-") else (False, written)
            try:
                number = int(digits, 16) if digits[:2].lower() == "0x" else int(digits, 10)
            except ValueError:
                raise wrong from None
            return str(-number if negative else number)
        if option.type == "bool":
            if written not in ("true", "false", "1", "0"):
                raise wrong
            return "1" if written in ("true", "1") else "0"
        if not (len(written) >= 2 and written[0] == written[-1] == '"'):
            raise wrong
        return written

    def value_of(self, name: str, fields: dict) -> str | None:
        """An option's value for a package, as C reads it: what its package sets, else the default its source gives; none
        where it has neither."""
        if self.shape_of(name) is not None:
            value = self.shaped_value(name, fields)
            return None if value is None else self.types.initializer(value)
        if name not in fields.get("option", {}):
            return self.default_of(name)
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
        self.types.check()
        for name, option in self.defined.items():
            shape = self.shape_of(name)
            if option.type not in OPTION_TYPES and (shape is None or not self.types.knows(shape)):
                raise SystemExit(f"{option.at}: {name}: there is no option type {option.type}; an option is int, bool, "
                                 f"string, enum, an integer primitive or a registered type, or a fixed array of either")
        for name, at in self.count_read:
            shape = self.shape_of(name) if name in self.defined else None
            if shape is None or not (shape.dynamic or shape.count is not None):
                raise SystemExit(f"{at}: PE2_OPTION_COUNT reads an array option, and {name} is none")
        for args, at in self.at_read:
            if args[0] not in self.defined or self.shape_of(args[0]) is None:
                raise SystemExit(f"{at}: PE2_OPTION_AT reads an option of a registered type or an array, and {args[0]} is none")
            value = self.shaped_value(args[0], fields)
            paths = {path for path, _ in self.types.leaves(value)} if value is not None else set()
            if value is not None and tuple(args[1:]) not in paths:
                raise SystemExit(f"{at}: {args[0]} has no integer at {', '.join(args[1:])}")
        for name, member, at in self.read:
            if name not in self.defined:
                raise SystemExit(f"{at}: {name} has no PE2_OPTION_DEF")
            if member is not None and self.defined[name].type != "enum":
                raise SystemExit(f"{at}: {name} is {self.defined[name].type}, and only an enum has members")
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
            if self.shape_of(name) is not None and value is not None:
                shaped = self.shaped_value(name, fields)
                if isinstance(shaped, list):
                    header += f"#define pe2_option_{name}_count {len(shaped)}\n"
                for path, leaf in self.types.leaves(shaped):
                    if path:
                        header += f"#define pe2_option_{name}_{'_'.join(path)} {leaf}\n"
            if option.type == "enum":
                for at, member in enumerate(option.members):
                    held = unset if value is None else str(int(value == str(at)))
                    header += f"#define pe2_option_{name}_is_{member} {held}\n"
        for key in CONFIG_KEYS:
            header += f'#define pe2_config_{key} "{config[key]}"\n#define pe2_config_{key}_is_{config[key]} 1\n'
        # Past the first pass: there, an undefined name in `#if` is what divides by zero.
        return header + f"#ifndef PE2_FIRST_PASS\n{unwritten}#endif\n"


def gathered(package: Package, declarations: Declarations, target: str) -> tuple[Options, list[Path]]:
    """What a package's C units and what they include say of options, and every file read, once: each include is held
    to the rules of the package whose file has it (`Package.resolved`), and its slots and options to being declared in
    one of its units (`DECLARING`), as the editor's gather holds them: never a header, which whatever includes it reads.
    """
    options, seen = Options(Types(target)), []
    declaring: list[Path] = []
    units = [unit for unit in package.units() if unit.suffix == ".c"]

    def read(file: Path) -> None:
        if file in seen:
            return
        seen.append(file)
        text = file.read_text(encoding="latin-1")
        owner = declarations.owner_of(file)
        if owner is None:
            raise SystemExit(f"{file.as_posix()} is no package's")
        for found in uses(text):
            at = f"{file.as_posix()}:{found.line}"
            if found.name in DECLARING:
                if file not in units:
                    raise SystemExit(f"{at}: {found.name} in no unit of {package.name}: a package declares its slots and options in one of its units, never in a header")
                if declaring and declaring[0] != file:
                    raise SystemExit(f"{at}: {found.name} outside {declaring[0].as_posix()}: a package's slots and options are declared in one file, in the order the editor lists them")
                declaring[:1] = [file]
            options.take(found, at, text)
        for spelled, angled, line in includes(text):
            reached = owner.resolved(declarations, file, spelled, angled, f"{file.as_posix()}:{line}")
            if reached is not None:
                read(reached)

    for unit in units:
        read(unit)
    return options, seen


# --- values -------------------------------------------------------------------------------------------------------


@dataclass
class Slot:
    type: str | None = None
    # What it is beside its type, after it (`_self`: filled by default with the package built; `_hidden`).
    attributes: set[str] = field(default_factory=set)
    args: dict[tuple[tuple[str, ...], str], list[str]] = field(default_factory=dict)


@dataclass(frozen=True, order=True)
class Query:
    binding: str
    key: str
    args: tuple[str, ...]

    def name(self) -> str:
        """The macro it reads, as `PE2_QUERY` pastes it."""
        return f"pe2_query_{self.binding}_{self.key}" + "".join(f"_{arg}" for arg in self.args)


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


def slots_of(texts: list[str]) -> tuple[dict[str, Slot], set[Read_], set[Query]]:
    """What a first pass left says of a package's slots."""
    slots: dict[str, Slot] = {}
    reads: set[Read_] = set()
    queries: set[Query] = set()
    for text in texts:
        for found in uses(text):
            if not found.args or not found.args[0]:
                continue
            name, rest = found.args[0], found.args[1:]
            if found.name == "PE2_BIND":
                slot = slots.setdefault(name, Slot())
                slot.type = slot.type or (rest[0] if rest else None)
                slot.attributes |= {word[1:] for word in rest[1:] if word.startswith("_")}
            elif found.name == "PE2_QUERY" and rest:
                slots.setdefault(name, Slot())
                queries.add(Query(name, rest[0], tuple(rest[1:])))
            elif found.name == "PE2_VALUE":
                slots.setdefault(name, Slot())
                read = Read_.of(found.args)
                if read:
                    reads.add(read)
            elif found.name == "PE2_BIND_ARG":
                at = next((index for index, word in enumerate(rest) if word.startswith("_")), None)
                if at is not None:
                    slots.setdefault(name, Slot()).args[rest[:at], rest[at][1:]] = list(rest[at + 1:])
    return slots, reads, queries


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


def bound_of(slots: dict[str, Slot], package: Package) -> dict:
    """What fills a package's slots, by name: what its declaration binds, else for a slot filled by default with the
    package built, the package."""
    own = {"uid": package.declaration.uid, "path": f"res://{package.path}.node"}
    defaults = {name: own for name, slot in slots.items() if "self" in slot.attributes}
    return {**defaults, **package.bound}


def holder(declarations: Declarations, reference: str | dict) -> Declaration:
    """What a slot's reference comes to as a holder of others: a file of a group is the file's asset."""
    node, reads_as, args = reached(declarations, reference, ())
    if type_of(node) == "file_group" and reads_as == "file":
        entry = next(file for file in node.table.get("files", []) if file.get("name") == args["file"][0])
        return declarations.named(entry["asset"])
    return node


def answer(declarations: Declarations, slots: dict[str, Slot], bound: dict, query: Query) -> str:
    """What the build says to a query, 1 or 0: `contains`, whether what a slot holds names, anywhere in it, what
    another holds."""
    if query.key not in QUERIES:
        raise ValueError(f"there is no query {query.key}; there is {', '.join(QUERIES)}")
    if len(query.args) != 1 or query.args[0] not in slots:
        raise ValueError(f"{query.key} takes a slot of the package")
    for name in (query.binding, query.args[0]):
        if name not in bound:
            raise ValueError(f"nothing fills {name} on this package")
    wanted = holder(declarations, bound[query.args[0]]).uid
    held = holder(declarations, bound[query.binding])
    return "1" if any(uid_of(reference)[0] == wanted for reference in references(held.table)) else "0"


def values(slots: dict[str, Slot], reads: set[Read_], queries: set[Query], package: Package, reader: Reader) -> str:
    """`pe2_values.h`."""
    header = ""
    bound = bound_of(slots, package)
    for name, slot in sorted(slots.items()):
        wrong = None if slot.type == "package" else slot_problem(reader.declarations, bound, name, slot.type)
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
            if read.binding not in bound:
                raise ValueError("nothing fills it on this package")
            node, _, led = reached(reader.declarations, bound[read.binding], read.path)
            args: Args = {arg: given for (path, arg), given in slot.args.items() if path == read.path}
            header += defined(name, tokens(reader, node, read.derived, {**args, **led}))
        except (KeyError, ValueError) as why:
            header += failing(name, said(read.binding, str(why)))
    for query in sorted(queries):
        try:
            header += defined(query.name(), answer(reader.declarations, slots, bound, query))
        except (KeyError, ValueError, StopIteration) as why:
            header += failing(query.name(), said(query.binding, str(why)))
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
    package = next((one for one in declarations.packages(args.target) if one.name == args.package), None)
    if package is None:
        raise SystemExit(f"{args.package}: no package of that name is declared")
    if args.step == "options":
        options, read = gathered(package, declarations, args.target)
        config = {"name": args.config or args.release.lower(), "release": args.release, "target": args.target}
        written(args.out, options.header(package.fields, config), [*read, *declarations.files])
    else:
        disc = Stage0(declarations, args.rom, args.release)
        slots, reads, queries = slots_of([file.read_text(encoding="latin-1") for file in args.first])
        header = values(slots, reads, queries, package, Reader(declarations, disc, args.release))
        written(args.out, header, [*declarations.files, *disc.files])


if __name__ == "__main__":
    main()
