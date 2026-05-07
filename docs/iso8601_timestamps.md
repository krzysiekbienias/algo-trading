# ISO-8601 Timestamps — ściąga

## Anatomia timestampu

```
2000 - 01 - 01 T 00:00:00 .123 Z
────   ──   ──   ────────  ───  ─
rok  mies dzień  czas[s]   ms  strefa
```

| Część | Przykład | Znaczenie |
|---|---|---|
| `2000-01-01` | data | rok-miesiąc-dzień (zawsze w tej kolejności) |
| `T` | separator | oddziela datę od czasu (**T**ime) |
| `00:00:00` | czas | godzina:minuta:sekunda |
| `.123` | milisekundy | część podsekundowa (opcjonalna) |
| `Z` | strefa | UTC+0 (patrz niżej) |

---

## Co oznacza `Z` na końcu?

`Z` to skrót od **"Zulu time"** — militarny kod NATO na **UTC+0**.
Pochodzi od alfabetu fonetycznego (Alfa, Bravo, Charlie... Zulu).

Praktycznie oznacza: *"ten czas jest w UTC, nie przesuwaj go o żadną strefę."*

### Porównanie zapisu stref

| String | Znaczenie |
|---|---|
| `2000-01-01T12:00:00Z` | Południe w UTC (= 13:00 w Polsce zimą) |
| `2000-01-01T12:00:00+02:00` | Południe w Polsce latem (= 10:00 UTC) |
| `2000-01-01T12:00:00` | Południe... ale **nie wiemy gdzie!** Niebezpieczne |

Timestamp bez `Z` lub bez offsetu jest niejednoznaczny — dlatego zawsze dodajemy `Z`.

---

## Dlaczego w tym projekcie zawsze używamy UTC / `Z`?

- **XTB API** zwraca pole `ctm` w milisekundach od Unix epoch (UTC)
- **Apache Parquet** przechowuje timestampy jako `timestamp[ms, UTC]`
- Dzięki temu nie ma nigdy pytania *"w jakiej strefie jest ten timestamp?"*

---

## Akceptowane formaty wejściowe (`parseIso8601`)

| Format | Przykład | Uwagi |
|---|---|---|
| Tylko data | `2026-05-06` | interpretowana jako `00:00:00 UTC` |
| Data + czas + `Z` | `2026-05-06T17:42:13Z` | standard |
| Data + czas + ms + `Z` | `2026-05-06T17:42:13.123Z` | z milisekundami |
| Legacy (spacja) | `2026-05-06 17:42:13` | bez `Z`, zakładamy UTC |

## Format wyjściowy (`formatIso8601`)

Zawsze produkuje pełny string z milisekundami i `Z`:

```
2026-05-06T17:42:13.123Z
```
