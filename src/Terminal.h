#include <M5Cardputer.h> 
#include <M5GFX.h>

#include <FS.h>

#include <string>

#include "sd_utils.h"

struct ColoredLine {
    std::vector<String> fragments;
    std::vector<int> colors;
};

std::vector<ColoredLine> terminalHistory;

String dirPath = "/";
String usr = "$: ";

bool isTerminal = false;
bool appsMenu = false;

void printToTerminal(const String& text, const int &color) {
    const int maxLines = 7;
    terminalHistory.push_back({{text}, {color}});

    if (terminalHistory.size() > maxLines) {
        terminalHistory.erase(terminalHistory.begin());
    }

    M5Cardputer.Display.fillRect(4, 30, M5Cardputer.Display.width() - 8, 85, BLACK);

    int y = 30;
    for (const auto& line : terminalHistory) {
        int x = 8;
        for (size_t i = 0; i < line.fragments.size(); ++i) {
            M5Cardputer.Display.setTextColor(line.colors[i]);
            M5Cardputer.Display.drawString(line.fragments[i], x, y);
            x += M5Cardputer.Display.textWidth(line.fragments[i]);
        }
        y += 12;
    }

    M5Cardputer.Display.setTextColor(TFT_ORANGE);
}

void printListToTerminal(std::vector<DirEntry> entries) {
    std::sort(entries.begin(), entries.end(), dirAndAlphaSort);

    const int maxWidth = M5Cardputer.Display.width() - 16;
    const int spacing = 10;

    std::vector<String> names;
    std::vector<int> colors;
    int lineWidth = 0;
    int y = 30 + terminalHistory.size() * 12;
    int x = 8;

    for (const auto& entry : entries) {
        String name = entry.path;
        if (name.startsWith("/")) name = name.substring(1);
        name += "  ";

        int nameWidth = M5Cardputer.Display.textWidth(name);

        if (lineWidth + nameWidth > maxWidth) {
            x = 8;
            for (size_t i = 0; i < names.size(); ++i) {
                M5Cardputer.Display.setTextColor(colors[i]);
                M5Cardputer.Display.drawString(names[i], x, y);
                x += M5Cardputer.Display.textWidth(names[i]);
            }

            terminalHistory.push_back({names, colors});
            if (terminalHistory.size() > 7) {
                terminalHistory.erase(terminalHistory.begin());
            }

            y += 12;
            names.clear();
            colors.clear();
            lineWidth = 0;
        }

        names.push_back(name);
        colors.push_back(entry.isDirectory ? TFT_BLUE : TFT_GREEN);
        lineWidth += nameWidth;
    }

    if (!names.empty()) {
        x = 8;
        for (size_t i = 0; i < names.size(); ++i) {
            M5Cardputer.Display.setTextColor(colors[i]);
            M5Cardputer.Display.drawString(names[i], x, y);
            x += M5Cardputer.Display.textWidth(names[i]);
        }

        terminalHistory.push_back({names, colors});
        if (terminalHistory.size() > 7) {
            terminalHistory.erase(terminalHistory.begin());
        }
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
            if (!arg.startsWith("/")) {
                arg = "/" + arg;
            }
            if (pathExists(arg)) {
                dirPath = arg;

                const int dirPathWidth = M5Cardputer.Display.textWidth(dirPath);
                const int usrWidth = M5Cardputer.Display.textWidth(usr);
                M5Cardputer.Display.fillRect(0, M5Cardputer.Display.height() - 18, M5Cardputer.Display.width(), 18, BLACK);
                if (!dirPath.endsWith("/")) {
                    dirPath = dirPath + "/";
                }
                M5Cardputer.Display.drawString(dirPath + usr, 5, M5Cardputer.Display.height() - 14);
            } else {
                printToTerminal("Path is not exists", RED);
            }
        } else {
            printToTerminal("Path not specifed.", RED);
        }
    } else if(command == "ls") {
        if (arg.length() > 0) {
            if (pathExists(arg)) {
                printListToTerminal(listDir(SD, arg.c_str(), 0));
            } else {
                printToTerminal("Path is not exists", RED);
            }
        } else {
            printListToTerminal(listDir(SD, dirPath.c_str(), 0));
            /*
            if (!SD.cardType() == CARD_NONE) {
                printToTerminal("SD", BLUE);
            }
            printToTerminal("LittleFS", BLUE);*/
        }
    } else {
        printToTerminal("Unkown command.", RED);
    }
}

String handleInputKeyboard() {
    static String data = "";
    bool cursorVisible = true;
    static int cursorPos = 0;
    unsigned long lastCursorBlink = 0;
    const unsigned long cursorBlinkInterval = 700; // миллисекунд
    const unsigned long SCROLL_START_DELAY = 300;  // Задержка перед началом автоповтора
    const unsigned long SCROLL_REPEAT_DELAY = 50;  // Скорость автоповтора
    unsigned long currentTime = millis();
    static unsigned long lastScrollTime = 0;
    static unsigned long scrollDelay = 0;

    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

        bool hasModifierKey = status.ctrl || status.opt || status.alt || status.fn;
        bool hasCharInput = false;

        for (auto i : status.word) {
            if (i != '\0') {
                hasCharInput = true;
                break;
            }
        }

        if (currentTime - lastScrollTime >= scrollDelay) {
            if (!hasCharInput && hasModifierKey) { 
                scrollDelay = 0;
            }

            for (auto i : status.word) {
                if (status.fn && (i == ',' || i == '/')) {
                    continue;
                }
                data = data.substring(0, cursorPos) + i + data.substring(cursorPos);
                cursorPos++;
            }

            if (status.fn) {
                for (auto k : status.word) {
                    if (k == ',' && cursorPos > 0) {
                        cursorPos--;
                    } else if (k == '/' && cursorPos < data.length()) {
                        cursorPos++;
                    }
                }
            }

            if (status.opt) {
                if (M5Cardputer.BtnA.wasPressed()) {
                    appsMenu = false;
                    isTerminal = false;
                }
                for (auto k : status.word) {
                  if (k == 'q') {
                    appsMenu = false;
                    isTerminal = false;
                  }
                }
            }

            if (status.del) {
                data.remove(data.length() - 1);
                cursorPos--;
            }

            if (status.enter) {
                String result = data;
                data = "";
                return result;
            }

            lastScrollTime = currentTime;
            scrollDelay = (scrollDelay == 0) ? SCROLL_START_DELAY : SCROLL_REPEAT_DELAY;
        }
    } else {
        scrollDelay = 0;
    }

    return data;
}



/*void handleTerminalInput() {
    bool cursorVisible = true;
    unsigned long lastCursorBlink = 0;
    const unsigned long cursorBlinkInterval = 700; // миллисекунд
    const unsigned long SCROLL_START_DELAY = 300;  // Задержка перед началом автоповтора
    const unsigned long SCROLL_REPEAT_DELAY = 50;  // Скорость автоповтора
    int cursorPos = 0;
    while (appsMenu) {
        M5Cardputer.update();
        unsigned long currentTime = millis();
        static unsigned long lastScrollTime = 0;
        static unsigned long scrollDelay = 0;

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

                for (auto i : status.word) {
                    if (status.fn && (i == ',' || i == '/')) {
                        continue;
                    }
                    cmd = cmd.substring(0, cursorPos) + i + cmd.substring(cursorPos);
                    cursorPos++;
                }

                if (status.fn) {
                    for (auto k : status.word) {
                        if (k == ',' && cursorPos > 0) {
                            cursorPos--;
                        } else if (k == '/' && cursorPos < cmd.length()) {
                            cursorPos++;
                        }
                    }
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
                    cursorPos--;
                }

                if (status.enter && isTerminal) {
                    M5Cardputer.Display.fillRect(0, M5Cardputer.Display.height() - 18, M5Cardputer.Display.width(), 18, BLACK);
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
                String beforeCursor = cmd.substring(0, cursorPos);
                String afterCursor = cmd.substring(cursorPos);
                M5Cardputer.Display.drawString(beforeCursor + afterCursor, 5 + promptWidth, M5Cardputer.Display.height() - 14);
                M5Cardputer.Display.fillRect(promptWidth, M5Cardputer.Display.height() - 18, M5Cardputer.Display.width(), 25, BLACK);
                M5Cardputer.Display.drawString(cmd, 5 + promptWidth, M5Cardputer.Display.height() - 14);
                
                lastScrollTime = currentTime;
                scrollDelay = (scrollDelay == 0) ? SCROLL_START_DELAY : SCROLL_REPEAT_DELAY;
            }
        } else {
            scrollDelay = 0; // Сброс задержки, если клавиша отпущена
        }

        // Рисуем мигающий курсор
        unsigned long now = millis();
        if (now - lastCursorBlink >= cursorBlinkInterval) {
            cursorVisible = !cursorVisible;
            lastCursorBlink = now;

            int promptWidth = M5Cardputer.Display.textWidth(dirPath + usr);
            int cmdWidth = M5Cardputer.Display.textWidth(cmd.substring(0, cursorPos));

            int cursorX = 5 + promptWidth + cmdWidth;
            int cursorY = M5Cardputer.Display.height() - 16;

            // Стираем старый курсор
            M5Cardputer.Display.fillRect(cursorX, cursorY, 8, 12, BLACK);

            if (cursorVisible) {
                // Нарисовать квадрат (курсор)
                M5Cardputer.Display.fillRect(cursorX, cursorY, 8, 12, TFT_ORANGE);
            }
        }
        delay(50);
    }
}*/