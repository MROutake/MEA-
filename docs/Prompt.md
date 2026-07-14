Du arbeitest als erfahrener Embedded-Software-Architekt und C++-Entwickler in einem bestehenden PlatformIO-Multi-Repository-Workspace.

## Projektkontext

Workspace-Root:

```text
MEA-Embedded-Workspace/
```

Vorhandene Git-Repositories:

```text
repositories/
├── mea-core
├── mea-device-analog-input
├── mea-processing
├── mea-communication
├── mea-managers
├── mea-state-machine
└── mea-demo-firmware
```

Git-Server:

```text
http://192.168.178.99:3000/Theo/
```

Die Repositories sind bereits initialisiert, besitzen jeweils einen `main`-Branch und einen konfigurierten `origin`.

Das aktuelle Zielsystem ist zunächst:

```text
Board: ESP32 DevKit
PlatformIO Board-ID: esp32dev
Framework: Arduino
Sprache: C++17
Entwicklungs-PC: Arch Linux
IDE: Visual Studio Code mit PlatformIO
```

## Hauptziel

Entwickle den vorhandenen Workspace schrittweise zu einer produktionsreifen, modularen Embedded-Plattform.

Die Plattform soll folgende Eigenschaften besitzen:

* Hardwaretreiber funktionieren eigenständig.
* Konkrete Hardware ist von Ablauf- und Verarbeitungslogik getrennt.
* Alle Module besitzen klar definierte Schnittstellen.
* Komponenten werden über IDs und Manager registriert.
* Zustandsmaschinen kennen nur Schnittstellen und keine konkrete Hardware.
* Sensorwerte können durch Verarbeitungsketten geleitet werden.
* Messwerte können an ein oder mehrere Kommunikationsziele gesendet werden.
* Eingehende Kommunikationsdaten und Befehle sollen später integrierbar sein.
* Alle Abläufe sind nicht blockierend.
* Der Speicherverbrauch ist begrenzt und vorhersehbar.
* Die Kernlogik ist ohne Arduino-Abhängigkeiten auf dem Entwicklungs-PC testbar.
* Jede Library ist unabhängig versionierbar.
* Das Demo-Projekt zeigt den echten Einsatz auf einem ESP32.

Arbeite nicht nur an Beispielcode. Entwickle belastbaren, dokumentierten und getesteten Bibliothekscode.

---

# 1. Verbindliche Arbeitsweise

## 1.1 Vor Änderungen

Untersuche zuerst alle vorhandenen Repositories, Dateien, Abhängigkeiten, Tests und PlatformIO-Konfigurationen.

Erstelle anschließend im Workspace:

```text
docs/06-PRODUCTION-ROADMAP.md
docs/adr/
```

Dokumentiere mindestens:

* den aktuellen Zustand,
* erkennbare Schwächen,
* geplante Änderungen,
* Abhängigkeiten zwischen Repositories,
* Migrationsschritte,
* Risiken,
* offene Entscheidungen.

Erstelle Architecture Decision Records für wesentliche Entscheidungen, beispielsweise:

```text
docs/adr/0001-memory-and-ownership.md
docs/adr/0002-status-and-error-model.md
docs/adr/0003-measurement-format.md
docs/adr/0004-component-lifecycle.md
docs/adr/0005-state-machine-execution.md
docs/adr/0006-communication-layering.md
```

Treffe sinnvolle Entscheidungen selbst. Stelle keine Rückfragen zu Details, die sich anhand der bestehenden Architektur vernünftig entscheiden lassen.

## 1.2 Änderungen in kleinen Schritten

Arbeite in dieser Reihenfolge:

1. Architektur prüfen und dokumentieren.
2. `mea-core` stabilisieren.
3. `mea-managers` stabilisieren.
4. `mea-processing` erweitern.
5. `mea-device-analog-input` hardwareabstrahiert umsetzen.
6. `mea-communication` in Transport, Kodierung und Sink trennen.
7. `mea-state-machine` produktionsreif erweitern.
8. `mea-demo-firmware` als Composition Root aufbauen.
9. Tests, statische Analyse und Dokumentation vervollständigen.
10. Releasefähigkeit aller Repositories prüfen.

Nach jedem Schritt müssen alle bereits betroffenen Tests weiterhin erfolgreich laufen.

## 1.3 Git-Regeln

Arbeite in jedem Repository auf einem eigenen Branch:

```text
feat/production-foundation
```

Erstelle kleine, fachlich getrennte Commits nach Conventional Commits, beispielsweise:

```text
feat(core): add diagnostic status model
test(core): cover measurement validation
refactor(managers): use bounded generic registry
docs(state-machine): document retry behavior
```

Keine Force-Pushes, keine Tags und keine Releases erstellen.

Nicht automatisch auf den Remote-Server pushen. Am Ende nur die notwendigen Push-Befehle ausgeben.

---

# 2. Verbindliche Abhängigkeitsstruktur

Es dürfen keine zyklischen Abhängigkeiten entstehen.

Zielstruktur:

```text
mea-core
├── mea-device-analog-input
├── mea-processing
├── mea-communication
└── mea-managers

mea-state-machine
├── mea-core
└── mea-managers

mea-demo-firmware
├── mea-core
├── mea-device-analog-input
├── mea-processing
├── mea-communication
├── mea-managers
└── mea-state-machine
```

Regeln:

* `mea-core` kennt kein Arduino.
* `mea-core` kennt keine konkrete Hardware.
* `mea-core` kennt keine konkrete Zustandsmaschine.
* Gerätetreiber kennen keine Manager oder Zustandsmaschinen.
* Processing-Komponenten kennen keine konkreten Sensoren.
* Kommunikationskomponenten kennen keine konkreten Sensoren.
* Manager besitzen registrierte Komponenten nicht.
* Die State Machine kennt Komponenten nur über Interfaces und IDs.
* Nur `mea-demo-firmware` darf alle konkreten Klassen miteinander verbinden.

Lege keine neuen Repositories an, sofern keine zwingende und dokumentierte Begründung besteht.

---

# 3. Produktionsregeln für Embedded C++

Für alle Libraries gelten folgende Regeln:

* C++17
* keine Exceptions
* kein RTTI, sofern es nicht nachweislich benötigt wird
* keine dynamischen Speicherallokationen nach dem Systemstart
* kein `new` oder `delete` in der normalen Laufzeit
* keine Arduino-`String`-Objekte in Libraries
* keine unbeschränkten Container
* keine versteckten globalen Zustände
* keine blockierenden `delay()`-Aufrufe
* keine Endlosschleifen innerhalb einer Library
* keine Hardwareinitialisierung in Konstruktoren
* keine unkontrollierten blockierenden Schreibzugriffe
* keine ignorierten Rückgabewerte
* keine still verschluckten Fehler
* keine fest codierten Pins in einer Library
* keine Zugangsdaten oder Tokens im Repository

Verwende:

* feste Arrays,
* begrenzte Ringpuffer,
* explizite Kapazitäten,
* `std::uint8_t`, `std::uint16_t` und `std::uint32_t`,
* `enum class`,
* `constexpr`,
* `[[nodiscard]]`,
* `noexcept`, wo fachlich korrekt,
* Initialisierung über `begin()`,
* zyklische Verarbeitung über `update(nowMs)`,
* rollover-sichere Zeitvergleiche mit unsigned Differenzen,
* nicht besitzende Referenzen oder Pointer mit dokumentierter Lebensdauer.

Compilerwarnungen müssen aktiviert sein:

```text
-Wall
-Wextra
-Wpedantic
-Wconversion
-Wshadow
-Werror
```

Falls eine Warnung auf einer Plattform technisch nicht sinnvoll aktivierbar ist, dokumentiere die konkrete Ausnahme.

---

# 4. Inhalt von mea-core

`mea-core` bildet den stabilen Vertrag zwischen allen Repositories.

Es darf kein `Arduino.h` enthalten.

## 4.1 Grundtypen

Definiere mindestens:

```cpp
namespace mea {

using ComponentId = std::uint16_t;
using TimestampMs = std::uint32_t;
using SequenceNumber = std::uint32_t;

constexpr ComponentId InvalidComponentId = 0;

}
```

ID `0` ist ungültig und reserviert.

## 4.2 Status- und Fehlermodell

Trenne den Rückgabestatus einer Operation von den transportierten Messdaten.

Verwende mindestens folgende Statuscodes:

```cpp
enum class StatusCode : std::uint8_t {
    Ok = 0,
    Busy,
    NoData,
    WouldBlock,
    NotInitialized,
    AlreadyInitialized,
    InvalidArgument,
    InvalidConfiguration,
    CapacityExceeded,
    DuplicateId,
    NotFound,
    Timeout,
    IoError,
    ProtocolError,
    ChecksumError,
    ProcessingError,
    Unsupported,
    InternalError
};
```

Implementiere einen kleinen Status-Typ:

```cpp
struct Status {
    StatusCode code{StatusCode::Ok};
    ComponentId origin{InvalidComponentId};
    std::uint16_t detail{0};

    [[nodiscard]] constexpr bool ok() const noexcept;
    [[nodiscard]] constexpr bool transient() const noexcept;
};
```

Anforderungen:

* `origin` identifiziert die Komponente, die den Fehler meldet.
* `detail` kann geräte- oder protokollspezifische Zusatzinformationen enthalten.
* Keine dynamischen Fehlermeldungstexte im Status.
* Menschlich lesbare Statusnamen dürfen über eine separate Funktion bereitgestellt werden.
* Der Status muss trivial kopierbar bleiben.

## 4.3 Messwertmodell

Ein Messwert darf nicht gleichzeitig als Kontrollfluss für Fehler dienen.

Definiere:

```cpp
enum class MeasurementKind : std::uint8_t {
    Unknown = 0,
    RawAnalog,
    Voltage,
    Current,
    Resistance,
    Temperature,
    Humidity,
    Pressure,
    Frequency,
    DigitalState
};

enum class Unit : std::uint8_t {
    None = 0,
    RawCount,
    Volt,
    MilliVolt,
    Ampere,
    MilliAmpere,
    Ohm,
    DegreeCelsius,
    Percent,
    Pascal,
    Hertz,
    Boolean
};
```

Definiere Qualitätsflags als Bitmaske:

```cpp
enum class QualityFlag : std::uint16_t {
    None = 0,
    Stale = 1 << 0,
    OutOfRange = 1 << 1,
    Estimated = 1 << 2,
    SensorFault = 1 << 3,
    CommunicationFault = 1 << 4
};
```

Messwertstruktur:

```cpp
struct Measurement {
    ComponentId sourceId{InvalidComponentId};
    MeasurementKind kind{MeasurementKind::Unknown};
    Unit unit{Unit::None};
    float value{0.0F};
    TimestampMs sampledAtMs{0};
    SequenceNumber sequence{0};
    QualityFlag quality{QualityFlag::None};
};
```

Implementiere:

* Prüfung auf gültige ID,
* Prüfung auf endliche Werte,
* Bitoperationen für Qualitätsflags,
* Hilfsfunktionen zur Validierung,
* Compile-Time-Prüfungen für Größe und triviale Kopierbarkeit.

Dokumentiere ausdrücklich, dass ein erfolgreicher Funktionsstatus und die Messwertqualität zwei verschiedene Dinge sind.

## 4.4 Views für feste Konfigurationen

Da C++17 kein `std::span` besitzt, implementiere einen kleinen nicht besitzenden Array-View:

```cpp
template<typename T>
class ArrayView {
public:
    constexpr ArrayView() noexcept;
    constexpr ArrayView(const T* data, std::size_t size) noexcept;

    [[nodiscard]] constexpr const T* data() const noexcept;
    [[nodiscard]] constexpr std::size_t size() const noexcept;
    [[nodiscard]] constexpr bool empty() const noexcept;
    [[nodiscard]] constexpr const T& operator[](std::size_t index) const noexcept;
};
```

Keine Bounds-Checks mit Exceptions. Optional kann eine sichere `at`-Funktion mit Status oder Pointer ergänzt werden.

## 4.5 Kerninterfaces

Die öffentlichen Kerninterfaces sollen mindestens so aussehen:

```cpp
class IMeasurementSource {
public:
    virtual ~IMeasurementSource() = default;

    [[nodiscard]] virtual ComponentId id() const noexcept = 0;
    virtual Status begin() noexcept = 0;
    virtual Status update(TimestampMs nowMs) noexcept = 0;
    [[nodiscard]] virtual std::size_t available() const noexcept = 0;
    virtual Status read(Measurement& output) noexcept = 0;
};

class IMeasurementProcessor {
public:
    virtual ~IMeasurementProcessor() = default;

    [[nodiscard]] virtual ComponentId id() const noexcept = 0;
    virtual Status begin() noexcept = 0;

    [[nodiscard]] virtual bool accepts(
        MeasurementKind kind,
        Unit unit
    ) const noexcept = 0;

    virtual Status process(
        const Measurement& input,
        Measurement& output
    ) noexcept = 0;
};

class IMeasurementSink {
public:
    virtual ~IMeasurementSink() = default;

    [[nodiscard]] virtual ComponentId id() const noexcept = 0;
    virtual Status begin() noexcept = 0;
    virtual Status update(TimestampMs nowMs) noexcept = 0;
    [[nodiscard]] virtual std::size_t capacityAvailable() const noexcept = 0;
    virtual Status submit(const Measurement& measurement) noexcept = 0;
};
```

Semantik:

* `update()` darf nicht blockieren.
* `available()` gibt die Anzahl auslesbarer Messwerte an.
* `read()` entfernt genau einen Wert aus der Quelle.
* `submit()` kopiert einen Wert in einen begrenzten internen Puffer oder verarbeitet ihn sofort.
* `WouldBlock` signalisiert Backpressure.
* `capacityAvailable()` darf null sein.
* `begin()` muss ein dokumentiertes Verhalten bei mehrfacher Ausführung besitzen.
* Komponenten bleiben Eigentum des Composition Roots.

## 4.6 Diagnosemodell

Ergänze ein einfaches Diagnosemodell:

```cpp
struct ComponentHealth {
    ComponentId componentId;
    Status lastStatus;
    TimestampMs lastSuccessMs;
    std::uint32_t successCount;
    std::uint32_t errorCount;
};
```

Vermeide dafür unnötige virtuelle Zusatzinterfaces, sofern die Informationen durch Manager gepflegt werden können.

## 4.7 Spätere Kommunikationsbefehle vorbereiten

Definiere zunächst nur stabile Basistypen, ohne ein komplexes universelles Nachrichtensystem zu erfinden:

```cpp
enum class CommandType : std::uint16_t {
    Unknown = 0,
    Start,
    Stop,
    Reset,
    RequestMeasurement,
    SetParameter
};

struct Command {
    ComponentId sourceId;
    ComponentId targetId;
    CommandType type;
    std::uint32_t argument;
    TimestampMs receivedAtMs;
};
```

Interfaces:

```cpp
class ICommandSource {
public:
    virtual ~ICommandSource() = default;

    [[nodiscard]] virtual ComponentId id() const noexcept = 0;
    virtual Status begin() noexcept = 0;
    virtual Status update(TimestampMs nowMs) noexcept = 0;
    [[nodiscard]] virtual std::size_t available() const noexcept = 0;
    virtual Status read(Command& output) noexcept = 0;
};

class ICommandHandler {
public:
    virtual ~ICommandHandler() = default;

    [[nodiscard]] virtual ComponentId id() const noexcept = 0;
    virtual Status handle(
        const Command& command,
        TimestampMs nowMs
    ) noexcept = 0;
};
```

Implementiere diese Schnittstellen im Core, aber baue noch kein unnötig komplexes Command-Framework.

---

# 5. Inhalt von mea-managers

`mea-managers` enthält Registries und Laufzeitverwaltung, aber keine Fachlogik.

## 5.1 Generische feste Registry

Implementiere intern eine wiederverwendbare Registry:

```cpp
template<typename Interface, std::size_t Capacity>
class FixedRegistry;
```

Anforderungen:

* keine Besitzübernahme,
* keine dynamische Allokation,
* ID `0` ablehnen,
* doppelte IDs ablehnen,
* Kapazitätsüberschreitung melden,
* Suche nach ID,
* Iteration über registrierte Komponenten,
* deterministisches Verhalten,
* native Unit Tests.

## 5.2 Fachliche Manager

Stelle weiterhin klar benannte Manager bereit:

```cpp
SensorManager<Capacity>
ProcessorManager<Capacity>
SinkManager<Capacity>
CommandSourceManager<Capacity>
CommandHandlerManager<Capacity>
```

Manager sollen mindestens besitzen:

```cpp
Status registerComponent(...);
Status beginAll();
Status updateAll(TimestampMs nowMs);
Interface* find(ComponentId id) const;
std::size_t size() const;
ComponentHealth health(ComponentId id) const;
```

Nur Manager mit aktualisierbaren Komponenten benötigen `updateAll()`.

## 5.3 Initialisierungsregeln

Manager werden durch das Hauptprogramm genau einmal initialisiert.

Eine State Machine darf nicht bei jedem Start pauschal alle registrierten Komponenten erneut initialisieren.

`beginAll()` muss entweder:

* idempotent sein, oder
* bei erneutem Aufruf eindeutig `AlreadyInitialized` melden.

Dokumentiere die gewählte Variante.

## 5.4 Fehlerbehandlung

Manager müssen Fehler inklusive verursachender Component-ID zurückgeben.

Ein Fehler einer Komponente darf nicht still in einen allgemeinen Fehler ohne Herkunft umgewandelt werden.

Implementiere Diagnosezähler pro Komponente.

---

# 6. Inhalt von mea-processing

`mea-processing` enthält reine, hardwareunabhängige Verarbeitung.

Implementiere mindestens:

```text
PassThroughProcessor
LinearProcessor
ClampProcessor
RangeValidationProcessor
MovingAverageProcessor
```

## 6.1 Regeln

* keine Arduino-Abhängigkeit,
* keine dynamische Allokation,
* vollständig native testbar,
* Prüfung von Eingangs-Kind und Einheit,
* Prüfung auf `NaN` und unendliche Werte,
* klare Ausgabeeinheit,
* keine stillen Einheitenkonvertierungen,
* deterministisches Verhalten.

## 6.2 Moving Average

Der gleitende Mittelwert muss eine Compile-Time-Kapazität besitzen:

```cpp
template<std::size_t WindowSize>
class MovingAverageProcessor;
```

Teste:

* Anlaufphase,
* volles Fenster,
* Reset-Verhalten,
* ungültige Werte,
* unterschiedliche Zeitstempel,
* Sequenznummern.

## 6.3 Verarbeitungsketten

Die State Machine soll mehrere Prozessoren nacheinander ausführen können.

Die einzelnen Prozessoren sollen sich gegenseitig nicht kennen.

Keine zentrale monolithische `DataProcessor`-Klasse bauen.

---

# 7. Inhalt von mea-device-analog-input

Die Analog-Eingangs-Library soll nicht direkt untestbar an Arduino-Funktionen gekoppelt sein.

## 7.1 HAL-Schnittstelle

Definiere eine Hardwareabstraktion:

```cpp
class IAnalogReader {
public:
    virtual ~IAnalogReader() = default;

    virtual Status beginPin(std::uint8_t pin) noexcept = 0;

    virtual Status readRaw(
        std::uint8_t pin,
        std::uint32_t& output
    ) noexcept = 0;

    [[nodiscard]] virtual std::uint32_t maximumRawValue() const noexcept = 0;
};
```

Implementiere:

```text
ArduinoAnalogReader
FakeAnalogReader für native Tests
```

`AnalogInputSensor` erhält `IAnalogReader&` per Konstruktor.

## 7.2 Konfiguration

Die Konfiguration soll mindestens enthalten:

```cpp
struct AnalogInputConfig {
    ComponentId sourceId;
    std::uint8_t pin;
    TimestampMs sampleIntervalMs;
    std::uint16_t samplesPerMeasurement;
    MeasurementKind outputKind;
    Unit outputUnit;
};
```

## 7.3 Nicht blockierendes Oversampling

Führe nicht beliebig viele `analogRead()`-Aufrufe in einer einzigen langen Schleife aus.

Implementiere einen begrenzten, nicht blockierenden Ablauf:

* pro `update()` höchstens eine konfigurierbare kleine Anzahl Samples,
* Akkumulation über mehrere Updates,
* fertiger Messwert wird in einem begrenzten Puffer gespeichert,
* Sequenznummer wird je Messwert erhöht,
* Zeitstempel entspricht dem Abschluss oder dokumentiert dem Start der Messung,
* bei vollem Puffer gilt eine dokumentierte Drop-Policy.

Bevorzugte Drop-Policy:

```text
Neuen Messwert verwerfen und Diagnosezähler erhöhen.
```

Die Library darf keine Umrechnung in Volt vornehmen. Das ist Aufgabe von `mea-processing`.

## 7.4 Tests

Teste mindestens:

* ungültige ID,
* ungültiges Intervall,
* null Samples,
* Initialisierungsfehler der HAL,
* Leseprobleme,
* Oversampling,
* Integerüberlauf der Summe,
* Zeitüberlauf von `uint32_t`,
* Puffer voll,
* wiederholtes Lesen,
* Sequenznummern.

---

# 8. Inhalt von mea-communication

Trenne Kommunikation in drei Schichten:

```text
Transport
Kodierung
Measurement Sink
```

Eine Klasse darf nicht gleichzeitig Hardwaretransport, Serialisierung, Queue und Anwendungslogik übernehmen.

## 8.1 Transportinterface

Definiere ein Byte-Transportinterface innerhalb von `mea-communication`:

```cpp
class IByteTransport {
public:
    virtual ~IByteTransport() = default;

    virtual Status begin() noexcept = 0;
    virtual Status update(TimestampMs nowMs) noexcept = 0;

    [[nodiscard]] virtual std::size_t writable() const noexcept = 0;

    virtual Status write(
        const std::uint8_t* data,
        std::size_t size,
        std::size_t& written
    ) noexcept = 0;

    [[nodiscard]] virtual std::size_t readable() const noexcept = 0;

    virtual Status read(
        std::uint8_t* data,
        std::size_t capacity,
        std::size_t& readCount
    ) noexcept = 0;
};
```

Implementiere zunächst:

```text
ArduinoStreamTransport
FakeByteTransport
```

Die Arduino-spezifische Klasse darf `Arduino.h` enthalten. Encoder und Tests dürfen es nicht benötigen.

## 8.2 Encoder

Definiere:

```cpp
class IMeasurementEncoder {
public:
    virtual ~IMeasurementEncoder() = default;

    virtual Status encode(
        const Measurement& measurement,
        std::uint8_t* output,
        std::size_t capacity,
        std::size_t& encodedSize
    ) const noexcept = 0;
};
```

Implementiere zunächst:

```text
CsvMeasurementEncoder
```

Anforderungen:

* keine dynamische Zeichenkette,
* `snprintf` nur mit streng begrenzten Puffern,
* kein Pufferüberlauf,
* definierter Dezimaltrenner,
* definierter Feldtrenner,
* dokumentierte Feldreihenfolge,
* Versionsfeld im Format,
* exakte native Tests.

Vorgeschlagenes CSV-Format:

```text
version;source_id;kind;unit;value;sampled_at_ms;sequence;quality
```

## 8.3 Gepufferter Sink

Implementiere:

```text
BufferedMeasurementSink<QueueCapacity, FrameSize>
```

Der Sink:

* implementiert `IMeasurementSink`,
* nimmt Messwerte über `submit()` an,
* serialisiert sie in einen begrenzten Frame,
* behandelt partielle Schreibvorgänge,
* schreibt in `update()` nicht blockierend weiter,
* meldet `WouldBlock`, wenn die Queue voll ist,
* verliert Daten nicht still,
* führt Diagnosezähler.

Teste:

* vollständiger Write,
* partieller Write,
* Transport nicht bereit,
* Queue voll,
* zu kleiner Framepuffer,
* Encoderfehler,
* Transportfehler,
* Wiederanlauf.

Der bisherige `SerialCsvSink` kann als kompatibler Wrapper bestehen bleiben oder kontrolliert ersetzt werden. Dokumentiere die Migration.

## 8.4 Eingehende Kommunikation

Bereite einen einfachen zeilenorientierten Command-Decoder vor, aber halte ihn klein.

Kein JSON-Framework und keine dynamische Allokation einführen.

---

# 9. Inhalt von mea-state-machine

`mea-state-machine` koordiniert Abläufe. Sie initialisiert nicht die gesamte Anwendung.

## 9.1 Pipeline-Konfiguration

Eine Pipeline muss unterstützen:

* genau eine Messquelle,
* null bis mehrere Prozessoren,
* ein bis mehrere Sinks,
* Messintervall,
* Timeout,
* Retry-Strategie,
* Backpressure,
* aktivierbar/deaktivierbar.

Konfiguration:

```cpp
struct RetryPolicy {
    TimestampMs delayMs;
    std::uint16_t maximumAttempts;
};

struct PipelineConfig {
    ComponentId pipelineId;
    ComponentId sourceId;
    ArrayView<const ComponentId> processorIds;
    ArrayView<const ComponentId> sinkIds;
    TimestampMs cycleIntervalMs;
    TimestampMs acquisitionTimeoutMs;
    TimestampMs publishTimeoutMs;
    RetryPolicy retry;
    bool startImmediately;
};
```

Die ID-Arrays werden statisch im Composition Root angelegt und müssen länger leben als die State Machine.

## 9.2 Zustände

Mindestens:

```cpp
enum class PipelineState : std::uint8_t {
    Uninitialized = 0,
    Disabled,
    WaitingForCycle,
    WaitingForMeasurement,
    Processing,
    Publishing,
    Backpressure,
    RetryDelay,
    Fault
};
```

## 9.3 Ablauf

Die Maschine soll:

1. Konfiguration validieren.
2. Komponenten über Manager auflösen.
3. Auf den Zykluszeitpunkt warten.
4. Auf einen Messwert warten.
5. Timeout bei fehlendem Messwert erkennen.
6. Alle Prozessoren in Reihenfolge ausführen.
7. Ergebnis an alle Sinks verteilen.
8. Backpressure eines Sinks behandeln.
9. Retrys nach Policy durchführen.
10. Zähler und letzten Fehler bereitstellen.
11. Nach erfolgreichem Zyklus erneut warten.

## 9.4 Wichtige Regeln

* Kein `delay()`.
* Keine Endlosschleife.
* Pro `update()` nur begrenzte Arbeit.
* Keine Initialisierung aller Manager in `begin()`.
* Keine konkreten Klassen referenzieren.
* Keine dynamische Pipeline-Zusammenstellung zur Laufzeit.
* IDs bei `begin()` auflösen und Pointer cachen.
* Bei Registry-Änderungen ist ein explizites erneutes `begin()` nötig.
* Zeitüberlauf muss korrekt funktionieren.
* Backpressure ist nicht automatisch ein permanenter Fehler.
* Ein Sink darf keine anderen Sinks blockieren.
* Definiere, ob ein Zyklus erst erfolgreich ist, wenn alle Sinks übernommen haben.
* Verwende standardmäßig „alle konfigurierten Sinks müssen übernehmen“.

## 9.5 Beobachtbarkeit

Stelle mindestens bereit:

```cpp
PipelineState state() const noexcept;
Status lastStatus() const noexcept;
std::uint32_t completedCycles() const noexcept;
std::uint32_t failedCycles() const noexcept;
std::uint32_t droppedMeasurements() const noexcept;
const Measurement& lastMeasurement() const noexcept;
```

Optional kann ein kleines Observer-Interface ergänzt werden, sofern es ohne unnötige Komplexität möglich ist.

## 9.6 Tests

Teste mindestens:

* Happy Path,
* Start nicht sofort,
* Source Busy,
* Source NoData,
* Mess-Timeout,
* Prozessorfehler,
* mehrere Prozessoren,
* mehrere Sinks,
* Sink WouldBlock,
* Publish-Timeout,
* temporärer Transportfehler,
* permanenter Fehler,
* Retry erfolgreich,
* Retry-Limit erreicht,
* fehlende Component-ID,
* doppelte IDs,
* ungültige Konfiguration,
* Zeitüberlauf,
* Deaktivieren und erneutes Aktivieren,
* keine unbeabsichtigte Neuinitialisierung der Komponenten.

---

# 10. Inhalt von mea-demo-firmware

Das Firmware-Repository ist der einzige Composition Root.

## 10.1 Struktur

Baue folgende Struktur auf:

```text
mea-demo-firmware/
├── include/
│   ├── AppConfig.h
│   ├── AppIds.h
│   └── BoardConfig.h
├── src/
│   ├── main.cpp
│   ├── Application.h
│   └── Application.cpp
├── test/
│   ├── native/
│   └── embedded/
├── docs/
│   ├── wiring.md
│   └── runtime.md
└── platformio.ini
```

## 10.2 Verantwortlichkeiten

`main.cpp` soll nur:

```cpp
void setup() {
    application.begin();
}

void loop() {
    application.update(millis());
}
```

enthalten.

`Application` übernimmt:

* Instanziierung der HAL,
* Instanziierung des Sensors,
* Instanziierung der Prozessoren,
* Instanziierung des Transports,
* Instanziierung des Encoders und Sinks,
* Registrierung der Komponenten,
* Initialisierung der Manager,
* Start der Pipeline,
* zyklisches Aktualisieren aller Manager und Maschinen,
* Ausgabe schwerer Diagnosefehler.

## 10.3 IDs

Alle IDs liegen zentral in `AppIds.h`:

```cpp
namespace app::ids {

constexpr mea::ComponentId AnalogInput1 = 100;
constexpr mea::ComponentId RawToVoltage = 200;
constexpr mea::ComponentId VoltageClamp = 201;
constexpr mea::ComponentId SerialOutput = 300;
constexpr mea::ComponentId MeasurementPipeline = 400;

}
```

Keine Magic Numbers im Anwendungscode.

## 10.4 Reale Demo-Pipeline

Setze folgende Pipeline um:

```text
ESP32 ADC GPIO 34
    ↓
AnalogInputSensor
    ↓
LinearProcessor: ADC-Rohwert zu Volt
    ↓
ClampProcessor: gültiger Spannungsbereich
    ↓
BufferedMeasurementSink
    ↓
CsvMeasurementEncoder
    ↓
ArduinoStreamTransport
    ↓
Serial
```

Die Demo muss auf einem ESP32 kompilieren und flashbar sein.

## 10.5 Konfiguration

Definiere alle Werte zentral:

* ADC-Pin,
* Sampling-Intervall,
* Oversampling,
* ADC-Maximalwert,
* Referenzspannung,
* Verarbeitungsparameter,
* Queue-Größen,
* Pipeline-Timeouts,
* Retry-Policy,
* Baudrate.

Keine Library darf diese Werte fest vorgeben.

---

# 11. Tests und Qualität

## 11.1 Teststruktur jedes Repositories

Jedes Library-Repository soll mindestens enthalten:

```text
test/
├── native/
└── embedded/
```

Embedded-Tests sind nur erforderlich, wenn echte Hardwarefunktionen getestet werden.

## 11.2 Testarten

Implementiere:

* native Unit Tests,
* Contract Tests für Interfaces,
* Integrations-Test der gesamten Pipeline,
* ESP32 Smoke Build,
* mindestens einen Embedded Smoke Test,
* statische Analyse.

## 11.3 Contract Tests

Erstelle wiederverwendbare Testregeln für:

```text
IMeasurementSource
IMeasurementProcessor
IMeasurementSink
```

Beispiele:

Eine Source muss:

* vor `begin()` `NotInitialized` melden,
* ungültige Konfiguration ablehnen,
* nach erfolgreichem `begin()` aktualisierbar sein,
* `available()` korrekt pflegen,
* keine Daten doppelt ausgeben.

Ein Processor muss:

* ungültige Eingangsarten ablehnen,
* keine ungültigen Floating-Point-Werte erzeugen,
* den Eingabemesswert nicht unerwartet verändern.

Ein Sink muss:

* Backpressure melden,
* seine Queue-Grenzen einhalten,
* nicht blockieren,
* partielle Ausgaben korrekt fortsetzen.

## 11.4 Native Tests

Aktiviere nach Möglichkeit:

```text
-fsanitize=address
-fsanitize=undefined
```

Nur für die native Umgebung.

Prüfe, ob PlatformIO diese Flags zuverlässig unterstützt. Dokumentiere nötige Ausnahmen.

## 11.5 Statische Analyse und Formatierung

Richte ein:

* `.clang-format`
* `.clang-tidy`, soweit mit PlatformIO sinnvoll nutzbar
* `cppcheck`
* konsistente Include-Reihenfolge
* Doxygen-kompatible Kommentare für öffentliche APIs

Erstelle Workspace-Skripte:

```text
scripts/format-all.sh
scripts/check-format.sh
scripts/analyze-all.sh
scripts/test-all.sh
scripts/build-all.sh
scripts/verify-all.sh
```

`verify-all.sh` führt alle lokal möglichen Qualitätsprüfungen nacheinander aus und beendet sich beim ersten Fehler mit einem Exit-Code ungleich null.

---

# 12. PlatformIO-Konfiguration

## 12.1 Lokale Entwicklung

Behalte für die lokale gemeinsame Entwicklung:

```ini
lib_deps =
    mea-core=symlink://../mea-core
```

und entsprechende Einträge für weitere Repositories.

## 12.2 Releasefähige Abhängigkeiten

Dokumentiere zusätzlich, wie ein Firmware-Release später getaggte Remote-Versionen verwendet:

```ini
lib_deps =
    git+http://192.168.178.99:3000/Theo/mea-core.git#v0.1.0
```

Keine Tokens oder Passwörter in `platformio.ini`.

## 12.3 Build-Umgebungen

Das Demo-Projekt soll mindestens besitzen:

```text
native
esp32dev
esp32dev_test
```

Optional:

```text
native_debug
esp32dev_release
```

Der Release-Build soll sinnvolle Optimierungen verwenden, ohne Warnungen zu unterdrücken.

---

# 13. Dokumentation je Repository

Jedes Repository benötigt:

```text
README.md
CHANGELOG.md
LICENSE
library.json
platformio.ini
examples/
test/
```

Das README muss enthalten:

* Zweck der Library,
* Verantwortlichkeiten,
* Nicht-Verantwortlichkeiten,
* Abhängigkeiten,
* öffentliche API,
* Besitz- und Lebensdauerregeln,
* Speicherverhalten,
* Fehlerverhalten,
* Thread-/Task-Sicherheit,
* minimales Beispiel,
* Testbefehle,
* bekannte Grenzen.

Ergänze in jedem Repository eine Datei:

```text
docs/API.md
```

Keine API darf nur aus dem Quellcode erraten werden müssen.

---

# 14. Versionierung

Verwende Semantic Versioning.

Starte alle noch nicht stabilen Libraries bei:

```text
0.1.0
```

Regeln:

* Breaking Change vor 1.0 erhöht die Minor-Version.
* Neue rückwärtskompatible Funktion erhöht die Minor-Version.
* Fehlerkorrektur erhöht die Patch-Version.
* `library.json` und Versionsheader müssen übereinstimmen.
* Keine Version anhand eines Branch-Namens ableiten.

Erstelle noch keine Tags. Dokumentiere nur die später notwendigen Befehle.

---

# 15. CI-Vorbereitung

Prüfe, ob der Workspace für Gitea Actions vorbereitet werden kann.

Erstelle nur dann `.gitea/workflows/ci.yml`, wenn die Konfiguration ohne Annahmen über einen nicht vorhandenen Runner sinnvoll ist.

Andernfalls erstelle eine dokumentierte Vorlage:

```text
docs/GITEA-CI.md
```

Die CI soll später mindestens ausführen:

* Native Tests,
* ESP32 Build,
* cppcheck,
* Formatprüfung,
* Paket-/Manifestprüfung.

Keine Zugangsdaten in Workflows speichern.

---

# 16. Abnahmekriterien

Die Arbeit ist erst abgeschlossen, wenn folgende Bedingungen erfüllt sind:

1. Es bestehen keine zyklischen Repository-Abhängigkeiten.
2. `mea-core` lässt sich nativ ohne Arduino kompilieren.
3. Processing, Manager und State Machine lassen sich nativ testen.
4. Hardwarezugriffe sind über eine testbare HAL gekapselt.
5. Die Kommunikation behandelt partielle Writes und Backpressure.
6. Die State Machine unterstützt mehrere Prozessoren und Sinks.
7. Keine Library verwendet `delay()`.
8. Keine Library verwendet Arduino-`String`.
9. Keine Library allokiert während des normalen Betriebs dynamischen Speicher.
10. Alle IDs und Kapazitäten werden validiert.
11. Zeitüberlauf wird getestet.
12. Jeder öffentliche Fehler besitzt einen Status und eine Herkunft.
13. Die komplette Demo-Pipeline läuft in einem Native-Integrationstest.
14. Der ESP32-Build ist erfolgreich.
15. Der Embedded Smoke Test kompiliert.
16. Statische Analyse meldet keine unbehandelten relevanten Fehler.
17. Alle öffentlichen APIs sind dokumentiert.
18. Jede Library besitzt eigene Tests.
19. Alle bestehenden Funktionen wurden entweder migriert oder ihre Entfernung dokumentiert.
20. `scripts/verify-all.sh` läuft erfolgreich durch.

---

# 17. Vorgegebene Ausgabestruktur des Agenten

Berichte nach jeder Phase:

```text
Phase:
Geänderte Repositories:
Architekturentscheidungen:
Implementierte Funktionen:
Ausgeführte Tests:
Testergebnis:
Offene Risiken:
Nächster Schritt:
```

Am Ende liefere:

1. Zusammenfassung der Architektur.
2. Liste aller geänderten Dateien.
3. Abhängigkeitsgraph.
4. RAM-relevante feste Puffer und Kapazitäten.
5. Alle ausgeführten Befehle.
6. Exakte Testergebnisse.
7. Noch offene Punkte.
8. Empfohlene Versionsnummer je Repository.
9. Vorgeschlagene Commit-Historie.
10. Befehle zum manuellen Pushen.

Beispiel:

```bash
git -C repositories/mea-core push -u origin feat/production-foundation
git -C repositories/mea-processing push -u origin feat/production-foundation
git -C repositories/mea-managers push -u origin feat/production-foundation
git -C repositories/mea-device-analog-input push -u origin feat/production-foundation
git -C repositories/mea-communication push -u origin feat/production-foundation
git -C repositories/mea-state-machine push -u origin feat/production-foundation
git -C repositories/mea-demo-firmware push -u origin feat/production-foundation
```

## Ehrlichkeitsregel

Behaupte niemals, dass ein Test, Build, Flash-Vorgang oder Hardwaretest erfolgreich war, wenn er nicht tatsächlich ausgeführt wurde.

Falls kein ESP32 angeschlossen ist:

* Firmware kompilieren,
* Embedded-Test kompilieren,
* klar angeben, dass Upload und Hardwarelauf nicht ausgeführt wurden.

Beginne jetzt mit der Bestandsaufnahme. Verändere noch keinen Quellcode, bevor die Roadmap und die ADR-Grundstruktur erstellt wurden.
