#pragma once

/// @file Measurement.h
/// @brief Messwertmodell (ADR 0003). Operationsstatus und Messwertqualität sind
///        zwei verschiedene Dinge: Ein erfolgreicher Funktionsstatus (Status::ok())
///        sagt nichts über die Qualität des Wertes (Measurement::quality) aus.

#include <cmath>
#include <cstdint>
#include <type_traits>

#include "Types.h"

namespace mea {

/// Physikalische bzw. logische Art eines Messwerts.
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

/// Einheit eines Messwerts.
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

/// Qualitätsflags als Bitmaske. QualityFlag::None bedeutet uneingeschränkte Qualität.
enum class QualityFlag : std::uint16_t {
    None = 0,
    Stale = 1U << 0U,
    OutOfRange = 1U << 1U,
    Estimated = 1U << 2U,
    SensorFault = 1U << 3U,
    CommunicationFault = 1U << 4U
};

[[nodiscard]] constexpr QualityFlag operator|(const QualityFlag lhs,
                                              const QualityFlag rhs) noexcept {
    return static_cast<QualityFlag>(static_cast<std::uint16_t>(lhs) |
                                    static_cast<std::uint16_t>(rhs));
}

[[nodiscard]] constexpr QualityFlag operator&(const QualityFlag lhs,
                                              const QualityFlag rhs) noexcept {
    return static_cast<QualityFlag>(static_cast<std::uint16_t>(lhs) &
                                    static_cast<std::uint16_t>(rhs));
}

constexpr QualityFlag& operator|=(QualityFlag& lhs, const QualityFlag rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

/// True, wenn @p flags alle in @p flag gesetzten Bits enthält.
[[nodiscard]] constexpr bool hasFlag(const QualityFlag flags,
                                     const QualityFlag flag) noexcept {
    return (flags & flag) == flag && flag != QualityFlag::None;
}

/// Ein einzelner Messwert. Enthält bewusst keinen Operationsstatus (ADR 0003).
struct Measurement {
    ComponentId sourceId{InvalidComponentId};
    MeasurementKind kind{MeasurementKind::Unknown};
    Unit unit{Unit::None};
    float value{0.0F};
    /// Zeitpunkt des Abschlusses der Messung (bei Oversampling: letztes Sample).
    TimestampMs sampledAtMs{0};
    /// Monoton je Quelle; Lücken zeigen verlorene Messwerte an. Überlauf erlaubt.
    SequenceNumber sequence{0};
    QualityFlag quality{QualityFlag::None};
};

/// True, wenn die Quelle gültig und der Wert endlich ist.
[[nodiscard]] inline bool isValid(const Measurement& measurement) noexcept {
    return measurement.sourceId != InvalidComponentId && std::isfinite(measurement.value);
}

/// True, wenn der Wert endlich ist (kein NaN, kein ±Inf).
[[nodiscard]] inline bool hasFiniteValue(const Measurement& measurement) noexcept {
    return std::isfinite(measurement.value);
}

static_assert(std::is_trivially_copyable<Measurement>::value,
              "Measurement muss trivial kopierbar bleiben");
static_assert(sizeof(Measurement) <= 20, "Measurement muss kompakt bleiben");

}  // namespace mea
