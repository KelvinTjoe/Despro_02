# Catatan Teknis — Despro2 SAR LoRa Mesh

## Daftar Isi

- [UART / Serial](#uart--serial)
- [Perhitungan Jangkauan (Link Budget)](#perhitungan-jangkauan-link-budget)
- [Efek Perubahan Setiap Parameter](#efek-perubahan-setiap-parameter)
- [Ringkasan Prioritas](#ringkasan-prioritas)

---

## UART / Serial

```cpp
Serial2.begin(baudrate, config, rxPin, txPin);
```

| Parameter | Keterangan |
|---|---|
| `baudrate` | 9600 untuk NEO-6M (default pabrik) |
| `config` | Format data, umumnya `SERIAL_8N1` |
| `rxPin` | Pin ESP32 penerima, ke **TX** modul GPS |
| `txPin` | Pin ESP32 pengirim, ke **RX** modul GPS |

**`SERIAL_8N1`** = 8 bit data per karakter, **N**o parity, **1** stop bit.

Format standar untuk koneksi UART sederhana. NEO-6M mengirim kalimat NMEA (`$GPGGA`, `$GPRMC`) dengan konfigurasi ini secara default dari pabrik — jadi ini bukan pilihan sembarang, tapi memang harus cocok dengan cara modul mengirim data. Parity tidak diperlukan karena NMEA sudah punya checksum sendiri di akhir tiap kalimat (`*XX`).

---

## Perhitungan Jangkauan (Link Budget)

**Parameter acuan:** 433 MHz, SF9, BW 125 kHz, CR 4/5, TX 17 dBm

### Langkah 1 — Sensitivitas penerima

Titik awal perhitungan, menentukan seberapa lemah sinyal yang masih terbaca.

```
S = -174 + 10·log10(BW) + NF + SNR_min
S = -174 + 10·log10(125000) + 6 + (-12,5)
S = -174 + 50,97 + 6 - 12,5
S = -129,5 dBm
```

| Suku | Nilai | Asal |
|---|---|---|
| `-174 dBm/Hz` | tetap | Noise floor termal pada suhu ruang |
| `10·log10(BW)` | 50,97 dB | Semakin lebar bandwidth, semakin banyak noise masuk |
| `NF` | 6 dB | Noise figure khas SX1278 |
| `SNR_min` | -12,5 dB | Untuk SF9 — nilai negatif inilah keunggulan LoRa, bisa bekerja di bawah noise floor |

Datasheet SX1278 untuk SF9/BW125 menyebut **-129 dBm**, cocok dengan hitungan di atas.

### Langkah 2 — Link budget

```
Budget = P_tx + G_tx + G_rx - L_kabel - S
Budget = 17 + 2 + 2 - 1 + 129
Budget = 149 dB
```

Gain antena (`G`) adalah asumsi paling rapuh di sini:

| Jenis antena | `G_tx + G_rx` | Budget |
|---|---|---|
| Rubber duck 2 dBi | +4 dB | **149 dB** |
| Spring/helical 0 dBi | 0 dB | 145 dB |
| Spring buruk -3 dBi | -6 dB | 139 dB |

> Antena spring kaku yang biasa ikut modul SX1278 murah realistisnya ada di baris kedua atau ketiga, bukan pertama.

### Langkah 3 — Jangan pakai Free Space Path Loss

```
FSPL = 20·log10(d_km) + 20·log10(f_MHz) + 32,44
FSPL = 20·log10(d_km) + 52,73 + 32,44

Kalau budget 149 dB dimasukkan:
20·log10(d_km) = 149 - 85,17 = 63,83
d = 1553 km        <-- TIDAK ADA ARTINYA
```

FSPL berlaku untuk dua antena di ruang hampa tanpa tanah, bukan untuk personel yang memegang radio 1,5 meter di atas permukaan.

**Alasan fisiknya bisa dihitung.** Zona Fresnel pertama harus bebas hambatan minimal 60%:

```
r = 17,32 · sqrt(d_km / (4 · f_GHz))
r = 17,32 · sqrt(5 / (4 · 0,433))
r = 29,4 m        (pada jarak 5 km)
```

Dibutuhkan 29 meter ruang bebas di titik tengah lintasan, sementara antena hanya 1,5–3 meter di atas tanah. Tanahnya sendiri sudah memotong zona Fresnel, sehingga kondisi free space tidak pernah tercapai.

### Langkah 4 — Model Two-Ray Ground Reflection

Model yang tepat untuk antena rendah. Cek jarak breakpoint terlebih dahulu:

```
λ            = 300 / 433 = 0,693 m
d_breakpoint = 4π · h1 · h2 / λ
             = 4π · 1,5 · 3 / 0,693
             = 82 m
```

Lewat 82 meter, path loss naik **40 dB per dekade**, bukan 20 dB seperti free space:

```
PL = 40·log10(d_m) - 20·log10(h1) - 20·log10(h2)
PL = 40·log10(d_m) - 3,52 - 9,54
PL = 40·log10(d_m) - 13,06
```

dengan `h1 = 1,5 m` (personel) dan `h2 = 3 m` (posko).

### Langkah 5 — Hitung jarak

Fade margin wajib disisihkan untuk fluktuasi sinyal.

```
Path loss tersedia = 149 - 15 = 134 dB
40·log10(d) = 134 + 13,06 = 147,06
log10(d)    = 3,677
d           = 4753 m  (~4,8 km)
```

| Fade margin | Path loss tersedia | Jarak |
|---|---|---|
| 10 dB | 139 dB | 6,3 km |
| **15 dB** | **134 dB** | **4,8 km** |
| 20 dB | 129 dB | 3,6 km |

### Langkah 6 — Batas kelengkungan bumi

Berapapun budget-nya, geometri membatasi:

```
d_horizon = 4,12 · (sqrt(h1) + sqrt(h2))        [h dalam meter]
d_horizon = 4,12 · (1,22 + 1,73)
d_horizon = 12,2 km
```

Plafon mutlak untuk ketinggian antena tersebut, sebelum memperhitungkan medan.

### Langkah 7 — Koreksi vegetasi

Redaman dedaunan pada UHF sekitar **0,05–0,15 dB per meter** kedalaman vegetasi (ITU-R P.833). Untuk kanopi lebat, tambahan 20–40 dB realistis.

```
Path loss tersedia = 149 - 15 - 30 = 104 dB
40·log10(d) = 104 + 13,06 = 117,06
d = 845 m
```

### Langkah 8 — Time on Air

```
T_sym      = 2^SF / BW = 512 / 125000 = 4,096 ms
T_preamble = (8 + 4,25) · T_sym = 50,2 ms

n_payload = 8 + ceil((8·PL - 4·SF + 28 + 16·CRC) / (4·(SF-DE))) · (CR+4)
          = 8 + ceil((8·34 - 36 + 28 + 16) / 36) · 5
          = 8 + ceil(280/36) · 5
          = 8 + 40 = 48 simbol

T_payload  = 48 · 4,096 = 196,6 ms
```

```
Time on Air = 50,2 + 196,6 = 247 ms
Bitrate     = SF · (BW / 2^SF) · (4/(4+CR))
            = 9 · 244,14 · 0,8 = 1758 bps
```

### Ringkasan hasil

| Kondisi | Perkiraan jarak |
|---|---|
| LOS area terbuka | 3–5 km |
| Perkampungan / NLOS ringan | 1–2 km |
| Kanopi hutan lebat | **0,3–1 km** |

> **Aturan praktis:** di rezim 40 dB/dekade, setiap perubahan `X` dB pada budget mengubah jarak dengan faktor `10^(X/40)`.
> Jadi +6 dB budget = **1,41x** jarak, bukan 2x.

**Implikasi desain:** dengan ~800 m per hop di bawah kanopi, target `MAX_HOP=3` hanya mencakup radius sekitar 2,4 km. Ini memperkuat argumen bahwa 1 relay node tidak cukup.

---

## Efek Perubahan Setiap Parameter

### Spreading Factor (SF)

**Sekarang:** SF9 — sensitivitas -129 dBm, ToA 247 ms

| SF | Sensitivitas | Selisih vs SF9 | Faktor jarak | Time on Air |
|---|---|---|---|---|
| SF7 | -123 dBm | -6 dB | 0,71x | ~60 ms |
| SF8 | -126 dBm | -3 dB | 0,84x | ~120 ms |
| **SF9** | **-129 dBm** | **acuan** | **1,00x** | **247 ms** |
| SF10 | -132 dBm | +3 dB | 1,19x | ~450 ms |
| SF11 | -134,5 dBm | +5,5 dB | 1,37x | ~900 ms |
| SF12 | -137 dBm | +8 dB | 1,58x | ~1500 ms |

**Naik:**
- Sensitivitas dan jangkauan lebih baik
- Time on air melonjak — SF12 sekitar 1,5 detik per paket
- Peluang tabrakan paket naik drastis (belum ada CSMA/backoff)
- Baterai lebih cepat habis karena radio menyala lebih lama

**Turun:**
- Time on air jauh lebih pendek, kanal lebih lega
- Baterai lebih hemat
- Jangkauan berkurang

> SF7 adalah default library LoRa kalau `setSpreadingFactor()` tidak pernah dipanggil. Kode P2P lama berjalan di SF7 — board yang belum memakai `config.h` tidak akan bisa menerima paket dari field node yang kini di SF9.

### Bandwidth (BW)

**Sekarang:** 125 kHz

| BW | Sensitivitas | Faktor jarak | Time on Air |
|---|---|---|---|
| 62,5 kHz | -132 dBm | 1,19x | 2x lebih lama |
| **125 kHz** | **-129 dBm** | **1,00x** | **acuan** |
| 250 kHz | -126 dBm | 0,84x | 2x lebih cepat |
| 500 kHz | -123 dBm | 0,71x | 4x lebih cepat |

Setiap penggandaan BW menurunkan sensitivitas 3 dB, karena suku `10·log10(BW)` di rumus sensitivitas. Menurunkan BW ke 62,5 kHz menaikkan jangkauan tapi membuat sistem lebih sensitif terhadap pergeseran frekuensi kristal.

### Coding Rate (CR)

**Sekarang:** 4/5 (paling rendah, tidak bisa diturunkan lagi)

**Naik ke 4/6, 4/7, 4/8:**
- Koreksi error lebih kuat, lebih tahan interferensi
- Payload overhead naik, time on air bertambah
- Efek ke jangkauan kecil (~1–2 dB) — tidak sebanding dengan tambahan waktu udaranya

### TX Power

**Sekarang:** 17 dBm — **melebihi batas legal 12,15 dBm EIRP** (Permen Komdigi No. 2/2025)

| TX Power | Selisih | Faktor jarak | Jarak (LOS) |
|---|---|---|---|
| 12 dBm | -5 dB | 0,75x | 3,6 km |
| **17 dBm** | **acuan** | **1,00x** | **4,8 km** |
| 20 dBm | +3 dB | 1,19x | 5,7 km |

**Turun ke 12 dBm** untuk kepatuhan: kehilangan jangkauan 25%, tapi baterai lebih hemat dan PA lebih dingin. Penalti relatif ringan dibanding manfaat kepatuhan regulasi.

**Naik ke 20 dBm** (batas SX1278): hanya menambah 19% jarak, perlu duty cycle terbatas, PA bisa panas, dan semakin jauh dari kepatuhan.

### Gain Antena

**Pengungkit terbesar — bukan TX power.**

| Antena | Budget | Jarak (LOS) |
|---|---|---|
| Rubber duck 2 dBi | 149 dB | 4,8 km |
| Spring 0 dBi | 145 dB | 3,8 km |
| Spring buruk -3 dBi | 139 dB | 2,7 km |

Beda antena bagus versus buruk = 10 dB = **1,78x jarak**. Dampaknya lebih besar daripada mengubah TX dari 17 ke 12 dBm.

Antena directional (Yagi 7 dBi) di posko bisa menambah 5 dB tanpa menyalahi batas daya pancar, tapi hanya mencakup satu arah.

### Ketinggian Antena

**Pengungkit kedua terbesar, dan paling murah.**

| Konfigurasi | Path loss | Faktor jarak | Horizon |
|---|---|---|---|
| Posko 3 m | `40·log10(d) - 13,06` | 1,00x | 12,2 km |
| Posko 10 m | `40·log10(d) - 23,5` | 1,83x | 18,0 km |
| Posko di bukit 30 m | `40·log10(d) - 33,0` | 2,85x | 27,6 km |

```
Posko 10 m:
PL = 40·log10(d) - 20·log10(1,5) - 20·log10(10)
   = 40·log10(d) - 3,52 - 20,0
Budget efektif bertambah 10,5 dB = 1,83x jarak
Horizon = 4,12 · (1,22 + 3,16) = 18 km
```

> Menaikkan antena jauh lebih efektif daripada menaikkan daya pancar.

### Frekuensi

**Sekarang:** 433 MHz (sudah final)

Kalau pindah ke 915 MHz:
- FSPL naik 6,5 dB — dari `20·log10(915/433)`
- Penetrasi vegetasi lebih buruk
- Antena lebih kecil untuk gain yang sama

Untuk kanopi hutan, 433 MHz memang pilihan lebih baik.

### Panjang Payload

**Sekarang:** 34 byte CSV, ToA 247 ms

Tidak mempengaruhi jangkauan sama sekali, hanya time on air. Migrasi ke binary struct (~16 byte) memotong ToA jadi sekitar 170 ms, artinya peluang tabrakan paket turun sekitar 30%.

---

## Ringkasan Prioritas

Kalau jangkauan kurang, urutan yang paling berdampak:

| Prioritas | Tindakan | Perolehan | Biaya |
|---|---|---|---|
| 1 | Naikkan antena posko ke 10 m | 1,83x | Tiang, hampir gratis |
| 2 | Ganti antena ke rubber duck layak | 1,78x | Murah |
| 3 | Yagi directional di posko | 1,33x | Murah, satu arah saja |
| 4 | Naikkan SF9 ke SF11 | 1,37x | ToA 4x, rawan tabrakan |
| 5 | Naikkan TX 17 ke 20 dBm | 1,19x | Melanggar regulasi |

### Catatan tentang ketidakpastian

Angka-angka ini punya error bar besar. Model two-ray mengasumsikan tanah datar dengan refleksi sempurna, yang tidak ada di medan bencana. Perlakukan sebagai perkiraan orde besaran untuk perencanaan, lalu verifikasi dengan pengukuran RSSI lapangan.

Kode field node sudah mencetak RSSI setiap paket diterima, jadi hasil ukur bisa langsung dibandingkan dengan prediksi di dokumen ini.
