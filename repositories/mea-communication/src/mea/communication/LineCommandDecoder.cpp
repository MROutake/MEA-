#include "LineCommandDecoder.h"

#include <cstdint>
#include <cstdlib>

namespace mea {

namespace {

/// Parst eine vorzeichenlose Dezimalzahl ab @p text; @p end zeigt danach auf
/// das erste Zeichen hinter der Zahl. false, wenn keine Ziffer vorliegt oder
/// der Wert @p maxValue überschreitet.
bool parseUnsigned(const char* text, const char** end, const std::uint32_t maxValue,
                   std::uint32_t& value) noexcept {
    if (*text < '0' || *text > '9') {
        return false;
    }
    std::uint32_t result = 0;
    while (*text >= '0' && *text <= '9') {
        const auto digit = static_cast<std::uint32_t>(*text - '0');
        if (result > (maxValue - digit) / 10U) {
            return false;  // Überlauf
        }
        result = (result * 10U) + digit;
        ++text;
    }
    *end = text;
    value = result;
    return true;
}

}  // namespace

Status LineCommandDecoder::begin() noexcept {
    initialized_ = false;
    if (decoderId_ == InvalidComponentId) {
        return makeStatus(StatusCode::InvalidConfiguration, decoderId_);
    }
    queue_.clear();
    lineLength_ = 0;
    discardingLine_ = false;
    initialized_ = true;
    return okStatus();
}

Status LineCommandDecoder::update(const TimestampMs nowMs) noexcept {
    if (!initialized_) {
        return makeStatus(StatusCode::NotInitialized, decoderId_);
    }

    std::size_t totalRead = 0;
    while (totalRead < kMaxBytesPerUpdate) {
        std::uint8_t byte = 0;
        std::size_t readCount = 0;
        Status status = transport_.read(&byte, 1, readCount);
        if (!status.ok()) {
            if (status.origin == InvalidComponentId) {
                status.origin = decoderId_;
            }
            return status;
        }
        if (readCount == 0) {
            break;  // keine weiteren Daten
        }
        ++totalRead;

        const char character = static_cast<char>(byte);
        if (character == '\r') {
            continue;
        }
        if (character == '\n') {
            if (discardingLine_) {
                discardingLine_ = false;  // Ende der überlangen Zeile
            } else if (lineLength_ > 0) {
                handleLine(nowMs);
            }
            lineLength_ = 0;
            continue;
        }
        if (discardingLine_) {
            continue;
        }
        if (lineLength_ >= kMaxLineLength - 1U) {
            ++protocolErrors_;
            discardingLine_ = true;  // Rest der Zeile ignorieren
            lineLength_ = 0;
            continue;
        }
        line_[lineLength_] = character;
        ++lineLength_;
    }
    return okStatus();
}

void LineCommandDecoder::handleLine(const TimestampMs nowMs) noexcept {
    line_[lineLength_] = '\0';
    Command command{};
    if (!parseLine(command)) {
        ++protocolErrors_;
        return;
    }
    command.sourceId = decoderId_;
    command.receivedAtMs = nowMs;
    if (!queue_.push(command)) {
        ++droppedCommands_;
    }
}

bool LineCommandDecoder::parseLine(Command& command) const noexcept {
    const char* cursor = line_;
    const char* end = nullptr;

    std::uint32_t target = 0;
    if (!parseUnsigned(cursor, &end, 0xFFFFU, target) || *end != ';' ||
        target == InvalidComponentId) {
        return false;
    }
    cursor = end + 1;

    std::uint32_t type = 0;
    if (!parseUnsigned(cursor, &end, 0xFFFFU, type) || *end != ';' ||
        type > static_cast<std::uint32_t>(CommandType::SetParameter)) {
        return false;
    }
    cursor = end + 1;

    std::uint32_t argument = 0;
    if (!parseUnsigned(cursor, &end, 0xFFFFFFFFU, argument) || *end != '\0') {
        return false;
    }

    command.targetId = static_cast<ComponentId>(target);
    command.type = static_cast<CommandType>(type);
    command.argument = argument;
    return true;
}

std::size_t LineCommandDecoder::available() const noexcept {
    return queue_.size();
}

Status LineCommandDecoder::read(Command& output) noexcept {
    if (!initialized_) {
        return makeStatus(StatusCode::NotInitialized, decoderId_);
    }
    if (!queue_.pop(output)) {
        return makeStatus(StatusCode::NoData, decoderId_);
    }
    return okStatus();
}

}  // namespace mea
