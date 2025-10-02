#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <AiEsp32RotaryEncoder.h>
#include <TEA5767Radio.h>
#include <SPI.h>
#include <DHT.h>
#include <RtcDS1302.h>
#include <EEPROM.h>

// === ESP32 OZELLIKLERI ICIN GEREKLI KUTUPHANE ===
#include "esp32/rom/spi_flash.h"
#include "esp_heap_caps.h"

// === PIN TANIMLARI ===
#define TFT_RST 16
#define TFT_DC  17
#define TFT_CS  5
#define DHT_PIN 4
#define DHT_TYPE DHT11
#define ROTARY_CLK 32
#define ROTARY_DT  33
#define ROTARY_SW  25

// === NESNELER ===
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
DHT dht(DHT_PIN, DHT_TYPE);
ThreeWire myWire(27, 26, 14);
RtcDS1302<ThreeWire> Rtc(myWire);
AiEsp32RotaryEncoder rotaryEncoder(ROTARY_DT, ROTARY_CLK, ROTARY_SW, -1, 1);
TEA5767Radio radio;

// === HAFIZA ===
#define EEPROM_SIZE 512
#define MEMORY_CHANNELS 10
int memoryChannels[MEMORY_CHANNELS];
int usedMemorySlots = 0;
int selectedMemoryIndex = 0;

// === RADYO ===
int freqX10 = 934;
int minFreq = 875;
int maxFreq = 1080;
int tempFreqToSave = 0;

// === HAVA DURUMU ===
float temperature = 0;
float humidity = 0;

// === EKRAN DURUMU ===
enum Screen {
  SCREEN_MAIN = 0,
  SCREEN_MEMORY = 1,
  SCREEN_CONFIRM_SAVE = 2,
  SCREEN_CONFIRM_EXIT = 3,
  SCREEN_CONFIRM_CLEAR = 4
};
Screen currentScreen = SCREEN_MAIN;

// === BUTON KONTROLU ===
bool buttonPressed = false;
unsigned long buttonPressTime = 0;
unsigned long lastClickTime = 0;
#define DEBOUNCE_MS 300

// === ONAY EKRANI ===
bool confirmYes = true;

// === TIMER'LAR ===
unsigned long lastDHTRead = 0;
unsigned long lastScreenUpdate = 0;
unsigned long startTime = 0; // Cihazın açılış zamanı

// === ANIMASYON DURUMU ===
unsigned long lastDotChange = 0;
int dotCount = 1;

// === SPEKTRUM ICIN SON DEGERLERI TUT ===
#define SPECTRUM_BARS 40
int spectrumHeights[SPECTRUM_BARS];
unsigned long lastSpectrumUpdate = 0;

// === ENCODER ISR ===
void IRAM_ATTR readEncoderISR() {
  rotaryEncoder.readEncoder_ISR();
}

// === HAFIZA FONKSIYONLARI ===

void loadMemory() {
  EEPROM.get(0, usedMemorySlots);
  if (usedMemorySlots < 0 || usedMemorySlots > MEMORY_CHANNELS) usedMemorySlots = 0;
  
  for (int i = 0; i < MEMORY_CHANNELS; i++) {
    EEPROM.get(sizeof(int) + i * sizeof(int), memoryChannels[i]);
    if (memoryChannels[i] < minFreq || memoryChannels[i] > maxFreq) memoryChannels[i] = 0;
  }
  
  usedMemorySlots = 0;
  for (int i = 0; i < MEMORY_CHANNELS; i++) {
    if (memoryChannels[i] != 0) usedMemorySlots++;
  }
}

void saveMemory() {
  EEPROM.put(0, usedMemorySlots);
  for (int i = 0; i < MEMORY_CHANNELS; i++) {
    EEPROM.put(sizeof(int) + i * sizeof(int), memoryChannels[i]);
  }
  EEPROM.commit();
}

void saveFrequencyToSlot(int slot, int freq) {
  memoryChannels[slot] = freq;
  usedMemorySlots = 0;
  for (int i = 0; i < MEMORY_CHANNELS; i++) {
    if (memoryChannels[i] != 0) usedMemorySlots++;
  }
  saveMemory();
  
  // Basari mesaji
  tft.fillScreen(ST7735_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_GREEN);
  tft.setCursor(25, 40);
  tft.print("BASARILI");
  
  tft.setTextSize(1);
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(20, 70);
  char msg[25];
  sprintf(msg, "%.1f MHz", freq / 10.0);
  tft.print(msg);
  
  tft.setCursor(20, 85);
  sprintf(msg, "%d. KANALA KAYDEDILDI", slot + 1);
  tft.print(msg);
  
  delay(2000);
  currentScreen = SCREEN_MEMORY;
  selectedMemoryIndex = 0;
  rotaryEncoder.setBoundaries(0, 11, false);
  rotaryEncoder.setEncoderValue(0);
  drawMemoryScreen();
}

void clearAllMemory() {
  for (int i = 0; i < MEMORY_CHANNELS; i++) {
    memoryChannels[i] = 0;
  }
  usedMemorySlots = 0;
  saveMemory();
  
  tft.fillScreen(ST7735_BLACK);
  tft.setTextSize(2);  
  tft.setTextColor(ST7735_RED);
  tft.setCursor(15, 60);
  tft.print("TUM HAFIZA");
  tft.setCursor(30, 85);
  tft.print("SILINDI");
  
  delay(2000);
  
  currentScreen = SCREEN_MEMORY;
  selectedMemoryIndex = 0;
  rotaryEncoder.setBoundaries(0, 11, false);
  rotaryEncoder.setEncoderValue(0);
  drawMemoryScreen();
}

// === YARDIMCI FONKSIYONLAR ===

void getStationName(int freq, String &line1, String &line2) {
  line1 = ""; line2 = "";
  switch(freq) {
    case 934: line1 = "TRT"; line2 = "TURKU"; break;
    case 918: line1 = "TRT"; line2 = "HABER"; break;
    case 992: line1 = "TRT"; line2 = "RADYO3"; break;
    case 1005: line1 = "ENERJI"; line2 = "FM"; break;
    case 1049: line1 = "TRT"; line2 = "RADYO1"; break;
    case 964: line1 = "TRT"; line2 = "NAGME"; break;
    case 972: line1 = "TRT"; line2 = "FM"; break;
    case 915: line1 = "METEOR"; line2 = "FM"; break;
    case 1024: line1 = "TRT"; line2 = "TURKU"; break;  
    default: line1 = "Kanal"; line2 = "Araniyor"; break;  
  }
}

void readDHT() {
  if (millis() - lastDHTRead < 3000) return;
  lastDHTRead = millis();
  
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  
  if (!isnan(t) && !isnan(h)) {
    temperature = t;
    humidity = h;
  }
}

// YENI: DAHA DINAMIK SPEKTRUM / VU METRE FONKSIYONU
void drawSpectrum(int x, int y) {
  // Spektrum cubuklarinin temizlenecegi alan (120x12)
  tft.fillRect(x, y - 12, 120, 12, ST7735_BLACK);  
  
  for (int i = 0; i < SPECTRUM_BARS; i++) {
    // Mevcut yuksekligi rastgele -2 ile +2 arasinda degistir
    int change = random(-2, 3);
    int new_h = constrain(spectrumHeights[i] + change, 1, 12);
    spectrumHeights[i] = new_h;
    
    // Yukseklige gore renk secimi
    uint16_t c = (new_h > 8) ? ST7735_RED : (new_h > 5) ? ST7735_YELLOW : ST7735_GREEN;
    
    // Cubugu ciz
    tft.fillRect(x + i * 3, y - new_h, 2, new_h, c);
  }
}

// === ANA EKRAN (STEREO GOSTERGESI TAMAMEN KALDIRILDI) ===
void drawMainScreen() {
  // SPEKTRUM haric herseyi yeniden ciziyoruz.
  tft.fillScreen(ST7735_BLACK);  
  
  RtcDateTime now = Rtc.GetDateTime();
  
  // UST: SAAT (SOL) - TARIH (SAG)
  tft.setTextSize(1);
  tft.setTextColor(ST7735_CYAN);
  
  char timeStr[10];
  sprintf(timeStr, "%02d:%02d:%02d", now.Hour(), now.Minute(), now.Second());
  tft.setCursor(5, 5);
  tft.print(timeStr);
  
  char dateStr[12];
  sprintf(dateStr, "%02d/%02d/%04d", now.Day(), now.Month(), now.Year());
  int dateWidth = strlen(dateStr) * 6;
  tft.setCursor(128 - dateWidth - 5, 5);
  tft.print(dateStr);
  
  // ORTA: FREKANS
  char freqStr[12];
  sprintf(freqStr, "%.1f MHz", freqX10 / 10.0);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_CYAN);
  int fw = strlen(freqStr) * 12;
  // Frekansı ortalamak için
  int freqX = (128 - fw) / 2;
  tft.setCursor(freqX, 25);
  tft.print(freqStr);
  
  // STEREO GOSTERGESI KODU BURADAN KALDIRILDI.

  tft.drawLine(10, 45, 118, 45, ST7735_CYAN);
  
  // ISTASYON ADI VE ANIMASYON
  String line1, line2;
  getStationName(freqX10, line1, line2);
  
  // Eger "Kanal Araniyor..." ise
  if (line1 == "Kanal") {
      tft.setTextSize(1);
      tft.setTextColor(ST7735_RED);
      
      // "Kanal" yazisi
      int line1_width = line1.length() * 6;
      tft.setCursor((128 - line1_width) / 2, 55);
      tft.print(line1);
      
      // "Araniyor" yazisi ve ANIMASYON: Noktalari ciz
      tft.setTextColor(ST7735_YELLOW);
      const char* araniyor = "Araniyor";
      int araniyor_width = strlen(araniyor) * 6;
      int dots_width = dotCount * 6; // 1, 2, veya 3 nokta
      int total_width = araniyor_width + dots_width;
      
      int start_x = (128 - total_width) / 2; // Ortalamanin baslangic noktasi
      
      tft.setCursor(start_x, 70);
      tft.print(araniyor);
      
      // Noktalari cizmeyi temizleyip yeniden cizer
      tft.setCursor(start_x + araniyor_width, 70);  
      for (int i = 0; i < dotCount; i++) {
          tft.print(".");
      }
      // Kalan noktalari temizle (Maks. 3 nokta oldugu icin)
      for (int i = dotCount; i < 3; i++) {
          tft.print(" ");
      }

  } else {
      // Normal istasyon adi
      tft.setTextSize(2);
      tft.setTextColor(ST7735_WHITE);
      int line1_width = line1.length() * 12;
      int line2_width = line2.length() * 12;
      
      tft.setCursor((128 - line1_width) / 2, 52);
      tft.print(line1);
      tft.setCursor((128 - line2_width) / 2, 70);
      tft.print(line2);
  }
  
  // SICAKLIK & NEM
  tft.setTextSize(1);
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(20, 92);
  tft.print("SICAKLIK");
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(20, 102);
  if (temperature > 0) {
    tft.print(temperature, 1);
    tft.print("C");
  } else {
    tft.print("--C");
  }
  
  // Termometre ikonu
  int tempBar = map(constrain(temperature, 0, 50), 0, 50, 0, 20);
  tft.drawRect(5, 92, 8, 22, ST7735_WHITE);
  tft.fillRect(6, 93, 6, 20, ST7735_BLACK);
  tft.fillRect(6, 93 + (20 - tempBar), 6, tempBar, ST7735_RED);
  tft.fillCircle(9, 115, 4, ST7735_RED);
  
  tft.setTextColor(ST7735_BLUE);
  tft.setCursor(85, 92);
  tft.print("NEM");
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(85, 102);
  if (humidity > 0) {
    char humStr[8];
    sprintf(humStr, "%%%d", (int)humidity);  
    tft.print(humStr);
  } else {
    tft.print("--%");
  }
  
  // Nem cubugu ikonu
  int humBar = map(constrain(humidity, 0, 100), 0, 100, 0, 20);
  tft.drawRect(115, 92, 8, 22, ST7735_WHITE);
  tft.fillRect(116, 93, 6, 20, ST7735_BLACK);
  tft.fillRect(116, 93 + (20 - humBar), 6, humBar, ST7735_BLUE);
  
  // ALT: SISTEM BILGILERI
  tft.setTextColor(ST7735_MAGENTA);
  tft.setTextSize(1);
  
  uint32_t cpuFreq = getCpuFrequencyMhz();
  char cpuStr[20];
  sprintf(cpuStr, "CPU:%dMHz", cpuFreq);
  tft.setCursor(5, 125);
  tft.print(cpuStr);
  
  uint32_t freeRam = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  char ramStr[20];
  sprintf(ramStr, "RAM:%dkB", freeRam / 1024);
  tft.setCursor(70, 125);
  tft.print(ramStr);
  
  unsigned long uptimeMs = millis() - startTime;
  unsigned long seconds = uptimeMs / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  seconds %= 60;
  minutes %= 60;
  
  char upTimeStr[25];
  if (hours > 0) {
      sprintf(upTimeStr, "%ldSaat %02ldDk %02ldSn", hours, minutes, seconds);
  } else {
      sprintf(upTimeStr, "%02ldDk %02ldSn", minutes, seconds);
  }

  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5, 137);
  tft.print("Acik: ");
  tft.setTextColor(ST7735_MAGENTA);
  tft.print(upTimeStr);
  
  // SPEKTRUM
  drawSpectrum(5, 158);
}


// SADECE EKRANIN UST KISMINI GUNCELLEYEN HIZLI FONKSIYON (STEREO KALDIRILDI)
void updateMainScreenTop() {
  RtcDateTime now = Rtc.GetDateTime();
  
  // 1. SAAT VE TARIH BOLGESI
  tft.setTextSize(1);
  tft.setTextColor(ST7735_CYAN, ST7735_BLACK);
  char timeStr[10];
  sprintf(timeStr, "%02d:%02d:%02d", now.Hour(), now.Minute(), now.Second());
  tft.setCursor(5, 5);
  tft.print(timeStr);
  
  // 2. STEREO GOSTERGESI GUNCELLEME KISMI KALDIRILDI.
  
  // 3. ANIMASYON BOLGESI (Eger Araniyor ise)
  String line1, line2;
  getStationName(freqX10, line1, line2);
  
  if (line1 == "Kanal") {
      tft.setTextSize(1);
      tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
      const char* araniyor = "Araniyor";
      int araniyor_width = strlen(araniyor) * 6;
      int total_width = araniyor_width + (3 * 6); // Maksimum 3 nokta alani
      int start_x = (128 - total_width) / 2; 
      
      tft.setCursor(start_x + araniyor_width, 70);  
      for (int i = 0; i < dotCount; i++) {
          tft.print(".");
      }
      for (int i = dotCount; i < 3; i++) {
          tft.print(" ");
      }
  }
  
  // 4. UPTIME GUNCELLEMESI (137. satir)
  unsigned long uptimeMs = millis() - startTime;
  unsigned long seconds = uptimeMs / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  seconds %= 60;
  minutes %= 60;
  
  char upTimeStr[25];
  if (hours > 0) {
      sprintf(upTimeStr, "%ldSaat %02ldDk %02ldSn", hours, minutes, seconds);
  } else {
      sprintf(upTimeStr, "%02ldDk %02ldSn", minutes, seconds);
  }
  
  tft.fillRect(40, 137, 90, 8, ST7735_BLACK); // Sadece magenta yaziyi temizle
  tft.setTextColor(ST7735_MAGENTA);
  tft.setCursor(5 + 5 * 6, 137); // "Acik: " yazisinin bitiminden basla
  tft.print(upTimeStr);
}


// === HAFIZA EKRANI ===
void drawMemoryScreen() {
  tft.fillScreen(ST7735_BLACK);
  
  tft.setTextSize(1);
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(18, 5);
  tft.print("HAFIZA KANALLARI");
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(5, 20);
  char info[25];
  sprintf(info, "Dolu:%d/10 Frek:%.1f", usedMemorySlots, freqX10 / 10.0);
  tft.print(info);
  
  // 10 KANAL - 2 SUTUN (5'er kanal)
  for (int i = 0; i < MEMORY_CHANNELS; i++) {
    int col = i / 5;
    int row = i % 5;
    int x = col * 64 + 5;
    int y = 38 + row * 11;
    
    if (i == selectedMemoryIndex) {
      tft.setTextColor(ST7735_BLACK, ST7735_GREEN);
    } else if (memoryChannels[i] != 0) {
      tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
    } else {
      tft.setTextColor(ST7735_CYAN, ST7735_BLACK);
    }
    
    tft.setCursor(x, y);
    if (memoryChannels[i] != 0) {
      char slot[15];
      sprintf(slot, "%d:%.1f", i + 1, memoryChannels[i] / 10.0);
      tft.print(slot);
    } else {
      char slot[12];
      sprintf(slot, "%d:<bos>", i + 1);
      tft.print(slot);
    }
  }
  
  // 11. SECENEK: ANA EKRANA DON
  const char* exitText = ">> ANA EKRANA DON";
  int exitWidth = strlen(exitText) * 6;
  int exitX = (128 - exitWidth) / 2;
  
  if (selectedMemoryIndex == 10) {
    tft.fillRect(5, 95, 118, 14, ST7735_BLUE);
    tft.setTextColor(ST7735_BLACK);
  } else {
    tft.setTextColor(ST7735_BLUE);
  }
  tft.setCursor(exitX, 98);
  tft.print(exitText);
  
  // 12. SECENEK: TUM HAFIZIYI SIL
  const char* clearText = ">> TUM HAFIZIYI SIL";
  int clearWidth = strlen(clearText) * 6;
  int clearX = (128 - clearWidth) / 2;
  
  if (selectedMemoryIndex == 11) {
    tft.fillRect(5, 112, 118, 14, ST7735_RED);
    tft.setTextColor(ST7735_BLACK);
  } else {
    tft.setTextColor(ST7735_RED);
  }
  tft.setCursor(clearX, 115);
  tft.print(clearText);
  
  // TALIMATLAR
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(5, 135);
  tft.print("Encoder: Secim yap");
  
  tft.setCursor(5, 148);
  tft.setTextColor(ST7735_GREEN);
  tft.print("Tikla: Islemi onayla");
}

// === ONAY EKRANLARI ===
void drawConfirmSave() {
  tft.fillScreen(ST7735_BLACK);
  
  tft.setTextSize(1);
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(10, 15);
  tft.print("KANALA KAYDET?");
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(15, 35);
  char msg[25];
  sprintf(msg, "Frekans: %.1f MHz", tempFreqToSave / 10.0);
  tft.print(msg);
  
  tft.setCursor(15, 50);
  sprintf(msg, "Hedef: Kanal %d", selectedMemoryIndex + 1);
  tft.print(msg);
  
  // Eger kanal doluysa uyari
  if (memoryChannels[selectedMemoryIndex] != 0) {
    tft.setTextColor(ST7735_RED);
    tft.setCursor(15, 65);
    tft.print("! Kanal dolu !");
    tft.setCursor(15, 77);
    sprintf(msg, "(%.1f MHz)", memoryChannels[selectedMemoryIndex] / 10.0);
    tft.print(msg);
  }
  
  // EVET/HAYIR
  int yStart = (memoryChannels[selectedMemoryIndex] != 0) ? 95 : 75;
  
  if (confirmYes) {
    tft.fillRect(20, yStart, 88, 16, ST7735_GREEN);
    tft.setTextColor(ST7735_BLACK);
  } else {
    tft.setTextColor(ST7735_WHITE);
  }
  tft.setCursor(45, yStart + 4);
  tft.print("EVET");
  
  if (!confirmYes) {
    tft.fillRect(20, yStart + 23, 88, 16, ST7735_RED);
    tft.setTextColor(ST7735_BLACK);
  } else {
    tft.setTextColor(ST7735_WHITE);
  }
  tft.setCursor(42, yStart + 27);
  tft.print("HAYIR");
  
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(10, 145);
  tft.print("Cevir: Sec");
  tft.setCursor(10, 157);
  tft.print("Tikla: Onayla");
}

void drawConfirmExit() {
  tft.fillScreen(ST7735_BLACK);
  
  tft.setTextSize(1);
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(15, 30);
  tft.print("ANA EKRANA");
  tft.setCursor(15, 45);
  tft.print("DONMEK ISTIYOR");
  tft.setCursor(15, 60);
  tft.print("MUSUNUZ?");
  
  // EVET/HAYIR
  if (confirmYes) {
    tft.fillRect(20, 85, 88, 16, ST7735_GREEN);
    tft.setTextColor(ST7735_BLACK);
  } else {
    tft.setTextColor(ST7735_WHITE);
  }
  tft.setCursor(45, 89);
  tft.print("EVET");
  
  if (!confirmYes) {
    tft.fillRect(20, 108, 88, 16, ST7735_RED);
    tft.setTextColor(ST7735_BLACK);
  } else {
    tft.setTextColor(ST7735_WHITE);
  }
  tft.setCursor(42, 112);
  tft.print("HAYIR");
  
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(10, 145);
  tft.print("Cevir: Sec");
  tft.setCursor(10, 157);
  tft.print("Tikla: Onayla");
}

void drawConfirmClear() {
  tft.fillScreen(ST7735_BLACK);
  
  tft.setTextSize(1);
  tft.setTextColor(ST7735_RED);
  tft.setCursor(15, 20);
  tft.print("!! DIKKAT !!");
  
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(10, 40);
  tft.print("TUM HAFIZA");
  tft.setCursor(10, 55);
  tft.print("SILINECEK!");
  
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(10, 75);
  char msg[25];
  sprintf(msg, "(%d kanal silinecek)", usedMemorySlots);
  tft.print(msg);
  
  // EVET/HAYIR
  if (confirmYes) {
    tft.fillRect(20, 100, 88, 16, ST7735_GREEN);
    tft.setTextColor(ST7735_BLACK);
  } else {
    tft.setTextColor(ST7735_WHITE);
  }
  tft.setCursor(45, 104);
  tft.print("EVET");
  
  if (!confirmYes) {
    tft.fillRect(20, 123, 88, 16, ST7735_RED);
    tft.setTextColor(ST7735_BLACK);
  } else {
    tft.setTextColor(ST7735_WHITE);
  }
  tft.setCursor(42, 127);
  tft.print("HAYIR");
  
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(10, 152);
  tft.print("Cevir: Sec | Tikla: Onayla");
}

// === ACILIS EKRANI ===
void drawSplashScreen() {
  tft.fillScreen(ST7735_BLACK);
  
  // Baslik
  tft.setTextSize(3);
  tft.setTextColor(ST7735_CYAN);
  tft.setCursor(15, 30);
  tft.print("TA2CAY");
  
  // FM RADYO PROJE BUYUK HARFLE VE ORTALANMIS
  const char* projectText = "FM RADYO PROJE";
  tft.setTextSize(1);
  tft.setTextColor(ST7735_YELLOW);
  int textWidth = strlen(projectText) * 6;
  tft.setCursor((128 - textWidth) / 2, 55);
  tft.print(projectText);
  
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(30, 80);
  tft.print("BASLATILIYOR...");
  
  // Yukleniyor Bari
  int barX = 15;
  int barY = 120;
  int barW = 98;
  int barH = 15;
  
  tft.drawRect(barX, barY, barW, barH, ST7735_WHITE);
  
  // 5 saniye bekleme icin 10 adim * 500ms
  for (int i = 0; i <= 100; i += 10) {
    int barFill = map(i, 0, 100, 0, barW - 2);
    tft.fillRect(barX + 1, barY + 1, barFill, barH - 2, ST7735_GREEN);
    
    // Yuzdelik Temizle ve Yaz
    tft.fillRect(50, 140, 40, 15, ST7735_BLACK);  
    tft.setTextSize(2);
    tft.setTextColor(ST7735_WHITE);
    char percent[5];
    sprintf(percent, "%d%%", i);
    int pW = strlen(percent) * 12;
    tft.setCursor((128 - pW) / 2, 140);
    tft.print(percent);
    
    delay(500);  
  }
  
  delay(100);  
}

// === SETUP ===
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== TA2CAY BASLIYOR ===");
  
  startTime = millis();  
  
  EEPROM.begin(EEPROM_SIZE);
  loadMemory();
  
  dht.begin();
  Rtc.Begin();
  Wire.begin(21, 22);
  
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  
  // Acilis Ekrani
  drawSplashScreen();
  
  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  rotaryEncoder.setBoundaries(minFreq, maxFreq, false);
  rotaryEncoder.setEncoderValue(freqX10);
  rotaryEncoder.setAcceleration(0);
  
  pinMode(ROTARY_SW, INPUT_PULLUP);
  
  radio.setFrequency(freqX10 / 10.0);
  
  readDHT();
  
  // Spektrum baslangic degerlerini ayarla
  for (int i = 0; i < SPECTRUM_BARS; i++) {
    spectrumHeights[i] = 1;
  }
  
  Serial.println("=== SISTEM HAZIR ===\n");
  drawMainScreen();
}

// === LOOP ===
void loop() {
  readDHT();
  
  // === BUTON OKUMA ===
  bool btnNow = (digitalRead(ROTARY_SW) == LOW);
  
  if (btnNow && !buttonPressed) {
    buttonPressed = true;
    buttonPressTime = millis();
  }
  
  if (!btnNow && buttonPressed) {
    buttonPressed = false;
    
    if (millis() - lastClickTime > DEBOUNCE_MS) {
      lastClickTime = millis();
      
      if (currentScreen == SCREEN_MAIN) {
        currentScreen = SCREEN_MEMORY;
        selectedMemoryIndex = 0;
        rotaryEncoder.setBoundaries(0, 11, false);
        rotaryEncoder.setEncoderValue(0);
        drawMemoryScreen();
        
      } else if (currentScreen == SCREEN_MEMORY) {
        if (selectedMemoryIndex == 10) {
          confirmYes = true;
          currentScreen = SCREEN_CONFIRM_EXIT;
          rotaryEncoder.setBoundaries(0, 1, false);
          rotaryEncoder.setEncoderValue(0);
          drawConfirmExit();
          
        } else if (selectedMemoryIndex == 11) {
          confirmYes = false;
          currentScreen = SCREEN_CONFIRM_CLEAR;
          rotaryEncoder.setBoundaries(0, 1, false);
          rotaryEncoder.setEncoderValue(1);
          drawConfirmClear();
          
        } else {
          tempFreqToSave = freqX10;
          confirmYes = true;
          currentScreen = SCREEN_CONFIRM_SAVE;
          rotaryEncoder.setBoundaries(0, 1, false);
          rotaryEncoder.setEncoderValue(0);
          drawConfirmSave();
        }
        
      } else if (currentScreen == SCREEN_CONFIRM_SAVE) {
        if (confirmYes) {
          saveFrequencyToSlot(selectedMemoryIndex, tempFreqToSave);
        } else {
          currentScreen = SCREEN_MEMORY;
          rotaryEncoder.setBoundaries(0, 11, false);
          rotaryEncoder.setEncoderValue(selectedMemoryIndex);
          drawMemoryScreen();
        }
        
      } else if (currentScreen == SCREEN_CONFIRM_EXIT) {
        if (confirmYes) {
          currentScreen = SCREEN_MAIN;
          rotaryEncoder.setBoundaries(minFreq, maxFreq, false);
          rotaryEncoder.setEncoderValue(freqX10);
          drawMainScreen();
        } else {
          currentScreen = SCREEN_MEMORY;
          selectedMemoryIndex = 10;
          rotaryEncoder.setBoundaries(0, 11, false);
          rotaryEncoder.setEncoderValue(10);
          drawMemoryScreen();
        }
        
      } else if (currentScreen == SCREEN_CONFIRM_CLEAR) {
        if (confirmYes) {
          clearAllMemory();
        } else {
          currentScreen = SCREEN_MEMORY;
          selectedMemoryIndex = 11;
          rotaryEncoder.setBoundaries(0, 11, false);
          rotaryEncoder.setEncoderValue(11);
          drawMemoryScreen();
        }
      }
    }
  }
  
  // === ENCODER DEGISIKLIGI ===
  if (rotaryEncoder.encoderChanged()) {
    int newValue = (int)rotaryEncoder.readEncoder();
    
    if (currentScreen == SCREEN_MAIN) {
      freqX10 = newValue;
      radio.setFrequency(freqX10 / 10.0);
      drawMainScreen(); // Frekans degisince tum ekrani yeniden ciz
      
    } else if (currentScreen == SCREEN_MEMORY) {
      selectedMemoryIndex = newValue;
      drawMemoryScreen();
      
    } else if (currentScreen == SCREEN_CONFIRM_SAVE ||  
                currentScreen == SCREEN_CONFIRM_EXIT ||  
                currentScreen == SCREEN_CONFIRM_CLEAR) {
      confirmYes = (newValue == 0);
      
      if (currentScreen == SCREEN_CONFIRM_SAVE) drawConfirmSave();
      else if (currentScreen == SCREEN_CONFIRM_EXIT) drawConfirmExit();
      else if (currentScreen == SCREEN_CONFIRM_CLEAR) drawConfirmClear();
    }
  }
  
  // === EKRAN GUNCELLEME ve ANIMASYON MANTIGI ===
  if (currentScreen == SCREEN_MAIN) {
      bool should_update = false;

      // 1. Kanal Araniyor Animasyonu Mantigi (500ms)
      String line1, line2;
      getStationName(freqX10, line1, line2);

      if (line1 == "Kanal" && millis() - lastDotChange > 500) {
          dotCount = (dotCount % 3) + 1; // 1, 2, 3 arasinda don
          lastDotChange = millis();
          should_update = true;
      }
      
      // 2. Normal Saat/Uptime Guncellemesi Mantigi (1000ms)
      if (millis() - lastScreenUpdate > 1000) {
          lastScreenUpdate = millis();
          should_update = true;
      }
      
      // Sadece zaman/animasyon guncellenmesi gerektiginde hizli guncelleme
      if (should_update) {
          updateMainScreenTop();
      }

      // SPEKTRUM GUNCELLEMESI (Daha sik, digerlerinden bagimsiz)
      if (millis() - lastSpectrumUpdate > 150) { // 150ms'de bir guncelle
          lastSpectrumUpdate = millis();
          drawSpectrum(5, 158);
      }
  }
}