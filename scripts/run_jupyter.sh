#!/usr/bin/env bash
# ============================================================================
# Start Jupyter Lab for quant_sandbox (requires setup_quant_sandbox.sh first)
#
# Usage:
#   scripts/run_jupyter.sh
# ============================================================================
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SANDBOX="${REPO_ROOT}/quant_sandbox"
VENV="${SANDBOX}/.venv"

if [[ ! -d "${VENV}" ]]; then
    echo "[jupyter] ERROR: venv missing. Run: scripts/setup_quant_sandbox.sh" >&2
    exit 1
fi

# shellcheck disable=SC1091
source "${VENV}/bin/activate"

cd "${SANDBOX}"
exec jupyter lab --notebook-dir=notebooks
