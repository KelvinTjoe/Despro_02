# Data logger Despro2

Mencatat setiap paket LoRa yang diterima Command Node ke file CSV di folder
yang disinkronkan Google Drive for desktop.

```
Field / Relay Node memancar
  -> Command Node (ESP32) menerima, menyaring, mencetak baris "DATA,..." ke USB serial
  -> logger.py menambahkan waktu laptop, menulis ke CSV
  -> Google Drive for desktop meng-upload file itu sendiri
```

Tidak ada sensor di sistem ini. Pencatatan bersifat event-driven: satu baris
per paket yang lolos saring, bukan per interval waktu.

## Kenapa tetap butuh script di laptop

ESP32 tidak bisa menulis file ke folder laptop. `SD.h` dan `LittleFS` hanya
menulis ke penyimpanan milik board sendiri (kartu microSD atau flash internal).
USB serial adalah aliran byte, bukan akses filesystem, jadi ESP32 tidak punya
cara memberi tahu Windows "buat file di folder ini". Pihak yang membuat file
harus program di sisi laptop, dan itulah tugas `logger.py`.

## Instalasi

Butuh Python 3 dan satu paket:

```powershell
python -m pip install -r tools\requirements.txt
```

## Menjalankan

```powershell
python tools\logger.py
```

Pilih port lain (nomor COM di Windows berpindah antar board):

```powershell
python tools\logger.py --port COM7
```

Lihat port yang terdeteksi lalu keluar:

```powershell
python tools\logger.py --daftar-port
```

Opsi lain: `--baud`, `--folder`. Berhenti dengan `Ctrl+C`; file ditutup
dengan benar dan ringkasan jumlah baris ditampilkan.

## PENTING: tutup serial monitor PlatformIO dulu

Satu port serial tidak bisa dipakai dua program sekaligus. Kalau
`pio device monitor` masih jalan, `logger.py` akan gagal dengan pesan
`Access is denied` di COM9, dan sebaliknya.

Selama logging, pantau lewat output konsol `logger.py` — script menampilkan
semua baris dari Command Node, termasuk pesan diagnostik, jadi serial monitor
PlatformIO tidak diperlukan lagi.

## Hasil

Satu file per hari di
`C:\Users\ruben\Documents\Despro 2\DatasetDespro2`:

```
data_2026-10-01.csv
```

Isinya:

```
# PARAMETER: freq=433000000Hz sf=9 bw=125000Hz cr=4/5 tx=17dBm spi=2000000Hz crc=on jalur=WAJIB_RELAY ulang=2 jeda=300ms dedup=16/60000ms node=0
waktu_laptop,sourceID,lastHopID,seq,lat,lon,rssi,snr,teks
2026-10-01 14:22:07.431,1,2,17,-6.365432,106.824512,-97,8.25,"siaran #17, 7 satelit, FIX"
```

Kolom:

| kolom | asal | keterangan |
|---|---|---|
| `waktu_laptop` | laptop | waktu baris diterima script, presisi milidetik |
| `sourceID` | paket | node yang MEMBUAT paket, tidak pernah berubah |
| `lastHopID` | paket | node yang TERAKHIR menyiarkan; `lastHopID != sourceID` berarti lewat relay |
| `seq` | paket | nomor urut dari pembuat; seq yang bolong = paket hilang |
| `lat`, `lon` | paket | posisi pengirim, `0.000000` kalau belum fix GPS |
| `rssi` | radio | `LoRa.packetRssi()` di Command Node, dBm |
| `snr` | radio | `LoRa.packetSnr()` di Command Node, dB |
| `teks` | paket | isi pesan; boleh mengandung koma, di-quote oleh modul `csv` |

### Baris `# PARAMETER`

Baris pertama tiap file mencatat parameter radio dari `config.h` yang berlaku
saat pengujian itu. Tanpa itu, nilai RSSI dan SNR tidak bisa dibandingkan
antar sesi, karena ganti `LORA_SF` atau `LORA_TX_POWER` saja sudah mengubah
artinya.

Firmware mengulang baris `META` tiap 60 detik, bukan hanya sekali saat boot,
supaya logger yang dijalankan setelah board menyala tetap kebagian. Kalau file
dibuat sebelum baris `META` pertama datang, baris parameter berisi penjelasan
sementara dan parameter sesungguhnya ditambahkan sebagai komentar kedua begitu
diterima.

Membaca dengan pandas:

```python
import pandas as pd
df = pd.read_csv("data_2026-10-01.csv", comment="#")
```

## Baris mana yang masuk file

Hanya baris berawalan `DATA,`, yaitu paket yang lolos `saringPaket()` di
Command Node.

TIDAK dicatat, hanya tampil di konsol: `RUSAK`, `ASING`, `GEMA`, `DUPLIKAT`,
`VIA RELAY`, `LANGSUNG`, `DITERIMA`, `POSISI`, `PESAN`, `TERKIRIM`, dan pesan
init LoRa. Baris kosong, baris sampah, baris `DATA` dengan jumlah kolom salah,
dan baris `DATA` yang kolom angkanya bukan angka juga diabaikan serta dihitung
di ringkasan saat keluar.

## Kalau CSV tetap kosong

Periksa `MODE_JALUR` di `MainCode/config.h`. Saat ini nilainya
`JALUR_WAJIB_RELAY`: Command Node **menolak** semua paket langsung dari Field
Node dan hanya menerima yang sudah diteruskan Relay Node. Jadi kalau Relay
Node belum dinyalakan, konsol hanya memperlihatkan baris `LANGSUNG` dan file
CSV tidak akan terisi. Untuk uji dua node saja, ubah ke `JALUR_LANGSUNG` atau
`JALUR_BEBAS` lalu flash ulang.

## Firmware yang dibutuhkan

Baris `DATA,` dan `META,` dicetak oleh `MainCode/LoraCommandNode.cpp`, env
`command` di `platformio.ini`. Flash dulu sebelum logger dipakai:

```powershell
pio run -e command -t upload
```
