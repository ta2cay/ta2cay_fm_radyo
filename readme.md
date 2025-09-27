# TA2CAY FM Radyo & DHT11 Hava Durumu İstasyonu v2.0

Bu sürüm ile proje ciddi geliştirmeler ve yeni özellikler kazandı. Artık hem radyo keyfi hem de çevresel veriler çok daha zengin şekilde takip edilebiliyor.

---

## 🆕 Yenilikler v2.0

### 1️⃣ Hafıza Kanalları
- **8 favori frekans kaydı** desteği.
- **Uzun basma (5 saniye)** ile mevcut frekansı hafızaya ekleme.
- Kanal zaten kayıtlıysa veya hafıza doluysa uyarı gösterilir.

### 2️⃣ Detay Ekranı
- Son 24 saatteki **Min/Max sıcaklık ve nem değerleri**.
- **Sistem bilgisi:** RAM, CPU frekansı, uptime.
- Daha fazla bilgi için ekranlar arasında geçiş yapabilirsiniz.

### 3️⃣ Gelişmiş Spektrum Animasyonu
- **Frekans ve sinyal gücü görselleştirme.**
- Gece modu ve gündüz modu için renkler farklıdır.
- Daha görsel ve anlaşılır sinyal çubukları.

### 4️⃣ Gece Modu 🌙
- Saat **22:00 – 07:00** arası aktif.
- Kırmızı tema ve renk animasyonu ile göz yorgunluğunu azaltır.
- Gündüz moduna otomatik geçiş yapılır.

### 5️⃣ Sıcaklık ve Nem Trend Göstergesi
- Anlık trend göstergesi: `+` yükseliyor, `-` düşüyor, `=` sabit.
- Anlık değerler ve trend tek bakışta görülebilir.

### 6️⃣ Splash Screen
- **Yükleme animasyonu** ile cihaz açılışı daha kullanıcı dostu.
- Gökkuşağı renkleri ve basit animasyon ile projeye hoş bir giriş.


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
