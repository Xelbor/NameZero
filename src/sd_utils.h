#include <SPI.h>
#include <SD.h>
#include <FS.h>

#include <vector>

int sck = 40;
int miso = 39;
int mosi = 14;
int cs = 12;

struct DirEntry {
  String path;
  bool isDirectory;
};

void initializeSD() {
  SPI.begin(sck, miso, mosi, cs);
  if (!SD.begin(cs)) {
      Serial.println("Card Mount Failed");
      return;
  }

  uint8_t cardType = SD.cardType();
  
  if(cardType == CARD_NONE){
      Serial.println("No SD card attached");
      return;
  }
}

bool pathExists(const String& dirPath) {
    File file = SD.open(dirPath.c_str());
    if (!file) {
      return false; // Не удалось открыть файл или директорию
    }
    file.close();
    return true; // Файл или директория существует
}  

std::vector<DirEntry> listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  std::vector<DirEntry> entries;
  
  File root = fs.open(dirname);
  if (!root || !root.isDirectory()) {
    Serial.println("Failed to open directory or not a directory");
    return entries;
  }

  File file = root.openNextFile();
  while (file) {
    DirEntry entry;
    entry.path = file.path();
    entry.isDirectory = file.isDirectory();
    entries.push_back(entry);

    if (file.isDirectory() && levels > 0) {
      // Рекурсивно добавим содержимое поддиректории
      std::vector<DirEntry> subEntries = listDir(fs, file.path(), levels - 1);
      entries.insert(entries.end(), subEntries.begin(), subEntries.end());
    }

    file = root.openNextFile();
  }

  return entries;
}

bool alphaSort(const DirEntry& a, const DirEntry& b) {
  String nameA = a.path;
  String nameB = b.path;

  if (nameA.startsWith("/")) nameA = nameA.substring(1);
  if (nameB.startsWith("/")) nameB = nameB.substring(1);

  int len = std::min(nameA.length(), nameB.length());

  for (int i = 0; i < len; i++) {
      char ca = nameA[i];
      char cb = nameB[i];

      char uca = toupper(ca);
      char ucb = toupper(cb);

      if (uca != ucb) {
          return uca < ucb;
      }

      if (ca != cb) {
          return ca < cb;
      }
  }

  return nameA.length() < nameB.length();
}

bool dirAndAlphaSort(const DirEntry& a, const DirEntry& b) {
  if (a.isDirectory != b.isDirectory) {
      return a.isDirectory > b.isDirectory;
  }
  return alphaSort(a, b);
}