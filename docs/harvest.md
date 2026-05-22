# IBKR historical harvest → data lake

Backfill OHLCV bars from TWS into monthly Parquet shards via `dev_main --harvest`.
Requires TWS or IB Gateway with the socket API enabled.

## Data lake layout

```
{data_lake}/{SYMBOL}/{SYMBOL}_{YYYY}_{MM}.parquet
```

Examples:

- `data_lake/NVDA/NVDA_2026_04.parquet` — US stock, hourly bars
- `data_lake/EURUSD/EURUSD_2026_05.parquet` — FX pair (`EUR.USD` → folder `EURUSD`)
- `data_lake_m1/NVDA/NVDA_2026_05.parquet` — same symbol, **1 min** bars in a separate root

Parquet schema: `timestamp[ms,UTC]`, `open`, `high`, `low`, `close`, `volume`.

Monthly shards use the **UTC calendar month** of each bar's timestamp.

### Bar size vs directory

Use **different `--data-lake` roots** for different bar sizes (e.g. `data_lake` for H1,
`data_lake_m1` for M1). The harvester merges by timestamp only; mixing resolutions in one
shard breaks downstream backtests.

Both `data_lake/` and `data_lake_m1/` are gitignored (regenerate from TWS).

## Asset classes

| `--asset` | IB contract | Defaults |
|-----------|-------------|----------|
| `stock` (default) | `STK` + `SMART` + `USD` | `what_to_show=TRADES`, `use_rth=1` |
| `fx` | `CASH` + `IDEALPRO` | `what_to_show=MIDPOINT`, `use_rth=0` |

FX symbols: `EUR.USD` or `EURUSD` (base + quote).

GPW / WSE (`CDR`, etc.) is not wired yet — needs `exchange=WSE`, `currency=PLN` (see
roadmap).

## CLI

```bash
./build/dev_main --help

# Connectivity
./build/dev_main --ibkr-check

# Smoke: 1 day of 1-hour bars (no Parquet)
./build/dev_main --harvest-test NVDA
./build/dev_main --harvest-test --asset fx EUR.USD

# Backfill into monthly Parquet
./build/dev_main --harvest --symbol NVDA --from 2026-04-01
./build/dev_main --harvest --asset fx --symbol EUR.USD --from 2026-05-01
```

### Harvest options

| Flag | Meaning |
|------|---------|
| `--symbol` | Required. Stock ticker or FX pair. |
| `--from DATE` | Required. ISO date (e.g. `2024-05-01`). Backfill stops when the oldest bar in a chunk is on or before this instant (UTC midnight). |
| `--asset stock\|fx` | Instrument type. Default: `stock`. |
| `--bar-size SIZE` | IB bar size. Default: `"1 hour"`. Use `"1 min"` for minute bars. |
| `--chunk DURATION` | IB duration per request. Default: `"2 W"`. Use `"1 W"` or `"1 D"` for `"1 min"`. |
| `--data-lake PATH` | Output root. Default: `data_lake`. |

Environment (see `.env.example`):

- `IBKR_HOST`, `IBKR_PORT`, `IBKR_CLIENT_ID`
- `HARVEST_SLEEP_MS` — pause between chunks (default `12000`)
- `HARVEST_PACING_SLEEP_MS` — sleep after IB error 162 (default `60000`)

## How backfill works

1. **Direction:** from **now** (or resume point) **backward** to `--from`.
2. **Chunks:** each `reqHistoricalData` asks for `--chunk` of history ending at `end`.
3. **Resume:** if shards already exist, the next run continues from the **oldest** bar in
   the lake minus one bar period (no re-download of newer data unless you delete shards).
4. **Append:** existing monthly files are read, merged, deduplicated by timestamp (newer
   wins), and rewritten.

Re-running with the **same** `--from` when history already covers the target is a no-op.
To go **deeper**, use an earlier `--from`. To **re-download** the same range, delete the
symbol directory under the data lake first.

## Long runs (macOS)

Minute backfills can take many IB requests. Run in **Terminal.app** (not only Cursor's
integrated terminal) so closing the IDE does not kill the job:

```bash
cd /path/to/algo-trading

nohup ./build/dev_main --harvest --symbol NVDA --from 2024-05-01 \
  --bar-size "1 min" --chunk "1 W" \
  --data-lake data_lake_m1 \
  > harvest_nvda_m1.log 2>&1 &
```

Monitor:

```bash
tail -f harvest_nvda_m1.log
ps aux | grep dev_main
```

Keep TWS running; avoid sleeping the Mac during long jobs.

## Inspecting data

Folder size:

```bash
du -sh data_lake_m1/NVDA
du -sh data_lake_m1/*
```

Row count and date range (DuckDB):

```bash
duckdb -c "
SELECT COUNT(*) AS bars,
       MIN(timestamp) AS first_bar,
       MAX(timestamp) AS last_bar
FROM 'data_lake_m1/NVDA/*.parquet';
"
```

## Rough sizing

~200k one-minute RTH bars for one US stock over ~2 years ≈ **7 MB** Parquet per symbol
(compressed). Scale linearly with symbols and history depth.
