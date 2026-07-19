#pragma once

/// @file FakeEspNowRadio.h
/// @brief IEspNowRadio-Fake für native Tests: zeichnet gesendete Frames auf,
///        injizierbare Empfangspakete, konfigurierbare Fehler und Zähler.
///        Keine Arduino-Abhängigkeit.

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <MeaCore.h>

#include "../EspNowTypes.h"
#include "../IEspNowRadio.h"

namespace mea::testing {

class FakeEspNowRadio final : public IEspNowRadio {
public:
    struct SentFrame {
        MacAddress destination{};
        std::uint8_t length{0};
        std::uint8_t data[kEspNowMaxFrameSize]{};
    };

    static constexpr std::size_t kMaxSentFrames = 16;

    Status begin() noexcept override {
        ++beginCalls;
        return beginResult;
    }

    Status setChannel(const std::uint8_t newChannel) noexcept override {
        ++setChannelCalls;
        channelValue = newChannel;
        return setChannelResult;
    }

    [[nodiscard]] std::uint8_t channel() const noexcept override {
        return channelValue;
    }
    [[nodiscard]] std::uint8_t maximumChannel() const noexcept override {
        return maximumChannelValue;
    }
    [[nodiscard]] MacAddress localAddress() const noexcept override {
        return localMac;
    }

    Status send(const MacAddress& destination, const std::uint8_t* data,
                const std::size_t size) noexcept override {
        ++sendCalls;
        if (!sendResult.ok()) {
            return sendResult;
        }
        if (sentCount < kMaxSentFrames && size <= kEspNowMaxFrameSize) {
            SentFrame& frame = sent[sentCount];
            frame.destination = destination;
            frame.length = static_cast<std::uint8_t>(size);
            std::memcpy(frame.data, data, size);
            ++sentCount;
        }
        return okStatus();
    }

    Status removePeer(const MacAddress& address) noexcept override {
        ++removePeerCalls;
        lastRemovedPeer = address;
        return okStatus();
    }

    [[nodiscard]] std::size_t available() const noexcept override {
        return receiveQueue.size();
    }

    Status receive(EspNowPacket& output) noexcept override {
        if (!receiveQueue.pop(output)) {
            return makeStatus(StatusCode::NoData, InvalidComponentId);
        }
        return okStatus();
    }

    /// Injiziert eine Steuernachricht (nur Header) von @p source.
    void injectControl(const EspNowMessageType type, const MacAddress& source) noexcept {
        EspNowPacket packet{};
        packet.source = source;
        packet.length = static_cast<std::uint8_t>(kEspNowHeaderSize);
        (void)encodeEspNowHeader(packet.data, type);
        (void)receiveQueue.push(packet);
    }

    /// Injiziert einen Data-Frame mit @p payload von @p source.
    void injectData(const MacAddress& source, const std::uint8_t* payload,
                    const std::size_t size) noexcept {
        EspNowPacket packet{};
        packet.source = source;
        packet.length = static_cast<std::uint8_t>(kEspNowHeaderSize + size);
        (void)encodeEspNowHeader(packet.data, EspNowMessageType::Data);
        std::memcpy(packet.data + kEspNowHeaderSize, payload, size);
        (void)receiveQueue.push(packet);
    }

    /// Injiziert ein Paket mit ungültigem Header.
    void injectGarbage(const MacAddress& source) noexcept {
        EspNowPacket packet{};
        packet.source = source;
        packet.length = 3;
        packet.data[0] = 0x42;
        (void)receiveQueue.push(packet);
    }

    /// Letzter gesendeter Frame eines Typs; nullptr, wenn keiner passt.
    [[nodiscard]] const SentFrame* lastSentOfType(
        const EspNowMessageType type) const noexcept {
        for (std::size_t index = sentCount; index > 0; --index) {
            const SentFrame& frame = sent[index - 1];
            EspNowMessageType frameType{};
            if (decodeEspNowHeader(frame.data, frame.length, frameType) &&
                frameType == type) {
                return &frame;
            }
        }
        return nullptr;
    }

    /// Konfiguration
    Status beginResult{okStatus()};
    Status setChannelResult{okStatus()};
    Status sendResult{okStatus()};
    std::uint8_t maximumChannelValue{13};
    MacAddress localMac{{0x10, 0x20, 0x30, 0x40, 0x50, 0x60}};

    /// Zähler und Aufzeichnung
    std::uint32_t beginCalls{0};
    std::uint32_t setChannelCalls{0};
    std::uint32_t sendCalls{0};
    std::uint32_t removePeerCalls{0};
    std::uint8_t channelValue{0};
    MacAddress lastRemovedPeer{};
    SentFrame sent[kMaxSentFrames]{};
    std::size_t sentCount{0};

    RingBuffer<EspNowPacket, 8> receiveQueue{};
};

}  // namespace mea::testing
