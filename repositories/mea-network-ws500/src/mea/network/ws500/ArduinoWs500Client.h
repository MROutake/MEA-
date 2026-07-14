#pragma once

/// @file ArduinoWs500Client.h
/// @brief Referenz-IWs500Client über einen Arduino-`Client` (z. B. EthernetClient
///        eines WS500/W5500-Moduls oder WiFiClient). Einzige Arduino-abhängige
///        Klasse dieser Library; nur unter ARDUINO übersetzt und bewusst als
///        Template gehalten, um keine feste Netzwerk-Library zu erzwingen.
///
/// Bekannte Einschränkung: die Arduino-`Client::connect(IPAddress, port)`-API
/// blockiert je nach Stack kurz. Für streng nicht blockierenden Aufbau einen
/// modul-spezifischen Client mit asynchronem connect implementieren
/// (siehe README, "Offene Punkte").

#ifdef ARDUINO

#include <Arduino.h>
#include <IPAddress.h>

#include <cstddef>
#include <cstdint>

#include "IWs500Client.h"

namespace mea {
namespace network {
namespace ws500 {

/// @tparam ClientT Arduino-`Client`-kompatibler Typ mit:
///   int connect(IPAddress, uint16_t); uint8_t connected();
///   int available(); int read(uint8_t*, size_t); size_t write(const uint8_t*, size_t);
///   void stop();
template <typename ClientT>
class ArduinoWs500Client final : public IWs500Client {
public:
    /// @param client Nicht besitzend; muss den Adapter überleben (ADR 0001).
    /// @param sendChunk Max. Bytes je send()-Aufruf (Obergrenze der Arbeit).
    explicit ArduinoWs500Client(ClientT& client,
                                const std::size_t sendChunk = 256) noexcept
        : client_(client), sendChunk_(sendChunk) {}

    Ws500Result begin(const Ws500Config& config) noexcept override {
        config_ = config;
        faulted_ = false;
        return Ws500Result::Ok;
    }

    Ws500Result startConnect(TimestampMs) noexcept override {
        const IPAddress ip(config_.host[0], config_.host[1], config_.host[2],
                           config_.host[3]);
        const int result = client_.connect(ip, config_.port);
        if (result == 1) {
            return Ws500Result::Ok;
        }
        // 0/negativ: kein Verbindungsaufbau möglich.
        faulted_ = false;
        return Ws500Result::NotReady;
    }

    [[nodiscard]] bool connected() const noexcept override {
        return client_.connected() != 0;
    }
    [[nodiscard]] bool faulted() const noexcept override { return faulted_; }

    void close() noexcept override { client_.stop(); }

    [[nodiscard]] std::size_t writable() const noexcept override {
        return client_.connected() != 0 ? sendChunk_ : 0U;
    }

    Ws500Result send(const std::uint8_t* data, const std::size_t size,
                     std::size_t& sent) noexcept override {
        sent = 0;
        if (client_.connected() == 0) {
            return Ws500Result::Disconnected;
        }
        const std::size_t toWrite = size < sendChunk_ ? size : sendChunk_;
        const std::size_t written = client_.write(data, toWrite);
        sent = written;
        return Ws500Result::Ok;
    }

    [[nodiscard]] std::size_t readable() const noexcept override {
        const int available = client_.available();
        return available > 0 ? static_cast<std::size_t>(available) : 0U;
    }

    Ws500Result recv(std::uint8_t* data, const std::size_t capacity,
                     std::size_t& received) noexcept override {
        received = 0;
        if (client_.connected() == 0) {
            return Ws500Result::Disconnected;
        }
        const int count = client_.read(data, capacity);
        if (count > 0) {
            received = static_cast<std::size_t>(count);
        }
        return Ws500Result::Ok;
    }

private:
    ClientT& client_;
    Ws500Config config_{};
    std::size_t sendChunk_;
    bool faulted_{false};
};

}  // namespace ws500
}  // namespace network
}  // namespace mea

#endif  // ARDUINO
