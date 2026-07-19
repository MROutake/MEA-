#ifdef ARDUINO

#include "ArduinoBmp280Driver.h"

namespace mea {

namespace {

// Registeradressen (Datenblatt Bosch BMP280, Kap. 4)
constexpr std::uint8_t kRegisterChipId = 0xD0;
constexpr std::uint8_t kRegisterCalibrationStart = 0x88;
constexpr std::uint8_t kRegisterCtrlMeas = 0xF4;
constexpr std::uint8_t kRegisterConfig = 0xF5;
constexpr std::uint8_t kRegisterDataStart = 0xF7;

constexpr std::uint8_t kChipIdBmp280 = 0x58;
constexpr std::uint8_t kChipIdBme280 = 0x60;

// Oversampling T x2, P x16, Normal Mode: 0b010'101'11
constexpr std::uint8_t kCtrlMeasValue = 0x57;
// Standby 250 ms, IIR-Filter 4: 0b011'010'00
constexpr std::uint8_t kConfigValue = 0x68;

constexpr std::size_t kCalibrationSize = 24;
constexpr std::size_t kDataFrameSize = 6;

/// Rohwert der Datenregister vor Abschluss der ersten Messung (Reset-Wert).
constexpr std::int32_t kRawValueNoConversion = 0x80000;

[[nodiscard]] std::uint16_t readLittleEndianU16(const std::uint8_t* bytes) noexcept {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8U |
                                      static_cast<std::uint16_t>(bytes[0]));
}

[[nodiscard]] std::int16_t readLittleEndianS16(const std::uint8_t* bytes) noexcept {
    return static_cast<std::int16_t>(readLittleEndianU16(bytes));
}

}  // namespace

ArduinoBmp280Driver::ArduinoBmp280Driver(TwoWire& wire,
                                         const std::uint8_t address) noexcept
    : wire_(wire), address_(address) {}

Status ArduinoBmp280Driver::readRegisters(const std::uint8_t startRegister,
                                          std::uint8_t* buffer,
                                          const std::size_t length) noexcept {
    wire_.beginTransmission(address_);
    wire_.write(startRegister);
    const std::uint8_t error = wire_.endTransmission();
    if (error != 0) {
        return makeStatus(StatusCode::IoError, InvalidComponentId, error);
    }
    if (wire_.requestFrom(address_, static_cast<std::uint8_t>(length)) != length) {
        return makeStatus(StatusCode::IoError, InvalidComponentId);
    }
    for (std::size_t index = 0; index < length; ++index) {
        buffer[index] = static_cast<std::uint8_t>(wire_.read());
    }
    return okStatus();
}

Status ArduinoBmp280Driver::writeRegister(const std::uint8_t registerAddress,
                                          const std::uint8_t value) noexcept {
    wire_.beginTransmission(address_);
    wire_.write(registerAddress);
    wire_.write(value);
    const std::uint8_t error = wire_.endTransmission();
    if (error != 0) {
        return makeStatus(StatusCode::IoError, InvalidComponentId, error);
    }
    return okStatus();
}

Status ArduinoBmp280Driver::readCalibration() noexcept {
    std::uint8_t calibration[kCalibrationSize] = {};
    const Status status =
        readRegisters(kRegisterCalibrationStart, calibration, kCalibrationSize);
    if (!status.ok()) {
        return status;
    }

    digT1_ = readLittleEndianU16(&calibration[0]);
    digT2_ = readLittleEndianS16(&calibration[2]);
    digT3_ = readLittleEndianS16(&calibration[4]);
    digP1_ = readLittleEndianU16(&calibration[6]);
    digP2_ = readLittleEndianS16(&calibration[8]);
    digP3_ = readLittleEndianS16(&calibration[10]);
    digP4_ = readLittleEndianS16(&calibration[12]);
    digP5_ = readLittleEndianS16(&calibration[14]);
    digP6_ = readLittleEndianS16(&calibration[16]);
    digP7_ = readLittleEndianS16(&calibration[18]);
    digP8_ = readLittleEndianS16(&calibration[20]);
    digP9_ = readLittleEndianS16(&calibration[22]);

    // dig_T1/dig_P1 sind laut Datenblatt nie 0; 0 deutet auf Lesefehler hin.
    if (digT1_ == 0 || digP1_ == 0) {
        return makeStatus(StatusCode::ProtocolError, InvalidComponentId, 1);
    }
    return okStatus();
}

Status ArduinoBmp280Driver::begin() noexcept {
    std::uint8_t chipId = 0;
    Status status = readRegisters(kRegisterChipId, &chipId, 1);
    if (!status.ok()) {
        return status;
    }
    if (chipId != kChipIdBmp280 && chipId != kChipIdBme280) {
        return makeStatus(StatusCode::ProtocolError, InvalidComponentId, chipId);
    }

    status = readCalibration();
    if (!status.ok()) {
        return status;
    }

    status = writeRegister(kRegisterConfig, kConfigValue);
    if (!status.ok()) {
        return status;
    }
    return writeRegister(kRegisterCtrlMeas, kCtrlMeasValue);
}

Status ArduinoBmp280Driver::readSample(Bmp280Sample& output) noexcept {
    std::uint8_t frame[kDataFrameSize] = {};
    const Status status = readRegisters(kRegisterDataStart, frame, kDataFrameSize);
    if (!status.ok()) {
        return status;
    }

    const std::int32_t rawPressure =
        static_cast<std::int32_t>((static_cast<std::uint32_t>(frame[0]) << 12U) |
                                  (static_cast<std::uint32_t>(frame[1]) << 4U) |
                                  (static_cast<std::uint32_t>(frame[2]) >> 4U));
    const std::int32_t rawTemperature =
        static_cast<std::int32_t>((static_cast<std::uint32_t>(frame[3]) << 12U) |
                                  (static_cast<std::uint32_t>(frame[4]) << 4U) |
                                  (static_cast<std::uint32_t>(frame[5]) >> 4U));

    if (rawTemperature == kRawValueNoConversion ||
        rawPressure == kRawValueNoConversion) {
        return makeStatus(StatusCode::Busy, InvalidComponentId);
    }

    // Kompensation in Gleitkomma (Datenblatt Kap. 8.1). t_fine koppelt die
    // Druckkompensation an die Temperaturmessung.
    const float adcT = static_cast<float>(rawTemperature);
    float var1 = (adcT / 16384.0F - static_cast<float>(digT1_) / 1024.0F) *
                 static_cast<float>(digT2_);
    const float partialT = adcT / 131072.0F - static_cast<float>(digT1_) / 8192.0F;
    const float var2 = partialT * partialT * static_cast<float>(digT3_);
    const float tFine = var1 + var2;
    output.temperatureCelsius = tFine / 5120.0F;

    const float adcP = static_cast<float>(rawPressure);
    var1 = tFine / 2.0F - 64000.0F;
    float var2p = var1 * var1 * static_cast<float>(digP6_) / 32768.0F;
    var2p = var2p + var1 * static_cast<float>(digP5_) * 2.0F;
    var2p = var2p / 4.0F + static_cast<float>(digP4_) * 65536.0F;
    var1 = (static_cast<float>(digP3_) * var1 * var1 / 524288.0F +
            static_cast<float>(digP2_) * var1) /
           524288.0F;
    var1 = (1.0F + var1 / 32768.0F) * static_cast<float>(digP1_);
    if (var1 == 0.0F) {
        return makeStatus(StatusCode::ProcessingError, InvalidComponentId);
    }
    float pressure = 1048576.0F - adcP;
    pressure = (pressure - var2p / 4096.0F) * 6250.0F / var1;
    var1 = static_cast<float>(digP9_) * pressure * pressure / 2147483648.0F;
    var2p = pressure * static_cast<float>(digP8_) / 32768.0F;
    output.pressurePascal =
        pressure + (var1 + var2p + static_cast<float>(digP7_)) / 16.0F;

    return okStatus();
}

}  // namespace mea

#endif  // ARDUINO
