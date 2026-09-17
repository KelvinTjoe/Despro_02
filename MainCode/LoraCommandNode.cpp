#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

#include "config.h"
#include "packet.h"

#define NODE_ID  ID_COMMAND

void terimaPaket();
void bacaInputOperator();
void kirimPesan(const String &teks);

String bufferInput;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.printf("=== COMMAND NODE (id %d) ===\n", NODE_ID);

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

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

  Serial.println("LoRa: OK");

#if PAKSA_LEWAT_RELAY
  Serial.printf("MODE UJI: hanya menerima paket via relay (id %d).\n", ID_RELAY);
  Serial.println("          paket langsung dari field akan DITOLAK.");
  Serial.println("          ubah PAKSA_LEWAT_RELAY jadi 0 di config.h untuk normal.");
#else
  Serial.println("MODE NORMAL: paket langsung maupun via relay diterima.");
#endif

  Serial.println("Ketik pesan lalu tekan Enter untuk menyiarkan ke lapangan.");
  Serial.println();
}

void loop() {
  terimaPaket();
  bacaInputOperator();
}

void terimaPaket() {
  int panjang = LoRa.parsePacket();
  if (panjang == 0) {
    return;
  }

  int rssi = LoRa.packetRssi();

  String raw;
  while (LoRa.available()) {
    raw += (char)LoRa.read();
  }

  Paket p;
  if (!parsePaket(raw, p)) {
    Serial.printf("RUSAK  : \"%s\" (RSSI %d)\n", raw.c_str(), rssi);
    return;
  }

  if (!nodeDikenal(p.sourceID)) {
    Serial.printf("ASING  : sourceID %d tidak dikenal (RSSI %d)\n", p.sourceID, rssi);
    return;
  }

  // Abaikan gema pesan sendiri yang dipantulkan relay.
  if (p.sourceID == NODE_ID) {
    return;
  }

#if PAKSA_LEWAT_RELAY
  // Mode uji jarak dekat: paksa paket menempuh 2 hop. Paket yang tiba
  // langsung dari field node ditolak walaupun sinyalnya bagus.
  if (p.lastHopID != ID_RELAY) {
    Serial.printf("DITOLAK: paket langsung dari node %d (lastHop %d, RSSI %d)\n",
                  p.sourceID, p.lastHopID, rssi);
    return;
  }
#endif

  Serial.printf("DITERIMA: dari node %d via node %d, RSSI %d dBm\n",
                p.sourceID, p.lastHopID, rssi);

  if (p.tipe == TIPE_STATUS) {
    Serial.printf("  STATUS %s\n", namaStatus(p.status));
    Serial.printf("  POSISI %.6f, %.6f (%s)\n",
                  p.lat, p.lon, namaKualitas(p.kualitas));
    if (p.kualitas == POS_NOFIX) {
      Serial.println("  perhatian: personel belum dapat fix GPS");
    } else if (p.kualitas == POS_STALE) {
      Serial.println("  perhatian: posisi sudah lama tidak diperbarui");
    }
  } else if (p.tipe == TIPE_PESAN) {
    Serial.printf("  PESAN  \"%s\"\n", p.teks.c_str());
  }

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
  String paket = buatPaketPesan(NODE_ID, NODE_ID, teks);

  LoRa.beginPacket();
  LoRa.print(paket);
  LoRa.endPacket();

  Serial.printf("TERKIRIM: %s\n\n", paket.c_str());
}
