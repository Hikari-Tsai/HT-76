"""Validate Windows installer payloads before running the native compiler in CI."""

import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('windows_packager', ROOT / 'scripts/package-windows.py')
windows = importlib.util.module_from_spec(spec)
spec.loader.exec_module(windows)


class WindowsInstallerTests(unittest.TestCase):
    def fixture(self, root, fmt='VST3', metadata_arch='x64', machine=0x8664, extra_path=None):
        extension = windows.FORMATS[fmt]
        archive = root / f'HT-76-1.0.0-local-windows-x64-{fmt}.zip'
        binary = bytearray(256)
        binary[:2] = b'MZ'
        struct.pack_into('<I', binary, 0x3C, 128)
        binary[128:132] = b'PE\0\0'
        struct.pack_into('<H', binary, 132, machine)
        subdir = 'x86_64-win' if fmt == 'VST3' else 'x64'
        with zipfile.ZipFile(archive, 'w') as zipped:
            prefix = archive.stem + '/'
            metadata = dict(product='HT-76', platform='windows', architecture=metadata_arch,
                            format=fmt, version='1.0.0', revision='local', configuration='Release')
            zipped.writestr(prefix + 'build-info.json', json.dumps(metadata))
            zipped.writestr(prefix + f'HT-76.{extension}/Contents/{subdir}/HT-76.{extension}', binary)
            zipped.writestr(prefix + f'HT-76.{extension}/Contents/Resources/preset.txt', 'bundle resource')
            for name in ('LICENSE', 'NOTICE', 'third_party/fetcomp-dsp/LICENSE',
                         'third_party/licenses/math.txt', 'third_party/DEPENDENCIES.md'):
                zipped.writestr(prefix + name, 'retained license notice')
            if extra_path:
                zipped.writestr(extra_path, 'unsafe')
        self.checksum(archive)
        return archive

    def checksum(self, archive):
        archive.with_suffix('.zip.sha256').write_text(
            f'{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}\n')

    def test_metadata_and_native_machine_must_both_match(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = self.fixture(root, metadata_arch='arm64')
            with self.assertRaisesRegex(ValueError, 'architecture'):
                windows.extract_archive(archive, root / 'out', 'VST3', '1.0.0', 'local')
            self.assertFalse((root / 'out').exists())
            archive = self.fixture(root, machine=0x14C)
            with self.assertRaisesRegex(ValueError, 'Windows x64'):
                windows.extract_archive(archive, root / 'out', 'VST3', '1.0.0', 'local')

    def test_checksum_and_source_revision_are_verified(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = self.fixture(root)
            with self.assertRaisesRegex(ValueError, 'revision'):
                windows.extract_archive(archive, root / 'out', 'VST3', '1.0.0', 'a' * 40)
            archive.with_suffix('.zip.sha256').write_text('0' * 64 + '  ' + archive.name)
            with self.assertRaisesRegex(ValueError, 'Checksum mismatch'):
                windows.extract_archive(archive, root / 'out', 'VST3', '1.0.0', 'local')
            self.assertFalse((root / 'out').exists())

    def test_rejects_windows_and_posix_path_escapes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for path in ('../escape', '/absolute', 'C:\\escape', 'test/../escape',
                         'HT-76-1.0.0-local-windows-x64-VST3/a:stream'):
                with self.subTest(path=path):
                    archive = self.fixture(root, extra_path=path)
                    with self.assertRaisesRegex(ValueError, 'Unsafe or ambiguous'):
                        windows.extract_archive(archive, root / 'out', 'VST3', '1.0.0', 'local')
            self.assertFalse((root / 'out').exists())

    def test_staging_preserves_bundles_notices_and_runtime_metadata(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for fmt in windows.FORMATS:
                self.fixture(root, fmt)
            project = root / 'project'
            for name in ('LICENSE.md', 'modules/juce_audio_plugin_client/AAX/SDK/LICENSE.txt',
                         'modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt'):
                license_path = project / 'third_party/JUCE' / name
                license_path.parent.mkdir(parents=True, exist_ok=True)
                license_path.write_text('SDK license')
            redist = root / 'fixture-redist.exe'
            redist.write_bytes(b'fixture only; never executed')
            stage = root / 'stage'
            stage.mkdir()
            with patch.object(windows, 'PROJECT', project):
                windows.stage_payload(root, stage, '1.0.0', 'local', redist)
            for fmt, extension in windows.FORMATS.items():
                self.assertEqual((stage / fmt / f'HT-76.{extension}' / 'Contents/Resources/preset.txt').read_text(),
                                 'bundle resource')
            self.assertEqual((stage / 'LICENSE').read_text(), 'retained license notice')
            self.assertEqual((stage / 'third_party/licenses/AAX-SDK-LICENSE.txt').read_text(), 'SDK license')
            metadata = json.loads((stage / 'build-info.json').read_text())
            self.assertEqual(metadata['formats'], ['VST3', 'AAX'])
            self.assertEqual(metadata['vc_redist_sha256'], hashlib.sha256(redist.read_bytes()).hexdigest())


if __name__ == '__main__':
    unittest.main()
