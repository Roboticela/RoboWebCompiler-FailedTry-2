#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# RoboHTMLCompiler build driver for POSIX hosts (Linux/macOS/MinGW).
#
# Usage:
#   Tools/Build.sh [Debug|Release|RelWithDebInfo] [clean]
# ---------------------------------------------------------------------------
set -euo pipefail

RoboBuildType="${1:-Release}"
RoboAction="${2:-}"

RoboRoot="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RoboBuildDirectory="${RoboRoot}/build"

if [ "${RoboAction}" = "clean" ]; then
    rm -rf "${RoboBuildDirectory}"
fi

RoboGeneratorArgs=()
if command -v ninja >/dev/null 2>&1; then
    RoboGeneratorArgs+=(-G Ninja)
fi

echo "[RoboHTMLCompiler] configuring ${RoboBuildType}"
cmake -S "${RoboRoot}" -B "${RoboBuildDirectory}" \
    "${RoboGeneratorArgs[@]}" \
    -DCMAKE_BUILD_TYPE="${RoboBuildType}"

echo "[RoboHTMLCompiler] building"
cmake --build "${RoboBuildDirectory}" --config "${RoboBuildType}"

echo "[RoboHTMLCompiler] build complete: ${RoboBuildDirectory}/bin"
