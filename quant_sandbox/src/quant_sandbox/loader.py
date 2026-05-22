"""Load OHLCV bars from monthly Parquet shards."""

from __future__ import annotations

from datetime import datetime
from pathlib import Path
from typing import Union

import pandas as pd

from quant_sandbox.paths import monthly_shard_paths, resolve_lake_root
from quant_sandbox.schema import validate_ohlcv_frame

DateLike = Union[str, datetime, pd.Timestamp]


def _to_utc_timestamp(value: DateLike) -> pd.Timestamp:
    ts = pd.Timestamp(value)
    if ts.tzinfo is None:
        return ts.tz_localize("UTC")
    return ts.tz_convert("UTC")


def load_bars(
    symbol: str,
    *,
    start: DateLike | None = None,
    end: DateLike | None = None,
    lake_root: str | Path | None = None,
    timestamp_index: bool = True,
) -> pd.DataFrame:
    """Load and merge monthly shards for one symbol.

    Returns a DataFrame sorted by timestamp (UTC). Duplicate timestamps keep the
    last row (same contract as C++ ParquetWriter::append).
    """
    root = resolve_lake_root(lake_root)
    paths = monthly_shard_paths(root, symbol)
    if not paths:
        raise FileNotFoundError(
            f"no Parquet shards for {symbol!r} under {root} "
            f"(run C++ harvest first or check QUANT_DATA_LAKE)"
        )

    frames = [pd.read_parquet(path) for path in paths]
    df = pd.concat(frames, ignore_index=True)
    validate_ohlcv_frame(df)

    df["timestamp"] = pd.to_datetime(df["timestamp"], unit="ms", utc=True)
    df = df.sort_values("timestamp")
    df = df.drop_duplicates(subset=["timestamp"], keep="last")

    if start is not None:
        df = df[df["timestamp"] >= _to_utc_timestamp(start)]
    if end is not None:
        df = df[df["timestamp"] <= _to_utc_timestamp(end)]

    df = df.reset_index(drop=True)

    if timestamp_index:
        return df.set_index("timestamp")
    return df
