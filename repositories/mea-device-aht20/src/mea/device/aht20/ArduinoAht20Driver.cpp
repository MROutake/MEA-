#ifdef ARDUINO

#include "ArduinoAht20Driver.h"

#include <Arduino.h>

namespace mea {

namespace {

// AHT20-Protokoll (Datenblatt ASAIR AHT20, Rev. 1.1)
constexpr std::uint8_t kCommandStatus = 0x71;
constexpr std::uint8_t kCommandInitialize = 0xBE;
constexpr std::uint8_t kCommandTrigger = 0xAC;
constexpr std::uint8_t kStatusBusyBit = 0x80;
constexpr std::uint8_t kStatusCalibratedBit = 0x08;
constexpr std::size_t kSampleFrameSize = 7;  // Status + 5 Datenbytes + CRC

}  // namespace

ArduinoAht20Driver::ArduinoAht20Driver(TwoWire& wire, const std::uint8_t address) noexcept
    : wire_(wire), address_(address) {}

Status ArduinoAht20Driver::writeCommand(const std::uint8_t command,
                                        const std::uint8_t parameter1,
                                        const std::uint8_t parameter2) noexcept {
    wire_.beginTransmission(address_);
    wire_.write(command);
    wire_.write(parameter1);
    wire_.write(parameter2);
    const std::uint8_t error = wire_.endTransmission();
    if (error != 0) {
        return makeStatus(StatusCode::IoError, InvalidComponentId, error);
    }
    return okStatus();
}

Status ArduinoAht20Driver::readStatusByte(std::uint8_t& statusByte) noexcept {
    wire_.beginTransmission(address_);
    wire_.write(kCommandStatus);
    const std::uint8_t error = wire_.endTransmission();
    if (error != 0) {
        return makeStatus(StatusCode::IoError, InvalidComponentId, error);
    }
    if (wire_.requestFrom(address_, static_cast<std::uint8_t>(1)) != 1) {
        return makeStatus(StatusCode::IoError, InvalidComponentId);
    }
    statusByte = static_cast<std::uint8_t>(wire_.read());
    return okStatus();
}

Status ArduinoAht20Driver::begin() noexcept {
    delay(40);  // Anlaufzeit nach Power-on (Datenblatt); nur bei Initialisierung

    std::uint8_t statusByte = 0;
    Status status = readStatusByte(statusByte);
    if (!status.ok()) {
        return status;
    }

    if ((statusByte & kStatusCalibratedBit) == 0) {
        status = writeCommand(kCommandInitialize, 0x08, 0x00);
        if (!status.ok()) {
            return status;
        }
        delay(10);  // Kalibrierdauer (Datenblatt)
        status = readStatusByte(statusByte);
        if (!status.ok()) {
            return status;
        }
        if ((statusByte & kStatusCalibratedBit) == 0) {
            return makeStatus(StatusCode::ProtocolError, InvalidComponentId, statusByte);
        }
    }
    return okStatus();
}

Status ArduinoAht20Driver::triggerMeasurement() noexcept {
    return writeCommand(kCommandTrigger, 0x33, 0x00);
}

Status ArduinoAht20Driver::readSample(Aht20Sample& output) noexcept {
    if (wire_.requestFrom(address_, static_cast<std::uint8_t>(kSampleFrameSize)) !=
        kSampleFrameSize) {
        return makeStatus(StatusCode::IoError, InvalidComponentId);
    }

    std::uint8_t frame[kSampleFrameSize] = {};
    for (std::size_t index = 0; index < kSampleFrameSize; ++index) {
        frame[index] = static_cast<std::uint8_t>(wire_.read());
    }

    if ((frame[0] & kStatusBusyBit) != 0) {
        return makeStatus(StatusCode::Busy, InvalidComponentId);
    }
    if (crc8(frame, kSampleFrameSize - 1) != frame[kSampleFrameSize - 1]) {
        return makeStatus(StatusCode::ChecksumError, InvalidComponentId);
    }

    const std::uint32_t rawHumidity =
        (static_cast<std::uint32_t>(frame[1]) << 12U) |
        (static_cast<std::uint32_t>(frame[2]) << 4U) |
        (static_cast<std::uint32_t>(frame[3]) >> 4U);
    const std::uint32_t rawTemperature =
        ((static_cast<std::uint32_t>(frame[3]) & 0x0FU) << 16U) |
        (static_cast<std::uint32_t>(frame[4]) << 8U) |
        static_cast<std::uint32_t>(frame[5]);

    // 20-Bit-Rohwerte -> physikalische Größen (Datenblatt Kap. 6.1)
    constexpr float kRawFullScale = 1048576.0F;  // 2^20
    output.humidityPercent = (static_cast<float>(rawHumidity) / kRawFullScale) * 100.0F;
    output.temperatureCelsius =
        (static_cast<float>(rawTemperature) / kRawFullScale) * 200.0F - 50.0F;
    return okStatus();
}

std::uint8_t ArduinoAht20Driver::crc8(const std::uint8_t* data,
                                      const std::size_t length) noexcept {
    // CRC-8/NRSC-5: Polynom 0x31, Startwert 0xFF (Datenblatt Kap. 6.2)
    std::uint8_t crc = 0xFF;
    for (std::size_t index = 0; index < length; ++index) {
        crc ^= data[index];
        for (std::uint8_t bit = 0; bit < 8; ++bit) {
            if ((crc & 0x80U) != 0) {
                crc = static_cast<std::uint8_t>((crc << 1U) ^ 0x31U);
            } else {
                crc = static_cast<std::uint8_t>(crc << 1U);
            }
        }
    }
    return crc;
}

}  // namespace mea

#endif  // ARDUINO
