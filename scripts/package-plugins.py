#!/usr/bin/env python3
"""Validate and archive JUCE Release bundles, one artifact per format/architecture."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import zipfile

PROJECT = Path(__file__).resolve().parents[1]
PRODUCT = "HT-76"
EXTENSIONS = {"AU": "component", "VST3": "vst3", "AAX": "aaxplugin"}


def executable_path(bundle, platform, format_name):
    if platform == "macos":
        return bundle / "Contents" / "MacOS" / PRODUCT
    folder = "x64" if format_name == "AAX" else "x86_64-win"
    return bundle / "Contents" / folder / f"{PRODUCT}.{EXTENSIONS[format_name]}"


def validate_binary(binary, platform, arch):
    if not binary.is_file():
        raise ValueError(f"Missing plugin executable: {binary}")
    if platform == "macos":
        actual = subprocess.check_output(["lipo", "-archs", str(binary)], text=True).split()
        if actual != [arch]:
            raise ValueError(f"Expected {arch}, found {actual}: {binary}")
    else:
        with binary.open("rb") as stream:
            if stream.read(2) != b"MZ":
                raise ValueError(f"Not a Windows PE binary: {binary}")
            stream.seek(0x3C)
            offset = struct.unpack("<I", stream.read(4))[0]
            stream.seek(offset)
            header = stream.read(6)
        if header[:4] != b"PE\0\0" or header[4:] != b"\x64\x86":
            raise ValueError(f"Expected Windows x64 PE binary: {binary}")


def package(build_dir, output_dir, platform, arch, formats, revision):
    if (platform, arch) not in {("macos", "arm64"), ("macos", "x86_64"), ("windows", "x64")}:
        raise ValueError(f"Unsupported platform/architecture: {platform}/{arch}")
    if not formats or any(f not in EXTENSIONS or (platform == "windows" and f == "AU") for f in formats):
        raise ValueError(f"Unsupported formats for {platform}: {formats}")
    if not re.fullmatch(r"[a-fA-F0-9]{7,40}|local", revision):
        raise ValueError("Revision must be a Git SHA or 'local'")
    match = re.search(r"project\(HT-76\s+VERSION\s+(\d+\.\d+\.\d+)", (PROJECT / "CMakeLists.txt").read_text())
    if not match:
        raise ValueError("Cannot read project version from CMakeLists.txt")
    version = match.group(1)
    artifacts = build_dir / "FieldEffect_artefacts" / "Release"
    # Check every requested format before producing any deliverables.
    for format_name in formats:
        bundle = artifacts / format_name / f"{PRODUCT}.{EXTENSIONS[format_name]}"
        validate_binary(executable_path(bundle, platform, format_name), platform, arch)
    output_dir.mkdir(parents=True, exist_ok=True)
    archives = []
    for format_name in formats:
        bundle = artifacts / format_name / f"{PRODUCT}.{EXTENSIONS[format_name]}"
        label = f"HT-76-{version}-{revision[:12]}-{platform}-{arch}-{format_name}"
        archive = output_dir / f"{label}.zip"
        with tempfile.TemporaryDirectory() as temporary:
            stage = Path(temporary) / label
            stage.mkdir()
            copied = stage / bundle.name
            shutil.copytree(bundle, copied, symlinks=True)
            for name in ("LICENSE", "NOTICE"):
                shutil.copy2(PROJECT / name, stage / name)
            notices = stage / "third_party"
            (notices / "fetcomp-dsp").mkdir(parents=True)
            for name in ("LICENSE", "LOCAL_CHANGES.md"):
                shutil.copy2(PROJECT / "third_party" / "fetcomp-dsp" / name,
                             notices / "fetcomp-dsp" / name)
            shutil.copy2(PROJECT / "third_party" / "DEPENDENCIES.md", notices)
            shutil.copytree(PROJECT / "third_party" / "licenses", notices / "licenses")
            if platform == "macos":
                subprocess.run(["codesign", "--force", "--sign", "-", str(copied)], check=True)
                subprocess.run(["codesign", "--verify", "--deep", "--strict", str(copied)], check=True)
            metadata = {"product": PRODUCT, "version": version, "revision": revision,
                        "platform": platform, "architecture": arch, "format": format_name,
                        "configuration": "Release", "pace_signed": False,
                        "macos_notarized": False,
                        "signing": "ad-hoc" if platform == "macos" else "unsigned"}
            (stage / "build-info.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
            note = (f"{PRODUCT} {version} / {platform} {arch} / {format_name}\n"
                    f"Source revision: {revision}\n\n"
                    "Copy the complete plugin bundle to the appropriate host plugin folder.\n"
                    "This is a development build. macOS builds are ad-hoc signed, not notarized;\n"
                    "Windows builds are unsigned. Host compatibility is not certified by CI.\n")
            if format_name == "AAX":
                note += ("AAX Native only (no HDX DSP). No Avid/PACE signature is included.\n"
                         "Use Pro Tools Developer for testing; retail Pro Tools needs AAX signing.\n")
            (stage / "README.txt").write_text(note, encoding="utf-8")
            if platform == "macos":
                subprocess.run(["ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", str(stage), str(archive.resolve())], check=True)
            else:
                with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as zipped:
                    for path in sorted(stage.rglob("*")):
                        zipped.write(path, path.relative_to(stage.parent))
        with zipfile.ZipFile(archive) as zipped:
            if zipped.testzip() is not None:
                raise ValueError(f"Corrupt archive: {archive}")
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        archive.with_suffix(".zip.sha256").write_text(f"{digest}  {archive.name}\n", encoding="utf-8")
        archives.append(archive)
        print(f"Packaged {archive.name} ({archive.stat().st_size} bytes)", flush=True)
    return archives


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--output-dir", type=Path, default=Path("dist"))
    parser.add_argument("--platform", choices=["macos", "windows"], required=True)
    parser.add_argument("--arch", choices=["arm64", "x86_64", "x64"], required=True)
    parser.add_argument("--formats", nargs="+", choices=list(EXTENSIONS), required=True)
    args = parser.parse_args()
    package(args.build_dir, args.output_dir, args.platform, args.arch, args.formats,
            os.environ.get("GITHUB_SHA", "local"))
