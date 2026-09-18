#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
tools_dir="$project_dir/build-juce-tools"
projucer_app="$tools_dir/extras/Projucer/Projucer_artefacts/Release/Projucer.app"

# Use the same JUCE version as the plugin when exporting Xcode projects.
if [[ ! -x "$projucer_app/Contents/MacOS/Projucer" ]]; then
    cmake -S "$project_dir/third_party/JUCE" -B "$tools_dir" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$(uname -m)" \
        -DJUCE_BUILD_EXTRAS=ON
    cmake --build "$tools_dir" --target Projucer --parallel "${FIELD_JOBS:-4}"
fi
open -n -a "$projucer_app" "$project_dir/HT-76.jucer"
