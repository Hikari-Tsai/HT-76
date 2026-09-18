#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
cmake -S "$project_dir" -B "$project_dir/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="${FIELD_ARCHS:-arm64;x86_64}"
cmake --build "$project_dir/build" --parallel "${FIELD_JOBS:-4}"
ctest --test-dir "$project_dir/build" --output-on-failure
