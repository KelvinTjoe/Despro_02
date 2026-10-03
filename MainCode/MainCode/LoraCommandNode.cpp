#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <esp_system.h>

#include "config.h"
#include "packet.h"

#define NODE_ID  ID_COMMAND

// ============================================================
// Command node juga bertugas sebagai sumber data logger. Untuk setiap
// paket yang LOLOS saring ia mencetak satu baris CSV berawalan "DATA,"
// ke Serial, dan script Python di laptop menyimpannya ke file.
//
// Semua pesan lain (DITERIMA, POSISI, RUSAK, ASING, GEMA, DUPLIKAT,
// TERKIRIM, ...) sengaja TIDAK berformat CSV dan tidak berawalan DATA,
// supaya tetap terbaca manusia dan tidak ikut tersimpan ke file.
//
// Baris "META," berisi parameter radio yang sedang dipakai, dan dipakai
// script sebagai baris komentar pertama file log. Itu catatan kondisi
// pengujian: tanpa parameter radio, data RSSI di file tidak bisa
// dibandingkan antar sesi pengujian.
// ============================================================

// Urutan kolom baris DATA. Script di laptop menambahkan waktu_laptop
// di depan, jadi header file menjadi:
//   waktu_laptop,sourceID,lastHopID,seq,lat,lon,jumlah_hop,hops,teks
//
// hops = penerima:rssi:snr untuk SETIAP hop, berurutan, dipisah ';'.
//        Entri sebelumnya dibawa paket (diisi relay), entri terakhir
//        adalah ukuran command sendiri saat paket tiba. Contoh:
//          2:-67:8.25;0:-41:9.50   field->relay, lalu relay->command
//          0:-38:9.75              field->command langsung
#define KOLOM_DATA  "sourceID,lastHopID,seq,lat,lon,jumlah_hop,hops,teks"

// Jeda pengulangan baris META. Diulang berkala, bukan sekali saat boot,
// supaya logger yang baru dijalankan setelah board menyala tetap kebagian
// parameter radio tanpa board perlu di-reset dulu.
#define META_ULANG_MS  60000UL

void terimaPaket();
void bacaInputOperator();
void kirimPesan(const String &teks);
void siarkan(const String &paket);
void kirimMeta();

String bufferInput;

// Nomor urut paket berikutnya, lihat penjelasan di LoraFieldNode.cpp.
uint16_t seqBerikutnya = 0;

unsigned long metaTerakhir = 0;

// Nama MODE_JALUR dalam bentuk teks, untuk dicatat di baris META.
inline const char* namaModeJalur() {
#if MODE_JALUR == JALUR_LANGSUNG
  return "LANGSUNG";
#elif MODE_JALUR == JALUR_BEBAS
  return "BEBAS";
#else
  return "WAJIB_RELAY";
#endif
}

// Teks paket BOLEH mengandung koma, karena posisinya paling akhir pada
// baris CSV. Yang tidak boleh adalah pergantian baris: pembaca di laptop
// bekerja per baris, jadi satu CR/LF yang nyelip akan memecah satu record
// menjadi dua baris rusak. Diganti spasi, bukan dibuang, supaya kata di
// kiri dan kanannya tidak menyatu.
inline String amankanUntukCSV(const String &teks) {
  String hasil = teks;
  hasil.replace('\r', ' ');
  hasil.replace('\n', ' ');
  return hasil;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.printf("=== COMMAND NODE (id %d) ===\n", NODE_ID);

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  // Turunkan clock SPI dari 8 MHz bawaan library, lihat LORA_SPI_FREQ
  // di config.h. Harus dipanggil sebelum LoRa.begin().
  LoRa.setSPIFrequency(LORA_SPI_FREQ);

  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("LoRa: init GAGAL, cek wiring SPI dan catu daya modul");
    while (1) {
      delay(1000);
    }
  }

  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setCodingRate4(LORA_CR);
  LoRa.setTxPower(LORA_TX_POWER);

  // CRC dinyalakan supaya radio membuang sendiri paket yang rusak. Tanpa
  // ini derau 433 MHz dan paket hantu ikut sampai ke kode, dan logger
  // ikut mencatat baris RUSAK yang sebetulnya bukan siaran node mana pun.
  LoRa.enableCrc();

  seqBerikutnya = (uint16_t)(esp_random() & 0xFFFF);

  Serial.println("LoRa: OK");

#if MODE_JALUR == JALUR_LANGSUNG
  Serial.println("JALUR: LANGSUNG, hanya field <-> command tanpa perantara.");
  Serial.println("       paket yang sudah diteruskan relay akan DITOLAK.");
  Serial.println("       ubah MODE_JALUR di config.h untuk mode lain.");
#elif MODE_JALUR == JALUR_BEBAS
  Serial.println("JALUR: BEBAS, paket langsung maupun via relay diterima.");
#else
  Serial.println("JALUR: WAJIB RELAY, paket langsung dari field DITOLAK.");
  Serial.println("       semua paket harus lewat relay dulu. Kalau relay");
  Serial.println("       mati, komunikasi berhenti total.");
#endif
  Serial.printf("Tiap paket disiarkan %d kali (seq mulai %u)\n", ULANG_KIRIM, seqBerikutnya);

  Serial.printf("LOGGER: baris data berawalan \"DATA,\" dengan kolom %s\n", KOLOM_DATA);
  Serial.println("        baris lain hanya untuk dibaca manusia, tidak dicatat.");

  kirimMeta();

  Serial.println("Ketik pesan lalu tekan Enter untuk menyiarkan ke lapangan.");
  Serial.println();
}

void loop() {
  terimaPaket();
  bacaInputOperator();

  if (millis() - metaTerakhir >= META_ULANG_MS) {
    kirimMeta();
  }
}

// Parameter radio yang sedang berlaku, seluruhnya dibaca dari config.h
// supaya isi baris ini tidak pernah berbeda dari yang benar-benar dipakai
// radio. Satu baris, dipisah spasi, bukan koma, supaya script bisa
// menaruhnya apa adanya sebagai komentar di file CSV.
void kirimMeta() {
  metaTerakhir = millis();

  Serial.printf("META,freq=%.0fHz sf=%d bw=%.0fHz cr=4/%d tx=%ddBm "
                "spi=%.0fHz crc=on jalur=%s ulang=%d jeda=%lums "
                "dedup=%d/%lums node=%d\n",
                (double)LORA_FREQ, LORA_SF, (double)LORA_BW, LORA_CR,
                LORA_TX_POWER, (double)LORA_SPI_FREQ, namaModeJalur(),
                ULANG_KIRIM, (unsigned long)JEDA_ULANG_MS,
                DEDUP_UKURAN, (unsigned long)DEDUP_UMUR_MS, NODE_ID);
}

void terimaPaket() {
  int panjang = LoRa.parsePacket();
  if (panjang == 0) {
    return;
  }

  // RSSI dan SNR harus diambil di sini, sebelum parsePacket() berikutnya
  // menimpanya dengan paket lain.
  int   rssi = LoRa.packetRssi();
  float snr  = LoRa.packetSnr();

  // Pembacaan dibatasi pada panjang yang dilaporkan radio, dan tidak lebih
  // dari BATAS_BACA_FIFO. Jangan hanya mengandalkan LoRa.available(), karena
  // ia membaca ulang register panjang lewat SPI tiap putaran: kalau modul
  // menjawab ngawur, loop ini bisa tidak pernah berhenti.
  int sisa = panjang > BATAS_BACA_FIFO ? BATAS_BACA_FIFO : panjang;

  String raw;
  while (sisa > 0 && LoRa.available()) {
    raw += (char)LoRa.read();
    sisa--;
  }

  Paket p;
  if (!parsePaket(raw, p)) {
    Serial.printf("RUSAK  : \"%s\" (RSSI %d)\n", raw.c_str(), rssi);
    return;
  }

  HasilSaring hasil = saringPaket(p, NODE_ID);
  if (hasil != SARING_LOLOS) {
    Serial.printf("%-9s: dari node %d, lastHop %d, seq %u (RSSI %d)\n",
                  namaSaring(hasil), p.sourceID, p.lastHopID, p.seq, rssi);
    return;
  }

  Serial.printf("DITERIMA: dari node %d, lastHop %d, seq %u, RSSI %d dBm, SNR %.2f dB\n",
                p.sourceID, p.lastHopID, p.seq, rssi, snr);
  String hops = tambahHop(p.hops, NODE_ID, rssi, snr);
  Serial.printf("  HOPS   %s\n", hops.c_str());
  Serial.printf("  POSISI %.6f, %.6f (%s)\n",
                p.lat, p.lon, namaKualitas(p.kualitas));

  if (p.kualitas == POS_NOFIX) {
    Serial.println("  perhatian: pengirim belum dapat fix GPS");
  } else if (p.kualitas == POS_STALE) {
    Serial.println("  perhatian: posisi sudah lama tidak diperbarui");
  }

  Serial.printf("  PESAN  \"%s\"\n", p.teks.c_str());

  // Baris untuk logger. Hanya paket LOLOS yang dicatat: paket RUSAK, ASING,
  // GEMA, DUPLIKAT, VIA RELAY, dan LANGSUNG tetap cuma tampil di konsol.
  // jumlah_hop = jumlah entri di hops = jumlah ';' + 1.
  int jumlahHop = 1;
  for (unsigned int i = 0; i < hops.length(); i++) {
    if (hops[i] == ';') {
      jumlahHop++;
    }
  }

  String teksAman = amankanUntukCSV(p.teks);
  Serial.printf("DATA,%u,%u,%u,%.6f,%.6f,%d,%s,%s\n",
                (unsigned)p.sourceID, (unsigned)p.lastHopID, (unsigned)p.seq,
                p.lat, p.lon, jumlahHop, hops.c_str(), teksAman.c_str());

  Serial.println();
}

void bacaInputOperator() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (bufferInput.length() > 0) {
        kirimPesan(bufferInput);
        bufferInput = "";
      }
      continue;
    }

    if (bufferInput.length() < PESAN_MAX_CHAR) {
      bufferInput += c;
    }
    // karakter berlebih dibuang, bukan disambung ke pesan berikutnya
  }
}

void kirimPesan(const String &teks) {
  // Command node tidak punya GPS: koordinat selalu 0,0 dan kualitasnya
  // NOFIX. Kolomnya tetap diisi supaya format paket kedua node sama
  // persis dan parsernya cuma satu.
  // seq naik SETELAH dipakai, jadi siaran ulang membawa nomor yang sama.
  String paket = buatPaketPesan(NODE_ID, NODE_ID, seqBerikutnya,
                                0.0f, 0.0f, POS_NOFIX, teks);
  seqBerikutnya++;

  siarkan(paket);
  Serial.println();
}

// Siarkan paket ULANG_KIRIM kali. delay() di sini boleh karena hanya
// terjadi saat operator menekan Enter, bukan di jalur penerimaan rutin.
void siarkan(const String &paket) {
  for (int i = 0; i < ULANG_KIRIM; i++) {
    if (i > 0) {
      delay(JEDA_ULANG_MS);
    }
    LoRa.beginPacket();
    LoRa.print(paket);
    LoRa.endPacket();
    Serial.printf("TERKIRIM (%d/%d): %s\n", i + 1, ULANG_KIRIM, paket.c_str());
  }
}
