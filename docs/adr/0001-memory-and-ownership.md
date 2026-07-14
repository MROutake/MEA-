# ADR 0001 – Speicher und Besitz

Status: akzeptiert · Datum: 2026-07-13

## Kontext

Die Plattform läuft auf einem ESP32 (begrenzter RAM) und muss deterministisch sein.
Dynamische Allokation zur Laufzeit ist fehleranfällig (Fragmentierung, OOM) und schwer
vorhersehbar.

## Entscheidung

1. **Keine dynamische Allokation nach dem Systemstart.** Kein `new`/`delete`, keine
   unbeschränkten Container. Alle Kapazitäten sind Compile-Time-Parameter (Templates)
   oder dokumentierte Konstanten.
2. **Der Composition Root (mea-demo-firmware) besitzt alle Komponenten** als statische
   Objekte mit Programm-Lebensdauer.
3. **Manager und State Machine besitzen nichts.** Sie halten nicht besitzende Pointer bzw.
   Referenzen. Registrierte Komponenten müssen länger leben als jeder Manager, der sie
   referenziert; das garantiert der Composition Root durch statische Lebensdauer.
4. **Konfigurations-Arrays (z. B. Prozessor-ID-Listen)** liegen statisch im Composition
   Root und werden über den nicht besitzenden `mea::ArrayView<const ComponentId>` übergeben.
5. **Puffer** sind feste Arrays oder der generische `mea::RingBuffer<T, Capacity>`
   (Compile-Time-Kapazität, keine Allokation).
6. Referenz-Parameter in Konstruktoren (`IAnalogReader&`, `IByteTransport&`, …) sind nicht
   besitzend; die referenzierten Objekte müssen die nutzende Komponente überleben.

## Konsequenzen

- RAM-Bedarf ist statisch aus den Template-Parametern ablesbar.
- Kapazitätsüberschreitung ist ein normaler, gemeldeter Fehler (`CapacityExceeded`), kein Absturz.
- Kein RTTI, keine Exceptions nötig (siehe ADR 0002).
