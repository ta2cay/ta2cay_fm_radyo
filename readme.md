# TA2CAY FM Radyo & DHT11 Hava Durumu İstasyonu v2.0

Bu sürüm ile birlikte proje ciddi geliştirmeler ve yeni özellikler kazandı.  

## Yenilikler v2.0

- **Hafıza Kanalları:**  
  - 8 favori frekans kaydı.  
  - Uzun basma (5 saniye) ile mevcut frekansı hafızaya ekleme.  

- **Detay Ekranı:**  
  - Min/Max sıcaklık ve nem değerleri.  
  - Sistem bilgisi: RAM, CPU frekansı ve uptime.  

- **Gelişmiş Spektrum Animasyonu:**  
  - Frekans ve sinyal gücü görselleştirme.  

- **Gece Modu:**  
  - 22:00 – 07:00 arası kırmızı tema ve renk animasyonu.  

- **Sıcaklık ve Nem Trend Göstergesi:**  
  - Basit + / - / = ile anlık trend bilgisi.  

- **Splash Screen:**  
  - Yükleme animasyonu ile kullanıcı dostu açılış.  

- **RTC Saat ve Tarih:**  
  - DS1302 modülü ile güncel saat ve tarih gösterimi.  
  - **Önemli:** İlk kurulumda saat ve tarih kodlardan ayarlanmalıdır:
```cpp
// RtcDateTime compiled = RtcDateTime(YIL, AY, GÜN, SAAT, DAKIKA, SANIYE);
// Rtc.SetDateTime(compiled);
