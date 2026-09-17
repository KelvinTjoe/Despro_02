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

// ---- Identitas node ----
// NODE_ID tidak didefinisikan di sini, tapi di masing-masing file node,
// supaya satu config.h bisa dipakai ketiganya tanpa saling menimpa.
#define ID_COMMAND      0
#define ID_FIELD        1
#define ID_RELAY        2

// Whitelist: paket dari sourceID di luar daftar ini diabaikan,
// untuk menolak gangguan dari radio 433 MHz lain di sekitar.
#define NODE_DIKENAL    { ID_COMMAND, ID_FIELD, ID_RELAY }

// ---- Penanda mode pengujian ----
// 1 = paket yang tidak datang lewat relay DITOLAK, memaksa seluruh
//     trafik menempuh 2 hop walaupun ketiga node berdekatan.
//     Dipakai untuk uji jarak dekat hari ini.
// 0 = operasi normal, paket langsung ikut diterima.
#define PAKSA_LEWAT_RELAY  1

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
