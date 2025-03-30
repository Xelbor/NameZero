#include "configure_wifi.h"
#include <cstring> // для memcpy

esp_err_t esp_deauther_configure_wifi(uint8_t channel, const char *ssid, const char *password) {
    wifi_config_t ap_config = {};

    if (ssid) {
        strncpy((char *)ap_config.ap.ssid, ssid, sizeof(ap_config.ap.ssid) - 1);
        ap_config.ap.ssid_len = strlen(ssid);
    }

    if (password) {
        strncpy((char *)ap_config.ap.password, password, sizeof(ap_config.ap.password) - 1);
    }

    ap_config.ap.channel = channel;
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap_config.ap.ssid_hidden = 0;
    ap_config.ap.max_connection = 4;
    ap_config.ap.beacon_interval = 60000;

    return esp_wifi_set_config(WIFI_IF_AP, &ap_config);
}
