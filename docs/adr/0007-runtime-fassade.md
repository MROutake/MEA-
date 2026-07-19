# ADR 0007 – Runtime-Fassade (MeasurementNode)

Status: akzeptiert · Datum: 2026-07-19

## Kontext

Der Composition Root der Sensor-Node-Firmware bestand zu ~60 % aus
Verdrahtungs-Boilerplate: Komponenten-IDs wurden dreifach genannt (AppIds,
Komponenten-Config, Pipeline-Config), Initialisierungs-Reihenfolgen (Gerät vor
Kanälen, Transport vor Sink, Quellen vor Pipelines) standen nur in Kommentaren,
und jede Registrierung brauchte manuelles Status-Checking. Verdrahtungsfehler
fielen erst zur Laufzeit als `NotFound` auf. Geteilte Mehrkanal-Chips
(AHT20/BMP280-Device) hatten kein gemeinsames Interface und mussten von jeder
Firmware einzeln initialisiert werden.

## Entscheidung

1. **`IDevice` in mea-core**: gemeinsames Lebenszyklus-Interface
   (`begin()`/`update()`) für geteilte Geräte und Transporte.
   `IByteTransport` erbt von `IDevice`; `Aht20Device`/`Bmp280Device`
   implementieren es (`update()` == `poll()`).
2. **`MeasurementPipelineMachine` wird default-konstruierbar** mit
   nachgelagertem `configure()`, damit Fassaden Pipelines in fester Kapazität
   vorhalten können. Die bestehende Konstruktor-API bleibt erhalten.
3. **Neues Repo `mea-runtime`** mit der Fassade `MeasurementNode`
   (Compile-Time-Kapazitäten, kein Heap, ADR 0001):
   - Fluent-Verdrahtung ohne ID-Wiederholung:
     `node.addPipeline(id, source).through(p1, p2).into(sink)` – Quellen,
     Prozessoren und Sinks registrieren sich dabei automatisch (dedupliziert).
   - `begin()` erzwingt die Reihenfolge Geräte → Quellen → Prozessoren →
     Sinks → Pipelines; `update()` die Reihenfolge Geräte → Quellen → Sinks →
     Pipelines.
   - Fehlermodell: Verdrahtungsfehler (doppelte IDs, Kapazität) blockieren
     den Start; Hardware-Ausfälle (Gerät/Quelle) deaktivieren nur die
     abhängigen Pipelines (degradierter Betrieb); Statusmeldungen laufen
     dedupliziert über einen injizierten `StatusReporter`.
   - Die Manager aus mea-managers dienen dem Node als Registries/Locators
     (ADR 0005); den Lebenszyklus treibt der Node je Komponente selbst, weil
     `beginAll()`-Semantik (alles oder nichts) keinen degradierten Betrieb
     erlaubt.

## Konsequenzen

- Composition Roots deklarieren nur noch Komponenten und ihre Verdrahtung;
  der Sensor-Node schrumpfte von ~250 auf ~115 Zeilen ohne Verlust an
  Diagnose (Reporter, `node.pipeline(id)`-Zugriff).
- `mea-demo-firmware` bleibt bewusst auf dem Low-Level-Weg (Manager +
  Pipeline von Hand) als Lehrbeispiel für die darunterliegende Mechanik.
- Neue Sensor-Libraries sollten geteilte Chips als `IDevice` modellieren,
  damit sie ohne Sonderbehandlung in den Node passen
  (siehe mea-device-aht20 / mea-device-bmp280 als Muster).
