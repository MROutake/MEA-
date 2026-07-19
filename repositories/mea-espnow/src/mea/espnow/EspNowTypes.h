#pragma once

/// @file EspNowTypes.h
/// @brief Grundtypen und Nachrichtenformat des MEA-ESP-NOW-Protokolls
///        (ADR 0008). ESP-NOW ist datagrammbasiert (max. 250 Bytes je Frame);
///        jede Nachricht trägt einen 4-Byte-Header:
///
///     Byte 0..1: Magic 'M','N'   Byte 2: Protokollversion   Byte 3: Typ
///
/// Discovery/Verbindung (keine hardcodierten MAC-Adressen oder Kanäle):
/// Der Client scannt Kanäle und broadcastet Discover; der Server antwortet
/// mit Offer (Absender-MAC lernt jede Seite aus dem Empfang). Danach folgt
/// ConnectRequest/ConnectAccept; die Verbindung wird per Ping/Pong geführt.
/// Data-Frames transportieren einen kodierten Messwert je Datagramm.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <MeaCore.h>

namespace mea {

/// 6-Byte-MAC-Adresse (trivial kopierbar).
struct MacAddress {
    std::uint8_t bytes[6]{};
};

[[nodiscard]] inline bool operator==(const MacAddress& lhs, const MacAddress& rhs) noexcept {
    return std::memcmp(lhs.bytes, rhs.bytes, sizeof(lhs.bytes)) == 0;
}

[[nodiscard]] inline bool operator!=(const MacAddress& lhs, const MacAddress& rhs) noexcept {
    return !(lhs == rhs);
}

/// ESP-NOW-Broadcast-Adresse (alle Geräte auf dem aktuellen Kanal).
constexpr MacAddress kBroadcastAddress{{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

/// Nachrichtentypen des Protokolls.
enum class EspNowMessageType : std::uint8_t {
    Discover = 1,       ///< Client -> Broadcast: sucht einen Server
    Offer = 2,          ///< Server -> Client: "ich bin hier"
    ConnectRequest = 3, ///< Client -> Server: Verbindung anfragen
    ConnectAccept = 4,  ///< Server -> Client: Verbindung bestätigt
    Ping = 5,           ///< Client -> Server: Heartbeat
    Pong = 6,           ///< Server -> Client: Heartbeat-Antwort
    Data = 7            ///< Client -> Server: kodierter Messwert
};

/// Maximale ESP-NOW-Framegröße (Hardware-Limit).
constexpr std::size_t kEspNowMaxFrameSize = 250;
/// Protokoll-Header: Magic (2) + Version (1) + Typ (1).
constexpr std::size_t kEspNowHeaderSize = 4;
/// Maximale Nutzlast eines Data-Frames.
constexpr std::size_t kEspNowMaxPayload = kEspNowMaxFrameSize - kEspNowHeaderSize;

constexpr std::uint8_t kEspNowMagic0 = 'M';
constexpr std::uint8_t kEspNowMagic1 = 'N';
constexpr std::uint8_t kEspNowProtocolVersion = 1;

/// Schreibt den Protokoll-Header nach @p output (mindestens kEspNowHeaderSize
/// Bytes) und liefert die Headerlänge.
inline std::size_t encodeEspNowHeader(std::uint8_t* output,
                                      const EspNowMessageType type) noexcept {
    output[0] = kEspNowMagic0;
    output[1] = kEspNowMagic1;
    output[2] = kEspNowProtocolVersion;
    output[3] = static_cast<std::uint8_t>(type);
    return kEspNowHeaderSize;
}

/// Prüft Magic und Version; true bei gültigem Header (@p type wird gesetzt).
[[nodiscard]] inline bool decodeEspNowHeader(const std::uint8_t* data,
                                             const std::size_t size,
                                             EspNowMessageType& type) noexcept {
    if (data == nullptr || size < kEspNowHeaderSize || data[0] != kEspNowMagic0 ||
        data[1] != kEspNowMagic1 || data[2] != kEspNowProtocolVersion) {
        return false;
    }
    type = static_cast<EspNowMessageType>(data[3]);
    return true;
}

/// Ein empfangenes ESP-NOW-Datagramm (Rohframe inkl. Header).
struct EspNowPacket {
    MacAddress source{};
    std::uint8_t length{0};
    std::uint8_t data[kEspNowMaxFrameSize]{};
};

static_assert(std::is_trivially_copyable<EspNowPacket>::value,
              "EspNowPacket muss trivial kopierbar bleiben (RingBuffer)");

/// Nutzlast eines empfangenen Data-Frames (Server-Seite, ohne Header).
struct EspNowDataFrame {
    MacAddress source{};
    std::uint8_t length{0};
    std::uint8_t payload[kEspNowMaxPayload]{};
};

static_assert(std::is_trivially_copyable<EspNowDataFrame>::value,
              "EspNowDataFrame muss trivial kopierbar bleiben (RingBuffer)");

/// Verbindungs-Sicht für Sender (z. B. EspNowMeasurementSink): implementiert
/// von EspNowClient; für native Tests einfach fakebar.
class IEspNowSession {
public:
    virtual ~IEspNowSession() = default;

    [[nodiscard]] virtual bool connected() const noexcept = 0;

    /// Sendet @p payload als Data-Frame an den verbundenen Server.
    /// WouldBlock, solange keine Verbindung besteht (transient).
    virtual Status sendData(const std::uint8_t* payload, std::size_t size) noexcept = 0;
};

}  // namespace mea
