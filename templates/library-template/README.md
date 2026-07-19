# MEA Library Template

Dieses Template ist der Startpunkt fuer neue MEA-Libraries im Zielsystem.

## Zielregeln

1. Von `mea-core` abhaengen, wenn die Library MEA-Komponenten anbietet.
2. Genau eine klare Rolle haben: Device, Processor, Sink, Runtime-Erweiterung
   oder Communication-Baustein.
3. Oeffentliche Komponenten implementieren ein MEA-Interface.
4. Arduino-/ESP32-Code hinter einem kleinen HAL oder Adapter kapseln.
5. Native Tests mit Fakes schreiben.
6. README, `library.json`, `platformio.ini` und `Version.h` pflegen.

## Minimalstruktur

```text
src/
  MeaYourLibrary.h
  mea/your/library/Version.h
test/
  native/
platformio.ini
library.json
README.md
```

Siehe [../../docs/05-NEUE-LIBRARY-ANLEGEN.md](../../docs/05-NEUE-LIBRARY-ANLEGEN.md).
