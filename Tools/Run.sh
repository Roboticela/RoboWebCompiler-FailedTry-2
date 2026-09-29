#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# RoboHTMLCompiler run driver for POSIX hosts (Linux/macOS/MinGW).
#
# Compiles every HTML document in an input folder into its own strongly typed
# C++ native model, keeping the generated browser output and IR alongside it:
#
#     <output>/<DocumentName>/GeneratedHTML/<document>.html
#     <output>/<DocumentName>/GeneratedNative/<DocumentName>NativeModel.{h,cpp}
#     <output>/<DocumentName>/GeneratedIR/<DocumentName>.ir.json
#     <output>/<DocumentName>/GeneratedReports/
#     <output>/BatchReport.json
#
# Usage:
#   Tools/Run.sh <inputDirectory> [outputDirectory] [compiler options...]
#
# Examples:
#   Tools/Run.sh Examples/MultiPage
#   Tools/Run.sh Examples/MultiPage build/compiled/MultiPage
#   Tools/Run.sh Examples/BasicApp build/out --target native
# ---------------------------------------------------------------------------
set -euo pipefail

usage() {
    cat <<'EOF'
RoboHTMLCompiler run driver

Usage:
    Tools/Run.sh <inputDirectory> [outputDirectory] [compiler options...]

Compiles every .html document in the input directory into native C++,
one subdirectory per document, plus a BatchReport.json summary.

Example:
    Tools/Run.sh Examples/MultiPage
EOF
}

RoboInput="${1:-}"
if [ -z "${RoboInput}" ]; then
    usage
    exit 2
fi

RoboOutput="${2:-}"
if [ "$#" -ge 2 ]; then
    shift 2
else
    shift "$#"
fi
RoboExtra=("$@")

RoboRoot="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RoboBuildDirectory="${RoboRoot}/build"

RoboExe="${RoboBuildDirectory}/bin/RoboHTMLCompiler"
if [ ! -x "${RoboExe}" ]; then
    RoboExe="${RoboBuildDirectory}/bin/RoboHTMLCompiler.exe"
fi

if [ ! -d "${RoboInput}" ]; then
    echo "[RoboHTMLCompiler] input directory not found: ${RoboInput}" >&2
    exit 1
fi

if [ ! -x "${RoboExe}" ]; then
    echo "[RoboHTMLCompiler] the compiler is not built yet; building it first"
    "${RoboRoot}/Tools/Build.sh" Release
fi

if [ -z "${RoboOutput}" ]; then
    RoboLeaf="$(basename "$(cd "${RoboInput}" && pwd)")"
    RoboOutput="${RoboBuildDirectory}/compiled/${RoboLeaf}"
fi

echo "[RoboHTMLCompiler] input:  ${RoboInput}"
echo "[RoboHTMLCompiler] output: ${RoboOutput}"
echo

"${RoboExe}" "${RoboInput}" --output "${RoboOutput}" --all ${RoboExtra[@]+"${RoboExtra[@]}"}

echo
echo "[RoboHTMLCompiler] done; open ${RoboOutput} to inspect the generated C++."
