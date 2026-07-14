#include <Arduino.h>

#include <MeaAnalogInput.h>

namespace {

// Beispiel: alle Werte kommen aus dem Anwendungscode, nichts ist in der
// Library fest codiert.
constexpr mea::ComponentId kSensorId = 1;
constexpr std::uint8_t kAdcPin = 34;
constexpr std::uint32_t kAdcMaxRaw = 4095;  // ESP32: 12 Bit

mea::ArduinoAnalogReader reader(kAdcMaxRaw);

mea::AnalogInputSensor sensor(reader, {
                                          kSensorId,
                                          kAdcPin,
                                          1000,  // sampleIntervalMs
                                          4,     // samplesPerMeasurement
                                          2,     // maxSamplesPerUpdate
                                          mea::MeasurementKind::RawAnalog,
                                          mea::Unit::RawCount,
                                      });

}  // namespace

void setup() {
    Serial.begin(115200);
    const mea::Status status = sensor.begin();
    if (!status.ok()) {
        Serial.print("sensor begin failed: ");
        Serial.println(mea::statusCodeName(status.code));
    }
}

void loop() {
    const mea::Status status = sensor.update(millis());
    if (!status.ok() && !status.transient()) {
        Serial.print("sensor update failed: ");
        Serial.println(mea::statusCodeName(status.code));
    }

    mea::Measurement measurement{};
    while (sensor.available() > 0 && sensor.read(measurement).ok()) {
        Serial.println(measurement.value);
    }
}
