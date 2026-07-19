#pragma once

/// @file ArduinoEspNowRadio.h
/// @brief IEspNowRadio-Implementierung für ESP32 (esp_now + WiFi-STA).
///        Einzige hardwareabhängige Klasse dieser Library; nur unter ESP32
///        übersetzt.
///
/// - Es darf nur EINE Instanz existieren (ESP-NOW-Callbacks sind global).
/// - Der Empfangs-Callback läuft im WiFi-Task; Pakete werden über eine mit
///   Spinlock geschützte Queue in den Aufruferkontext übergeben.
/// - Peers werden beim Senden automatisch angelegt (Kanal 0 = aktueller
///   Kanal); removePeer() gibt Einträge wieder frei (Hardware-Limit ~20).
/// - Unverschlüsselt (v1); PMK/LMK sind bewusst außerhalb des Umfangs.

#ifdef ESP32

#include <cstddef>
#include <cstdint>

#include <freertos/FreeRTOS.h>

#include <MeaCore.h>

#include "EspNowTypes.h"
#include "IEspNowRadio.h"

namespace mea {

class ArduinoEspNowRadio final : public IEspNowRadio {
public:
    /// Kapazität der Empfangsqueue (RAM: 8 * sizeof(EspNowPacket) ≈ 2 KB).
    static constexpr std::size_t kReceiveQueueCapacity = 8;

    /// @param maximumChannel Höchster Scan-/Arbeitskanal (EU 13, USA 11).
    explicit ArduinoEspNowRadio(std::uint8_t maximumChannel = 13) noexcept;
    ~ArduinoEspNowRadio() override;

    /// WiFi in den STA-Modus versetzen und ESP-NOW initialisieren.
    /// InvalidConfiguration, wenn bereits eine andere Instanz aktiv ist.
    Status begin() noexcept override;

    Status setChannel(std::uint8_t channel) noexcept override;
    [[nodiscard]] std::uint8_t channel() const noexcept override { return channel_; }
    [[nodiscard]] std::uint8_t maximumChannel() const noexcept override {
        return maximumChannel_;
    }
    [[nodiscard]] MacAddress localAddress() const noexcept override;

    Status send(const MacAddress& destination, const std::uint8_t* data,
                std::size_t size) noexcept override;
    Status removePeer(const MacAddress& address) noexcept override;

    [[nodiscard]] std::size_t available() const noexcept override;
    Status receive(EspNowPacket& output) noexcept override;

    /// Diagnose: wegen voller Queue verworfene Empfangspakete.
    [[nodiscard]] std::uint32_t droppedPackets() const noexcept {
        return droppedPackets_;
    }

    /// Nur für den globalen ESP-NOW-Callback gedacht (kein API-Bestandteil).
    [[nodiscard]] static ArduinoEspNowRadio* instanceForCallback() noexcept {
        return instance_;
    }
    /// Nur für den globalen ESP-NOW-Callback gedacht (WiFi-Task-Kontext).
    void dispatchReceive(const std::uint8_t* sourceMac, const std::uint8_t* data,
                         const int length) noexcept {
        handleReceive(sourceMac, data, length);
    }

private:
    Status ensurePeer(const MacAddress& address) noexcept;
    void handleReceive(const std::uint8_t* sourceMac, const std::uint8_t* data,
                       int length) noexcept;

    static ArduinoEspNowRadio* instance_;

    RingBuffer<EspNowPacket, kReceiveQueueCapacity> queue_{};
    mutable portMUX_TYPE queueLock_ = portMUX_INITIALIZER_UNLOCKED;

    std::uint8_t channel_{1};
    std::uint8_t maximumChannel_;
    std::uint32_t droppedPackets_{0};
    bool initialized_{false};
};

}  // namespace mea

#endif  // ESP32
