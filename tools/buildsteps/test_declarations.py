#!/usr/bin/env python3
"""The declarations reader, on a small tree of its own and on the real ones.

    python3 tools/buildsteps/test_declarations.py
"""
import hashlib
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from declarations import DECLARATIONS, Declarations, ROOT, uid_of  # noqa: E402
from stage0 import Stage0  # noqa: E402

ROM = ROOT / "rom"
OUTFITS = ("aya_mist", "aya_dryfield", "aya_akropolis", "aya_range")


def tree(root: Path, files: dict[str, str]) -> Declarations:
    for name, text in files.items():
        file = root / name
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(text)
    return Declarations(root)


class Reading(unittest.TestCase):
    def setUp(self) -> None:
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.root = Path(folder.name)

    def test_a_table_with_a_uid_is_an_asset_of_its_group(self) -> None:
        declared = tree(self.root, {"a/b.toml": '[one]\nuid = "uid://1"\n\n[plain]\nvalue = 1\n'})
        self.assertEqual(list(declared.by_path), ["a/b/one"])

    def test_the_discs_layout_is_no_asset_and_a_file_beside_it_is(self) -> None:
        declared = tree(self.root, {"disc/USA/stage0.toml": '[s]\nuid = "uid://1"\n', "disc.toml": '[d]\nuid = "uid://2"\n'})
        self.assertEqual(list(declared.by_path), ["disc/d"])

    def test_a_member_of_a_prefabs_group_comes_to_its_model(self) -> None:
        declared = tree(self.root, {"c.toml": """
[model]
uid = "uid://m"

[prefab]
uid = "uid://p"

[prefab.groups.outfit.casual.body]
model = { slot = { uid = "uid://m", path = "res://c/model.node" } }
"""})
        self.assertEqual(declared.holding("uid://p#outfit/casual/body").path, "c/model")
        self.assertEqual(declared.holding("uid://p").path, "c/prefab")
        self.assertEqual(uid_of("uid://p#outfit/casual/body"), ("uid://p", "outfit/casual/body"))

    def test_a_reference_naming_nothing_is_a_problem(self) -> None:
        declared = tree(self.root, {"c.toml": '[a]\nuid = "uid://1"\nslot = { uid = "uid://gone", path = "res://c/b.node" }\n'})
        self.assertEqual(declared.problems(), ["c/a: uid://gone names nothing"])

    def test_a_text_naming_nothing_is_a_problem_too(self) -> None:
        declared = tree(self.root, {"c.toml": '[a]\nuid = "uid://1"\nbind.x = "uid://gone#outfit/a"\n'})
        self.assertEqual(declared.problems(), ["c/a: uid://gone names nothing"])

    def test_a_uid_twice_is_a_problem(self) -> None:
        declared = tree(self.root, {"c.toml": '[a]\nuid = "uid://1"\n\n[b]\nuid = "uid://1"\n'})
        self.assertEqual(declared.problems(), ["uid://1 is declared twice"])

    def test_a_stage_0_file_is_its_groups_by_its_id(self) -> None:
        declared = tree(self.root, {"c.toml": """
[thing]
uid = "uid://t"

[player]
uid = "uid://g"
template = "file_group"
files = [{ name = "thing", id = 200, asset = { uid = "uid://t", path = "res://c/thing.node" } }]

[player.from.USA]
at = [{ stage = 0, group = 1 }]
"""})
        found = declared.stage0_file(10200)
        assert found is not None
        self.assertEqual((found[0].path, found[1]), ("c/thing", "thing"))
        self.assertIsNone(declared.stage0_file(10300))
        self.assertIsNone(declared.stage0_file(20200))


    def test_a_package_is_built_where_a_target_links_it(self) -> None:
        declared = tree(self.root, {"packages.toml": """
[title]
uid = "uid://1"
template = "package"
slot = "resident"
sources = ["src/*.c", "!src/*.inc.c"]

[title.target.psx]
link_script = "psx/link.ld"

[core]
uid = "uid://2"
template = "library"

[main]
uid = "uid://3"
template = "library"

[main.target.psx]
symbols = "psx/symbols.ld"

[no_target]
uid = "uid://4"
template = "package"
"""})
        package, = declared.packages()
        self.assertEqual((package.path, package.name, package.slot), ("packages/title", "title", "resident"))
        self.assertEqual(package.link_script, Path("packages/title/psx/link.ld"))
        self.assertEqual(declared.published(), [Path("packages/main/psx/symbols.ld")])
        self.assertEqual(declared.package_names(), {"title", "core", "main", "no_target"})
        self.assertEqual(declared.packages("pc"), [])
        self.assertEqual(declared.public(), {})

    def test_sources_are_globs_a_bang_leaves_out(self) -> None:
        folder = self.root / "packages/title"
        (folder / "src").mkdir(parents=True)
        for name in ("a.c", "b.inc.c"):
            (folder / "src" / name).write_text("")
        declared = tree(self.root, {"packages.toml": """
[title]
uid = "uid://1"
template = "package"
sources = ["src/*.c", "!src/*.inc.c"]

[title.target.psx]
link_script = "psx/link.ld"
"""})
        package, = declared.packages()
        here = Path.cwd()
        self.addCleanup(os.chdir, here)
        os.chdir(self.root)
        self.assertEqual(package.units(), [Path("packages/title/src/a.c")])


class Real(unittest.TestCase):
    declared: Declarations

    @classmethod
    def setUpClass(cls) -> None:
        cls.declared = Declarations(DECLARATIONS)

    def test_nothing_is_wrong_with_them(self) -> None:
        self.assertEqual(self.declared.problems(), [])

    def test_every_slot_of_ayas_packages_is_where_it_says(self) -> None:
        """What a package binds, read from the disc as declared, is the bytes declared."""
        if not (ROM / "USA").is_dir():
            self.skipTest("no disc here")
        disc = Stage0(self.declared, ROM, "USA")
        checked = 0
        for name in OUTFITS:
            package = next(one for one in self.declared.packages() if one.name == name)
            for slot, reference in package.bound.items():
                held = self.declared.holding(reference)
                stored = held.stored("USA")
                self.assertIsNotNone(stored, f"{name}.{slot}: {held.path}")
                file = disc.slot(stored.files[0], stored.slot)
                bytes_there = file[stored.offset:stored.offset + stored.size]
                self.assertEqual(hashlib.sha1(bytes_there).hexdigest(), stored.sha1, f"{name}.{slot}: {held.path}")
                checked += 1
        self.assertGreater(checked, 30)


if __name__ == "__main__":
    unittest.main()
