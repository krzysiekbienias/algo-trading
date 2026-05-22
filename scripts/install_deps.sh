#!/usr/bin/env bash
# Install all build/runtime dependencies via Homebrew (macOS).
# Idempotent: safe to re-run; brew install is a no-op if already present.
set -euo pipefail

if ! command -v brew >/dev/null 2>&1; then
    echo "ERROR: Homebrew not found. Install it first: https://brew.sh" >&2
    exit 1
fi

PACKAGES=(
    cmake               # build system (>=3.20 required)
    ninja               # fast build backend for scripts/build.sh
    llvm                # clang-format (scripts/format.sh)
    pkg-config          # transitive use by Arrow et al.
    openssl@3           # system TLS (TWS API / tooling)
    nlohmann-json       # JSON parser (header-only)
    apache-arrow        # Provides Arrow + Parquet C++
    spdlog              # Logging
    fmt                 # spdlog dep + general formatting
    googletest          # Unit tests
)
echo "Installing dependencies via Homebrew..."
for pkg in "${PACKAGES[@]}"; do
    if brew list --versions "$pkg" >/dev/null 2>&1; then
        echo "  [skip] $pkg already installed: $(brew list --versions "$pkg")"
    else
        echo "  [install] $pkg"
        brew install "$pkg"
    fi
done

echo
echo "Done. Versions installed:"
brew list --versions "${PACKAGES[@]}"
