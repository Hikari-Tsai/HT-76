# 本機簽署 Release 的 macOS AAX

此工具沿用已驗證的 PACE 本機測試簽署流程，處理 **macOS Universal AAX ZIP**。Windows AAX、DMG／EXE 內的 AAX 需要另外簽署／重新打包。

## 日常操作

在專案根目錄執行：

```bash
# 下載最新公開 Release（包含 Pre-release）、簽署、驗證，再替換 GitHub 附件。
./scripts/sign-release-aax.sh --upload

# 指定版本，避免選到其他 Release。
./scripts/sign-release-aax.sh --tag v0.93 --upload

# 只在本機下載、簽署及產出 ZIP，不修改 GitHub。
./scripts/sign-release-aax.sh --tag v0.93
```

`latest` 以公開發佈時間選擇，包含 Pre-release，略過草稿；不是 GitHub 的正式版 Latest 標籤。不支援覆寫 immutable Release。建議發佈時指定 tag，並等該 tag 的 CI 發佈工作完成後再執行。

不加 `--upload` 不會上傳任何檔案。之後加上 `--upload` 再執行時，會重新下載與簽署，不直接沿用上次產物。

## 首次設定／換電腦

需要 macOS、Bash、Python 3（僅標準函式庫）、Git、已登入且有儲存庫寫入權限的 GitHub CLI (`gh auth login`)、PACE wraptool，以及已匯入本機鑰匙圈、可使用私鑰的簽署身分。`codesign`、`lipo`、`ditto` 也必須可用。

預設 wraptool 路徑：

```text
/Applications/PACEAntiPiracy/Eden/Fusion/Versions/6/bin/wraptool
```

設定檔放在儲存庫之外的 `~/.config/ht76/pace.json`。已有設定時不要覆寫；新電腦可先建立：

```bash
mkdir -p ~/.config/ht76
chmod 700 ~/.config/ht76
cp -n scripts/pace-signing.example.json ~/.config/ht76/pace.json
chmod 600 ~/.config/ht76/pace.json
```

以文字編輯器填入三個欄位：`PACE_CUSTOMER_NUMBER`、`PACE_CUSTOMER_NAME`、`PACE_SIGN_ID`。範例檔只有空值，實際帳號與身分名稱不應提交。設定檔沒有私鑰，私鑰仍在本機鑰匙圈；腳本不會更改憑證信任設定。

也可透過同名環境變數覆寫設定。其他選项：

```bash
./scripts/sign-release-aax.sh --config /absolute/path/to/private.json --tag v0.93 --upload
./scripts/sign-release-aax.sh --wraptool /absolute/path/to/wraptool --tag v0.93
./scripts/sign-release-aax.sh --help
```

`PACE_WRAPTOOL` 環境變數可設定工具位置；`--wraptool` 優先。`--repo` 預設為 `Hikari-Tsai/HT-76`。設定檔權限必須為 `600` 或更嚴格。

## 腳本做了什麼

1. 保存 Release 原始說明與附件資訊，選取唯一的 macOS Universal AAX ZIP。
2. 下載、比對 GitHub 的 SHA-256／大小，檢查 ZIP 完整性與路徑。
3. 以 `ditto` 解壓，執行下列 PACE 指令，再驗證 PACE、macOS 簽章及 arm64／x86_64。
4. 更新 ZIP 內的 README 與 build-info，保留原始版本、commit、授權文件，以 `ditto` 重新打包並保留符號連結。
5. 解壓新 ZIP，再驗證一次簽章與架構。
6. 有 `--upload` 才繼續：重新讀取 Release，確認附件與說明未被更動；替換同名 ZIP，更新簽署說明，再核對遠端 SHA-256、大小及其他附件。保留 Pre-release 狀態，不修改 tag。

腳本實際使用的簽署參數與手動流程相同；以下變數由本機 JSON／環境變數提供：

```bash
"$PACE_WRAPTOOL" sign \
  --customernumber "$PACE_CUSTOMER_NUMBER" \
  --customername "$PACE_CUSTOMER_NAME" \
  --signid "$PACE_SIGN_ID" \
  --in "$INPUT_BUNDLE" \
  --out "$OUTPUT_BUNDLE"
```

Bash 入口呼叫相鄰的 Python 輔助檔處理 JSON、ZIP 與 GitHub 回應；使用時只需一行 Bash 指令。兩支程式都必須保留。

## 備份、失敗與簽署範圍

每次執行都會印出獨立的本機工作目錄：`$(git rev-parse --absolute-git-dir)/local-signing/run-…/`。其中保存 `original/` 原始 ZIP、`upload/` 簽署 ZIP、`release-before.json`、`release-notes.md`、驗證摘要及 `logs/`。目錄／檔案以私人權限建立，位於 Git 管理目錄，不會被提交。工作目錄可能很大，可在確認發佈後手動清理。

PACE 日誌可能包含帳號與簽署資訊，請留在本機；腳本只上傳選定的 AAX ZIP 及不含設定值的 Release 說明，不上傳工作目錄或雜湊附件。不要開啟 shell tracing 或把簽署設定寫入 Actions。PACE 工具本身仍會接收所需參數，這不是對同一部電腦上其他程式的秘密隔離機制。

簽署／驗證失敗會停止，保留本機檔案。GitHub 附件替換和 Release 說明更新不是單一原子操作；若在上傳階段斷線，先檢查 Release，再決定重試或用 `original/` 與 `release-before.json` 復原。腳本不會自動覆蓋可能已被其他人更新的內容。

此版本的文件與封裝中繼資料針對**本機自簽測試憑證**，不是 Developer ID／公證流程。換成正式發行簽署前，應一併調整簽署流程與說明。通過 PACE 驗證不代表已實測正式版 Pro Tools 載入。若重新執行 tag 的 CI 發佈工作，附件可能被 CI 原始產物覆蓋，需要再執行本機簽署。

## 將已簽署 AAX 重新放入 DMG

`sign-release-aax.sh` 只更新 AAX ZIP；DMG 使用獨立的 `package-dmg.py`。準備相同版本、revision 與 Universal 架構的 AU／VST3／已簽署 AAX ZIP，核對來源雜湊，並在本機保留每份 ZIP 的 `.zip.sha256` 檔案後執行：

```bash
GITHUB_SHA=<原始完整commit> python3 scripts/package-dmg.py \
  --archive-dir /absolute/path/to/archives \
  --output-dir /absolute/path/to/output --arch universal

python3 scripts/verify-dmg.py /absolute/path/to/output/HT-76-版本-commit-macos-universal-Installer.dmg
```

執行前確認目前 checkout 的 `CMakeLists.txt` 版本與 ZIP 相同。`.zip.sha256` 的內容格式為 `SHA256值  ZIP檔名`；這些檔案只用於本機驗證，不需上傳 Release。

打包程式會依 AAX 的 `build-info.json` 辨識已簽署狀態，呼叫 wraptool 驗證，再保留簽章封裝。`verify-dmg.py` 會掛載 DMG、展開 PKG，重新驗證其中的 AAX，並檢查三種格式預設全選、安裝路徑與解除安裝工具。兩支程式都支援 `--wraptool` 或 `PACE_WRAPTOOL`；PKG／DMG 本身不會因此取得 Developer ID 簽署或公證。

完成後另行替換 Release 的同名 DMG 並更新簽署說明；ZIP 簽署腳本不會自動做這一步。

## 離線測試

```bash
bash -n scripts/sign-release-aax.sh
python3 -m unittest discover -s Tests -p 'test_sign_release_aax.py' -v
```

測試不會呼叫 GitHub 或 PACE，也不會替換 Release。
