"""Parquet OHLCV schema shared with the C++ harvester."""

from __future__ import annotations

import pandas as pd

OHLCV_COLUMNS: tuple[str, ...] = (
    "timestamp",
    "open",
    "high",
    "low",
    "close",
    "volume",
)

PRICE_COLUMNS: tuple[str, ...] = ("open", "high", "low", "close", "volume")


def validate_ohlcv_frame(df: pd.DataFrame) -> None:
    """Raise ValueError if required columns or dtypes look wrong."""
    missing = [c for c in OHLCV_COLUMNS if c not in df.columns]
    if missing:
        raise ValueError(f"missing OHLCV columns: {missing}")

    for col in PRICE_COLUMNS:
        if not pd.api.types.is_numeric_dtype(df[col]):
            raise ValueError(f"column {col!r} must be numeric")
