#include "EspNowClient.h"

#include <cstring>

namespace mea {

EspNowClient::EspNowClient(IEspNowRadio& radio, const Config& config) noexcept
    : radio_(radio), config_(config) {}

Status EspNowClient::begin() noexcept {
    initialized_ = false;
    state_ = State::Uninitialized;
    if (config_.channelDwellMs == 0 || config_.connectTimeoutMs == 0 ||
        config_.pingIntervalMs == 0 || config_.maximumMissedPongs == 0 ||
        config_.firstChannel == 0 ||
        config_.firstChannel > radio_.maximumChannel()) {
        return makeStatus(StatusCode::InvalidConfiguration, InvalidComponentId);
    }

    const Status status = radio_.begin();
    if (!status.ok()) {
        return status;
    }

    scanChannel_ = config_.firstChannel;
    enterScanning();
    initialized_ = true;
    return okStatus();
}

void EspNowClient::enterScanning() noexcept {
    state_ = State::Scanning;
    // Erst den aktuellen Kanal probieren (nach Abriss steht der Server dort
    // vermutlich noch), dann weiterschalten.
    switchChannelNow_ = true;
}

Status EspNowClient::sendControl(const EspNowMessageType type,
                                 const MacAddress& destination) noexcept {
    std::uint8_t frame[kEspNowHeaderSize];
    (void)encodeEspNowHeader(frame, type);
    return radio_.send(destination, frame, kEspNowHeaderSize);
}

Status EspNowClient::update(const TimestampMs nowMs) noexcept {
    if (!initialized_) {
        return makeStatus(StatusCode::NotInitialized, InvalidComponentId);
    }

    drainReceive(nowMs);

    switch (state_) {
        case State::Scanning:
            return updateScanning(nowMs);
        case State::Connecting:
            if (intervalElapsed(nowMs, connectStartMs_, config_.connectTimeoutMs)) {
                enterScanning();  // Server antwortet nicht: weitersuchen
            }
            return okStatus();
        case State::Connected: {
            const TimestampMs livenessMs =
                config_.pingIntervalMs *
                static_cast<TimestampMs>(config_.maximumMissedPongs + 1U);
            if (elapsedMs(nowMs, lastPongMs_) >= livenessMs) {
                ++reconnects_;
                enterScanning();  // Heartbeat ausgefallen: neu verbinden
                return okStatus();
            }
            if (intervalElapsed(nowMs, lastPingSentMs_, config_.pingIntervalMs)) {
                lastPingSentMs_ = nowMs;
                return sendControl(EspNowMessageType::Ping, serverAddress_);
            }
            return okStatus();
        }
        case State::Uninitialized:
            break;
    }
    return makeStatus(StatusCode::InternalError, InvalidComponentId);
}

Status EspNowClient::updateScanning(const TimestampMs nowMs) noexcept {
    const bool dwellElapsed =
        intervalElapsed(nowMs, dwellStartMs_, config_.channelDwellMs);
    if (!switchChannelNow_ && !dwellElapsed) {
        return okStatus();
    }
    if (!switchChannelNow_) {
        scanChannel_ =
            static_cast<std::uint8_t>(scanChannel_ >= radio_.maximumChannel()
                                          ? 1
                                          : scanChannel_ + 1U);
    }
    switchChannelNow_ = false;
    dwellStartMs_ = nowMs;

    const Status channelStatus = radio_.setChannel(scanChannel_);
    if (!channelStatus.ok()) {
        return channelStatus;
    }
    return sendControl(EspNowMessageType::Discover, kBroadcastAddress);
}

void EspNowClient::drainReceive(const TimestampMs nowMs) noexcept {
    EspNowPacket packet{};
    while (radio_.available() > 0 && radio_.receive(packet).ok()) {
        EspNowMessageType type{};
        if (!decodeEspNowHeader(packet.data, packet.length, type)) {
            ++protocolErrors_;
            continue;
        }

        switch (state_) {
            case State::Scanning:
                if (type == EspNowMessageType::Offer) {
                    serverAddress_ = packet.source;
                    connectStartMs_ = nowMs;
                    state_ = State::Connecting;
                    (void)sendControl(EspNowMessageType::ConnectRequest,
                                      serverAddress_);
                }
                break;
            case State::Connecting:
                if (type == EspNowMessageType::ConnectAccept &&
                    packet.source == serverAddress_) {
                    state_ = State::Connected;
                    lastPongMs_ = nowMs;
                    lastPingSentMs_ = nowMs;
                }
                break;
            case State::Connected:
                if (type == EspNowMessageType::Pong &&
                    packet.source == serverAddress_) {
                    lastPongMs_ = nowMs;
                }
                break;
            case State::Uninitialized:
                break;
        }
    }
}

Status EspNowClient::sendData(const std::uint8_t* payload,
                              const std::size_t size) noexcept {
    if (!initialized_) {
        return makeStatus(StatusCode::NotInitialized, InvalidComponentId);
    }
    if (payload == nullptr || size == 0) {
        return makeStatus(StatusCode::InvalidArgument, InvalidComponentId);
    }
    if (size > kEspNowMaxPayload) {
        return makeStatus(StatusCode::CapacityExceeded, InvalidComponentId,
                          static_cast<std::uint16_t>(size));
    }
    if (state_ != State::Connected) {
        return makeStatus(StatusCode::WouldBlock, InvalidComponentId);
    }

    std::uint8_t frame[kEspNowMaxFrameSize];
    (void)encodeEspNowHeader(frame, EspNowMessageType::Data);
    std::memcpy(frame + kEspNowHeaderSize, payload, size);

    const Status status = radio_.send(serverAddress_, frame, kEspNowHeaderSize + size);
    if (status.ok()) {
        ++sentDataFrames_;
    } else {
        ++sendFailures_;
    }
    return status;
}

}  // namespace mea
