from __future__ import annotations

from pathlib import Path

import pandas as pd
import pyarrow as pa
import pyarrow.parquet as pq
import pytest

from quant_sandbox.loader import load_bars
from quant_sandbox.paths import canonical_symbol, find_repo_root, monthly_shard_paths
from quant_sandbox.schema import validate_ohlcv_frame


def test_canonical_symbol() -> None:
    assert canonical_symbol("eur.usd") == "EURUSD"
    assert canonical_symbol("NVDA") == "NVDA"


def test_find_repo_root_from_quant_sandbox() -> None:
    root = find_repo_root(Path(__file__).resolve().parent.parent)
    assert (root / "CMakeLists.txt").is_file()
    assert (root / "quant_sandbox" / "pyproject.toml").is_file()


def _write_shard(path: Path, timestamps_ms: list[int]) -> None:
    n = len(timestamps_ms)
    table = pa.table(
        {
            "timestamp": timestamps_ms,
            "open": [100.0] * n,
            "high": [101.0] * n,
            "low": [99.0] * n,
            "close": [100.5] * n,
            "volume": [1000.0] * n,
        }
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    pq.write_table(table, path)


def test_load_bars_merges_and_dedupes(tmp_path: Path) -> None:
    lake = tmp_path / "data_lake_test"
    shard_a = lake / "TEST" / "TEST_2024_01.parquet"
    shard_b = lake / "TEST" / "TEST_2024_02.parquet"

    _write_shard(shard_a, [1_704_067_200_000, 1_704_153_600_000])  # 2024-01-01, 2024-01-02 UTC
    _write_shard(shard_b, [1_704_153_600_000, 1_704_240_000_000])  # duplicate + 2024-01-03

    df = load_bars("TEST", lake_root=lake)
    assert len(df) == 3
    assert df.index.tz is not None
    assert float(df.iloc[-1]["close"]) == pytest.approx(100.5)


def test_monthly_shard_paths_empty_when_missing(tmp_path: Path) -> None:
    assert monthly_shard_paths(tmp_path, "MISSING") == []


def test_validate_ohlcv_frame_rejects_missing_column() -> None:
    df = pd.DataFrame({"timestamp": [1], "open": [1.0]})
    with pytest.raises(ValueError, match="missing OHLCV"):
        validate_ohlcv_frame(df)
