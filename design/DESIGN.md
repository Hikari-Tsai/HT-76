# HT-76 / Field Effect — 雙面板設計 v6

日期：2026-09-18。

實作狀態：已完成 JUCE 原生 C++ 外掛，建置與測試資訊請見專案 README。以下保留面板設計階段的範圍與對接規格；其中「尚未建立」描述的是當時的設計階段。

## 設計階段交付與範圍

這一輪先完成面板設計：AI 雙面板概念圖、可切換的互動面板預覽、以及後續 JUCE 整合規格。尚未建立 JUCE C++ 外掛、尚未接入 fetcomp-dsp，也沒有音訊輸出或實機音色驗證。

- `../output/imagegen/1176-dual-panel-concept.png`：2048 × 1536 的 AI 造型概念圖。
- `panel-preview.html`：可編輯的互動設計來源。
- `index.html`：可直接以瀏覽器開啟的獨立預覽。修改來源後執行 `python3 design/build-preview.py` 更新。
- `previews/`：瀏覽器檢查截圖。
- `prompts/panel-concept.txt`：圖片生成使用的完整提示詞。透過 imagegen 提供的 CLI 呼叫 gpt-image-2，high quality。API 金鑰只從剪貼簿載入程序環境，未存入專案。

AI 圖片定義材質與視覺方向；刻度、數值、按鍵狀態可能有生成偏差，不用作工程規格。互動原型與本文件定義控制邏輯。概念圖中兩張面板的 Ratio 狀態不同，正式產品和互動原型則必須共用同一組參數。

## 視覺方向

產品名稱為 **HT-76**。本輪以使用者附上的原始概念圖為唯一外觀基準，取代上一版藍灰色與固定 2U 的要求。中性炭黑拉絲金屬、暖灰文字、青綠輸入、米白輸出及琥珀 GR / 選中燈號。面板不再加藍色色罩。

從 1824 × 1368 顯示圖量測（原檔為 2048 × 1536）：Rack 邊界約 `(33,58)–(1786,515)`，Dynamic 約 `(33,583)–(1786,1278)`。因此採 Rack **3.84:1**、Dynamic **2.52:1**，而非實體 19 吋 / 2U 比例。原生設計座標寬 1280，兩種高度分別為 333.69 與 507.47；切換時寬度一致，高度隨模式改變。

Rack 依附件定位 Input、Output、上下堆疊的 Attack / Release、垂直 Ratio、右側 VU 及底部雙聲道 LED 列；Dynamic 依附件恢復上方圖表與右側電平表、下方整排旋鈕 / Ratio / Bypass。VU 改為寬扇形雙刻度，紙面取自附件的空白區。

色票：面板取樣中位數約 #282829、圖表約 #0F1417、文字 #CFC5AA、輸入 #71BDAE、輸出 #E3D5AD、GR #EDB552。面板、旋鈕和 VU 的實際色彩由擷取的像素材質決定。概念圖生成的部分數字沒有工程意義，實際旋鈕保留對應參數的有效刻度；兩模式共用參數，不複製原圖中互相矛盾的 Ratio 選中狀態。

### 概念圖取材

本次直接對照使用者附上的 `output/imagegen/1176-dual-panel-concept.png`。

- `assets/concept-knob-body.png`：從原概念圖擷取旋鈕，移除原指針並補齊表面。互動指針獨立繪製，旋轉時固定表面光源。
- `assets/concept-metal.jpg`：從原概念圖的無字面板區擷取並鏡像接縫，保留拉絲紋理。
- `extract-concept-assets.py`：可重現取材流程；需要 Pillow，可用 `uv run --with pillow python design/extract-concept-assets.py`。
- `assets/concept-vu-paper.jpg`：從概念圖的無字 VU 紙面區擷取並鏡像接縫。
- `assets/concept-metal-blue.jpg` 與 `panel-2u.css`：保留上一版素材，不再載入。
- `panel-skin.css`：基本外觀；`panel-reference.css`：v5 依附件量測的固定座標布局；`build-preview.py` 把材質嵌入預覽，使內嵌和獨立預覽一致且可離線顯示。
- 此次没有重新呼叫 imagegen，未使用 API 金鑰。資產取自既有生成圖，原圖保留不變。

這些是面板審閱用素材。正式 JUCE 出貨時需製作更高解析度旋鈕素材或等效原生繪製，並驗證不同 DPI；原始素材尺寸不足以直接保證任意放大品質。

## v6 控制項微調

- Input / Output 旋鈕放大；Rack 外部直徑增加約 21%，Dynamic 約 15–19%，並維持原面板比例。
- 滑鼠選取與鍵盤焦點的光圈僅描繪旋鈕本體外緣，刻度與文字不納入光圈。
- Rack Ratio 按鈕為 58 × 28 px、Dynamic Ratio 約 55 × 32 px、Meter 為 62 × 27 px，均為直角橫向長方形。

## Rack 面板

- 深色拉絲金屬、機架耳、螺絲與有立體邊緣的旋鈕。
- Input / Output 大旋鈕，Attack / Release 小旋鈕。
- 4 / 8 / 12 / 20 Ratio 按鍵與獨立 ALL。
- 附件暖紙色背光 VU 表，可選 GR / IN / OUT。
- 下方細長立體聲 IN / OUT 電平條；搭配 dBFS 數值。
- VU 在 GR 模式以 0 dB 為無衰減，增加壓縮時向左偏轉。IN / OUT 模式的設計校準為 -18 dBFS = 0 VU；正式實作需 RMS / VU 球動特性，不能直接用瞬時 peak 冒充 VU。

Rack 已改為與概念圖相同的 Attack / Release 上下堆疊、Input / Output 大旋鈕與右側橫式 VU 表。

## Dynamic 面板

- 左側為十二秒滾動電平歷史圖，時間向右推進，最新訊號位於右側。
- 輸入顯示青綠輪廓與低透明填色，輸出顯示米白輪廓；這是電平時間序列，並非 FFT 頻譜。
- GR 以琥珀線從 0 dB 向下偏移；使用獨立、清楚標示的 dB 衰減刻度，不能把 GR 與 dBFS 當同一個量。
- 圖表旁為 IN L/R、GR、OUT L/R 電平表。電平由下往上，GR 由上往下；包含 peak 指示與數字。
- 下方控制區依序為 Input、Attack、Release、Output、Ratio、Bypass；旋鈕仍保持可直接操作。
- 未加入 Threshold 旋鈕；壓縮深度以 Input 驅動。

FabFilter 參考重點是同時呈現 IN / OUT / GR、易讀的時間圖與精細電平表；產品標識與面板造型使用本案自己的設計。

## 互動原型

- RACK / DYNAMIC 切換後保留所有數值、Ratio 與 Bypass。
- 旋鈕上下拖曳、滑鼠滾輪、鍵盤方向鍵；Shift 精細拖曳 / 滾輪；雙擊回到預設值。
- Attack / Release 越往順時針數值越小、反應越快。
- ALL 是獨立開關，記住先前單一 Ratio；退出 ALL 回到前一 Ratio。選任一數字則退出 ALL。
- GR / IN / OUT 切換 VU 來源。
- 示範工具列位於產品面板外，可選 Drums / Vocal / Sustain、暫停 / 繼續；不屬於正式外掛面板。
- Bypass 示範使 IN / OUT 相等且 GR 為零；正式實作需做無爆音旁通及延遲對齊。
- 支援 reduced-motion：初次載入時暫停示範，可手動開啟。

原型使用 **電平域模擬資料** 來驗證操作和動態顯示，沒有實際樣本音訊、沒有移植 FET 模型。示範的 stereo 差異亦是視覺示意。不能依照此預覽判斷壓縮器音質、20 µs attack、精確 ratio 或失真表現。

## JUCE 實作對接規格

建議 JUCE / CMake，第一目標為 macOS Standalone、AU、VST3。已確認本機 Xcode 26.6 與 CMake 指令可用；此輪未取得或建置 JUCE。

兩個原生 `juce::Component` 視圖共用同一個 `AudioProcessorValueTreeState`，使用 `SliderAttachment` / `ButtonAttachment` 或等效參數綁定。編輯器只控制參數及消費計量資料，不執行 DSP。主機自動化更新需同步到兩個視圖，切換視圖不得重建 DSP、清除壓縮包絡或改變聲音。

| UI 參數 | APVTS id（規劃） | fetcomp-dsp 欄位 | 範圍 / 映射 |
| --- | --- | --- | --- |
| Input | inputDb | Parameters::inputDb | -20 至 +40 dB，線性 dB |
| Output | outputDb | Parameters::outputDb | -20 至 +20 dB，線性 dB |
| Attack | attackUs | Parameters::attackUs | 20 至 800 µs，反向對數 |
| Release | releaseMs | Parameters::releaseMs | 50 至 1100 ms，反向對數 |
| Ratio | ratio | Parameters::ratio | four / eight / twelve / twenty |
| ALL | allButtons | Parameters::allButtons | 獨立 bool，保留 Ratio |
| Bypass | bypass | 外層處理器控制 | 旁通整條效果链，保留延遲 |

上述範圍直接依據已檢查的 `FetLimiterDsp.h`。原型預設 Input +12 dB、Output -6 dB、Attack 200 µs、Release 400 ms、Ratio 4:1 是視覺示範起點；正式初始 preset 應以音訊測試校準。

`limiting=false` 是關閉壓縮但保留染色，與整體 Bypass 不同；首版主面板只露出 Bypass，不能把兩者混用。`ironAmount`、`linearisation`、`txModel`、`relModel` 初期保留參考 DSP 的預設，不以未經測試的旋鈕暴露。

### 計量資料流

1. Audio thread 收集待壓縮的 input peak / RMS、DSP 真正的 gain-cell reduction、最終 output peak / RMS。清楚定義 IN 為 Input gain 後訊號，OUT 為 Output gain 後訊號。必須檢查核心既有增益與平滑邏輯，避免計量路徑重複施加 Input gain。
2. GR 使用核心控制增益；不能用 `input dB - output dB` 推算，因 Output gain、飽和、變壓器和濾波都會改變輸出。
3. Audio thread 在預先配置的固定時間桶聚合 peak / RMS / 最大 GR；依樣本時鐘產生每秒約 60 個歷史桶。UI 再以約 60 Hz 消費；不能由 UI timer 採樣 audio buffer。
4. 用預先配置的 single-producer / single-consumer ring buffer 傳送，採 JUCE `AbstractFifo` 或已驗證等效實作；audio callback 禁止記憶體配置、鎖、檔案 IO、GUI 操作。
5. 十二秒顯示約 720 桶；逐像素以 min/max 或 peak 聚合保留瞬態。UI 落後時丟棄過舊歷史，音訊不得等待畫面。
6. 電平表即時上升、適量下降平滑、約 1 秒 peak hold；VU 另用約 300 ms 響應設計並量測校準。顯示範圍 IN / OUT -60 至 +6 dBFS，GR 0 至 24 dB。超出顯示範圍保留真實數字及清楚的超限提示。
7. 過採樣及 DSP 延遲必須透過 `getLatencySamples()` 等核心介面確認、向主機申報，並補償 IN / OUT 歷史曲線的時間對齊。不同視圖不得改變延遲。
8. 圖表用 `juce::Graphics` / `Path` 原生繪製。AI 圖片是設計參考，不能整張塞進外掛來假裝所有旋鈕都可縮放和互動。背景材質可獨立製作 asset，文字、曲線與刻度保持向量。

### 元件邊界

- `PluginProcessor`：APVTS、狀態存取、DSP 生命周期、主機旁通和延遲申報。
- `FetDspAdapter`：固定版本的上游 FET 核心、參數轉換與 GR 計量接口。
- `MeterBridge`：固定容量資料佇列、peak / RMS / GR 聚合。
- `PluginEditor`：切換視图、尺寸和無障礙處理。
- `RackPanel` / `DynamicPanel`：各自布局與控制項；共用 `FetLookAndFeel`。
- `LevelHistoryComponent` / `StereoMeterComponent` / `VuMeterComponent`：只讀取計量狀態并繪製。

兩種视圖寬度均為 1280 logical px；Rack 高 333.69、Dynamic 高 507.47，依附件的各自比例調整。正式 JUCE 切換模式時保留寬度和參數，並按新模式調整高度與 resize constrainer。網頁原型以 CSS zoom 整體縮放，保持附件布局；窄螢幕文字也會縮小，正式桌面編輯器應限制最小寬度並驗證 DPI。這一輪未建立 JUCE C++ 元件。

v5 已以瀏覽器截圖對照附件，互動及布局結果見 `previews/verification-v5.json`；截圖為 `rack-reference-v5.png`、`dynamic-reference-v5.png`。

## 後續驗收

- 兩種面板的參數、preset、主機 automation、undo 手勢同步；同參數時切換面板前後音訊逐樣本一致。
- Timing 方向、ALL 切換及 host bypass 與 limiting off 的區別。
- 真實聲音的 IN / OUT / GR 顯示和已知測試訊號量測一致，跨不同 block size 正確更新。
- 無輸入時無虛構動態；靜音、NaN / Inf、極大訊號、重啟播放均無不穩定。
- 44.1 / 48 / 96 kHz、mono / stereo、不同 block size；過採樣延遲對齊與主機補償。
- 檢查 audio thread 無配置與鎖，UI 60 Hz 負載可接受；關閉視窗不影響音訊。

## 來源

- [Universal Audio 1176LN 官方手冊](https://media.uaudio.com/assetlibrary/1/1/1176ln_manual.pdf)：機身寬 19 吋、高 3.5 吋，佔兩個 rack units。

- [fetcomp-dsp 專案](https://github.com/Paulllux/fetcomp-dsp)
- [FetLimiterDsp.h](https://github.com/Paulllux/fetcomp-dsp/blob/master/FetLimiterDsp.h)
- [上游 MIT 授權](https://github.com/Paulllux/fetcomp-dsp/blob/master/LICENSE)；正式整合時保留授權文字，另處理 JUCE 授權。
- [FabFilter Pro-C 顯示與計量官方文件](https://www.fabfilter.com/help/pro-c/using/displays)

此輪未複製上游 DSP 程式碼至專案，也未對其音色或穩定性作出實測結論。
