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
#define NODE_ID         1
#define ID_COMMAND      0

// ---- Parameter waktu (ms) ----
#define HOLD_DURATION   2000UL
#define DEBOUNCE_MS     50UL
#define GPS_STALE_AGE   60000UL
#define GPS_REPORT_MS   5000UL
