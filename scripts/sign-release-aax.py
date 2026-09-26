#!/usr/bin/env python3
"""Locally sign a macOS Universal AAX release ZIP; upload only with --upload."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import plistlib
import re
import shutil
import stat
import subprocess
import sys
import tempfile
from urllib.parse import quote
import zipfile

DEFAULT_TOOL = "/Applications/PACEAntiPiracy/Eden/Fusion/Versions/6/bin/wraptool"
CONFIG_KEYS = ("PACE_CUSTOMER_NUMBER", "PACE_CUSTOMER_NAME", "PACE_SIGN_ID")
BEGIN = "<!-- ht76-local-aax-signing:start -->"
END = "<!-- ht76-local-aax-signing:end -->"


class Runner:
    def __init__(self, directory):
        self.directory = directory
        self.count = 0

    def run(self, label, command):
        # Never echo commands: wraptool arguments contain local account details.
        self.count += 1
        log = self.directory / f"{self.count:02d}-{label}.log"
        print(f"  {label}", flush=True)
        with log.open("wb") as stream:
            result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError(f"{label} failed; local log: {log}")
        return log.read_text(errors="replace")


def latest_release(releases):
    published = [r for r in releases if not r["draft"] and r.get("published_at")]
    if not published:
        raise ValueError("No published releases found")
    # /releases/latest omits prereleases. Select by publication date instead.
    return max(published, key=lambda r: r["published_at"])


def select_asset(release):
    assets = [a for a in release["assets"]
              if a["name"].endswith("-macos-universal-AAX.zip")]
    if len(assets) != 1 or not re.fullmatch(r"HT-76-[A-Za-z0-9._-]+", assets[0]["name"]):
        raise ValueError("Expected exactly one HT-76 macOS Universal AAX ZIP")
    return assets[0]


def digest(path):
    with path.open("rb") as stream:
        checksum = hashlib.sha256()
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(block)
    return "sha256:" + checksum.hexdigest()


def verify_download(path, asset):
    if path.stat().st_size != asset["size"] or digest(path) != asset.get("digest"):
        raise ValueError("Asset size/SHA-256 mismatch (or GitHub digest missing)")


def validate_archive(path):
    with zipfile.ZipFile(path) as archive:
        if archive.testzip():
            raise ValueError("ZIP integrity check failed")
        links = set()
        names = []
        for entry in archive.infolist():
            name = PurePosixPath(entry.filename)
            if name.is_absolute() or ".." in name.parts or "\\" in entry.filename:
                raise ValueError("Unsafe ZIP entry")
            names.append(name)
            if stat.S_ISLNK(entry.external_attr >> 16):
                target = archive.read(entry).decode("utf-8")
                # Permit relative links, but never links escaping the top-level package.
                resolved = os.path.normpath(str(name.parent / target))
                if target.startswith("/") or "\\" in target or not resolved.startswith(name.parts[0] + "/"):
                    raise ValueError("Unsafe ZIP symlink")
                links.add(name)
        if any(parent in links for name in names for parent in name.parents):
            raise ValueError("ZIP entry traverses a symlink")


def find_bundle(directory):
    bundles = [p for p in directory.rglob("*.aaxplugin") if "__MACOSX" not in p.parts]
    if len(bundles) != 1 or bundles[0].name != "HT-76.aaxplugin":
        raise ValueError("Expected one HT-76.aaxplugin bundle")
    return bundles[0]


def verify_bundle(runner, tool, bundle):
    for path in bundle.rglob("*"):
        if path.is_symlink():
            resolved = path.resolve(strict=True)
            if bundle.resolve() not in resolved.parents:
                raise ValueError("Bundle symlink escapes plugin")
    runner.run("pace-verify", [tool, "verify", "--in", str(bundle)])
    runner.run("codesign-verify", ["codesign", "--verify", "--all-architectures",
                                  "--deep", "--strict", str(bundle)])
    with (bundle / "Contents/Info.plist").open("rb") as stream:
        executable = plistlib.load(stream)["CFBundleExecutable"]
    if Path(executable).name != executable:
        raise ValueError("Invalid bundle executable")
    runner.run("universal-verify", ["lipo", str(bundle / "Contents/MacOS" / executable),
                                    "-verify_arch", "arm64", "x86_64"])


def sign_bundle(runner, tool, config, source, output):
    runner.run("pace-sign", [tool, "sign", "--customernumber", config["PACE_CUSTOMER_NUMBER"],
                            "--customername", config["PACE_CUSTOMER_NAME"],
                            "--signid", config["PACE_SIGN_ID"],
                            "--in", str(source), "--out", str(output)])
    verify_bundle(runner, tool, output)


def require_unchanged(before, after):
    for key in ("id", "tag_name", "draft", "prerelease", "immutable", "body", "assets"):
        if key == "assets":
            def identity(release):
                return sorted((a["id"], a["name"], a["size"], a.get("digest"))
                              for a in release["assets"])
            equal = identity(before) == identity(after)
        else:
            equal = before.get(key) == after.get(key)
        if not equal:
            raise ValueError(f"Remote release changed ({key}); refusing replacement")


def release_notes(body, url):
    body = body or ""
    body = re.sub(re.escape(BEGIN) + r".*?" + re.escape(END) + r"\n*", "", body, flags=re.S)
    # Correct the unsigned notices generated by this project's CI template.
    body = body.replace("- **macOS：** 外掛只有 ad-hoc 簽章，沒有 Developer ID 身分驗證；",
                        "- **macOS：** 除下述 PACE 簽署的 AAX ZIP 外，其餘外掛仍只有 ad-hoc 簽章；")
    body = re.sub(r"^- \*\*AAX／Pro Tools：\*\*.*$",
                  "- **AAX／Pro Tools：** macOS AAX ZIP 的簽署狀態見下方更新；DMG 狀態見其封裝說明。Windows AAX 不由此 macOS 簽署流程處理，是否提供下載以 Release 附件為準。",
                  body, flags=re.M)
    date = datetime.now().astimezone().date().isoformat()
    note = f"""{BEGIN}
## macOS AAX 本機簽署更新（{date}）

[macOS Universal AAX ZIP]({url}) 已使用 PACE wraptool 在本機簽署。
重新封裝、解壓後通過 PACE、macOS 簽章完整性及 Intel／Apple Silicon 架構驗證。
此流程未做 Apple 公證，也未實測 Pro Tools 載入；簽章驗證不等於正式版 Pro Tools 相容性保證。
目前設定使用本機自簽測試憑證，並非 Apple Developer ID 發行簽署。
請使用 macOS「封存工具程式」或 `ditto` 解壓，保留完整 bundle 與符號連結。

本次僅更新上述 ZIP。**DMG、Windows AAX ZIP 與 EXE 未由本次操作更新；請依各附件說明確認簽署狀態。**
簽署設定、私鑰與本機記錄不會上傳。此段為上述 macOS AAX ZIP 的最新簽署狀態。
{END}
"""
    return body.rstrip() + "\n\n" + note


def publish_release(runner, repo, release, asset, upload, notes):
    endpoint = f"repos/{repo}/releases/{release['id']}"
    tag = release["tag_name"]
    current = json.loads(runner.run("check-remote", ["gh", "api", endpoint]))
    require_unchanged(release, current)
    runner.run("upload", ["gh", "release", "upload", tag, str(upload), "--repo", repo, "--clobber"])
    runner.run("release-notes", ["gh", "release", "edit", tag, "--repo", repo, "--notes-file", str(notes)])
    after = json.loads(runner.run("verify-remote", ["gh", "api", endpoint]))
    replacement = select_asset(after)
    verify_download(upload, replacement)
    # Check that only the intended ZIP and notes changed; preserve the release status.
    expected_release = dict(release, body=notes.read_text(), assets=[
        replacement if a["id"] == asset["id"] else a for a in release["assets"]])
    require_unchanged(expected_release, after)
    (notes.parent / "release-after.json").write_text(json.dumps(after, indent=2, ensure_ascii=False))
    print(f"Uploaded and verified: {after['html_url']}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", default="latest", help="Tag, or latest published release including prereleases")
    parser.add_argument("--repo", default="Hikari-Tsai/HT-76")
    parser.add_argument("--config", type=Path, default=Path.home() / ".config/ht76/pace.json")
    parser.add_argument("--wraptool", default=os.environ.get("PACE_WRAPTOOL", DEFAULT_TOOL))
    parser.add_argument("--upload", action="store_true", help="Replace the AAX ZIP and update release notes")
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("Run on macOS; this tool does not sign Windows AAX or installers")
    if not re.fullmatch(r"[\w.-]+/[\w.-]+", args.repo):
        parser.error("Invalid owner/repository")
    for tool in ("gh", "ditto", "codesign", "lipo", "git", args.wraptool):
        if not shutil.which(tool):
            parser.error(f"Required tool missing: {tool}")
    config = {}
    if args.config.exists():
        if args.config.stat().st_mode & 0o077:
            parser.error("Local config must be private: chmod 600 " + str(args.config))
        config = json.loads(args.config.read_text())
    config = {key: os.environ.get(key, config.get(key, "")) for key in CONFIG_KEYS}
    if not all(isinstance(v, str) and v.strip() for v in config.values()):
        parser.error("Set PACE_CUSTOMER_NUMBER, PACE_CUSTOMER_NAME, PACE_SIGN_ID in local JSON or environment")
    project = Path(__file__).resolve().parents[1]
    git_dir = Path(subprocess.check_output(
        ["git", "-C", str(project), "rev-parse", "--absolute-git-dir"], text=True).strip())
    os.umask(0o077)
    local = git_dir / "local-signing"
    local.mkdir(mode=0o700, exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix="run-" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ-"), dir=local))
    print(f"Local backup / logs / output: {directory}", flush=True)
    logs = directory / "logs"
    logs.mkdir()
    runner = Runner(logs)

    def api(endpoint, label):
        return json.loads(runner.run(label, ["gh", "api", endpoint]))

    endpoint = f"repos/{args.repo}/releases"
    if args.tag == "latest":
        pages = json.loads(runner.run("list-releases", ["gh", "api", endpoint, "--paginate", "--slurp"]))
        release = latest_release([r for page in pages for r in page])
    else:
        release = api(endpoint + "/tags/" + quote(args.tag, safe=""), "get-release")
    if release["draft"] or release.get("immutable"):
        raise ValueError("Draft/immutable releases are not supported")
    asset = select_asset(release)
    tag = release["tag_name"]
    if tag.startswith("-"):
        raise ValueError("Invalid release tag")
    print(f"Selected {args.repo} {tag}: {asset['name']}", flush=True)
    (directory / "release-before.json").write_text(json.dumps(release, indent=2, ensure_ascii=False))
    original, work, signed, output, roundtrip = [directory / n for n in ("original", "work", "signed", "upload", "roundtrip")]
    for folder in (original, work, signed, output, roundtrip):
        folder.mkdir()
    runner.run("download", ["gh", "release", "download", tag, "--repo", args.repo,
                            "--pattern", asset["name"], "--dir", str(original)])
    archive = original / asset["name"]
    verify_download(archive, asset)
    validate_archive(archive)
    runner.run("extract", ["ditto", "-x", "-k", str(archive), str(work)])
    source = find_bundle(work)
    package = source.parent
    if package.parent != work:
        raise ValueError("Unexpected release ZIP layout")
    metadata_file = package / "build-info.json"
    metadata = json.loads(metadata_file.read_text())
    if any(metadata.get(k) != v for k, v in dict(product="HT-76", platform="macos", architecture="universal", format="AAX").items()):
        raise ValueError("Unexpected build metadata")
    sign_bundle(runner, args.wraptool, config, source, signed / source.name)
    shutil.rmtree(source)
    runner.run("stage-signed", ["ditto", str(signed / source.name), str(source)])
    metadata.update(pace_signed=True, pace_verified=True, macos_notarized=False,
                    pro_tools_host_tested=False, signing="PACE; local self-signed macOS test certificate",
                    signing_date=datetime.now().astimezone().date().isoformat())
    metadata_file.write_text(json.dumps(metadata, indent=2) + "\n")
    (package / "README.txt").write_text(
        "HT-76 macOS Universal AAX Native (no HDX DSP)\n"
        "PACE signed locally; PACE/codesign/Universal checks passed after ZIP extraction.\n"
        "Uses a local self-signed test certificate, not Apple Developer ID. Not notarized.\n"
        "Pro Tools loading has not been tested; host compatibility is not guaranteed.\n"
        "Extract with Archive Utility or ditto to preserve symlinks. Copy the entire bundle to:\n"
        "/Library/Application Support/Avid/Audio/Plug-Ins/\n"
        "Only this ZIP was updated; DMG and Windows ZIP/EXE AAX remain unchanged.\n"
        "Original source revision and version are recorded in build-info.json.\n")
    upload = output / asset["name"]
    runner.run("package", ["ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", str(package), str(upload)])
    validate_archive(upload)
    runner.run("roundtrip-extract", ["ditto", "-x", "-k", str(upload), str(roundtrip)])
    verify_bundle(runner, args.wraptool, find_bundle(roundtrip))
    expected = dict(size=upload.stat().st_size, digest=digest(upload))
    (directory / "upload-manifest.json").write_text(json.dumps(dict(name=asset["name"], **expected), indent=2))
    notes = directory / "release-notes.md"
    notes.write_text(release_notes(release.get("body"), asset["browser_download_url"]))
    print(f"Verified ZIP: {upload}", flush=True)
    if not args.upload:
        print("Local-only run complete. Use --upload on the next run to publish.")
        return
    publish_release(runner, args.repo, release, asset, upload, notes)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, RuntimeError, OSError, KeyError, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        print(f"Stopped: {error}\nLocal backups are retained. If upload started, inspect the release before retrying.", file=sys.stderr)
        sys.exit(1)
