#!/usr/bin/env python3
"""Mount/inspect a DMG; optionally install/uninstall only on a fresh hosted CI runner."""

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import subprocess
import tempfile
import xml.etree.ElementTree as ET

PROJECT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('dmg_packager', PROJECT / 'scripts/package-dmg.py')
dmg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dmg)


def verify(image, install_test=False):
    image = image.resolve()
    if install_test and (os.environ.get('GITHUB_ACTIONS') != 'true'
                         or os.environ.get('RUNNER_ENVIRONMENT') != 'github-hosted'):
        raise ValueError('Install/uninstall smoke test is restricted to a fresh GitHub-hosted runner.')
    digest = hashlib.sha256(image.read_bytes()).hexdigest()
    if image.with_suffix('.dmg.sha256').read_text().split() != [digest, image.name]:
        raise ValueError('DMG checksum mismatch')
    with tempfile.TemporaryDirectory(prefix='ht76-verify-') as temporary:
        work = Path(temporary)
        mount = work / 'mount'
        mount.mkdir()
        dmg.run('/usr/bin/hdiutil', 'attach', '-readonly', '-nobrowse', '-mountpoint', mount, image)
        try:
            metadata = json.loads((mount / 'build-info.json').read_text())
            arch = metadata['architecture']
            installer = mount / 'Install HT-76.pkg'
            uninstaller = mount / 'Uninstall HT-76.command'
            if not os.access(uninstaller, os.X_OK):
                raise ValueError('Uninstaller lost its executable permission')
            if uninstaller.read_bytes() != (PROJECT / 'scripts/macos/uninstall.command').read_bytes():
                raise ValueError('Unexpected uninstaller contents')
            for name in ('LICENSE', 'NOTICE'):
                if (mount / name).read_bytes() != (PROJECT / name).read_bytes():
                    raise ValueError(f'Unexpected {name}')
            expanded = work / 'expanded'
            dmg.run('/usr/sbin/pkgutil', '--expand-full', installer, expanded)
            definition = ET.parse(expanded / 'Distribution')
            if definition.find('options').get('hostArchitectures') != dmg.host_architectures(arch):
                raise ValueError('Installer architecture mismatch')
            for fmt, (extension, destination) in dmg.FORMATS.items():
                choice = definition.find(f"choice[@id='{fmt}']")
                if choice.get('start_selected') != 'true':
                    raise ValueError(f'Unexpected default selection: {fmt}')
                component = expanded / f'{fmt}.pkg'
                info = ET.parse(component / 'PackageInfo').getroot()
                if info.get('install-location') != destination:
                    raise ValueError(f'Wrong install destination for {fmt}')
                if info.get('identifier') != f'com.hikaritsai.ht76.pkg.{fmt.lower()}':
                    raise ValueError(f'Wrong receipt identifier for {fmt}')
                bundle = component / 'Payload' / f'HT-76.{extension}'
                if plistlib.loads((bundle / 'Contents/Info.plist').read_bytes()).get('CFBundleIdentifier') != dmg.BUNDLE_ID:
                    raise ValueError(f'Wrong bundle identity for {fmt}')
                dmg.packager.validate_binary(dmg.packager.executable_path(bundle, 'macos', fmt), 'macos', arch)
                dmg.run('/usr/bin/codesign', '--verify', '--all-architectures', '--deep', '--strict', bundle)
            dmg.run('/usr/sbin/installer', '-showChoicesXML', '-pkg', installer, '-target', '/')
            if install_test:
                # Never replace an existing installation even on the disposable CI runner.
                for extension, destination in dmg.FORMATS.values():
                    for parent in (Path(destination), Path.home() / destination.lstrip('/')):
                        for name in ('HT-76', '1176 Field Effect'):
                            path = parent / f'{name}.{extension}'
                            if path.exists() or path.is_symlink():
                                raise ValueError(f'Refusing to overwrite an existing installation: {path}')
                # Exercise the actual defaults, without overriding format selections.
                dmg.run('/usr/bin/sudo', '-n', '/usr/sbin/installer', '-pkg', installer, '-target', '/')
                for fmt, (extension, destination) in dmg.FORMATS.items():
                    bundle = Path(destination) / f'HT-76.{extension}'
                    dmg.packager.validate_binary(dmg.packager.executable_path(bundle, 'macos', fmt), 'macos', arch)
                    dmg.run('/usr/bin/codesign', '--verify', '--all-architectures', '--deep', '--strict', bundle)
                    dmg.run('/usr/sbin/pkgutil', '--pkg-info', f'com.hikaritsai.ht76.pkg.{fmt.lower()}')
                subprocess.run(['/bin/bash', str(uninstaller)], input='UNINSTALL\n', text=True, check=True)
                for fmt, (extension, destination) in dmg.FORMATS.items():
                    if (Path(destination) / f'HT-76.{extension}').exists():
                        raise ValueError(f'Uninstaller left {fmt} installed')
                    result = subprocess.run(['/usr/sbin/pkgutil', '--pkg-info', f'com.hikaritsai.ht76.pkg.{fmt.lower()}'],
                                            capture_output=True)
                    if result.returncode == 0:
                        raise ValueError(f'Uninstaller left {fmt} receipt')
                # Running again must be harmless and require no confirmation.
                subprocess.run(['/bin/bash', str(uninstaller)], input='', text=True, check=True)
                print('Hosted-runner install/uninstall smoke test passed.')
        finally:
            dmg.run('/usr/bin/hdiutil', 'detach', mount)
    print(f'Verified DMG contents: {image.name}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--install-test', action='store_true')
    args = parser.parse_args()
    verify(args.image, args.install_test)
