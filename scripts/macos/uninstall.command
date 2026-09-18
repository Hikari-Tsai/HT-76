#!/bin/bash
# HT-76 uninstaller. Apache-2.0; see the accompanying LICENSE.
set -euo pipefail

bundle_matches() {
    local bundle="$1" parent="$1" identifier
    [[ -d "$bundle" && -f "$bundle/Contents/Info.plist" ]] || return 1
    # Refuse redirected paths, including a symlink in any parent directory.
    while [[ "$parent" != / ]]; do
        [[ ! -L "$parent" ]] || return 1
        parent="$(/usr/bin/dirname "$parent")"
    done
    [[ ! -L "$bundle/Contents" && ! -L "$bundle/Contents/Info.plist" ]] || return 1
    identifier="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$bundle/Contents/Info.plist" 2>/dev/null)" || return 1
    [[ "$identifier" == com.hikaritsai.fieldeffect1176 ]]
}

main() {
    local dry_run=false format folder extension base name candidate answer receipt
    local -a targets=() receipts=()
    case "${1:-}" in
        --dry-run) dry_run=true ;;
        '') ;;
        *) echo 'Usage: uninstall.command [--dry-run]' >&2; return 2 ;;
    esac
    [[ $# -le 1 ]] || return 2
    if [[ $EUID -eq 0 ]]; then
        echo 'Run as your normal macOS user, without sudo. Administrator access is requested only when needed.' >&2
        return 1
    fi
    [[ "$HOME" == /* && "$HOME" != / ]] || return 1
    for format in AU VST3 AAX; do
        case "$format" in
            AU) folder='Audio/Plug-Ins/Components'; extension=component ;;
            VST3) folder='Audio/Plug-Ins/VST3'; extension=vst3 ;;
            AAX) folder='Application Support/Avid/Audio/Plug-Ins'; extension=aaxplugin ;;
        esac
        for base in /Library "$HOME/Library"; do
            for name in HT-76 '1176 Field Effect'; do
                candidate="$base/$folder/$name.$extension"
                if [[ -e "$candidate" || -L "$candidate" ]]; then
                    if bundle_matches "$candidate"; then
                        targets+=("$candidate")
                    else
                        echo "Skipped (identity mismatch or redirected path): $candidate" >&2
                    fi
                fi
            done
        done
        receipt="com.hikaritsai.ht76.pkg.$(echo "$format" | /usr/bin/tr '[:upper:]' '[:lower:]')"
        if /usr/sbin/pkgutil --pkg-info "$receipt" >/dev/null 2>&1; then
            receipts+=("$receipt")
        fi
    done
    if [[ ${#targets[@]} -eq 0 && ${#receipts[@]} -eq 0 ]]; then
        echo 'No matching HT-76 installation found.'
        return 0
    fi
    echo 'Close your DAW before continuing. The following items will be removed:'
    if [[ ${#targets[@]} -gt 0 ]]; then printf '  %s\n' "${targets[@]}"; fi
    if [[ ${#receipts[@]} -gt 0 ]]; then printf '  Installer receipt: %s\n' "${receipts[@]}"; fi
    echo 'Presets, DAW sessions, backups and other plugins are preserved.'
    if $dry_run; then return 0; fi
    printf 'Type UNINSTALL to confirm (anything else cancels): '
    if ! IFS= read -r answer || [[ "$answer" != UNINSTALL ]]; then
        echo 'Cancelled.'
        return 0
    fi
    if [[ ${#targets[@]} -gt 0 ]]; then
        for candidate in "${targets[@]}"; do
            # Recheck identity immediately before removing this exact bundle.
            if ! bundle_matches "$candidate"; then
                echo "Installation changed; stopped before removing: $candidate" >&2
                return 1
            fi
            case "$candidate" in
                /Library/*) /usr/bin/sudo /bin/rm -rf -- "$candidate" ;;
                "$HOME/Library/"*) /bin/rm -rf -- "$candidate" ;;
                *) return 1 ;;
            esac
            [[ ! -e "$candidate" && ! -L "$candidate" ]] || return 1
            echo "Removed: $candidate"
        done
    fi
    if [[ ${#receipts[@]} -gt 0 ]]; then
        for receipt in "${receipts[@]}"; do
            /usr/bin/sudo /usr/sbin/pkgutil --forget "$receipt"
        done
    fi
    echo 'HT-76 uninstall complete. Reopen your DAW and rescan plugins if necessary.'
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    main "$@"
fi
