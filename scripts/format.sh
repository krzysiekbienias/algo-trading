#!/usr/bin/env bash
# ============================================================================
# algo-trading — clang-format on C/C++ sources
#
# Prefers git-tracked paths; falls back to scanning header/, src/, app/,
# unit_tests/ when nothing is tracked yet.
#
# Usage:
#   scripts/format.sh             # rewrite files in place
#   scripts/format.sh --check     # exit non-zero if any file is not formatted
# ============================================================================
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}"

if ! command -v clang-format >/dev/null 2>&1; then
    echo "[format] ERROR: clang-format not found." >&2
    echo "         Install with: brew install llvm" >&2
    exit 1
fi

FILES=()
while IFS= read -r f; do
    [[ -f "$f" ]] && FILES+=("$f")
done < <(git ls-files '*.hpp' '*.h' '*.cpp' '*.cxx' '*.cc' 2>/dev/null || true)

if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "[format] git ls-files empty — scanning header/ src/ app/ unit_tests/"
    while IFS= read -r f; do
        FILES+=("$f")
    done < <(find header src app unit_tests \
        -type f \( -name '*.hpp' -o -name '*.h' -o -name '*.cpp' -o -name '*.cxx' -o -name '*.cc' \) \
        2>/dev/null | LC_ALL=C sort)
fi

if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "[format] No source files found."
    exit 0
fi

if [[ "${1:-}" == "--check" ]]; then
    echo "[format] Checking ${#FILES[@]} files (no changes will be made)..."
    if ! clang-format --dry-run --Werror "${FILES[@]}"; then
        echo "[format] FAILED: some files are not formatted. Run scripts/format.sh to fix." >&2
        exit 1
    fi
    echo "[format] All files clean."
else
    echo "[format] Formatting ${#FILES[@]} files in place..."
    clang-format -i "${FILES[@]}"
    echo "[format] Done."
fi
