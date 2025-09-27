TA2CAY FM Radyo & DHT11 Hava Durumu İstasyonu v2.0

Bu sürüm ile proje ciddi geliştirmeler ve yeni özellikler kazandı.

Yenilikler v2.0

Hafıza Kanalları:

8 favori frekans kaydı.

Uzun basma (5 saniye) ile mevcut frekansı hafızaya ekleme.

Detay Ekranı:

Min/Max sıcaklık ve nem değerleri.

Sistem bilgisi: RAM, CPU frekansı ve uptime.

Gelişmiş Spektrum Animasyonu:

Frekans ve sinyal gücü görselleştirme.

Gece Modu:

22:00 – 07:00 arası kırmızı tema ve renk animasyonu.

Sıcaklık ve Nem Trend Göstergesi:

Basit + / - / = ile anlık trend bilgisi.

Splash Screen:

Yükleme animasyonu ile kullanıcı dostu açılış.



## RTC Saat ve Tarih Kurulumu

Proje **DS1302 RTC modülü** ile çalışır ve **ilk kurulumda saat ve tarih koddan ayarlanmalıdır**. Aksi hâlde ekranda yanlış saat/gün gösterilir.

### Kod Örneği (setup içinde)

```cpp
#include <RtcDS1302.h>
ThreeWire myWire(27, 26, 14); // DAT, CLK, RST
RtcDS1302<ThreeWire> Rtc(myWire);

void setup() {
  Rtc.Begin();
  // İlk kurulumda aşağıdaki satırı kullanın:
  // RtcDateTime compiled = RtcDateTime(YIL, AY, GÜN, SAAT, DAKIKA, SANIYE);
  // Rtc.SetDateTime(compiled);

  // Örnek: 27 Eylül 2025, 20:00:00
  // RtcDateTime compiled = RtcDateTime(2025, 9, 27, 20, 0, 0);
  // Rtc.SetDateTime(compiled);
}
