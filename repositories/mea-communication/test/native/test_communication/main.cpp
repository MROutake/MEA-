#include <unity.h>

#include <cstdint>
#include <cstring>
#include <limits>

#include <MeaCommunication.h>
#include <mea/communication/testing/FakeByteTransport.h>
#include <mea/testing/ContractChecks.h>

void setUp() {}
void tearDown() {}

namespace {

mea::Measurement sampleMeasurement() {
    mea::Measurement measurement{};
    measurement.sourceId = 100;
    measurement.kind = mea::MeasurementKind::Voltage;  // = 2
    measurement.unit = mea::Unit::Volt;                // = 2
    measurement.value = 1.25F;
    measurement.sampledAtMs = 12345;
    measurement.sequence = 42;
    measurement.quality = mea::QualityFlag::None;
    return measurement;
}

}  // namespace

// ---------------------------------------------------------------- Encoder

static void test_encoder_exact_output() {
    const mea::CsvMeasurementEncoder encoder;
    std::uint8_t buffer[96] = {};
    std::size_t encoded = 0;
    TEST_ASSERT_TRUE(
        encoder.encode(sampleMeasurement(), buffer, sizeof(buffer), encoded).ok());
    // version;source_id;kind;unit;value;sampled_at_ms;sequence;quality\n
    TEST_ASSERT_EQUAL_STRING("1;100;2;2;1.250;12345;42;0\n",
                             reinterpret_cast<const char*>(buffer));
    TEST_ASSERT_EQUAL_size_t(std::strlen("1;100;2;2;1.250;12345;42;0\n"), encoded);
}

static void test_encoder_reports_capacity_exceeded() {
    const mea::CsvMeasurementEncoder encoder;
    std::uint8_t buffer[8] = {};
    std::size_t encoded = 0;
    const mea::Status status =
        encoder.encode(sampleMeasurement(), buffer, sizeof(buffer), encoded);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::CapacityExceeded),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_size_t(0, encoded);
    TEST_ASSERT_GREATER_THAN_UINT16(8, status.detail);  // benötigte Länge im Detail
}

static void test_encoder_rejects_non_finite_values() {
    const mea::CsvMeasurementEncoder encoder;
    std::uint8_t buffer[96] = {};
    std::size_t encoded = 0;
    mea::Measurement measurement = sampleMeasurement();
    measurement.value = std::numeric_limits<float>::quiet_NaN();
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<std::uint8_t>(mea::StatusCode::InvalidArgument),
        static_cast<std::uint8_t>(
            encoder.encode(measurement, buffer, sizeof(buffer), encoded).code));
}

static void test_encoder_custom_separator_and_decimals() {
    const mea::CsvMeasurementEncoder encoder({',', 1});
    std::uint8_t buffer[96] = {};
    std::size_t encoded = 0;
    TEST_ASSERT_TRUE(
        encoder.encode(sampleMeasurement(), buffer, sizeof(buffer), encoded).ok());
    TEST_ASSERT_EQUAL_STRING("1,100,2,2,1.2,12345,42,0\n",
                             reinterpret_cast<const char*>(buffer));
}

static void test_encoder_quality_flags_are_numeric() {
    const mea::CsvMeasurementEncoder encoder;
    std::uint8_t buffer[96] = {};
    std::size_t encoded = 0;
    mea::Measurement measurement = sampleMeasurement();
    measurement.quality = mea::QualityFlag::Stale | mea::QualityFlag::OutOfRange;  // 3
    TEST_ASSERT_TRUE(encoder.encode(measurement, buffer, sizeof(buffer), encoded).ok());
    TEST_ASSERT_EQUAL_STRING("1;100;2;2;1.250;12345;42;3\n",
                             reinterpret_cast<const char*>(buffer));
}

// ---------------------------------------------------------------- Text-Encoder

namespace {

/// Statische Lebensdauer: das Label-Array muss den Encoder überleben (ADR 0001).
constexpr mea::TextMeasurementEncoder::SourceLabel kSourceLabels[] = {
    {100, "boden"},
};

}  // namespace

static void test_text_encoder_uses_label_and_unit_symbol() {
    mea::TextMeasurementEncoder::Config config{};
    config.decimalPlaces = 2;
    config.labels =
        mea::ArrayView<const mea::TextMeasurementEncoder::SourceLabel>(kSourceLabels, 1);
    const mea::TextMeasurementEncoder encoder(config);

    mea::Measurement measurement = sampleMeasurement();  // sourceId 100
    measurement.kind = mea::MeasurementKind::SoilMoisture;
    measurement.unit = mea::Unit::Percent;
    measurement.value = 42.5F;

    std::uint8_t buffer[96] = {};
    std::size_t encoded = 0;
    TEST_ASSERT_TRUE(encoder.encode(measurement, buffer, sizeof(buffer), encoded).ok());
    TEST_ASSERT_EQUAL_STRING("boden: 42.50 % (seq=42, t=12345 ms)\n",
                             reinterpret_cast<const char*>(buffer));
    TEST_ASSERT_EQUAL_size_t(std::strlen("boden: 42.50 % (seq=42, t=12345 ms)\n"),
                             encoded);
}

static void test_text_encoder_unknown_source_prints_id_and_kind() {
    const mea::TextMeasurementEncoder encoder;  // keine Labels

    mea::Measurement measurement = sampleMeasurement();
    measurement.sourceId = 121;
    measurement.kind = mea::MeasurementKind::Pressure;
    measurement.unit = mea::Unit::Hectopascal;
    measurement.value = 991.05F;

    std::uint8_t buffer[96] = {};
    std::size_t encoded = 0;
    TEST_ASSERT_TRUE(encoder.encode(measurement, buffer, sizeof(buffer), encoded).ok());
    TEST_ASSERT_EQUAL_STRING("source 121: 991.05 hPa (Pressure, seq=42, t=12345 ms)\n",
                             reinterpret_cast<const char*>(buffer));
}

static void test_text_encoder_quality_flags_are_text() {
    const mea::TextMeasurementEncoder encoder;

    mea::Measurement measurement = sampleMeasurement();
    measurement.quality = mea::QualityFlag::Stale | mea::QualityFlag::OutOfRange;

    std::uint8_t buffer[96] = {};
    std::size_t encoded = 0;
    TEST_ASSERT_TRUE(encoder.encode(measurement, buffer, sizeof(buffer), encoded).ok());
    TEST_ASSERT_EQUAL_STRING(
        "source 100: 1.25 V [Stale|OutOfRange] (Voltage, seq=42, t=12345 ms)\n",
        reinterpret_cast<const char*>(buffer));
}

static void test_text_encoder_reports_capacity_exceeded() {
    const mea::TextMeasurementEncoder encoder;
    std::uint8_t buffer[8] = {};
    std::size_t encoded = 0;
    const mea::Status status =
        encoder.encode(sampleMeasurement(), buffer, sizeof(buffer), encoded);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::CapacityExceeded),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_size_t(0, encoded);
}

// ---------------------------------------------------------------- Sink

using TestSink = mea::BufferedMeasurementSink<2, 64>;

static void test_sink_contract_pre_begin() {
    mea::testing::FakeByteTransport transport;
    const mea::CsvMeasurementEncoder encoder;
    TestSink sink(transport, encoder, 300);
    TEST_ASSERT_NULL(mea::testing::checkSinkPreBeginContract(sink));
}

static void test_sink_complete_write() {
    mea::testing::FakeByteTransport transport;
    const mea::CsvMeasurementEncoder encoder;
    TestSink sink(transport, encoder, 300);
    TEST_ASSERT_TRUE(sink.begin().ok());

    TEST_ASSERT_TRUE(sink.submit(sampleMeasurement()).ok());
    TEST_ASSERT_TRUE(sink.update(0).ok());
    TEST_ASSERT_EQUAL_STRING("1;100;2;2;1.250;12345;42;0\n", transport.outputText());
    TEST_ASSERT_EQUAL_UINT32(1, sink.sentFrames());
}

static void test_sink_partial_write_resumes() {
    mea::testing::FakeByteTransport transport;
    transport.writableLimit = 5;  // erzwingt partielle Writes
    const mea::CsvMeasurementEncoder encoder;
    TestSink sink(transport, encoder, 300);
    TEST_ASSERT_TRUE(sink.begin().ok());
    TEST_ASSERT_TRUE(sink.submit(sampleMeasurement()).ok());

    TEST_ASSERT_TRUE(sink.update(0).ok());
    TEST_ASSERT_EQUAL_UINT32(0, sink.sentFrames());  // Frame noch unvollständig

    for (mea::TimestampMs now = 1; now < 12 && sink.sentFrames() == 0; ++now) {
        TEST_ASSERT_TRUE(sink.update(now).ok());
    }
    TEST_ASSERT_EQUAL_UINT32(1, sink.sentFrames());
    TEST_ASSERT_EQUAL_STRING("1;100;2;2;1.250;12345;42;0\n", transport.outputText());
}

static void test_sink_transport_not_ready_keeps_frame() {
    mea::testing::FakeByteTransport transport;
    transport.writableLimit = 0;  // Transport nimmt nichts an
    const mea::CsvMeasurementEncoder encoder;
    TestSink sink(transport, encoder, 300);
    TEST_ASSERT_TRUE(sink.begin().ok());
    TEST_ASSERT_TRUE(sink.submit(sampleMeasurement()).ok());

    TEST_ASSERT_TRUE(sink.update(0).ok());  // kein Fehler, nur Stau
    TEST_ASSERT_EQUAL_UINT32(0, sink.sentFrames());

    transport.writableLimit = mea::testing::FakeByteTransport::kBufferSize;
    TEST_ASSERT_TRUE(sink.update(1).ok());
    TEST_ASSERT_EQUAL_UINT32(1, sink.sentFrames());
}

static void test_sink_full_queue_reports_would_block() {
    mea::testing::FakeByteTransport transport;
    transport.writableLimit = 0;
    const mea::CsvMeasurementEncoder encoder;
    TestSink sink(transport, encoder, 300);  // QueueCapacity = 2
    TEST_ASSERT_TRUE(sink.begin().ok());

    TEST_ASSERT_TRUE(sink.submit(sampleMeasurement()).ok());
    TEST_ASSERT_TRUE(sink.submit(sampleMeasurement()).ok());
    TEST_ASSERT_EQUAL_size_t(0, sink.capacityAvailable());

    const mea::Status status = sink.submit(sampleMeasurement());
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::WouldBlock),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT16(300, status.origin);
    TEST_ASSERT_EQUAL_UINT32(1, sink.rejectedSubmits());
}

static void test_sink_frame_too_small_drops_measurement() {
    mea::testing::FakeByteTransport transport;
    const mea::CsvMeasurementEncoder encoder;
    mea::BufferedMeasurementSink<2, 8> sink(transport, encoder, 300);  // Frame zu klein
    TEST_ASSERT_TRUE(sink.begin().ok());
    TEST_ASSERT_TRUE(sink.submit(sampleMeasurement()).ok());

    const mea::Status status = sink.update(0);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::CapacityExceeded),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT16(300, status.origin);
    TEST_ASSERT_EQUAL_UINT32(1, sink.encodeErrors());

    TEST_ASSERT_TRUE(sink.update(1).ok());  // Messwert wurde verworfen, Queue leer
    TEST_ASSERT_EQUAL_UINT32(0, sink.sentFrames());
}

static void test_sink_encoder_error_skips_measurement() {
    mea::testing::FakeByteTransport transport;
    const mea::CsvMeasurementEncoder encoder;
    TestSink sink(transport, encoder, 300);
    TEST_ASSERT_TRUE(sink.begin().ok());

    mea::Measurement invalid = sampleMeasurement();
    invalid.value = std::numeric_limits<float>::infinity();
    TEST_ASSERT_TRUE(sink.submit(invalid).ok());
    TEST_ASSERT_TRUE(sink.submit(sampleMeasurement()).ok());

    const mea::Status status = sink.update(0);  // ungültiger Wert wird gemeldet
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::InvalidArgument),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT32(1, sink.encodeErrors());

    TEST_ASSERT_TRUE(sink.update(1).ok());  // gültiger Wert wird danach gesendet
    TEST_ASSERT_EQUAL_UINT32(1, sink.sentFrames());
}

static void test_sink_transport_error_retries_frame() {
    mea::testing::FakeByteTransport transport;
    transport.failWrites = true;
    const mea::CsvMeasurementEncoder encoder;
    TestSink sink(transport, encoder, 300);
    TEST_ASSERT_TRUE(sink.begin().ok());
    TEST_ASSERT_TRUE(sink.submit(sampleMeasurement()).ok());

    const mea::Status status = sink.update(0);
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::IoError),
                            static_cast<std::uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT16(300, status.origin);
    TEST_ASSERT_EQUAL_UINT32(1, sink.transportErrors());

    // Wiederanlauf: Transport funktioniert wieder, Frame geht ohne Verlust raus.
    transport.failWrites = false;
    TEST_ASSERT_TRUE(sink.update(1).ok());
    TEST_ASSERT_EQUAL_UINT32(1, sink.sentFrames());
    TEST_ASSERT_EQUAL_STRING("1;100;2;2;1.250;12345;42;0\n", transport.outputText());
}

static void test_sink_rebegin_clears_pending_data() {
    mea::testing::FakeByteTransport transport;
    transport.writableLimit = 0;
    const mea::CsvMeasurementEncoder encoder;
    TestSink sink(transport, encoder, 300);
    TEST_ASSERT_TRUE(sink.begin().ok());
    TEST_ASSERT_TRUE(sink.submit(sampleMeasurement()).ok());

    TEST_ASSERT_TRUE(sink.begin().ok());  // reinitialisierend (ADR 0004)
    TEST_ASSERT_EQUAL_size_t(2, sink.capacityAvailable());
    transport.writableLimit = mea::testing::FakeByteTransport::kBufferSize;
    TEST_ASSERT_TRUE(sink.update(0).ok());
    TEST_ASSERT_EQUAL_UINT32(0, sink.sentFrames());  // nichts mehr zu senden
}

// ---------------------------------------------------------------- Decoder

static void test_decoder_parses_valid_line() {
    mea::testing::FakeByteTransport transport;
    mea::LineCommandDecoder decoder(transport, 500);
    TEST_ASSERT_TRUE(decoder.begin().ok());

    transport.feedInput("100;1;0\r\n");
    TEST_ASSERT_TRUE(decoder.update(77).ok());
    TEST_ASSERT_EQUAL_size_t(1, decoder.available());

    mea::Command command{};
    TEST_ASSERT_TRUE(decoder.read(command).ok());
    TEST_ASSERT_EQUAL_UINT16(100, command.targetId);
    TEST_ASSERT_EQUAL_UINT16(static_cast<std::uint16_t>(mea::CommandType::Start),
                             static_cast<std::uint16_t>(command.type));
    TEST_ASSERT_EQUAL_UINT32(0, command.argument);
    TEST_ASSERT_EQUAL_UINT16(500, command.sourceId);
    TEST_ASSERT_EQUAL_UINT32(77, command.receivedAtMs);
}

static void test_decoder_counts_invalid_lines() {
    mea::testing::FakeByteTransport transport;
    mea::LineCommandDecoder decoder(transport, 500);
    TEST_ASSERT_TRUE(decoder.begin().ok());

    transport.feedInput("kaputt\n0;1;2\n100;99;0\n100;5;7\n");
    TEST_ASSERT_TRUE(decoder.update(0).ok());

    // Nur die letzte Zeile ist gültig (Ziel 0 und Typ 99 sind ungültig).
    TEST_ASSERT_EQUAL_UINT32(3, decoder.protocolErrors());
    TEST_ASSERT_EQUAL_size_t(1, decoder.available());
    mea::Command command{};
    TEST_ASSERT_TRUE(decoder.read(command).ok());
    TEST_ASSERT_EQUAL_UINT16(static_cast<std::uint16_t>(mea::CommandType::SetParameter),
                             static_cast<std::uint16_t>(command.type));
    TEST_ASSERT_EQUAL_UINT32(7, command.argument);
}

static void test_decoder_discards_overlong_lines() {
    mea::testing::FakeByteTransport transport;
    mea::LineCommandDecoder decoder(transport, 500);
    TEST_ASSERT_TRUE(decoder.begin().ok());

    transport.feedInput("9999999999999999999999999999999999999999;1;2\n100;2;0\n");
    TEST_ASSERT_TRUE(decoder.update(0).ok());
    TEST_ASSERT_EQUAL_UINT32(1, decoder.protocolErrors());
    TEST_ASSERT_EQUAL_size_t(1, decoder.available());  // Folgezeile bleibt gültig

    mea::Command command{};
    TEST_ASSERT_TRUE(decoder.read(command).ok());
    TEST_ASSERT_EQUAL_UINT16(static_cast<std::uint16_t>(mea::CommandType::Stop),
                             static_cast<std::uint16_t>(command.type));
}

static void test_decoder_read_without_data_reports_no_data() {
    mea::testing::FakeByteTransport transport;
    mea::LineCommandDecoder decoder(transport, 500);
    TEST_ASSERT_TRUE(decoder.begin().ok());
    mea::Command command{};
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(mea::StatusCode::NoData),
                            static_cast<std::uint8_t>(decoder.read(command).code));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_encoder_exact_output);
    RUN_TEST(test_encoder_reports_capacity_exceeded);
    RUN_TEST(test_encoder_rejects_non_finite_values);
    RUN_TEST(test_encoder_custom_separator_and_decimals);
    RUN_TEST(test_encoder_quality_flags_are_numeric);
    RUN_TEST(test_text_encoder_uses_label_and_unit_symbol);
    RUN_TEST(test_text_encoder_unknown_source_prints_id_and_kind);
    RUN_TEST(test_text_encoder_quality_flags_are_text);
    RUN_TEST(test_text_encoder_reports_capacity_exceeded);
    RUN_TEST(test_sink_contract_pre_begin);
    RUN_TEST(test_sink_complete_write);
    RUN_TEST(test_sink_partial_write_resumes);
    RUN_TEST(test_sink_transport_not_ready_keeps_frame);
    RUN_TEST(test_sink_full_queue_reports_would_block);
    RUN_TEST(test_sink_frame_too_small_drops_measurement);
    RUN_TEST(test_sink_encoder_error_skips_measurement);
    RUN_TEST(test_sink_transport_error_retries_frame);
    RUN_TEST(test_sink_rebegin_clears_pending_data);
    RUN_TEST(test_decoder_parses_valid_line);
    RUN_TEST(test_decoder_counts_invalid_lines);
    RUN_TEST(test_decoder_discards_overlong_lines);
    RUN_TEST(test_decoder_read_without_data_reports_no_data);
    return UNITY_END();
}
