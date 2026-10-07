#pragma once

#include <Arduino.h>
#include "config.h"
#include "dedup.h"

// ============================================================
// Format paket CSV:
//
//   sourceID,lastHopID,seq,tipe,lat,lon,flag,hops,teks
//
// sourceID  = node yang MEMBUAT paket, tidak pernah berubah
// lastHopID = node yang TERAKHIR menyiarkan, diubah tiap kali diteruskan
// seq       = nomor urut dari pembuat, naik 1 tiap paket BARU. Siaran
//             ulang / paket yang diteruskan membawa seq yang sama, dan
//             pasangan (sourceID, seq) itulah yang dipakai penerima
//             untuk mengenali duplikat.
// tipe      = untuk sekarang selalu PESAN. Kolomnya sengaja
//             dipertahankan supaya tipe lain bisa ditambahkan nanti
//             tanpa mengubah susunan field yang sudah ada.
// lat, lon  = posisi pengirim, 0.000000 kalau tidak punya/belum fix
// flag      = kualitas posisi: FIX, STALE, atau NOFIX
// hops      = kualitas sinyal tiap hop yang sudah dilalui, berurutan,
//             dipisah ';'. Satu entri = penerima:rssi:snr, ditambahkan
//             oleh node yang MENERIMA hop itu. Pembuat paket mengirimnya
//             kosong, tiap relay menambahkan satu entri sebelum
//             meneruskan, dan penerima akhir menambahkan entri hop
//             terakhir sendiri saat mencatat (tidak disiarkan lagi).
//             Pengirim tiap hop = penerima entri sebelumnya, atau
//             sourceID untuk entri pertama.
// teks      = isi pesan, bentuknya bebas
//
// Contoh:
//   1,1,17,PESAN,-6.365432,106.824512,FIX,,siaran #17, 7 satelit, FIX
//   1,2,17,PESAN,-6.365432,106.824512,FIX,2:-67:8.25,siaran #17, 7 satelit, FIX
//     (baris kedua = paket yang sama setelah diteruskan relay id 2, yang
//      menerimanya dari field dengan RSSI -67 dBm dan SNR 8.25 dB)
//   0,0,4,PESAN,0.000000,0.000000,NOFIX,,segera kembali ke titik kumpul
//
// Saat dicatat command, hops paket di atas menjadi lengkap, misalnya
//   2:-67:8.25;0:-41:9.50   = field->relay -67/8.25, relay->command -41/9.50
//
// Payload teks sengaja ditaruh PALING AKHIR supaya boleh mengandung
// koma tanpa merusak pemisahan field.
//
// Enum StatusPersonel (AMAN/SIAGA/BANTUAN) untuk sementara DIHAPUS dari
// paket. Status personel sekarang cuma kata biasa di dalam teks kalau
// memang perlu dikirim.
//
// Ketiga node WAJIB memakai header ini agar urutan field sepakat.
// ============================================================

enum KualitasPosisi {
  POS_FIX,    // GPS baru saja memperbarui posisi
  POS_STALE,  // pernah dapat posisi, tapi sudah lama tidak diperbarui
  POS_NOFIX   // belum pernah dapat posisi sejak board menyala
};

enum TipePaket {
  TIPE_PESAN,
  TIPE_TIDAKVALID
};

struct Paket {
  uint8_t    sourceID;
  uint8_t    lastHopID;
  uint16_t   seq;
  TipePaket  tipe;

  float          lat;
  float          lon;
  KualitasPosisi kualitas;

  // Daftar penerima:rssi:snr per hop, apa adanya dari paket.
  String hops;

  String teks;
};

// Fungsi di header harus `inline`, kalau tidak linker akan protes
// "multiple definition" begitu header ini di-include lebih dari satu .cpp.
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

inline String buatPaketPesan(uint8_t sourceID,
                             uint8_t lastHopID,
                             uint16_t seq,
                             float lat,
                             float lon,
                             KualitasPosisi kualitas,
                             const String &teks) {
  String p = buatHeader(sourceID, lastHopID, seq);
  p += "PESAN,";
  p += String(lat, 6);
  p += ',';
  p += String(lon, 6);
  p += ',';
  p += namaKualitas(kualitas);
  // hops kosong: paket baru belum melewati hop mana pun.
  p += ",,";
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

  if (tipe != "PESAN") {
    p.tipe = TIPE_TIDAKVALID;
    return false;
  }

  // isi = lat,lon,flag,hops,teks
  int m1 = isi.indexOf(',');
  int m2 = isi.indexOf(',', m1 + 1);
  int m3 = isi.indexOf(',', m2 + 1);
  int m4 = isi.indexOf(',', m3 + 1);
  if (m1 < 0 || m2 < 0 || m3 < 0 || m4 < 0) {
    p.tipe = TIPE_TIDAKVALID;
    return false;
  }

  p.lat = isi.substring(0, m1).toFloat();
  p.lon = isi.substring(m1 + 1, m2).toFloat();

  String f = isi.substring(m2 + 1, m3);
  if      (f == "FIX")   p.kualitas = POS_FIX;
  else if (f == "STALE") p.kualitas = POS_STALE;
  else                   p.kualitas = POS_NOFIX;

  p.hops = isi.substring(m3 + 1, m4);

  // Sisa baris apa adanya, termasuk koma yang ada di dalamnya.
  p.teks = isi.substring(m4 + 1);

  p.tipe = TIPE_PESAN;
  return true;
}

// Nomor kolom (mulai 0) yang diubah relay. Semuanya terletak sebelum
// kolom teks, jadi koma di dalam teks tidak ikut terhitung.
#define KOLOM_LASTHOP    1
#define KOLOM_HOPS       7

// Ganti isi satu kolom tanpa membongkar seluruh paket, sehingga kolom
// lain, terutama teks dan koordinat, diteruskan persis seperti dikirim
// pembuatnya (tidak melewati konversi float bolak-balik).
inline String gantiKolom(const String &raw, int kolom, const String &nilai) {
  int awal = 0;
  for (int i = 0; i < kolom; i++) {
    awal = raw.indexOf(',', awal) + 1;
    if (awal == 0) {
      return raw;
    }
  }
  int akhir = raw.indexOf(',', awal);
  if (akhir < 0) {
    return raw;
  }
  return raw.substring(0, awal) + nilai + raw.substring(akhir);
}

// Daftar hops ditambah satu entri penerima:rssi:snr. Dipakai relay
// sebelum meneruskan dan command saat mencatat hop terakhir.
inline String tambahHop(const String &hops, uint8_t penerima, int rssi, float snr) {
  String entri = String(penerima) + ':' + String(rssi) + ':' + String(snr, 2);
  return hops.length() == 0 ? entri : hops + ';' + entri;
}

// Bentuk paket yang disiarkan ulang relay: lastHopID diganti id relay,
// dan ukuran hop yang baru diterima relay ditambahkan ke hops.
inline String buatPaketTerusan(const String &raw, const Paket &p,
                               uint8_t idRelay, int rssi, float snr) {
  String hasil = gantiKolom(raw, KOLOM_LASTHOP, String(idRelay));
  return gantiKolom(hasil, KOLOM_HOPS, tambahHop(p.hops, idRelay, rssi, snr));
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
  SARING_VIA_RELAY,   // sudah diteruskan relay, ditolak di JALUR_LANGSUNG
  SARING_LANGSUNG,    // belum lewat relay, ditolak di JALUR_WAJIB_RELAY
  SARING_DUPLIKAT     // (sourceID, seq) sudah pernah diterima
};

inline const char* namaSaring(HasilSaring h) {
  switch (h) {
    case SARING_LOLOS:     return "LOLOS";
    case SARING_ASING:     return "ASING";
    case SARING_GEMA:      return "GEMA";
    case SARING_VIA_RELAY: return "VIA RELAY";
    case SARING_LANGSUNG:  return "LANGSUNG";
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

  // Aturan jalur. Letaknya WAJIB sebelum dedup di bawah, karena dedup
  // mencatat (sourceID, seq) ke tabel. Dalam JALUR_WAJIB_RELAY, salinan
  // langsung dan salinan dari relay membawa seq yang sama; kalau yang
  // langsung sempat tercatat lebih dulu, salinan dari relay akan dibuang
  // sebagai DUPLIKAT dan jalur relay mati total.
#if MODE_JALUR == JALUR_LANGSUNG
  // lastHopID != sourceID berarti sudah lewat tangan node lain.
  if (p.lastHopID != p.sourceID) {
    return SARING_VIA_RELAY;
  }
#elif MODE_JALUR == JALUR_WAJIB_RELAY
  // Kebalikannya: paket yang belum diteruskan siapa pun ditolak, supaya
  // field dan command terbukti tidak bicara langsung.
  if (p.lastHopID == p.sourceID) {
    return SARING_LANGSUNG;
  }
#endif

  if (sudahPernahDilihat(p.sourceID, p.seq)) {
    return SARING_DUPLIKAT;
  }

  return SARING_LOLOS;
}
