#!/usr/bin/env bash
# Public entry point; Python's standard library handles JSON and ZIP validation.
set -euo pipefail
script_dir="$(cd "$(dirname "$0")" && pwd)"
exec python3 "$script_dir/sign-release-aax.py" "$@"
