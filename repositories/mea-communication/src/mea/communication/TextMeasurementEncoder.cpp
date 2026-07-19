#include "TextMeasurementEncoder.h"

#include <cinttypes>
#include <cstdarg>
#include <cstdio>

namespace mea {

namespace {

/// Hängt formatierten Text an buffer+offset an. false, wenn der Puffer nicht
/// reicht (offset bleibt dann auf dem letzten vollständigen Stand).
bool appendFormat(char* buffer, const std::size_t capacity, std::size_t& offset,
                  const char* format, ...) noexcept {
    if (offset >= capacity) {
        return false;
    }
    std::va_list args;
    va_start(args, format);
    const int written =
        std::vsnprintf(buffer + offset, capacity - offset, format, args);
    va_end(args);
    if (written < 0 || static_cast<std::size_t>(written) >= capacity - offset) {
        return false;
    }
    offset += static_cast<std::size_t>(written);
    return true;
}

}  // namespace

const char* TextMeasurementEncoder::findLabel(const ComponentId sourceId) const noexcept {
    for (std::size_t index = 0; index < config_.labels.size(); ++index) {
        const SourceLabel& entry = config_.labels[index];
        if (entry.sourceId == sourceId && entry.label != nullptr) {
            return entry.label;
        }
    }
    return nullptr;
}

Status TextMeasurementEncoder::encode(const Measurement& measurement,
                                      std::uint8_t* output, const std::size_t capacity,
                                      std::size_t& encodedSize) const noexcept {
    encodedSize = 0;
    if (output == nullptr || capacity == 0) {
        return makeStatus(StatusCode::InvalidArgument, InvalidComponentId);
    }
    if (!hasFiniteValue(measurement)) {
        return makeStatus(StatusCode::InvalidArgument, InvalidComponentId);
    }

    char* buffer = reinterpret_cast<char*>(output);
    std::size_t offset = 0;

    const char* label = findLabel(measurement.sourceId);
    bool ok = (label != nullptr)
                  ? appendFormat(buffer, capacity, offset, "%s: ", label)
                  : appendFormat(buffer, capacity, offset, "source %u: ",
                                 static_cast<unsigned>(measurement.sourceId));

    if (ok) {
        // Dezimaltrenner ist '.' ("C"-Verhalten von printf, wie CSV-Encoder).
        const char* symbol = unitSymbol(measurement.unit);
        ok = (symbol[0] != '\0')
                 ? appendFormat(buffer, capacity, offset, "%.*f %s",
                                static_cast<int>(config_.decimalPlaces),
                                static_cast<double>(measurement.value), symbol)
                 : appendFormat(buffer, capacity, offset, "%.*f",
                                static_cast<int>(config_.decimalPlaces),
                                static_cast<double>(measurement.value));
    }

    // Qualitätsflags als Klartext, z. B. " [Stale|OutOfRange]" (ADR 0003).
    if (ok && measurement.quality != QualityFlag::None) {
        ok = appendFormat(buffer, capacity, offset, " [");
        constexpr QualityFlag kFlags[] = {
            QualityFlag::Stale, QualityFlag::OutOfRange, QualityFlag::Estimated,
            QualityFlag::SensorFault, QualityFlag::CommunicationFault};
        bool first = true;
        for (const QualityFlag flag : kFlags) {
            if (!ok || !hasFlag(measurement.quality, flag)) {
                continue;
            }
            ok = appendFormat(buffer, capacity, offset, first ? "%s" : "|%s",
                              qualityFlagName(flag));
            first = false;
        }
        if (ok) {
            ok = appendFormat(buffer, capacity, offset, "]");
        }
    }

    if (ok) {
        // Ohne Label hilft die Messwertart bei der Zuordnung.
        ok = (label != nullptr)
                 ? appendFormat(buffer, capacity, offset,
                                " (seq=%" PRIu32 ", t=%" PRIu32 " ms)\n",
                                measurement.sequence, measurement.sampledAtMs)
                 : appendFormat(buffer, capacity, offset,
                                " (%s, seq=%" PRIu32 ", t=%" PRIu32 " ms)\n",
                                measurementKindName(measurement.kind),
                                measurement.sequence, measurement.sampledAtMs);
    }

    if (!ok) {
        return makeStatus(StatusCode::CapacityExceeded, InvalidComponentId,
                          static_cast<std::uint16_t>(offset));
    }
    encodedSize = offset;
    return okStatus();
}

}  // namespace mea
