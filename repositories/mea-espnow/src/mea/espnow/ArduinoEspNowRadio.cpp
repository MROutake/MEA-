#ifdef ESP32

#include "ArduinoEspNowRadio.h"

#include <WiFi.h>
#include <esp_idf_version.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <cstring>

namespace mea {

ArduinoEspNowRadio* ArduinoEspNowRadio::instance_ = nullptr;

namespace {

/// ESP-NOW-Empfangs-Callback: Signatur hängt von der IDF-Version ab.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void onEspNowReceive(const esp_now_recv_info_t* info, const std::uint8_t* data,
                     int length) {
    if (ArduinoEspNowRadio* const radio = ArduinoEspNowRadio::instanceForCallback()) {
        radio->dispatchReceive(info->src_addr, data, length);
    }
}
#else
void onEspNowReceive(const std::uint8_t* sourceMac, const std::uint8_t* data,
                     int length) {
    if (ArduinoEspNowRadio* const radio = ArduinoEspNowRadio::instanceForCallback()) {
        radio->dispatchReceive(sourceMac, data, length);
    }
}
#endif

}  // namespace

ArduinoEspNowRadio::ArduinoEspNowRadio(const std::uint8_t maximumChannel) noexcept
    : maximumChannel_(maximumChannel) {}

ArduinoEspNowRadio::~ArduinoEspNowRadio() {
    if (instance_ == this) {
        esp_now_unregister_recv_cb();
        instance_ = nullptr;
    }
}

Status ArduinoEspNowRadio::begin() noexcept {
    if (instance_ != nullptr && instance_ != this) {
        return makeStatus(StatusCode::InvalidConfiguration, InvalidComponentId);
    }
    if (maximumChannel_ == 0 || maximumChannel_ > 14) {
        return makeStatus(StatusCode::InvalidConfiguration, InvalidComponentId);
    }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    esp_err_t error = esp_now_init();
    if (error != ESP_OK && error != ESP_ERR_ESPNOW_EXIST) {
        return makeStatus(StatusCode::IoError, InvalidComponentId,
                          static_cast<std::uint16_t>(error));
    }

    instance_ = this;
    error = esp_now_register_recv_cb(&onEspNowReceive);
    if (error != ESP_OK) {
        return makeStatus(StatusCode::IoError, InvalidComponentId,
                          static_cast<std::uint16_t>(error));
    }

    std::uint8_t primary = 0;
    wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE;
    if (esp_wifi_get_channel(&primary, &secondary) == ESP_OK && primary != 0) {
        channel_ = primary;
    }

    portENTER_CRITICAL(&queueLock_);
    queue_.clear();
    portEXIT_CRITICAL(&queueLock_);

    initialized_ = true;
    return okStatus();
}

Status ArduinoEspNowRadio::setChannel(const std::uint8_t channel) noexcept {
    if (!initialized_) {
        return makeStatus(StatusCode::NotInitialized, InvalidComponentId);
    }
    if (channel == 0 || channel > maximumChannel_) {
        return makeStatus(StatusCode::InvalidArgument, InvalidComponentId, channel);
    }
    const esp_err_t error = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (error != ESP_OK) {
        return makeStatus(StatusCode::IoError, InvalidComponentId,
                          static_cast<std::uint16_t>(error));
    }
    channel_ = channel;
    return okStatus();
}

MacAddress ArduinoEspNowRadio::localAddress() const noexcept {
    MacAddress address{};
    (void)esp_wifi_get_mac(WIFI_IF_STA, address.bytes);
    return address;
}

Status ArduinoEspNowRadio::ensurePeer(const MacAddress& address) noexcept {
    if (esp_now_is_peer_exist(address.bytes)) {
        return okStatus();
    }
    esp_now_peer_info_t peer{};
    std::memcpy(peer.peer_addr, address.bytes, sizeof(peer.peer_addr));
    peer.channel = 0;  // 0 = aktueller Kanal (folgt setChannel())
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    const esp_err_t error = esp_now_add_peer(&peer);
    if (error != ESP_OK) {
        return makeStatus(StatusCode::IoError, InvalidComponentId,
                          static_cast<std::uint16_t>(error));
    }
    return okStatus();
}

Status ArduinoEspNowRadio::send(const MacAddress& destination,
                                const std::uint8_t* data,
                                const std::size_t size) noexcept {
    if (!initialized_) {
        return makeStatus(StatusCode::NotInitialized, InvalidComponentId);
    }
    if (data == nullptr || size == 0 || size > kEspNowMaxFrameSize) {
        return makeStatus(StatusCode::InvalidArgument, InvalidComponentId);
    }
    const Status peerStatus = ensurePeer(destination);
    if (!peerStatus.ok()) {
        return peerStatus;
    }
    const esp_err_t error = esp_now_send(destination.bytes, data, size);
    if (error != ESP_OK) {
        return makeStatus(StatusCode::IoError, InvalidComponentId,
                          static_cast<std::uint16_t>(error));
    }
    return okStatus();
}

Status ArduinoEspNowRadio::removePeer(const MacAddress& address) noexcept {
    if (esp_now_is_peer_exist(address.bytes)) {
        (void)esp_now_del_peer(address.bytes);
    }
    return okStatus();
}

std::size_t ArduinoEspNowRadio::available() const noexcept {
    portENTER_CRITICAL(&queueLock_);
    const std::size_t size = queue_.size();
    portEXIT_CRITICAL(&queueLock_);
    return size;
}

Status ArduinoEspNowRadio::receive(EspNowPacket& output) noexcept {
    portENTER_CRITICAL(&queueLock_);
    const bool popped = queue_.pop(output);
    portEXIT_CRITICAL(&queueLock_);
    if (!popped) {
        return makeStatus(StatusCode::NoData, InvalidComponentId);
    }
    return okStatus();
}

void ArduinoEspNowRadio::handleReceive(const std::uint8_t* sourceMac,
                                       const std::uint8_t* data,
                                       const int length) noexcept {
    if (sourceMac == nullptr || data == nullptr || length <= 0 ||
        static_cast<std::size_t>(length) > kEspNowMaxFrameSize) {
        return;
    }
    EspNowPacket packet{};
    std::memcpy(packet.source.bytes, sourceMac, sizeof(packet.source.bytes));
    packet.length = static_cast<std::uint8_t>(length);
    std::memcpy(packet.data, data, packet.length);

    portENTER_CRITICAL(&queueLock_);
    const bool pushed = queue_.push(packet);
    portEXIT_CRITICAL(&queueLock_);
    if (!pushed) {
        ++droppedPackets_;  // Drop-Policy: neues Paket verwerfen
    }
}

}  // namespace mea

#endif  // ESP32
