#pragma once

/// @file FakeWs500Client.h
/// @brief IWs500Client-Fake für native Tests: steuerbarer Verbindungsaufbau,
///        Loopback, simulierter Verbindungsverlust und injizierbare Fehler.

#include <cstddef>
#include <cstdint>

#include "../IWs500Client.h"

namespace mea {
namespace network {
namespace ws500 {
namespace testing {

class FakeWs500Client final : public IWs500Client {
public:
    static constexpr std::size_t kBufferSize = 1024;

    Ws500Result begin(const Ws500Config&) noexcept override {
        ++beginCalls;
        connected_ = false;
        connecting_ = false;
        faulted_ = failBegin;
        outputLength = 0;
        inputLength_ = 0;
        inputPosition_ = 0;
        return failBegin ? Ws500Result::HardwareFault : Ws500Result::Ok;
    }

    Ws500Result startConnect(TimestampMs) noexcept override {
        ++connectCalls;
        if (failConnect) {
            return Ws500Result::NotReady;
        }
        connecting_ = true;
        if (connectImmediately) {
            connected_ = true;
        }
        return Ws500Result::Ok;
    }

    [[nodiscard]] bool connected() const noexcept override { return connected_; }
    [[nodiscard]] bool faulted() const noexcept override { return faulted_; }

    void close() noexcept override {
        connected_ = false;
        connecting_ = false;
    }

    [[nodiscard]] std::size_t writable() const noexcept override {
        if (!connected_) {
            return 0;
        }
        const std::size_t remaining = kBufferSize - outputLength;
        return writableLimit < remaining ? writableLimit : remaining;
    }

    Ws500Result send(const std::uint8_t* data, const std::size_t size,
                     std::size_t& sent) noexcept override {
        sent = 0;
        if (failSend) {
            return Ws500Result::HardwareFault;
        }
        if (!connected_) {
            return Ws500Result::Disconnected;
        }
        const std::size_t limit = writable();
        const std::size_t toWrite = size < limit ? size : limit;
        for (std::size_t index = 0; index < toWrite; ++index) {
            output[outputLength] = data[index];
            ++outputLength;
            if (loopback && inputLength_ < kBufferSize) {
                input_[inputLength_] = data[index];
                ++inputLength_;
            }
        }
        sent = toWrite;
        return Ws500Result::Ok;
    }

    [[nodiscard]] std::size_t readable() const noexcept override {
        return connected_ ? inputLength_ - inputPosition_ : 0U;
    }

    Ws500Result recv(std::uint8_t* data, const std::size_t capacity,
                     std::size_t& received) noexcept override {
        received = 0;
        if (failRecv) {
            return Ws500Result::HardwareFault;
        }
        if (!connected_) {
            return Ws500Result::Disconnected;
        }
        while (received < capacity && inputPosition_ < inputLength_) {
            data[received] = input_[inputPosition_];
            ++received;
            ++inputPosition_;
        }
        return Ws500Result::Ok;
    }

    // -------- Test-Steuerung --------
    void completeConnect() noexcept { connected_ = true; }
    void dropConnection() noexcept {
        connected_ = false;
        connecting_ = false;
    }
    void injectFault() noexcept { faulted_ = true; }
    void feedInput(const std::uint8_t* data, const std::size_t size) noexcept {
        for (std::size_t index = 0; index < size && inputLength_ < kBufferSize; ++index) {
            input_[inputLength_] = data[index];
            ++inputLength_;
        }
    }
    void clearOutput() noexcept { outputLength = 0; }

    // Konfiguration
    bool failBegin{false};
    bool failConnect{false};
    bool failSend{false};
    bool failRecv{false};
    bool loopback{false};
    bool connectImmediately{true};
    std::size_t writableLimit{kBufferSize};

    // Beobachtung
    std::uint32_t beginCalls{0};
    std::uint32_t connectCalls{0};
    std::uint8_t output[kBufferSize]{};
    std::size_t outputLength{0};

private:
    bool connected_{false};
    bool connecting_{false};
    bool faulted_{false};
    std::uint8_t input_[kBufferSize]{};
    std::size_t inputLength_{0};
    std::size_t inputPosition_{0};
};

}  // namespace testing
}  // namespace ws500
}  // namespace network
}  // namespace mea
