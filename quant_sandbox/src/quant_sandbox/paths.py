"""Data lake path conventions (mirrors C++ at::storage data_lake_paths)."""

from __future__ import annotations

import os
from pathlib import Path


def find_repo_root(start: Path | None = None) -> Path:
    """Walk parents until the monorepo root (CMakeLists.txt + quant_sandbox/) is found."""
    here = (start or Path.cwd()).resolve()
    for path in (here, *here.parents):
        if (path / "CMakeLists.txt").is_file() and (path / "quant_sandbox" / "pyproject.toml").is_file():
            return path
        if (path / "pyproject.toml").is_file() and (path / "src" / "quant_sandbox").is_dir():
            repo = path.parent
            if (repo / "CMakeLists.txt").is_file():
                return repo
    raise FileNotFoundError("could not locate monorepo root (CMakeLists.txt + quant_sandbox/)")


def canonical_symbol(symbol: str) -> str:
    """TWS 'EUR.USD' → 'EURUSD'; same rules as C++ canonicalSymbol."""
    out = "".join(ch for ch in symbol if ch != ".")
    out = out.upper()
    if not out:
        raise ValueError("empty symbol")
    return out


def resolve_lake_root(lake_root: str | Path | None = None) -> Path:
    """Return absolute path to a data lake root directory."""
    repo = find_repo_root()
    raw = lake_root if lake_root is not None else os.environ.get("QUANT_DATA_LAKE", "data_lake_m1")
    path = Path(raw)
    if not path.is_absolute():
        path = repo / path
    return path.resolve()


def symbol_dir(lake_root: Path, symbol: str) -> Path:
    canon = canonical_symbol(symbol)
    return lake_root / canon


def monthly_shard_paths(lake_root: Path, symbol: str) -> list[Path]:
    """Sorted list of monthly Parquet files for one instrument."""
    canon = canonical_symbol(symbol)
    directory = symbol_dir(lake_root, symbol)
    if not directory.is_dir():
        return []
    return sorted(directory.glob(f"{canon}_*.parquet"))
