"""What a plan makes of several releases, and how a release is declared after another."""
import sys
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "buildsteps"))
import plan as plans  # noqa: E402
import release  # noqa: E402


class Releases(unittest.TestCase):
    def test_what_releases_declare_alike_is_one_file_made_of_any_of_them(self):
        groups = plans.by_what_they_declare({"USA": "a", "EUR": "a", "JAP": "b"})
        self.assertEqual(groups, [["USA", "EUR"], ["JAP"]])
        made = plans.Plan(["USA", "EUR", "JAP"])
        plans.emit_by_release(made, "n", "native.bin", groups, {"USA": "u", "EUR": "e", "JAP": "j"})
        self.assertEqual(made.emits["n"], {"native.bin": ["u", "e"], "JAP/native.bin": "j"})

    def test_what_a_release_declares_nothing_of_is_unlike_anything(self):
        self.assertEqual(plans.by_what_they_declare({"USA": None, "EUR": None}), [["USA"], ["EUR"]])

    def test_a_place_taking_any_of_several_is_placed_after_all_of_them(self):
        ops = {"t": {"id": "t", "op": "x", "in": {"chunks": [{"any": ["b", "a"]}]}}, "a": {"id": "a", "op": "y"},
               "b": {"id": "b", "op": "y"}}
        self.assertEqual([o["id"] for o in plans.ordered(ops)], ["a", "b", "t"])
        self.assertEqual(plans.any_of(["a", "a"]), "a")
        self.assertEqual(plans.any_of(["a", "b"]), {"any": ["a", "b"]})

    def test_a_release_block_keeps_the_others_lines_but_what_differs(self):
        theirs = ["[x.from.USA]", 'at = [{ disc = "disk1", slot = "SLUS_010.42" }]', "offset = 4", 'sha1 = "aa"']
        ours = {"at": [{"disc": "disk1", "slot": "SLES_025.58"}], "offset": 4, "sha1": "bb"}
        self.assertEqual(release.block(theirs, "x", "EUR", ours), [
            "[x.from.EUR]", 'at = [{ disc = "disk1", slot = "SLES_025.58" }]', "offset = 4", 'sha1 = "bb"'])

    def test_a_declaration_is_placed_for_the_release_as_for_the_other(self):
        names = {("disk1", "SLUS_010.42"): "SLES_025.58"}
        args = {"release": "USA", "reads": [{"disc": "disk1", "slot": "SLUS_010.42"}],
                "images": {"p": {"from": {"USA": {"sha1": "aa"}}}}}
        out = release.as_release(args, "USA", "EUR", names)
        self.assertEqual(out["release"], "EUR")
        self.assertEqual(out["reads"], [{"disc": "disk1", "slot": "SLES_025.58"}])
        self.assertEqual(out["images"]["p"]["from"], {"USA": {"sha1": "aa"}, "EUR": {"sha1": "aa"}})


if __name__ == "__main__":
    unittest.main()
