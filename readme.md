# TA2CAY FM Radyo & DHT11 Hava Durumu İstasyonu v2.1

[![TA2CAY FM Radyo & Hava Durumu İstasyonu Tanıtımı](https://img.youtube.com/vi/myVpQR3ZlVU/0.jpg)](https://www.youtube.com/watch?v=myVpQR3ZlVU)

## 🆕 Yenilikler v2.1

### 🔥 Ana Özellikler
- **Gelişmiş Hafıza Yönetimi**: 8 kanal tam destek
- **Optimized UI**: Daha temiz ve kullanıcı dostu arayüz  
- **Stabil Encoder Kontrolü**: %100 çalışan buton ve encoder sistemi
- **Smart Navigation**: Akıllı menü geçiş sistemi

### 1️⃣ Hafıza Sistemi Tamamen Yenilendi
- **Ana ekranda 3 saniye basma** ile mevcut frekansı hafızaya kaydetme
- **Hafıza ekranında encoder ile gezinme** - kayıtlı kanallar arası kolayca geçiş
- **Seçili kanala gitme** - hafıza ekranında kısa basma ile istediğiniz kanala anında geçiş
- **Toplu silme** - hafıza ekranında 3 saniye basma ile tüm kanalları temizleme
- **Duplicate kontrolü** - aynı frekansı tekrar kaydetmeye çalışırsa uyarı
- **Visual feedback** - seçili kanal yeşil arka planla vurgulanır

### 2️⃣ UI/UX İyileştirmeleri
- **Sinyal göstergesi kaldırıldı** - daha temiz görünüm
- **Detay ekranı optimize edildi** - tüm bilgiler ekrana sığdırıldı
- **Uptime formatı değişti**: `Cihaz 1580s 0d acik` şeklinde kısa format
- **"ZAMAN:" başlığı kaldırıldı** - daha fazla alan

### 3️⃣ Gelişmiş Encoder Kontrolü
- **Boundaries sistemi** - ekranlar arası geçişte encoder değerleri otomatik ayarlanır
- **Ana ekran**: 875-1080 MHz arası frekans kontrolü
- **Hafıza ekranı**: 0-7 arası kanal seçimi
- **Debounce sistemi** - yanlış basmalara karşı korunma
- **Long press detection** - 3 saniye uzun basma algılama (5 saniyeden düşürüldü)

### 4️⃣ Ekran Sistemi
**3 Ekran Modu:**
- **Ana Ekran**: Frekans, hava durumu, saat, spektrum
- **Detay Ekran**: Sistem bilgileri, 24 saatlik min/max değerler
- **Hafıza Ekran**: Kanal listesi, gezinme, yönetim

**Kontrol Şeması:**
ANA EKRAN:
Encoder döndürme: Frekans ayarlama
Kısa basma: Detay ekranına geç
3sn basma: Mevcut frekansı hafızaya kaydet

DETAY EKRAN:
Kısa basma: Hafıza ekranına geç

HAFIZA EKRAN:
Encoder döndürme: Kanallar arası gezinme
Kısa basma: Seçili kanala git (ana ekrana döner)
3sn basma: Tüm kanalları sil


### 5️⃣ Gece Modu  (v2.0'dan devam)
- Saat **22:00 – 07:00** arası aktif
- Kırmızı tema ile göz yorgunluğunu azaltır
- Spektrum renkleri gece moduna uygun

### 6️⃣ Hava Durumu Özellikleri (v2.0'dan devam)
- DHT11 sensörü ile anlık sıcaklık ve nem
- **Trend göstergesi**: `+` yükseliyor, `-` düşüyor, `=` sabit
- **Görsel termometre** ve nem göstergesi
- **24 saatlik min/max** değerleri tutma

## 🔧 Donanım Gereksinimleri

| Bileşen | Pin | Açıklama |
|---------|-----|----------|
| **ESP32** | - | Ana mikrokontrolör |
| **ST7735 TFT** | CS:5, DC:17, RST:16 | 1.8" Renkli Ekran |
| **TEA5767 FM** | SDA:21, SCL:22 | FM Radyo Modülü |
| **DHT11** | Data:4 | Sıcaklık & Nem Sensörü |
| **DS1302 RTC** | DAT:27, CLK:26, RST:14 | Gerçek Zamanlı Saat |
| **Rotary Encoder** | CLK:32, DT:33, SW:25 | Encoder & Buton |

## 📋 Gerekli Kütüphaneler

```cpp
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <AiEsp32RotaryEncoder.h>
#include <TEA5767Radio.h>
#include <SPI.h>
#include <DHT.h>
#include <RtcDS1302.h>
#include <EEPROM.h>
```

RTC Saat ve Tarih Kurulumu
Proje DS1302 RTC modülü ile çalışır ve ilk kurulumda saat ve tarih koddan ayarlanmalıdır.
Kod Örneği:
```cpp
#include <RtcDS1302.h>
ThreeWire myWire(27, 26, 14); // DAT, CLK, RST
RtcDS1302<ThreeWire> Rtc(myWire);

void setup() {
  Rtc.Begin();
  
  // İlk kurulumda aşağıdaki satırları kullanın:
  // RtcDateTime compiled = RtcDateTime(YIL, AY, GÜN, SAAT, DAKIKA, SANIYE);
  // Rtc.SetDateTime(compiled);
  
  // Örnek: 28 Eylül 2025, 15:30:00
  // RtcDateTime compiled = RtcDateTime(2025, 9, 28, 15, 30, 0);
  // Rtc.SetDateTime(compiled);
}
```
# KULLANIM KILAVUZU

Bu cihazın/yazılımın temel işlevleri ve kullanım adımları aşağıda özetlenmiştir.

## Hafıza (Kanal) İşlemleri

| İşlem | Adım |
| :--- | :--- |
| **Kanal Kaydetme** | Ana ekranda istenen frekansa gidin. **Encoder butonunu 3 saniye basılı tutun.** |
| **Kaydedilen Kanala Gitme** | Hafıza ekranında Encoder ile kanalı seçin ve **Encoder butonuna kısa basın.** |
| **Kanal Gezinme** | Hafıza ekranındayken **Encoder'ı çevirin.** |
| **Tüm Hafızayı Silme** | Hafıza ekranındayken **Encoder butonunu 3 saniye basılı tutun.** |

---

## Ekran Geçişleri ve Kontrol

Cihaz üç temel ekrandan oluşur.

| Ekran | Geçiş/Kontrol | Açıklama |
| :--- | :--- | :--- |
| **Ekranlar Arası Geçiş** | **Encoder butonuna kısa basın.** | Geçiş sırası: **Ana $\rightarrow$ Detay $\rightarrow$ Hafıza $\rightarrow$ Ana**. |
| **Ana Ekran (Frekans Ayarı)** | **Encoder'ı çevirin.** | Frekans ayarlama ve kanal kaydetme için kullanılır. |
| **Detay Ekran** | Bilgi amaçlıdır. | Sistem bilgilerini gösterir. |
| **Hafıza Ekranı** | Kanal yönetimi için kullanılır. | Kayıtlı kanalları yönetmenizi sağlar. |

---

# v2.0'dan v2.1'e Yükseltme Notları

Bu sürüm (v2.1), önemli iyileştirmeler ve değişiklikler içermektedir:

* **Encoder Sınırları (Boundaries) Sistemi:** Encoder'ın yanlışlıkla aşırı değerlere çıkmasını engelleyen sistem eklendi.
* **Uzun Basma Süresi Değişikliği:** Uzun basma algılama süresi 5 saniyeden **3 saniyeye düşürüldü**.
* **Kullanıcı Arayüzü (UI) Optimizasyonu:** Kullanıcı deneyimini iyileştiren UI düzenlemeleri yapıldı.
* **Hafıza Navigasyonu:** Kanal seçimi ve yönetimi için hafıza ekranı arayüzü **tamamen yenilendi**.
* **Buton Kontrolü:** Daha stabil ve güvenilir bir buton (encoder) kontrol mekanizması sağlandı.

---

# SORUN GİDERME (Troubleshooting)

Karşılaşabileceğiniz yaygın sorunlar ve çözüm önerileri:

## Encoder Çalışmıyor

1.  **Pin Bağlantılarını kontrol edin:** CLK:32, DT:33, SW:25.
2.  **Pull-up dirençlerinin** doğru şekilde bağlı olduğundan emin olun.

## Kanal Kaydedilmiyor

1.  **Ana ekranda** olduğunuzdan emin olun.
2.  **3 saniye** süreyle tam olarak basılı tutun (uzun basma gereklidir).
3.  Sorun devam ederse, `Serial Monitor` üzerinden **hata ayıklama (debug)** mesajlarını kontrol edin.

## RTC Yanlış Saat Gösteriyor

1.  DS1302 **RTC (Gerçek Zaman Saati)** pil bağlantısını kontrol edin.
2.  Kod içindeki tarih/saat ayarlama satırlarını **aktif** edin ve zaman ayarını yeniden yapın.
