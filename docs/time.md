# Module: `at::time` — Timestamp utilities

**File:** `header/util/time.hpp` / `src/util/time.cpp`  
**Namespace:** `at::time`

---

## Overview

Centralny moduł odpowiedzialny za reprezentację i konwersję znaczników czasu
(timestamps) w całym projekcie. Zapewnia jeden kanoniczny typ (`Timestamp`)
i zestaw funkcji pomocniczych do konwersji między formatami używanymi przez:
- **XTB API** — pole `ctm` w milisekundach od Unix epoch (UTC)
- **Apache Arrow / Parquet** — `timestamp[ms, UTC]` = int64 ms od epoch
- **Człowiek** — łańcuchy ISO-8601 (`2026-05-06T17:42:13.123Z`)

---

## Canonical type

```cpp
using Timestamp = std::chrono::sys_time<std::chrono::milliseconds>;
```

`sys_time<milliseconds>` to silnie typowany punkt w czasie zakotwiczony w UTC
z precyzją do milisekund. Nie można go pomylić z "milisekundami od czegoś
innego", bo jest to typ, a nie gołe `int64`.

---

## Functions

### `toEpochMs`
```cpp
std::int64_t toEpochMs(Timestamp tp) noexcept;
```
Zwraca liczbę milisekund od Unix epoch (1970-01-01T00:00:00Z).
Jest to **wire format** stosowany przez XTB API i Apache Parquet.

| Argument | Typ         | Opis                        |
|----------|-------------|---------------------------- |
| `tp`     | `Timestamp` | Dowolny punkt w czasie UTC  |

**Przykład:**
```cpp
auto t = at::time::fromEpochMs(946'684'800'000LL);
std::int64_t ms = at::time::toEpochMs(t); // 946684800000
```

---

### `fromEpochMs`
```cpp
Timestamp fromEpochMs(std::int64_t ms) noexcept;
```
Inverse `toEpochMs`. Tworzy `Timestamp` z liczby milisekund od Unix epoch.
Używany bezpośrednio przy parsowaniu pola `ctm` z XTB API.

**Przykład:**
```cpp
// XTB ctm = 1746529333123
auto tp = at::time::fromEpochMs(1'746'529'333'123LL);
```

---

### `fromEpochSeconds`
```cpp
Timestamp fromEpochSeconds(std::int64_t s) noexcept;
```
Skrót dla wejść w sekundach (np. z CLI, konfiguracji). Wewnętrznie mnoży
wartość przez 1000 i deleguje do `fromEpochMs`.

**Przykład:**
```cpp
auto start = at::time::fromEpochSeconds(1'746'529'333LL);
```

---

### `now`
```cpp
Timestamp now() noexcept;
```
Zwraca bieżący czas systemowy obcięty do precyzji milisekund.
Używany przy logowaniu i w zapytaniach do API, gdzie potrzebny jest
aktualny timestamp.

---

### `formatIso8601`
```cpp
std::string formatIso8601(Timestamp tp);
```
Formatuje `Timestamp` do czytelnego dla człowieka łańcucha ISO-8601 w UTC.
Sufiks `Z` jest zawsze dołączany — strefa czasowa jest jednoznaczna.

**Format wyjściowy:** `"YYYY-MM-DDTHH:MM:SS.mmmZ"`

**Przykład:**
```cpp
auto tp = at::time::fromEpochMs(946'684'800'123LL);
at::time::formatIso8601(tp); // "2000-01-01T00:00:00.123Z"
```

---

### `parseIso8601`
```cpp
std::optional<Timestamp> parseIso8601(std::string_view s);
```
Parsuje łańcuch ISO-8601 / RFC-3339 na `Timestamp`. W razie błędu
parsowania zwraca `std::nullopt` — decyzja co do reakcji na błąd
należy do wywołującego.

**Akceptowane formaty:**

| Format                      | Przykład                    |
|-----------------------------|-----------------------------|
| Tylko data                  | `"2026-05-06"`              |
| Data + czas z `Z`           | `"2026-05-06T17:42:13Z"`   |
| Data + czas + ms z `Z`      | `"2026-05-06T17:42:13.123Z"`|
| Legacy ze spacją (bez `Z`)  | `"2026-05-06 17:42:13"`    |

> Wszystkie formaty są zawsze interpretowane jako **UTC**.

**Przykład:**
```cpp
auto tp = at::time::parseIso8601("2026-05-06T17:42:13.123Z");
if (tp) {
    // użyj *tp
} else {
    // błąd parsowania
}
```

---

## Design decisions

- **Brak strefy czasowej** — cały system operuje wyłącznie na UTC.
  `sys_time` (w odróżnieniu od `local_time`) gwarantuje to na poziomie systemu typów.
- **`noexcept` na inline funkcjach** — konwersje są czystymi obliczeniami
  bez alokacji, więc `noexcept` jest zgodne z prawdą i pozwala kompilatorowi
  na lepszą optymalizację.
- **`std::nullopt` zamiast wyjątku** — `parseIso8601` może dostawać dane
  z zewnątrz (CLI, XTB API), więc błąd parsowania to oczekiwana ścieżka,
  nie wyjątek.
