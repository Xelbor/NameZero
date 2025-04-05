#include <SD.h>

bool pathExists(const String& dirPath) {
    File file = SD.open(dirPath.c_str());
    if (!file) {
      return false; // Не удалось открыть файл или директорию
    }
    file.close();
    return true; // Файл или директория существует
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