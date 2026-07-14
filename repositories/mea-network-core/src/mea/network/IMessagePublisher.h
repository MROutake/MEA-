#pragma once

/// @file IMessagePublisher.h
/// @brief Abstraktion des Sende-/Pump-Pfads für Protokollnachrichten. Erlaubt
///        protokoll-bewusste Sinks (z. B. NetworkMeasurementSink) ohne
///        Template-Kopplung an eine konkrete ProtocolBridge-Instanz.

#include <MeaCore.h>

#include <mea/protocol/MessageEnvelope.h>

namespace mea {
namespace network {

class IMessagePublisher {
public:
    virtual ~IMessagePublisher() = default;

    /// Legt @p envelope zum Senden in den TX-Puffer. WouldBlock, wenn kein
    /// Platz ist (der Frame wird verworfen und gezählt – kein stiller Verlust).
    virtual Status publish(const protocol::MessageEnvelope& envelope) noexcept = 0;

    /// Pumpt Verbindung und I/O nicht blockierend (Session + TX/RX).
    virtual Status update(TimestampMs nowMs) noexcept = 0;
};

}  // namespace network
}  // namespace mea
