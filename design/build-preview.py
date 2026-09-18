"""Build the standalone panel study from the editable inline preview."""
from pathlib import Path
import base64
import re

root = Path(__file__).resolve().parent
fragment = (root / "panel-preview.html").read_text()
skin = (root / "panel-skin.css").read_text().split("@media(max-width:950px)")[0] + "\n" + (root / "panel-reference.css").read_text()
for placeholder, filename, mime in [
    ("__PAPER_TEXTURE__", "concept-vu-paper.jpg", "image/jpeg"),
    ("__KNOB_TEXTURE__", "concept-knob-body.png", "image/png"),
    ("__METAL_TEXTURE__", "concept-metal.jpg", "image/jpeg"),
]:
    encoded = base64.b64encode((root / "assets" / filename).read_bytes()).decode()
    skin = skin.replace(placeholder, f"data:{mime};base64,{encoded}")
fragment = re.sub(r"<style>.*?</style>", lambda _: "<style>\n" + skin + "</style>", fragment, count=1, flags=re.S)
(root / "panel-preview.html").write_text(fragment)
head = '''<!doctype html>
<html lang="zh-Hant"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>HT-76 — Panel design preview</title>
<style>
body{margin:0;background:#050708;padding:48px 32px;color:#cbd6dc}
main{max-width:1280px;margin:0 auto}
.study-label{font:10px -apple-system,sans-serif;letter-spacing:3px;color:#9eadb7;margin:0 0 18px;display:flex;justify-content:space-between}
@media(max-width:620px){body{padding:22px 12px}.study-label{font-size:9px;letter-spacing:1.5px}}
</style></head><body><main><div class="study-label"><span>HT-76 / INTERFACE STUDY</span><span>06 · CONTROL REFINEMENT</span></div>
'''
(root / "index.html").write_text(head + fragment + "\n</main></body></html>\n")
