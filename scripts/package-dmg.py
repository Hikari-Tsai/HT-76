#!/usr/bin/env python3
"""Create a selectable macOS installer and DMG from the validated format ZIPs."""

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile

PROJECT = Path(__file__).resolve().parents[1]
FORMATS = {
    'AU': ('component', '/Library/Audio/Plug-Ins/Components'),
    'VST3': ('vst3', '/Library/Audio/Plug-Ins/VST3'),
    'AAX': ('aaxplugin', '/Library/Application Support/Avid/Audio/Plug-Ins'),
}
BUNDLE_ID = 'com.hikaritsai.fieldeffect1176'
spec = importlib.util.spec_from_file_location('plugin_packager', PROJECT / 'scripts/package-plugins.py')
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


def run(*args):
    subprocess.run([str(arg) for arg in args], check=True)


def host_architectures(arch):
    if arch not in packager.MACOS_ARCHITECTURES:
        raise ValueError(f'Invalid macOS architecture: {arch}')
    return ','.join(packager.MACOS_ARCHITECTURES[arch])


def distribution(version, arch):
    root = ET.Element('installer-gui-script', minSpecVersion='2')
    ET.SubElement(root, 'title').text = f'HT-76 {version} ({arch})'
    ET.SubElement(root, 'options', customize='always', hostArchitectures=host_architectures(arch), **{'require-scripts': 'false'})
    ET.SubElement(root, 'domains', enable_anywhere='false', enable_currentUserHome='false', enable_localSystem='true')
    ET.SubElement(root, 'welcome', file='README.txt', **{'mime-type': 'text/plain'})
    ET.SubElement(ET.SubElement(root, 'volume-check'), 'allowed-os-versions')
    ET.SubElement(root.find('volume-check/allowed-os-versions'), 'os-version', min='12.0')
    outline = ET.SubElement(root, 'choices-outline')
    for fmt in FORMATS:
        ET.SubElement(outline, 'line', choice=fmt)
        title = fmt if fmt != 'AAX' else 'AAX Native — Pro Tools Developer only'
        description = f'Install HT-76 {fmt} to {FORMATS[fmt][1]}.'
        if fmt == 'AAX':
            description += ' No Avid/PACE signing; not ready for retail Pro Tools.'
        choice = ET.SubElement(root, 'choice', id=fmt, title=title, description=description,
                               start_selected='true')
        identifier = f'com.hikaritsai.ht76.pkg.{fmt.lower()}'
        ET.SubElement(choice, 'pkg-ref', id=identifier)
        ET.SubElement(root, 'pkg-ref', id=identifier, version=version, auth='Root').text = f'{fmt}.pkg'
    ET.indent(root)
    return ET.tostring(root, encoding='utf-8', xml_declaration=True)


def read_archive(archive, fmt, arch, version, revision):
    with zipfile.ZipFile(archive) as zipped:
        if zipped.testzip() is not None:
            raise ValueError(f'Corrupt ZIP: {archive}')
        for name in zipped.namelist():
            path = Path(name)
            if path.is_absolute() or '..' in path.parts:
                raise ValueError(f'Unsafe archive path: {name}')
        metadata = json.loads(zipped.read(f'{archive.stem}/build-info.json'))
    expected = dict(product='HT-76', platform='macos', architecture=arch, format=fmt,
                    version=version, revision=revision, configuration='Release')
    for key, value in expected.items():
        if metadata.get(key) != value:
            raise ValueError(f'{archive.name}: expected {key}={value!r}, got {metadata.get(key)!r}')
    recorded = archive.with_suffix('.zip.sha256').read_text().split()
    if recorded != [hashlib.sha256(archive.read_bytes()).hexdigest(), archive.name]:
        raise ValueError(f'Checksum mismatch: {archive}')


def package(archive_dir, output_dir, arch, revision):
    if sys.platform != 'darwin':
        raise ValueError('DMG packaging requires macOS.')
    if arch not in packager.MACOS_ARCHITECTURES or not re.fullmatch(r'[a-fA-F0-9]{7,40}|local', revision):
        raise ValueError('Invalid architecture or revision.')
    version = re.search(r'project\(HT-76\s+VERSION\s+(\d+\.\d+\.\d+)', (PROJECT / 'CMakeLists.txt').read_text()).group(1)
    label = f'HT-76-{version}-{revision[:12]}-macos-{arch}'
    archives = {fmt: archive_dir / f'{label}-{fmt}.zip' for fmt in FORMATS}
    for fmt, archive in archives.items():
        read_archive(archive, fmt, arch, version, revision)
    output_dir.mkdir(parents=True, exist_ok=True)
    image = output_dir.resolve() / f'{label}-Installer.dmg'
    with tempfile.TemporaryDirectory(prefix='ht76-dmg-') as temporary:
        work = Path(temporary)
        disk = work / 'disk'
        packages = work / 'packages'
        disk.mkdir()
        packages.mkdir()
        for fmt, archive in archives.items():
            extracted = work / f'extracted-{fmt}'
            run('/usr/bin/ditto', '-x', '-k', archive.resolve(), extracted)
            source = extracted / archive.stem
            extension, destination = FORMATS[fmt]
            bundle = source / f'HT-76.{extension}'
            packager.validate_binary(packager.executable_path(bundle, 'macos', fmt), 'macos', arch)
            info = plistlib.loads((bundle / 'Contents/Info.plist').read_bytes())
            if info.get('CFBundleIdentifier') != BUNDLE_ID or info.get('CFBundleShortVersionString') != version:
                raise ValueError(f'Unexpected bundle identity/version: {bundle}')
            run('/usr/bin/codesign', '--verify', '--all-architectures', '--deep', '--strict', bundle)
            payload = work / f'payload-{fmt}'
            payload.mkdir()
            run('/usr/bin/ditto', bundle, payload / bundle.name)
            components = work / f'{fmt}-components.plist'
            components.write_bytes(plistlib.dumps([{
                'RootRelativeBundlePath': bundle.name,
                'BundleIsRelocatable': False,
                'BundleIsVersionChecked': True,
                'BundleHasStrictIdentifier': True,
                'BundleOverwriteAction': 'upgrade',
            }]))
            run('/usr/bin/pkgbuild', '--root', payload, '--component-plist', components,
                '--identifier', f'com.hikaritsai.ht76.pkg.{fmt.lower()}', '--version', version,
                '--install-location', destination, '--ownership', 'recommended', packages / f'{fmt}.pkg')
            if fmt == 'AU':
                for name in ('LICENSE', 'NOTICE'):
                    shutil.copy2(source / name, disk / name)
                shutil.copytree(source / 'third_party', disk / 'third_party')
        # Include the framework/SDK license texts with this combined distribution.
        juce = PROJECT / 'third_party/JUCE'
        for source, name in [
            (juce / 'LICENSE.md', 'JUCE-LICENSE.md'),
            (juce / 'modules/juce_audio_plugin_client/AAX/SDK/LICENSE.txt', 'AAX-SDK-LICENSE.txt'),
            (juce / 'modules/juce_audio_plugin_client/AU/AudioUnitSDK/LICENSE.txt', 'AudioUnitSDK-LICENSE.txt'),
            (juce / 'modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt', 'VST3-SDK-LICENSE.txt'),
        ]:
            shutil.copy2(source, disk / 'third_party/licenses' / name)
        uninstaller = disk / 'Uninstall HT-76.command'
        shutil.copy2(PROJECT / 'scripts/macos/uninstall.command', uninstaller)
        uninstaller.chmod(0o755)
        note = f'''HT-76 {version} — macOS {arch} (macOS 12 or later)

INSTALL
Close your DAW and open Install HT-76.pkg. Administrator access is required.
AU, VST3 and AAX are selected by default. Deselect any formats you do not need.
AAX has no Avid/PACE signature and requires Pro Tools Developer for testing.
This package replaces HT-76 at the selected system-wide plugin locations:
''' + '\n'.join(f'{fmt}: {folder}/HT-76.{ext}' for fmt, (ext, folder) in FORMATS.items()) + '''

If you previously installed HT-76 or 1176 Field Effect in your user Library,
run the uninstaller first to avoid duplicate copies, then install this version.
Reopen your DAW and rescan plugins after installation.

UNINSTALL
Close your DAW and double-click Uninstall HT-76.command.
Review the exact paths, then type UNINSTALL to confirm.
Administrator access is requested for system files and installer receipts.
The tool checks the bundle identity before removing current/legacy HT-76
bundles in standard system and current-user AU/VST3/AAX locations.
Presets, sessions, backups, other users' files and other plugins are preserved.
Keep this DMG or download it again when you need to uninstall.

BUILD STATUS
Plugin bundles are ad-hoc signed. The installer and DMG are unsigned and
not notarized; macOS may block opening this development distribution.
No Apple Developer ID or Avid/PACE signing is included.

LICENSE
HT-76 original work: Apache-2.0. See LICENSE and NOTICE.
Third-party components retain their own terms; see third_party/.
'''
        (disk / 'README.txt').write_text(note, encoding='utf-8')
        (disk / 'build-info.json').write_text(json.dumps(dict(
            product='HT-76', version=version, revision=revision, platform='macos',
            architecture=arch, formats=list(FORMATS), installer_signed=False,
            macos_notarized=False, pace_signed=False), indent=2) + '\n')
        resources = work / 'resources'
        resources.mkdir()
        shutil.copy2(disk / 'README.txt', resources / 'README.txt')
        definition = work / 'Distribution.xml'
        definition.write_bytes(distribution(version, arch))
        run('/usr/bin/productbuild', '--distribution', definition, '--package-path', packages,
            '--resources', resources, disk / 'Install HT-76.pkg')
        # Parse with Installer without installing anything on the build machine.
        run('/usr/sbin/installer', '-showChoicesXML', '-pkg', disk / 'Install HT-76.pkg', '-target', '/')
        run('/usr/bin/hdiutil', 'create', '-volname', f'HT-76 {version} {arch}', '-srcfolder', disk,
            '-format', 'UDZO', '-fs', 'HFS+', '-ov', image)
        run('/usr/bin/hdiutil', 'verify', image)
    digest = hashlib.sha256(image.read_bytes()).hexdigest()
    image.with_suffix('.dmg.sha256').write_text(f'{digest}  {image.name}\n')
    print(f'Packaged {image.name} ({image.stat().st_size} bytes)', flush=True)
    return image


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive-dir', type=Path, default=Path('dist'))
    parser.add_argument('--output-dir', type=Path, default=Path('dist'))
    parser.add_argument('--arch', choices=['arm64', 'x86_64', 'universal'], required=True)
    args = parser.parse_args()
    package(args.archive_dir, args.output_dir, args.arch, os.environ.get('GITHUB_SHA', 'local'))
