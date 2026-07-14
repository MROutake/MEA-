#pragma once

/// @file FakeNetworkTransport.h
/// @brief INetworkTransport-Fake für native Tests: simulierter
///        Verbindungsaufbau (Erfolg/Fehler/Timeout), Loopback-Modus,
///        Verbindungsabbruch und injizierbare I/O-Fehler.

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include "../INetworkTransport.h"

namespace mea {
namespace network {
namespace testing {

class FakeNetworkTransport final : public INetworkTransport {
public:
    static constexpr std::size_t kBufferSize = 1024;

    explicit FakeNetworkTransport(const ComponentId id) noexcept : id_(id) {}

    [[nodiscard]] ComponentId id() const noexcept override { return id_; }

    Status begin() noexcept override {
        ++beginCalls;
        state_ = LinkState::Down;
        return beginResult;
    }

    Status connect(const TimestampMs nowMs) noexcept override {
        ++connectCalls;
        if (failConnect) {
            state_ = LinkState::Error;
            return okStatus();  // Fehler wird über linkState() sichtbar
        }
        state_ = LinkState::Connecting;
        connectAtMs_ = nowMs;
        return okStatus();
    }

    Status disconnect() noexcept override {
        state_ = LinkState::Down;
        return okStatus();
    }

    [[nodiscard]] LinkState linkState() const noexcept override { return state_; }

    Status update(const TimestampMs nowMs) noexcept override {
        if (state_ == LinkState::Connecting &&
            intervalElapsed(nowMs, connectAtMs_, connectDelayMs)) {
            state_ = LinkState::Up;
        }
        return okStatus();
    }

    [[nodiscard]] std::size_t writable() const noexcept override {
        if (state_ != LinkState::Up) {
            return 0;
        }
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
            if (loopback && inputLength < kBufferSize) {
                input[inputLength] = data[index];
                ++inputLength;
            }
        }
        written = toWrite;
        return okStatus();
    }

    [[nodiscard]] std::size_t readable() const noexcept override {
        if (state_ != LinkState::Up) {
            return 0;
        }
        return inputLength - inputPosition;
    }

    Status read(std::uint8_t* data, const std::size_t capacity,
                std::size_t& readCount) noexcept override {
        readCount = 0;
        if (failReads) {
            return readResult;
        }
        if (state_ != LinkState::Up) {
            return okStatus();
        }
        while (readCount < capacity && inputPosition < inputLength) {
            data[readCount] = input[inputPosition];
            ++readCount;
            ++inputPosition;
        }
        return okStatus();
    }

    // -------- Test-Steuerung --------

    /// Simuliert den Verlust einer bestehenden Verbindung.
    void dropLink() noexcept { state_ = LinkState::Error; }

    /// Stellt Empfangsdaten bereit (Rohbytes).
    void feedInput(const std::uint8_t* data, const std::size_t size) noexcept {
        for (std::size_t index = 0; index < size && inputLength < kBufferSize; ++index) {
            input[inputLength] = data[index];
            ++inputLength;
        }
    }

    void clearOutput() noexcept { outputLength = 0; }

    // Konfiguration
    bool failConnect{false};
    bool failWrites{false};
    bool failReads{false};
    bool loopback{false};
    TimestampMs connectDelayMs{0};
    std::size_t writableLimit{kBufferSize};
    Status beginResult{okStatus()};
    Status writeResult{makeStatus(StatusCode::IoError, InvalidComponentId, 0)};
    Status readResult{makeStatus(StatusCode::IoError, InvalidComponentId, 0)};

    // Beobachtung
    std::uint32_t beginCalls{0};
    std::uint32_t connectCalls{0};
    std::uint8_t output[kBufferSize]{};
    std::size_t outputLength{0};

private:
    ComponentId id_;
    LinkState state_{LinkState::Down};
    TimestampMs connectAtMs_{0};

    std::uint8_t input[kBufferSize]{};
    std::size_t inputLength{0};
    std::size_t inputPosition{0};
};

}  // namespace testing
}  // namespace network
}  // namespace mea
