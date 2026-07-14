#pragma once

/// @file IMeasurementEncoder.h
/// @brief Serialisierung eines Messwerts in einen Aufruferpuffer
///        (ADR 0006, Schicht 2). Keine dynamische Allokation.

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

namespace mea {

class IMeasurementEncoder {
public:
    virtual ~IMeasurementEncoder() = default;

    /// Serialisiert @p measurement nach @p output (höchstens @p capacity Bytes).
    /// @p encodedSize meldet die geschriebene Länge. CapacityExceeded, wenn der
    /// Puffer nicht reicht (Inhalt dann undefiniert).
    virtual Status encode(const Measurement& measurement, std::uint8_t* output,
                          std::size_t capacity,
                          std::size_t& encodedSize) const noexcept = 0;
};

}  // namespace mea
