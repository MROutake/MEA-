# Verdrahtung (ESP32 DevKit)

## Demo-Aufbau

Die Demo misst eine Spannung an **GPIO 34** (ADC1_CH6) und gibt sie als CSV
über die serielle USB-Schnittstelle aus.

```text
Spannungsquelle 0 … 3,3 V ────────► GPIO 34 (ADC1_CH6)
GND der Quelle ───────────────────► GND ESP32
```

Einfachster Testaufbau: ein Potentiometer (10 kΩ) zwischen 3V3 und GND,
Schleifer an GPIO 34.

```text
3V3 ──┬──────────────┐
      │              │
      │          [10 kΩ Poti]
      │              │ Schleifer ──► GPIO 34
      │              │
GND ──┴──────────────┘
```

## Hinweise

- **GPIO 34 ist nur Eingang** (kein interner Pull-up/-down möglich); die
  Quelle muss den Pin aktiv treiben.
- **Niemals mehr als 3,3 V** an den Pin legen.
- ADC1 verwenden (GPIO 32–39): ADC2 kollidiert mit WLAN.
- Das ADC-Modell der Demo ist bewusst vereinfacht linear
  (`Volt = Rohwert · 3,3 / 4095`). Der reale ESP32-ADC ist nichtlinear und
  benötigt für genaue Messungen Kalibrierung und passende Dämpfung
  (siehe `BoardConfig.h`); das liegt außerhalb des Demo-Umfangs.

## Konfigurationsorte

| Wert | Datei |
|---|---|
| Pin, ADC-Maximalwert, Referenzspannung, Baudrate | `include/BoardConfig.h` |
| Abtastintervall, Oversampling, Pipeline-Timing, Queue-Größen | `include/AppConfig.h` |
| Komponenten-IDs | `include/AppIds.h` |
