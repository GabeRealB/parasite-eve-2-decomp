import unittest

from dist import normalize_jumptable_references as normalize_score
from normalize_asm import normalize_jumptable_references as normalize_diff


class JumpTableNormalizationTest(unittest.TestCase):
    def test_legacy_and_actor_tables(self):
        for normalize in (normalize_score, normalize_diff):
            for symbol in ("jtbl_80013EB0", "Actor01600_Jt001BC"):
                for relocation in ("hi", "lo"):
                    self.assertEqual(
                        normalize(f"lui v0,%{relocation}({symbol})"),
                        f"lui v0,%{relocation}(.rodata)",
                    )

    def test_other_actor_symbols_are_preserved(self):
        for normalize in (normalize_score, normalize_diff):
            for symbol in ("Actor01600_Fn04EB0", "Actor01600_D001BC", "Actor01600_Jt001BC_extra"):
                self.assertEqual(normalize(symbol), symbol)


if __name__ == "__main__":
    unittest.main()
