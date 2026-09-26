[![繁體中文](https://img.shields.io/badge/%E7%B9%81%E9%AB%94%E4%B8%AD%E6%96%87-62B6A5?style=for-the-badge)](INSTALLATION.md) &emsp;&emsp;
[![English](https://img.shields.io/badge/English-454B50?style=for-the-badge)](INSTALLATION.en.md)

# HT-76 安裝指南

[← 返回 README](../README.md)

GitHub Actions 設定以下 Release 建置組合：

| 平台 | 架構 | 格式 |
| --- | --- | --- |
| Windows | x64 | VST3、AAX Native |
| macOS Universal | Intel x86_64 ＋ Apple Silicon arm64 | AU、VST3、AAX Native |

macOS 最低版本為 12。另可從原始碼建置 Standalone，直接選擇音訊裝置測試。

### 先下載正確的檔案

1. 開啟 [HT-76 Releases](https://github.com/Hikari-Tsai/HT-76/releases)，選擇要安裝的版本，展開 **Assets**。標示 **Pre-release** 的版本屬於測試版。
2. 依下表下載安裝檔。macOS 的單一 Universal 版本同時支援 Intel 與 Apple Silicon，不需分別下載。
3. 儲存 DAW 專案並關閉 DAW，再依下方平台教學安裝。一般使用者不需要安裝 JUCE、Xcode 或 Visual Studio。

| 使用方式 | Assets 中的檔名結尾 |
| --- | --- |
| macOS 安裝程式（建議） | `-macos-universal-Installer.dmg` |
| Windows x64 手動安裝 | `-windows-x64-VST3.zip`（AAX／EXE 已下架） |
| 手動安裝單一格式 | 對應平台的 `-AU.zip`、`-VST3.zip` 或 `-AAX.zip` |

GitHub 自動提供的 **Source code (zip／tar.gz)** 是原始碼，不含可直接使用的外掛。Release 不另提供雜湊檔案；CI 內部仍驗證完整性。若改從 **Actions → Build plugins → Artifacts** 下載，先解開 GitHub 的外層 ZIP，再使用其中的安裝檔或格式 ZIP。

**格式怎麼選？** Logic Pro 使用 **AU**；支援 VST3 的 DAW 可選 **VST3**。HT-76 是音訊效果器，需在 DAW 的效果器插槽中載入，不能將 `.vst3` 或 `.component` 當成一般應用程式雙擊啟動。Release 不包含 Standalone 應用程式。

**AAX 狀態：** 目前輸出 AAX Native，未實作 HDX AAX DSP。v0.93 更新後的 macOS AAX ZIP／DMG 已包含通過驗證的 PACE 簽章，使用本機自簽測試憑證，尚未實測 Pro Tools 載入。Windows 未簽署 AAX ZIP 與包含它的 EXE 已從 v0.93 下架。macOS AU／VST3 為 ad-hoc 簽署，PKG／DMG 未做 Developer ID 簽署或公證；Windows 產物未做 Authenticode 簽署。各附件狀態見 [Release 說明](https://github.com/Hikari-Tsai/HT-76/releases/tag/v0.93)。

### macOS DMG 安裝與解除安裝

1. 下載 `HT-76-<版本>-<commit>-macos-universal-Installer.dmg`（Intel／Apple Silicon 通用），關閉 DAW 後開啟。
2. 在掛載的磁碟視窗內雙擊 `Install HT-76.pkg`，依提示繼續，於格式選擇頁選擇需要的格式。AU／VST3／AAX 預設全勾選，可自行取消不需要的格式。AAX 已有 PACE 簽章，但尚未實測 Pro Tools 載入。
3. 依 macOS Installer 提示輸入管理員密碼；若系統阻擋安裝程式，請參考下方的[安裝問題排查](#安裝問題排查)。
4. 安裝完成後退出掛載的 DMG，重新啟動 DAW，依[掃描與載入教學](#在-daw-中掃描與載入)搜尋 **HT-76**。保留下載的 DMG，方便日後解除安裝。

DMG 安裝程式使用系統共用目錄：

| 格式 | 安裝位置 |
| --- | --- |
| AU | `/Library/Audio/Plug-Ins/Components/HT-76.component` |
| VST3 | `/Library/Audio/Plug-Ins/VST3/HT-76.vst3` |
| AAX | `/Library/Application Support/Avid/Audio/Plug-Ins/HT-76.aaxplugin` |

安裝會更新所選目錄中的同名 HT-76。若曾用 ZIP 或 `install-macos.sh` 安裝到使用者目錄，或仍有舊名稱 `1176 Field Effect`，請先執行解除安裝，再安裝 DMG 版本，以免重複載入。

**解除安裝：** 關閉 DAW，開啟 DMG，雙擊 `Uninstall HT-76.command`。終端機會列出找到的外掛與安裝紀錄，輸入 `UNINSTALL` 才會繼續；移除系統檔案時需要管理員權限。請以一般使用者執行，工具會自行在必要時呼叫 `sudo`。

工具只移除標準系統／目前使用者 AU、VST3、AAX 目錄中，名稱為 `HT-76` 或 `1176 Field Effect` 且 bundle identifier 符合本專案的外掛，以及本安裝程式的 receipts。遇到符號連結或識別碼不符會跳過；預設集、DAW 專案、備份、其他使用者與其他外掛都會保留。自行放到其他位置的副本需手動處理。

請保留 DMG，或日後重新下載以取得解除安裝工具。原始碼中的工具為 [uninstall.command](../scripts/macos/uninstall.command)；可先預覽而不移除任何檔案：

```sh
bash scripts/macos/uninstall.command --dry-run
```

AU／VST3 使用 ad-hoc 簽章，AAX 使用 PACE 簽章及本機自簽測試憑證。PKG／DMG 未做 Developer ID 簽署及 Apple 公證；macOS 可能阻擋開啟。DMG 封裝會保留 AAX 的既有簽章，不會補上 Apple 發行簽署或公證。

### Windows v0.93 手動安裝

未簽署的 Windows AAX ZIP 與 EXE 已從 v0.93 下架。請解壓 `-windows-x64-VST3.zip`，將完整 `HT-76.vst3` 複製到 `C:\Program Files\Common Files\VST3`，再開啟 DAW 掃描。解除安裝時先關閉 DAW，再移除這份外掛。ZIP 不附 Visual C++ 執行環境。

### 舊版 Windows EXE 安裝與解除安裝

以下僅供先前已下載或已安裝 EXE 的使用者參考；v0.93 現已不提供此安裝包。

1. 下載 `HT-76-<版本>-<commit>-windows-x64-Setup.exe`，儲存專案並關閉 DAW。
2. 雙擊 EXE，依提示允許管理員權限。若出現 SmartScreen 提示，請參考下方的[安裝問題排查](#安裝問題排查)。
3. 選擇需要的格式。**VST3／AAX 預設全勾選**；一般使用者可保留 VST3、取消 AAX。Windows 不提供 AU。
4. 完成 Microsoft Visual C++ 執行環境及外掛安裝；若安裝程式要求重新啟動電腦，先依提示完成。
5. 開啟 DAW，啟用 VST3 掃描並搜尋 **HT-76**。請使用支援 x64 VST3 的 DAW；本專案沒有 32 位元或 VST2 版本。

| 內容 | 預設安裝位置 |
| --- | --- |
| VST3 | `C:\Program Files\Common Files\VST3\HT-76.vst3` |
| AAX | `C:\Program Files\Common Files\Avid\Audio\Plug-Ins\HT-76.aaxplugin` |
| 文件與解除安裝程式 | `C:\Program Files\HT-76` |

路徑依系統的 Program Files／Common Files 設定決定。安裝程式包含 Visual Studio 建置環境提供的 Microsoft Visual C++ x64 Redistributable，打包前會驗證其 Microsoft 數位簽章；安裝時先安裝／更新共用執行環境，再安裝外掛。

**解除安裝：** 關閉 DAW，在 Windows「設定 → 應用程式 → 已安裝的應用程式」找到 **HT-76 Plugins**，選擇解除安裝；也可執行 `C:\Program Files\HT-76\unins000.exe`。

解除安裝依安裝紀錄移除本安裝程式管理的檔案，保留使用者自行新增的檔案、預設集、DAW 專案、其他外掛及共用 Visual C++ 執行環境。其他位置或舊名稱的手動安裝副本需另行處理。重新安裝時取消某格式，不會移除已存在的該格式；若要減少格式，請先解除安裝，再重新選擇。

目前 HT-76 的 Windows EXE 與外掛未做 Authenticode 簽署，AAX 也未經 Avid／PACE 簽署，仍供 Pro Tools Developer 測試。

### ZIP 手動安裝

若已使用 DMG／EXE 安裝，不必再複製 ZIP 內的外掛。手動安裝適合需要自行管理外掛位置的使用者；Windows v0.93 現以 VST3 ZIP 手動安裝，執行環境需由使用者另外準備。

1. 關閉 DAW，解壓縮對應平台及格式的 ZIP，找到 `HT-76.component`、`HT-76.vst3` 或 `HT-76.aaxplugin`。
2. 複製**完整外掛 bundle／資料夾**到對應目錄，不要只取出其中的二進位檔案、DLL 或 `Contents`。隨附的 `README.txt`、`build-info.json` 與授權文件可另外保存。
3. macOS AU／VST3 可使用下列目前使用者目錄；Finder 按 **Shift + Command + G**，貼上路徑即可前往。若目錄不存在，先建立對應資料夾。

| 格式 | macOS 手動安裝位置 |
| --- | --- |
| AU | `~/Library/Audio/Plug-Ins/Components/` |
| VST3 | `~/Library/Audio/Plug-Ins/VST3/` |

Windows 及 macOS AAX 請使用上方安裝位置表中的系統目錄，寫入時可能需要管理員權限。`~` 代表目前使用者的家目錄，與 DMG 使用的 `/Library/…` 系統目錄不同；同一格式請只保留一份，避免 DAW 掃描到重複版本。完成後重新開啟 DAW 並掃描。

手動安裝的 Windows 副本不會登記在 EXE 的解除安裝紀錄內，移除時需關閉 DAW，再刪除自己複製的完整 HT-76 bundle。macOS 的解除安裝工具也會檢查上述標準目錄。

### 在 DAW 中掃描與載入

安裝外掛後，於 DAW 的外掛瀏覽器搜尋 **HT-76**，廠商名稱為 **Field Effect**。把它加入有音訊的軌道或匯流排之效果器插槽，播放音訊後，IN／OUT 音量表便會跟隨訊號變化。

| DAW | 格式與載入方式 |
| --- | --- |
| Logic Pro | 安裝 AU。在軌道的 **Audio FX → Audio Units → Field Effect → HT-76** 載入。若未出現，開啟 **Logic Pro → Settings（舊版為 Preferences）→ Plug-in Manager**，選取 HT-76 後執行 **Reset & Rescan Selection**；若清單中完全沒有，先確認安裝位置，再重新啟動 Mac。見 [Apple 外掛排查說明](https://support.apple.com/en-gb/122179)。 |
| Ableton Live | 在 **Settings／Preferences → Plug-Ins** 啟用 **Use VST3 Plug-In System Folders**（名稱可能依版本略異），必要時按 **Rescan**；在瀏覽器的 **Plug-Ins** 搜尋 HT-76 並拖到音軌。macOS 若使用 AU，啟用對應的 Audio Units 選項。見 [Windows 教學](https://help.ableton.com/hc/en-us/articles/209071729-Using-VST-plug-ins-on-Windows)／[macOS 教學](https://help.ableton.com/hc/en-us/articles/209068929-Using-AU-and-VST-plug-ins-on-macOS)。 |
| 其他支援 VST3 的 DAW | 在外掛管理員啟用 VST3、確認上表中的 VST3 目錄並重新掃描，再於音訊效果器清單搜尋 HT-76。選單名稱依 DAW 版本而異。 |
| Pro Tools | macOS AAX ZIP／DMG 已包含通過驗證的 PACE 簽章，但尚未實測主機載入。Windows AAX 未經 PACE 簽署，僅供 Developer 測試；重裝或重新掃描無法代替簽署。 |

上表是載入與掃描操作說明，不代表已在所有列出的 DAW 完成相容性驗證。第一次使用可先保留預設 Rev D，播放音軌後調整 Input 觀察 GR，再用 Output 配合旁通比較音量。

### 安裝問題排查

| 遇到的情況 | 處理方式 |
| --- | --- |
| macOS 顯示無法驗證開發者或未公證 | AU／VST3 為 ad-hoc 簽署，AAX 使用本機測試憑證，PKG／DMG 未簽署、未公證。確認檔案來自本專案 Release 且可信後，先嘗試開啟，再到 **系統設定 → 隱私權與安全性 → 強制打開（Open Anyway）**，依系統提示允許該檔案。此選項未必適用於所有阻擋情況；若提示檔案損毀或含惡意軟體，先停止安裝並重新確認來源。見 [Apple 官方說明](https://support.apple.com/en-gb/102445)。 |
| Windows 顯示未知發行者或「Windows 已保護您的電腦」 | 目前安裝程式未做 Authenticode 簽署。確認來源可信且系統提供選項時，可按 **其他資訊 → 仍要執行**；企業政策或 Smart App Control 可能不允許繼續。見 [Microsoft SmartScreen 說明](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation)。 |
| DAW 找不到 HT-76 | 確認下載的是安裝檔而非 Source code、已選取 DAW 支援的格式、完整 bundle 位於正確目錄；重新啟動 DAW 並掃描。Logic Pro 需 AU；只安裝 VST3 不會出現在其清單。 |
| 掃描出現重複版本或仍是舊版 | 關閉 DAW，檢查系統與使用者目錄是否同時有 HT-76 或舊名稱 `1176 Field Effect`。先依上述方式解除舊版，再重新安裝及掃描；不要刪除整個共用外掛目錄。 |
| Windows 提示缺少 VCRUNTIME／MSVCP DLL | 使用本專案 EXE 安裝程式安裝／修復所附的 Microsoft Visual C++ x64 執行環境，再重新開啟 DAW。不要從第三方網站下載個別 DLL。 |
| 外掛已開啟但音量表不動 | 在 DAW 播放有音訊的軌道，確認訊號有經過 HT-76 所在的效果器插槽，並檢查軌道靜音、路由與主機是否停用外掛。 |

安裝程式成功執行不代表 DAW 已通過外掛驗證；若仍失敗，請在 [GitHub Issues](https://github.com/Hikari-Tsai/HT-76/issues) 提供 HT-76 版本、作業系統、CPU 架構、DAW 版本、使用格式及完整錯誤訊息。
