#pragma once

/// @file ArduinoStreamTransport.h
/// @brief IByteTransport auf Arduino-Stream-Basis (z. B. Serial). Einzige
///        Arduino-abhängige Klasse dieser Library; nur unter ARDUINO übersetzt.
///
/// Nicht blockierend: write() schreibt höchstens availableForWrite() Bytes.
/// Die Baudraten-Initialisierung (Serial.begin) ist Aufgabe des Composition
/// Roots; begin() dieser Klasse prüft nur die Konfiguration.

#ifdef ARDUINO

#include <Arduino.h>

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "IByteTransport.h"

namespace mea {

class ArduinoStreamTransport final : public IByteTransport {
public:
    /// @param stream Nicht besitzend; muss den Transport überleben (ADR 0001).
    explicit ArduinoStreamTransport(Stream& stream) noexcept : stream_(stream) {}

    Status begin() noexcept override { return okStatus(); }
    Status update(TimestampMs) noexcept override { return okStatus(); }

    [[nodiscard]] std::size_t writable() const noexcept override {
        const int available = stream_.availableForWrite();
        return available > 0 ? static_cast<std::size_t>(available) : 0U;
    }

    Status write(const std::uint8_t* data, const std::size_t size,
                 std::size_t& written) noexcept override {
        written = 0;
        if (data == nullptr) {
            return makeStatus(StatusCode::InvalidArgument, InvalidComponentId);
        }
        const std::size_t limit = writable();
        const std::size_t toWrite = size < limit ? size : limit;
        if (toWrite == 0) {
            return okStatus();  // Transport voll: partieller Write mit 0 Bytes
        }
        written = stream_.write(data, toWrite);
        return okStatus();
    }

    [[nodiscard]] std::size_t readable() const noexcept override {
        const int available = stream_.available();
        return available > 0 ? static_cast<std::size_t>(available) : 0U;
    }

    Status read(std::uint8_t* data, const std::size_t capacity,
                std::size_t& readCount) noexcept override {
        readCount = 0;
        if (data == nullptr) {
            return makeStatus(StatusCode::InvalidArgument, InvalidComponentId);
        }
        while (readCount < capacity && stream_.available() > 0) {
            const int byte = stream_.read();
            if (byte < 0) {
                break;
            }
            data[readCount] = static_cast<std::uint8_t>(byte);
            ++readCount;
        }
        return okStatus();
    }

private:
    Stream& stream_;
};

}  // namespace mea

#endif  // ARDUINO
