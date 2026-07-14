#pragma once

/// @file FakeByteTransport.h
/// @brief IByteTransport-Fake für native Tests: begrenzte Puffer, einstellbares
///        Schreiblimit (partielle Writes), injizierbare Fehler.

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "../IByteTransport.h"

namespace mea::testing {

class FakeByteTransport final : public IByteTransport {
public:
    static constexpr std::size_t kBufferSize = 512;

    Status begin() noexcept override {
        ++beginCalls;
        return beginResult;
    }

    Status update(TimestampMs) noexcept override { return okStatus(); }

    [[nodiscard]] std::size_t writable() const noexcept override {
        const std::size_t remaining = kBufferSize - outputLength;
        return writableLimit < remaining ? writableLimit : remaining;
    }

    Status write(const std::uint8_t* data, const std::size_t size,
                 std::size_t& written) noexcept override {
        written = 0;
        if (failWrites) {
            return writeResult;
        }
        const std::size_t limit = writable();
        const std::size_t toWrite = size < limit ? size : limit;
        for (std::size_t index = 0; index < toWrite; ++index) {
            output[outputLength] = data[index];
            ++outputLength;
        }
        written = toWrite;
        return okStatus();
    }

    [[nodiscard]] std::size_t readable() const noexcept override {
        return inputLength - inputPosition;
    }

    Status read(std::uint8_t* data, const std::size_t capacity,
                std::size_t& readCount) noexcept override {
        readCount = 0;
        if (failReads) {
            return readResult;
        }
        while (readCount < capacity && inputPosition < inputLength) {
            data[readCount] = input[inputPosition];
            ++readCount;
            ++inputPosition;
        }
        return okStatus();
    }

    /// Stellt Eingabedaten für read() bereit (z. B. eingehende Kommandozeilen).
    void feedInput(const char* text) noexcept {
        for (const char* current = text; *current != '\0' && inputLength < kBufferSize;
             ++current) {
            input[inputLength] = static_cast<std::uint8_t>(*current);
            ++inputLength;
        }
    }

    /// Ausgabe als nullterminierter Text (für String-Vergleiche in Tests).
    [[nodiscard]] const char* outputText() noexcept {
        output[outputLength] = 0U;
        return reinterpret_cast<const char*>(output);
    }

    void clearOutput() noexcept { outputLength = 0; }

    std::size_t writableLimit{kBufferSize};
    bool failWrites{false};
    bool failReads{false};
    Status beginResult{okStatus()};
    Status writeResult{makeStatus(StatusCode::IoError, InvalidComponentId, 0)};
    Status readResult{makeStatus(StatusCode::IoError, InvalidComponentId, 0)};
    std::uint32_t beginCalls{0};

    std::uint8_t output[kBufferSize + 1]{};
    std::size_t outputLength{0};

private:
    std::uint8_t input[kBufferSize]{};
    std::size_t inputLength{0};
    std::size_t inputPosition{0};
};

}  // namespace mea::testing
