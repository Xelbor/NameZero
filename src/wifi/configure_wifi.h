#pragma once
#include "esp_wifi.h"

esp_err_t esp_deauther_configure_wifi(uint8_t channel, const char *ssid = "NameZero", const char *password = "12345678");