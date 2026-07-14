# ADR 0005 – Ausführungsmodell der State Machine

Status: akzeptiert · Datum: 2026-07-13

## Kontext

Die Pipeline-Maschine koordiniert Quelle → Prozessorkette → Sinks, ohne konkrete Klassen
zu kennen, ohne zu blockieren und mit vorhersehbarer Arbeit pro `update()`.

## Entscheidung

1. **Zustände:** `Uninitialized, Disabled, WaitingForCycle, WaitingForMeasurement,
   Processing, Publishing, Backpressure, RetryDelay, Fault`.
2. **Auflösung:** `begin(nowMs)` validiert `PipelineConfig`, löst alle IDs über
   `IComponentLocator<>`-Interfaces (implementiert von den Managern) auf und cached die
   Pointer. Kapazitätsgrenzen: max. 8 Prozessoren, max. 4 Sinks je Pipeline
   (Compile-Time-Konstanten, Verletzung → `CapacityExceeded`).
3. **Begrenzte Arbeit pro `update()`:** höchstens ein Zustandsübergang plus die Arbeit des
   aktuellen Zustands. `Processing` führt die gesamte (konfigurationsbegrenzte)
   Prozessorkette in einem Update aus; `Publishing` versucht jeden noch ausstehenden Sink
   genau einmal.
4. **Zyklusdefinition:** Ein Zyklus ist erst erfolgreich, wenn **alle konfigurierten Sinks
   den Wert übernommen haben** (Standard laut Anforderung). Übernommene Sinks werden in
   einer Bitmaske gemerkt; ein `WouldBlock` eines Sinks blockiert die anderen nicht.
5. **Backpressure:** `WouldBlock` eines Sinks führt in den Zustand `Backpressure`; dort
   wird bei jedem Update erneut versucht, bis `publishTimeoutMs` überschritten ist.
   Backpressure ist kein permanenter Fehler.
6. **Retry-Policy:** Zyklusfehler (Akquise-Timeout, Publish-Timeout, transienter
   Komponentenfehler, Prozessorfehler) führen in `RetryDelay` (`retry.delayMs`), danach
   Wiederholung des Zyklus. Nach `retry.maximumAttempts` erfolglosen Versuchen wird der
   Zyklus als fehlgeschlagen gezählt (`failedCycles`, ggf. `droppedMeasurements`) und die
   Maschine kehrt zu `WaitingForCycle` zurück – die Pipeline läuft weiter.
7. **Fault:** Nur nicht behebbare Zustände (`NotInitialized`, `InvalidConfiguration`,
   `NotFound`, `InternalError` einer Komponente) führen nach `Fault`. `Fault` ist sticky
   und wird nur durch ein explizites erneutes `begin(nowMs)` verlassen.
8. **Aktivierung:** `startImmediately=false` startet in `Disabled`; `enable(nowMs)` /
   `disable()` schalten zur Laufzeit. `enable()` beginnt einen frischen Zyklus.
9. **Zeit:** Alle Vergleiche rollover-sicher über unsigned Differenzen
   (`elapsedMs(now, since) >= interval`).
10. Die Maschine ruft niemals `beginAll()`/`updateAll()` der Manager auf (siehe ADR 0004)
    und ruft `source->update()` nicht selbst – das macht der Composition Root.

## Konsequenzen

- Beobachtbarkeit: `state()`, `lastStatus()`, `completedCycles()`, `failedCycles()`,
  `droppedMeasurements()`, `lastMeasurement()`.
- Kein `delay()`, keine Endlosschleifen, deterministische Obergrenze der Arbeit pro Update.
