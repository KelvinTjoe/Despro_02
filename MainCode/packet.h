#pragma once

#include <Arduino.h>
#include "config.h"

// ============================================================
// Format paket CSV:
//
//   sourceID,lastHopID,tipe,isi...
//
// sourceID  = node yang MEMBUAT paket, tidak pernah berubah
// lastHopID = node yang TERAKHIR menyiarkan, diubah tiap kali diteruskan
// tipe      = STATUS atau PESAN
// isi       = sisa baris, bentuknya tergantung tipe
//
// tipe STATUS -> isi = status,lat,lon,flag
//   1,1,STATUS,BANTUAN,-6.365432,106.824512,FIX     (asli dari field)
//   1,2,STATUS,BANTUAN,-6.365432,106.824512,FIX     (setelah relay id 2)
//
// tipe PESAN -> isi = teks bebas
//   0,0,PESAN,segera kembali ke titik kumpul
//   0,2,PESAN,segera kembali ke titik kumpul        (setelah relay id 2)
//
// Payload teks sengaja ditaruh PALING AKHIR supaya boleh mengandung
// koma tanpa merusak pemisahan field.
//
// Ketiga node WAJIB memakai header ini agar urutan field sepakat.
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

enum TipePaket {
  TIPE_STATUS,
  TIPE_PESAN,
  TIPE_TIDAKVALID
};

struct Paket {
  uint8_t    sourceID;
  uint8_t    lastHopID;
  TipePaket  tipe;

  // terisi hanya kalau tipe == TIPE_STATUS
  StatusPersonel status;
  float          lat;
  float          lon;
  KualitasPosisi kualitas;

  // terisi hanya kalau tipe == TIPE_PESAN
  String teks;
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

// ---------- penyusun paket ----------

inline String buatPaketStatus(uint8_t sourceID,
                              uint8_t lastHopID,
                              StatusPersonel status,
                              float lat,
                              float lon,
                              KualitasPosisi kualitas) {
  String p;
  p += String(sourceID);
  p += ',';
  p += String(lastHopID);
  p += ",STATUS,";
  p += namaStatus(status);
  p += ',';
  p += String(lat, 6);
  p += ',';
  p += String(lon, 6);
  p += ',';
  p += namaKualitas(kualitas);
  return p;
}

inline String buatPaketPesan(uint8_t sourceID,
                             uint8_t lastHopID,
                             const String &teks) {
  String p;
  p += String(sourceID);
  p += ',';
  p += String(lastHopID);
  p += ",PESAN,";
  p += teks;
  return p;
}

// ---------- pembaca paket ----------

inline bool parsePaket(const String &raw, Paket &p) {
  int k1 = raw.indexOf(',');
  int k2 = raw.indexOf(',', k1 + 1);
  int k3 = raw.indexOf(',', k2 + 1);
  if (k1 < 0 || k2 < 0 || k3 < 0) {
    return false;
  }

  p.sourceID  = (uint8_t)raw.substring(0, k1).toInt();
  p.lastHopID = (uint8_t)raw.substring(k1 + 1, k2).toInt();

  String tipe = raw.substring(k2 + 1, k3);
  String isi  = raw.substring(k3 + 1);

  if (tipe == "PESAN") {
    p.tipe = TIPE_PESAN;
    p.teks = isi;
    return true;
  }

  if (tipe != "STATUS") {
    p.tipe = TIPE_TIDAKVALID;
    return false;
  }

  // isi = status,lat,lon,flag
  int m1 = isi.indexOf(',');
  int m2 = isi.indexOf(',', m1 + 1);
  int m3 = isi.indexOf(',', m2 + 1);
  if (m1 < 0 || m2 < 0 || m3 < 0) {
    p.tipe = TIPE_TIDAKVALID;
    return false;
  }

  String s = isi.substring(0, m1);
  if      (s == "AMAN")    p.status = STATUS_AMAN;
  else if (s == "SIAGA")   p.status = STATUS_SIAGA;
  else if (s == "BANTUAN") p.status = STATUS_BANTUAN;
  else { p.tipe = TIPE_TIDAKVALID; return false; }

  p.lat = isi.substring(m1 + 1, m2).toFloat();
  p.lon = isi.substring(m2 + 1, m3).toFloat();

  String f = isi.substring(m3 + 1);
  if      (f == "FIX")   p.kualitas = POS_FIX;
  else if (f == "STALE") p.kualitas = POS_STALE;
  else                   p.kualitas = POS_NOFIX;

  p.tipe = TIPE_STATUS;
  return true;
}

// Ganti field lastHopID tanpa membongkar seluruh paket, sehingga
// payload teks tetap utuh apa adanya termasuk komanya.
inline String gantiLastHop(const String &raw, uint8_t idBaru) {
  int k1 = raw.indexOf(',');
  int k2 = raw.indexOf(',', k1 + 1);
  if (k1 < 0 || k2 < 0) {
    return raw;
  }
  return raw.substring(0, k1 + 1) + String(idBaru) + raw.substring(k2);
}

inline bool nodeDikenal(uint8_t id) {
  const uint8_t daftar[] = NODE_DIKENAL;
  for (uint8_t i = 0; i < sizeof(daftar); i++) {
    if (daftar[i] == id) {
      return true;
    }
  }
  return false;
}
