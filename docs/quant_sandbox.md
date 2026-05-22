# quant_sandbox — Python research on the data lake

Isolated Python/Jupyter layer for exploring Parquet OHLCV shards written by the C++ harvester.
Not part of the CMake build.

## Data contract

Same layout as [harvest.md](harvest.md):

```
{lake_root}/{SYMBOL}/{SYMBOL}_{YYYY}_{MM}.parquet
```

| Field | Type | Notes |
|-------|------|--------|
| `timestamp` | int64 ms | UTC epoch milliseconds |
| `open`, `high`, `low`, `close`, `volume` | float64 | real prices / size |

**Timezone policy:** all timestamps are UTC. Convert to local time only for display.

**Bar size policy:** do not mix resolutions in one lake root (`data_lake` vs `data_lake_m1`).

## Environment

| Variable | Default | Purpose |
|----------|---------|---------|
| `QUANT_DATA_LAKE` | `data_lake_m1` | Default root for `load_bars()` |

Set in `quant_sandbox/.env` (gitignored) or export in the shell.

## Setup

```bash
./scripts/setup_quant_sandbox.sh
./scripts/run_jupyter.sh
```

Kernel name: **`algo-trading-quant`**.

## API sketch

```python
from quant_sandbox import load_bars, canonical_symbol, find_repo_root

df = load_bars("NVDA", start="2024-05-01", lake_root="data_lake_m1")
```

## Notebook workflow

1. `00_setup_smoke` — verify imports and one symbol load
2. `01_explore_*` — stats, gaps, charts
3. `02_simple_signal` — prototype signals (no PnL engine yet)

Extract reusable code into `quant_sandbox/src/quant_sandbox/` before porting to C++.
