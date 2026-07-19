#pragma once

/// @file TextMeasurementEncoder.h
/// @brief Menschenlesbarer Encoder für Serial-Monitore. Eine Zeile je
///        Messwert, z. B.:
///
///     aht20-temp: 26.81 degC (seq=9, t=16160 ms)
///     boden: 0.00 % [OutOfRange] (seq=9, t=16130 ms)
///     source 121: 991.05 hPa (Pressure, seq=9, t=16162 ms)
///
/// - Quellen-Labels liefert der Composition Root über die Konfiguration
///   (ADR 0001: das Label-Array muss länger leben als der Encoder); ohne
///   Treffer wird "source <id>" plus Messwertart ausgegeben.
/// - Einheiten und Qualitätsflags erscheinen als Klartext (mea-core-Helfer).
/// - Für Maschinenauswertung ist CsvMeasurementEncoder gedacht; dieses Format
///   ist bewusst NICHT versioniert und kann sich ändern.

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "IMeasurementEncoder.h"

namespace mea {

class TextMeasurementEncoder final : public IMeasurementEncoder {
public:
    /// Klartextname einer Quelle (label muss ein statisches Literal oder
    /// länger lebend als der Encoder sein).
    struct SourceLabel {
        ComponentId sourceId{InvalidComponentId};
        const char* label{nullptr};
    };

    struct Config {
        std::uint8_t decimalPlaces{2};
        ArrayView<const SourceLabel> labels{};
    };

    TextMeasurementEncoder() noexcept = default;
    explicit TextMeasurementEncoder(const Config& config) noexcept : config_(config) {}

    Status encode(const Measurement& measurement, std::uint8_t* output,
                  std::size_t capacity, std::size_t& encodedSize) const noexcept override;

private:
    [[nodiscard]] const char* findLabel(ComponentId sourceId) const noexcept;

    Config config_{};
};

}  // namespace mea
