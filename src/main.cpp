#include <M5Cardputer.h> 
#include <M5GFX.h>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//#include "wifi/configure_wifi.h"
}
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <soc/adc_channel.h>

#include "Terminal.h"
#include "Timers.h"

#include "gui.h"
#include "wifi/wifi.h"
//#include "ble.h"

// Font
//#include "Fonts/Teletactile11.h"
#include "Fonts/Org_01.h"

#define TAG "NameZero"

#define MAINCOLOR TFT_ORANGE
#define TFT_BL 38

#define BATTERY_PIN 10

#define BATTERY_MIN_VOLTAGE 3.0
#define BATTERY_MAX_VOLTAGE 4.2
#define VOLTAGE_DIVIDER_RATIO 2.0

TaskHandle_t timerTaskHandle = NULL;

M5Canvas canvas(&M5Cardputer.Display);

static int16_t *rec_data      = nullptr;
static constexpr const size_t record_number     = 256;
static constexpr const size_t record_length     = 240;
static constexpr const size_t record_size       = record_number * record_length;
static constexpr const size_t record_samplerate = 17000;

MenuItem* currentMenu = mainMenuItems; // Текущее меню
MenuItem* parentMenu = nullptr; // Саб-меню

bool attackIsRunning = false;
bool needsRedraw = false;

bool keyboardPress = false;

int item_selected = 0; // which item in the menu is selected
int parentSelected = 0;

int item_sel_previous; // previous item - used in the menu screen to draw the item before the selected one
int item_sel_next; // next item - used in the menu screen to draw next item after the selected one

int currentMenuSize = NUM_ITEMS; // Получаем количество пунктов текущего меню
int parentMenuSize = 0; // Получаем количество пунктов родительского меню

int display_items = 5;    // Количиство отображаемых пунктов на одной странице

const unsigned long SCROLL_START_DELAY = 300;  // Задержка перед началом автоповтора
const unsigned long SCROLL_REPEAT_DELAY = 50; // Скорость автоповтора

const int screen_off_time = 20; // in seconds
unsigned long lastActivityTime = 0;
bool screenDimmed = false;
bool screenOff = false;

Timer* startMenuTimer;
Timer* batTimer;
Timer* dimTimer;

float readBatteryVoltage() {
    int raw = analogRead(BATTERY_PIN);
    float voltage = (raw / 4095.0) * 3.3;  // 12-бит АЦП на ESP32
    return voltage * VOLTAGE_DIVIDER_RATIO;
  }
  
int getBatteryPercent() {
    float voltage = readBatteryVoltage();
    int percent = (voltage - BATTERY_MIN_VOLTAGE) / 
                  (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE) * 100;
    percent = constrain(percent, 0, 100);
    return percent;
}

void updateBatteryProcents() {
    if (batTimer->status == TIMER_FINISHED)
    {
        int percent = getBatteryPercent();

        // Очищаем область батареи
        M5Cardputer.Display.fillRect(186, 8, 22, 12, BLACK);
        M5Cardputer.Display.fillRect(215, 10, 17, 9, BLACK);

        // Процентная заливка
        int fillWidth = map(percent, 0, 100, 0, 17);
        M5Cardputer.Display.drawBitmap(210, 7, image_battery_empty_bits, 24, 16, MAINCOLOR);
        int fillX = 215 + (17 - fillWidth);
        M5Cardputer.Display.fillRect(fillX, 10, fillWidth, 9, MAINCOLOR);

        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setFont(&Org_01);
        M5Cardputer.Display.drawString(String(percent) + "%", 188, 12);

        batTimer = startTimer(5000, 1);
    }
}

void startMenu() {
    M5Cardputer.Display.clear();
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setFont(&FreeMonoBoldOblique12pt7b);
    
    String text = "NameZero";
    int16_t textWidth = M5Cardputer.Display.textWidth(text);
    int16_t textHeight = M5Cardputer.Display.fontHeight();
    
    int16_t centerX = (M5Cardputer.Display.width() - textWidth) / 2;
    int16_t centerY = (M5Cardputer.Display.height() - textHeight) / 2;

    M5Cardputer.Display.drawString(text, centerX, centerY);
}

void drawUpperMenu() {
    M5Cardputer.Display.fillRect(0, 0, M5Cardputer.Display.width(), 25, BLACK);
    M5Cardputer.Display.setTextSize(1);
    int screenWidth = M5Cardputer.Display.width();
    int screenHeight = M5Cardputer.Display.height();

    M5Cardputer.Display.setFont(&FreeMonoBoldOblique12pt7b);
    M5Cardputer.Display.drawString("NameZero", screenWidth * 0.03, screenHeight * 0.03);

    // Заряд батареи
    int percent = getBatteryPercent();
    int fillWidth = map(percent, 0, 100, 0, 17);

    M5Cardputer.Display.drawBitmap(210, 7, image_battery_empty_bits, 24, 16, MAINCOLOR);
    int fillX = 215 + (17 - fillWidth);
    M5Cardputer.Display.fillRect(fillX, 10, fillWidth, 9, MAINCOLOR);

    // Другие иконки
    if (!SD.cardType() == CARD_NONE) M5Cardputer.Display.drawBitmap(168, 7, image_sd_icon_bits, 14, 16, MAINCOLOR);
    if (wifi_softAP) M5Cardputer.Display.drawBitmap(147, 7, image_wifi5_icon_bits, 19, 16, MAINCOLOR);
    if (ble_server) M5Cardputer.Display.drawBitmap(132, 7, image_bluetooth_on_icon_bits, 14, 16, MAINCOLOR);

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setFont(&Org_01);
    M5Cardputer.Display.drawString(String(percent) + "%", 188, 12);
    M5Cardputer.Display.drawLine(0, 25, 266, 25, MAINCOLOR);  // Limitter Line
}

void drawMainMenu() {
    M5Cardputer.Display.fillRect(0, 26, M5Cardputer.Display.width(), M5Cardputer.Display.height() - 26, BLACK);  // Пытаемся избежать полной очистки экрана чтобы получилось все без мерцаний
    M5Cardputer.Display.setFont(&fonts::Font0);
    M5Cardputer.Display.setTextSize(1);

    // Выбираемая иконка
    int iconCenterX = (M5Cardputer.Display.width() - mainMenuItems[item_selected].large_icon->width) / 2;
    int iconCenterY = (M5Cardputer.Display.height() - mainMenuItems[item_selected].large_icon->height) / 2;

    // Вычисляем координаты для соседних значков
    int spacing = 27;  // Расстояние между иконками

    item_sel_previous = (item_selected - 1 + currentMenuSize) % currentMenuSize;
    item_sel_next = (item_selected + 1) % currentMenuSize;

    int prevIconX = iconCenterX - mainMenuItems[item_sel_previous].icon->width - spacing;
    int prevIconY = iconCenterY + (mainMenuItems[item_selected].large_icon->height - mainMenuItems[item_sel_previous].icon->height) / 2;  // Выравниваем по центру

    int nextIconX = iconCenterX + mainMenuItems[item_selected].large_icon->width + spacing;
    int nextIconY = iconCenterY + (mainMenuItems[item_selected].large_icon->height - mainMenuItems[item_sel_next].icon->height) / 2;

    // Вычисляем координаты текста
    const char* prevText = mainMenuItems[item_sel_previous].name;
    const char* selectedItemText = mainMenuItems[item_selected].name;
    const char* nextText = mainMenuItems[item_sel_next].name;

    int textWidth = M5Cardputer.Display.textWidth(selectedItemText);
    int textHeight = M5Cardputer.Display.fontHeight();
    int textCenterX = iconCenterX + (mainMenuItems[item_selected].large_icon->width / 2) - (textWidth / 2);
    int textCenterY = (M5Cardputer.Display.height() - textHeight) / 2;

    int prevTextWidth = M5Cardputer.Display.textWidth(prevText);
    int prevTextX = prevIconX + (mainMenuItems[item_sel_next].icon->width / 2) - (prevTextWidth / 2);
    int prevTextY = prevIconY + mainMenuItems[item_sel_next].icon->height + 5;  // Отступ вниз

    int nextTextWidth = M5Cardputer.Display.textWidth(nextText);
    int nextTextX = nextIconX + (mainMenuItems[item_sel_next].icon->width / 2) - (nextTextWidth / 2);
    int nextTextY = nextIconY + mainMenuItems[item_sel_next].icon->height + 5;

    M5Cardputer.Display.drawBitmap(10, 65, image_ArrowLeft_icon_bits, 8, 14, MAINCOLOR);

    // Предыдущий пункт
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.drawBitmap(prevIconX, prevIconY + 6, mainMenuItems[item_sel_previous].icon->data, mainMenuItems[item_sel_previous].icon->width, mainMenuItems[item_sel_previous].icon->height, MAINCOLOR);
    M5Cardputer.Display.drawString(prevText, prevTextX, prevTextY + 10);

    // Выбранный пункт
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.drawBitmap(iconCenterX, iconCenterY + 6, mainMenuItems[item_selected].large_icon->data, mainMenuItems[item_selected].large_icon->width, mainMenuItems[item_selected].large_icon->height, MAINCOLOR);
    M5Cardputer.Display.drawString(selectedItemText, textCenterX - (textWidth / 2) + 2, textCenterY + 47);

    // Следующий пункт
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.drawBitmap(nextIconX, nextIconY + 6, mainMenuItems[item_sel_next].icon->data, mainMenuItems[item_sel_next].icon->width, mainMenuItems[item_sel_next].icon->height, MAINCOLOR);
    M5Cardputer.Display.drawString(nextText, nextTextX, nextTextY + 10);

    M5Cardputer.Display.drawBitmap(220, 65, image_ArrowRight_icon_bits, 8, 14, MAINCOLOR);

}

void drawMenu(MenuItem* menu, int menuSize) {
    M5Cardputer.Display.fillRect(0, 26, M5Cardputer.Display.width(), M5Cardputer.Display.height() - 26, BLACK);
    M5Cardputer.Display.setTextSize(1);

    // Получаем ширину экрана
    int screenWidth = M5Cardputer.Display.width();
    int screenHeight = M5Cardputer.Display.height();

    drawUpperMenu();
    M5Cardputer.Display.setTextColor(MAINCOLOR);

    int start_index;
    int x_offset = 40;  // Начальная кордината оси x
    int y_offset = 29;  // Начальная кордината оси y

    if (menuSize > display_items - 1) {
        start_index = item_selected - display_items / 2;
        if (start_index < 0) {
            start_index += menuSize;
        }
    } else {
        start_index = 0;
    }

    for (int i = 0; i < min(display_items, menuSize); i++) {;
        int current_index;

        if (menuSize > display_items - 1) {
            current_index = (start_index + i) % menuSize;
        } else {
            current_index = start_index + i;
        }

        String itemText = menu[current_index].name;

        // Получаем ширину и высоту текста
        int16_t textWidth = M5Cardputer.Display.textWidth(itemText);
        int16_t textHeight = M5Cardputer.Display.fontHeight();

        int16_t boxHeight = textHeight + 11;

        if (current_index == item_selected) {
            M5Cardputer.Display.drawRoundRect(5, 66, 210, boxHeight, 4, MAINCOLOR);
            M5Cardputer.Display.setFont(&fonts::Font0);
            if (textWidth > screenWidth / 2) {
                y_offset += 5;
                M5Cardputer.Display.setTextSize(2);
            } else {
                M5Cardputer.Display.setTextSize(3);
            }
        } else {
            M5Cardputer.Display.setFont(&fonts::Font0);
            M5Cardputer.Display.setTextSize(2);
        }

        // Отрисовка текста и иконки
        if (menu[current_index].getParamValue) {
            itemText += "  <" + String(menu[current_index].getParamValue()) + ">";
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

        if (menu[current_index].icon) {
            M5Cardputer.Display.drawBitmap(14, y_offset + 2, menu[current_index].icon->data, menu[current_index].icon->width, menu[current_index].icon->height, MAINCOLOR);
        } else {
            x_offset = 20;
        }
    
        M5Cardputer.Display.drawString(itemText, x_offset, y_offset);

        // Увеличиваем отступ, учитывая дополнительное пространство для выбранного элемента
        if (current_index == item_selected) {
            y_offset += 20 + 5; // Больше отступ для выделенного элемента
        } else {
            y_offset += 20; // Стандартный отступ
        }
    }

    if (menuSize > 2) {
        // Отображение фона и ползунка скроллбара
        M5Cardputer.Display.drawBitmap(220, 25, bitmap_scrollbar_background, 18, 115, MAINCOLOR);

        int scrollbar_area_height = 115; // Высота области прокрутки
        int scrollbar_top = 25;          // Начальная координата области прокрутки
        int scrollbar_height = scrollbar_area_height / menuSize + 5; // Высота ползунка
        int scrollbar_y = scrollbar_top + ((scrollbar_area_height - scrollbar_height) * item_selected / (menuSize - 1));
        M5Cardputer.Display.fillRect(232, scrollbar_y, 5, scrollbar_height, MAINCOLOR);
    }
}

void drawAttackMenu() {
    appsMenu = true;
    drawUpperMenu();

    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.drawString("The Attack has been", 25, 109);
    M5Cardputer.Display.drawString("started", 100, 130);
    M5Cardputer.Display.drawBitmap(131, 150, image_EviSmile_bits, 18, 21, MAINCOLOR);
}

void drawTerminal() {
    appsMenu = true;
    M5Cardputer.Display.clear();
    drawUpperMenu();
    M5Cardputer.Display.setFont(&fonts::Font0);
    M5Cardputer.Display.setTextSize(1);
    printToTerminal("Press opt+q to quit or type \"exit\"", ORANGE);
    printToTerminal("Type \"help\" to print help message", ORANGE);
    M5Cardputer.Display.drawLine(0, 115, 240, 115, MAINCOLOR);
    M5Cardputer.Display.drawString(dirPath + "$:", 5, 122);                     // directory

    handleTerminalInput();
}

void drawMicTest() {
    M5Cardputer.Display.clear();
    static int16_t prev_y[record_length];
    static int16_t prev_h[record_length];
    static size_t rec_record_idx  = 2;
    static size_t draw_record_idx = 0;

    while(appsMenu) {
        M5Cardputer.update();

        if (M5Cardputer.Keyboard.isKeyPressed('`') || M5Cardputer.BtnA.wasClicked()) {
            appsMenu = false;
        }

        if (M5Cardputer.Mic.isEnabled()) {
            static constexpr int shift = 6;
            auto data = &rec_data[rec_record_idx * record_length];

            if (M5Cardputer.Mic.record(data, record_length, record_samplerate)) {
                data = &rec_data[draw_record_idx * record_length];

                int32_t w = M5Cardputer.Display.width();
                if (w > record_length - 1) {
                    w = record_length - 1;
                }

                for (int32_t x = 0; x < w; ++x) {
                    M5Cardputer.Display.writeFastVLine(x, prev_y[x], prev_h[x], TFT_BLACK);

                    int32_t y1 = (data[x] >> shift);
                    int32_t y2 = (data[x + 1] >> shift);
                    if (y1 > y2) std::swap(y1, y2);

                    int32_t y = (M5Cardputer.Display.height() >> 1) + y1;
                    int32_t h = ((M5Cardputer.Display.height()) >> 1) + y2 + 1 - y;

                    prev_y[x] = y;
                    prev_h[x] = h;

                    M5Cardputer.Display.writeFastVLine(x, prev_y[x], prev_h[x], TFT_ORANGE);
                }

                M5Cardputer.Display.display();

                if (++draw_record_idx >= record_number) draw_record_idx = 0;
                if (++rec_record_idx >= record_number) rec_record_idx = 0;
            }
        }
    }
}

void drawWiFiNetworksMenu() {
    M5Cardputer.Display.fillRect(0, 26, 240, 109, BLACK);
    M5Cardputer.Display.setTextFont(1);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.drawString("Scanning...", 5, 37);
    
    wifi_scanning = true;
    wifi_scan();

    // Очищаем предыдущее меню, если оно было создано
    if (networksMenu) {
        delete[] networksMenu;
    }

    // Создаем новое меню
    networksMenu = new MenuItem[wifi_ap_count];

    MenuItem* selectedSubMenu = wifi_connect_sta ? nullptr : attackSubMenu;

    for (int i = 0; i < wifi_ap_count; i++) {
        networksMenu[i].name = wifi_aps[i].ssid;
        networksMenu[i].icon = get_wifi_icon(wifi_aps[i].rssi);
        networksMenu[i].subItems = selectedSubMenu;
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

    drawMenu(currentMenu, currentMenuSize);
}

void startSoftAP() {
    if (wifi_softAP) {
        wifi_init_softap();
    } else {
        ESP_ERROR_CHECK(esp_wifi_stop());
    }
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
    appsMenu = false;
    attackIsRunning = false;

    vTaskDelay(pdMS_TO_TICKS(100));
    vTaskDelete(NULL);

    drawMenu(currentMenu, currentMenuSize);*/
}

void startAttack() {/*
    xTaskCreatePinnedToCore(startAttackTask, "StartAttack", 4096, NULL, 0, &attackTask, 0);*/  // Приоритет 0
}

void handleKeyboard() {
    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
    unsigned long currentTime = millis();
    static unsigned long lastScrollTime = 0;
    static unsigned long scrollDelay = 0;
    if (currentMenu == mainMenuItems && !appsMenu && !valueEdit) {
        if (M5Cardputer.Keyboard.isKeyPressed('/') || M5Cardputer.Keyboard.isKeyPressed(',')) {
            if (currentTime - lastScrollTime >= scrollDelay) {
                if (M5Cardputer.Keyboard.isKeyPressed('/')) {
                    item_selected++;
                    if (item_selected >= currentMenuSize) item_selected = 0;
                }

                if (M5Cardputer.Keyboard.isKeyPressed(',')) {
                    item_selected--;
                    if (item_selected < 0) item_selected = currentMenuSize - 1;
                }

                needsRedraw = true;
                lastScrollTime = currentTime;
                scrollDelay = (scrollDelay == 0) ? SCROLL_START_DELAY : SCROLL_REPEAT_DELAY;
            }
        } else {
            scrollDelay = 0; // Сброс задержки, если клавиша отпущена
        }
    } else if (!appsMenu && !valueEdit) {
        if (M5Cardputer.Keyboard.isKeyPressed('.') || M5Cardputer.Keyboard.isKeyPressed(';')) {
            if (currentTime - lastScrollTime >= scrollDelay) {
                if (M5Cardputer.Keyboard.isKeyPressed('.')) { 
                    item_selected++;
                    if (item_selected >= currentMenuSize) item_selected = 0;
                }

                if (M5Cardputer.Keyboard.isKeyPressed(';')) { 
                    item_selected--;
                    if (item_selected < 0) item_selected = currentMenuSize - 1;
                }

                needsRedraw = true;
                lastScrollTime = currentTime;
                scrollDelay = (scrollDelay == 0) ? SCROLL_START_DELAY : SCROLL_REPEAT_DELAY;
            }
        } else {
            scrollDelay = 0; // Сброс задержки, если клавиша отпущена
        }
    }

    if (M5Cardputer.Keyboard.isChange()) {
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
            if (wifi_scanning && currentMenu == networksMenu) {
                wifi_scanning = false;
            }

            if (currentMenu[item_selected].trigger) {
                *(currentMenu[item_selected].trigger) = !*(currentMenu[item_selected].trigger);
                if (currentMenu[item_selected].switchable) {
                    currentMenu[item_selected].icon = getMenuItemIconStatus(currentMenu[item_selected].trigger);
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

        if (M5Cardputer.Keyboard.isKeyPressed('`') || status.del || M5Cardputer.BtnA.wasClicked()) {
            if (parentMenu) {
                currentMenu = parentMenu;
                currentMenuSize = parentMenuSize;
                item_selected = parentSelected;
                parentMenu = nullptr;

                wifi_scanning = false;
                wifi_deauther_spamer_target = false;
                wifi_beacon_spamer = false;
                appsMenu = false;
                attackIsRunning = false;
                drawUpperMenu();
            } else {
                currentMenu = mainMenuItems;
                currentMenuSize = NUM_ITEMS;
            }
            needsRedraw = true;
        }
    }

    if (needsRedraw && currentMenu != mainMenuItems && !appsMenu) {
        drawMenu(currentMenu, currentMenuSize);
        needsRedraw = false;
    } else if(needsRedraw && currentMenu == mainMenuItems && !appsMenu) {
        drawMainMenu();
        needsRedraw = false;
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
    pinMode(BATTERY_PIN, INPUT);

    // Инициализация
    auto display_cfg = M5.config();
    M5Cardputer.begin(display_cfg, true);

    M5Cardputer.Display.startWrite();
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextColor(MAINCOLOR);
    M5Cardputer.Display.setBrightness(255);
    M5Cardputer.Speaker.setVolume(0);

    rec_data = (int16_t*) heap_caps_malloc(record_size * sizeof(int16_t), MALLOC_CAP_8BIT);
    memset(rec_data, 0, record_size * sizeof(int16_t));

    M5Cardputer.Speaker.end();
    M5Cardputer.Mic.begin();
    initializeSD();

    startMenuTimer = startTimer(2000, 0);

    startMenu();
    while (startMenuTimer->status == TIMER_RUNNING) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isPressed()) {
            startMenuTimer->status = TIMER_FINISHED;
        }
    }

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

    // Выводим страницы
    M5Cardputer.Display.clear();
    drawUpperMenu();
    drawMainMenu();
    batTimer = startTimer(5000, 1);
    dimTimer = startTimer(screen_off_time / 2, 1);
}

void loop() {
    M5Cardputer.update();
    updateBatteryProcents();
    handleKeyboard();
    delay(50);
}