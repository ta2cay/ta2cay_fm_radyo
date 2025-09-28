#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <AiEsp32RotaryEncoder.h>
#include <TEA5767Radio.h>
#include <SPI.h>
#include <DHT.h>
#include <RtcDS1302.h>
#include <EEPROM.h>

// --- TFT SPI Bağlantıları ---
#define TFT_RST 16
#define TFT_DC  17
#define TFT_CS  5
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// --- DHT11 Bağlantısı ---
#define DHT_PIN 4
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// --- DS1302 RTC Bağlantıları ---
ThreeWire myWire(27, 26, 14); // DAT, CLK, RST
RtcDS1302<ThreeWire> Rtc(myWire);

// --- Rotary Encoder ---
#define ROTARY_CLK 32
#define ROTARY_DT  33
#define ROTARY_SW  25
#define ROTARY_VCC -1
#define ROTARY_STEPS 1
AiEsp32RotaryEncoder rotaryEncoder(ROTARY_DT, ROTARY_CLK, ROTARY_SW, ROTARY_VCC, ROTARY_STEPS);

// --- TEA5767 Radyo ---
TEA5767Radio radio;

// === HAFIZA VE FAVORİLER ===
#define EEPROM_SIZE 512
#define MEMORY_CHANNELS 8  // 8 kanal hafıza
int memoryChannels[MEMORY_CHANNELS]; // Hafıza kanalları
int currentMemorySlot = 0; // Bir sonraki kaydetme yeri (kullanılmıyor ama bıraktım)
int usedMemorySlots = 0;   // Kullanılan slot sayısı
int selectedMemorySlot = 0; // Hafıza ekranında seçili slot

// === DEĞİŞKENLER ===
int freqX10 = 934;   // 93.4 MHz açılış frekansı
int minFreq = 875;   // 87.5 MHz
int maxFreq = 1080;  // 108.0 MHz
int lastFreqX10 = 0; // Değişiklik kontrolü için

float temperature = 0;
float humidity = 0;
float lastTemp = 0;
float maxTemp = -999, minTemp = 999;
float maxHum = -999, minHum = 999;

unsigned long lastDHTRead = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastMinMaxReset = 0;
unsigned long lastButtonPress = 0;

bool showingDetails = false;
bool showingMemory = false;  // Hafıza ekranı durumu
bool isNightMode = false;
bool dataChanged = true; // Ekran güncelleme kontrolü

// Ekran durumları enum
enum ScreenState {
  SCREEN_MAIN = 0,
  SCREEN_DETAILS = 1, 
  SCREEN_MEMORY = 2
};
ScreenState currentScreen = SCREEN_MAIN;

// === RENK VE ANİMASYON ===
unsigned long lastColorChange = 0;
int currentColorIndex = 0;
uint16_t rainbowColors[] = { 
  ST7735_RED, ST7735_MAGENTA, ST7735_BLUE, 
  ST7735_CYAN, ST7735_GREEN, ST7735_YELLOW 
};

// === Enkoder ISR ===
void IRAM_ATTR readEncoderISR() {
  rotaryEncoder.readEncoder_ISR();
}

// === YARDIMCI FONKSİYONLAR ===

// Gece modu kontrolü (22:00 - 07:00 arası)
bool isNightTime() {
  RtcDateTime now = Rtc.GetDateTime();
  int hour = now.Hour();
  return (hour >= 22 || hour < 7);
}

// Gökkuşağı renk animasyonu
uint16_t getRainbowColor() {
  if (millis() - lastColorChange > 800) {
    lastColorChange = millis();
    currentColorIndex = (currentColorIndex + 1) % 6;
  }
  return rainbowColors[currentColorIndex];
}

// Sinyal gücü simülasyonu
int getSignalStrength() {
  // Gerçek TEA5767'de radio.getSignalLevel() kullanılabilir
  return random(2, 6); // 2-5 arası
}

// Sıcaklık trendi (basit)
String getTempTrend() {
  if (temperature > lastTemp + 0.3) return "+";
  if (temperature < lastTemp - 0.3) return "-";
  return "=";
}

// Min/Max güncelleme
void updateMinMax() {
  if (temperature > maxTemp) maxTemp = temperature;
  if (temperature < minTemp) minTemp = temperature;
  if (humidity > maxHum) maxHum = humidity;
  if (humidity < minHum) minHum = humidity;
  
  // Her gün resetle
  if (millis() - lastMinMaxReset > 86400000) {
    maxTemp = minTemp = temperature;
    maxHum = minHum = humidity;
    lastMinMaxReset = millis();
  }
}

// === HAFIZA FONKSİYONLARI ===

// EEPROM'dan hafıza kanallarını yükle
void loadMemoryChannels() {
  // İlk olarak kullanılan slot sayısını oku (eski değer)
  EEPROM.get(0, usedMemorySlots);
  if (usedMemorySlots < 0 || usedMemorySlots > MEMORY_CHANNELS) {
    usedMemorySlots = 0; // Geçersizse sıfırla
  }
  
  // Hafıza kanallarını yükle
  for (int i = 0; i < MEMORY_CHANNELS; i++) {
    int addr = sizeof(int) + (i * sizeof(int)); // İlk int usedMemorySlots için
    EEPROM.get(addr, memoryChannels[i]);
    if (memoryChannels[i] < minFreq || memoryChannels[i] > maxFreq) {
      memoryChannels[i] = 0; // Geçersiz değerleri sıfırla
    }
  }
  
  // Gerçekten dolu olan slot sayısını hesapla (güvenlik için)
  usedMemorySlots = 0;
  for (int i = 0; i < MEMORY_CHANNELS; i++) {
    if (memoryChannels[i] != 0) usedMemorySlots++;
  }
  
  // Bir sonraki kaydetme yerini bul
  currentMemorySlot = usedMemorySlots;
}

// EEPROM'a hafıza kanallarını kaydet
void saveMemoryChannels() {
  // Kullanılan slot sayısını kaydet
  EEPROM.put(0, usedMemorySlots);
  
  // Hafıza kanallarını kaydet
  for (int i = 0; i < MEMORY_CHANNELS; i++) {
    int addr = sizeof(int) + (i * sizeof(int));
    EEPROM.put(addr, memoryChannels[i]);
  }
  EEPROM.commit();
}

// Aynı frekansın zaten kayıtlı olup olmadığını kontrol et
bool isFrequencyAlreadySaved(int frequency) {
  for (int i = 0; i < MEMORY_CHANNELS; i++) {
    if (memoryChannels[i] == frequency) {
      return true;
    }
  }
  return false;
}

// Tüm hafıza kanallarını sil
void clearAllMemoryChannels() {
  for (int i = 0; i < MEMORY_CHANNELS; i++) {
    memoryChannels[i] = 0;
  }
  usedMemorySlots = 0;
  selectedMemorySlot = 0;
  saveMemoryChannels();
  
  // Başarı mesajı
  tft.fillRect(10, 70, 108, 40, ST7735_RED);
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(15, 75);
  tft.setTextSize(1);
  tft.print("TUM KANALLAR");
  tft.setCursor(15, 85);
  tft.print("SILINDI!");
  tft.setCursor(15, 95);
  tft.print("8 kanal bos");
  delay(2000);
  drawMemoryScreen();
}

// Seçili hafıza kanalına git
void selectMemoryChannel(int slot) {
  if (slot < 0 || slot >= MEMORY_CHANNELS || memoryChannels[slot] == 0) {
    return; // Geçersiz slot
  }
  
  // Frekansı değiştir
  freqX10 = memoryChannels[slot];
  rotaryEncoder.setEncoderValue(freqX10);
  radio.setFrequency(freqX10 / 10.0);
  
  // Başarı mesajı
  tft.fillRect(10, 70, 108, 30, ST7735_GREEN);
  tft.setTextColor(ST7735_BLACK);
  tft.setCursor(15, 75);
  tft.setTextSize(1);
  char msg[25];
  sprintf(msg, "%.1f MHz", freqX10 / 10.0);
  tft.print(msg);
  tft.setCursor(15, 85);
  tft.print("kanalina gecildi");
  delay(1500);
  
  // Ana ekrana dön
  currentScreen = SCREEN_MAIN;
  dataChanged = true;
  drawMainScreen();
}

// Mevcut frekansı hafızaya kaydet (ilk boş slota)
void saveCurrentFrequency() {
  // Aynı frekans zaten kayıtlı mı kontrol et
  if (isFrequencyAlreadySaved(freqX10)) {
    // Kanal var uyarısı
    tft.fillRect(10, 70, 108, 30, ST7735_YELLOW);
    tft.setTextColor(ST7735_BLACK);
    tft.setCursor(15, 75);
    tft.setTextSize(1);
    tft.print("KANAL VAR!");
    tft.setCursor(15, 85);
    char freqStr[25];
    sprintf(freqStr, "%.1f MHz kayitli", freqX10 / 10.0);
    tft.print(freqStr);
    delay(2000);
    dataChanged = true;
    drawMainScreen();
    return;
  }
  
  // İlk boş slotu bul
  int slot = -1;
  for (int i = 0; i < MEMORY_CHANNELS; i++) {
    if (memoryChannels[i] == 0) {
      slot = i;
      break;
    }
  }
  
  if (slot == -1) {
    // Hafıza dolu uyarısı
    tft.fillRect(10, 70, 108, 30, ST7735_RED);
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(15, 75);
    tft.setTextSize(1);
    tft.print("HAFIZA DOLU!");
    tft.setCursor(15, 85);
    tft.print("Max 8 kanal");
    delay(2000);
    dataChanged = true;
    drawMainScreen();
    return;
  }
  
  // Frekansı kaydet
  memoryChannels[slot] = freqX10;

  // Kullanılan slot sayısını güncelle
  usedMemorySlots = 0;
  for (int i = 0; i < MEMORY_CHANNELS; i++) if (memoryChannels[i] != 0) usedMemorySlots++;
  saveMemoryChannels();
  
  // Başarı mesajı (2s)
  tft.fillRect(10, 70, 108, 30, ST7735_GREEN);
  tft.setTextColor(ST7735_BLACK);
  tft.setCursor(15, 75);
  tft.setTextSize(1);
  char successMsg[40];
  sprintf(successMsg, "%.1f %d. kanala", freqX10 / 10.0, slot + 1);
  tft.print(successMsg);
  tft.setCursor(15, 85);
  tft.print("kaydedildi");
  delay(2000);
  dataChanged = true;
  drawMainScreen();
}

// === ÇİZİM FONKSİYONLARI ===

// Küçük sinyal göstergesi kaldırıldı
void drawSmallSignal(int x, int y, int strength) {
  // Sinyal göstergesi kaldırıldı
}

// Gelişmiş spektrum
void drawAdvancedSpectrum(int x, int y) {
  int barWidth = 2;
  int spacing = 1;
  int totalWidth = tft.width() - (x * 2);
  int barCount = totalWidth / (barWidth + spacing);
  
  for (int i = 0; i < barCount; i++) {
    int barHeight = random(1, 15);
    uint16_t color;
    
    if (isNightMode) {
      color = (barHeight > 10) ? ST7735_RED : ST7735_BLUE;
    } else {
      if (barHeight > 12) color = ST7735_RED;
      else if (barHeight > 10) color = ST7735_YELLOW;
      else if (barHeight > 8) color = ST7735_CYAN;
      else if (barHeight < 5) color = ST7735_BLUE;
      else color = ST7735_GREEN;
    }
    
    tft.fillRect(x + i * (barWidth + spacing), y - barHeight, barWidth, barHeight, color);
  }
}

// İstasyon adı getir
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
    // Bilinmeyen istasyonlar için boş bırak
  }
}

// Hafıza ekranı - güncellenmiş
void drawMemoryScreen() {
  tft.fillScreen(ST7735_BLACK);
  
  tft.setTextSize(1);
  tft.setCursor(25, 5);
  tft.setTextColor(ST7735_CYAN, ST7735_BLACK);
  tft.print("HAFIZA KANALLARI");
  
  tft.setCursor(5, 20);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  char memoryInfo[25];
  sprintf(memoryInfo, "Kayitli: %d/%d kanal", usedMemorySlots, MEMORY_CHANNELS);
  tft.print(memoryInfo);
  
  // Seçili slot göstergesi
  if (usedMemorySlots > 0) {
    tft.setCursor(5, 30);
    tft.setTextColor(ST7735_GREEN, ST7735_BLACK);
    char selectedInfo[25];
    sprintf(selectedInfo, "Secili: %d", selectedMemorySlot + 1);
    tft.print(selectedInfo);
  }
  
  // Kayıtlı kanalları göster
  for (int i = 0; i < MEMORY_CHANNELS; i++) {
    int yPos = 45 + i * 12;
    tft.setCursor(5, yPos);
    
    // Seçili slot'u vurgula
    if (i == selectedMemorySlot && memoryChannels[i] != 0) {
      tft.setTextColor(ST7735_BLACK, ST7735_GREEN);
    } else if (memoryChannels[i] != 0) {
      tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
    } else {
      tft.setTextColor(ST7735_CYAN, ST7735_BLACK); // Boş slotlar için mavi
    }
    
    if (memoryChannels[i] != 0) {
      String line1, line2;
      getStationName(memoryChannels[i], line1, line2);
      
      char channelStr[25];
      if (line1 != "" && line2 != "") {
        sprintf(channelStr, "%d: %.1f - %s %s", i+1, memoryChannels[i]/10.0, line1.c_str(), line2.c_str());
      } else {
        sprintf(channelStr, "%d: %.1f MHz", i+1, memoryChannels[i]/10.0);
      }
      tft.print(channelStr);
    } else {
      char emptySlot[15];
      sprintf(emptySlot, "%d: <bos>", i+1);
      tft.print(emptySlot);
    }
  }
  
  // Kontrol bilgileri
  tft.setCursor(5, 138);
  tft.setTextColor(ST7735_GREEN, ST7735_BLACK);
  tft.print("Donus: Kanal sec");
  
  tft.setCursor(5, 148);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.print("Tikla: Kanala git");
  
  tft.setCursor(5, 158);
  tft.setTextColor(ST7735_RED, ST7735_BLACK);
  tft.print("5s bas: Hepsini sil");
}

// Ana ekran - sinyal çubukları küçültülmüş
void drawMainScreen() {
  if (dataChanged) {
    tft.fillScreen(isNightMode ? ST7735_BLACK : ST7735_BLACK);
    dataChanged = false;
  }
  
  // Gece modu kontrolü
  isNightMode = isNightTime();
  uint16_t titleColor = isNightMode ? ST7735_RED : getRainbowColor();
  uint16_t textColor = isNightMode ? ST7735_RED : ST7735_WHITE;
  
  // Başlık - küçük font (tek satır: TA2CAY FM RADYO V2.0)
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.setTextColor(titleColor, ST7735_BLACK);
  tft.print("TA2CAY FM RADYO V2.1");
  
  // Frekans
  char freqStr[12];
  sprintf(freqStr, "%.1f MHz", freqX10 / 10.0);
  tft.setTextSize(2);
  int16_t freqWidth = strlen(freqStr) * 12;
  int16_t freqX = (tft.width() - freqWidth) / 2;
  tft.setCursor(freqX, 25);
  tft.setTextColor(ST7735_CYAN, ST7735_BLACK);
  tft.print(freqStr);
  
  // Küçük sinyal göstergesi kaldırıldı
  // drawSmallSignal(freqX + freqWidth + 5, 35, getSignalStrength());
  
  // Dekoratif çizgi
  tft.drawLine(freqX - 5, 45, freqX + freqWidth + 5, 45, titleColor);
  
  // Sıcaklık bölümü
  tft.setTextSize(1);
  tft.setCursor(5, 55);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.print("SICAKLIK");
  tft.setCursor(5, 65);
  tft.setTextColor(textColor, ST7735_BLACK);
  if (temperature > 0) {
    tft.print(temperature, 1);
    tft.print("C " + getTempTrend());
  } else {
    tft.print("--C");
  }
  
  // Sıcaklık göstergesi
  int tempBar = map(constrain(temperature, 0, 50), 0, 50, 0, 20);
  tft.drawRect(5, 80, 8, 22, ST7735_WHITE);
  tft.fillRect(6, 81, 6, 20, ST7735_BLACK); // Temizle
  tft.fillRect(6, 81 + (20 - tempBar), 6, tempBar, ST7735_RED);
  tft.fillCircle(9, 103, 4, ST7735_RED);
  
  // Nem bölümü
  tft.setCursor(85, 55);
  tft.setTextColor(ST7735_BLUE, ST7735_BLACK);
  tft.print("NEM");
  tft.setCursor(85, 65);
  tft.setTextColor(textColor, ST7735_BLACK);
  if (humidity > 0) {
    tft.print("%");
    tft.print(humidity, 1);
  } else {
    tft.print("%--%");
  }
  
  // Nem göstergesi
  int humBar = map(constrain(humidity, 0, 100), 0, 100, 0, 20);
  tft.drawRect(115, 80, 8, 22, ST7735_WHITE);
  tft.fillRect(116, 81, 6, 20, ST7735_BLACK); // Temizle
  tft.fillRect(116, 81 + (20 - humBar), 6, humBar, ST7735_BLUE);
  
  // İstasyon adı - sadece bilinen istasyonlar için göster
  String line1, line2;
  getStationName(freqX10, line1, line2);
  
  if (line1 != "" && line2 != "") {
    tft.setTextSize(2);
    tft.setTextColor(textColor, ST7735_BLACK);
    int16_t textWidth1 = line1.length() * 12;
    int16_t textWidth2 = line2.length() * 12;
    tft.setCursor((tft.width() - textWidth1) / 2, 75);
    tft.print(line1);
    tft.setCursor((tft.width() - textWidth2) / 2, 95);
    tft.print(line2);
  }
  
  // Tarih ve saat
  RtcDateTime now = Rtc.GetDateTime();
  char timeStr[10];
  sprintf(timeStr, "%02d:%02d:%02d", now.Hour(), now.Minute(), now.Second());
  char dateStr[12];
  sprintf(dateStr, "%02d/%02d/%04d", now.Day(), now.Month(), now.Year());
  
  tft.setTextSize(1);
  tft.setCursor((tft.width() - strlen(timeStr) * 6) / 2, 115);
  tft.setTextColor(textColor, ST7735_BLACK);
  tft.print(timeStr);
  
  tft.setCursor((tft.width() - strlen(dateStr) * 6) / 2, 125);
  tft.print(dateStr);
  
  // Spektrum
  drawAdvancedSpectrum(5, 155);
}

// Detay ekranı - düzeltilmiş görünüm
void drawDetailScreen() {
  tft.fillScreen(ST7735_BLACK);
  
  tft.setTextSize(1);
  tft.setCursor(20, 5);
  tft.setTextColor(ST7735_CYAN, ST7735_BLACK);
  tft.print("SISTEM DETAYLARI");
  
  RtcDateTime now = Rtc.GetDateTime();
  
  // Tarih/Saat - ZAMAN yazısı kaldırıldı
  char dateTimeStr[20];
  sprintf(dateTimeStr, "%02d/%02d/%04d %02d:%02d", now.Day(), now.Month(), now.Year(), now.Hour(), now.Minute());
  tft.setCursor(5, 20);
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  tft.print(dateTimeStr);
  
  // Radyo bilgileri - bir satır yukarı
  tft.setCursor(5, 35);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  char radioStr[20];
  sprintf(radioStr, "RADYO: %.1f MHz", freqX10/10.0);
  tft.print(radioStr);
  
  // Sinyal bilgisi - bir satır yukarı
  tft.setCursor(5, 45);
  tft.setTextColor(ST7735_GREEN, ST7735_BLACK);
  char signalStr[20];
  sprintf(signalStr, "Sinyal: %d/5", getSignalStrength());
  tft.print(signalStr);
  
  // Hava durumu detayları - bir satır yukarı
  tft.setCursor(5, 60);
  tft.setTextColor(ST7735_MAGENTA, ST7735_BLACK);
  tft.print("HAVA DURUMU:");
  
  tft.setCursor(5, 70);
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  char tempStr[20];
  sprintf(tempStr, "Sicaklik: %.1fC %s", temperature, getTempTrend().c_str());
  tft.print(tempStr);
  
  tft.setCursor(5, 80);
  char humStr[20];
  sprintf(humStr, "Nem: %.1f%%", humidity);
  tft.print(humStr);
  
  // Min/Max değerler - bir satır yukarı
  tft.setCursor(5, 95);
  tft.setTextColor(ST7735_CYAN, ST7735_BLACK);
  tft.print("SON 24 SAAT:");
  
  tft.setCursor(5, 105);
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  char minMaxTempStr[25];
  if (maxTemp > -999 && minTemp < 999) {
    sprintf(minMaxTempStr, "MaxS:%.1f MinS:%.1f", maxTemp, minTemp);
  } else {
    sprintf(minMaxTempStr, "MaxS:-- MinS:--");
  }
  tft.print(minMaxTempStr);
  
  tft.setCursor(5, 115);
  char minMaxHumStr[25];
  if (maxHum > -999 && minHum < 999) {
    sprintf(minMaxHumStr, "MaxN:%.1f MinN:%.1f", maxHum, minHum);
  } else {
    sprintf(minMaxHumStr, "MaxN:-- MinN:--");
  }
  tft.print(minMaxHumStr);
  
  // Sistem bilgileri - bir satır yukarı
  tft.setCursor(5, 130);
  tft.setTextColor(ST7735_RED, ST7735_BLACK);
  tft.print("SISTEM:");
  
  tft.setCursor(5, 140);
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  char sysInfo[25];
  sprintf(sysInfo, "RAM:%dKB CPU:%dMHz", ESP.getFreeHeap()/1024, ESP.getCpuFreqMHz());
  tft.print(sysInfo);
  
  // Uptime - kısaltılmış format
  tft.setCursor(5, 150);
  unsigned long uptimeSeconds = millis() / 1000;
  unsigned long days = uptimeSeconds / 86400;
  unsigned long remainingSeconds = uptimeSeconds % 86400;
  char uptimeStr[20];
  sprintf(uptimeStr, "Cihaz %lus %lud acik", remainingSeconds, days);
  tft.print(uptimeStr);
}

// DHT okuma
void readDHT() {
  if (millis() - lastDHTRead > 3000) { // 3 saniyede bir
    lastDHTRead = millis();
    lastTemp = temperature;
    
    float newTemp = dht.readTemperature();
    float newHum = dht.readHumidity();
    
    if (!isnan(newTemp) && !isnan(newHum)) {
      temperature = newTemp;
      humidity = newHum;
      updateMinMax();
    }
  }
}

// GLOBAL: tık sırasında uzun basma sonrası kısa basmayı göz ardı etme
bool ignoreNextClick = false;

void setup() {
  Serial.begin(115200);
  Serial.println("TA2CAY v2.1 Baslatiliyor...");
  
  // EEPROM başlat ve hafıza kanallarını yükle
  EEPROM.begin(EEPROM_SIZE);
  
  // İlk başlatmada hafıza kanallarını sıfırla
  for (int i = 0; i < MEMORY_CHANNELS; i++) {
    memoryChannels[i] = 0;
  }
  loadMemoryChannels();
  
  dht.begin();
  
  Rtc.Begin();
  // İlk kurulumda zamanı ayarla (isteğe bağlı)
  // RtcDateTime compiled = RtcDateTime(2025, 9, 24, 15, 30, 0);
  // Rtc.SetDateTime(compiled);
  
  Wire.begin(21, 22);
  
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  
  // Splash screen
  tft.fillScreen(ST7735_BLACK);
  tft.setTextSize(2);
  tft.setCursor(15, 30);
  tft.setTextColor(ST7735_CYAN);
  tft.print("TA2CAY");
  tft.setTextSize(1);
  tft.setCursor(20, 55);
  tft.setTextColor(ST7735_YELLOW);
  tft.print("Smart Radio v2.1");
  tft.setCursor(30, 75);
  tft.setTextColor(ST7735_GREEN);
  tft.print("Loading...");
  
  for (int i = 0; i < 10; i++) {
    tft.fillRect(20 + i * 9, 100, 8, 4, rainbowColors[i % 6]);
    delay(150);
  }
  
  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  rotaryEncoder.setBoundaries(minFreq, maxFreq, false);
  rotaryEncoder.setEncoderValue(freqX10);
  rotaryEncoder.setAcceleration(0);
  
  // Buton pini pullup olarak ayarla (digitalRead ile kullanmak için)
  pinMode(ROTARY_SW, INPUT_PULLUP);
  
  radio.setFrequency(freqX10 / 10.0);
  
  readDHT();
  maxTemp = minTemp = temperature;
  maxHum = minHum = humidity;
  lastMinMaxReset = millis();
  
  Serial.println("Sistem Hazir!");
  dataChanged = true;
  drawMainScreen();
}

void loop() {
  readDHT();
  
  // Encoder değişikliği
  if (rotaryEncoder.encoderChanged()) {
    int newValue = (int)rotaryEncoder.readEncoder();
    
    if (currentScreen == SCREEN_MAIN) {
      // Ana ekranda frekans değişimi
      freqX10 = newValue;
      radio.setFrequency(freqX10 / 10.0);
      dataChanged = true;
      drawMainScreen();
    } else if (currentScreen == SCREEN_MEMORY && usedMemorySlots > 0) {
      // Hafıza ekranında kanal seçimi - sadece dolu kanallar arasında gezin
      int direction = newValue - selectedMemorySlot;
      
      if (direction > 0) {
        // İleri gitmek için sonraki dolu slotu bul
        for (int i = selectedMemorySlot + 1; i < MEMORY_CHANNELS; i++) {
          if (memoryChannels[i] != 0) {
            selectedMemorySlot = i;
            break;
          }
        }
        // Son dolu kanaldaysak başa dön
        if (selectedMemorySlot == MEMORY_CHANNELS - 1 || memoryChannels[selectedMemorySlot + 1] == 0) {
          for (int i = 0; i < selectedMemorySlot; i++) {
            if (memoryChannels[i] != 0) {
              selectedMemorySlot = i;
              break;
            }
          }
        }
      } else if (direction < 0) {
        // Geriye gitmek için önceki dolu slotu bul
        for (int i = selectedMemorySlot - 1; i >= 0; i--) {
          if (memoryChannels[i] != 0) {
            selectedMemorySlot = i;
            break;
          }
        }
        // İlk dolu kanaldaysak sona git
        if (selectedMemorySlot == 0 || memoryChannels[selectedMemorySlot - 1] == 0) {
          for (int i = MEMORY_CHANNELS - 1; i > selectedMemorySlot; i--) {
            if (memoryChannels[i] != 0) {
              selectedMemorySlot = i;
              break;
            }
          }
        }
      }
      
      // Encoder'ın değerini tekrar sıfırla
      rotaryEncoder.setEncoderValue(selectedMemorySlot);
      drawMemoryScreen();
    }
  }
  
  // Kısa basma kontrolü - ekranlar arası geçiş
  if (rotaryEncoder.isEncoderButtonClicked()) {
    unsigned long pressTime = millis();
    
    if (pressTime - lastButtonPress > 300) { // Debounce
      if (ignoreNextClick) {
        // Uzun basma sonrası gelmiş olabilecek yanlış kliki atla
        ignoreNextClick = false;
        lastButtonPress = pressTime;
      } else {
        if (currentScreen == SCREEN_MEMORY && usedMemorySlots > 0) {
          // Hafıza ekranında seçili kanala git
          selectMemoryChannel(selectedMemorySlot);
          // Ana ekrana döndükten sonra encoder'ı frekans moduna ayarla
          rotaryEncoder.setBoundaries(minFreq, maxFreq, false);
          rotaryEncoder.setEncoderValue(freqX10);
        } else {
          // Diğer ekranlarda normal geçiş
          currentScreen = (ScreenState)((currentScreen + 1) % 3);
          
          switch(currentScreen) {
            case SCREEN_MAIN:
              // Ana ekrana dönerken encoder'ı frekans moduna ayarla
              rotaryEncoder.setBoundaries(minFreq, maxFreq, false);
              rotaryEncoder.setEncoderValue(freqX10);
              dataChanged = true;
              drawMainScreen();
              Serial.println("Ana ekran");
              break;
            case SCREEN_DETAILS:
              drawDetailScreen();
              Serial.println("Detay ekran");
              break;
            case SCREEN_MEMORY:
              // İlk dolu slotu seç
              selectedMemorySlot = 0;
              for (int i = 0; i < MEMORY_CHANNELS; i++) {
                if (memoryChannels[i] != 0) {
                  selectedMemorySlot = i;
                  break;
                }
              }
              // Encoder'ı hafıza moduna ayarla (0-7 arası)
              rotaryEncoder.setBoundaries(0, MEMORY_CHANNELS - 1, false);
              rotaryEncoder.setEncoderValue(selectedMemorySlot);
              drawMemoryScreen();
              Serial.println("Hafiza ekran");
              break;
          }
        }
        lastButtonPress = pressTime;
      }
    }
  }
  
  // UZUN BASMA KONTROLÜ - 3 SANİYE
  static unsigned long buttonHoldStartTime = 0;
  static bool isButtonHeld = false;
  static bool longPressExecuted = false;
  
  // Buton durumunu kontrol et (LOW = basılı, çünkü INPUT_PULLUP)
  bool buttonCurrentlyPressed = (digitalRead(ROTARY_SW) == LOW);
  
  if (buttonCurrentlyPressed && !isButtonHeld) {
    // Buton yeni basıldı
    buttonHoldStartTime = millis();
    isButtonHeld = true;
    longPressExecuted = false;
    Serial.print("Buton basili - sayim basladi. Mevcut ekran: ");
    Serial.println(currentScreen);
  }
  
  if (buttonCurrentlyPressed && isButtonHeld && !longPressExecuted) {
    // Buton hala basılı - süreyi kontrol et
    unsigned long holdTime = millis() - buttonHoldStartTime;
    
    if (holdTime >= 3000) {
      // 3 saniye tamamlandı!
      if (currentScreen == SCREEN_MAIN) {
        // Ana ekranda kanal kaydet
        Serial.print("3 saniye tamamlandi - kaydediliyor! Frekans: ");
        Serial.println(freqX10);
        saveCurrentFrequency();
      } else if (currentScreen == SCREEN_MEMORY) {
        // Hafıza ekranında tüm kanalları sil
        Serial.println("3 saniye tamamlandi - tum kanallar siliniyor!");
        clearAllMemoryChannels();
      }
      longPressExecuted = true;
      ignoreNextClick = true; // Uzun basmadan sonra gelebilecek click'i atla
      lastButtonPress = millis(); // click debounce'i sıfırla
    }
  }
  
  if (!buttonCurrentlyPressed && isButtonHeld) {
    // Buton bırakıldı
    isButtonHeld = false;
    longPressExecuted = false;
    Serial.println("Buton birakildi");
  }
  
  // Ekran güncelleme (1 saniyede bir)
  if (millis() - lastDisplayUpdate > 1000) {
    lastDisplayUpdate = millis();
    
    switch(currentScreen) {
      case SCREEN_MAIN:
        drawMainScreen();
        break;
      case SCREEN_DETAILS:
        drawDetailScreen();
        break;
      case SCREEN_MEMORY:
        drawMemoryScreen();
        break;
    }
  }
}