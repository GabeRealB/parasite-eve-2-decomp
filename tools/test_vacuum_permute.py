#!/usr/bin/env python3
from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import vacuum_permute as vp


class SeedPickerTests(unittest.TestCase):
    def _scratch(self, files: dict[str, str], log: str) -> Path:
        tmp = Path(tempfile.mkdtemp())
        for name, text in files.items():
            (tmp / name).write_text(text, encoding="utf-8")
        (tmp / "match_log.txt").write_text(log, encoding="utf-8")
        return tmp

    def test_prefers_unpinned_within_window(self):
        scratch = self._scratch(
            {
                "base_1.c": "void f(void) {}\n",
                "base_2.c": 'register s32 keep asm("a0");\nvoid f(void) { keep = 1; }\n',
                "base_3.c": "void f(void) { int x = 1; }\n",
            },
            "base_1.c 91.000%\nbase_2.c 99.500%\nbase_3.c 99.200%\n",
        )
        seeds = vp.parse_match_log(scratch)
        picked = vp.pick_seed(seeds, 95.0)
        self.assertIsNotNone(picked)
        self.assertEqual(picked.path.name, "base_3.c")
        self.assertFalse(picked.pinned)

    def test_skips_below_min_score(self):
        scratch = self._scratch(
            {"base_1.c": "void f(void) {}\n"},
            "base_1.c 90.000%\n",
        )
        self.assertIsNone(vp.pick_seed(vp.parse_match_log(scratch), 95.0))

    def test_falls_back_to_pinned_best(self):
        scratch = self._scratch(
            {
                "base_1.c": 'register s32 keep asm("a0");\n',
                "base_2.c": "void f(void) {}\n",
            },
            "base_1.c 99.900%\nbase_2.c 91.000%\n",
        )
        picked = vp.pick_seed(vp.parse_match_log(scratch), 95.0)
        self.assertEqual(picked.path.name, "base_1.c")
        self.assertTrue(picked.pinned)

    def test_strip_register_asm_keeps_gte(self):
        src = (
            'register s32 keep asm("a0");\n'
            "keep = arg0;\n"
            '__asm__ volatile("nop; nop; .word 0x4B98003D");\n'
            'register Task* p asm("s0");\n'
        )
        out = vp.strip_register_asm(src)
        self.assertNotIn('asm("a0")', out)
        self.assertNotIn('asm("s0")', out)
        self.assertIn("s32 keep", out)
        self.assertIn("Task* p", out)
        self.assertIn("__asm__ volatile", out)


if __name__ == "__main__":
    unittest.main()
