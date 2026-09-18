# HT-76

![Rev D 黑色 Rack 原生 JUCE 介面](docs/images/rack.png)
![Rev D 黑色 Dynamic 原生 JUCE 介面](docs/images/dynamic.png)

**以JUCE框架所開發的1176風格FET壓縮器，結合經典機架面板與現代化的即時動態分析兩種介面。**

除此之外，並提供了黑色以及白色兩種經典1176的DSP演算法可供選擇



**開始使用：** [下載外掛](https://github.com/Hikari-Tsai/HT-76/releases) · [安裝教學](#平台下載與安裝) · [在 DAW 載入](#在-daw-中掃描與載入) · [安裝問題排查](#安裝問題排查)

## 介面預覽

### Classic Rack

Rev D 使用炭黑髮絲紋，切換 Rev H 時改為中性銀色髮絲紋與深色面板文字。大型 Input／Output 旋鈕、指針 VU 表與長方形 Ratio／Meter 按鈕維持相同佈局。下方雙聲道 IN／OUT PPM 區域也隨版本切換銀色／黑色底板，文字與刻度同步調整對比；LED 音量條保留深色凹槽。旋鈕焦點沿旋鈕外緣顯示。

**Rev D · 黑色面板**

![Rev D 黑色 Rack 原生 JUCE 介面](docs/images/rack.png)

**Rev H · 銀色面板與 PPM 區域**

![Rev H 銀色 Rack 原生 JUCE 介面](docs/images/rack-revh-silver-badda5a1.png)

### Dynamic View

以約 12 秒的 Input、Output 與 Gain Reduction 曲線呈現動態變化，右側顯示雙聲道音量與壓縮量，下方集中放置控制項。金屬面板同樣跟隨版本切換，圖表與計量區維持深色背景。

**Rev D · 黑色面板**

![Rev D 黑色 Dynamic 原生 JUCE 介面](docs/images/dynamic.png)

**Rev H · 銀色面板**

![Rev H 銀色 Dynamic 原生 JUCE 介面](docs/images/dynamic-revh-silver-e3e71d60.png)

以上為原生 JUCE Editor 截圖，由離線測試音訊驅動實際 DSP 產生。

## 功能與操作

- 單聲道及立體聲處理；立體聲共用偵測器與壓縮控制，維持左右聲道的增益關係。
- 4:1、8:1、12:1、20:1，以及獨立的 ALL 模式。
- Rack／Dynamic 即時切換、介面等比縮放，以及視圖與視窗大小保存。
- IN／OUT peak、RMS、peak hold 與核心增益衰減計量；Rack VU 可切換 GR／IN／OUT。
- 支援 DAW 參數自動化、專案狀態還原與主機旁通。
- 旋鈕支援拖曳、Shift 微調、滾輪、方向鍵和雙擊重設。

| 控制項 | 範圍／選項 | 用途 |
| --- | --- | --- |
| Input | −20 至 +40 dB | 調整送入 FET 核心的電平，影響壓縮量與非線性反應 |
| Output | −20 至 +20 dB | 調整輸出級增益；位於模型的變壓器之前，也會影響該級的驅動量 |
| Attack | 20–800 µs | 順時針越快 |
| Release | 50–1100 ms | 順時針越快 |
| Ratio | 4:1／8:1／12:1／20:1 | 選擇壓縮比例 |
| ALL | 開／關 | 啟用上游模型的 all-buttons 模式；退出後保留原 Ratio |
| Bypass | 開／關 | 切換經延遲補償的原始訊號 |
| Circuit revision | Rev D／Rev H（實驗性） | 切換輸入與輸出級模型；以 20 ms 淡化銜接 |

使用時，先調整 Input 取得需要的壓縮量，再用 Output 配合旁通比較音量；Attack 和 Release 決定壓縮對瞬態及後續音量恢復的反應。本介面沒有獨立 Threshold 旋鈕。

IN 在啟用時量測 Input gain 後的訊號，旁通時量測原始訊號；OUT 量測實際送出的音訊。GR 量測核心增益單元的衰減，不以 IN 與 OUT 的差值估算。

## 演算法來源

| 模型與說明連結 | 面板 | 演算法重點 |
| --- | --- | --- |
| [Rev D 演算法說明](docs/REV_D_MODEL.md) | 黑色髮絲紋 | fetcomp-dsp 的 Rev D 導向 FET 核心、Class A 輸出級與變壓器磁性近似 |
| [Rev H 演算法說明（實驗性）](docs/REV_H_MODEL.md) | 銀色髮絲紋 | 沿用 FET 側鏈，新增電子輸入、推挽輸出與獨立變壓器模型；尚未經實機校準 |

### FET 壓縮核心：fetcomp-dsp

| 項目 | 來源 |
| --- | --- |
| 上游專案 | [Paulllux/fetcomp-dsp](https://github.com/Paulllux/fetcomp-dsp) |
| 原作者 | Paul Ulrix |
| 採用版本 | [`de18f5ac793e36397c725abdca7fcb8c08760ce2`](https://github.com/Paulllux/fetcomp-dsp/tree/de18f5ac793e36397c725abdca7fcb8c08760ce2) |
| 本地核心 | [FetLimiterDsp.h](third_party/fetcomp-dsp/FetLimiterDsp.h)、[MathUtils.h](third_party/fetcomp-dsp/MathUtils.h) |
| 上游授權 | [MIT License](third_party/fetcomp-dsp/LICENSE) |

本專案直接整合上游程式碼並做局部修改；FET 電路模型的原始實作歸功於上述作者。

依上游說明，模型參考 UREI 1176LN Rev D 的 R-10743 電路圖，主要包含：

- **JFET 增益單元**：將串聯電阻與分流 JFET 建成非線性分壓器，以逐樣本的二次方程求解壓縮與失真。
- **回授式側鏈**：從增益單元之後取樣，經整流、包絡與控制電路影響後續樣本的增益。
- **Attack／Release 與 Ratio 網路**：描述壓縮建立及恢復過程，並提供 ALL 模式。
- **輸出級音色**：包含 Class A 放大級與 Jiles–Atherton 類型的變壓器磁性模型。

模型包含元件與數值積分近似。上游說明其校準對象為參考外掛；本專案尚未與實體 1176 做量測比對。電路拓撲、近似方式與限制詳見保留的 [上游 README](third_party/fetcomp-dsp/README.md)。

### 數學近似：Chowdhury DSP

上游的 `MathUtils.h` 註明部分三角函式與雙曲函式近似取自或重實作自 **Jatin Chowdhury／[chowdsp_utils](https://github.com/Chowdhury-DSP/chowdsp_utils)** 的數學近似程式碼。

此來源透過 `fetcomp-dsp` 使用，本專案沒有直接連結整套 `chowdsp_utils`。相關模組標示為 BSD 3-Clause，來源註記保留於 [chowdsp-math-module.txt](third_party/licenses/chowdsp-math-module.txt)。

### Rev H 實驗性模型

面板的 `REV D`／`REV H*` 切換實際 DSP 路徑。Rev H 新增電子平衡輸入的行為模型、具有負回授的對稱推挽放大級，以及獨立的可飽和輸出變壓器模型；程式位於 [RevHStages.h](Source/RevHStages.h)，採 Apache 2.0。

**這是依 G／H 電路架構建立的近似模型，尚未經實體 Rev H 校準。** FET 增益單元、Attack／Release、Ratio 與 ALL 仍沿用現有 fetcomp-dsp 核心；新增級的電平、頻寬、飽和與回授係數是目前的工程假設，不能宣稱逐元件或完整實機重現。來源、方程、係數與待驗證項目詳見 [Rev H 模型說明](docs/REV_H_MODEL.md)。

兩個模型會持續運算以保留各自的歷史狀態，因此 CPU 用量增加。IN 表是數位 Input gain 後的參考電平，OUT 是實際輸出；切換淡化期間的 GR 是兩個核心的計量混合值。Rev D 沿用原訊號路徑與參數識別碼，新增版本選擇支援 DAW 自動化與狀態保存。

### 本專案的整合與修改

在保留上游主要電路方程與立體聲連動設計的基礎上，本專案加入：

- **即時音訊記憶體管理**：在 prepare 階段配置工作緩衝區，避免首次 callback 或區塊大小改變時才配置；較大的區塊分段處理。
- **準確的 GR 計量**：取得逐樣本的增益單元衰減，保留區塊內的壓縮峰值，供 VU 與歷史曲線使用。
- **增益與旁通平滑**：Rev D 的 Input 在 wrapper 中套用一次，Rev H 則在電子輸入級之後套用；兩者皆使用 20 ms 平滑。旁通採用 5 ms 交叉淡化，並補償過採樣延遲。
- **過採樣策略**：低於 88.2 kHz 使用 2× IIR 過採樣，88.2 kHz 以上使用原生取樣率，向 DAW 回報實際延遲。
- **計量與狀態整合**：以固定容量 SPSC 佇列傳遞約 60 Hz 的計量資料，修正 reset 的增益平滑狀態，並處理非有限數值。

完整修改紀錄見 [LOCAL_CHANGES.md](third_party/fetcomp-dsp/LOCAL_CHANGES.md)，整合程式位於 [Source/DspEngine.cpp](Source/DspEngine.cpp)。

## 平台、下載與安裝

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
| Windows x64 安裝程式（建議） | `-windows-x64-Setup.exe` |
| 手動安裝單一格式 | 對應平台的 `-AU.zip`、`-VST3.zip` 或 `-AAX.zip` |

GitHub 自動提供的 **Source code (zip／tar.gz)** 是原始碼，不含可直接使用的外掛。Release 不另提供雜湊檔案；CI 內部仍驗證完整性。若改從 **Actions → Build plugins → Artifacts** 下載，先解開 GitHub 的外層 ZIP，再使用其中的安裝檔或格式 ZIP。

**格式怎麼選？** Logic Pro 使用 **AU**；支援 VST3 的 DAW 可選 **VST3**。HT-76 是音訊效果器，需在 DAW 的效果器插槽中載入，不能將 `.vst3` 或 `.component` 當成一般應用程式雙擊啟動。Release 不包含 Standalone 應用程式。

**AAX 狀態：** 目前輸出 AAX Native，未實作 HDX AAX DSP，也未包含 Avid／PACE 簽章。開發版本供 Pro Tools Developer 測試，正式版 Pro Tools 需要有效的 AAX 簽章。macOS 產物為 ad-hoc 簽署、未公證；Windows 產物未做 Authenticode 簽署。參考 [Avid AAX 開發者資訊](https://developer.avid.com/aax/)。

### macOS DMG 安裝與解除安裝

1. 下載 `HT-76-<版本>-<commit>-macos-universal-Installer.dmg`（Intel／Apple Silicon 通用），關閉 DAW 後開啟。
2. 在掛載的磁碟視窗內雙擊 `Install HT-76.pkg`，依提示繼續，於格式選擇頁選擇需要的格式。AU／VST3／AAX 預設全勾選，可自行取消不需要的格式；一般使用者可取消 AAX，因目前僅供 Pro Tools Developer 測試。
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

請保留 DMG，或日後重新下載以取得解除安裝工具。原始碼中的工具為 [uninstall.command](scripts/macos/uninstall.command)；可先預覽而不移除任何檔案：

```sh
bash scripts/macos/uninstall.command --dry-run
```

目前外掛使用 ad-hoc 簽章，PKG／DMG 未做 Developer ID 簽署及 Apple 公證；macOS 可能阻擋開啟。這仍是開發版分發包，DMG 封裝不會補上 Apple 或 Avid／PACE 簽章。

### Windows 安裝與解除安裝

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

若已使用 DMG／EXE 安裝，不必再複製 ZIP 內的外掛。手動安裝適合需要自行管理外掛位置的使用者；Windows 建議使用 EXE，讓安裝程式一併處理 Visual C++ 執行環境。

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
| Pro Tools | 使用 AAX，但**目前未經 Avid／PACE 簽署，正式版 Pro Tools 不會載入**。現有產物只供 Pro Tools Developer 測試，重裝或重新掃描無法解除這項限制。 |

上表是載入與掃描操作說明，不代表已在所有列出的 DAW 完成相容性驗證。第一次使用可先保留預設 Rev D，播放音軌後調整 Input 觀察 GR，再用 Output 配合旁通比較音量。

### 安裝問題排查

| 遇到的情況 | 處理方式 |
| --- | --- |
| macOS 顯示無法驗證開發者或未公證 | 目前外掛只有 ad-hoc 簽章，PKG／DMG 未簽署、未公證。確認檔案來自本專案 Release 且可信後，先嘗試開啟，再到 **系統設定 → 隱私權與安全性 → 強制打開（Open Anyway）**，依系統提示允許該檔案。此選項未必適用於所有阻擋情況；若提示檔案損毀或含惡意軟體，先停止安裝並重新確認來源。見 [Apple 官方說明](https://support.apple.com/en-gb/102445)。 |
| Windows 顯示未知發行者或「Windows 已保護您的電腦」 | 目前安裝程式未做 Authenticode 簽署。確認來源可信且系統提供選項時，可按 **其他資訊 → 仍要執行**；企業政策或 Smart App Control 可能不允許繼續。見 [Microsoft SmartScreen 說明](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation)。 |
| DAW 找不到 HT-76 | 確認下載的是安裝檔而非 Source code、已選取 DAW 支援的格式、完整 bundle 位於正確目錄；重新啟動 DAW 並掃描。Logic Pro 需 AU；只安裝 VST3 不會出現在其清單。 |
| 掃描出現重複版本或仍是舊版 | 關閉 DAW，檢查系統與使用者目錄是否同時有 HT-76 或舊名稱 `1176 Field Effect`。先依上述方式解除舊版，再重新安裝及掃描；不要刪除整個共用外掛目錄。 |
| Windows 提示缺少 VCRUNTIME／MSVCP DLL | 使用本專案 EXE 安裝程式安裝／修復所附的 Microsoft Visual C++ x64 執行環境，再重新開啟 DAW。不要從第三方網站下載個別 DLL。 |
| 外掛已開啟但音量表不動 | 在 DAW 播放有音訊的軌道，確認訊號有經過 HT-76 所在的效果器插槽，並檢查軌道靜音、路由與主機是否停用外掛。 |

安裝程式成功執行不代表 DAW 已通過外掛驗證；若仍失敗，請在 [GitHub Issues](https://github.com/Hikari-Tsai/HT-76/issues) 提供 HT-76 版本、作業系統、CPU 架構、DAW 版本、使用格式及完整錯誤訊息。

## 從原始碼建置

需要 **CMake 3.22+、Git 與 C++17 編譯器**。macOS 使用 Xcode／Command Line Tools；Windows 使用 Visual Studio 2022 的 Desktop development with C++ 工具。

JUCE 固定為 **8.0.12**，commit [`29396c22c93392d6738e021b83196283d6e4d850`](https://github.com/juce-framework/JUCE/tree/29396c22c93392d6738e021b83196283d6e4d850)，內附 AAX SDK 2.9.0。若 `third_party/JUCE` 不存在，CMake 會取得該版本；首次建置需要網路。

### macOS

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

預設發行 Universal 版本，同時包含兩個架構。若只需本機開發，可將架構改成 `arm64` 或 `x86_64`。測試程式需在可執行目標架構的環境中執行。

Apple Silicon 也可直接執行 `./scripts/build-macos.sh`。編譯完成後，`./scripts/install-macos.sh` 會安裝 AU／VST3，並先備份既有同名外掛；AAX 不包含在此安裝腳本內。

### Windows

在 Visual Studio 2022 Developer PowerShell 中執行：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target FieldEffect_VST3 FieldEffect_AAX FieldEffectTests --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

外掛產物位於 `build/FieldEffect_artefacts/Release/<格式>/`。各格式的 CMake target 為 `FieldEffect_AU`、`FieldEffect_VST3`、`FieldEffect_AAX`；Standalone 為 `FieldEffect_Standalone`。

### Projucer／Xcode

專案檔為 [HT-76.jucer](HT-76.jucer)，請使用與模組相符的 Projucer 8.0.12。若本機尚無 JUCE checkout，可先準備固定版本：

```sh
git clone --branch 8.0.12 --depth 1 https://github.com/juce-framework/JUCE.git third_party/JUCE
./scripts/open-projucer.sh
```

若 `third_party/JUCE` 已存在，直接執行開啟腳本。腳本會在需要時建置相符版本的 Projucer；若其他版本正在執行，請先儲存並結束它。

在 Projucer 選擇 **Xcode (macOS) → Save and Open in IDE**，再選擇 AU、VST3、AAX 或 Standalone Plugin target。匯出專案位於 `Builds/MacOSX/HT-76.xcodeproj`。

## GitHub Actions 與 Release

[build-plugins.yml](.github/workflows/build-plugins.yml) 支援 push、Pull Request 與手動執行。Windows x64 與 macOS Universal 各自編譯、執行測試、檢查架構並打包；Universal 產物另外送到 Intel runner 執行同一份測試程式與安裝／解除安裝測試。全部成功時產生 **5 份外掛 ZIP、1 份 Universal macOS DMG 與 1 份 Windows EXE**，另附 CI 建置紀錄。

**推送 `v` 開頭的 tag，會在建置與 Intel 驗證全部成功後自動發佈同名 GitHub Release。**

新建 Release 的說明會自動附上 macOS Gatekeeper／簽署與公證狀態、Windows SmartScreen／未知發行者提示，以及未簽署 AAX 無法在正式版 Pro Tools 載入的限制，並提供各平台的官方說明連結。

```sh
git tag -a v0.93 -m "HT-76 v0.93 pre-release"
git push origin v0.93
```

`v0.93` 對應外掛版本 `0.9.3`，此次發行標示為 **Pre-release**，不設為 Latest。所有平台建置、測試與打包成功後才會公開發行。

發佈工作在 CI 內驗證五份 ZIP、一份 Universal DMG、一份 Windows EXE 與各自的 SHA-256，Release 只上傳這七份安裝下載檔，先建立草稿，全部上傳完成才公開。重新執行會沿用同名 Release 並更新同名附件；immutable releases 啟用後無法覆寫已公開附件。一般分支、PR、手動執行和非 `v` 開頭 tag 只產生 Actions artifacts。

產品版本來自 `CMakeLists.txt`，**tag 不會自動修改外掛版本**；發佈前請同步更新 CMake 與 Projucer 的版本。ZIP／DMG／EXE 檔名包含產品版本、commit、平台、架構與格式。Actions 外掛 artifacts 保存 30 天、logs 保存 14 天；Release 附件不受此期限影響。

工作流程使用 GitHub 自動提供的 token，只有發佈工作取得 `contents: write`，不需額外 PAT。提交時請包含 `.github/`、原始碼、測試、腳本、`design/assets/`、`docs/images/` 與隨附的 DSP 原始碼；JUCE 由 CI 取得，不必提交。

本機可使用相同打包程式：

```sh
python3 scripts/package-plugins.py --build-dir build --platform macos --arch universal --formats AU VST3 AAX --output-dir dist
python3 scripts/package-dmg.py --arch universal --archive-dir dist --output-dir dist
python3 scripts/verify-dmg.py dist/HT-76-0.9.3-local-macos-universal-Installer.dmg
```

DMG 工具使用 macOS 內建的 `pkgbuild`、`productbuild` 與 `hdiutil`。需先產生同一版本、架構及 revision 的 AU／VST3／AAX ZIP。`verify-dmg.py` 預設只掛載、展開與檢查安裝包；上述檔名應依實際版本調整。macOS CI 會在 Apple Silicon 與 Intel 的全新 GitHub-hosted runner 上，使用同一份 Universal DMG 實際安裝三種格式、核對雙架構外掛與 receipts，再執行解除安裝及重複執行檢查。

Windows 安裝程式使用 **Inno Setup 6.3+**（GitHub `windows-2022` runner 已預裝 Inno Setup 6）。本機 Windows 安裝 Inno Setup 6 與 Visual Studio 2022 C++ 工具後，可執行：

```powershell
python scripts/package-plugins.py --build-dir build --platform windows --arch x64 --formats VST3 AAX --output-dir dist
python scripts/package-windows.py --archive-dir dist --output-dir dist
```

若工具不在預設位置，可指定 `--iscc <ISCC.exe 路徑>` 與 `--vc-redist <vc_redist.x64.exe 路徑>`。Windows CI 在全新的 GitHub-hosted runner 上驗證預設全選安裝、重裝、單獨選擇 VST3、解除安裝與使用者檔案保留。這些會修改系統安裝目錄的測試只允許在 GitHub-hosted runner 執行。

## 測試與目前驗證範圍

測試涵蓋單／雙聲道、44.1–192 kHz 取樣率、不同區塊大小、參數與狀態還原、旁通延遲、GR／peak／RMS 計量、非有限數值及原生 Editor。另有 macOS 音訊 callback 記憶體配置檢查與打包檢查測試。

```sh
ctest --test-dir build -C Release --output-on-failure
python3 -m unittest discover -s Tests -p 'test_package*.py' -v
```

`v0.91` 原先分架構的 [GitHub Actions](https://github.com/Hikari-Tsai/HT-76/actions/runs/35358874019) 已通過 Windows x64、macOS Intel／ARM 的建置、音訊／介面測試、打包與安裝／解除安裝檢查。Universal 工作流程要求每個外掛包含兩個 Mach-O 架構，並在 Apple Silicon 與 Intel runner 執行同一份測試產物。AAX 尚未在 Pro Tools 主機內驗證載入與播放。上述測試不代表已完成所有 DAW 相容性、長時間壓力測試或實體硬體音色比對。

## 專案結構

| 路徑 | 內容 |
| --- | --- |
| [Source/PluginProcessor.cpp](Source/PluginProcessor.cpp) | JUCE 主機介面、參數自動化、狀態與旁通 |
| [Source/DspEngine.cpp](Source/DspEngine.cpp) | FET 核心整合、平滑、過採樣延遲與計量 |
| [Source/PluginEditor.cpp](Source/PluginEditor.cpp) | Rack／Dynamic 原生 JUCE 介面 |
| [Source/MeterBridge.h](Source/MeterBridge.h) | 音訊執行緒到 UI 的固定容量計量佇列 |
| [third_party/fetcomp-dsp/](third_party/fetcomp-dsp/) | 上游 DSP、MIT 授權與本地修改紀錄 |
| [Tests/](Tests/) | 音訊、狀態、Editor 與打包測試 |
| [scripts/](scripts/) | 建置、安裝、Projucer 開啟與跨平台打包 |
| [design/assets/](design/assets/) | 原生面板使用的旋鈕與材質 |
| [.github/workflows/](.github/workflows/) | 建置與 tag Release 自動化 |

## 致謝與授權來源

感謝 **Paul Ulrix** 提供 FET 壓縮模型、**Jatin Chowdhury／Chowdhury DSP** 提供數學近似程式碼，以及 **JUCE** 與 **Avid** 提供外掛開發框架和 SDK。

HT-76 自身的原創程式碼、文件與專案素材採用 **Apache License 2.0**，完整條文見 [LICENSE](LICENSE)，來源聲明見 [NOTICE](NOTICE)。

第三方元件維持原授權：`fetcomp-dsp` 為 MIT；ChowDSP 數學模組標示為 BSD 3-Clause；JUCE 為 AGPLv3／商業雙重授權；AAX SDK 適用其隨附條款。Apache 2.0 僅涵蓋本專案原創部分，不取代這些依賴或其衍生成品適用的授權要求。

完整依賴版本與來源請見 [third_party/DEPENDENCIES.md](third_party/DEPENDENCIES.md)，並保留相關原作者及授權註記。
