[![繁體中文](https://img.shields.io/badge/%E7%B9%81%E9%AB%94%E4%B8%AD%E6%96%87-62B6A5?style=for-the-badge)](README.md)[![English](https://img.shields.io/badge/English-454B50?style=for-the-badge)](README.en.md)

# HT-76

![Rev D 黑色 Rack 動態預覽](docs/images/rack-revd.gif)
![Rev D 黑色 Dynamic 動態預覽](docs/images/dynamic-revd.gif)

**以JUCE框架所開發的1176風格FET壓縮器，結合經典機架面板與現代化的即時動態分析兩種介面。**

除此之外，並提供了黑色以及白色兩種經典1176的DSP演算法可供選擇

## 下載與安裝

[![AAX](https://img.shields.io/badge/AAX-662D91?style=for-the-badge&logo=protools&logoColor=FFFFFF)](#plugin-downloads)[![AU](https://img.shields.io/badge/AU-D1D1D6?style=for-the-badge&logo=data%3Aimage%2Fsvg%2Bxml%3Bbase64%2CPHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI%2BPHBhdGggZmlsbD0ibm9uZSIgc3Ryb2tlPSIjMUQxRDFGIiBzdHJva2Utd2lkdGg9IjIuNSIgc3Ryb2tlLWxpbmVjYXA9InJvdW5kIiBkPSJNMyAxMHY0bTQtN3YxMG01LTE0djE4bTUtMTR2MTBtNC03djQiLz48L3N2Zz4%3D)](https://github.com/Hikari-Tsai/HT-76/releases/download/v0.93/HT-76-0.9.3-8fc2d5056ea9-macos-universal-AU.zip)[![VST3](https://img.shields.io/badge/VST3-C90526?style=for-the-badge&logo=data%3Aimage%2Fsvg%2Bxml%3Bbase64%2CPHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9Ijk3MCAwIDQyMCA1MDAiPjxwYXRoIGZpbGw9IiNGRkZGRkYiIGZpbGwtcnVsZT0iZXZlbm9kZCIgZD0iTTEyMjAuMSw3LjFsODAuMiw4MC4yYy04Mi4yLDUuNi0xNDcuMSw3NC4xLTE0Ny4xLDE1Ny43YzAsODcuMSw3MC4zLDE1Ny43LDE1Ny4zLDE1OC4xbC05MC4zLDkwLjNMOTc3LDI1MC4zIEwxMjIwLjEsNy4xTDEyMjAuMSw3LjF6IE0xMjQ0LjEsMjQ1LjFjMC0zNy4xLDMwLjEtNjcuMiw2Ny4yLTY3LjJjMzcuMSwwLDY3LjIsMzAuMSw2Ny4yLDY3LjJjMCwzNy4xLTMwLjEsNjcuMi02Ny4yLDY3LjIgQzEyNzQuMSwzMTIuMiwxMjQ0LjEsMjgyLjIsMTI0NC4xLDI0NS4xTDEyNDQuMSwyNDUuMXoiLz48L3N2Zz4%3D&logoColor=FFFFFF)](#plugin-downloads)[![DMG](https://img.shields.io/badge/DMG-000000?style=for-the-badge&logo=apple&logoColor=FFFFFF)](https://github.com/Hikari-Tsai/HT-76/releases/download/v0.93/HT-76-0.9.3-8fc2d5056ea9-macos-universal-Installer.dmg)[![EXE](https://img.shields.io/badge/EXE-0078D4?style=for-the-badge&logo=data%3Aimage%2Fsvg%2Bxml%3Bbase64%2CPHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI%2BPHBhdGggZmlsbD0iI0ZGRkZGRiIgZD0iTTIgMmg5djlIMnptMTEgMGg5djloLTl6TTIgMTNoOXY5SDJ6bTExIDBoOXY5aC05eiIvPjwvc3ZnPg%3D%3D&logoColor=FFFFFF)](https://github.com/Hikari-Tsai/HT-76/releases/download/v0.93/HT-76-0.9.3-8fc2d5056ea9-windows-x64-Setup.exe)

AAX／VST3 請在下表選擇平台；AU、DMG、EXE 按鈕直接下載 **v0.93**。

1. **下載：** 在 Releases 展開 **Assets**，依平台選擇下表的安裝檔。標示 Pre-release 的版本為測試版；Source code 是原始碼。
2. **安裝：** 關閉 DAW，執行安裝程式並選擇格式。所有格式預設勾選，可取消不需要的項目。
3. **載入：** 重新開啟 DAW 並掃描外掛，在音訊效果器中搜尋 **HT-76**（廠商 **Field Effect**）。Logic Pro 選 AU，支援 VST3 的 DAW 選 VST3。

<a id="plugin-downloads"></a>

| 平台 | 下載與安裝 | 格式 |
| --- | --- | --- |
| macOS 12+ · Intel／Apple Silicon 通用 | 開啟 `*-macos-universal-Installer.dmg` → 執行 `Install HT-76.pkg` | [AU](https://github.com/Hikari-Tsai/HT-76/releases/download/v0.93/HT-76-0.9.3-8fc2d5056ea9-macos-universal-AU.zip)、[VST3](https://github.com/Hikari-Tsai/HT-76/releases/download/v0.93/HT-76-0.9.3-8fc2d5056ea9-macos-universal-VST3.zip)、[AAX](https://github.com/Hikari-Tsai/HT-76/releases/download/v0.93/HT-76-0.9.3-8fc2d5056ea9-macos-universal-AAX.zip) |
| Windows x64 | 執行 `*-windows-x64-Setup.exe`，內含 Visual C++ 執行環境 | [VST3](https://github.com/Hikari-Tsai/HT-76/releases/download/v0.93/HT-76-0.9.3-8fc2d5056ea9-windows-x64-VST3.zip)、[AAX](https://github.com/Hikari-Tsai/HT-76/releases/download/v0.93/HT-76-0.9.3-8fc2d5056ea9-windows-x64-AAX.zip) |

**目前為未正式簽署的測試版。** 系統若阻擋安裝，請參考[安裝問題排查](docs/INSTALLATION.md#安裝問題排查)。AAX 未經 Avid／PACE 簽署，僅供 Pro Tools Developer 測試，正式版 Pro Tools 無法載入；一般使用者可取消 AAX。

**解除安裝：** macOS 執行 DMG 內的 `Uninstall HT-76.command`，依提示輸入 `UNINSTALL`；Windows 至「設定 → 應用程式」解除安裝 **HT-76 Plugins**。操作前請先關閉 DAW。

ZIP 手動安裝、外掛路徑及 DAW 掃描步驟請見[完整安裝指南](docs/INSTALLATION.md)。

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
