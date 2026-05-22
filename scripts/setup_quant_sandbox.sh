#!/usr/bin/env bash
# ============================================================================
# quant_sandbox — create venv, install package, register Jupyter kernel
#
# Usage:
#   scripts/setup_quant_sandbox.sh
# ============================================================================
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SANDBOX="${REPO_ROOT}/quant_sandbox"
VENV="${SANDBOX}/.venv"
KERNEL_NAME="algo-trading-quant"

cd "${SANDBOX}"

if ! command -v python3 >/dev/null 2>&1; then
    echo "[quant_sandbox] ERROR: python3 not found (need 3.11+)" >&2
    exit 1
fi

PY_VER="$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')"
echo "[quant_sandbox] Using python3 (${PY_VER})"

if [[ ! -d "${VENV}" ]]; then
    echo "[quant_sandbox] Creating venv at quant_sandbox/.venv"
    python3 -m venv "${VENV}"
fi

# shellcheck disable=SC1091
source "${VENV}/bin/activate"

python -m pip install --upgrade pip
python -m pip install -e ".[dev]"

echo "[quant_sandbox] Registering Jupyter kernel: ${KERNEL_NAME} (inside .venv)"
python -m ipykernel install --prefix="${VENV}" --name="${KERNEL_NAME}" \
    --display-name="Algo Trading (quant_sandbox)"

python -c "from quant_sandbox import load_bars, find_repo_root; print('[quant_sandbox] import OK, repo:', find_repo_root())"

echo "[quant_sandbox] Done."
echo "[quant_sandbox]   source quant_sandbox/.venv/bin/activate"
echo "[quant_sandbox]   ./scripts/run_jupyter.sh"
