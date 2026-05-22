# algo-trading

C++20 data pipeline for algorithmic trading: connect to
[Interactive Brokers TWS API](https://interactivebrokers.github.io/tws-api/),
persist OHLCV bars to columnar Parquet files for downstream backtesting and
research (DuckDB, Polars, pandas).

> **Phase 1 (current):** TWS connectivity smoke test (`reqCurrentTime`).
> Historical backfill via `reqHistoricalData` → Parquet is next.

## Stack

| Concern        | Library                               |
| -------------- | ------------------------------------- |
| Broker API     | [IBKR TWS API](https://interactivebrokers.github.io/tws-api/) (local `twsapi_macunix`) |
| Parquet I/O    | [Apache Arrow C++](https://arrow.apache.org/)             |
| JSON           | [nlohmann/json](https://github.com/nlohmann/json)         |
| Logging        | [spdlog](https://github.com/gabime/spdlog)                |
| Tests          | GoogleTest                            |
| Build          | CMake ≥ 3.20, C++20                   |

## Prerequisites

macOS with Homebrew. (Linux support is similar — replace `brew` with `apt`/`dnf`.)

```bash
bash scripts/install_deps.sh
```

This installs (via Homebrew): `cmake`, `ninja`, `llvm` (clang-format),
`pkg-config`, `openssl@3`, `nlohmann-json`, `apache-arrow`, `spdlog`, `fmt`,
`googletest`.

### IBKR TWS API (required for `ENABLE_IBKR=ON`)

Clone the official API next to this repo (sibling directory):

```bash
cd ..
git clone https://github.com/InteractiveBrokers/tws-api-public twsapi_macunix
```

CMake expects `../twsapi_macunix/IBJts/source/cppclient` (see `cmake/TwsApi.cmake`).

### TWS / IB Gateway

1. Install [Trader Workstation](https://www.interactivebrokers.com/en/trading/tws.php) or IB Gateway.
2. Enable the socket API: **Edit → Global Configuration → API → Settings**
   - Enable *ActiveX and Socket Clients*
   - Note the socket port (paper default **7497**, live **7496**)
3. Keep TWS running while using `dev_main --ibkr-check`.

## Configuration

IBKR connection settings live in a gitignored `.env` at repo root:

```bash
cp .env.example .env
$EDITOR .env   # IBKR_HOST, IBKR_PORT, IBKR_CLIENT_ID
```

## Build

```bash
scripts/build.sh              # Debug (default)
scripts/build.sh Release      # optimized (Arrow fast paths)
RUN_TESTS=1 scripts/build.sh  # build + run unit tests
```

Manual CMake (equivalent):

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

Disable IBKR if the TWS API tree is not available:

```bash
cmake -S . -B build -DENABLE_IBKR=OFF
```

Other helpers:

```bash
scripts/clean.sh              # remove build/
scripts/clean.sh --deep       # also remove data/ and cache/
scripts/format.sh             # clang-format all sources
scripts/format.sh --check     # CI-style format check
```

Binaries:

| Target        | Purpose                          |
| ------------- | -------------------------------- |
| `./build/app` | Production CLI (`--check`, etc.) |
| `./build/dev_main` | IBKR experiments (`--ibkr-check`) |
| `./build/test_environment` | Unit tests                 |

Run tests:

```bash
./build/test_environment
```

## IBKR smoke test (dev_main)

Verify TWS is reachable and the API responds:

```bash
./build/dev_main --ibkr-check
```

Expected: log line with TWS server time (Unix epoch seconds). Requires TWS
running with API enabled and `.env` pointing at the correct port.

## Project layout

```
algo-trading/
├── app/
│   ├── main.cpp          # Production CLI
│   └── dev_main.cpp      # IBKR scratchpad
├── header/
│   ├── ibkr/             # TWS Session wrapper
│   ├── market/           # OHLCV Bar type
│   ├── storage/          # ParquetWriter
│   └── util/             # time, env, logging
├── src/                  # Implementations mirroring header/
├── cmake/TwsApi.cmake    # Builds official TWS C++ client
├── unit_tests/
├── docs/                 # Design notes (time, timestamps, …)
├── scripts/
├── data/                 # Parquet output (gitignored — create locally)
└── CMakeLists.txt
```

Parquet schema per file: `timestamp[ms,UTC]`, `open`, `high`, `low`, `close`,
`volume` (all `float64` except timestamp).

### Inspect Parquet (optional)

```bash
duckdb -c "SELECT * FROM 'data/EURUSD_H1.parquet' LIMIT 5;"
```

## Implementation status

- [x] ParquetWriter: write, append, readAll, readLastTimestamp
- [x] `at::time` — UTC ms timestamps, ISO-8601 parse/format
- [x] IBKR Session: connect, `reqCurrentTime`, disconnect
- [x] `dev_main --ibkr-check` smoke test
- [ ] `reqHistoricalData` → `at::market::Bar` → Parquet backfill
- [ ] Promote backfill CLI from dev_main → app/main
- [ ] Phase 2 — live market data + strategy daemon

## Roadmap

- [x] Step 1 — build scaffold, Arrow/Parquet, env, tests
- [x] Step 2 — IBKR TWS connect + server time
- [ ] Step 3 — historical bars via TWS API
- [ ] Step 4 — backfill engine + CLI
- [ ] Step 5 — first end-to-end run + DuckDB sanity check
- [ ] Phase 2 — live streaming + execution
