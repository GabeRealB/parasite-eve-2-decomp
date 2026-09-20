import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from land_overlay import merge_difficult


class MergeDifficultTests(unittest.TestCase):
    """The list is merged between a long-lived worktree and a moving trunk.

    A worktree is cut once and lands at the end of a sweep, so by then trunk
    carries entries the worktree has never seen and has cleared entries the
    worktree still lists. Only what the worktree itself changed may be applied.
    """

    def merge(self, current, incoming, base):
        return merge_difficult(current, incoming, base)

    def test_worktree_removal_is_honoured(self):
        base = "fn_a 1 90\nfn_b 2 91\n"
        out = self.merge(base, "fn_a 1 90\n", base)
        self.assertNotIn("fn_b", out)

    def test_worktree_addition_lands(self):
        base = "fn_a 1 90\n"
        out = self.merge(base, "fn_a 1 90\nfn_b 2 91\n", base)
        self.assertIn("fn_b 2 91", out)

    def test_trunk_clearing_is_not_undone(self):
        # Trunk matched fn_b and removed it while this worktree was live; the
        # worktree still carries the give-up it was cut with.
        base = "fn_a 1 90\nfn_b 2 91\n"
        out = self.merge("fn_a 1 90\n", base, base)
        self.assertNotIn("fn_b", out)

    def test_trunk_score_is_not_regressed(self):
        base = "fn_a 1 90\n"
        out = self.merge("fn_a 9 99\n", base, base)
        self.assertIn("fn_a 9 99", out)

    def test_worktree_rescore_wins_over_trunk(self):
        base = "fn_a 1 90\n"
        out = self.merge("fn_a 9 99\n", "fn_a 4 95\n", base)
        self.assertIn("fn_a 4 95", out)

    def test_both_sides_add_different_names(self):
        base = "fn_a 1 90\n"
        out = self.merge("fn_a 1 90\nfn_t 3 92\n", "fn_a 1 90\nfn_w 4 93\n", base)
        self.assertIn("fn_t 3 92", out)
        self.assertIn("fn_w 4 93", out)

    def test_empty_base_keeps_both_sides(self):
        out = self.merge("fn_a 1 90\n", "fn_b 2 91\n", "")
        self.assertIn("fn_a 1 90", out)
        self.assertIn("fn_b 2 91", out)


if __name__ == "__main__":
    unittest.main()
