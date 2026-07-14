#pragma once

/// @file ProtocolBridge.h
/// @brief Verbindet Protokoll-Codec und Transport über eine NetworkSession.
///        TX: feste Byte-FIFO, Frames werden atomar eingestellt (voll → Drop +
///        WouldBlock). RX: Byte-Puffer + decodierte Frame-Queue. Nicht
///        blockierend; kein dynamischer Speicher (ADR 0001).
///
/// Puffer-Strategie (siehe README): publish() kodiert direkt in den TX-Puffer
/// (bei vollem Puffer wird der neue Frame verworfen, bestehende bleiben intakt).
/// update() pumpt bei Online-Verbindung TX→Transport und Transport→RX und
/// decodiert vollständige Frames in die RX-Queue.

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <MeaCore.h>

#include <mea/protocol/IMessageCodec.h>
#include <mea/protocol/MessageEnvelope.h>
#include <mea/protocol/MessageHeader.h>
#include <mea/protocol/MessageKind.h>

#include "IMessagePublisher.h"
#include "INetworkTransport.h"
#include "NetworkMetrics.h"
#include "NetworkSession.h"

namespace mea {
namespace network {

/// @tparam TxCapacity Byte-Kapazität des TX-Puffers (>= kMaxFrameSize).
/// @tparam RxCapacity Byte-Kapazität des RX-Puffers (>= kMaxFrameSize).
/// @tparam RxFrames   Anzahl decodierter Frames, die gepuffert werden (> 0).
template <std::size_t TxCapacity = 512, std::size_t RxCapacity = 512,
          std::size_t RxFrames = 4>
class ProtocolBridge final : public IMessagePublisher {
    static_assert(TxCapacity >= protocol::kMaxFrameSize,
                  "TxCapacity muss mindestens einen maximalen Frame fassen");
    static_assert(RxCapacity >= protocol::kMaxFrameSize,
                  "RxCapacity muss mindestens einen maximalen Frame fassen");
    static_assert(RxFrames > 0, "RxFrames muss > 0 sein");

public:
    ProtocolBridge(NetworkSession& session, INetworkTransport& transport,
                   const protocol::IMessageEncoder& encoder,
                   const protocol::IMessageDecoder& decoder,
                   NetworkMetrics& metrics) noexcept
        : session_(session),
          transport_(transport),
          encoder_(encoder),
          decoder_(decoder),
          metrics_(metrics) {}

    /// Leert die Puffer (reinitialisierend, ADR 0004).
    Status begin() noexcept {
        txHead_ = 0;
        txTail_ = 0;
        rxLen_ = 0;
        rxQueue_.clear();
        lastStatus_ = okStatus();
        initialized_ = true;
        return okStatus();
    }

    Status publish(const protocol::MessageEnvelope& envelope) noexcept override {
        if (!initialized_) {
            return makeStatus(StatusCode::NotInitialized, session_.id());
        }
        if (!protocol::isKnownKind(envelope.header.kind)) {
            return makeStatus(StatusCode::InvalidArgument, envelope.header.componentId);
        }

        compactTx();
        const std::size_t freeSpace = TxCapacity - txHead_;
        std::size_t written = 0;
        Status status = encoder_.encode(envelope, tx_ + txHead_, freeSpace, written);
        if (status.code == StatusCode::CapacityExceeded) {
            ++metrics_.txDropCount;  // TX-Puffer voll: neuen Frame verwerfen
            return makeStatus(StatusCode::WouldBlock, session_.id());
        }
        if (!status.ok()) {
            return status;  // z. B. InvalidArgument
        }
        txHead_ += written;
        ++metrics_.txFrameCount;
        return okStatus();
    }

    /// Entnimmt die nächste decodierte Nachricht. NoData, wenn keine vorliegt.
    Status poll(protocol::MessageEnvelope& envelope) noexcept {
        if (!initialized_) {
            return makeStatus(StatusCode::NotInitialized, session_.id());
        }
        if (!rxQueue_.pop(envelope)) {
            return makeStatus(StatusCode::NoData, session_.id());
        }
        return okStatus();
    }

    Status update(TimestampMs nowMs) noexcept override {
        if (!initialized_) {
            return makeStatus(StatusCode::NotInitialized, session_.id());
        }
        (void)session_.update(nowMs);
        if (session_.isOnline()) {
            pumpTx();
            pumpRx();
            decodeRx();
        }
        return lastStatus_;
    }

    /// @name Diagnose
    /// @{
    [[nodiscard]] std::size_t txPending() const noexcept { return txHead_ - txTail_; }
    [[nodiscard]] std::size_t rxBuffered() const noexcept { return rxLen_; }
    [[nodiscard]] std::size_t rxQueued() const noexcept { return rxQueue_.size(); }
    [[nodiscard]] Status lastStatus() const noexcept { return lastStatus_; }
    /// @}

private:
    /// Schiebt bereits gesendete Bytes aus dem TX-Puffer (schafft Platz).
    void compactTx() noexcept {
        if (txTail_ == 0) {
            return;
        }
        const std::size_t remaining = txHead_ - txTail_;
        if (remaining > 0) {
            std::memmove(tx_, tx_ + txTail_, remaining);
        }
        txHead_ = remaining;
        txTail_ = 0;
    }

    void pumpTx() noexcept {
        while (txTail_ < txHead_) {
            if (transport_.writable() == 0) {
                break;
            }
            std::size_t written = 0;
            Status status = transport_.write(tx_ + txTail_, txHead_ - txTail_, written);
            if (!status.ok()) {
                if (status.origin == InvalidComponentId) {
                    status.origin = session_.id();
                }
                lastStatus_ = status;
                break;
            }
            if (written == 0) {
                break;
            }
            txTail_ += written;
        }
        if (txTail_ == txHead_) {
            txHead_ = 0;
            txTail_ = 0;
        }
    }

    void pumpRx() noexcept {
        while (rxLen_ < RxCapacity) {
            if (transport_.readable() == 0) {
                break;
            }
            std::size_t readCount = 0;
            Status status = transport_.read(rx_ + rxLen_, RxCapacity - rxLen_, readCount);
            if (!status.ok()) {
                if (status.origin == InvalidComponentId) {
                    status.origin = session_.id();
                }
                lastStatus_ = status;
                break;
            }
            if (readCount == 0) {
                break;
            }
            rxLen_ += readCount;
        }
    }

    void consumeRx(const std::size_t count) noexcept {
        if (count >= rxLen_) {
            rxLen_ = 0;
            return;
        }
        std::memmove(rx_, rx_ + count, rxLen_ - count);
        rxLen_ -= count;
    }

    void decodeRx() noexcept {
        while (rxLen_ > 0 && !rxQueue_.full()) {
            protocol::MessageEnvelope envelope{};
            std::size_t consumed = 0;
            const Status status = decoder_.decode(rx_, rxLen_, envelope, consumed);
            if (status.ok()) {
                (void)rxQueue_.push(envelope);
                ++metrics_.rxFrameCount;
                consumeRx(consumed);
            } else if (status.code == StatusCode::NoData) {
                break;  // unvollständig: auf mehr Bytes warten
            } else {
                ++metrics_.rxErrorCount;
                consumeRx(consumed == 0 ? 1U : consumed);
            }
        }
    }

    NetworkSession& session_;
    INetworkTransport& transport_;
    const protocol::IMessageEncoder& encoder_;
    const protocol::IMessageDecoder& decoder_;
    NetworkMetrics& metrics_;

    std::uint8_t tx_[TxCapacity]{};
    std::size_t txHead_{0};  ///< belegte Länge
    std::size_t txTail_{0};  ///< bereits gesendete Länge

    std::uint8_t rx_[RxCapacity]{};
    std::size_t rxLen_{0};
    RingBuffer<protocol::MessageEnvelope, RxFrames> rxQueue_{};

    Status lastStatus_{okStatus()};
    bool initialized_{false};
};

}  // namespace network
}  // namespace mea
