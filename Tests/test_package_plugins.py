"""Packaging contract tests; synthetic PE fixtures are not plugin build tests."""

import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location("packager", Path(__file__).resolve().parents[1] / "scripts/package-plugins.py")
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


class PackagingTests(unittest.TestCase):
    def make_fixture(self, build_dir, format_name, machine=0x8664):
        bundle = build_dir / "FieldEffect_artefacts" / "Release" / format_name / f"HT-76.{packager.EXTENSIONS[format_name]}"
        binary = packager.executable_path(bundle, "windows", format_name)
        binary.parent.mkdir(parents=True)
        data = bytearray(256)
        data[:2] = b"MZ"
        struct.pack_into("<I", data, 0x3C, 128)
        data[128:132] = b"PE\0\0"
        struct.pack_into("<H", data, 132, machine)
        binary.write_bytes(data)
        (bundle / "Contents" / "resource.txt").write_text("keep complete bundle", encoding="utf-8")
        return binary

    def test_windows_formats_keep_bundle_resources_metadata_and_checksums(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for format_name in ["VST3", "AAX"]:
                self.make_fixture(root / "build", format_name)
            archives = packager.package(root / "build", root / "dist", "windows", "x64", ["VST3", "AAX"], "a" * 40)
            self.assertEqual(len(archives), 2)
            for archive in archives:
                with zipfile.ZipFile(archive) as zipped:
                    names = zipped.namelist()
                    metadata = json.loads(zipped.read(next(n for n in names if n.endswith("/build-info.json"))))
                    self.assertEqual(metadata["architecture"], "x64")
                    self.assertEqual(metadata["revision"], "a" * 40)
                    self.assertFalse(metadata["pace_signed"])
                    self.assertTrue(any(n.endswith("/Contents/resource.txt") for n in names))
                    self.assertEqual(sum(n.endswith(".aaxplugin") or n.endswith(".vst3") for n in names), 1)
                digest = hashlib.sha256(archive.read_bytes()).hexdigest()
                self.assertEqual(archive.with_suffix(".zip.sha256").read_text().split()[0], digest)

    def test_wrong_architecture_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            binary = self.make_fixture(Path(temporary), "AAX", machine=0x14C)
            with self.assertRaisesRegex(ValueError, "Expected Windows x64"):
                packager.validate_binary(binary, "windows", "x64")

    def test_missing_format_produces_no_partial_deliverables(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_fixture(root / "build", "VST3")
            with self.assertRaisesRegex(ValueError, "Missing plugin executable"):
                packager.package(root / "build", root / "dist", "windows", "x64", ["VST3", "AAX"], "local")
            self.assertFalse((root / "dist").exists())

    def test_windows_au_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "Unsupported formats"):
            packager.package(Path("unused"), Path("unused"), "windows", "x64", ["AU"], "local")


if __name__ == "__main__":
    unittest.main()
