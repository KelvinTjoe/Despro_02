#pragma once

// ============================================================
// Konfigurasi bersama untuk SEMUA node (field / relay / command).
// Ubah di sini saja, jangan salin-tempel ke tiap node.
// ============================================================

// ---- LoRa SX1278 (SPI) ----
#define LORA_SCK        18
#define LORA_MISO       19
#define LORA_MOSI       23
#define LORA_SS         5
#define LORA_RST        14
#define LORA_DIO0       26

#define LORA_FREQ       433E6

// Parameter radio. SEMUA node wajib memakai nilai yang sama persis,
// kalau beda satu saja paket tidak akan pernah terbaca.
#define LORA_SF         9
#define LORA_BW         125E3
#define LORA_CR         5

// 17 dBm melebihi batas legal EIRP 433 MHz di Indonesia (12,15 dBm,
// Permen Komdigi No. 2/2025). Turunkan sebelum pengujian di luar lab.
#define LORA_TX_POWER   17

// ---- GPS NEO-6M (UART2) ----
#define GPS_RX          16
#define GPS_TX          17
#define GPS_BAUD        9600

// ---- OLED DISPLAY SSD1306 (I2C, belum dipakai) ----
#define OLED_SDA        21
#define OLED_SCL        22

// ---- Tombol field node ----
#define BUTTON_AMAN     32
#define BUTTON_BANTUAN  33
#define BUTTON_SIAGA    25

// Tombol untuk sementara DINONAKTIFKAN: pemicu kirim sekarang murni
// periodik. Seluruh kode tombol masih utuh di LoraFieldNode.cpp,
// dipagari #if TOMBOL_AKTIF, jadi cukup ubah ke 1 untuk menghidupkannya
// kembali. Saat aktif, tiap tombol mengirim namanya sebagai teks biasa
// karena enum StatusPersonel sudah tidak ada di paket.
#define TOMBOL_AKTIF    0

// ---- Identitas node ----
// NODE_ID tidak didefinisikan di sini, tapi di masing-masing file node,
// supaya satu config.h bisa dipakai ketiganya tanpa saling menimpa.
#define ID_COMMAND      0
#define ID_FIELD        1
#define ID_RELAY        2

// ---- Pemilihan jalur ----
// Menentukan paket mana yang boleh diproses penerima. Penandanya ada di
// dalam paket itu sendiri: lastHopID == sourceID berarti paket datang
// langsung dari pembuatnya dan belum disentuh siapa pun, sedangkan
// lastHopID != sourceID berarti sudah diteruskan relay.
//
//   JALUR_LANGSUNG    = hanya paket langsung. Untuk uji dua node;
//                       hasil terusan relay dibuang.
//   JALUR_BEBAS       = operasi normal. Paket langsung maupun via relay
//                       sama-sama diterima, mana saja yang sampai duluan.
//   JALUR_WAJIB_RELAY = kebalikan JALUR_LANGSUNG. Paket langsung dibuang,
//                       hanya yang sudah lewat relay yang diproses, jadi
//                       field dan command tidak bisa bicara berduaan.
//                       Untuk membuktikan topologi tiga node benar-benar
//                       jalan. Kalau relay mati, komunikasi berhenti
//                       total walau kedua node bersebelahan.
#define JALUR_LANGSUNG     0
#define JALUR_BEBAS        1
#define JALUR_WAJIB_RELAY  2

#define MODE_JALUR      JALUR_WAJIB_RELAY

// Whitelist: paket dari sourceID di luar daftar ini diabaikan,
// untuk menolak gangguan dari radio 433 MHz lain di sekitar.
// Yang disaring di sini PEMBUAT paket, bukan penerusnya, dan relay tidak
// pernah menjadi sourceID karena ia hanya mengubah lastHopID.
#if MODE_JALUR == JALUR_LANGSUNG
  #define NODE_DIKENAL  { ID_COMMAND, ID_FIELD }
#else
  #define NODE_DIKENAL  { ID_COMMAND, ID_FIELD, ID_RELAY }
#endif

// ---- Anti-duplikasi ----
// Tiap paket disiarkan ULANG_KIRIM kali dengan seq yang sama. Penerima
// hanya memproses yang pertama; sisanya harus tercatat DUPLIKAT di
// Serial. Itulah cara memastikan tabel dedup bekerja. Set 1 untuk
// kembali ke sekali kirim.
#define ULANG_KIRIM     2
// Jeda antar siaran ulang, harus lebih panjang dari time-on-air satu
// paket (sekitar 247 ms pada SF9) supaya penerima sempat membaca FIFO.
#define JEDA_ULANG_MS   300
// Jumlah (sourceID, seq) terakhir yang diingat, dan berapa lama.
// Setelah kedaluwarsa entri dilepas, sehingga node yang reboot dan
// mengulang seq tidak dianggap duplikat selamanya.
#define DEDUP_UKURAN    16
#define DEDUP_UMUR_MS   60000UL

// ---- Siaran otomatis field node ----
// Pemicu kirim field node saat ini: periodik tiap AUTO_KIRIM_MS, tanpa
// campur tangan tombol. Teks yang dibawa berisi nomor siaran, jumlah
// satelit, dan status fix GPS, supaya paket yang hilang di penerima
// langsung kelihatan dari lompatan nomornya.
#define AUTO_KIRIM      1
#define AUTO_KIRIM_MS   10000UL

// ---- Parameter waktu (ms) ----
#define HOLD_DURATION   2000UL
#define DEBOUNCE_MS     50UL
#define GPS_STALE_AGE   60000UL
#define GPS_REPORT_MS   5000UL

// Jeda acak sebelum relay meneruskan, mencegah tabrakan di udara.
// Time-on-air satu paket sekitar 247 ms pada SF9.
#define BACKOFF_MIN_MS  20
#define BACKOFF_MAX_MS  200

// Batas panjang teks yang boleh diketik operator di command node.
#define PESAN_MAX_CHAR  180
