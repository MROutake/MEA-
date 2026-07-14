#pragma once

/// @file MessageKind.h
/// @brief Nachrichtenarten des MEA-Netzprotokolls (siehe PROTOCOL-SPEC §3).

#include <cstddef>
#include <cstdint>

namespace mea {
namespace protocol {

/// Art einer Protokollnachricht. Der Wert steht im Header-Feld `kind`.
enum class MessageKind : std::uint8_t {
    Unknown = 0,
    Measurement,      ///< Messwert (Abbild von mea::Measurement)
    OutputState,      ///< Ausgangszustand eines IC-/Treibers
    StateTransition,  ///< Pipeline-/Orchestrator-Zustandsübergang
    ErrorEvent,       ///< Abbild von mea::Status
    Heartbeat,        ///< Lebenszeichen eines Node
    Command           ///< Optional: eingehendes Kommando (mea::Command)
};

/// Kleinster/größter gültiger Kind-Wert (ohne Unknown).
constexpr std::uint8_t kFirstMessageKind =
    static_cast<std::uint8_t>(MessageKind::Measurement);
constexpr std::uint8_t kLastMessageKind =
    static_cast<std::uint8_t>(MessageKind::Command);

/// True, wenn @p kind ein bekannter, versendbarer Typ ist (nicht Unknown).
[[nodiscard]] constexpr bool isKnownKind(const MessageKind kind) noexcept {
    const std::uint8_t value = static_cast<std::uint8_t>(kind);
    return value >= kFirstMessageKind && value <= kLastMessageKind;
}

/// Erwartete Payload-Länge in Byte je Kind (0, wenn unbekannt).
[[nodiscard]] constexpr std::size_t payloadLengthFor(const MessageKind kind) noexcept {
    switch (kind) {
        case MessageKind::Measurement:
            return 18U;
        case MessageKind::OutputState:
            return 16U;
        case MessageKind::StateTransition:
            return 12U;
        case MessageKind::ErrorEvent:
            return 12U;
        case MessageKind::Heartbeat:
            return 14U;
        case MessageKind::Command:
            return 14U;
        case MessageKind::Unknown:
            return 0U;
    }
    return 0U;
}

/// Menschlich lesbarer Name (statisches Stringliteral).
[[nodiscard]] constexpr const char* messageKindName(const MessageKind kind) noexcept {
    switch (kind) {
        case MessageKind::Unknown:
            return "Unknown";
        case MessageKind::Measurement:
            return "Measurement";
        case MessageKind::OutputState:
            return "OutputState";
        case MessageKind::StateTransition:
            return "StateTransition";
        case MessageKind::ErrorEvent:
            return "ErrorEvent";
        case MessageKind::Heartbeat:
            return "Heartbeat";
        case MessageKind::Command:
            return "Command";
    }
    return "Unknown";
}

/// Bitmaske erlaubter Kinds für die ComponentRegistry.
[[nodiscard]] constexpr std::uint16_t kindBit(const MessageKind kind) noexcept {
    return static_cast<std::uint16_t>(1U << static_cast<std::uint8_t>(kind));
}

}  // namespace protocol
}  // namespace mea
