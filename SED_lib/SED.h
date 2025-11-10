#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"

// Structure qui contient tous les paramètres du beacon SED
struct SEDParameters {
  // --- 802.11 header fields (management beacon) ---
  uint8_t frame_ctrl[2] = { 0x80, 0x00 };  // beacon (type=0 subtype=8) little-endian
  uint16_t duration = 0x0000;
  uint8_t addr1[6];  // Destination -> Broadcast (ff:ff:ff:ff:ff:ff)
  uint8_t addr2[6];  // Source MAC (AP MAC)
  uint8_t addr3[6];  // BSSID (souvent égal à addr2)
  uint16_t seq_ctrl = 0x0000;

  // --- Fixed parameters ---
  uint8_t timestamp[8] = { 0 };       // driver/hardware may override
  uint16_t beacon_interval = 100;     // TU (100 TU -> 0x0064)
  uint16_t capability_info = 0x0001;  // ESS=1, no privacy

  // --- Tagged parameters ---
  // SSID
  char ssid[33] = { 0 };  // max 32 + '\0'
  uint8_t ssid_len = 0;

  // Supported Rates (simple)
  uint8_t supp_rates_len = 1;
  uint8_t supp_rates[8] = { 0x82 };  // 0x82 -> 1 Mbps basic

  // DS Parameter set (channel)
  uint8_t ds_channel = 6;

  // Vendor Specific IE (Tag 221 / 0xDD)
  uint8_t vendor_oui[3] = { 0x6A, 0x5C, 0x35 };  // OUI 6A:5C:35
  uint8_t vendor_vstype = 0x01;                  // VS-Type = 0x01
  // vendor payload (TLV list) - pointer + length
  const uint8_t *vendor_payload = nullptr;
  uint16_t vendor_payload_len = 0;

  // Constructeur par défaut met Broadcast in addr1
  SEDParameters() {
    for (int i = 0; i < 6; ++i) addr1[i] = 0xFF;
  }

  // Helper: set ssid
  void setSSID(const char *s) {
    size_t l = strlen(s);
    if (l > 32) l = 32;
    memcpy(ssid, s, l);
    ssid[l] = 0;
    ssid_len = (uint8_t)l;
  }

  // Helper: copy MAC (6 bytes)
  void setSourceMAC(const uint8_t mac[6]) {
    memcpy(addr2, mac, 6);
    memcpy(addr3, mac, 6);  // BSSID = same by default
  }

  // Serialization : écrit la trame dans buf (taille bufsize). Retourne longueur écrite, 0 en cas d'erreur
  size_t serialize(uint8_t *buf, size_t bufsize) const {
    size_t pos = 0;

    auto put_u8 = [&](uint8_t v) {
      if (pos + 1 > bufsize) return false;
      buf[pos++] = v;
      return true;
    };
    auto put_u16_le = [&](uint16_t v) {
      if (pos + 2 > bufsize) return false;
      buf[pos++] = (uint8_t)(v & 0xFF);
      buf[pos++] = (uint8_t)((v >> 8) & 0xFF);
      return true;
    };
    auto put_bytes = [&](const uint8_t *data, size_t len) {
      if (pos + len > bufsize) return false;
      memcpy(&buf[pos], data, len);
      pos += len;
      return true;
    };

    // 802.11 header
    if (!put_bytes(frame_ctrl, 2)) return 0;
    if (!put_u16_le(duration)) return 0;
    if (!put_bytes(addr1, 6)) return 0;
    if (!put_bytes(addr2, 6)) return 0;
    if (!put_bytes(addr3, 6)) return 0;
    if (!put_u16_le(seq_ctrl)) return 0;

    // Fixed params
    if (!put_bytes(timestamp, 8)) return 0;
    if (!put_u16_le(beacon_interval)) return 0;
    if (!put_u16_le(capability_info)) return 0;

    // --- Tag: SSID (tag 0) ---
    if (!put_u8(0x00)) return 0;      // tag
    if (!put_u8(ssid_len)) return 0;  // len
    if (ssid_len > 0 && !put_bytes((const uint8_t *)ssid, ssid_len)) return 0;

    // --- Tag: Supported Rates (tag 1) ---
    if (!put_u8(0x01)) return 0;
    if (!put_u8(supp_rates_len)) return 0;
    if (supp_rates_len > 0 && !put_bytes(supp_rates, supp_rates_len)) return 0;

    // --- Tag: DS Parameter set (tag 3) ---
    if (!put_u8(0x03)) return 0;
    if (!put_u8(0x01)) return 0;
    if (!put_u8(ds_channel)) return 0;

    // --- Tag: Vendor Specific (0xDD) ---
    // Length = OUI(3) + VS-Type(1) + vendor_payload_len
    uint16_t vendor_total_len = 3 + 1 + vendor_payload_len;
    if (vendor_total_len > 255) {
      // IE length is 1 byte, cannot exceed 255 -- error
      return 0;
    }
    if (!put_u8(0xDD)) return 0;                       // Tag vendor
    if (!put_u8((uint8_t)vendor_total_len)) return 0;  // Length
    if (!put_bytes(vendor_oui, 3)) return 0;           // OUI
    if (!put_u8(vendor_vstype)) return 0;              // VS-Type
    if (vendor_payload_len > 0 && !put_bytes(vendor_payload, vendor_payload_len)) return 0;

    return pos;
  }
};