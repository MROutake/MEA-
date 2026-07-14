#include <Arduino.h>

#include <MeaCommunication.h>

namespace {

// Composition-Root-Beispiel: Transport, Encoder und Sink werden hier
// verdrahtet; die Library codiert nichts fest.
constexpr mea::ComponentId kSinkId = 1;

mea::ArduinoStreamTransport transport(Serial);
const mea::CsvMeasurementEncoder encoder;
mea::BufferedMeasurementSink<8, 96> sink(transport, encoder, kSinkId);

mea::TimestampMs lastSubmitMs = 0;
mea::SequenceNumber sequence = 0;

}  // namespace

void setup() {
    Serial.begin(115200);
    (void)transport.begin();  // Stream ist bereits initialisiert
    const mea::Status status = sink.begin();
    if (!status.ok()) {
        Serial.print("sink begin failed: ");
        Serial.println(mea::statusCodeName(status.code));
    }
}

void loop() {
    const mea::TimestampMs now = millis();

    if (mea::intervalElapsed(now, lastSubmitMs, 1000)) {
        lastSubmitMs = now;
        mea::Measurement measurement{};
        measurement.sourceId = 7;
        measurement.kind = mea::MeasurementKind::Temperature;
        measurement.unit = mea::Unit::DegreeCelsius;
        measurement.value = 23.5F;
        measurement.sampledAtMs = now;
        measurement.sequence = ++sequence;
        (void)sink.submit(measurement);  // WouldBlock = Backpressure, hier ignoriert
    }

    (void)transport.update(now);
    const mea::Status status = sink.update(now);
    if (!status.ok() && !status.transient()) {
        Serial.print("sink update failed: ");
        Serial.println(mea::statusCodeName(status.code));
    }
}
