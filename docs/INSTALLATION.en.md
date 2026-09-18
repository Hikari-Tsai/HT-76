[![繁體中文](https://img.shields.io/badge/%E7%B9%81%E9%AB%94%E4%B8%AD%E6%96%87-454B50?style=for-the-badge)](INSTALLATION.md) &emsp;&emsp;
[![English](https://img.shields.io/badge/English-62B6A5?style=for-the-badge)](INSTALLATION.en.md)

# HT-76 Installation Guide

[← Back to README](../README.en.md)

GitHub Actions builds the following release packages:

| Platform | Architecture | Formats |
| --- | --- | --- |
| Windows | x64 | VST3, AAX Native |
| macOS Universal | Intel x86_64 + Apple Silicon arm64 | AU, VST3, AAX Native |

macOS 12 or later is required. A standalone application can also be built from source for testing with your chosen audio device.

### Download the right file

1. Open [HT-76 Releases](https://github.com/Hikari-Tsai/HT-76/releases), choose a version, and expand **Assets**. Releases marked **Pre-release** are test versions.
2. Download the installer listed below for your platform. The single Universal macOS package supports both Intel and Apple Silicon.
3. Save your DAW project and close the DAW before installing. You do not need JUCE, Xcode, or Visual Studio to install the release packages.

| Installation method | Asset filename ending |
| --- | --- |
| macOS installer (recommended) | `-macos-universal-Installer.dmg` |
| Windows x64 installer (recommended) | `-windows-x64-Setup.exe` |
| Manual installation of one format | `-AU.zip`, `-VST3.zip`, or `-AAX.zip` for your platform |

GitHub's **Source code (zip / tar.gz)** downloads contain source files, not ready-to-use plug-ins. Checksums are verified internally in CI, without separate checksum downloads on the Release page. If downloading from **Actions → Build plugins → Artifacts**, extract GitHub's outer ZIP first, then use the installer or format ZIP inside it.

**Choosing a format:** Use **AU** in Logic Pro, or **VST3** in a DAW that supports it. HT-76 is an audio effect loaded in your DAW's effects slot; `.vst3` and `.component` files cannot be launched by double-clicking like regular applications. Releases do not include the standalone application.

**AAX status:** Builds contain AAX Native, with no HDX AAX DSP implementation or Avid/PACE signature. They are intended for Pro Tools Developer testing; retail Pro Tools requires valid AAX signing. macOS plug-ins are ad-hoc signed and not notarized; Windows builds are not Authenticode signed. See [Avid's AAX developer information](https://developer.avid.com/aax/).

### macOS DMG installation and uninstallation

1. Download `HT-76-<version>-<commit>-macos-universal-Installer.dmg` for Intel or Apple Silicon. Close your DAW, then open the DMG.
2. Double-click `Install HT-76.pkg` in the mounted disk window. Follow the prompts and choose the formats you need. AU/VST3/AAX are all selected by default. Most users can deselect AAX, which is currently only for Pro Tools Developer testing.
3. Enter your administrator password when prompted. If macOS blocks the installer, see [troubleshooting](#troubleshooting).
4. Eject the DMG after installation, reopen your DAW, and follow the [scanning and loading instructions](#scanning-and-loading-in-your-daw) to find **HT-76**. Keep the downloaded DMG for uninstallation.

The DMG installer uses shared system directories:

| Format | Installation location |
| --- | --- |
| AU | `/Library/Audio/Plug-Ins/Components/HT-76.component` |
| VST3 | `/Library/Audio/Plug-Ins/VST3/HT-76.vst3` |
| AAX | `/Library/Application Support/Avid/Audio/Plug-Ins/HT-76.aaxplugin` |

Installation updates existing HT-76 bundles in the selected locations. If you previously installed ZIPs or used `install-macos.sh` in your user directory, or still have a copy named `1176 Field Effect`, uninstall those copies first to avoid duplicate plug-ins.

**Uninstall:** Close your DAW, open the DMG, and double-click `Uninstall HT-76.command`. Terminal lists the detected plug-ins and installation receipts. Enter `UNINSTALL` to proceed. Removing system files requires administrator privileges. Run the tool as your normal user; it invokes `sudo` only when necessary.

The tool removes only bundles named `HT-76` or `1176 Field Effect` with matching project bundle identifiers in the standard system/current-user AU, VST3, and AAX directories, along with this installer's receipts. It skips symbolic links and mismatched identifiers. Presets, DAW projects, backups, other users' files, and other plug-ins are preserved. Copies in custom locations must be handled manually.

Keep the DMG or download it again to access the uninstaller. The source tool is [uninstall.command](../scripts/macos/uninstall.command). From the repository root, preview its actions without removing files:

```sh
bash scripts/macos/uninstall.command --dry-run
```

The plug-ins currently use ad-hoc signatures. The PKG/DMG lacks Developer ID signing and Apple notarization, so macOS may block it. DMG packaging does not add Apple or Avid/PACE signing to these development builds.

### Windows installation and uninstallation

1. Download `HT-76-<version>-<commit>-windows-x64-Setup.exe`, save your project, and close your DAW.
2. Double-click the EXE and allow administrator access when prompted. For SmartScreen messages, see [troubleshooting](#troubleshooting).
3. Choose the formats you need. **VST3/AAX are both selected by default.** Most users can keep VST3 and deselect AAX. AU is not available on Windows.
4. Complete the Microsoft Visual C++ runtime and plug-in installation. Restart the computer if the installer requests it.
5. Open your DAW, enable VST3 scanning, and search for **HT-76**. Use a DAW supporting x64 VST3; this project does not provide 32-bit or VST2 builds.

| Content | Default installation location |
| --- | --- |
| VST3 | `C:\Program Files\Common Files\VST3\HT-76.vst3` |
| AAX | `C:\Program Files\Common Files\Avid\Audio\Plug-Ins\HT-76.aaxplugin` |
| Documentation and uninstaller | `C:\Program Files\HT-76` |

Paths follow your system's Program Files / Common Files settings. The installer includes the Microsoft Visual C++ x64 Redistributable from the Visual Studio build environment. Its Microsoft signature is checked before packaging. Setup installs or updates the shared runtime before installing the plug-ins.

**Uninstall:** Close your DAW, find **HT-76 Plugins** under Windows Settings → Apps → Installed apps, and choose Uninstall. You can also run `C:\Program Files\HT-76\unins000.exe`.

Uninstallation removes files tracked by this installer while preserving user-added files, presets, DAW projects, other plug-ins, and the shared Visual C++ runtime. Manually installed copies in other locations or under old names require separate removal. Deselecting a format during reinstallation does not remove an existing copy; to reduce the installed formats, uninstall first, then install your desired selection.

The HT-76 Windows EXE and plug-ins are not Authenticode signed. AAX is also not Avid/PACE signed and remains intended for Pro Tools Developer testing.

### Manual ZIP installation

If you have used the DMG/EXE installer, you do not need to copy the ZIP contents as well. Manual installation is for users who prefer to manage plug-in locations themselves. On Windows, the EXE is recommended because it also handles the Visual C++ runtime.

1. Close your DAW and extract the ZIP for your platform and format. Locate `HT-76.component`, `HT-76.vst3`, or `HT-76.aaxplugin`.
2. Copy the **entire plug-in bundle/folder** to the appropriate directory. Do not copy only its binary, DLL, or `Contents` folder. Keep the supplied `README.txt`, `build-info.json`, and license files separately if desired.
3. For macOS AU/VST3, use the current-user folders below. In Finder, press **Shift + Command + G** and paste the directory path. Create the directory if it does not exist.

| Format | macOS manual installation directory |
| --- | --- |
| AU | `~/Library/Audio/Plug-Ins/Components/` |
| VST3 | `~/Library/Audio/Plug-Ins/VST3/` |

For Windows or macOS AAX, use the system locations listed above; administrator access may be required. `~` means your current user's home directory, distinct from the DMG installer's `/Library/…` system locations. Keep only one copy of each format to avoid duplicate versions. Reopen your DAW and rescan afterward.

Manually installed Windows copies are not registered in the EXE installer's uninstall log. Close your DAW and remove the complete bundle you copied to uninstall them. The macOS uninstaller also checks the standard user folders above.

### Scanning and loading in your DAW

Search your DAW's plug-in browser for **HT-76**, under manufacturer **Field Effect**. Add it to an effects slot on an audio track or bus. When audio passes through it, the IN/OUT meters follow the signal.

| DAW | Format and loading instructions |
| --- | --- |
| Logic Pro | Install AU, then choose **Audio FX → Audio Units → Field Effect → HT-76** on a track. If missing, open **Logic Pro → Settings (Preferences in older versions) → Plug-in Manager**, select HT-76, and click **Reset & Rescan Selection**. If it is completely absent from that list, check its location and restart your Mac. See [Apple's troubleshooting guide](https://support.apple.com/en-gb/122179). |
| Ableton Live | Under **Settings / Preferences → Plug-Ins**, enable **Use VST3 Plug-In System Folders** (wording varies by version) and click **Rescan** if needed. Search for HT-76 under **Plug-Ins** and drag it onto an audio track. For AU on macOS, enable the corresponding Audio Units option. See the [Windows guide](https://help.ableton.com/hc/en-us/articles/209071729-Using-VST-plug-ins-on-Windows) or [macOS guide](https://help.ableton.com/hc/en-us/articles/209068929-Using-AU-and-VST-plug-ins-on-macOS). |
| Other DAWs with VST3 support | Enable VST3 in the plug-in manager, check the VST3 locations above, rescan, and search for HT-76 in the audio effects list. Menu names depend on your DAW version. |
| Pro Tools | Uses AAX, but **retail Pro Tools cannot load these builds without Avid/PACE signing**. Current artifacts are only for Pro Tools Developer testing. Reinstalling or rescanning does not remove that restriction. |

These are scanning/loading instructions, not a claim of verified compatibility with every listed DAW. For a first listen, leave the default Rev D selected, play your track, adjust Input while watching GR, then use Output to match the level when comparing with bypass.

### Troubleshooting

| Symptom | What to check |
| --- | --- |
| macOS reports an unidentified developer or missing notarization | Plug-ins are ad-hoc signed; PKG/DMG files are unsigned and not notarized. After confirming that the file comes from this project's Release and is trustworthy, try opening it, then go to **System Settings → Privacy & Security → Open Anyway** and follow the prompts for that file. This option is not available for every type of block. If macOS reports damage or malware, stop and verify the source. See [Apple's guidance](https://support.apple.com/en-gb/102445). |
| Windows reports an unknown publisher or “Windows protected your PC” | The installer is not Authenticode signed. If you trust the source and your system offers the option, choose **More info → Run anyway**. Organization policies or Smart App Control may prevent proceeding. See [Microsoft's SmartScreen documentation](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation). |
| Your DAW cannot find HT-76 | Confirm you downloaded an installer rather than Source code, selected a supported format, and placed the complete bundle in the correct directory. Restart and rescan. Logic Pro requires AU; installing only VST3 will not make it appear there. |
| Duplicate entries or an older version appear | Close your DAW and check both system and user directories for HT-76 or the old `1176 Field Effect` name. Uninstall old copies as described above, then reinstall and rescan. Do not remove the entire shared plug-in directory. |
| Windows reports missing VCRUNTIME / MSVCP DLLs | Use this project's EXE installer to install or repair the bundled Microsoft Visual C++ x64 runtime, then reopen the DAW. Do not download individual DLLs from third-party sites. |
| The interface opens but its meters do not move | Play a track with audio and confirm that its signal passes through the effects slot containing HT-76. Check track mute, routing, and whether the host has deactivated the plug-in. |

A successful installation does not mean the DAW has validated the plug-in. If the problem persists, open a [GitHub Issue](https://github.com/Hikari-Tsai/HT-76/issues) with your HT-76 version, operating system, CPU architecture, DAW version, plug-in format, and complete error message.
