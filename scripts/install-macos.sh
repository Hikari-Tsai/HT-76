#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
artifacts="$project_dir/build/FieldEffect_artefacts/Release"
for kind in AU VST3; do
    if [[ "$kind" == AU ]]; then extension=component; folder=Components; else extension=vst3; folder=VST3; fi
    source_bundle="$artifacts/$kind/HT-76.$extension"
    destination="$HOME/Library/Audio/Plug-Ins/$folder/HT-76.$extension"
    [[ -d "$source_bundle" ]] || { echo "Build first: $source_bundle" >&2; exit 1; }
    mkdir -p "$(dirname "$destination")"
    if [[ -e "$destination" ]]; then
        backup="$project_dir/output/previous-install/$(date +%Y%m%d-%H%M%S)/$folder"
        mkdir -p "$backup"
        mv "$destination" "$backup/"
    fi
    ditto "$source_bundle" "$destination"
    # Archive the previous product filename when it has the same host identity.
    legacy="$HOME/Library/Audio/Plug-Ins/$folder/1176 Field Effect.$extension"
    if [[ -d "$legacy" ]] && [[ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$legacy/Contents/Info.plist" 2>/dev/null)" == "com.hikaritsai.fieldeffect1176" ]]; then
        backup="$project_dir/output/previous-install/$(date +%Y%m%d-%H%M%S)/$folder"
        mkdir -p "$backup"
        mv "$legacy" "$backup/"
    fi
    echo "Installed: $destination"
done
