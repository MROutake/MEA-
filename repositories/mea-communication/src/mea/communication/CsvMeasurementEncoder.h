#pragma once

/// @file CsvMeasurementEncoder.h
/// @brief CSV-Encoder (Formatversion 1). Eine Zeile je Messwert:
///
///     version;source_id;kind;unit;value;sampled_at_ms;sequence;quality\n
///
/// - Feldtrenner: ';' (konfigurierbar), Dezimaltrenner: '.' (fest, keine Locale)
/// - kind/unit/quality numerisch (Werte der enum class)
/// - Versionsfeld ist immer das erste Feld

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "IMeasurementEncoder.h"

namespace mea {

class CsvMeasurementEncoder final : public IMeasurementEncoder {
public:
    /// Version des CSV-Formats (erstes Feld jeder Zeile).
    static constexpr std::uint8_t kFormatVersion = 1;

    struct Config {
        char fieldSeparator{';'};
        std::uint8_t decimalPlaces{3};
    };

    CsvMeasurementEncoder() noexcept = default;
    explicit CsvMeasurementEncoder(const Config& config) noexcept : config_(config) {}

    Status encode(const Measurement& measurement, std::uint8_t* output,
                  std::size_t capacity, std::size_t& encodedSize) const noexcept override;

private:
    Config config_{};
};

}  // namespace mea
