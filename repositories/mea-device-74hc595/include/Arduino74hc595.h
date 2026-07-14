#pragma once

#include <stddef.h>
#include <stdint.h>

#include "IShiftWriter.h"
#include "ArduinoShiftWriterConfig.h"

namespace mea {

class Arduino74hc595 final : public IShiftWriter {
public:
    explicit Arduino74hc595(ArduinoShiftWriterConfig pins) noexcept;

    Status begin() noexcept override;
    Status setLatch(bool state) noexcept override;
    Status write(const uint8_t* data, size_t length) noexcept override;

private:
    uint8_t dataPin_;
    uint8_t shiftClockPin_;
    uint8_t storageClockPin_;
    uint8_t enablePin_;
    uint8_t masterResetPin_;
    uint8_t bitOrder_;
    bool latchState_{false};
    bool latchDirty_{false};
};

}  // namespace mea
