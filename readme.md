# TA2CAY FM Radyo & DHT11 Hava Durumu İstasyonu

Bu proje, ESP32 mikrodenetleyici, 1.8" ST7735 TFT ekran, TEA5767 FM Radyo modülü ve DHT11/22 sıcaklık/nem sensörünü birleştirerek taşınabilir bir radyo ve çevre istasyonu oluşturur.
İnternet bağlantısı  gerektirmez. 

## Özellikler

- **FM Radyo:** Döner enkoder ile frekans ayarlama (87.5 MHz - 108.0 MHz).
- **Hava Durumu:** DHT11/22 sensöründen anlık sıcaklık ve nem okuma.
- **RTC Saat:** DS1302 modülü ile güncel saat ve tarih gösterimi.
- **Görselleştirme:** Sinyal gücü ve spektrum analizörü simülasyonu.
- **Ekranlar:** Ana ekran (Frekans, Temel Bilgiler) ve Detay Ekranı (Min/Max, Sistem Bilgileri).

## Donanım Bağlantıları (ESP32)

| Modül | ESP32 Pini | Açıklama |
| :--- | :--- | :--- |
| **TFT ST7735** | | |
| RST | 16 | Reset |
| DC | 17 | Data/Command |
| CS | 5 | Chip Select |
| **Döner Enkoder** | | |
| CLK | 32 | Clock Pin |
| DT | 33 | Data Pin |
| SW | 25 | Switch (Buton) |
| **TEA5767 (Radyo)** | | |
| SDA | 21 | I2C Data |
| SCL | 22 | I2C Clock |
| **DHT11/22** | 4 | Data Pini |
| **DS1302 RTC** | | |
| DAT | 27 | Data Pin |
| CLK | 26 | Clock Pin |
| RST | 14 | Reset Pin |

## Kullanılan Kütüphaneler

Bu projeyi derlemek için aşağıdaki kütüphanelerin kurulu olması gerekmektedir:
- Adafruit GFX
- Adafruit ST7735
- AiEsp32RotaryEncoder
- TEA5767Radio
- DHT sensor library
- RtcDS1302 (Makuna Versiyonu önerilir)

## Lisans

Bu proje TA2CAY tarafından açık kaynak (Public) olarak yayınlanmıştır.
