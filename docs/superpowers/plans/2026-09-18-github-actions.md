# Cross-platform plugin builds

> Execute inline in this workspace; the user has requested implementation. This directory currently has no Git repository or remote, so hosted runs cannot be dispatched here.

**Goal:** Automatically build Windows x64 VST3/AAX and separate macOS x86_64/arm64 AU/VST3/AAX artifacts.

**Architecture:** Three native GitHub-hosted runner jobs share the existing CMake targets. A standard-library Python packager validates executable architecture and creates one ZIP per format, with version, source revision, checksums, and development-signing notes. Tests run before uploads; failure logs remain downloadable.

**Constraints:** JUCE commit `29396c22c93392d6738e021b83196283d6e4d850`; Release configuration; macOS 12 minimum; no PACE credentials. Follow-up: publish GitHub Releases automatically for pushed v-prefixed tags only after all build jobs pass. Windows uses VS 2022 x64; macOS runners are `macos-15-intel` and `macos-15`.

- [x] Create `.github/workflows/build-plugins.yml` with push, pull_request and workflow_dispatch, read-only permissions, three matrix entries, pinned checkout/setup-python/upload actions, native configuration, builds, CTest and eight format-specific artifact uploads.
- [x] Create `scripts/package-plugins.py`: CLI `--build-dir --platform --arch --formats --output-dir`; validate bundle layouts and PE/Mach-O architecture; stage complete bundles; ad-hoc sign and verify on macOS; archive with metadata and SHA-256. Add focused negative-path tests in `Tests/test_package_plugins.py`.
- [x] Verify with `python3 -m unittest discover -s Tests -p 'test_package_plugins.py'`, actionlint, actual macOS ARM packaging and archive extraction/signature validation, and `ctest --test-dir build --output-on-failure`. Document triggers, artifact names, requirements, and local versus hosted verification in README.

## Verification results

- actionlint 1.7.12: passed with no findings.
- Four packaging contract tests passed (synthetic Windows PE fixtures; not a Windows compiler run).
- Native macOS arm64 AU/VST3/AAX and test harness rebuilt successfully. CTest passed.
- All three real macOS ZIPs extracted successfully; SHA-256, executable modes, architecture and codesign verification passed after extraction.
- Windows and Intel hosted jobs have not run: this directory has no Git remote.

## Follow-up: v-prefixed tags

- Added a release job gated on successful matrix builds and pushed `refs/tags/v*`.
- Download and checksum-check all eight ZIPs and eight SHA-256 files.
- Publish new releases from a draft only after successful uploads; reuse existing releases on rerun.
- actionlint and offline mocked create/update/failure flows passed; actual GitHub publication remains untested without a remote.
