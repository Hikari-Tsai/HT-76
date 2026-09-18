#!/usr/bin/env python3
"""Build a Windows x64 installer with Inno Setup 6 and its native uninstaller."""

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import zipfile

PROJECT = Path(__file__).resolve().parents[1]
FORMATS = {'VST3': 'vst3', 'AAX': 'aaxplugin'}
spec = importlib.util.spec_from_file_location('plugin_packager', PROJECT / 'scripts/package-plugins.py')
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


def extract_archive(archive, destination, fmt, version, revision):
    expected = dict(product='HT-76', platform='windows', architecture='x64', format=fmt,
                    version=version, revision=revision, configuration='Release')
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if archive.with_suffix('.zip.sha256').read_text().split() != [digest, archive.name]:
        raise ValueError(f'Checksum mismatch: {archive}')
    with zipfile.ZipFile(archive) as zipped:
        if zipped.testzip() is not None:
            raise ValueError(f'Corrupt ZIP: {archive}')
        seen = set()
        for member in zipped.infolist():
            name = member.filename
            parts = PurePosixPath(name).parts
            normalized = name.rstrip('/').casefold()
            if (not parts or parts[0] != archive.stem or '..' in parts
                    or '\\' in name or ':' in name or normalized in seen
                    or stat.S_ISLNK(member.external_attr >> 16)
                    or any(part.endswith((' ', '.')) for part in parts)):
                raise ValueError(f'Unsafe or ambiguous archive path: {name}')
            seen.add(normalized)
        metadata = json.loads(zipped.read(f'{archive.stem}/build-info.json'))
        for key, value in expected.items():
            if metadata.get(key) != value:
                raise ValueError(f'{archive.name}: expected {key}={value!r}, got {metadata.get(key)!r}')
        zipped.extractall(destination)
    source = destination / archive.stem
    bundle = source / f'HT-76.{FORMATS[fmt]}'
    packager.validate_binary(packager.executable_path(bundle, 'windows', fmt), 'windows', 'x64')
    return source


def stage_payload(archive_dir, stage, version, revision, redist):
    label = f'HT-76-{version}-{revision[:12]}-windows-x64'
    with tempfile.TemporaryDirectory(prefix='ht76-win-extract-') as temporary:
        for fmt, extension in FORMATS.items():
            archive = archive_dir / f'{label}-{fmt}.zip'
            source = extract_archive(archive, Path(temporary) / fmt, fmt, version, revision)
            shutil.copytree(source / f'HT-76.{extension}', stage / fmt / f'HT-76.{extension}')
            if fmt == 'VST3':
                for name in ('LICENSE', 'NOTICE'):
                    shutil.copy2(source / name, stage / name)
                shutil.copytree(source / 'third_party', stage / 'third_party')
    juce = PROJECT / 'third_party/JUCE'
    for source, name in [
        (juce / 'LICENSE.md', 'JUCE-LICENSE.md'),
        (juce / 'modules/juce_audio_plugin_client/AAX/SDK/LICENSE.txt', 'AAX-SDK-LICENSE.txt'),
        (juce / 'modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt', 'VST3-SDK-LICENSE.txt'),
    ]:
        shutil.copy2(source, stage / 'third_party/licenses' / name)
    shutil.copy2(redist, stage / 'vc_redist.x64.exe')
    (stage / 'build-info.json').write_text(json.dumps(dict(
        product='HT-76', version=version, revision=revision, platform='windows',
        architecture='x64', formats=list(FORMATS), installer_signed=False, pace_signed=False,
        vc_redist_sha256=hashlib.sha256(redist.read_bytes()).hexdigest()), indent=2) + '\n')
    (stage / 'README.txt').write_text(f'''HT-76 {version} — Windows x64

Close your DAW before installing. VST3 and AAX are both selected by default.
You can deselect either format in the installer. Administrator access is required.

VST3: Common Files\\VST3\\HT-76.vst3
AAX:  Common Files\\Avid\\Audio\\Plug-Ins\\HT-76.aaxplugin
Documentation and uninstaller: Program Files\\HT-76

Uninstall using Windows Settings > Apps > Installed apps > HT-76 Plugins,
or run unins000.exe in Program Files\\HT-76. Close your DAW first.
Only installer-managed files are removed. Presets, DAW sessions, other plugins
and the shared Microsoft Visual C++ runtime are preserved. Manually copied
plugins at other locations and legacy names are not managed by this installer.
Deselecting a format during an upgrade does not uninstall an existing copy;
use Uninstall and reinstall your desired selection to remove a format.

This installer includes Microsoft's Visual C++ x64 Redistributable, obtained
from the Visual Studio build environment and validated for Microsoft signing.
The shared runtime is installed or updated before the plugin files.

Development build: HT-76 plugins and this installer are not Authenticode signed.
AAX has no Avid/PACE signature and requires Pro Tools Developer for testing.

HT-76 original work: Apache-2.0. See LICENSE, NOTICE and third_party/.
Microsoft Visual C++ Redistributable retains Microsoft's own license terms.
''', encoding='utf-8-sig')
    return label


def find_tool(override, default, executable):
    candidate = override or shutil.which(executable) or default
    candidate = Path(candidate)
    if not candidate.is_file():
        raise ValueError(f'Missing {executable}: {candidate}')
    return candidate.resolve()


def package(archive_dir, output_dir, revision, compiler=None, redist=None):
    if sys.platform != 'win32':
        raise ValueError('Windows installer compilation requires Windows and Inno Setup 6.')
    if not re.fullmatch(r'[a-fA-F0-9]{7,40}|local', revision):
        raise ValueError('Invalid source revision')
    version = re.search(r'project\(HT-76\s+VERSION\s+(\d+\.\d+\.\d+)', (PROJECT / 'CMakeLists.txt').read_text()).group(1)
    compiler = find_tool(compiler, Path(os.environ['ProgramFiles(x86)']) / 'Inno Setup 6/ISCC.exe', 'ISCC.exe')
    if redist is None:
        vswhere = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        vs_path = subprocess.check_output([str(vswhere), '-latest', '-products', '*',
                                          '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
                                          '-property', 'installationPath'], text=True).strip()
        if not vs_path:
            raise ValueError('Visual Studio C++ toolchain not found')
        redist = Path(vs_path) / 'VC/Redist/MSVC/v143/vc_redist.x64.exe'
    redist = Path(redist).resolve()
    if not redist.is_file():
        raise ValueError(f'Missing Microsoft Visual C++ x64 Redistributable: {redist}')
    # Check the redistributable's trust chain and publisher before embedding it.
    environment = dict(os.environ, HT76_VC_REDIST=str(redist))
    subprocess.run(['powershell.exe', '-NoProfile', '-NonInteractive', '-Command',
                    "$s = Get-AuthenticodeSignature -LiteralPath $env:HT76_VC_REDIST; "
                    "if ($s.Status -ne 'Valid' -or $s.SignerCertificate.Subject -notmatch '(^|, )CN=Microsoft Corporation(,|$)') "
                    "{ throw 'Redistributable is not validly signed by Microsoft Corporation' }"],
                   env=environment, check=True)
    output_dir = output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='ht76-inno-') as temporary:
        stage = Path(temporary)
        label = stage_payload(archive_dir, stage, version, revision, redist)
        name = f'{label}-Setup'
        subprocess.run([str(compiler), f'/DProductVersion={version}', f'/DStageDirectory={stage}',
                        f'/DOutputDirectory={output_dir}', f'/DOutputName={name}',
                        str(PROJECT / 'scripts/windows/installer.iss')], check=True)
    installer = output_dir / f'{name}.exe'
    if not installer.is_file():
        raise ValueError('Inno Setup did not produce the expected installer')
    installer.with_suffix('.exe.sha256').write_text(
        f'{hashlib.sha256(installer.read_bytes()).hexdigest()}  {installer.name}\n')
    print(f'Packaged {installer.name} ({installer.stat().st_size} bytes)', flush=True)
    return installer


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive-dir', type=Path, default=Path('dist'))
    parser.add_argument('--output-dir', type=Path, default=Path('dist'))
    parser.add_argument('--iscc', type=Path)
    parser.add_argument('--vc-redist', type=Path)
    args = parser.parse_args()
    package(args.archive_dir, args.output_dir, os.environ.get('GITHUB_SHA', 'local'), args.iscc, args.vc_redist)
