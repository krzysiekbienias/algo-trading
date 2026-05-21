#!/usr/bin/env bash
# ============================================================================
# algo-trading — remove build artifacts
#
# Usage:
#   scripts/clean.sh              # remove ./build/
#   BUILD_DIR=out scripts/clean.sh
#   scripts/clean.sh --deep       # also remove ./cache/ and ./data/ (Parquet)
# ============================================================================
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build}"
DEEP=0

for arg in "$@"; do
    case "${arg}" in
        --deep) DEEP=1 ;;
        -h|--help)
            sed -n '2,10p' "$0" | sed 's/^# \?//'
            exit 0
            ;;
        *)
            echo "[clean] Unknown option: ${arg}" >&2
            exit 1
            ;;
    esac
done

cd "${REPO_ROOT}"

if [[ -d "${BUILD_DIR}" ]]; then
    echo "[clean] Removing ${BUILD_DIR}/ ..."
    rm -rf "${BUILD_DIR}"
else
    echo "[clean] Nothing to do — ${BUILD_DIR}/ does not exist."
fi

if [[ "${DEEP}" == "1" ]]; then
    for dir in cache data; do
        if [[ -d "${dir}" ]]; then
            echo "[clean] Removing ${dir}/ ..."
            rm -rf "${dir}"
        fi
    done
fi

echo "[clean] Done."
