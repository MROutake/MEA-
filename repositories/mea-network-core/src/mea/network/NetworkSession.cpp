#include "NetworkSession.h"

namespace mea {
namespace network {

NetworkSession::NetworkSession(INetworkTransport& transport,
                               const ReconnectPolicy& policy, NetworkMetrics& metrics,
                               const ComponentId id) noexcept
    : transport_(transport), policy_(policy), metrics_(metrics), id_(id) {}

Status NetworkSession::begin(const TimestampMs nowMs) noexcept {
    (void)nowMs;
    state_ = State::Disconnected;
    wantOnline_ = false;
    attempts_ = 0;
    backoffMs_ = 0;
    lastStatus_ = okStatus();
    initialized_ = false;

    Status status = transport_.begin();
    if (!status.ok()) {
        if (status.origin == InvalidComponentId) {
            status.origin = id_;
        }
        lastStatus_ = status;
        return status;
    }
    initialized_ = true;
    return okStatus();
}

Status NetworkSession::connect(const TimestampMs nowMs) noexcept {
    if (!initialized_) {
        return makeStatus(StatusCode::NotInitialized, id_);
    }
    wantOnline_ = true;
    attempts_ = 0;
    backoffMs_ = 0;
    enterConnecting(nowMs);
    return lastStatus_;
}

Status NetworkSession::disconnect() noexcept {
    wantOnline_ = false;
    const Status status = transport_.disconnect();
    state_ = State::Disconnected;
    lastStatus_ = okStatus();
    return status;
}

void NetworkSession::enterConnecting(const TimestampMs nowMs) noexcept {
    ++attempts_;
    ++metrics_.connectAttempts;
    state_ = State::Connecting;
    connectStartMs_ = nowMs;
    Status status = transport_.connect(nowMs);
    if (status.origin == InvalidComponentId) {
        status.origin = id_;
    }
    lastStatus_ = status;
}

void NetworkSession::enterOnline(const TimestampMs nowMs) noexcept {
    (void)nowMs;
    state_ = State::Online;
    attempts_ = 0;
    backoffMs_ = 0;
    lastStatus_ = okStatus();
}

void NetworkSession::enterBackoff(const TimestampMs nowMs) noexcept {
    backoffMs_ = nextBackoff(policy_, backoffMs_);
    backoffStartMs_ = nowMs;
    state_ = State::Backoff;
}

void NetworkSession::handleFailure(Status cause, const TimestampMs nowMs) noexcept {
    (void)transport_.disconnect();
    if (cause.origin == InvalidComponentId) {
        cause.origin = id_;
    }
    lastStatus_ = cause;
    if (policy_.maxAttempts != 0 && attempts_ >= policy_.maxAttempts) {
        state_ = State::Fault;
        return;
    }
    enterBackoff(nowMs);
}

Status NetworkSession::update(const TimestampMs nowMs) noexcept {
    if (!initialized_) {
        return makeStatus(StatusCode::NotInitialized, id_);
    }

    switch (state_) {
        case State::Disconnected:
            if (wantOnline_) {
                enterConnecting(nowMs);
            }
            break;

        case State::Connecting: {
            (void)transport_.update(nowMs);
            const LinkState link = transport_.linkState();
            if (link == LinkState::Up) {
                enterOnline(nowMs);
            } else if (link == LinkState::Error) {
                handleFailure(makeStatus(StatusCode::IoError, id_), nowMs);
            } else if (intervalElapsed(nowMs, connectStartMs_,
                                       policy_.connectTimeoutMs)) {
                handleFailure(makeStatus(StatusCode::Timeout, id_), nowMs);
            }
            break;
        }

        case State::Online: {
            (void)transport_.update(nowMs);
            if (transport_.linkState() != LinkState::Up) {
                ++metrics_.reconnectCount;
                lastStatus_ = makeStatus(StatusCode::IoError, id_);
                (void)transport_.disconnect();
                if (wantOnline_) {
                    // Verlust einer bestehenden Verbindung: sofort neuer Versuch
                    // nach dem regulären Backoff-Fahrplan.
                    attempts_ = 0;
                    backoffMs_ = 0;
                    enterBackoff(nowMs);
                } else {
                    state_ = State::Disconnected;
                }
            }
            break;
        }

        case State::Backoff:
            if (!wantOnline_) {
                state_ = State::Disconnected;
                break;
            }
            if (intervalElapsed(nowMs, backoffStartMs_, backoffMs_)) {
                enterConnecting(nowMs);
            }
            break;

        case State::Fault:
            // Sticky: nur begin() oder connect() setzt zurück.
            break;
    }

    return lastStatus_;
}

const char* NetworkSession::stateName(const State state) noexcept {
    switch (state) {
        case State::Disconnected:
            return "Disconnected";
        case State::Connecting:
            return "Connecting";
        case State::Online:
            return "Online";
        case State::Backoff:
            return "Backoff";
        case State::Fault:
            return "Fault";
    }
    return "Unknown";
}

}  // namespace network
}  // namespace mea
