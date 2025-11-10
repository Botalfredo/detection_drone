/*
  ESP32 - Raw WiFi Beacon sender (Vendor Specific IE 6A:5C:35, VS-Type 0x01)
  Envoie une trame beacon toutes les 100 ms et l'affiche en hex sur Serial.

  Testé avec Arduino-ESP32 core 3.3.x.
*/

#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"

#define TX_INTERVAL_MS 500
#define CHANNEL 6

// construit la trame beacon minimale dans buffer, retourne la longueur
size_t build_beacon(uint8_t *buf, size_t bufsize) {
  // SSID “cosmétique” du beacon
  const char *ssid = "ESP_BEACON_TEST";
  uint8_t ssid_len = strlen(ssid);

  // Vendor Specific payload TLV (exemple: version TLV)
  // TLV: T=0x01, L=0x01, V=0x01 (protocole version = 1)
  const uint8_t vs_tlv[] = { 0x01, 0x01, 0x01 };

  // OUI et VS-Type
  const uint8_t oui[] = { 0x6A, 0x5C, 0x35 };
  const uint8_t vs_type = 0x01;

  // MAC AP
  uint8_t mac[6];
  // Récupère la MAC de l’interface AP (plus portable que esp_read_mac)
  esp_wifi_get_mac(WIFI_IF_AP, mac);

  size_t pos = 0;

  // --- 802.11 Management header ---
  // Frame Control: beacon (subtype 8, type mgmt) -> 0x80 0x00 (little endian)
  if (pos + 2 > bufsize) return 0;
  buf[pos++] = 0x80;
  buf[pos++] = 0x00;

  // Duration
  if (pos + 2 > bufsize) return 0;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;

  // Addr1: Broadcast
  if (pos + 6 > bufsize) return 0;
  for (int i = 0; i < 6; ++i) buf[pos++] = 0xFF;

  // Addr2: Source MAC (notre MAC AP)
  if (pos + 6 > bufsize) return 0;
  for (int i = 0; i < 6; ++i) buf[pos++] = mac[i];

  // Addr3: BSSID (même que source)
  if (pos + 6 > bufsize) return 0;
  for (int i = 0; i < 6; ++i) buf[pos++] = mac[i];

  // Sequence control
  if (pos + 2 > bufsize) return 0;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;

  // --- Fixed parameters ---
  // Timestamp (8 bytes) - 0, le matériel peut surcharger
  if (pos + 8 > bufsize) return 0;
  for (int i = 0; i < 8; ++i) buf[pos++] = 0x00;

  // Beacon Interval (100 TU) -> 0x64 0x00
  if (pos + 2 > bufsize) return 0;
  buf[pos++] = 0x64;
  buf[pos++] = 0x00;

  // Capability info (ESS=1, no privacy) => 0x01 0x00
  if (pos + 2 > bufsize) return 0;
  buf[pos++] = 0x01;
  buf[pos++] = 0x00;

  // --- Tagged parameters ---

  // 1) SSID
  if (pos + 2 + ssid_len > bufsize) return 0;
  buf[pos++] = 0x00;         // Tag: SSID
  buf[pos++] = ssid_len;     // Length
  memcpy(&buf[pos], ssid, ssid_len);
  pos += ssid_len;

  // 2) Supported Rates - 1 Mbps basic (0x82)
  if (pos + 3 > bufsize) return 0;
  buf[pos++] = 0x01; // Supported Rates
  buf[pos++] = 0x01; // Length
  buf[pos++] = 0x82; // 1 Mbps basic

  // 3) DS Parameter set (channel)
  if (pos + 3 > bufsize) return 0;
  buf[pos++] = 0x03; // DS Parameter Set
  buf[pos++] = 0x01; // Length
  buf[pos++] = CHANNEL;

  // 4) Vendor Specific IE (Tag 221 / 0xDD)
  uint8_t payload_len = sizeof(vs_tlv); // 3 bytes (T,L,V)
  uint8_t vendor_len = 4 + payload_len; // OUI(3) + VS-type(1) + payload

  if (pos + 2 + vendor_len > bufsize) return 0;
  buf[pos++] = 0xDD;         // Tag: Vendor Specific
  buf[pos++] = vendor_len;   // Length

  // OUI
  memcpy(&buf[pos], oui, 3);
  pos += 3;

  // VS-Type
  buf[pos++] = vs_type;

  // payload TLV
  memcpy(&buf[pos], vs_tlv, payload_len);
  pos += payload_len;

  return pos;
}

void print_hex(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
    if (i + 1 < len) Serial.print(' ');
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(300);

  // Mode AP requis pour esp_wifi_80211_tx sur l’interface AP
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false); // évite les surprises d’horloge/sleep
  WiFi.softAP("ESP32_BEACON",  nullptr, CHANNEL, false, 1);

  delay(200);

  Serial.println();
  Serial.println("Raw beacon sender starting...");
  Serial.print("Channel: ");
  Serial.println(CHANNEL);

  // Affiche la MAC AP
  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_AP, mac);
  Serial.print("AP MAC: ");
  for (int i = 0; i < 6; ++i) {
    if (mac[i] < 0x10) Serial.print('0');
    Serial.print(mac[i], HEX);
    if (i < 5) Serial.print(':');
  }
  Serial.println();
}

void loop() {
  uint8_t frame[256];
  size_t frame_len = build_beacon(frame, sizeof(frame));
  if (frame_len == 0) {
    Serial.println("Error: frame too large or build failed");
    delay(1000);
    return;
  }

  Serial.print("TX beacon (len=");
  Serial.print(frame_len);
  Serial.println(") :");
  print_hex(frame, frame_len);

  // Envoi brut via l’interface AP
  esp_err_t res = esp_wifi_80211_tx(WIFI_IF_AP, frame, frame_len, true);
  if (res != ESP_OK) {
    Serial.print("esp_wifi_80211_tx failed: ");
    Serial.println((int)res);
  }

  delay(TX_INTERVAL_MS);
}
