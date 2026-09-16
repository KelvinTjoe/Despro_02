#pragma once

#include <Arduino.h>

// ============================================================
// Format paket CSV (tahap awal, nanti diganti binary struct):
//
//   sourceID,status,lat,lon,flag
//
// Contoh:
//   1,BANTUAN,-6.365432,106.824512,FIX
//   1,AMAN,-6.365432,106.824512,STALE
//   1,SIAGA,0.000000,0.000000,NOFIX
//
// Field mesh (packetID, lastHopID, hopCount) sengaja belum ada,
// ditambahkan saat masuk fase relay.
//
// Command node WAJIB membaca urutan field yang sama.
// ============================================================

enum StatusPersonel {
  STATUS_AMAN,
  STATUS_SIAGA,
  STATUS_BANTUAN
};

enum KualitasPosisi {
  POS_FIX,    // GPS baru saja memperbarui posisi
  POS_STALE,  // pernah dapat posisi, tapi sudah lama tidak diperbarui
  POS_NOFIX   // belum pernah dapat posisi sejak board menyala
};

// Fungsi di header harus `inline`, kalau tidak linker akan protes
// "multiple definition" begitu header ini di-include lebih dari satu .cpp.
inline const char* namaStatus(StatusPersonel s) {
  switch (s) {
    case STATUS_AMAN:    return "AMAN";
    case STATUS_SIAGA:   return "SIAGA";
    case STATUS_BANTUAN: return "BANTUAN";
  }
  return "TIDAKVALID";
}

inline const char* namaKualitas(KualitasPosisi k) {
  switch (k) {
    case POS_FIX:   return "FIX";
    case POS_STALE: return "STALE";
    case POS_NOFIX: return "NOFIX";
  }
  return "TIDAKVALID";
}

inline String buatPaket(uint8_t sourceID,
                        StatusPersonel status,
                        float lat,
                        float lon,
                        KualitasPosisi kualitas) {
  String paket;
  paket += String(sourceID);
  paket += ',';
  paket += namaStatus(status);
  paket += ',';
  paket += String(lat, 6);
  paket += ',';
  paket += String(lon, 6);
  paket += ',';
  paket += namaKualitas(kualitas);
  return paket;
}
