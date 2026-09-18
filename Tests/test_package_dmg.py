"""DMG metadata, Installer selection and uninstaller identity safeguards."""

import hashlib
import importlib.util
import json
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('dmg_packager', ROOT / 'scripts/package-dmg.py')
dmg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dmg)


class DmgTests(unittest.TestCase):
    def test_choices_architecture_locations_and_all_formats_selected(self):
        for arch in ('arm64', 'x86_64', 'universal'):
            xml = ET.fromstring(dmg.distribution('1.0.0', arch))
            expected = 'arm64,x86_64' if arch == 'universal' else arch
            self.assertEqual(xml.find('options').get('hostArchitectures'), expected)
            self.assertEqual(xml.find('domains').get('enable_anywhere'), 'false')
            self.assertEqual(xml.find('volume-check/allowed-os-versions/os-version').get('min'), '12.0')
            choices = {node.get('id'): node for node in xml.findall('choice')}
            self.assertEqual(set(choices), {'AU', 'VST3', 'AAX'})
            self.assertEqual(choices['AAX'].get('start_selected'), 'true')
            self.assertEqual(choices['AU'].get('start_selected'), 'true')
            self.assertEqual(choices['VST3'].get('start_selected'), 'true')
            for choice in choices.values():
                self.assertNotIn('selected', choice.attrib)  # Let users change the selection.
            refs = {node.get('id') for node in xml.findall('pkg-ref')}
            self.assertEqual(refs, {node.find('pkg-ref').get('id') for node in choices.values()})

    def archive(self, root, arch='arm64', extra_path=None):
        archive = root / 'test.zip'
        metadata = dict(product='HT-76', platform='macos', architecture=arch, format='AU',
                        version='1.0.0', revision='local', configuration='Release')
        with zipfile.ZipFile(archive, 'w') as zipped:
            zipped.writestr('test/build-info.json', json.dumps(metadata))
            if extra_path:
                zipped.writestr(extra_path, 'unexpected')
        archive.with_suffix('.zip.sha256').write_text(
            f'{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}\n')
        return archive

    def test_rejects_wrong_architecture_and_changed_archive(self):
        with tempfile.TemporaryDirectory() as temporary:
            archive = self.archive(Path(temporary))
            dmg.read_archive(archive, 'AU', 'arm64', '1.0.0', 'local')
            with self.assertRaisesRegex(ValueError, 'architecture'):
                dmg.read_archive(archive, 'AU', 'x86_64', '1.0.0', 'local')
            archive.with_suffix('.zip.sha256').write_text('0' * 64 + '  test.zip\n')
            with self.assertRaisesRegex(ValueError, 'Checksum mismatch'):
                dmg.read_archive(archive, 'AU', 'arm64', '1.0.0', 'local')

    def test_universal_archive_metadata(self):
        with tempfile.TemporaryDirectory() as temporary:
            archive = self.archive(Path(temporary), arch='universal')
            dmg.read_archive(archive, 'AU', 'universal', '1.0.0', 'local')
            with self.assertRaisesRegex(ValueError, 'architecture'):
                dmg.read_archive(archive, 'AU', 'arm64', '1.0.0', 'local')

    def test_rejects_archive_path_escape(self):
        with tempfile.TemporaryDirectory() as temporary:
            archive = self.archive(Path(temporary), extra_path='../outside')
            with self.assertRaisesRegex(ValueError, 'Unsafe archive path'):
                dmg.read_archive(archive, 'AU', 'arm64', '1.0.0', 'local')

    @unittest.skipUnless(sys.platform == 'darwin', 'Uses macOS PlistBuddy')
    def test_uninstaller_identity_and_symlink_safeguards(self):
        def matches(bundle):
            return subprocess.run(['/bin/bash', '-c', 'source "$1"; bundle_matches "$2"',
                                   'test', str(ROOT / 'scripts/macos/uninstall.command'), str(bundle)],
                                  capture_output=True).returncode == 0
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            bundle = root / 'HT-76.component'
            contents = bundle / 'Contents'
            contents.mkdir(parents=True)
            info = contents / 'Info.plist'
            info.write_bytes(plistlib.dumps({'CFBundleIdentifier': dmg.BUNDLE_ID}))
            self.assertTrue(matches(bundle))
            link = root / 'redirect.component'
            link.symlink_to(bundle, target_is_directory=True)
            self.assertFalse(matches(link))
            parent = root / 'redirect'
            parent.symlink_to(root, target_is_directory=True)
            self.assertFalse(matches(parent / bundle.name))
            info.write_bytes(plistlib.dumps({'CFBundleIdentifier': 'com.someone.else'}))
            self.assertFalse(matches(bundle))
            info.write_text('malformed plist')
            self.assertFalse(matches(bundle))
            self.assertTrue(bundle.exists())


if __name__ == '__main__':
    unittest.main()
