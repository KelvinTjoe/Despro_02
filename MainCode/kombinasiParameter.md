# Parameter LoRa 433 MHz Sesuai Aturan Indonesia

**Dasar hukum:** Permenkominfo No. 2 Tahun 2023 tentang Penggunaan Spektrum Frekuensi Radio Berdasarkan Izin Kelas, Lampiran II bagian LPWAN.

| Ketentuan | Nilai |
|---|---|
| Pita frekuensi | 433,05 – 434,79 MHz |
| Daya pancar maksimum (EIRP) | 16,4 mW (12,15 dBm) |
| Bandwidth maksimum per kanal | 125 kHz |
| Duty cycle maksimum | Tidak diatur |

---

## 1. Frekuensi Tengah

Seluruh kanal harus berada di dalam 433,05 – 434,79 MHz. Dengan BW 125 kHz, frekuensi tengah harus di antara **433,1125 dan 434,7275 MHz**.

Contoh kanal yang aman (jarak 200 kHz):

| Kanal | Frekuensi tengah |
|---|---|
| 1 | 433,175 MHz |
| 2 | 433,375 MHz |
| 3 | 433,575 MHz |
| 4 | 433,775 MHz |
| 5 | 433,975 MHz |
| 6 | 434,175 MHz |
| 7 | 434,375 MHz |
| 8 | 434,575 MHz |

> ⚠️ Banyak contoh kode memakai `LoRa.begin(433E6)`, yaitu 433,000 MHz. Ini **di luar pita** karena sinyalnya turun sampai 432,94 MHz.

---

## 2. Bandwidth

Maksimum 125 kHz.

| BW | Status |
|---|---|
| 7,8 / 10,4 / 15,6 / 20,8 / 31,25 / 41,7 / 62,5 / 125 kHz | ✅ Boleh |
| 250 / 500 kHz | ❌ Tidak boleh |

BW 31,25 kHz atau lebih kecil memerlukan modul dengan TCXO, karena kristal biasa pada SX1278 sering tidak stabil di BW sekecil itu.

---

## 3. Daya Pancar

Batasnya adalah **EIRP**, yaitu daya modul ditambah gain antena dikurangi rugi kabel:

```
Ptx_maks = 12,15 − Gain_antena + Rugi_kabel
```

| Gain antena | Setting Ptx maksimum |
|---|---|
| 0 dBi | 12 dBm |
| 2 dBi | 10 dBm |
| 3 dBi | 9 dBm |
| 5 dBi | 7 dBm |

> Hindari 14, 17, atau 20 dBm, karena itu melebihi batas meskipun modul SX1278 mampu.

---

## 4. SF, CR, dan Duty Cycle

| Parameter | Ketentuan |
|---|---|
| Spreading Factor | SF7 – SF12 bebas |
| Coding Rate | 4/5 – 4/8 bebas |
| Duty cycle | Tidak dibatasi (tetap jaga interval kirim yang wajar) |

---

## 5. Kombinasi SF dengan BW 125 kHz

**Asumsi:** EIRP 12,15 dBm, antena penerima 2 dBi, NF 6 dB, fade margin 10 dB, CR 4/5, n = 3 (suburban).

| SF | SNR_min | Sensitivitas | Bit rate | PL_max | Estimasi jarak |
|---|---|---|---|---|---|
| SF7 | −7,5 dB | −124,5 dBm | 5,47 kbps | 128,7 dB | ≈ 2,8 km |
| SF8 | −10 dB | −127 dBm | 3,13 kbps | 131,2 dB | ≈ 3,4 km |
| SF9 | −12,5 dB | −129,5 dBm | 1,76 kbps | 133,7 dB | ≈ 4,1 km |
| SF10 | −15 dB | −132 dBm | 977 bps | 136,2 dB | ≈ 5,0 km |
| SF11 | −17,5 dB | −134,5 dBm | 537 bps | 138,7 dB | ≈ 6,1 km |
| SF12 | −20 dB | −137 dBm | 293 bps | 141,2 dB | ≈ 7,3 km |

Menurunkan BW ke 62,5 kHz menambah sensitivitas sekitar 3 dB tetapi membagi dua bit rate.

---

## 6. Rumus untuk 433 MHz

**Sensitivitas:**
```
S (dBm) = −174 + 10·log10(BW) + NF + SNR_min
```

**Bit rate:**
```
Rb = SF × (BW / 2^SF) × 4 / (4 + CR)
```
CR = 1 untuk 4/5, 2 untuk 4/6, 3 untuk 4/7, 4 untuk 4/8.

**Path loss maksimum:**
```
PL_max = EIRP + Grx − S − Fade_margin
```

**Jarak (log-distance):**
```
d (meter) = 10^( (PL_max − 25,2) / (10·n) )
```

> Di 433 MHz, nilai PL(d0) pada 1 m adalah **≈ 25,2 dB**, bukan 31,7 dB seperti di 923 MHz.

| Lingkungan | n |
|---|---|
| Ruang terbuka, line-of-sight | 2 |
| Pedesaan / suburban | 2,7 – 3 |
| Perkotaan | 3 – 3,5 |
| Dalam gedung | 4 – 6 |

---

## 7. Kombinasi yang Disarankan untuk Pengujian 3 Modul

| Skenario | Frekuensi | BW | SF | CR | Ptx (antena 2 dBi) |
|---|---|---|---|---|---|
| Jarak dekat, cepat | 433,175 MHz | 125 kHz | SF7 | 4/5 | 10 dBm |
| Seimbang | 433,175 MHz | 125 kHz | SF9 | 4/5 | 10 dBm |
| Jarak jauh | 433,175 MHz | 125 kHz | SF12 | 4/8 | 10 dBm |

Contoh setting dengan library arduino-LoRa:

```cpp
LoRa.begin(433.175E6);
LoRa.setSignalBandwidth(125E3);
LoRa.setSpreadingFactor(9);
LoRa.setCodingRate4(5);
LoRa.setTxPower(10);   // dengan antena 2 dBi → EIRP 12 dBm
```

---

## 8. Catatan Lain

- Pasal 11 mewajibkan perangkat yang digunakan di Indonesia memenuhi standar teknis dan memiliki sertifikat.
- Penggunaan di luar ruangan dapat dikenai kewajiban registrasi (Pasal 17).
- Peraturan bisa diperbarui, jadi cek kembali situs Komdigi/SDPPI sebelum dipakai untuk keperluan resmi.
