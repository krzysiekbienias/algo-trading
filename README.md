# algo-trading

C++20 data pipeline that pulls historical and (later) live market data from
[XTB xStation 5 API](http://developers.xstore.pro/documentation/) over a
WebSocket connection and persists it to columnar Parquet files for
downstream backtesting / research.

> **Phase 1 (current):** historical M1 backfill via `getChartRangeRequest` →
> one Parquet file per symbol. Live tick/candle streaming will land in Phase 2.

## Stack

| Concern        | Library                               |
| -------------- | ------------------------------------- |
| WebSocket+TLS  | [IXWebSocket](https://github.com/machinezone/IXWebSocket) (built via CMake FetchContent) |
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

IXWebSocket is fetched and built automatically by CMake at configure time
(it was removed from Homebrew core, so we pin its source via FetchContent).

## Configuration

XTB credentials live in a gitignored `.env` file at repo root. Bootstrap it:

```bash
cp .env.example .env
$EDITOR .env   # fill in XTB_USER_ID and XTB_PASSWORD from your demo email
```

How to get a demo account: sign up at [xtb.com](https://www.xtb.com) → "Open
demo account". Your 7-digit account number is your `XTB_USER_ID`. The same
credentials work in xStation 5 web platform — verify there first.

## Build & run

```bash
cmake -S . -B build
cmake --build build -j
./build/app --help
```

## Project layout

```
algo-trading/
├── app/                  # Executable entry points
│   ├── main.cpp          # Production CLI
│   └── dev_main.cpp      # Scratchpad / experiments
├── header/               # Public headers (one folder per module)
│   ├── xtb/              # XTB API client
│   ├── storage/          # Parquet writer
│   └── util/             # Logging, env loading
├── src/                  # Implementations mirroring header/
├── unit_tests/           # GoogleTest suites
├── scripts/              # Dev helpers (deps install, etc.)
├── config/               # Runtime config files (gitignored content)
├── cache/                # Output Parquet files (gitignored)
└── CMakeLists.txt
```

## Roadmap

- [x] Step 1 — build scaffold, deps, initial commit
- [ ] Step 2 — IXWebSocket + XTB login + `getServerTime` ping
- [ ] Step 3 — `getChartRangeRequest` M1 + JSON → typed bars (price normalization via `digits`)
- [ ] Step 4 — Parquet writer (Arrow) with append + `read_last_timestamp` (smart resume)
- [ ] Step 5 — chunked backfill engine with throttling (≥250 ms) and retry/backoff
- [ ] Step 6 — first end-to-end run: `PKN.PL`, 7 days M1 → `cache/PKN.PL.parquet`
- [ ] Phase 2 — live streaming (`/demoStream`, `getCandles` subscription)
