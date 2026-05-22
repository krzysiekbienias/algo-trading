# quant_sandbox

Python research environment for exploring OHLCV data produced by the C++ IBKR harvester.
Use Jupyter Lab to prototype indicators and signals before porting logic to the C++ backtester.

**Data contract:** [docs/quant_sandbox.md](../docs/quant_sandbox.md)

## Setup

From the monorepo root:

```bash
./scripts/setup_quant_sandbox.sh
```

This creates `quant_sandbox/.venv`, installs the package in editable mode, and registers
the Jupyter kernel **`algo-trading-quant`** inside the venv (no writes to `~/Library`).

Optional config:

```bash
cp quant_sandbox/.env.example quant_sandbox/.env
# QUANT_DATA_LAKE=data_lake_m1
```

## Jupyter Lab

```bash
./scripts/run_jupyter.sh
```

Or manually:

```bash
cd quant_sandbox
source .venv/bin/activate
jupyter lab --notebook-dir=notebooks
```

Select kernel **algo-trading-quant** in notebooks.

## Loader (from Python or notebooks)

```python
from quant_sandbox import load_bars

df = load_bars("NVDA", start="2024-05-01", end="2026-05-01")
# index: timestamp (UTC); columns: open, high, low, close, volume
print(len(df), df.index.min(), df.index.max())
```

Environment variable `QUANT_DATA_LAKE` (default `data_lake_m1`) sets the Parquet root when
`lake_root` is omitted. Use separate roots for different bar sizes (e.g. `data_lake` for H1).

## Tests

```bash
cd quant_sandbox
source .venv/bin/activate
pytest
```

## Layout

```
quant_sandbox/
├── pyproject.toml
├── src/quant_sandbox/    # importable package (paths, loader, schema)
├── notebooks/            # numbered Jupyter notebooks
└── tests/
```

Reusable logic belongs in `src/quant_sandbox/`; notebooks should orchestrate and plot only.
