#include "CsvMeasurementEncoder.h"

#include <cinttypes>
#include <cstdio>

namespace mea {

Status CsvMeasurementEncoder::encode(const Measurement& measurement, std::uint8_t* output,
                                     const std::size_t capacity,
                                     std::size_t& encodedSize) const noexcept {
    encodedSize = 0;
    if (output == nullptr || capacity == 0 || config_.fieldSeparator == '\0') {
        return makeStatus(StatusCode::InvalidArgument, InvalidComponentId);
    }
    if (!hasFiniteValue(measurement)) {
        return makeStatus(StatusCode::InvalidArgument, InvalidComponentId);
    }

    const char sep = config_.fieldSeparator;
    // snprintf begrenzt strikt auf capacity und terminiert immer mit '\0'.
    // Dezimaltrenner ist '.' ("C"-Verhalten von printf auf native und newlib).
    const int written = std::snprintf(
        reinterpret_cast<char*>(output), capacity,
        "%u%c%u%c%u%c%u%c%.*f%c%" PRIu32 "%c%" PRIu32 "%c%u\n",
        static_cast<unsigned>(kFormatVersion), sep,
        static_cast<unsigned>(measurement.sourceId), sep,
        static_cast<unsigned>(measurement.kind), sep,
        static_cast<unsigned>(measurement.unit), sep,
        static_cast<int>(config_.decimalPlaces), static_cast<double>(measurement.value),
        sep, measurement.sampledAtMs, sep, measurement.sequence, sep,
        static_cast<unsigned>(measurement.quality));

    if (written < 0) {
        return makeStatus(StatusCode::InternalError, InvalidComponentId);
    }
    if (static_cast<std::size_t>(written) >= capacity) {
        // Benötigte Länge (gekappt) als Detail für die Diagnose.
        return makeStatus(StatusCode::CapacityExceeded, InvalidComponentId,
                          static_cast<std::uint16_t>(written));
    }

    encodedSize = static_cast<std::size_t>(written);
    return okStatus();
}

}  // namespace mea
