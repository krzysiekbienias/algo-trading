#!/usr/bin/env bash
# ============================================================================
# algo-trading — configure + build
#
# Usage:
#   scripts/build.sh                # Debug build into ./build/
#   scripts/build.sh Release        # Release build into ./build/
#   BUILD_DIR=out scripts/build.sh  # custom build dir
#
# Optional env:
#   ENABLE_DEV_MAIN=OFF   skip dev_main (faster iteration)
#   ENABLE_TESTS=OFF      skip unit tests target
#   ENABLE_APP=OFF        skip production app
#   RUN_TESTS=1           run ctest after build
# ============================================================================
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build}"
BUILD_TYPE="${1:-Debug}"

cd "${REPO_ROOT}"

if ! command -v ninja >/dev/null 2>&1; then
    echo "[build] ERROR: ninja not found. Install with: brew install ninja" >&2
    exit 1
fi

CMAKE_ARGS=(
    -S .
    -B "${BUILD_DIR}"
    -G Ninja
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    "-DENABLE_DEV_MAIN=${ENABLE_DEV_MAIN:-ON}"
    "-DENABLE_TESTS=${ENABLE_TESTS:-ON}"
    "-DENABLE_APP=${ENABLE_APP:-ON}"
)

echo "[build] Configuring ${BUILD_TYPE} build into ${BUILD_DIR}/ ..."
cmake "${CMAKE_ARGS[@]}"

echo "[build] Compiling..."
cmake --build "${BUILD_DIR}" -j

if [[ "${RUN_TESTS:-0}" == "1" ]]; then
    echo "[build] Running tests..."
    ctest --test-dir "${BUILD_DIR}" --output-on-failure
fi

echo "[build] Done. Binaries in ${BUILD_DIR}/"
echo "[build]   ./${BUILD_DIR}/app"
echo "[build]   ./${BUILD_DIR}/dev_main"
echo "[build]   ./${BUILD_DIR}/test_environment"
