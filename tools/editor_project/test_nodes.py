#!/usr/bin/env python3
"""Tests of a layer's nodes as written from declarations: `python3 tools/editor_project/test_nodes.py`."""
from __future__ import annotations

import sys
import tomllib
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import init  # noqa: E402
import nodes  # noqa: E402
import toml_out  # noqa: E402

STACK = """
[[stack]]
op = "file"
file = "{file}"

[[stack]]
id = "image"
op = "pe2.images@ora"

[[stack]]
op = "output"
name = "native"
"""
REGISTRY = {
    "templates": {
        "texture": {"node_type": "pe2.images@image", "stack": STACK},
        "file": {"node_type": "pe2.game@file", "stack": '[[stack]]\nop = "pe2.game@file"\n'},
        "binary": {"node_type": "pe2.game@binary", "stack": '[[stack]]\nop = "file"\nfile = "{file}"\n'},
    },
    "operations": {"pe2.images@ora": {"derives": ["size", "cells.*.rect"], "inverse": "pe2.images@ora_source"}},
    "host_operations": ["file", "constant", "below", "output", "preview", "graph"],
}
REF = {"uid": "uid://abc", "path": "res://engine/vram.node"}


class Writing(unittest.TestCase):
    def test_what_is_written_reads_back_as_it_was(self):
        doc = {
            "stack": [{"op": "file", "file": "a b.ora", "faces": [{"font": "uid://x", "band": [0, 47]}]}, {"op": "o"}],
            "original": {"USA": {"sha1": "00"}},
        }
        doc["stack"][0]["text"] = 'quote " and \\ and \n'
        self.assertEqual(tomllib.loads(toml_out.document(doc, "# head")), doc)

    def test_a_folder_uid_is_the_same_for_the_same_path_and_never_one_held(self):
        first = init.derived_uid("stages/akropolis", set())
        self.assertEqual(first, init.derived_uid("stages/akropolis", set()))
        self.assertNotEqual(first, init.derived_uid("stages/akropolis", {first}))


class Nodes(unittest.TestCase):
    def test_references_are_their_uids_and_outputs(self):
        made = nodes.uids_alone({"page": {"slot": REF, "at": [0, 0]}, "p": [dict(REF, output="palettes/a")]})
        self.assertEqual(made, {"page": {"slot": "uid://abc", "at": [0, 0]}, "p": ["uid://abc#palettes/a"]})

    def test_a_texture_is_its_source_without_what_its_import_derives(self):
        table = {
            "uid": "uid://t", "template": "texture", "mode": "indexed_4bpp", "size": [16, 16], "format": "x",
            "page": {"slot": REF, "at": [0, 0]}, "cells": {"a": {"rect": [0, 0, 1, 1], "palette": "p"}},
            "import": {"importer": "pe2.images@ora", "source": "source.ora", "kept": 1},
            "from": {"USA": {"sha1": "ff", "at": [{"stage": 0, "file": 1, "slot": "image"}]}},
        }
        node = nodes.asset_node("ui/t", table, REGISTRY, ["USA"])
        self.assertEqual(node["stack"][0], {"op": "file", "file": "source.ora"})
        self.assertEqual(
            node["stack"][1],
            {"id": "image", "op": "pe2.images@ora", "mode": "indexed_4bpp", "page": {"slot": "uid://abc", "at": [0, 0]},
             "cells": {"a": {"palette": "p"}}, "kept": 1},
        )
        self.assertEqual(node["original"], {"USA": {"sha1": "ff"}})

    def test_what_is_read_off_a_disc_is_its_native_file(self):
        table = {"uid": "uid://b", "template": "binary", "format": "exe",
                 "from": {"USA": {"sha1": "ff", "at": [{"disc": "disk1", "slot": "SLUS_010.42"}]}}}
        node = nodes.asset_node("main/exe", table, REGISTRY, ["USA"])
        self.assertEqual(node["stack"], [{"op": "file", "file": "native.exe", "format": "exe"}])

    def test_what_is_read_of_nothing_names_no_file(self):
        table = {"uid": "uid://f", "template": "file", "from": {"USA": {"at": [{"stage": 0, "file": 9}]}}}
        node = nodes.asset_node("x/f", table, REGISTRY, ["USA"])
        self.assertEqual(node, {"stack": [{"op": "pe2.game@file"}]})

    def test_what_a_node_lays_after_its_template_follows_its_stack(self):
        table = {"uid": "uid://f", "template": "file", "level": 2,
                 "layers": [{"op": "uid://translated"}, {"op": "output", "name": "strings"}]}
        node = nodes.asset_node("x/f", table, REGISTRY, ["USA"])
        self.assertEqual(node["stack"], [{"op": "pe2.game@file", "level": 2}, {"op": "uid://translated"},
                                         {"op": "output", "name": "strings"}])

    def test_settings_go_on_the_first_own_operation_before_an_output(self):
        stack = [{"op": "file"}, {"op": "output"}, {"op": "x"}]
        self.assertEqual(nodes.home(stack), 0)
        self.assertEqual(nodes.home([{"op": "file"}, {"op": "x"}, {"op": "output"}]), 1)
        self.assertIsNone(nodes.home([]))


if __name__ == "__main__":
    unittest.main()
