#include <M5Cardputer.h> 
#include <M5GFX.h>

#include <string>

#include "sd_utils.h"

std::vector<std::pair<String, int>> terminalHistory;

String cmd = "";
String dirPath = "/";
String usr = "$: ";

bool isTerminal = false;
bool appsMenu = false;

void printToTerminal(const String& text, const int &color) {
    const int maxLines = 7;
    terminalHistory.push_back({text, color});

    // Ограничиваем количество строк
    if (terminalHistory.size() > maxLines) {
        terminalHistory.erase(terminalHistory.begin());
    }

    M5Cardputer.Display.fillRect(4, 30, M5Cardputer.Display.width() - 8, 85, BLACK);
    
    // Выводим строки
    int y = 30;
    for (const auto& [line, col] : terminalHistory) {
        M5Cardputer.Display.setTextColor(col);
        M5Cardputer.Display.drawString(line, 8, y);
        y += 12;
    }

    M5Cardputer.Display.setTextColor(TFT_ORANGE);
}

void handleCommands(String cmd) {
    cmd.trim();  // Убираем лишние пробелы и переводы строк

    int spaceIndex = cmd.indexOf(' ');  // Ищем первый пробел
    String command, arg;

    if (spaceIndex != -1) {
        command = cmd.substring(0, spaceIndex);  // Команда до пробела
        arg = cmd.substring(spaceIndex + 1);     // Аргумент после пробела
    } else {
        command = cmd;  // Если пробелов нет, значит, это просто команда
    }

    if (command == "help") {
        printToTerminal("List commands:", ORANGE);
        printToTerminal("help, clear, ls, cd", BLUE);
    } else if (command == "exit") {
        appsMenu = false;
        isTerminal = false;
        return;
    } else if(command == "clear") {
        terminalHistory.clear();
        M5Cardputer.Display.fillRect(4, 30, M5Cardputer.Display.width() - 8, 85, BLACK);
    } else if (command == "cd") {
        if (arg.length() > 0) {
            if (pathExists(arg)) {
                dirPath += arg;

                const int dirPathWidth = M5Cardputer.Display.textWidth(dirPath);
                const int usrWidth = M5Cardputer.Display.textWidth(usr);
                M5Cardputer.Display.fillRect(0, M5Cardputer.Display.height() - 18, M5Cardputer.Display.width(), 18, BLACK);
                M5Cardputer.Display.drawString(dirPath + "/" + usr, 5, M5Cardputer.Display.height() - 14);
            } else {
                printToTerminal("Path is not exists", RED);
            }
        } else {
            printToTerminal("Path not specifed.", RED);
        }
    } else if(command == "ls") {
        if (arg.length() > 0) {
            if (pathExists(arg)) {
                listDir(SD, arg.c_str(), 0);
            } else {
                printToTerminal("Path is not exists", RED);
            }
        } else {
            if (!SD.cardType() == CARD_NONE) {
                printToTerminal("SD", BLUE);
            }
            printToTerminal("LittleFS", BLUE);
        }
    } else {
        printToTerminal("Unkown command.", RED);
    }
}

void handleTerminalInput() {
    while (appsMenu) {
        M5Cardputer.update();
        unsigned long currentTime = millis();
        static unsigned long lastScrollTime = 0;
        static unsigned long scrollDelay = 0;
        const unsigned long SCROLL_START_DELAY = 300;  // Задержка перед началом автоповтора
        const unsigned long SCROLL_REPEAT_DELAY = 50;  // Скорость автоповтора

        if (M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

            // Проверяем, нажаты ли служебные клавиши
            bool hasModifierKey = status.ctrl || status.opt || status.alt || status.fn;
            bool hasCharInput = false;

            // Проверяем, есть ли вводимые символы
            for (auto i : status.word) {
                if (i != '\0') {
                    hasCharInput = true;
                    break;
                }
            }

            if (currentTime - lastScrollTime >= scrollDelay) {
                if (!hasCharInput && hasModifierKey) { 
                    scrollDelay = 0;
                    continue;
                }

                // Обрабатываем только символы
                for (auto i : status.word) {
                    cmd += i;
                }

                if (status.opt) {
                    for (auto k : status.word) {
                      if (k == 'q') {
                        appsMenu = false;
                        isTerminal = false;
                        return;
                      }
                    }
                  }

                if (status.del) {
                    cmd.remove(cmd.length() - 1);
                }

                if (status.enter && isTerminal) {
                    M5Cardputer.Display.drawString(dirPath + usr, 5, 122);
                    printToTerminal(dirPath + usr + cmd, ORANGE);
                    handleCommands(cmd);
                    cmd.remove(0, 2);
                    cmd = "";
                }

                if (!isTerminal) {
                    isTerminal = true;
                }

                int promptWidth = M5Cardputer.Display.textWidth(dirPath + usr);
                M5Cardputer.Display.fillRect(promptWidth, M5Cardputer.Display.height() - 18, M5Cardputer.Display.width(), 25, BLACK);
                M5Cardputer.Display.drawString(cmd, 5 + promptWidth, M5Cardputer.Display.height() - 14);
                
                lastScrollTime = currentTime;
                scrollDelay = (scrollDelay == 0) ? SCROLL_START_DELAY : SCROLL_REPEAT_DELAY;
            }
        } else {
            scrollDelay = 0; // Сброс задержки, если клавиша отпущена
        }
    }
}