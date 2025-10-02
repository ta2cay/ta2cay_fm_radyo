# TA2CAY_FM_RADYO (v3.0)

**ESP32 ve TEA5767 temelli, ST7735 TFT ekranlı, gelişmiş hafıza yönetimine ve hava durumu entegrasyonuna sahip FM radyo projesi.**

Bu proje, **ESP32** mikrodenetleyici ve **TEA5767 FM** radyo modülünü birleştirerek, döner enkoder (Rotary Encoder) ile kontrol edilen, saat, tarih ve DHT11 sensörü üzerinden sıcaklık/nem bilgisi gösterebilen kapsamlı bir masaüstü radyo sistemi sunar.

---

## 🚀 Ana Özellikler (v3.0)

| Kategori | Özellik | Açıklama |
| :--- | :--- | :--- |
| **Kontrol** | **Stabil Encoder Sistemi** | Frekans ayarı, menü navigasyonu ve tüm işlemler için %100 güvenilir enkoder/buton kontrolü. |
| **Hafıza** | **Gelişmiş Hafıza Yönetimi** | **10 kanala** (0-9) kadar frekans kaydı, kaydetme, çağırma ve toplu silme işlevleri. |
| **Ekran** | **Optimize Edilmiş UI** | Frekans, İstasyon Adı, Saat ve Hava Durumu bilgileri temiz ve kullanıcı dostu arayüzde bir arada. |
| **Bilgi** | **Detaylı Sistem Bilgisi** | CPU Frekansı (MHz), Boş RAM (kB) ve Cihaz Açık Kalma Süresi (Uptime) göstergeleri. |
| **Ortam** | **Sıcaklık/Nem** | DHT11 sensörü ile anlık sıcaklık ve nem takibi. |
| **Ek** | **Canlı Spektrum** | Radyo sinyaline göre dinamik olarak değişen görsel spektrum/VU metre. |

---

## 🆕 Yenilikler ve Değişiklikler (v3.0)

v2.1 sürümünden bu yana yapılan en önemli güncellemeler ve değişiklikler:

### 1️⃣ Gelişmiş Hafıza Sistemi
* **Kanal Sayısı Artışı:** Hafıza 8 kanaldan **10 kanala** çıkarıldı.
* **Ana Ekrandan Kayıt:** Ana ekranda **3 saniye basma** ile mevcut frekansı seçili hafıza slotuna kaydetme.
* **Akıllı Hafıza Ekranı:** Hafıza ekranında kanallar arası gezinme, seçili kanala kısa basma ile anında geçiş ve uzun basma ile tüm hafızayı temizleme.
* **Görsel Geri Bildirim:** Seçili kanal **yeşil arka plan** ile vurgulanır.

### 2️⃣ UI/UX İyileştirmeleri
* **Stereo Göstergesi Kaldırıldı:** TEA5767 kütüphanesi uyumsuzluğu nedeniyle **Stereo/Sinyal göstergesi** tamamen kaldırıldı. (Derleme hatası çözüldü)
* **Uptime Formatı Basitleştirildi:** Cihazın açık kalma süresi daha anlaşılır bir formatta gösterilir. (Örn: `1 Saat 05 Dk 30 Sn`).

### 3️⃣ Gelişmiş Encoder Kontrolü
* **Sınır Sistemi (Boundaries):** Her ekran modu için (Ana Ekran: 875-1080, Hafıza Ekranı: 0-11) enkoder değerleri otomatik ayarlanır.
* **Uzun Basma Süresi:** Uzun basma algılama süresi **3 saniyeye** düşürüldü (Daha hızlı işlem onayı).
* **Hassas Frekans Ayarı:** Ana ekranda frekans kontrolü $87.5 - 108.0 \text{ MHz}$ aralığında çalışır.

### 4️⃣ Ekran Modları ve Navigasyon
Proje, temel olarak 3 ana ekran modu kullanır:

| Ekran Modu | İçerik | Ana İşlev |
| :--- | :--- | :--- |
| **Ana Ekran** | Frekans, İstasyon Adı, Saat/Tarih, Sıcaklık/Nem, Spektrum | Frekans Ayarı, Kayıt (Uzun Basma) |
| **Hafıza Ekranı** | Kayıtlı Kanallar Listesi, Hafıza Yönetimi | Kanal Seçimi, Kayıtlı Kanala Gitme, Toplu Silme |
| **Onay Ekranları** | Kayıt Onayı, Silme Onayı, Çıkış Onayı | Kullanıcı onayı alır (EVET/HAYIR) |

---

## 📋 Kontrol Şeması

| Kontrol | Ana Ekran | Hafıza Ekranı | Onay Ekranları |
| :--- | :--- | :--- | :--- |
| **Encoder Çevirme** | Frekans Ayarlama | Kanallar Arası Gezinme | EVET/HAYIR Seçimi |
| **Kısa Basma** | Hafıza Ekranına Geç | **Seçili Kanala Git** | Seçimi Onayla (EVET/HAYIR) |
| **3 Sn. Basma (Uzun Basma)** | Mevcut Frekansı **Kaydet** | **Tüm Hafızayı Sil** | - |

---

## 🔧 Donanım Gereksinimleri

| Bileşen | Pinler | Açıklama |
| :--- | :--- | :--- |
| **ESP32** | - | Ana Mikrokontrolör |
| **ST7735 TFT** | `CS: 5`, `DC: 17`, `RST: 16` | 1.8" Renkli Ekran |
| **TEA5767 FM** | `SDA: 21`, `SCL: 22` | FM Radyo Modülü (I2C) |
| **DHT11** | `Data: 4` | Sıcaklık & Nem Sensörü |
| **DS1302 RTC**| `DAT: 27`, `CLK: 26`, `RST: 14` | Gerçek Zamanlı Saat |
| **Rotary Encoder**| `CLK: 32`, `DT: 33`, `SW: 25` | Frekans ve Menü Kontrolü |

---

## 📚 Gerekli Kütüphaneler

Bu projenin derlenebilmesi ve çalışabilmesi için Arduino IDE'ye aşağıdaki kütüphanelerin eklenmesi gerekmektedir:

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
