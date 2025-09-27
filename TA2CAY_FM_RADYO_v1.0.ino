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
#define FAVORITES_COUNT 5
int favorites[FAVORITES_COUNT] = {934, 918, 992, 1005, 1049}; // Varsayılan favoriler
int currentFavoriteIndex = 0;

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
bool isNightMode = false;
bool showingFavorites = false;
bool dataChanged = true; // Ekran güncelleme kontrolü

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

// === FAVORİ İSTASYON FONKSİYONLARI ===

// EEPROM'dan favorileri yükle
void loadFavorites() {
  for (int i = 0; i < FAVORITES_COUNT; i++) {
    int addr = i * sizeof(int);
    EEPROM.get(addr, favorites[i]);
    if (favorites[i] < minFreq || favorites[i] > maxFreq) {
      favorites[i] = 934; // Varsayılan değer
    }
  }
}

// EEPROM'a favorileri kaydet
void saveFavorites() {
  for (int i = 0; i < FAVORITES_COUNT; i++) {
    int addr = i * sizeof(int);
    EEPROM.put(addr, favorites[i]);
  }
  EEPROM.commit();
}

// Mevcut frekansı favorilere ekle
void addToFavorites() {
  favorites[currentFavoriteIndex] = freqX10;
  currentFavoriteIndex = (currentFavoriteIndex + 1) % FAVORITES_COUNT;
  saveFavorites();
  
  // Onay mesajı göster
  tft.fillRect(20, 70, 88, 20, ST7735_GREEN);
  tft.setTextColor(ST7735_BLACK);
  tft.setCursor(25, 75);
  tft.setTextSize(1);
  tft.print("FAVORİ EKLENDİ!");
  delay(1500);
  dataChanged = true;
}

// === ÇİZİM FONKSİYONLARI ===

// Küçük sinyal çubukları
void drawSmallSignalBars(int x, int y, int strength) {
  for (int i = 0; i < 5; i++) {
    uint16_t color = (i < strength) ? ST7735_GREEN : ST7735_BLACK;
    if (i < strength && strength >= 4) color = ST7735_CYAN;
    if (isNightMode) color = (color == ST7735_BLACK) ? ST7735_BLACK : ST7735_RED;
    
    tft.fillRect(x + i * 2, y - i * 1, 2, 4 + i * 1, color);
    tft.drawRect(x + i * 2, y - i * 1, 2, 4 + i * 1, ST7735_WHITE);
  }
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

// Favori ekranı
void drawFavoriteScreen() {
  tft.fillScreen(ST7735_BLACK);
  
  tft.setTextSize(1);
  tft.setCursor(30, 5);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.print("FAVORİ İSTASYONLAR");
  
  for (int i = 0; i < FAVORITES_COUNT; i++) {
    uint16_t color = (i == currentFavoriteIndex) ? ST7735_CYAN : ST7735_WHITE;
    tft.setTextColor(color, ST7735_BLACK);
    tft.setCursor(5, 25 + i * 15);
    
    String line1, line2;
    getStationName(favorites[i], line1, line2);
    char favStr[25];
    if (line1 != "" && line2 != "") {
      sprintf(favStr, "%d: %.1f - %s %s", i+1, favorites[i]/10.0, line1.c_str(), line2.c_str());
    } else {
      sprintf(favStr, "%d: %.1f MHz", i+1, favorites[i]/10.0);
    }
    tft.print(favStr);
  }
  
  tft.setCursor(5, 140);
  tft.setTextColor(ST7735_GREEN, ST7735_BLACK);
  tft.print("Enter: Sec, Uzun: Kaydet");
  
  tft.setCursor(5, 155);
  tft.setTextColor(ST7735_RED, ST7735_BLACK);
  tft.print("Cevir: Cik");
}

// Ana ekran - sadece değişen kısımları güncelle
void drawMainScreen() {
  if (dataChanged) {
    tft.fillScreen(isNightMode ? ST7735_BLACK : ST7735_BLACK);
    dataChanged = false;
  }
  
  // Gece modu kontrolü
  isNightMode = isNightTime();
  uint16_t titleColor = isNightMode ? ST7735_RED : getRainbowColor();
  uint16_t textColor = isNightMode ? ST7735_RED : ST7735_WHITE;
  
  // Başlık - küçük font
  tft.setTextSize(1);
  tft.setCursor(25, 5);
  tft.setTextColor(titleColor, ST7735_BLACK);
  tft.print("TA2CAY FM v2.1");
  
  // Frekans
  char freqStr[12];
  sprintf(freqStr, "%.1f MHz", freqX10 / 10.0);
  tft.setTextSize(2);
  int16_t freqWidth = strlen(freqStr) * 12;
  int16_t freqX = (tft.width() - freqWidth) / 2;
  tft.setCursor(freqX, 25);
  tft.setTextColor(ST7735_CYAN, ST7735_BLACK);
  tft.print(freqStr);
  
  // Sinyal çubukları
  drawSmallSignalBars(freqX + freqWidth + 5, 35, getSignalStrength());
  
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

// Detay ekranı - iyileştirilmiş
void drawDetailScreen() {
  tft.fillScreen(ST7735_BLACK);
  
  tft.setTextSize(1);
  tft.setCursor(25, 5);
  tft.setTextColor(ST7735_CYAN, ST7735_BLACK);
  tft.print("SISTEM DETAYLARI");
  
  RtcDateTime now = Rtc.GetDateTime();
  
  // Tarih/Saat
  tft.setCursor(5, 20);
  tft.setTextColor(ST7735_GREEN, ST7735_BLACK);
  tft.print("ZAMAN:");
  char fullDateTime[25];
  sprintf(fullDateTime, "%02d/%02d/%04d %02d:%02d:%02d",
          now.Day(), now.Month(), now.Year(),
          now.Hour(), now.Minute(), now.Second());
  tft.setCursor(5, 30);
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  tft.print(fullDateTime);
  
  // Radyo bilgileri
  tft.setCursor(5, 45);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);
  tft.print("RADYO:");
  tft.setCursor(5, 55);
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  char radioInfo[25];
  sprintf(radioInfo, "%.1f MHz (Sinyal:%d/5)", freqX10/10.0, getSignalStrength());
  tft.print(radioInfo);
  
  // Hava durumu
  tft.setCursor(5, 70);
  tft.setTextColor(ST7735_MAGENTA, ST7735_BLACK);
  tft.print("HAVA:");
  tft.setCursor(5, 80);
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  char weatherInfo[25];
  sprintf(weatherInfo, "%.1fC %s, %%%0.1f", temperature, getTempTrend().c_str(), humidity);
  tft.print(weatherInfo);
  
  // Min/Max değerler - sıcaklık ve nem
  tft.setCursor(5, 90);
  tft.setTextColor(ST7735_CYAN, ST7735_BLACK);
  char minMaxTempStr[25];
  sprintf(minMaxTempStr, "MaxS:%.1f MinS:%.1f", maxTemp, minTemp);
  tft.print(minMaxTempStr);
  
  tft.setCursor(5, 100);
  tft.setTextColor(ST7735_CYAN, ST7735_BLACK);
  char minMaxHumStr[25];
  sprintf(minMaxHumStr, "MaxN:%.1f MinN:%.1f", maxHum, minHum);
  tft.print(minMaxHumStr);
  
  // Sistem
  tft.setCursor(5, 115);
  tft.setTextColor(ST7735_RED, ST7735_BLACK);
  tft.print("SISTEM:");
  tft.setCursor(5, 125);
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  char sysInfo[25];
  sprintf(sysInfo, "RAM:%dKB CPU:%dMHz", ESP.getFreeHeap()/1024, ESP.getCpuFreqMHz());
  tft.print(sysInfo);
  
  // Uptime
  unsigned long uptime = millis() / 1000;
  int hours = uptime / 3600;
  int minutes = (uptime % 3600) / 60;
  tft.setCursor(5, 135);
  char uptimeStr[25];
  sprintf(uptimeStr, "Uptime: %02dh %02dm", hours, minutes);
  tft.print(uptimeStr);
  
  // Kontrollar
  tft.setCursor(5, 150);
  tft.setTextColor(ST7735_GREEN, ST7735_BLACK);
  tft.print("Kisa: Geri, Uzun: Favoriler");
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

void setup() {
  Serial.begin(115200);
  Serial.println("🎵 TA2CAY v2.1 Başlatılıyor... 🎵");
  
  // EEPROM başlat
  EEPROM.begin(EEPROM_SIZE);
  loadFavorites();
  
  dht.begin();
  
  Rtc.Begin();
  // İlk kurulumda zamanı ayarla:
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
  
  radio.setFrequency(freqX10 / 10.0);
  
  readDHT();
  maxTemp = minTemp = temperature;
  maxHum = minHum = humidity;
  lastMinMaxReset = millis();
  
  Serial.println("🚀 Sistem Hazır! 🚀");
  dataChanged = true;
  drawMainScreen();
}

void loop() {
  readDHT();
  
  // Encoder değişikliği
  if (rotaryEncoder.encoderChanged()) {
    int newFreq = (int)rotaryEncoder.readEncoder();
    
    if (showingFavorites) {
      // Favori listesinde gezinme
      currentFavoriteIndex = constrain(newFreq - minFreq, 0, FAVORITES_COUNT - 1);
      drawFavoriteScreen();
    } else {
      // Normal frekans değişimi
      freqX10 = newFreq;
      radio.setFrequency(freqX10 / 10.0);
      dataChanged = true;
      
      if (!showingDetails) {
        drawMainScreen();
      }
    }
  }
  
  // Buton kontrolü
  if (rotaryEncoder.isEncoderButtonClicked()) {
    unsigned long pressTime = millis();
    
    if (showingFavorites) {
      // Favori seçimi
      freqX10 = favorites[currentFavoriteIndex];
      rotaryEncoder.setEncoderValue(freqX10);
      radio.setFrequency(freqX10 / 10.0);
      showingFavorites = false;
      dataChanged = true;
      drawMainScreen();
    } else {
      // Ekran değiştirme
      if (pressTime - lastButtonPress > 300) { // Debounce
        showingDetails = !showingDetails;
        
        if (showingDetails) {
          drawDetailScreen();
        } else {
          dataChanged = true;
          drawMainScreen();
        }
        lastButtonPress = pressTime;
      }
    }
  }
  
  // Uzun basma kontrolü (2 saniye) - digitalRead yerine doğrudan kontrol
  static bool buttonPressed = false;
  static unsigned long buttonPressStart = 0;
  
  bool buttonState = (rotaryEncoder.isEncoderButtonDown());
  
  if (buttonState && !buttonPressed) {
    // Buton yeni basıldı
    buttonPressed = true;
    buttonPressStart = millis();
  } else if (!buttonState && buttonPressed) {
    // Buton bırakıldı
    unsigned long pressDuration = millis() - buttonPressStart;
    buttonPressed = false;
    
    if (pressDuration > 2000) {
      // Uzun basma algılandı
      if (showingDetails) {
        // Detay ekranındayken: Favori listesi
        showingFavorites = true;
        rotaryEncoder.setBoundaries(0, FAVORITES_COUNT - 1, false);
        rotaryEncoder.setEncoderValue(currentFavoriteIndex);
        drawFavoriteScreen();
      } else if (!showingFavorites) {
        // Ana ekrandayken: Favorilere ekle
        addToFavorites();
      }
    }
  }
  
  // Ekran güncelleme (1 saniyede bir)
  if (millis() - lastDisplayUpdate > 1000) {
    lastDisplayUpdate = millis();
    
    if (!showingFavorites) {
      if (showingDetails) {
        drawDetailScreen();
      } else {
        drawMainScreen();
      }
    }
  }
}