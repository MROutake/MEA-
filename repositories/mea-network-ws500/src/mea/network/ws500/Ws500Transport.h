#pragma once

/// @file Ws500Transport.h
/// @brief WS500-Netzwerkadapter: implementiert INetworkTransport (mea-network-
///        core) über einen IWs500Client. Bewegt ausschließlich Bytes –
///        keinerlei Protokoll-Encode/Decode-Logik (harte Architekturregel).
///        Robuster, nicht blockierender Verbindungsaufbau mit Timeout;
///        Fehler werden auf das MEA-Statusmodell abgebildet.

#include <cstddef>
#include <cstdint>

#include <MeaCore.h>

#include <mea/network/INetworkTransport.h>

#include "IWs500Client.h"
#include "Ws500Config.h"

namespace mea {
namespace network {
namespace ws500 {

class Ws500Transport final : public INetworkTransport {
public:
    /// @param client Nicht besitzend; muss den Transport überleben (ADR 0001).
    Ws500Transport(IWs500Client& client, const Ws500Config& config) noexcept;

    [[nodiscard]] ComponentId id() const noexcept override { return config_.id; }

    Status begin() noexcept override;
    Status connect(TimestampMs nowMs) noexcept override;
    Status disconnect() noexcept override;
    [[nodiscard]] LinkState linkState() const noexcept override;
    Status update(TimestampMs nowMs) noexcept override;

    [[nodiscard]] std::size_t writable() const noexcept override;
    Status write(const std::uint8_t* data, std::size_t size,
                 std::size_t& written) noexcept override;
    [[nodiscard]] std::size_t readable() const noexcept override;
    Status read(std::uint8_t* data, std::size_t capacity,
                std::size_t& readCount) noexcept override;

    [[nodiscard]] Status lastStatus() const noexcept { return lastStatus_; }

private:
    [[nodiscard]] bool isUsable() const noexcept;

    IWs500Client& client_;
    Ws500Config config_;
    LinkState state_{LinkState::Down};
    TimestampMs connectStartMs_{0};
    Status lastStatus_{StatusCode::NotInitialized, InvalidComponentId, 0};
    bool initialized_{false};
};

}  // namespace ws500
}  // namespace network
}  // namespace mea
