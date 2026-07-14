# ADR 0002 – Status- und Fehlermodell

Status: akzeptiert · Datum: 2026-07-13

## Kontext

Das bisherige `enum class Status` verliert die Herkunft eines Fehlers: Ein Manager, der
Fehler mehrerer Komponenten aggregiert, kann den Verursacher nicht mehr benennen.
Exceptions sind ausgeschlossen (Embedded, kein Heap, `-fno-exceptions` auf Zielplattform üblich).

## Entscheidung

1. Rückgabetyp aller fehlbaren Operationen ist die triviale Struktur

   ```cpp
   struct Status {
       StatusCode  code{StatusCode::Ok};
       ComponentId origin{InvalidComponentId};  // meldende Komponente
       std::uint16_t detail{0};                 // geräte-/protokollspezifisch
   };
   ```

   mit `ok()` und `transient()` als `constexpr`-Abfragen.
2. `StatusCode` umfasst die 18 im Anforderungsdokument definierten Codes (Ok … InternalError).
3. **Transient** sind genau `Busy`, `NoData` und `WouldBlock`: Wiederholen ohne
   Zustandsänderung ist sinnvoll. `Timeout` ist bewusst nicht transient (die State Machine
   entscheidet über Retries per Policy).
4. Keine dynamischen Fehlertexte. Menschlich lesbare Namen liefert
   `statusCodeName(StatusCode)` (statische Stringliterale, für Logging/Debug).
5. Komponenten setzen `origin` auf ihre eigene ID. Manager reichen Fehler unverändert
   weiter; nur wenn `origin` leer ist, tragen sie die ID der verursachenden Komponente nach.
6. Rückgabewerte werden nicht ignoriert: fehlbare Funktionen sind `[[nodiscard]]` bzw.
   ihr Status wird ausgewertet oder ausdrücklich mit Begründung verworfen.

## Compilerwarnungen (Anhang)

`-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` sind in allen nativen Umgebungen
aktiv (inkl. Sanitizern, siehe ADR 0004/Tests). **Ausnahme:** Im ESP32-Build erzeugen
Arduino-Framework-Header `-Wconversion`-Warnungen, die wir nicht beheben können. Dort
gelten die strikten Flags nur für eigenen Code (`build_src_flags` der Firmware); die
Libraries selbst werden nativ strikt geprüft.

## Konsequenzen

- Breaking Change gegenüber dem Scaffold (`isOk(status)` → `status.ok()`); alle Repos
  werden in Abhängigkeitsreihenfolge migriert.
- `Status` bleibt trivial kopierbar (6 Bytes), geeignet für `ComponentHealth` und Rückgabe
  per Wert.
