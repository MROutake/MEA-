#include "Ws500Transport.h"

#include "Ws500Result.h"

namespace mea {
namespace network {
namespace ws500 {

Ws500Transport::Ws500Transport(IWs500Client& client, const Ws500Config& config) noexcept
    : client_(client), config_(config) {}

Status Ws500Transport::begin() noexcept {
    initialized_ = false;
    state_ = LinkState::Down;
    if (config_.id == InvalidComponentId) {
        lastStatus_ = makeStatus(StatusCode::InvalidConfiguration, InvalidComponentId);
        return lastStatus_;
    }
    const Ws500Result result = client_.begin(config_);
    lastStatus_ = statusFromWs500(result, config_.id);
    if (!lastStatus_.ok()) {
        return lastStatus_;
    }
    initialized_ = true;
    return okStatus();
}

Status Ws500Transport::connect(const TimestampMs nowMs) noexcept {
    if (!initialized_) {
        lastStatus_ = makeStatus(StatusCode::NotInitialized, config_.id);
        return lastStatus_;
    }
    connectStartMs_ = nowMs;
    const Ws500Result result = client_.startConnect(nowMs);
    lastStatus_ = statusFromWs500(result, config_.id);
    if (!lastStatus_.ok() && !lastStatus_.transient()) {
        state_ = LinkState::Error;
        return lastStatus_;
    }
    state_ = LinkState::Connecting;
    return okStatus();
}

Status Ws500Transport::disconnect() noexcept {
    client_.close();
    state_ = LinkState::Down;
    lastStatus_ = okStatus();
    return okStatus();
}

LinkState Ws500Transport::linkState() const noexcept {
    if (client_.faulted()) {
        return LinkState::Error;
    }
    return state_;
}

Status Ws500Transport::update(const TimestampMs nowMs) noexcept {
    if (!initialized_) {
        return makeStatus(StatusCode::NotInitialized, config_.id);
    }
    if (client_.faulted()) {
        state_ = LinkState::Error;
        lastStatus_ = statusFromWs500(Ws500Result::HardwareFault, config_.id);
        return lastStatus_;
    }

    switch (state_) {
        case LinkState::Connecting:
            if (client_.connected()) {
                state_ = LinkState::Up;
                lastStatus_ = okStatus();
            } else if (intervalElapsed(nowMs, connectStartMs_,
                                       config_.connectTimeoutMs)) {
                client_.close();
                state_ = LinkState::Error;
                lastStatus_ = statusFromWs500(Ws500Result::Timeout, config_.id);
            }
            break;
        case LinkState::Up:
            if (!client_.connected()) {
                client_.close();
                state_ = LinkState::Error;
                lastStatus_ = statusFromWs500(Ws500Result::Disconnected, config_.id);
            }
            break;
        case LinkState::Down:
        case LinkState::Error:
            break;
    }
    return lastStatus_;
}

bool Ws500Transport::isUsable() const noexcept {
    return initialized_ && state_ == LinkState::Up && !client_.faulted();
}

std::size_t Ws500Transport::writable() const noexcept {
    return isUsable() ? client_.writable() : 0U;
}

Status Ws500Transport::write(const std::uint8_t* data, const std::size_t size,
                             std::size_t& written) noexcept {
    written = 0;
    if (data == nullptr) {
        return makeStatus(StatusCode::InvalidArgument, config_.id);
    }
    if (!isUsable()) {
        return okStatus();  // nicht verbunden: nichts geschrieben (kein Fehler)
    }
    std::size_t sent = 0;
    const Ws500Result result = client_.send(data, size, sent);
    written = sent;
    lastStatus_ = statusFromWs500(result, config_.id);
    if (!lastStatus_.ok() && !lastStatus_.transient()) {
        state_ = LinkState::Error;  // harter Fehler -> Session baut neu auf
    }
    return lastStatus_;
}

std::size_t Ws500Transport::readable() const noexcept {
    return isUsable() ? client_.readable() : 0U;
}

Status Ws500Transport::read(std::uint8_t* data, const std::size_t capacity,
                            std::size_t& readCount) noexcept {
    readCount = 0;
    if (data == nullptr) {
        return makeStatus(StatusCode::InvalidArgument, config_.id);
    }
    if (!isUsable()) {
        return okStatus();
    }
    std::size_t received = 0;
    const Ws500Result result = client_.recv(data, capacity, received);
    readCount = received;
    lastStatus_ = statusFromWs500(result, config_.id);
    if (!lastStatus_.ok() && !lastStatus_.transient()) {
        state_ = LinkState::Error;
    }
    return lastStatus_;
}

}  // namespace ws500
}  // namespace network
}  // namespace mea
