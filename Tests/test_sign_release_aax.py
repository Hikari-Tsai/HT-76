"""Offline checks for the local release-signing tool; never contact GitHub/PACE."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

spec = importlib.util.spec_from_file_location(
    "sign_release_aax", Path(__file__).resolve().parents[1] / "scripts/sign-release-aax.py")
sign = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sign)


class SigningTests(unittest.TestCase):
    def test_latest_includes_prerelease_but_not_drafts(self):
        releases = [dict(id=1, draft=False, published_at="2026-01-01", prerelease=False),
                    dict(id=2, draft=False, published_at="2026-02-01", prerelease=True),
                    dict(id=3, draft=True, published_at=None)]
        self.assertEqual(sign.latest_release(releases)["id"], 2)

    def test_asset_selection_requires_exactly_one_universal_aax_zip(self):
        good = {"name": "HT-76-test-macos-universal-AAX.zip"}
        self.assertEqual(sign.select_asset({"assets": [good, {"name": "windows-AAX.zip"}]}), good)
        for assets in ([], [good, good], [{"name": "../" + good["name"]}]):
            with self.assertRaises(ValueError):
                sign.select_asset({"assets": assets})

    def test_archive_rejects_corruption_and_traversal(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "test.zip"
            for name in ("bundle/readme", "../escape", "/absolute"):
                with zipfile.ZipFile(p, "w") as z:
                    z.writestr(name, "payload")
                if name == "bundle/readme":
                    sign.validate_archive(p)
                else:
                    with self.assertRaises(ValueError):
                        sign.validate_archive(p)
            p.write_bytes(b"corrupt")
            with self.assertRaises((ValueError, zipfile.BadZipFile)):
                sign.validate_archive(p)

    def test_download_digest_mismatch_stops(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "file"
            p.write_bytes(b"abc")
            asset = dict(size=3, digest="sha256:" + hashlib.sha256(b"abc").hexdigest())
            sign.verify_download(p, asset)
            p.write_bytes(b"xyz")
            with self.assertRaises(ValueError):
                sign.verify_download(p, asset)

    def test_remote_change_stops_replacement(self):
        original = dict(id=1, tag_name="v1", draft=False, prerelease=True, immutable=False,
                        body="notes", assets=[dict(id=2, name="file", size=3, digest="abc")])
        sign.require_unchanged(original, copy.deepcopy(original))
        for field in ("body", "prerelease", "assets"):
            changed = copy.deepcopy(original)
            if field == "assets":
                changed[field][0]["digest"] = "changed"
            else:
                changed[field] = "changed"
            with self.assertRaises(ValueError):
                sign.require_unchanged(original, changed)

    def test_notes_are_idempotent_and_preserve_other_content(self):
        body = "Original release description\n\n## Other\nKeep me.\n"
        notes = sign.release_notes(body, "https://example.com/aax.zip")
        self.assertIn("Keep me.", notes)
        self.assertIn("DMG", notes)
        self.assertIn("Windows", notes)
        self.assertEqual(sign.release_notes(notes, "https://example.com/aax.zip"), notes)

    def test_failed_signing_stops_before_verification(self):
        with tempfile.TemporaryDirectory() as tmp:
            runner = sign.Runner(Path(tmp))
            with patch.object(sign.subprocess, "run") as run:
                run.return_value.returncode = 1
                with self.assertRaises(RuntimeError):
                    sign.sign_bundle(runner, "/fake/wraptool", dict(
                        PACE_CUSTOMER_NUMBER="test-number", PACE_CUSTOMER_NAME="test-name",
                        PACE_SIGN_ID="test-id"), Path("input"), Path("output"))
                self.assertEqual(run.call_count, 1)

    def test_publish_refuses_changed_remote_before_upload(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            notes = root / "notes.md"
            notes.write_text("new notes")
            original = dict(id=1, body="old notes", tag_name="v1", assets=[])
            changed = dict(original, body="someone else's edit")
            runner = sign.Runner(root)
            with patch.object(runner, "run") as run:
                run.return_value = json.dumps(changed)
                with self.assertRaises(ValueError):
                    sign.publish_release(runner, "owner/repo", original, {}, root / "file.zip", notes)
                self.assertEqual(run.call_count, 1)
                self.assertEqual(run.call_args.args[0], "check-remote")

    def test_publish_verifies_replacement_and_preserves_prerelease(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            upload = root / "HT-76-test-macos-universal-AAX.zip"
            upload.write_bytes(b"verified archive fixture")
            notes = root / "notes.md"
            notes.write_text("updated notes\n")
            old_asset = dict(id=10, name=upload.name, size=1, digest="old")
            other_asset = dict(id=11, name="windows.zip", size=2, digest="unchanged")
            original = dict(id=1, body="old notes", tag_name="v1", prerelease=True,
                            assets=[old_asset, other_asset])
            replacement = dict(old_asset, id=12, size=upload.stat().st_size, digest=sign.digest(upload))
            updated = dict(original, body=notes.read_text(), assets=[replacement, other_asset],
                           html_url="https://example.com/release")
            runner = sign.Runner(root)
            with patch.object(runner, "run") as run:
                run.side_effect = [json.dumps(original), "", "", json.dumps(updated)]
                sign.publish_release(runner, "owner/repo", original, old_asset, upload, notes)
                self.assertEqual([call.args[0] for call in run.call_args_list],
                                 ["check-remote", "upload", "release-notes", "verify-remote"])
                self.assertIn("--clobber", run.call_args_list[1].args[1])
                self.assertNotIn("--latest", run.call_args_list[2].args[1])


if __name__ == "__main__":
    unittest.main()
