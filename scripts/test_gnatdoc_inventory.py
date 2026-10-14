"""Source-visibility regression tests for the documentation campaign."""

import tempfile
import unittest
from pathlib import Path

from gnatdoc_inventory import inventory, specifications


class InventoryTests(unittest.TestCase):
    def test_private_part_before_nested_instance_does_not_hide_public_unit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "ada/src"
            source.mkdir(parents=True)
            (source / "demo.ads").write_text(
                "package Demo is\n   type Value is private;\nprivate\n"
                "   package Items is new Containers (Integer);\n"
                "   type Value is new Integer;\nend Demo;\n")
            (source / "demo-hidden.ads").write_text(
                "private package Demo.Hidden is\nend Demo.Hidden;\n")
            public, private = specifications(root)
            self.assertEqual(list(public), ["Demo"])
            self.assertEqual(list(private), ["Demo.Hidden"])
            self.assertEqual(inventory(root)["totals"]["declarations"], 1)

    def test_renaming_is_not_silently_counted_as_rendered(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "ada/src"
            source.mkdir(parents=True)
            (source / "demo.ads").write_text(
                "package Demo is\n   package Alias renames Other;\nend Demo;\n")
            report = inventory(root)
            self.assertEqual(report["unsupported_generator_constructs"][0]["identity"],
                             "Demo.Alias")
            self.assertEqual(report["totals"]["declarations"], 1)

    def test_empty_source_inventory_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "no public"):
                specifications(Path(directory))


if __name__ == "__main__":
    unittest.main()