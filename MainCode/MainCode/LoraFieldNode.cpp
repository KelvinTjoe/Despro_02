#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <TinyGPSPlus.h>
#include <esp_system.h>

#include "config.h"
#include "packet.h"

#define NODE_ID  ID_FIELD

// File .cpp tidak dapat prototipe otomatis seperti .ino,
// jadi setiap fungsi harus dideklarasikan sebelum dipakai.
void bacaGPS();
void terimaPaket();
void laporGPS();
void kirimPesan(const String &teks);
void siarkan(const String &paket);
void kirimPeriodik();
KualitasPosisi kualitasSekarang();

#if TOMBOL_AKTIF
void periksaTombol();
#endif

TinyGPSPlus gps;

// Nomor urut paket berikutnya. Dimulai dari angka acak supaya setelah
// reboot tidak mengulang seq yang mungkin masih diingat penerima.
uint16_t seqBerikutnya = 0;

// Nomor siaran periodik, dimulai dari 1 dan ikut dicetak di dalam teks.
// Berbeda dari seq: yang ini dibaca manusia dan selalu mulai dari awal
// tiap boot, jadi lompatan angkanya langsung terlihat di command node.
unsigned long nomorSiaran = 0;

// Posisi valid terakhir. Hanya diperbarui saat GPS benar-benar fix,
// sehingga nilainya tetap terpakai ketika sinyal satelit hilang.
float lastLat = 0.0f;
float lastLon = 0.0f;
unsigned long lastFixTime = 0;
bool pernahFix = false;

#if TOMBOL_AKTIF
// Tombol tidak lagi mengirim enum status, karena StatusPersonel sudah
// dikeluarkan dari paket. Yang dikirim sekarang cuma labelnya sebagai
// teks biasa.
struct Tombol {
  uint8_t     pin;
  const char *label;
  bool        bacaanMentah;
  bool        bacaanStabil;
  unsigned long waktuBerubah;
  unsigned long mulaiTahan;
  bool        sudahKirim;
};

Tombol tombol[] = {
  { BUTTON_AMAN,    "AMAN",    false, false, 0, 0, false },
  { BUTTON_SIAGA,   "SIAGA",   false, false, 0, 0, false },
  { BUTTON_BANTUAN, "BANTUAN", false, false, 0, 0, false },
};

const int JUMLAH_TOMBOL = sizeof(tombol) / sizeof(tombol[0]);
#endif

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.printf("=== Field Node %d ===\n", NODE_ID);

#if TOMBOL_AKTIF
  for (int i = 0; i < JUMLAH_TOMBOL; i++) {
    pinMode(tombol[i].pin, INPUT_PULLUP);
  }
#endif

  //inisialisasi GPS NEO-6M di UART2, pin RX/TX sesuai config.h
  //cold start minimum 30 detik
  Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("GPS: UART2 siap, menunggu koordinat fix dari satelit");

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  //jika lora gagal init, loop di sini selamanya
  //daripada menampilakn pesan error terus menerus
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("LoRa: init GAGAL, cek wiring SPI dan catu daya modul");
    while (1) {
      delay(1000);
    }
  }

  //inisialisasi parameter radio LoRa, set parameter sesuai config.h
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setCodingRate4(LORA_CR);
  LoRa.setTxPower(LORA_TX_POWER);

  seqBerikutnya = (uint16_t)(esp_random() & 0xFFFF);

  Serial.printf("LoRa: OK pada %d MHz, SF%d\n", (int)(LORA_FREQ / 1E6), LORA_SF);
#if MODE_JALUR == JALUR_LANGSUNG
  Serial.println("JALUR: LANGSUNG, paket hasil terusan relay DITOLAK.");
#elif MODE_JALUR == JALUR_BEBAS
  Serial.println("JALUR: BEBAS, paket langsung maupun via relay diterima.");
#else
  Serial.println("JALUR: WAJIB RELAY, paket langsung dari command DITOLAK.");
  Serial.println("       kalau relay mati, tidak ada paket yang masuk.");
#endif
  Serial.printf("Tiap paket disiarkan %d kali (seq mulai %u)\n", ULANG_KIRIM, seqBerikutnya);
#if AUTO_KIRIM
  Serial.printf("SIARAN PERIODIK: tiap %lu detik\n", AUTO_KIRIM_MS / 1000);
#endif
#if TOMBOL_AKTIF
  Serial.printf("Tahan salah satu tombol selama %lu detik untuk mengirim\n", HOLD_DURATION/1000);
#else
  Serial.println("Tombol dinonaktifkan (TOMBOL_AKTIF 0 di config.h)");
#endif
}

void loop() {
  bacaGPS();
#if TOMBOL_AKTIF
  periksaTombol();
#endif
  kirimPeriodik();
  terimaPaket();
  laporGPS();
}

// Siaran berkala. Siaran pertama langsung saat boot, supaya tidak perlu
// menunggu satu periode penuh untuk tahu radio jalan.
void kirimPeriodik() {
#if AUTO_KIRIM
  static unsigned long terakhirKirim = 0;
  static bool sudahPertama = false;
  unsigned long now = millis();

  if (sudahPertama && now - terakhirKirim < AUTO_KIRIM_MS) {
    return;
  }
  terakhirKirim = now;
  sudahPertama = true;

  nomorSiaran++;

  // Teks dibuat supaya satu baris di command node sudah cukup untuk
  // menilai kondisi field node: paket ke berapa, berapa satelit yang
  // terlihat, dan apakah koordinatnya benar-benar fix.
  String teks = "siaran #";
  teks += String(nomorSiaran);
  teks += ", ";
  teks += String(gps.satellites.isValid() ? gps.satellites.value() : 0);
  teks += " satelit, ";
  teks += namaKualitas(kualitasSekarang());

  kirimPesan(teks);
#endif
}

void bacaGPS() {
  while (Serial2.available() > 0) {
    if (gps.encode(Serial2.read())) {
      if (gps.location.isValid() && gps.location.isUpdated()) {
        lastLat = gps.location.lat();
        lastLon = gps.location.lng();
        lastFixTime = millis();
        pernahFix = true;
      }
    }
  }
}

#if TOMBOL_AKTIF
void periksaTombol() {
  unsigned long now = millis();

  for (int i = 0; i < JUMLAH_TOMBOL; i++) {
    // Referensi, bukan salinan, supaya perubahan di bawah benar-benar
    // tersimpan ke array dan tidak hilang di akhir iterasi.
    Tombol &t = tombol[i];

    bool mentah = (digitalRead(t.pin) == LOW);

    if (mentah != t.bacaanMentah) {
      t.bacaanMentah = mentah;
      t.waktuBerubah = now;
    }

    if (now - t.waktuBerubah >= DEBOUNCE_MS && mentah != t.bacaanStabil) {
      t.bacaanStabil = mentah;

      if (mentah) {
        t.mulaiTahan = now;
        t.sudahKirim = false;
        Serial.printf("[%s] mulai ditahan...\n", t.label);
      } else if (!t.sudahKirim) {
        Serial.printf("[%s] dilepas terlalu cepat, dibatalkan\n", t.label);
      }
    }

    if (t.bacaanStabil && !t.sudahKirim && now - t.mulaiTahan >= HOLD_DURATION) {
      kirimPesan(String(t.label));
      t.sudahKirim = true;
    }
  }
}
#endif

// Kualitas posisi saat ini, dipakai baik untuk field paket maupun
// untuk teksnya, supaya keduanya tidak mungkin berbeda.
KualitasPosisi kualitasSekarang() {
  if (!pernahFix) {
    return POS_NOFIX;
  }
  return (millis() - lastFixTime <= GPS_STALE_AGE) ? POS_FIX : POS_STALE;
}

void kirimPesan(const String &teks) {
  KualitasPosisi kualitas = kualitasSekarang();
  float lat = (kualitas == POS_NOFIX) ? 0.0f : lastLat;
  float lon = (kualitas == POS_NOFIX) ? 0.0f : lastLon;

  // lastHopID = NODE_ID karena paket ini baru dibuat dan belum diteruskan.
  // seq naik SETELAH dipakai, jadi siaran ulang di siarkan() memakai
  // nomor yang sama dan penerima mengenalinya sebagai duplikat.
  String paket = buatPaketPesan(NODE_ID, NODE_ID, seqBerikutnya,
                                lat, lon, kualitas, teks);
  seqBerikutnya++;

  siarkan(paket);

  if (kualitas == POS_NOFIX) {
    Serial.println("  peringatan: GPS belum pernah fix, koordinat dikirim 0,0");
  } else if (kualitas == POS_STALE) {
    Serial.printf("  peringatan: posisi berumur %lu detik\n",
                  (millis() - lastFixTime) / 1000);
  }
}

// Siarkan paket ULANG_KIRIM kali. delay() di sini masih bisa diterima
// karena hanya terjadi sesaat saat giliran kirim; kalau nanti mengganggu
// penerimaan, ubah jadi penjadwalan berbasis millis().
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

void terimaPaket() {
  int panjang = LoRa.parsePacket();
  if (panjang == 0) {
    return;
  }

  int   rssi = LoRa.packetRssi();
  float snr  = LoRa.packetSnr();

  String raw;
  while (LoRa.available()) {
    raw += (char)LoRa.read();
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
  Serial.printf("  HOPS   %s\n", tambahHop(p.hops, NODE_ID, rssi, snr).c_str());
  Serial.printf("  POSISI %.6f, %.6f (%s)\n",
                p.lat, p.lon, namaKualitas(p.kualitas));
  Serial.printf("  PESAN  \"%s\"\n", p.teks.c_str());
  Serial.println();
}

// Hanya cetak ke Serial untuk debugging, tidak mengirim paket LoRa.
void laporGPS() {
  static unsigned long terakhirLapor = 0;
  unsigned long now = millis();

  if (now - terakhirLapor < GPS_REPORT_MS) {
    return;
  }
  terakhirLapor = now;

  if (!pernahFix) {
    Serial.printf("GPS: belum fix (%lu satelit terlihat, %lu karakter NMEA)\n",
                  gps.satellites.isValid() ? gps.satellites.value() : 0,
                  gps.charsProcessed());
    return;
  }

  Serial.printf("GPS: %.6f, %.6f | %lu satelit | umur %lu detik\n",
                lastLat, lastLon,
                gps.satellites.isValid() ? gps.satellites.value() : 0,
                (now - lastFixTime) / 1000);
}
