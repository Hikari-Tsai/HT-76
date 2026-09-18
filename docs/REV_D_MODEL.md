# Rev D 演算法說明

Rev D 是 HT-76 的預設模型，使用黑色髮絲紋面板。未包含版本選擇的舊專案載入後也會選回 Rev D。

## 來源與實作

壓縮核心直接整合 Paul Ulrix 的 [fetcomp-dsp](https://github.com/Paulllux/fetcomp-dsp)，採用版本為 [`de18f5ac793e36397c725abdca7fcb8c08760ce2`](https://github.com/Paulllux/fetcomp-dsp/tree/de18f5ac793e36397c725abdca7fcb8c08760ce2)，保留 [MIT 授權](../third_party/fetcomp-dsp/LICENSE)。依作者的[原始說明](../third_party/fetcomp-dsp/README.md)，電路模型參考 UREI 1176LN Rev D 的 R-10743 圖面。

主要程式為 [FetLimiterDsp.h](../third_party/fetcomp-dsp/FetLimiterDsp.h)，JUCE 整合位於 [DspEngine.cpp](../Source/DspEngine.cpp)。

## 訊號與壓縮方式

主要音訊路徑：Input 增益 → 過採樣／耦合濾波 → FET 增益單元 → Class A 輸出級 → Output 增益 → 變壓器模型 → 降採樣。

- **FET 增益單元**：以串聯電阻與分流 JFET 的非線性分壓關係，逐樣本求解增益與失真。
- **回授側鏈**：從增益單元後取樣，經整流與包絡網路更新後續的 FET 控制電壓。立體聲共用偵測器，維持兩聲道的增益關係。
- **Attack／Release、Ratio 與 ALL**：沿用上游的時間常數網路、比例映射及校準資料。
- **輸出級**：沿用上游 Class A 放大級及 Jiles–Atherton 類型的變壓器磁性近似。Output 在變壓器之前，會改變該級的驅動量。
- **數值處理**：低於 88.2 kHz 採 2× IIR 過採樣；88.2 kHz 以上採原生取樣率。Input 使用 20 ms 平滑，旁通使用具延遲補償的 5 ms 淡化。

## 與 Rev H 的差別

[Rev H（實驗性）](REV_H_MODEL.md) 改用電子輸入、對稱推挽放大及獨立的可飽和輸出變壓器行為模型；FET 與壓縮側鏈仍沿用此核心。版本切換採 20 ms 淡化，兩條路徑持續運算以保存各自狀態。面板顏色由同一個版本參數決定，支援 DAW 自動化及專案還原。

## 驗證與限制

上游模型的校準對象是參考外掛，並非本專案實測的 Rev D 硬體。變壓器及元件行為包含近似，不能將它視為每一台黑面板 1176 的精確重現。

加入 Rev H 時，曾以修改前核心進行五種取樣率、共 254,000 個樣本的比對，Rev D 結果逐樣本完全一致；這是有限測試範圍內的相容性證據。整合修改詳見 [LOCAL_CHANGES.md](../third_party/fetcomp-dsp/LOCAL_CHANGES.md)。

[回到 README](../README.md#演算法來源)
