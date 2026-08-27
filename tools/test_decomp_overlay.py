#!/usr/bin/env python3
"""Tests for overlay discovery that do not depend on a live GCC 2.8.1 build."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import decomp_overlay as ov


class DiscoverOverlaysTests(unittest.TestCase):
    def test_discovers_current_yaml_overlays(self):
        names = {o.name for o in ov.discover_overlays("USA")}
        self.assertIn("main", names)
        self.assertIn("gameplay", names)
        self.assertIn("title", names)

    def test_list_nonmatchings_is_not_depth_limited(self):
        dirs = {p.as_posix() for p in ov.list_nonmatching_dirs("USA")}
        self.assertTrue(any(d.endswith("gameplay/nonmatchings") for d in dirs))
        self.assertTrue(any(d.endswith("main/nonmatchings") for d in dirs))

    def test_find_gameplay_and_main_functions(self):
        gp = ov.find_function("func_800D0614", "USA")
        self.assertIsNotNone(gp)
        self.assertEqual(gp.overlay.name, "gameplay")
        self.assertEqual(gp.kind, "nonmatchings")
        self.assertTrue(str(gp.asm_file).endswith("func_800D0614.s"))
        self.assertIn("3688", gp.unit)

        main = ov.find_function("Fs_LoadImageStrip", "USA")
        self.assertIsNotNone(main)
        self.assertEqual(main.overlay.name, "main")
        self.assertIn("src/main/fs.c", ov.loc_to_dict(main)["c_file"])

    def test_nested_overlay_without_yaml(self):
        """Future room overlays live deeper than asm/USA/<name>/nonmatchings."""
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            asm = (
                repo
                / "asm"
                / "USA"
                / "stage1"
                / "101"
                / "nonmatchings"
                / "room"
            )
            asm.mkdir(parents=True)
            (asm / "func_NESTED.s").write_text("glabel func_NESTED\n jr $ra\n", encoding="utf-8")
            with mock.patch.object(ov, "REPO_ROOT", repo):
                loc = ov.find_function("func_NESTED", "USA")
                self.assertIsNotNone(loc)
                self.assertEqual(loc.overlay.name, "stage1/101")
                self.assertEqual(loc.unit, "room")
                self.assertEqual(
                    loc.include_asm_folder, "stage1/101/nonmatchings/room"
                )
                dirs = [p.as_posix() for p in ov.list_nonmatching_dirs("USA")]
                self.assertTrue(any(d.endswith("stage1/101/nonmatchings") for d in dirs))

    def test_pack_context_mentions_overlay_and_include_asm(self):
        brief = ov.pack_context("func_800D0614", "USA")
        self.assertIn("Overlay: `gameplay`", brief)
        self.assertIn("INCLUDE_ASM", brief)
        self.assertIn("func_800D0614", brief)
        self.assertIn("Do **not** run `./tools/claude`", brief)


if __name__ == "__main__":
    unittest.main()
