
#include <stddef.h>
#include <stdint.h>
#include <array>
#include <vector>

#include "IShiftWriter.h"
#include "ArduinoShiftWriterConfig.h"

namespace mea {

    struct Pinmode
    {
        uint8_t mode = 0; // 0 = INPUT, 1 = OUTPUT
        uint8_t state = 0; // 0 = LOW, 1 = HIGH

    };


    

class Arduino74hc595 final : public IShiftWriter {
public:
    struct ShiftOutCall {
        uint8_t dataPin{0};
        uint8_t shiftClockPin{0};
        uint8_t bitOrder{0};  // 0 = MSBFIRST, 1 = LSBFIRST
        uint8_t value{0};
        std::array<uint8_t, 8> bitPattern{};
    };


    explicit Arduino74hc595(ArduinoShiftWriterConfig pins) noexcept
        : dataPin_(pins.dataPin),
          shiftClockPin_(pins.shiftClockPin),
          storageClockPin_(pins.storageClockPin),
          enablePin_(pins.enablePin),
          masterResetPin_(pins.masterResetPin),
          bitOrder_(pins.bitOrder) {}

    Status begin() noexcept override {
        setpinMode(dataPin_, 1, pinMode1_);
        setpinMode(shiftClockPin_, 1, pinMode2_);
        setpinMode(storageClockPin_, 1, pinMode3_);
        setpinMode(enablePin_, 1, pinMode4_);
        setpinMode(masterResetPin_, 1, pinMode5_);

        setDigitalWrite(shiftClockPin_, 0, pinMode2_);
        setDigitalWrite(storageClockPin_, 0, pinMode3_);
        setDigitalWrite(dataPin_, 0, pinMode1_);

        // Keep outputs disabled until a defined register state is established.
        setDigitalWrite(enablePin_, 1, pinMode4_);

        // Clear shift register via MR and release it again.
        setDigitalWrite(masterResetPin_, 0, pinMode5_);
        setDigitalWrite(masterResetPin_, 1, pinMode5_);

        // Latch cleared shift register into output register.
        setDigitalWrite(storageClockPin_, 1, pinMode3_);
        setDigitalWrite(storageClockPin_, 0, pinMode3_);

        // Outputs active after defined zero state is latched.
        setDigitalWrite(enablePin_, 0, pinMode4_);
        latchState_ = false;
        latchDirty_ = false;
        shiftOutCalls_.clear();
        return okStatus();
    }

    Status setLatch(bool state) noexcept override {
        latchState_ = state;
        latchDirty_ = true;
        return okStatus();
    }

    Status write(const uint8_t* data, size_t length) noexcept override {
        if (data == nullptr && length > 0) {
            return makeStatus(StatusCode::InvalidArgument, InvalidComponentId);
        }

        setDigitalWrite(storageClockPin_, 0, pinMode3_);

        for (size_t i = length; i > 0; --i) {
            const size_t byteIndex = i - 1;
            shiftOut(dataPin_, shiftClockPin_, bitOrder_, data[byteIndex]);
        }

        if (latchDirty_) {
            if (latchState_) {
                setDigitalWrite(storageClockPin_, 1, pinMode3_);
                setDigitalWrite(storageClockPin_, 0, pinMode3_);
            } else {
                setDigitalWrite(storageClockPin_, 0, pinMode3_);
            }
            latchDirty_ = false;
        }

        return okStatus();
    }

    const std::vector<ShiftOutCall>& shiftOutCalls() const noexcept {
        return shiftOutCalls_;
    }

    void clearShiftOutCalls() noexcept {
        shiftOutCalls_.clear();
    }

private:

    void shiftOut(uint8_t dataPin, uint8_t shiftClockPin, uint8_t bitOrder,
                  uint8_t value) noexcept {
        ShiftOutCall call{};
        call.dataPin = dataPin;
        call.shiftClockPin = shiftClockPin;
        call.bitOrder = bitOrder;
        call.value = value;

        if (bitOrder == 0) {
            for (uint8_t i = 0; i < 8; ++i) {
                const uint8_t shift = static_cast<uint8_t>(7U - i);
                call.bitPattern[i] = static_cast<uint8_t>((value >> shift) & 0x01U);
            }
        } else {
            for (uint8_t i = 0; i < 8; ++i) {
                call.bitPattern[i] = static_cast<uint8_t>((value >> i) & 0x01U);
            }
        }

        shiftOutCalls_.push_back(call);
    }

    static void setpinMode(uint8_t pin, uint8_t mode, Pinmode& pinmode) noexcept {
        (void)pin;
        pinmode.mode = mode;
    }

    static void setDigitalWrite(uint8_t pin, uint8_t value, Pinmode& pinmode) noexcept {
        (void)pin;
        pinmode.state = value;
    }
    
    


    Pinmode pinMode1_{0,0};
    Pinmode pinMode2_{0,0};
    Pinmode pinMode3_{0,0};
    Pinmode pinMode4_{0,0};
    Pinmode pinMode5_{0,0};
    uint8_t dataPin_;
    uint8_t shiftClockPin_;
    uint8_t storageClockPin_;
    uint8_t enablePin_;
    uint8_t masterResetPin_;
    uint8_t bitOrder_;
    bool latchState_{false};
    bool latchDirty_{false};
    std::vector<ShiftOutCall> shiftOutCalls_{};
};

}  // namespace mea





