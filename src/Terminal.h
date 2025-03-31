#include <M5Cardputer.h> 
#include <M5GFX.h>

#include <string>

#include <SD.h>
#include <FS.h>
#include <SPI.h>

std::vector<String> terminalHistory;

String cmd = "";
String dirPath = "/";
String postfix = "$: ";

bool isTerminal = false;
bool appsMenu = false;

int sck = 40;
int miso = 39;
int mosi = 14;
int cs = 12;

void initializeSD() {
    SPI.begin(sck, miso, mosi, cs);
    if (!SD.begin(cs)) {
        Serial.println("Card Mount Failed");
        return;
    }
}

void printToTerminal(const String& text, const int &color) {
    const int maxLines = 7;
    terminalHistory.push_back(text);

    // Ограничиваем количество строк
    if (terminalHistory.size() > maxLines) {
        terminalHistory.erase(terminalHistory.begin());
    }

    M5Cardputer.Display.fillRect(4, 30, M5Cardputer.Display.width() - 8, 85, BLACK);
    
    // Выводим строки
    int y = 30;
    for (const auto& line : terminalHistory) {
        M5Cardputer.Display.setColor(color);
        M5Cardputer.Display.drawString(line, 8, y);
        y += 12;  // Высота строки
    }
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
    Serial.printf("Listing directory: %s\n", dirname);
  
    File root = fs.open(dirname);
    if (!root) {
      Serial.println("Failed to open directory");
      return;
    }
    if (!root.isDirectory()) {
      Serial.println("Not a directory");
      return;
    }
  
    File file = root.openNextFile();
    while (file) {
      if (file.isDirectory()) {
        Serial.print("  DIR : ");
        Serial.println(file.name());
        if (levels) {
          listDir(fs, file.path(), levels - 1);
        }
      } else {
        Serial.print("  FILE: ");
        Serial.print(file.name());
        Serial.print("  SIZE: ");
        Serial.println(file.size());
      }
      file = root.openNextFile();
    }
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
    } else if(command == "clear") {
        terminalHistory.clear();
        M5Cardputer.Display.fillRect(4, 30, M5Cardputer.Display.width() - 8, 85, BLACK);
    } else if (command == "cd") {
        if (arg.length() > 0) {
            dirPath = arg;
            Serial.print("Go to directory: " + arg);
            printToTerminal("Go to directory: " + arg, ORANGE);
    
            M5Cardputer.Display.fillRect(5, 122, M5Cardputer.Display.width(), 12, BLACK);
            M5Cardputer.Display.drawString(dirPath + postfix, 5, 122);
        } else {
            Serial.println("Error: path not specifed.");
            printToTerminal("Error: path not specifed.", ORANGE);
        }
    } else if(command == "ls") {
        if (arg.length() > 0) {
            
        } else {
            if (!SD.cardType() == CARD_NONE) {
                printToTerminal("SD", ORANGE);
            }
            printToTerminal("LittleFS", ORANGE);
        }
    } else {
        Serial.println("Unkown command.");
        printToTerminal("Unkown command.", ORANGE);
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

                if (status.del) {
                    cmd.remove(cmd.length() - 1);
                }

                if (status.enter && isTerminal) {
                    M5Cardputer.Display.drawString(dirPath + postfix, 5, 122);
                    printToTerminal(dirPath + postfix + cmd, ORANGE);
                    handleCommands(cmd);
                    cmd.remove(0, 2);
                    cmd = "";
                }

                if (!isTerminal) {
                    isTerminal = true;
                }

                M5Cardputer.Display.fillRect(23, M5Cardputer.Display.height() - 18, M5Cardputer.Display.width(), 25, BLACK);
                M5Cardputer.Display.drawString(cmd, 23, M5Cardputer.Display.height() - 14);
                
                lastScrollTime = currentTime;
                scrollDelay = (scrollDelay == 0) ? SCROLL_START_DELAY : SCROLL_REPEAT_DELAY;
            }
        } else {
            scrollDelay = 0; // Сброс задержки, если клавиша отпущена
        }
    }
}