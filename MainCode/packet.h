#pragma once

#include <Arduino.h>
#include "config.h"
#include "dedup.h"

// ============================================================
// Format paket CSV:
//
//   sourceID,lastHopID,seq,tipe,isi...
//
// sourceID  = node yang MEMBUAT paket, tidak pernah berubah
// lastHopID = node yang TERAKHIR menyiarkan, diubah tiap kali diteruskan
// seq       = nomor urut dari pembuat, naik 1 tiap paket BARU. Siaran
//             ulang / paket yang diteruskan membawa seq yang sama, dan
//             pasangan (sourceID, seq) itulah yang dipakai penerima
//             untuk mengenali duplikat.
// tipe      = STATUS atau PESAN
// isi       = sisa baris, bentuknya tergantung tipe
//
// tipe STATUS -> isi = status,lat,lon,flag
//   1,1,17,STATUS,BANTUAN,-6.365432,106.824512,FIX   (asli dari field)
//   1,2,17,STATUS,BANTUAN,-6.365432,106.824512,FIX   (setelah relay id 2)
//
// tipe PESAN -> isi = teks bebas
//   0,0,4,PESAN,segera kembali ke titik kumpul
//   0,2,4,PESAN,segera kembali ke titik kumpul      (setelah relay id 2)
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
  uint16_t   seq;
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

inline String buatHeader(uint8_t sourceID, uint8_t lastHopID, uint16_t seq) {
  String h;
  h += String(sourceID);
  h += ',';
  h += String(lastHopID);
  h += ',';
  h += String(seq);
  h += ',';
  return h;
}

inline String buatPaketStatus(uint8_t sourceID,
                              uint8_t lastHopID,
                              uint16_t seq,
                              StatusPersonel status,
                              float lat,
                              float lon,
                              KualitasPosisi kualitas) {
  String p = buatHeader(sourceID, lastHopID, seq);
  p += "STATUS,";
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
                             uint16_t seq,
                             const String &teks) {
  String p = buatHeader(sourceID, lastHopID, seq);
  p += "PESAN,";
  p += teks;
  return p;
}

// ---------- pembaca paket ----------

inline bool parsePaket(const String &raw, Paket &p) {
  int k1 = raw.indexOf(',');
  int k2 = raw.indexOf(',', k1 + 1);
  int k3 = raw.indexOf(',', k2 + 1);
  int k4 = raw.indexOf(',', k3 + 1);
  if (k1 < 0 || k2 < 0 || k3 < 0 || k4 < 0) {
    return false;
  }

  p.sourceID  = (uint8_t)raw.substring(0, k1).toInt();
  p.lastHopID = (uint8_t)raw.substring(k1 + 1, k2).toInt();
  p.seq       = (uint16_t)raw.substring(k2 + 1, k3).toInt();

  String tipe = raw.substring(k3 + 1, k4);
  String isi  = raw.substring(k4 + 1);

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

// ---------- penyaring paket masuk ----------
//
// Semua alasan menolak paket dikumpulkan di satu tempat supaya field
// dan command node tidak punya dua versi aturan yang berbeda.
// Urutannya penting: pemeriksaan murah dan pasti (asing, gema) dulu,
// dedup paling akhir karena ia MENCATAT paket ke tabel.

enum HasilSaring {
  SARING_LOLOS,
  SARING_ASING,       // sourceID tidak ada di whitelist
  SARING_GEMA,        // paket buatan node ini sendiri yang kembali
  SARING_VIA_RELAY,   // sudah diteruskan node lain, ditolak di mode HANYA_LANGSUNG
  SARING_DUPLIKAT     // (sourceID, seq) sudah pernah diterima
};

inline const char* namaSaring(HasilSaring h) {
  switch (h) {
    case SARING_LOLOS:     return "LOLOS";
    case SARING_ASING:     return "ASING";
    case SARING_GEMA:      return "GEMA";
    case SARING_VIA_RELAY: return "VIA RELAY";
    case SARING_DUPLIKAT:  return "DUPLIKAT";
  }
  return "TIDAKVALID";
}

inline HasilSaring saringPaket(const Paket &p, uint8_t nodeID) {
  if (!nodeDikenal(p.sourceID)) {
    return SARING_ASING;
  }

  // Anti-loop 1: paket yang kita buat sendiri tidak boleh diproses lagi,
  // apa pun jalurnya kembali ke kita.
  if (p.sourceID == nodeID) {
    return SARING_GEMA;
  }

#if HANYA_LANGSUNG
  // Anti-loop 2: lastHopID != sourceID berarti sudah lewat tangan node
  // lain. Dalam uji dua node ini tidak ada yang boleh meneruskan.
  if (p.lastHopID != p.sourceID) {
    return SARING_VIA_RELAY;
  }
#endif

  if (sudahPernahDilihat(p.sourceID, p.seq)) {
    return SARING_DUPLIKAT;
  }

  return SARING_LOLOS;
}
