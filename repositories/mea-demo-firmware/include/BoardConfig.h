#pragma once

/// @file BoardConfig.h
/// @brief Board-spezifische Werte (ESP32 DevKit). Nur der Composition Root
///        kennt diese Werte; keine Library codiert sie fest.

#include <cstdint>

namespace app::board {

/// ADC1-Kanal GPIO 34 (nur Eingang, siehe docs/wiring.md).
constexpr std::uint8_t kAnalogInputPin = 34;

/// Klassisches ESP32-Arduino-ADC-Modell: 12 Bit.
constexpr std::uint32_t kAdcMaximumRaw = 4095;

/// Angenommene Referenzspannung. Für präzise Messungen sind Kalibrierung und
/// Dämpfungskonfiguration nötig (bewusst außerhalb des Demo-Umfangs).
constexpr float kAdcReferenceVolt = 3.3F;

constexpr std::uint32_t kSerialBaudRate = 115200;

}  // namespace app::board
