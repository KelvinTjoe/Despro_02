#pragma once

#include <Arduino.h>
#include "config.h"

// ============================================================
// Paket UJI COBA. Hanya untuk memastikan jalur komunikasi
// field <-> command hidup. Tidak ada routing, tidak ada relay,
// tidak ada nomor urut, tidak ada anti-duplikasi.
//
// Format CSV:
//
//   sourceID,lat,lon,pesan
//
// sourceID = node yang mengirim paket
// lat, lon = koordinat GPS pengirim, 0.000000 kalau belum/tidak ada fix
// pesan    = teks bebas, DITARUH PALING AKHIR supaya boleh mengandung
//            koma tanpa merusak pemisahan field
//
// Contoh:
//   1,-6.365432,106.824512,field node aktif
//   0,0.000000,0.000000,segera kembali ke titik kumpul
//
// Nama tipe dan fungsi di sini sengaja diberi akhiran "Uji" supaya tidak
// bentrok dengan packet.h kalau suatu saat keduanya ikut ter-include.
// ============================================================

struct PaketUji {
  uint8_t sourceID;
  float   lat;
  float   lon;
  String  pesan;
};

// ---------- penyusun paket ----------

inline String buatPaketUji(uint8_t sourceID,
                           float lat,
                           float lon,
                           const String &pesan) {
  String p;
  p += String(sourceID);
  p += ',';
  p += String(lat, 6);
  p += ',';
  p += String(lon, 6);
  p += ',';
  p += pesan;
  return p;
}

// ---------- pembaca paket ----------

inline bool parsePaketUji(const String &raw, PaketUji &p) {
  int k1 = raw.indexOf(',');
  int k2 = raw.indexOf(',', k1 + 1);
  int k3 = raw.indexOf(',', k2 + 1);
  if (k1 < 0 || k2 < 0 || k3 < 0) {
    return false;
  }

  p.sourceID = (uint8_t)raw.substring(0, k1).toInt();
  p.lat      = raw.substring(k1 + 1, k2).toFloat();
  p.lon      = raw.substring(k2 + 1, k3).toFloat();
  p.pesan    = raw.substring(k3 + 1);  // sisa baris, koma ikut apa adanya

  return true;
}

// Whitelist dari config.h, menolak siaran radio 433 MHz lain di sekitar.
inline bool nodeDikenalUji(uint8_t id) {
  const uint8_t daftar[] = NODE_DIKENAL;
  for (uint8_t i = 0; i < sizeof(daftar); i++) {
    if (daftar[i] == id) {
      return true;
    }
  }
  return false;
}
