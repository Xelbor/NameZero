extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//#include "wifi/configure_wifi.h"
}

#include "gui.h"
//#include "wifi/wifi.h"
//#include "ble.h"
#include <M5Cardputer.h>
#include <M5GFX.h>

#include <cstring>

// Font
#include "Fonts/Teletactile11.h"

#define TAG "NameZero"

#define MAINCOLOR TFT_ORANGE

TaskHandle_t handleEncoderTask;
TaskHandle_t redrawTaskHandle;
TaskHandle_t attackTask;

M5GFX display;

MenuItem* currentMenu = mainMenuItems; // Текущее меню
MenuItem* parentMenu = nullptr; // Саб-меню

bool attackIsRunning = false;
bool needsRedraw = false;

int item_selected = 0; // which item in the menu is selected
int parentSelected = 0;

int item_sel_previous; // previous item - used in the menu screen to draw the item before the selected one
int item_sel_next; // next item - used in the menu screen to draw next item after the selected one

int currentMenuSize = NUM_ITEMS; // Получаем количество пунктов текущего меню
int parentMenuSize = 0; // Получаем количество пунктов родительского меню

int display_items = 7;    // Количиство отображаемых пунктов на одной странице

void startMenu() {
    display.fillScreen(TFT_BLACK);
    display.setTextSize(2);
    display.setFont(&FreeMonoBoldOblique12pt7b);
    
    String text = "NameZero";
    int16_t textWidth = display.textWidth(text);
    int16_t textHeight = display.fontHeight();
    
    int16_t centerX = (display.width() - textWidth) / 2;
    int16_t centerY = (display.height() - textHeight) / 2;

    display.drawString(text, centerX, centerY);
}

void drawMenu(MenuItem* menu, int menuSize) {
    display.fillScreen(TFT_BLACK);
    display.setTextSize(1);
    display.setFont(&Teletactile11pt7b);

    // Получаем ширину экрана
    int screenWidth = display.width();
    int screenHeight = display.height();

    display.drawString("NameZero", screenWidth * 0.03, screenHeight * 0.03);

    int iconX = screenWidth * 0.875;
    int iconY = screenHeight * 0.03;

    display.drawBitmap(iconX, iconY, image_battery_icon_bits, 24, 16, MAINCOLOR);
    
    if (!wifi_softAP) {
        display.drawBitmap(iconX - 22, iconY, image_disabled_wifi_icon_bits, 19, 16, MAINCOLOR);
    } else {
        display.drawBitmap(iconX - 22, iconY, image_wifi5_icon_bits, 19, 16, MAINCOLOR);
    }
    
    if (!ble_server) {
        display.drawBitmap(iconX - 37, iconY, image_disabled_bluetooth_icon_bits, 14, 16, MAINCOLOR);
    } else {
        display.drawBitmap(iconX - 37, iconY, image_bluetooth_on_icon_bits, 14, 16, MAINCOLOR);
    }

    display.drawLine(0, screenHeight * 0.1, screenWidth, screenHeight * 0.1, MAINCOLOR);

    int start_index = (menuSize > 6) ? (item_selected - display_items / 2 + menuSize) % menuSize : 0;
    int y_offset = screenHeight * 0.12;

    if (menuSize > 6) {
        start_index = item_selected - display_items / 2;
        if (start_index < 0) {
            start_index += menuSize;
        }
    } else {
        start_index = 0;
    }

    for (int i = 0; i < min(display_items, menuSize); i++) {;
        int current_index = (start_index + i) % menuSize;

        if (menuSize > 6) {
            current_index = (start_index + i) % menuSize;
        } else {
            current_index = start_index + i;
        }

        // Настройка шрифта: выделение для выбранного пункта
        if (current_index == item_selected) {
            display.drawRoundRect(screenWidth * 0.04, y_offset - 5, screenWidth * 0.8, screenHeight * 0.1, 4, MAINCOLOR);
            display.setTextFont(1);
            display.setTextSize(2);
        } else {
            display.setTextFont(1);
            display.setTextSize(1);
        }

        // Отрисовка текста и иконки
        String itemText = menu[current_index].name;

        if (menu[current_index].getParamValue) {
            itemText += " <" + String(menu[current_index].getParamValue()) + ">";
        }

        if (menu[current_index].value) {
            itemText += "<";
            if (*menu[current_index].value == 0) {
                itemText += "Unlim";
            } else {
                itemText += String(*menu[current_index].value); // Разыменовываем указатель и конвертируем в строку
            }
            itemText += ">";
        }

        int x_offset = screenWidth * 0.15;

        if (menu[current_index].icon) {
            display.drawBitmap(screenWidth * 0.06, y_offset, menu[current_index].icon->data, menu[current_index].icon->width, menu[current_index].icon->height, MAINCOLOR);
        } else {
            x_offset = screenWidth * 0.08;
        }
    
        display.drawString(itemText, x_offset, y_offset);

        // Увеличиваем отступ, учитывая дополнительное пространство для выбранного элемента
        if (current_index == item_selected) {
            y_offset += screenHeight * 0.08 + 5; // Больше отступ для выделенного элемента
        } else {
            y_offset += screenHeight * 0.08; // Стандартный отступ
        }
    }

    if (menuSize > 2) {
        // Отображение фона и ползунка скроллбара
        display.drawBitmap(screenWidth * 0.92, screenHeight * 0.15, bitmap_scrollbar_background, 18, 140, MAINCOLOR);

        int scrollbar_area_height = 140; // Высота области прокрутки
        int scrollbar_top = 50;          // Начальная координата области прокрутки
        int scrollbar_height = scrollbar_area_height / menuSize + 5; // Высота ползунка
        int scrollbar_y = scrollbar_top + ((scrollbar_area_height - scrollbar_height) * item_selected / (menuSize - 1));
        display.fillRect(264, scrollbar_y, 5, scrollbar_height, MAINCOLOR);
    }
}

void drawAttackMenu() {
    attackMenu = true;
    display.fillScreen(TFT_BLACK);
    display.setTextSize(1);
    display.setFont(&Teletactile11pt7b);
    display.drawString("NameZero", 9, 25);
    display.drawBitmap(250, 28, image_battery_icon_bits, 24, 16, MAINCOLOR);
    if (!wifi_softAP) {
        display.drawBitmap(228, 28, image_disabled_wifi_icon_bits, 19, 16, MAINCOLOR);
    } else {
        display.drawBitmap(228, 28, image_wifi5_icon_bits, 19, 16, MAINCOLOR);
    }
    display.drawBitmap(213, 28, image_disabled_bluetooth_icon_bits, 14, 16, MAINCOLOR);
    display.drawLine(-2, 45, 278, 45, MAINCOLOR);    // limitter-line

    display.setTextFont(1);
    display.setTextSize(2);
    display.drawString("The Attack has been", 25, 109);
    display.drawString("started", 100, 130);
    display.drawBitmap(131, 150, image_EviSmile_bits, 18, 21, MAINCOLOR);
}

void drawWiFiNetworksMenu() {
  /*
    display.fillRect(0, 46, 280, 240 - 46, TFT_BLACK);
    display.setTextFont(1);
    display.setTextSize(2);
    display.drawString("Scanning...", 5, 55);
    
    wifi_scanning = true;
    wifi_scan();

    // Очищаем предыдущее меню, если оно было создано
    if (networksMenu) {
        delete[] networksMenu;
    }

    // Создаем новое меню
    networksMenu = new MenuItem[wifi_ap_count];

    for (int i = 0; i < wifi_ap_count; i++) {
        networksMenu[i].name = wifi_aps[i].ssid;
        networksMenu[i].icon = get_wifi_icon(wifi_aps[i].rssi);
        networksMenu[i].subItems = attackSubMenu;
        networksMenu[i].subItemCount = 2;
        networksMenu[i].trigger = nullptr;
        networksMenu[i].value = nullptr;
        networksMenu[i].getParamValue = nullptr;
        networksMenu[i].switchable = false;
        networksMenu[i].action = nullptr;
    }

    parentMenu = currentMenu;
    parentSelected = item_selected;
    parentMenuSize = currentMenuSize;
    currentMenu = networksMenu;
    currentMenuSize = wifi_ap_count;
    item_selected = 0;

    drawMenu(currentMenu, currentMenuSize);*/
}

void startSoftAP() {
  /*
    if (wifi_softAP) {
        wifi_init_softap();
    } else {
        ESP_ERROR_CHECK(esp_wifi_stop());
    }*/
}

void startAttackTask( void * parameter ) {
  /*
    drawAttackMenu();
    int64_t start_time = esp_timer_get_time(); // Получаем текущее время в микросекундах
    int64_t duration = (attackTime == 0) ? 3600 : attackTime;
    int64_t end_time = start_time + (duration * 1000000);


    attackIsRunning = true;
    ESP_ERROR_CHECK(nvs_flash_erase());

    const MacAddr TARGET = {   // MAC адрес на broadcast
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };
    
    const MacAddr AP = {    // MAC адрес на broadcast
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };

    while (esp_timer_get_time() < end_time && attackIsRunning) {
        if (wifi_deauther_spamer_target) {
            wifi_start_deauther(TARGET, wifi_aps[parentSelected].mac);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        } else if (wifi_deauther_spamer) {
            wifi_start_deauther(TARGET, AP);
        } else if (wifi_beacon_spamer) {
            wifi_start_beacon(ssids_for_beacon[ssidsCount], ssidsCount, 1, true);
            vTaskDelay(pdMS_TO_TICKS(50));
        } else if (wifi_ap_cloner) {
            wifi_start_beacon(wifi_aps[parentSelected].ssid, ssidsCount, 1, true);
            vTaskDelay(pdMS_TO_TICKS(50));
        } else if (wifi_probe_spamer) {
            MacAddr mac {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
            sender.probe(mac, "HiddenNetwork", 8);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    ESP_ERROR_CHECK(esp_wifi_stop());
    wifi_deauther_spamer_target = false;
    wifi_beacon_spamer = false;
    wifi_probe_spamer = false;
    attackMenu = false;
    attackIsRunning = false;

    vTaskDelay(pdMS_TO_TICKS(100));
    vTaskDelete(NULL);

    drawMenu(currentMenu, currentMenuSize);*/
}

void startAttack() {
    xTaskCreatePinnedToCore(startAttackTask, "StartAttack", 4096, NULL, 0, &attackTask, 0);  // Приоритет 0
}

void handleEncoder( void * parameter ) {
    while (true)
    {
        if (!valueEdit) {
            if (M5Cardputer.Keyboard.isKeyPressed('.')) { // Поворот вправо (вниз по меню)
                item_selected++;
                if (item_selected >= currentMenuSize) item_selected = 0;
                needsRedraw = true;
            }

            if (M5Cardputer.Keyboard.isKeyPressed(';')) { // Поворот влево (вверх по меню)
                item_selected--;
                if (item_selected < 0) item_selected = currentMenuSize - 1;
                needsRedraw = true;
            }

            if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
                if (wifi_scanning && currentMenu == networksMenu) {
                    wifi_scanning = false;
                }

                if (currentMenu[item_selected].trigger) {
                    *(currentMenu[item_selected].trigger) = !*(currentMenu[item_selected].trigger);
                    if (currentMenu[item_selected].switchable) {
                        currentMenu[item_selected].icon = getMenuItemIconStatus(currentMenu[item_selected].trigger); // Меняем иконку
                        needsRedraw = true;
                    }
                }

                if (currentMenu[item_selected].action) {
                    currentMenu[item_selected].action();
                }

                if (!wifi_scanning && currentMenu[item_selected].subItems && currentMenu[item_selected].subItemCount > 0) {
                    parentMenu = currentMenu;
                    parentSelected = item_selected;
                    parentMenuSize = currentMenuSize;
                    currentMenuSize = currentMenu[item_selected].subItemCount;
                    currentMenu = currentMenu[item_selected].subItems;
                    item_selected = 0;
                    needsRedraw = true;
                }

                needsRedraw = true;
            }

            if (M5Cardputer.Keyboard.isKeyPressed('`') && M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
                if (parentMenu) {
                    // Возвращаемся в предыдущее меню
                    currentMenu = parentMenu;
                    currentMenuSize = parentMenuSize;
                    item_selected = parentSelected;
                    parentMenu = nullptr; // Если мы вернулись в главное меню, parentMenu сбрасывается

                    // Сбрасываем ВСЕ флаги потому что выходим из меню
                    wifi_softAP = false;
                    wifi_scanning = false;
                    wifi_deauther_spamer_target = false;
                    wifi_beacon_spamer = false;
                    wifi_probe_spamer = false;
                    attackMenu = false;
                    attackIsRunning = false;
                } else {
                    // Если уже в главном меню, просто ничего не делаем
                    currentMenu = mainMenuItems; // Явно устанавливаем главное меню 
                    currentMenuSize = NUM_ITEMS;
                }
                needsRedraw = true;
            }
        } else {
            if (M5Cardputer.Keyboard.isKeyPressed('/')) {
                attackTime += 5; // Увеличиваем на 5 (можно изменить шаг)
                drawMenu(currentMenu, currentMenuSize);
            }
            if (M5Cardputer.Keyboard.isKeyPressed(',')) {
                attackTime -= 5; // Уменьшаем на 5
                if (attackTime < 0) attackTime = 0; // Не позволяем уйти в минус
                drawMenu(currentMenu, currentMenuSize);
            }
            if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) { // Повторный клик — выход из редактирования
                valueEdit = false;
                drawMenu(currentMenu, currentMenuSize);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void redrawTask(void *parameter) {
    while (true) {
        // Проверка условия для перерисовки экрана
        if (needsRedraw && !attackMenu) {
            drawMenu(currentMenu, currentMenuSize); // Перерисовка экрана
            needsRedraw = false; // Сбрасываем флаг
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Задержка для предотвращения перегрузки процессора
    }
}

/*
void getResourceUsage(float *cpu_usage, float *ram_usage) {
    static int64_t last_time = esp_timer_get_time();
    
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    *ram_usage = 100.0f * (1.0f - ((float)free_heap / (float)total_heap));
    
    int64_t start_idle_time = esp_timer_get_time();
    vTaskDelay(pdMS_TO_TICKS(1000));
    int64_t end_idle_time = esp_timer_get_time();
    
    int64_t idle_time = end_idle_time - start_idle_time;
    int64_t elapsed_time = esp_timer_get_time() - last_time;
    
    *cpu_usage = 100.0f * (1.0f - ((float)idle_time / (float)elapsed_time));
    last_time = esp_timer_get_time();
}

void resourceMonitor(void *parameter) {
    float cpu_usage, ram_usage;
    while (true) {
        getResourceUsage(&cpu_usage, &ram_usage);
        printf("CPU Usage: %.2f%%\n", cpu_usage);
        printf("RAM Usage: %.2f%%\n", ram_usage);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}*/

void setup() {
    Serial.begin(115200);

    // Инициализация дисплея
    display.init();
    display.fillScreen(TFT_BLACK);
    display.setRotation(1);
    display.setTextColor(MAINCOLOR);

    /*
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));*/

    xTaskCreatePinnedToCore(handleEncoder, "handleEncoderTask", 10000, NULL, 1, &handleEncoderTask, 1);

    xTaskCreatePinnedToCore(redrawTask, "redrawTask", 2048, NULL, 1, &redrawTaskHandle, 0);

    // Выводим страницы
    startMenu();
    vTaskDelay(pdMS_TO_TICKS(2000));
    drawMenu(currentMenu, currentMenuSize);
}

void loop() {
    
}