# algo-trading

C++20 data pipeline that pulls historical market data from the
[XTB xStation 5 API](https://xopenhub.pro/api/xapi-protocol-documentation)
over WebSocket and persists OHLCV bars to columnar Parquet files for
downstream backtesting and research (DuckDB, Polars, pandas).

> **Phase 1 (current):** historical backfill via `getChartRangeRequest` →
> one Parquet file per symbol/timeframe. Live tick streaming lands in Phase 2.

## Stack

| Concern        | Library                               |
| -------------- | ------------------------------------- |
| WebSocket+TLS  | [IXWebSocket](https://github.com/machinezone/IXWebSocket) (CMake FetchContent) |
| JSON           | [nlohmann/json](https://github.com/nlohmann/json)         |
| Parquet I/O    | [Apache Arrow C++](https://arrow.apache.org/)             |
| Logging        | [spdlog](https://github.com/gabime/spdlog)                |
| Tests          | GoogleTest                            |
| Build          | CMake ≥ 3.20, C++20                   |

## Prerequisites

macOS with Homebrew. (Linux support trivial — replace `brew` with `apt`/`dnf`.)

```bash
bash scripts/install_deps.sh
```

This installs (via Homebrew): `cmake`, `pkg-config`, `openssl@3`,
`nlohmann-json`, `apache-arrow`, `spdlog`, `fmt`, `googletest`.

IXWebSocket is fetched and built automatically by CMake at configure time.

## Configuration

XTB credentials live in a gitignored `.env` file at repo root:

```bash
cp .env.example .env
$EDITOR .env   # fill in XTB_USER_ID and XTB_PASSWORD from your demo email
```

**Demo endpoint:** `wss://ws.xapi.pro/demo` (legacy `ws.xtb.com` was disabled
2025-03-14). Use demo credentials only during development.

How to get a demo account: sign up at [xtb.com](https://www.xtb.com) → "Open
demo account". Your account number is `XTB_USER_ID`. Verify login in xStation 5
first.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

Binaries:

| Target        | Purpose                          |
| ------------- | -------------------------------- |
| `./build/app` | Production CLI (`--check`, etc.) |
| `./build/dev_main` | Scratchpad — backfill experiments |
| `./build/test_environment` | Unit tests                 |

Run tests:

```bash
./build/test_environment
```

## Backfill (dev_main)

End-to-end historical fetch: connect → login → chunked backfill → Parquet.

```bash
mkdir -p data

./build/dev_main --backfill \
  --symbol EURUSD \
  --period H1 \
  --out data/EURUSD_H1.parquet \
  --from 2024-01-01
```

| Flag       | Required | Description |
| ---------- | -------- | ----------- |
| `--backfill` | yes    | Run backfill mode |
| `--symbol`   | yes    | Instrument, e.g. `EURUSD`, `USDJPY` |
| `--period`   | yes    | `M1`, `M5`, `M15`, `M30`, `H1`, `H4`, `D1`, `W1`, `MN1` |
| `--out`      | yes    | Output Parquet path (one file per symbol+period) |
| `--from`     | no     | ISO-8601 start date. Default: 24h ago (M1/M5) or 2 years (others) |

Re-running the same command appends only missing bars (`readLastTimestamp` smart
resume). Throttle defaults to 250 ms between XTB requests.

Example — multiple symbols (run separately for now):

```bash
./build/dev_main --backfill --symbol EURUSD --period H1 --out data/EURUSD_H1.parquet --from 2024-01-01
./build/dev_main --backfill --symbol USDJPY --period H1 --out data/USDJPY_H1.parquet --from 2024-01-01
```

### Inspect Parquet (optional)

```bash
duckdb -c "SELECT * FROM 'data/EURUSD_H1.parquet' LIMIT 5;"
```

## Project layout

```
algo-trading/
├── app/
│   ├── main.cpp          # Production CLI
│   └── dev_main.cpp      # Backfill scratchpad (promote to main when stable)
├── header/
│   ├── backfill/         # BackfillEngine
│   ├── xtb/              # Client + api wrappers
│   ├── storage/          # ParquetWriter
│   └── util/             # time, env, logging
├── src/                  # Implementations mirroring header/
├── unit_tests/
├── docs/                 # Design notes (time, timestamps, …)
├── scripts/
├── data/                 # Parquet output (gitignored — create locally)
└── CMakeLists.txt
```

Parquet schema per file: `timestamp[ms,UTC]`, `open`, `high`, `low`, `close`,
`volume` (all `float64` except timestamp).

## Implementation status

- [x] XTB WebSocket client (`connect`, `login`, `call`, throttle)
- [x] API wrappers: `getServerTime`, `getChartRange` (XTB price decoding)
- [x] ParquetWriter: write, append, readAll, readLastTimestamp
- [x] BackfillEngine: chunked `runOne`, multi-symbol `run`
- [x] dev_main backfill CLI
- [ ] First verified live demo fetch (manual smoke test)
- [ ] Promote backfill CLI from dev_main → app/main
- [ ] Phase 2 — live streaming + strategy daemon

## Roadmap (original steps)

- [x] Step 1 — build scaffold, deps
- [x] Step 2 — IXWebSocket + XTB login
- [x] Step 3 — `getChartRangeRequest` + typed bars
- [x] Step 4 — Parquet writer with append + smart resume
- [x] Step 5 — BackfillEngine + dev_main CLI
- [ ] Step 6 — first end-to-end demo run + DuckDB sanity check
- [ ] Phase 2 — live streaming (`/demoStream`)
