#pragma once

#include "esp_wifi.h"

typedef uint8_t MacAddr[6];

const char ssids_for_beacon[32][32] = {
  "Network_1",
  "Free_WiFi_2",
  "Public_4",
  "WiFi_5",
  "Internet_6",
  "Access_7",
  "Hotspot_8",
  "Web_9",
  "Connect_10",
  "Loading...",
  "Searching...",
  "VIRUS.EXE",
  "Virus-Infected Wi-Fi",
  "Never Gonna Give You Up",
  "The Password Is 1234",
  "Free Public Wi-Fi",
  "No Free Wi-Fi Here",
  "Get Your Own Damn Wi-Fi",
  "It Hurts When IP",
  "Trying to Connect...",
  "No Signal Detected",
  "Searching for Wi-Fi Network…",
  "Network Connection Lost",
  "99 Problems But WiFi Ain't One",
  "There is no gods",
  "Feel The Sting Of My 802.11ac!",
  "Is it Safe?",
  "Malware Repository",
  "Taco Wi-Fi Truck",
  "Connection Failed Unexpectedly",
  "SeriousLAN",
  "NotTheWifiYoureLookingFor"
};

class PacketSender {
    public:
        esp_err_t deauth(const MacAddr ap, const MacAddr station,
                const MacAddr bssid, uint8_t reason, uint8_t channel);
        esp_err_t beacon(const char* ssid,
                uint8_t channel, bool wpa2);
        esp_err_t probe(const MacAddr mac, const char* ssid,
                uint8_t channel);
        esp_err_t raw(const uint8_t* packet, int32_t len,
                bool en_sys_seq = false);
    private:
        esp_err_t change_channel(uint8_t channel);
        uint16_t seqnum;
        uint8_t buffer[200];
};