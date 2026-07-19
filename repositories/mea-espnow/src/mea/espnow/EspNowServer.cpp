#include "EspNowServer.h"

#include <cstring>

namespace mea {

EspNowServer::EspNowServer(IEspNowRadio& radio, const Config& config) noexcept
    : radio_(radio), config_(config) {}

Status EspNowServer::begin() noexcept {
    initialized_ = false;
    if (config_.channel == 0 || config_.channel > radio_.maximumChannel() ||
        config_.clientTimeoutMs == 0) {
        return makeStatus(StatusCode::InvalidConfiguration, InvalidComponentId);
    }

    Status status = radio_.begin();
    if (!status.ok()) {
        return status;
    }
    status = radio_.setChannel(config_.channel);
    if (!status.ok()) {
        return status;
    }

    for (ClientInfo& client : clients_) {
        client.used = false;
    }
    dataQueue_.clear();
    initialized_ = true;
    return okStatus();
}

Status EspNowServer::sendControl(const EspNowMessageType type,
                                 const MacAddress& destination) noexcept {
    std::uint8_t frame[kEspNowHeaderSize];
    (void)encodeEspNowHeader(frame, type);
    return radio_.send(destination, frame, kEspNowHeaderSize);
}

bool EspNowServer::touchClient(const MacAddress& address,
                               const TimestampMs nowMs) noexcept {
    ClientInfo* freeSlot = nullptr;
    for (ClientInfo& client : clients_) {
        if (client.used && client.address == address) {
            client.lastSeenMs = nowMs;
            return true;
        }
        if (!client.used && freeSlot == nullptr) {
            freeSlot = &client;
        }
    }
    if (freeSlot == nullptr) {
        ++rejectedClients_;
        return false;
    }
    freeSlot->address = address;
    freeSlot->lastSeenMs = nowMs;
    freeSlot->used = true;
    return true;
}

void EspNowServer::expireClients(const TimestampMs nowMs) noexcept {
    for (ClientInfo& client : clients_) {
        if (client.used &&
            elapsedMs(nowMs, client.lastSeenMs) >= config_.clientTimeoutMs) {
            client.used = false;
            ++expiredClients_;
            (void)radio_.removePeer(client.address);
        }
    }
}

void EspNowServer::handlePacket(const EspNowPacket& packet,
                                const EspNowMessageType type,
                                const TimestampMs nowMs) noexcept {
    switch (type) {
        case EspNowMessageType::Discover:
            (void)sendControl(EspNowMessageType::Offer, packet.source);
            break;
        case EspNowMessageType::ConnectRequest:
            if (touchClient(packet.source, nowMs)) {
                (void)sendControl(EspNowMessageType::ConnectAccept, packet.source);
            }
            break;
        case EspNowMessageType::Ping:
            // Unbekannte Absender implizit wieder aufnehmen (Server-Neustart).
            if (touchClient(packet.source, nowMs)) {
                (void)sendControl(EspNowMessageType::Pong, packet.source);
            }
            break;
        case EspNowMessageType::Data: {
            if (!touchClient(packet.source, nowMs)) {
                break;
            }
            EspNowDataFrame frame{};
            frame.source = packet.source;
            frame.length =
                static_cast<std::uint8_t>(packet.length - kEspNowHeaderSize);
            std::memcpy(frame.payload, packet.data + kEspNowHeaderSize, frame.length);
            if (!dataQueue_.push(frame)) {
                ++droppedFrames_;  // Drop-Policy: neuen Frame verwerfen
            }
            break;
        }
        case EspNowMessageType::Offer:
        case EspNowMessageType::ConnectAccept:
        case EspNowMessageType::Pong:
            break;  // Server-Nachrichten anderer Server: ignorieren
    }
}

Status EspNowServer::update(const TimestampMs nowMs) noexcept {
    if (!initialized_) {
        return makeStatus(StatusCode::NotInitialized, InvalidComponentId);
    }

    EspNowPacket packet{};
    while (radio_.available() > 0 && radio_.receive(packet).ok()) {
        EspNowMessageType type{};
        if (!decodeEspNowHeader(packet.data, packet.length, type)) {
            ++protocolErrors_;
            continue;
        }
        handlePacket(packet, type, nowMs);
    }

    expireClients(nowMs);
    return okStatus();
}

std::size_t EspNowServer::clientCount() const noexcept {
    std::size_t count = 0;
    for (const ClientInfo& client : clients_) {
        if (client.used) {
            ++count;
        }
    }
    return count;
}

Status EspNowServer::read(EspNowDataFrame& output) noexcept {
    if (!initialized_) {
        return makeStatus(StatusCode::NotInitialized, InvalidComponentId);
    }
    if (!dataQueue_.pop(output)) {
        return makeStatus(StatusCode::NoData, InvalidComponentId);
    }
    return okStatus();
}

}  // namespace mea
