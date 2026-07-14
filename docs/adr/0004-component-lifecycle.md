# ADR 0004 – Komponenten-Lebenszyklus

Status: akzeptiert · Datum: 2026-07-13

## Kontext

Hardwareinitialisierung in Konstruktoren ist auf Embedded-Systemen fehleranfällig
(statische Initialisierungsreihenfolge, Hardware noch nicht bereit, kein Fehlerkanal).
Das Scaffold ließ zudem die State Machine bei jedem Fehler-Retry `beginAll()` aller
Manager aufrufen und initialisierte damit fremde Komponenten neu.

## Entscheidung

1. **Konstruktoren initialisieren keine Hardware.** Sie speichern nur Konfiguration und
   Referenzen und sind `noexcept`.
2. **`begin()` initialisiert**, validiert die Konfiguration und ist der einzige Ort für
   Hardware-Setup. Signatur: `Status begin() noexcept`.
3. **Komponenten-`begin()` ist reinitialisierend:** Ein erneuter Aufruf setzt die
   Komponente vollständig zurück (Puffer leeren, Zähler unverändert lassen ist erlaubt,
   Sequenznummern laufen weiter) und gibt bei Erfolg `Ok` zurück. Dieses Verhalten ist je
   Komponente dokumentiert.
4. **Manager-`beginAll()` ist genau einmal erlaubt:** Ein zweiter Aufruf gibt
   `AlreadyInitialized` zurück (gewählte Variante laut Anforderung 5.3). Damit kann keine
   State Machine versehentlich alle Komponenten neu initialisieren.
5. **Die State Machine initialisiert keine Manager.** Ihr `begin(nowMs)` validiert die
   Pipeline-Konfiguration und löst nur IDs über die Locator-Interfaces zu gecachten
   Pointern auf. Nach Registry-Änderungen ist ein explizites erneutes `begin(nowMs)` nötig.
6. **Zyklische Verarbeitung** ausschließlich über `update(TimestampMs nowMs)`; `update()`
   blockiert nie und leistet pro Aufruf nur begrenzte Arbeit.
7. Aufruffolge im Composition Root:
   Konstruktion (statisch) → Registrierung → `beginAll()` je Manager (genau einmal) →
   `pipeline.begin(millis())` → zyklisch `updateAll()`/`update()`.

## Konsequenzen

- Vor `begin()` melden Komponenten `NotInitialized` (Contract-Test).
- Diagnose (`ComponentHealth`) wird von Managern gepflegt, nicht von den Komponenten selbst.
