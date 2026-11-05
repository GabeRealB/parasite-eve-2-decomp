"""The types a package registers with the build (`PE2_TYPE`), and the values of options of them, as the editor's
`extensions/packages/src/types.rs`.

A registered type is an alias of an integer primitive (`typedef s16 Angle;`), or a struct whose fields are integer
primitives, registered types, or fixed arrays of either, one field to a declaration; never a pointer. A primitive is one
of core's fixed-size integers (`s8` to `u64`, `i8` to `i64`, `byte`, the Q fixed-point types) or one of C's, whose size
is the target's (`char`, `short`, `int`, `long`, `long long`, signed or unsigned). An option of one, or of a fixed array
of one (`_LevelTuning[3]`), or a dynamic array of one (`_Wave[]`, an option's own type alone, its length what it holds and at
least one), takes a C initializer as its default and a TOML value as what a package sets: a table for a struct, by field,
an array for an array, each merged into the default - a dynamic array's length is what the package sets, an element past
the default's set in full; `PE2_OPTION` is the whole initializer, `PE2_OPTION_AT(NAME, 2, wedges)` one integer of it and
`PE2_OPTION_COUNT(NAME)` an array's length.
"""
from __future__ import annotations

import re
from dataclasses import dataclass, field

# Core's fixed-size integers (`types.h`), by name: signed or not, and bits.
FIXED: dict[str, tuple[bool, int]] = {
    **{f"s{bits}": (True, bits) for bits in (8, 16, 32, 64)}, **{f"i{bits}": (True, bits) for bits in (8, 16, 32, 64)},
    **{f"u{bits}": (False, bits) for bits in (8, 16, 32, 64)}, "byte": (True, 8),
}
# C's integers, whose size is the target's, by target: signed or not, and bits. psx's char is unsigned (-funsigned-char).
TARGET_INTEGERS: dict[str, dict[str, tuple[bool, int]]] = {
    "psx": {
        "char": (False, 8), "signed char": (True, 8), "unsigned char": (False, 8),
        "short": (True, 16), "signed short": (True, 16), "unsigned short": (False, 16),
        "int": (True, 32), "signed int": (True, 32), "signed": (True, 32), "unsigned int": (False, 32), "unsigned": (False, 32),
        "long": (True, 32), "signed long": (True, 32), "unsigned long": (False, 32),
        "long long": (True, 64), "signed long long": (True, 64), "unsigned long long": (False, 64),
    },
}
# A Q fixed-point type, `q<integer bits>_<fraction bits>`: signed when a sign bit makes up its size.
FIXED_POINT = re.compile(r"q(\d+)_(\d+)")
COMMENTS = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
TYPE_NAME = r"(?:(?:un)?signed\s+)?(?:long\s+long|char|short|int|long)\b|(?:un)?signed\b|\w+"
FIELD = re.compile(rf"({TYPE_NAME})\s+(\w+)\s*(?:\[\s*(\w+)\s*\])?")
SPELLED = re.compile(rf"({TYPE_NAME})\s*(?:\[\s*(\w*)\s*\])?")
LITERAL = re.compile(r"-?\s*(?:0[xX][0-9a-fA-F]+|\d+)")


def words(text: str) -> str:
    """A type's name with its words one space apart (`unsigned  int` is `unsigned int`)."""
    return " ".join(text.split())


def integer(text: str) -> int | None:
    """An integer as C writes one, in decimal or hexadecimal, signed."""
    text = text.replace(" ", "")
    negative, digits = (True, text[1:]) if text.startswith("-") else (False, text)
    try:
        number = int(digits, 16) if digits[:2].lower() == "0x" else int(digits, 10)
    except ValueError:
        return None
    return -number if negative else number


@dataclass
class Field:
    name: str
    type: str
    count: int | None


@dataclass
class Type:
    name: str
    at: str
    base: str | None = None                       # an alias's fixed-size integer
    fields: list[Field] = field(default_factory=list)


@dataclass(frozen=True)
class Shape:
    """What an option or a field holds: a type, or a fixed array of it, or - an option alone - a dynamic array of it."""
    type: str
    count: int | None
    dynamic: bool = False


def spelled(text: str) -> Shape | None:
    """`T`, `T[N]` (N a literal) or `T[]`."""
    match = SPELLED.fullmatch(text.strip())
    if match is None:
        return None
    count = None
    if re.fullmatch(r".*\[\s*\]", text.strip()):
        return Shape(words(match.group(1)), None, True)
    if match.group(2) is not None:
        count = integer(match.group(2))
        if count is None or count <= 0:
            return None
    return Shape(words(match.group(1)), count)


class Types:
    """The types a package's files register."""

    def __init__(self, target: str) -> None:
        self.registered: dict[str, Type] = {}
        self.wrong: list[str] = []
        self.target = target

    def primitive(self, name: str) -> tuple[int, int] | None:
        """An integer primitive's range: core's fixed-size integers and Q types, and C's at the target's sizes."""
        if name in FIXED:
            signed, bits = FIXED[name]
        elif FIXED_POINT.fullmatch(name):
            whole, fraction = map(int, FIXED_POINT.fullmatch(name).groups())
            if whole + fraction + 1 in (8, 16, 32, 64):
                signed, bits = True, whole + fraction + 1
            elif whole + fraction in (8, 16, 32, 64):
                signed, bits = False, whole + fraction
            else:
                return None
        elif name in TARGET_INTEGERS.get(self.target, {}):
            signed, bits = TARGET_INTEGERS[self.target][name]
        else:
            return None
        return (-(1 << (bits - 1)), (1 << (bits - 1)) - 1) if signed else (0, (1 << bits) - 1)

    def register(self, name: str, text: str, offset: int, at: str) -> None:
        """A `PE2_TYPE(name)` at `offset` of `text`: the typedef of that name before it, in the same file."""
        if name in self.registered:
            self.wrong.append(f"{at}: {name} is registered already, at {self.registered[name].at}")
            return
        before = COMMENTS.sub(lambda match: " " * len(match.group()), text[:offset])
        alias = list(re.finditer(rf"\btypedef\s+([^;{{}}]+?)\s*\b{re.escape(name)}\s*;", before))
        closing = list(re.finditer(r"\}\s*" + re.escape(name) + r"\s*;", before))
        if alias and (not closing or alias[-1].start() > closing[-1].start()):
            base = words(alias[-1].group(1))
            if "*" in base:
                self.wrong.append(f"{at}: {name} is a pointer, which no registered type is")
                return
            if self.primitive(base) is None:
                self.wrong.append(f"{at}: {name} is an alias of {base}, and a registered alias is of an integer primitive")
                return
            self.registered[name] = Type(name, at, base=base)
            return
        if not closing:
            self.wrong.append(f"{at}: PE2_TYPE({name}) follows no typedef of {name} in its file")
            return
        end = closing[-1].start()
        depth, start = 0, end
        while start > 0:
            start -= 1
            if before[start] == "}":
                depth += 1
            elif before[start] == "{":
                if depth == 0:
                    break
                depth -= 1
        if not re.search(r"\btypedef\s+struct\s*\w*\s*$", before[:start]):
            self.wrong.append(f"{at}: {name} is registered as a struct or an alias of a fixed-size integer, and is neither")
            return
        registered = Type(name, at)
        for declaration in before[start + 1:end].split(";"):
            declaration = declaration.strip()
            if not declaration:
                continue
            if "*" in declaration:
                self.wrong.append(f"{at}: {name}: `{declaration}` is a pointer, which no field of a registered struct is")
                return
            match = FIELD.fullmatch(declaration)
            count = integer(match.group(3)) if match and match.group(3) else None
            if match is None or (match.group(3) and (count is None or count <= 0)):
                self.wrong.append(f"{at}: {name}: `{declaration}` is no field of a registered struct: one integer or "
                                  f"registered type, or a fixed array of one, to a declaration")
                return
            registered.fields.append(Field(match.group(2), words(match.group(1)), count))
        self.registered[name] = registered

    def check(self) -> None:
        """The first thing wrong with what was registered: a field of a type that is neither a fixed-size integer nor
        registered (C has its fields defined before it, so no type can hold itself)."""
        if self.wrong:
            raise SystemExit(self.wrong[0])
        for registered in self.registered.values():
            for one in registered.fields:
                if self.primitive(one.type) is None and one.type not in self.registered:
                    raise SystemExit(f"{registered.at}: {registered.name}.{one.name} is a {one.type}, which is neither an "
                                     f"integer primitive of {self.target} nor a registered type")

    def knows(self, shape: Shape) -> bool:
        return self.primitive(shape.type) is not None or shape.type in self.registered

    def integral(self, name: str) -> tuple[int, int] | None:
        """The range of a primitive or a registered alias; none for a struct."""
        if name in self.registered:
            base = self.registered[name].base
            return None if base is None else self.primitive(base)
        return self.primitive(name)

    # --- values ---------------------------------------------------------------------------------------------------

    def parsed(self, shape: Shape, written: str, at: str) -> object:
        """A default as C writes it, held to its shape: an integer, a dict for a struct, a list for an array."""
        tokens = re.findall(r"[{},]|[^{},\s][^{},]*", written)
        position = 0

        def tree() -> object:
            nonlocal position
            if position < len(tokens) and tokens[position] == "{":
                position += 1
                items = []
                while position < len(tokens) and tokens[position] != "}":
                    items.append(tree())
                    if position < len(tokens) and tokens[position] == ",":
                        position += 1
                position += 1
                return items
            token = tokens[position].strip()
            position += 1
            number = integer(token) if LITERAL.fullmatch(token) else None
            if number is None:
                raise SystemExit(f"{at}: `{token}` is no integer literal")
            return number

        parsed = tree()
        if position != len(tokens):
            raise SystemExit(f"{at}: the default ends before `{''.join(tokens[position:])}`")
        return self.shaped(shape, parsed, at, "")

    def shaped(self, shape: Shape, value: object, at: str, path: str) -> object:
        """A written value held to its shape, a C initializer's lists named by field."""
        where = f"{at}: {path or 'the value'}"
        if shape.dynamic:
            if not isinstance(value, list) or not value:
                raise SystemExit(f"{where} is one or more of {shape.type}")
            return [self.shaped(Shape(shape.type, None), one, at, f"{path}[{index}]") for index, one in enumerate(value)]
        if shape.count is not None:
            if not isinstance(value, list) or len(value) != shape.count:
                raise SystemExit(f"{where} is {shape.count} of {shape.type}")
            return [self.shaped(Shape(shape.type, None), one, at, f"{path}[{index}]") for index, one in enumerate(value)]
        if self.integral(shape.type) is not None:
            return self.ranged(shape.type, value, where)
        fields = self.registered[shape.type].fields
        if not isinstance(value, list) or len(value) != len(fields):
            raise SystemExit(f"{where} is a {shape.type}: {len(fields)} fields, in order")
        return {one.name: self.shaped(Shape(one.type, one.count), item, at, f"{path}.{one.name}".lstrip("."))
                for one, item in zip(fields, value)}

    def ranged(self, name: str, value: object, where: str) -> int:
        low, high = self.integral(name)
        if not isinstance(value, int) or isinstance(value, bool) or not low <= value <= high:
            raise SystemExit(f"{where} is a {name}, {low} to {high}, and is {value}")
        return value

    def merged(self, shape: Shape, default: object, given: object, at: str, path: str = "") -> object:
        """What a package sets merged into the default: a struct's fields it names, an array's elements, an integer. With
        no default, every field is set."""
        where = f"{at}: {path or 'the value'}"
        if shape.dynamic:
            if not isinstance(given, list) or not given:
                raise SystemExit(f"{where} is set to one or more of {shape.type}")
            known = default or []
            return [self.merged(Shape(shape.type, None), known[index] if index < len(known) else None, new, at, f"{path}[{index}]")
                    for index, new in enumerate(given)]
        if shape.count is not None:
            if not isinstance(given, list) or len(given) != shape.count:
                raise SystemExit(f"{where} is set to {shape.count} of {shape.type}")
            return [self.merged(Shape(shape.type, None), old, new, at, f"{path}[{index}]")
                    for index, (old, new) in enumerate(zip(default or [None] * shape.count, given))]
        if self.integral(shape.type) is not None:
            return self.ranged(shape.type, given, where)
        if not isinstance(given, dict):
            raise SystemExit(f"{where} is a {shape.type}, set by a table of its fields")
        fields = {one.name: one for one in self.registered[shape.type].fields}
        for name in given:
            if name not in fields:
                raise SystemExit(f"{where}: a {shape.type} has no field {name}")
        if default is None and set(given) != set(fields):
            raise SystemExit(f"{where} has no default, so all of a {shape.type}'s fields are set: {', '.join(fields)}")
        return {name: self.merged(Shape(one.type, one.count), default[name] if default else None, given[name], at,
                                  f"{path}.{name}".lstrip("."))
                if name in given else default[name] for name, one in fields.items()}

    @staticmethod
    def initializer(value: object) -> str:
        """A value as C initializes it."""
        if isinstance(value, dict):
            value = list(value.values())
        if isinstance(value, list):
            return "{ " + ", ".join(Types.initializer(one) for one in value) + " }"
        return str(value)

    @staticmethod
    def leaves(value: object, path: tuple[str, ...] = ()) -> list[tuple[tuple[str, ...], int]]:
        """Each integer of a value, by its path (`PE2_OPTION_AT`'s arguments after the name)."""
        if isinstance(value, dict):
            return [leaf for name, one in value.items() for leaf in Types.leaves(one, (*path, name))]
        if isinstance(value, list):
            return [leaf for index, one in enumerate(value) for leaf in Types.leaves(one, (*path, str(index)))]
        return [(path, value)]
