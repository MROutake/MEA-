
#include <Arduino.h>

#include <stddef.h>
#include <stdint.h>

#include "Arduino74hc595.h"

namespace mea {

Arduino74hc595::Arduino74hc595(ArduinoShiftWriterConfig pins) noexcept
        : dataPin_(pins.dataPin),
            shiftClockPin_(pins.shiftClockPin),
            storageClockPin_(pins.storageClockPin),
            enablePin_(pins.enablePin),
            masterResetPin_(pins.masterResetPin),
            bitOrder_(pins.bitOrder) {}

Status Arduino74hc595::begin() noexcept {
        pinMode(dataPin_, OUTPUT);
        pinMode(shiftClockPin_, OUTPUT);
        pinMode(storageClockPin_, OUTPUT);
        pinMode(enablePin_, OUTPUT);
        pinMode(masterResetPin_, OUTPUT);

        digitalWrite(shiftClockPin_, LOW);
        digitalWrite(storageClockPin_, LOW);
        digitalWrite(dataPin_, LOW);

        // Keep outputs disabled until a defined register state is established.
        digitalWrite(enablePin_, HIGH);

        // Clear shift register via MR and release it again.
        digitalWrite(masterResetPin_, LOW);
        delayMicroseconds(5);
        digitalWrite(masterResetPin_, HIGH);

        // Latch cleared shift register into output register.
        digitalWrite(storageClockPin_, HIGH);
        digitalWrite(storageClockPin_, LOW);

        // Outputs active after defined zero state is latched.
        digitalWrite(enablePin_, LOW);
        latchState_ = false;
        latchDirty_ = false;
        return okStatus();
}

Status Arduino74hc595::setLatch(bool state) noexcept {
    latchState_ = state;
    latchDirty_ = true;
    return okStatus();
}

Status Arduino74hc595::write(const uint8_t* data, size_t length) noexcept {
    if (data == nullptr && length > 0) {
        return makeStatus(StatusCode::InvalidArgument, InvalidComponentId);
    }

    digitalWrite(storageClockPin_, LOW);

    for (size_t i = length; i > 0; --i) {
        const size_t byteIndex = i - 1;
        const auto order = (bitOrder_ == 0U) ? MSBFIRST : LSBFIRST;
        shiftOut(dataPin_, shiftClockPin_, order, data[byteIndex]);
    }

    if (latchDirty_) {
        if (latchState_) {
            digitalWrite(storageClockPin_, HIGH);
            digitalWrite(storageClockPin_, LOW);
        } else {
            digitalWrite(storageClockPin_, LOW);
        }
        latchDirty_ = false;
    }

    return okStatus();
}

}  // namespace mea





