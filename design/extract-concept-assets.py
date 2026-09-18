"""Extract the knob and metal finish from our generated concept board.

The reference pointer is removed with a feathered 180-degree radial copy.
The interactive pointer is drawn separately, so lighting stays fixed when turned.
Run with: uv run --with pillow python design/extract-concept-assets.py
"""
from pathlib import Path
from PIL import Image, ImageOps, ImageFilter
import math

root = Path(__file__).resolve().parent
source = Image.open(root.parent / "output/imagegen/1176-dual-panel-concept.png").convert("RGB")
assets = root / "assets"
assets.mkdir(exist_ok=True)
cx, cy, radius = 338, 286, 76
size = 166
knob = Image.new("RGBA", (size, size))
for y in range(size):
    for x in range(size):
        dx, dy = x - size / 2, y - size / 2
        r = math.hypot(dx, dy)
        angle = math.degrees(math.atan2(dy, dx))
        original = source.getpixel((round(cx + dx), round(cy + dy)))
        opposite = source.getpixel((round(cx - dx), round(cy - dy)))
        wedge = max(0, min(1, (angle + 150) / 8, (-61 - angle) / 8))
        # Keep the front-lit rim, instead of copying the dark underside shadow.
        if r > 62 and -150 < angle < -61:
            endpoints = []
            for degrees in (-153, -58):
                a = math.radians(degrees)
                endpoints.append(source.getpixel((round(cx + r * math.cos(a)), round(cy + r * math.sin(a)))))
            blend = max(0, min(1, (angle + 153) / 95))
            rim = tuple(round(a * (1 - blend) + b * blend) for a, b in zip(*endpoints))
            rim_weight = min(1, (r - 62) / 5)
            opposite = tuple(round(a * (1 - rim_weight) + b * rim_weight) for a, b in zip(opposite, rim))
        rgb = tuple(round(a * (1 - wedge) + b * wedge) for a, b in zip(original, opposite))
        alpha = round(255 * max(0, min(1, radius + .5 - r)))
        knob.putpixel((x, y), (*rgb, alpha))
knob.save(assets / "concept-knob-body.png")
tile = source.crop((470, 165, 580, 235))
seamless = Image.new("RGB", (tile.width * 2, tile.height * 2))
seamless.paste(tile, (0, 0))
seamless.paste(ImageOps.mirror(tile), (tile.width, 0))
seamless.paste(ImageOps.flip(tile), (0, tile.height))
seamless.paste(ImageOps.flip(ImageOps.mirror(tile)), (tile.width, tile.height))
seamless.save(assets / "concept-metal.jpg", quality=88)
# Cool the same brushed texture while retaining its luminance detail.
blue = seamless.point(
    [int(i * .80) for i in range(256)]
    + [int(i * .96) for i in range(256)]
    + [min(255, int(i * 1.12 + 3)) for i in range(256)]
)
blue.save(assets / "concept-metal-blue.jpg", quality=88)
# Blank paper from the VU face, with no labels or needle baked in.
paper = source.crop((1446, 300, 1478, 326))
# Remove the reference's broad lighting gradient before tiling; retain paper grain.
blurred = paper.filter(ImageFilter.GaussianBlur(3))
for y in range(paper.height):
    for x in range(paper.width):
        detail = tuple(a - b for a, b in zip(paper.getpixel((x, y)), blurred.getpixel((x, y))))
        paper.putpixel((x, y), tuple(max(0, min(255, base + d)) for base, d in zip((204, 177, 133), detail)))
vu_tile = Image.new("RGB", (paper.width * 2, paper.height * 2))
vu_tile.paste(paper, (0, 0))
vu_tile.paste(ImageOps.mirror(paper), (paper.width, 0))
vu_tile.paste(ImageOps.flip(paper), (0, paper.height))
vu_tile.paste(ImageOps.flip(ImageOps.mirror(paper)), (paper.width, paper.height))
vu_tile.save(assets / "concept-vu-paper.jpg", quality=92)
print("Extracted knob, metal, and VU paper textures")
