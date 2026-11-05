#!/usr/bin/env python3
"""The package build's reading of sources and its tokens, against what the editor's own tests say of them.

    python3 tools/buildsteps/test_package_values.py
"""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from declarations import Declaration, Declarations  # noqa: E402
from gen_package_values import LINE, Options, Query, Read_, Slot, Use, answer, defined, failing, reached, slot_problem, slots_of, uses  # noqa: E402
from package_tokenizers import Reader, material_clut, material_tpage, tokens  # noqa: E402
from option_types import Types  # noqa: E402

CONFIG = {"name": "usa", "release": "USA", "target": "psx"}


def options_of(text: str) -> Options:
    options = Options(Types("psx"))
    for found in uses(text):
        options.take(found, f"a.c:{found.line}", text)
    return options


class TypedOption(unittest.TestCase):
    TYPE = "typedef struct {\n    s16 wedges; // Fanned\n    unsigned short limit;\n    u8 steps[2];\n} _Level;\nPE2_TYPE(_Level)\n"

    def test_a_registered_type_reads_whole_and_by_path(self) -> None:
        text = self.TYPE + "PE2_OPTION_DEF(LEVELS, _Level[2], { { 8, 0x90, { 1, 2 } }, { -1, 0, { 3, 4 } } })\nPE2_OPTION_AT(LEVELS, 1, steps, 0)\n"
        header = options_of(text).header({"option": {"LEVELS": [{}, {"limit": 7}]}}, CONFIG)
        for line in ("pe2_option_LEVELS { { 8, 144, { 1, 2 } }, { -1, 7, { 3, 4 } } }", "pe2_option_LEVELS_1_limit 7",
                     "pe2_option_LEVELS_1_steps_0 3", "pe2_option_LEVELS_0_wedges 8"):
            self.assertIn(f"#define {line}\n", header)

    def test_a_primitive_is_held_to_its_range(self) -> None:
        header = options_of("PE2_OPTION_DEF(Y, s16, -1024)\nPE2_OPTION_DEF(C, char, 200)\n").header({}, CONFIG)
        self.assertIn("#define pe2_option_Y -1024\n", header)
        self.assertIn("#define pe2_option_C 200\n", header)
        self.assertEqual(Option.wrong(self, "PE2_OPTION_DEF(Y, s16, 40000)"), "a.c:1: Y: the value is a s16, -32768 to 32767, and is 40000")

    def test_what_is_wrong_is_said_where(self) -> None:
        self.assertEqual(Option.wrong(self, "typedef struct { s16* p; } P;\nPE2_TYPE(P)\n"),
                         "a.c:2: P: `s16* p` is a pointer, which no field of a registered struct is")
        self.assertEqual(Option.wrong(self, "typedef struct { float f; } F;\nPE2_TYPE(F)\n"),
                         "a.c:2: F.f is a float, which is neither an integer primitive of psx nor a registered type")
        self.assertEqual(Option.wrong(self, "PE2_TYPE(G)\n"), "a.c:1: PE2_TYPE(G) follows no typedef of G in its file")
        self.assertEqual(Option.wrong(self, self.TYPE + "PE2_OPTION_DEF(L, _Level, { 1, 2 })\n"),
                         "a.c:7: L: the value is a _Level: 3 fields, in order")
        self.assertEqual(Option.wrong(self, self.TYPE + "PE2_OPTION_DEF(L, _Level)\n", {"option": {"L": {"wedges": 1}}}),
                         "a.c:7: L, as this package sets it: the value has no default, so all of a _Level's fields are set: wedges, limit, steps")
        self.assertEqual(Option.wrong(self, self.TYPE + "PE2_OPTION_DEF(L, _Level, { 1, 2, { 3, 4 } })\nPE2_OPTION_AT(L, steps, 2)\n"),
                         "a.c:8: L has no integer at steps, 2")


class DynamicOption(unittest.TestCase):
    TYPE = "typedef struct {\n    s16 wedges;\n    u8 steps[2];\n} _Level;\nPE2_TYPE(_Level)\n"
    TEXT = TYPE + "PE2_OPTION_DEF(WAVES, _Level[], { { 1, { 2, 3 } }, { 4, { 5, 6 } } })\nPE2_OPTION_COUNT(WAVES)\n"

    def test_its_length_is_what_the_package_sets(self) -> None:
        header = options_of(self.TEXT).header({}, CONFIG)
        self.assertIn("#define pe2_option_WAVES { { 1, { 2, 3 } }, { 4, { 5, 6 } } }\n", header)
        self.assertIn("#define pe2_option_WAVES_count 2\n", header)
        longer = options_of(self.TEXT).header({"option": {"WAVES": [{"wedges": 9}, {}, {"wedges": 7, "steps": [8, 9]}]}}, CONFIG)
        self.assertIn("#define pe2_option_WAVES { { 9, { 2, 3 } }, { 4, { 5, 6 } }, { 7, { 8, 9 } } }\n", longer)
        self.assertIn("#define pe2_option_WAVES_count 3\n", longer)
        shorter = options_of(self.TEXT).header({"option": {"WAVES": [{}]}}, CONFIG)
        self.assertIn("#define pe2_option_WAVES { { 1, { 2, 3 } } }\n", shorter)

    def test_what_is_wrong_is_said_where(self) -> None:
        self.assertEqual(Option.wrong(self, self.TEXT, {"option": {"WAVES": []}}),
                         "a.c:6: WAVES, as this package sets it: the value is set to one or more of _Level")
        self.assertEqual(Option.wrong(self, self.TEXT, {"option": {"WAVES": [{}, {}, {"wedges": 7}]}}),
                         "a.c:6: WAVES, as this package sets it: [2] has no default, so all of a _Level's fields are set: wedges, steps")
        self.assertEqual(Option.wrong(self, self.TYPE + "PE2_OPTION_DEF(N, s16, 1)\nPE2_OPTION_COUNT(N)\n"),
                         "a.c:7: PE2_OPTION_COUNT reads an array option, and N is none")
        self.assertEqual(Option.wrong(self, "typedef struct { s16 all[]; } F;\nPE2_TYPE(F)\n"),
                         "a.c:2: F: `s16 all[]` is no field of a registered struct: one integer or registered type, or a fixed array of one, to a declaration")


class Scanning(unittest.TestCase):
    def test_comments_and_strings_are_left_out(self) -> None:
        text = '// PE2_VALUE(a)\n/* PE2_VALUE(b) */\nchar* c = "PE2_VALUE(d)"; char e = \'"\';\nPE2_VALUE(f)\n'
        self.assertEqual(uses(text), [Use("PE2_VALUE", ("f",), 4)])

    def test_arguments_hold_parentheses_and_strings(self) -> None:
        self.assertEqual(uses('PE2_BIND_ARG(a, _b, f(1, 2), "x,y")')[0].args, ("a", "_b", "f(1, 2)", '"x,y"'))

    def test_a_use_runs_over_lines_and_the_next_is_counted(self) -> None:
        found = uses("PE2_VALUE(\n  a,\n  b)\nPE2_VALUE(c)")
        self.assertEqual([(use.args, use.line) for use in found], [(("a", "b"), 1), (("c",), 4)])

    def test_a_definition_is_no_use(self) -> None:
        self.assertEqual(uses("#define PE2_OPTION(name) pe2_option_##name\n# define PE2_VALUE(a) b\nPE2_OPTION(x)"), [Use("PE2_OPTION", ("x",), 3)])

    def test_a_macro_taking_parameters_uses_nothing_until_expanded(self) -> None:
        text = "#define PE2_BIND_SELF(binding) \\\n    PE2_BIND(binding, file)\n#define ONE PE2_VALUE(a)\nPE2_BIND_SELF(file)"
        self.assertEqual(uses(text), [Use("PE2_VALUE", ("a",), 3), Use("PE2_BIND_SELF", ("file",), 4)])

    def test_what_is_no_call_is_nothing(self) -> None:
        self.assertEqual(uses("PE2_VALUE PE2_UNKNOWN(a) XPE2_VALUE(b)"), [])


class Macros(unittest.TestCase):
    def test_a_read_is_named_as_pe2_value_pastes_it(self) -> None:
        self.assertEqual(Read_.of(("a",)).name(), "pe2_value_a")
        self.assertEqual(Read_.of(("a", "_height")).name(), "pe2_value_a__height")
        self.assertEqual(Read_.of(("a", "b", "c", "_d")).name(), "pe2_value_a_b_c__d")

    def test_a_failure_fires_at_its_use(self) -> None:
        self.assertEqual(failing("x", 'a "b"'), '#define x _Pragma("GCC error \\"a \\"b\\"\\"") pe2_error_x\n')

    def test_a_long_value_is_a_chain_cut_between_tokens(self) -> None:
        text = " ".join(["word"] * LINE)
        lines = defined("x", text).splitlines()
        self.assertEqual(lines[-1].split()[:3], ["#define", "x", "x_part0"])
        self.assertTrue(all(len(line) < LINE + 32 for line in lines[:-1]))
        self.assertEqual("".join(line.split(" ", 2)[2] for line in lines[:-1]), text)


class Option(unittest.TestCase):
    TEXT = "PE2_OPTION_DEF(COUNT, int)\nPE2_OPTION_DEF(FLAG, bool)\nPE2_OPTION_DEF(NAME, string)\nPE2_OPTION_DEF(FINISH, enum, matte, gloss)\n"

    def test_each_type_as_c_reads_it(self) -> None:
        header = options_of(self.TEXT).header({"option": {"COUNT": 31, "FLAG": True, "NAME": "a", "FINISH": "gloss"}}, CONFIG)
        for line in ("pe2_option_COUNT 31", "pe2_option_FLAG 1", 'pe2_option_NAME "a"', "pe2_option_FINISH 1",
                     "pe2_option_FINISH_is_matte 0", "pe2_option_FINISH_is_gloss 1"):
            self.assertIn(f"#define {line}\n", header)

    def test_a_default_is_what_a_package_does_not_set(self) -> None:
        text = 'PE2_OPTION_DEF(T, int, 900)\nPE2_OPTION_DEF(Y, int, 0x38)\nPE2_OPTION_DEF(N, int, -1)\nPE2_OPTION_DEF(B, bool, true)\nPE2_OPTION_DEF(S, string, "a")\n'
        header = options_of(text).header({"option": {"T": 600}}, CONFIG)
        for line in ("pe2_option_T 600", "pe2_option_Y 56", "pe2_option_N -1", "pe2_option_B 1", 'pe2_option_S "a"'):
            self.assertIn(f"#define {line}\n", header)
        self.assertNotIn("pe2_unset_", header)
        self.assertEqual(self.wrong("PE2_OPTION_DEF(T, int, fast)"), "a.c:1: T is int, and its default is fast")
        self.assertEqual(self.wrong("PE2_OPTION_DEF(T, int, 1, 2)"), "a.c:1: T: an option has one default")
        self.assertEqual(self.wrong("PE2_OPTION_DEF(T, int, 1)\nPE2_OPTION_IS(T, 1)"), "a.c:2: T is int, and only an enum has members")

    def test_one_with_no_value_divides_by_zero_and_says_so_past_the_first_pass(self) -> None:
        header = options_of("PE2_OPTION_DEF(COUNT, int)\n").header({}, CONFIG)
        self.assertIn("#define pe2_option_COUNT (0/pe2_unset_COUNT)\n", header)
        before, after = header.split("#ifndef PE2_FIRST_PASS\n")
        self.assertNotIn("pe2_error", before)
        self.assertIn("COUNT has no value on this package", after)

    def test_the_config_is_named_and_tested(self) -> None:
        header = options_of("").header({}, CONFIG)
        self.assertIn('#define pe2_config_release "USA"\n#define pe2_config_release_is_USA 1\n', header)
        self.assertIn('#define pe2_config_target "psx"\n#define pe2_config_target_is_psx 1\n', header)

    def wrong(self, text: str, fields: dict | None = None) -> str:
        with self.assertRaises(SystemExit) as raised:
            options_of(text).header(fields or {}, CONFIG)
        return str(raised.exception.code)

    def test_what_is_wrong_is_said_where(self) -> None:
        self.assertEqual(self.wrong("PE2_OPTION(A)"), "a.c:1: A has no PE2_OPTION_DEF")
        self.assertEqual(self.wrong("PE2_OPTION_DEF(A, enum, b)\nPE2_OPTION_IS(A, c)"), "a.c:2: A has no member c; it has b")
        self.assertEqual(self.wrong("PE2_OPTION_DEF(A, float)"), "a.c:1: A: there is no option type float; an option is int, bool, "
                         "string, enum, an integer primitive or a registered type, or a fixed array of either")
        self.assertEqual(self.wrong("PE2_OPTION_DEF(A, int)\nPE2_OPTION_DEF(A, int)"), "a.c:2: A is defined already, at a.c:1")
        self.assertEqual(self.wrong("PE2_CONFIG(disc)"), "a.c:1: a config has no disc; it has name, release, target")
        self.assertEqual(self.wrong("PE2_OPTION_DEF(A, int)", {"option": {"A": "b"}}), "a.c:1: A is an int, and this package gives it b")
        self.assertEqual(self.wrong("PE2_OPTION_DEF(A, int)", {"option": {"A": True}}), "a.c:1: A is an int, and this package gives it True")


class Slots(unittest.TestCase):
    def test_args_are_kept_by_their_path(self) -> None:
        slots, reads, _ = slots_of(["PE2_BIND(a, model) PE2_BIND_ARG(a, _symbol, gA) PE2_BIND_ARG(a, b, _symbol, gB) PE2_VALUE(a) PE2_VALUE(a, _seam)"])
        self.assertEqual(slots["a"].type, "model")
        self.assertEqual(slots["a"].args, {((), "symbol"): ["gA"], (("b",), "symbol"): ["gB"]})
        self.assertEqual({read.name() for read in reads}, {"pe2_value_a", "pe2_value_a__seam"})

    def test_a_slot_says_what_it_is_after_its_type_and_a_query_is_named_as_pasted(self) -> None:
        slots, _, queries = slots_of(["PE2_BIND(self, package, _self, _hidden) PE2_BIND(file, file) PE2_QUERY(file, contains, self)"])
        self.assertEqual((slots["self"].type, slots["self"].attributes), ("package", {"self", "hidden"}))
        self.assertEqual([query.name() for query in queries], ["pe2_query_file_contains_self"])


def tree(root: Path, text: str) -> Declarations:
    (root / "a.toml").write_text(text)
    return Declarations(root)


FIXTURE = """
[vram]
uid = "uid://v"
template = "vram_slot"
rect = [320, 256, 256, 256]

[palette_row]
uid = "uid://r"
template = "vram_slot"
rect = [0, 480, 256, 1]

[sheet]
uid = "uid://s"
template = "texture"
mode = "indexed_8bpp"
size = [256, 256]
page = { slot = { uid = "uid://v", path = "res://a/vram.node" }, at = [64, 0] }
palettes.main = { slot = { uid = "uid://r", path = "res://a/palette_row.node" }, at = [16, 2] }
cells.right = { rect = [128, 0, 128, 256], palette = "main" }

[lit]
uid = "uid://m"
template = "material"
shader = "textured"
blend = "add"
main = { uid = "uid://s", path = "res://a/sheet.node", output = "cells/right" }

[flat]
uid = "uid://f"
template = "material"
shader = "colored"

[body]
uid = "uid://b"
template = "model"

[prefab]
uid = "uid://p"
template = "prefab"

[holder]
uid = "uid://h"
template = "file"
slots = [{ name = "package", type = "package", package = { uid = "uid://b", path = "res://a/body.node" } }]

[prefab.groups.outfit.casual.body]
model = { slot = { uid = "uid://b", path = "res://a/body.node" } }
"""


class Tokens(unittest.TestCase):
    def setUp(self) -> None:
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.declared = tree(Path(folder.name), FIXTURE)
        self.reader = Reader(self.declared, None, "USA")

    def said(self, node: Declaration, derived: str | None, args: dict) -> str:
        with self.assertRaises(ValueError) as raised:
            tokens(self.reader, node, derived, args)
        return str(raised.exception)

    def test_a_material_says_its_page_and_palette(self) -> None:
        lit = self.declared.by_path["a/lit"]
        # 320 + 64 + 128 / 2 = 448, page 7; 256, page 1; add, 8 bits.
        self.assertEqual(material_tpage(self.reader, lit, {}), "0xB7")
        self.assertEqual(material_clut(self.reader, lit, {}), "0x7881")

    def test_what_a_tokenizer_cannot_make_it_says(self) -> None:
        by = self.declared.by_path
        self.assertEqual(self.said(by["a/flat"], "tpage", {}), "it has no texture")
        self.assertEqual(self.said(by["a/flat"], None, {}), "it has no value as a whole")
        self.assertEqual(self.said(by["a/lit"], "width", {}), "it derives no width; it derives tpage, clut")
        self.assertEqual(self.said(by["a/body"], None, {}), "it needs symbol")
        self.assertEqual(self.said(by["a/prefab"], None, {"symbol": ["g"]}), "it needs member")
        self.assertEqual(self.said(by["a/vram"], None, {}), "nothing tokenizes a vram_slot")

    def test_an_image_is_read_by_its_declaration(self) -> None:
        sheet = self.declared.by_path["a/sheet"]
        self.assertEqual(tokens(self.reader, sheet, "width_words", {}), "128")
        self.assertEqual(tokens(self.reader, sheet, "height", {}), "256")

    def test_a_member_is_a_model_read_from_the_prefab(self) -> None:
        node, reads_as, args = reached(self.declared, "uid://p#outfit/casual/body", ())
        self.assertEqual((node.path, reads_as, args), ("a/prefab", "model", {"member": ["outfit/casual/body"]}))
        self.assertEqual(self.reader.member(node, "outfit/casual/body").path, "a/body")

    def test_a_slot_is_what_fills_it(self) -> None:
        bound = {"body": "uid://p#outfit/casual/body", "eyes": "uid://s"}
        self.assertIsNone(slot_problem(self.declared, bound, "body", "model"))
        self.assertIsNone(slot_problem(self.declared, bound, "eyes", "image"))
        self.assertEqual(slot_problem(self.declared, bound, "eyes", "model"), "eyes is declared model, and what fills it is image")
        self.assertEqual(slot_problem(self.declared, bound, "gone", "model"), "gone: nothing fills it on this package")


    def test_a_slot_contains_what_it_names_anywhere(self) -> None:
        slots = {"file": Slot("file"), "self": Slot("package", {"self"})}
        query = Query("file", "contains", ("self",))
        self.assertEqual(answer(self.declared, slots, {"file": "uid://h", "self": "uid://b"}, query), "1")
        self.assertEqual(answer(self.declared, slots, {"file": "uid://h", "self": "uid://s"}, query), "0")
        with self.assertRaises(ValueError):
            answer(self.declared, slots, {"file": "uid://h"}, Query("file", "holds", ("self",)))


if __name__ == "__main__":
    unittest.main()
