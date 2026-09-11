#include "udp_actisense.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ActisenseReader.h>
#include <N2kMsg.h>

#include "udp_nmea2000_config.h"
#include "Ray_N2K.h"

namespace {

class UdpActisenseStream final : public Stream {
public:
  size_t write(uint8_t) override { return 0; }

  size_t write(const uint8_t *, size_t) override { return 0; }

  int available() override {
    return static_cast<int>(availableBytes());
  }

  int read() override {
    if (availableBytes() == 0) return -1;
    const uint8_t value = buffer[readPos];
    readPos = (readPos + 1) % sizeof(buffer);
    --count;
    return value;
  }

  int peek() override {
    if (availableBytes() == 0) return -1;
    return buffer[readPos];
  }

  void flush() override {}

  size_t append(const uint8_t *data, size_t length) {
    size_t accepted = 0;
    while (accepted < length && count < sizeof(buffer)) {
      buffer[writePos] = data[accepted++];
      writePos = (writePos + 1) % sizeof(buffer);
      ++count;
    }
    return accepted;
  }

  size_t availableBytes() const { return count; }

private:
  uint8_t buffer[N2K_UDP_STREAM_BUFFER_SIZE] = {};
  size_t readPos = 0;
  size_t writePos = 0;
  size_t count = 0;
};

WiFiUDP udp;
UdpActisenseStream stream;
tActisenseReader reader;
IPAddress allowedRemoteIp;
UdpNmea2000Config activeConfig;
bool started = false;
uint32_t receivedPackets = 0;
uint32_t acceptedPackets = 0;
uint32_t rejectedPackets = 0;
uint32_t receivedBytes = 0;
uint32_t droppedBytes = 0;
uint32_t decodedMessages = 0;
uint32_t lastDiagnosticsMs = 0;

void handleActisenseMessage(const tN2kMsg &message) {
  ++decodedMessages;
  N2K::processActisenseMessage(message);
}

void receiveUdpDatagrams() {
  int packetSize;
  while ((packetSize = udp.parsePacket()) > 0) {
    ++receivedPackets;
    const IPAddress sourceIp = udp.remoteIP();
    const uint16_t sourcePort = udp.remotePort();

    /*Serial.printf("UDP Actisense packet: size=%d from %s:%u\n",
            packetSize,
            sourceIp.toString().c_str(),
            static_cast<unsigned>(sourcePort));
    */
    if (sourceIp != allowedRemoteIp ||
      (activeConfig.remote_port != 0 && sourcePort != activeConfig.remote_port)) {
      ++rejectedPackets;
      while (udp.available() > 0) udp.read();
      continue;
    }

    ++acceptedPackets;
    uint8_t packet[1500];
    size_t remaining = static_cast<size_t>(packetSize);
    while (remaining > 0) {
      const size_t chunk = remaining > sizeof(packet) ? sizeof(packet) : remaining;
      const int received = udp.read(packet, chunk);
      if (received <= 0) break;
      receivedBytes += static_cast<uint32_t>(received);
      const size_t accepted = stream.append(packet, static_cast<size_t>(received));
      droppedBytes += static_cast<uint32_t>(received - accepted);
      remaining -= static_cast<size_t>(received);
    }
  }
}

} // namespace

bool udp_actisense_begin(void) {
  if (!activeConfig.enabled) {
    Serial.println("UDP Actisense: disabled");
    return false;
  }

  allowedRemoteIp.fromString(activeConfig.remote_ip);
  if (allowedRemoteIp == IPAddress(0, 0, 0, 0)) {
    Serial.println("UDP Actisense: invalid remote IP");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(activeConfig.wifi_ssid, activeConfig.wifi_password);

  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         static_cast<uint32_t>(millis() - startedAt) < N2K_UDP_WIFI_CONNECT_TIMEOUT_MS) {
    delay(100);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("UDP Actisense: Wi-Fi connection failed");
    return false;
  }

  Serial.print("UDP Actisense: Wi-Fi gateway ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("UDP Actisense: Wi-Fi RSSI ");
  Serial.println(WiFi.RSSI());

  if (!udp.begin(activeConfig.local_port)) {
    Serial.println("UDP Actisense: local port bind failed");
    return false;
  }

  reader.SetReadStream(&stream);
  reader.SetMsgHandler(handleActisenseMessage);
  started = true;

  Serial.print("UDP Actisense: listening on ");
  Serial.print(WiFi.localIP());
  Serial.print(":");
  Serial.print(activeConfig.local_port);
  Serial.print(" from ");
  Serial.print(allowedRemoteIp);
  if (activeConfig.remote_port == 0) {
    Serial.println(":any source port");
  } else {
    Serial.print(":");
    Serial.println(activeConfig.remote_port);
  }
  return true;
}

void udp_actisense_load_config(void) {
  udp_config_load(&activeConfig);
}

void udp_actisense_process(void) {
  if (!started) return;
  receiveUdpDatagrams();
  reader.ParseMessages();

  /*const uint32_t now = millis();
  if (static_cast<uint32_t>(now - lastDiagnosticsMs) >= 2000) {
    lastDiagnosticsMs = now;
    Serial.printf("UDP Actisense stats: packets=%lu accepted=%lu rejected=%lu bytes=%lu dropped=%lu decoded=%lu buffered=%d\n",
                  static_cast<unsigned long>(receivedPackets),
                  static_cast<unsigned long>(acceptedPackets),
                  static_cast<unsigned long>(rejectedPackets),
                  static_cast<unsigned long>(receivedBytes),
                  static_cast<unsigned long>(droppedBytes),
                  static_cast<unsigned long>(decodedMessages),
                  stream.available());
  }*/
}

bool udp_actisense_connected(void) {
  return started && WiFi.status() == WL_CONNECTED;
}