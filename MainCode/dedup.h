#pragma once

#include <Arduino.h>
#include "config.h"

// ============================================================
// Anti-duplikasi.
//
// Setiap paket punya pasangan (sourceID, seq) yang unik selama
// pembuatnya tidak reboot. Penerima mengingat pasangan yang baru saja
// dilihat di tabel kecil ini; kalau pasangan yang sama datang lagi,
// paket itu duplikat dan dibuang.
//
// Sumber duplikat yang dijaga di sini:
//   - siaran ulang yang disengaja (ULANG_KIRIM di config.h)
//   - nanti: paket yang sama tiba lewat dua relay berbeda
//
// Tabel berbentuk ring buffer: entri tertua ditimpa kalau penuh. Entri
// juga dianggap kosong setelah DEDUP_UMUR_MS, supaya node yang reboot
// dan seq-nya kebetulan sama tidak dianggap duplikat.
// ============================================================

struct JejakPaket {
  uint8_t       sourceID;
  uint16_t      seq;
  unsigned long waktu;
  bool          terpakai;
};

// Mengembalikan true kalau (sourceID, seq) SUDAH pernah dilihat.
// Kalau belum, pasangan itu dicatat lalu mengembalikan false.
inline bool sudahPernahDilihat(uint8_t sourceID, uint16_t seq) {
  static JejakPaket jejak[DEDUP_UKURAN] = {};
  static uint8_t posisiTulis = 0;

  unsigned long now = millis();

  for (uint8_t i = 0; i < DEDUP_UKURAN; i++) {
    if (!jejak[i].terpakai) {
      continue;
    }
    if (now - jejak[i].waktu > DEDUP_UMUR_MS) {
      jejak[i].terpakai = false;
      continue;
    }
    if (jejak[i].sourceID == sourceID && jejak[i].seq == seq) {
      return true;
    }
  }

  jejak[posisiTulis].sourceID = sourceID;
  jejak[posisiTulis].seq      = seq;
  jejak[posisiTulis].waktu    = now;
  jejak[posisiTulis].terpakai = true;
  posisiTulis = (posisiTulis + 1) % DEDUP_UKURAN;

  return false;
}
