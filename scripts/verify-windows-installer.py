#!/usr/bin/env python3
"""Exercise installation, component selection and uninstall on a fresh hosted Windows runner."""

import argparse
import ctypes
import hashlib
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import time
import zipfile

PROJECT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('plugin_packager', PROJECT / 'scripts/package-plugins.py')
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


def verify(installer, log_dir):
    if (sys.platform != 'win32' or os.environ.get('GITHUB_ACTIONS') != 'true'
            or os.environ.get('RUNNER_ENVIRONMENT') != 'github-hosted'):
        raise ValueError('Install/uninstall tests require a fresh GitHub-hosted Windows runner.')
    import winreg
    if not ctypes.windll.shell32.IsUserAnAdmin():
        raise ValueError('Installer smoke test requires an elevated CI runner.')
    installer = installer.resolve()
    digest = hashlib.sha256(installer.read_bytes()).hexdigest()
    if installer.with_suffix('.exe.sha256').read_text().split() != [digest, installer.name]:
        raise ValueError('Installer checksum mismatch')
    common = Path(os.environ.get('CommonProgramW6432', os.environ['CommonProgramFiles']))
    app = Path(os.environ.get('ProgramW6432', os.environ['ProgramFiles'])) / 'HT-76'
    bundles = {'VST3': common / 'VST3/HT-76.vst3',
               'AAX': common / 'Avid/Audio/Plug-Ins/HT-76.aaxplugin'}
    key_name = r'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\com.hikaritsai.ht76.installer_is1'

    def registered():
        try:
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, key_name, 0,
                                winreg.KEY_READ | winreg.KEY_WOW64_64KEY):
                return True
        except FileNotFoundError:
            return False

    if any(path.exists() for path in [app, *bundles.values()]) or registered():
        raise ValueError('Refusing to replace an existing HT-76 installation.')
    log_dir = log_dir.resolve()
    log_dir.mkdir(parents=True, exist_ok=True)
    common_args = ['/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-']

    def run(command):
        result = subprocess.run([str(arg) for arg in command], timeout=240)
        if result.returncode not in (0, 3010):
            raise ValueError(f'Installer command failed with exit code {result.returncode}')

    def check_bundle(fmt):
        bundle = bundles[fmt]
        binary = packager.executable_path(bundle, 'windows', fmt)
        packager.validate_binary(binary, 'windows', 'x64')
        archive = installer.with_name(installer.name.removesuffix('-Setup.exe') + f'-{fmt}.zip')
        with zipfile.ZipFile(archive) as zipped:
            prefix = f'{archive.stem}/{bundle.name}/'
            for info in zipped.infolist():
                if info.filename.startswith(prefix) and not info.is_dir():
                    target = bundle / info.filename[len(prefix):]
                    if hashlib.sha256(target.read_bytes()).digest() != hashlib.sha256(zipped.read(info)).digest():
                        raise ValueError(f'Installed file differs from validated ZIP: {target}')

    def uninstall(label):
        uninstaller = app / 'unins000.exe'
        if not uninstaller.is_file() or not registered():
            raise ValueError('Missing native Windows uninstall registration/executable')
        run([uninstaller, *common_args, f'/LOG={log_dir / (label + "-uninstall.log")}'])
        deadline = time.monotonic() + 30
        while (registered() or any(packager.executable_path(bundle, 'windows', fmt).exists()
                                   for fmt, bundle in bundles.items())) and time.monotonic() < deadline:
            time.sleep(0.25)
        if registered():
            raise ValueError('Uninstaller left its registry entry')
        for fmt, bundle in bundles.items():
            if packager.executable_path(bundle, 'windows', fmt).exists():
                raise ValueError(f'Uninstaller left the {fmt} plugin executable')

    # No /TYPE or /COMPONENTS: test what users get with the default selection.
    run([installer, *common_args, f'/LOG={log_dir / "windows-default-install.log"}'])
    for fmt in bundles:
        check_bundle(fmt)
    for name in ('LICENSE', 'NOTICE', 'build-info.json'):
        if not (app / name).is_file():
            raise ValueError(f'Missing installed document: {name}')
    runtime_installed = False
    for view in (winreg.KEY_WOW64_64KEY, winreg.KEY_WOW64_32KEY):
        try:
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
                                0, winreg.KEY_READ | view) as runtime:
                runtime_installed |= winreg.QueryValueEx(runtime, 'Installed')[0] == 1
        except FileNotFoundError:
            pass
    if not runtime_installed:
        raise ValueError('Microsoft Visual C++ x64 runtime is not installed')
    # Reinstall tests an upgrade with the same product ID and both formats present.
    run([installer, *common_args, f'/LOG={log_dir / "windows-reinstall.log"}'])
    for fmt in bundles:
        check_bundle(fmt)
    sentinel = bundles['VST3'] / 'user-preset-ci.txt'
    sentinel.write_text('User data must survive uninstall.\n')
    uninstall('windows-full')
    if sentinel.read_text() != 'User data must survive uninstall.\n':
        raise ValueError('Uninstaller removed an unmanaged user file')
    sentinel.unlink()
    # A preserved user file can leave an otherwise empty bundle directory.
    bundles['VST3'].rmdir()
    run([installer, *common_args, '/TYPE=custom', '/COMPONENTS=vst3',
         f'/LOG={log_dir / "windows-vst3-only-install.log"}'])
    check_bundle('VST3')
    if bundles['AAX'].exists():
        raise ValueError('AAX was installed despite being deselected')
    uninstall('windows-vst3-only')
    print('Windows defaults, reinstall, custom selection, uninstall and user-file preservation passed.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('installer', type=Path)
    parser.add_argument('--log-dir', type=Path, default=Path('ci-logs'))
    args = parser.parse_args()
    verify(args.installer, args.log_dir)
