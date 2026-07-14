#ifdef ARDUINO

#include "ArduinoAnalogReader.h"

#include <Arduino.h>

namespace mea {

ArduinoAnalogReader::ArduinoAnalogReader(const std::uint32_t maximumRawValue) noexcept
    : maximumRawValue_(maximumRawValue) {}

Status ArduinoAnalogReader::beginPin(const std::uint8_t pin) noexcept {
    if (maximumRawValue_ == 0) {
        return makeStatus(StatusCode::InvalidConfiguration, InvalidComponentId);
    }
    pinMode(pin, INPUT);
    return okStatus();
}

Status ArduinoAnalogReader::readRaw(const std::uint8_t pin,
                                    std::uint32_t& output) noexcept {
    const int raw = analogRead(pin);
    if (raw < 0) {
        return makeStatus(StatusCode::IoError, InvalidComponentId,
                          static_cast<std::uint16_t>(-raw));
    }
    output = static_cast<std::uint32_t>(raw);
    return okStatus();
}

std::uint32_t ArduinoAnalogReader::maximumRawValue() const noexcept {
    return maximumRawValue_;
}

}  // namespace mea

#endif  // ARDUINO
