#!/usr/bin/env bash
# Capture the real JUCE editor with synthetic audio, then encode silent loops.
# Usage: bash scripts/render-readme-gifs.sh [path/to/FieldEffectTests]
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
test_binary="${1:-$project_dir/build/FieldEffectTests_artefacts/Release/FieldEffectTests}"
if [[ ! -x "$test_binary" ]]; then
    echo "Build FieldEffectTests first, or pass its executable path." >&2
    exit 1
fi
command -v ffmpeg >/dev/null
frames_dir="$(mktemp -d "${TMPDIR:-/tmp}/ht76-readme-frames.XXXXXX")"
trap 'rm -rf "$frames_dir"' EXIT

"$test_binary" --render-animation "$frames_dir"
for view in rack dynamic; do
    ffmpeg -hide_banner -loglevel warning -y \
        -framerate 20 -i "$frames_dir/$view-revd/frame-%04d.png" \
        -filter_complex '[0:v]scale=960:-1:flags=lanczos,split[frames][palette_source];[palette_source]palettegen=stats_mode=diff[palette];[frames][palette]paletteuse=dither=bayer:bayer_scale=3:diff_mode=rectangle' \
        -loop 0 "$frames_dir/$view.gif"
    if [[ "$view" == dynamic ]]; then
        # A scrolling graph changes far more pixels than the Rack meters.
        # Use 100 frames and a smaller palette to keep the README download small.
        ffmpeg -hide_banner -loglevel warning -y -i "$frames_dir/$view.gif" \
            -filter_complex '[0:v]fps=12.5,split[frames][palette_source];[palette_source]palettegen=max_colors=128:stats_mode=diff[palette];[frames][palette]paletteuse=dither=bayer:bayer_scale=4:diff_mode=rectangle' \
            -loop 0 "$project_dir/docs/images/$view-revd.gif"
    else
        mv "$frames_dir/$view.gif" "$project_dir/docs/images/$view-revd.gif"
    fi
done
