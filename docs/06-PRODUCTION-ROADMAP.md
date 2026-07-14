# 06 – Production Roadmap

Stand: 2026-07-13 · Branch in allen Repositories: `feat/production-foundation`

Dieses Dokument ist die historische Umsetzungsroadmap fuer den Production-
Foundation-Umbau. Der aktuelle Codezustand ist in
[02-ARCHITEKTUR.md](02-ARCHITEKTUR.md),
[07-CODE-TOUR-FUER-TEAMS.md](07-CODE-TOUR-FUER-TEAMS.md) und den README-Dateien
der einzelnen Repositories beschrieben.

## 1. Ist-Zustand (Bestandsaufnahme)

Alle sieben Repositories existieren, besitzen einen `main`-Branch, einen konfigurierten
`origin` (`http://192.168.178.99:3000/Theo/`) und genau einen Scaffold-Commit.

| Repository | Inhalt (Scaffold) | Native Tests |
|---|---|---|
| mea-core | `Status` (einfaches Enum), `Measurement` (mit eingebettetem `Status`), Interfaces `IMeasurementSource/Processor/Sink`, Registry-Interfaces | 1 Test (Status-Helfer) |
| mea-managers | `SensorManager`, `ProcessorManager`, `SinkManager` (Templates, statisch), `RegistryHelpers` | 1 Test (Kapazität) |
| mea-processing | `PassThroughProcessor`, `LinearProcessor` | 1 Test (Linear) |
| mea-device-analog-input | `AnalogInputsensor` direkt auf `Arduino.h` (`analogRead` blockierend in Schleife) | keine (keine platformio.ini) |
| mea-communication | `SerialCsvSink` direkt auf Arduino-`Stream` (Transport+Kodierung+Sink vermischt) | keine (keine platformio.ini) |
| mea-state-machine | `MeasurementPipelineMachine` (1 Quelle, 1 Prozessor, 1 Sink, ruft `beginAll()` aller Registries) | 1 Test (ungültige Konfiguration) |
| mea-demo-firmware | Kompletter Aufbau in `main.cpp`, `AppConfig.h` | 1 Integrationstest, 1 Embedded-Smoke |

Werkzeuge auf dem Entwicklungs-PC: PlatformIO 6.1.19, gcc, cppcheck, clang-format, clang-tidy.

## 2. Erkennbare Schwächen

1. **Fehlermodell zu schwach:** `Status` ist ein nacktes Enum ohne Herkunft (`origin`) und
   ohne `detail`. Fehler verlieren ihre verursachende Komponente.
2. **Messwert transportiert Kontrollfluss:** `Measurement.status` vermischt Operations-
   status und Datenqualität. Keine Sequenznummern, keine Qualitätsflags.
3. **Sensor untestbar:** `AnalogInputSensor` bindet `Arduino.h` direkt, `analogRead` läuft
   in einer blockierenden Oversampling-Schleife, nur ein Ein-Element-Puffer ohne Drop-Policy.
4. **Kommunikation monolithisch:** `SerialCsvSink` übernimmt Transport, Serialisierung und
   Sink-Logik in einer Klasse; `Stream::print` kann blockieren; keine Queue, kein Backpressure.
5. **State Machine zu einfach:** genau 1 Prozessor / 1 Sink, kein Timeout, keine Retry-Policy,
   kein Backpressure-Zustand, ruft `beginAll()` aller Manager (Lebenszyklus-Verletzung).
6. **Kein Diagnosemodell:** keine Erfolgs-/Fehlerzähler, keine `ComponentHealth`.
7. **Lückenhafte Tests:** je Repository maximal ein Minimaltest, keine Contract-Tests, kein
   Rollover-Test, keine Sanitizer.
8. **Fehlende Infrastruktur:** keine `.clang-format`/`.clang-tidy`/cppcheck-Konfiguration,
   keine `docs/API.md`, keine CHANGELOGs, keine Verify-Skripte, zwei Repos ohne `platformio.ini`.
9. **Warnungen unvollständig:** `-Wconversion -Wshadow -Werror` fehlen überall.

## 3. Abhängigkeiten zwischen Repositories (Ziel)

```text
mea-core  (kein Arduino, keine Hardware)
 ├── mea-device-analog-input   (HAL: IAnalogReader; Arduino nur in ArduinoAnalogReader)
 ├── mea-processing            (rein nativ)
 ├── mea-communication         (Arduino nur in ArduinoStreamTransport)
 └── mea-managers              (rein nativ)

mea-state-machine → mea-core, mea-managers (nur Interfaces/IDs)

mea-demo-firmware → alle (einziger Composition Root)
```

Zyklen sind ausgeschlossen: Nur die Firmware kennt konkrete Klassen aller Libraries.

## 4. Geplante Änderungen (Phasen, umgesetzt im aktuellen Code)

| Phase | Repository | Kerninhalt |
|---|---|---|
| 1 | Workspace | Roadmap, ADR 0001–0006 |
| 2 | mea-core | `Types.h` (IDs, rollover-sichere Zeit), `Status{code,origin,detail}`, `Measurement` mit `QualityFlag`/`sequence`, `ArrayView`, `RingBuffer`, neue Kerninterfaces, Command-Basistypen, `ComponentHealth`, Contract-Check-Helfer, Tests |
| 3 | mea-managers | `FixedRegistry<Interface,Capacity>`, fünf Manager mit Health-Zählern, `beginAll()`-Semantik (AlreadyInitialized), Tests |
| 4 | mea-processing | `ClampProcessor`, `RangeValidationProcessor`, `MovingAverageProcessor<WindowSize>`, Umbau auf neues Status-/Messwertmodell, Tests |
| 5 | mea-device-analog-input | `IAnalogReader`-HAL, `ArduinoAnalogReader`, `FakeAnalogReader`, nicht blockierendes Oversampling mit Ringpuffer und Drop-Policy, Tests |
| 6 | mea-communication | `IByteTransport`, `ArduinoStreamTransport`, `FakeByteTransport`, `CsvMeasurementEncoder`, `BufferedMeasurementSink<QueueCapacity,FrameSize>`, `LineCommandDecoder`, Tests. `SerialCsvSink` entfällt (Migration dokumentiert) |
| 7 | mea-state-machine | `PipelineConfig` mit Prozessorkette und Multi-Sink, 9 Zustände, Timeout, Retry, Backpressure, Beobachtbarkeit, Tests |
| 8 | mea-demo-firmware | `Application` als Composition Root, `AppIds.h`/`AppConfig.h`/`BoardConfig.h`, nativer End-to-End-Integrationstest, Embedded-Smoke, ESP32-Build |
| 9 | alle | `.clang-format`, cppcheck, Skripte (`verify-all.sh` u. a.), README/API.md/CHANGELOG je Repo, GITEA-CI-Vorlage |
| 10 | alle | Abnahmekriterien prüfen, Push-Befehle dokumentieren |

## 5. Migrationsschritte

1. `mea-core` bricht bewusst die API (Status-Struktur, Interface-Signaturen). Alle
   nachgelagerten Repos werden in den Phasen 3–8 in Abhängigkeitsreihenfolge nachgezogen;
   erst nach Phase 8 ist der Workspace wieder als Ganzes konsistent baubar.
2. `SerialCsvSink` wird durch `BufferedMeasurementSink` + `CsvMeasurementEncoder` +
   `ArduinoStreamTransport` ersetzt. Migration in `mea-communication/CHANGELOG.md` und README.
3. Das CSV-Format erhält ein Versionsfeld (`version;source_id;kind;unit;value;sampled_at_ms;sequence;quality`).
4. `MeasurementPipelineMachine.begin()` initialisiert keine Manager mehr; die Firmware ruft
   `beginAll()` der Manager genau einmal selbst.
5. Alle bisherigen Funktionen werden migriert; entfernte Klassen sind in den CHANGELOGs benannt.

## 6. Risiken

| Risiko | Behandlung |
|---|---|
| `-Wconversion -Werror` auf ESP32/Arduino-Framework nicht aktivierbar (Framework-Header warnen) | Strikte Flags nativ überall; auf ESP32 nur für `src/` (`build_src_flags`), Ausnahme dokumentiert (ADR 0002-Anhang / README Firmware) |
| Sanitizer-Unterstützung in PlatformIO-native | Wird in Phase 2 verifiziert; bei Problemen dokumentierte Ausnahme |
| Kein ESP32 angeschlossen | Firmware und Embedded-Test werden nur kompiliert; Upload/Hardwarelauf werden ausdrücklich nicht behauptet |
| Gitea-Runner unbekannt | Nur Vorlage `docs/GITEA-CI.md`, kein Workflow-Commit |
| Breaking Changes über 7 Repos | Strikte Phasenreihenfolge, kleine Commits, Tests nach jeder Phase |

## 7. Offene Entscheidungen

- Kalibrierung des ESP32-ADC (Dämpfung, Nichtlinearität): bewusst außerhalb des Umfangs;
  die Demo dokumentiert die vereinfachte lineare Umrechnung.
- Eingehende Kommandos werden vorbereitet (`LineCommandDecoder`), aber noch nicht in die
  Demo-Pipeline verdrahtet.
- Gitea Actions werden erst mit vorhandenem Runner eingerichtet.

## 8. Versionierung

Alle Libraries bleiben bei `0.1.0` (erste belastbare Version). `library.json` und
`src/**/Version.h` müssen übereinstimmen. Tags werden nicht erstellt; die nötigen Befehle
stehen in `docs/03-GIT-UND-VERSIONIERUNG.md` bzw. im Abschlussbericht.
