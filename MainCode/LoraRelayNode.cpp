#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

#include "config.h"
#include "packet.h"

#define NODE_ID  ID_RELAY

void terimaDanTeruskan();
void tampilkanPaket(const Paket &p, int rssi);

unsigned long jumlahDiteruskan = 0;
unsigned long jumlahDitolak = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.printf("=== RELAY NODE (id %d) ===\n", NODE_ID);

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

  // Seed acak dari noise ADC, supaya backoff tiap board tidak seragam.
  randomSeed(analogRead(A0) + millis());

  Serial.println("LoRa: OK, menunggu paket untuk diteruskan");
}

void loop() {
  terimaDanTeruskan();
}

void terimaDanTeruskan() {
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
    jumlahDitolak++;
    Serial.printf("RUSAK   : \"%s\" (RSSI %d)\n", raw.c_str(), rssi);
    return;
  }

  // Filter 1: whitelist, tolak perangkat asing
  if (!nodeDikenal(p.sourceID)) {
    jumlahDitolak++;
    Serial.printf("ASING   : sourceID %d tidak dikenal (RSSI %d)\n", p.sourceID, rssi);
    return;
  }

  // Filter 2: anti-loop, jangan teruskan paket siaran sendiri.
  // Hari ini mustahil terjadi karena hanya ada satu relay, tapi baris
  // ini yang mencegah banjir paket begitu relay kedua ditambahkan.
  if (p.lastHopID == NODE_ID) {
    jumlahDitolak++;
    Serial.printf("SIARAN SENDIRI: diabaikan (RSSI %d)\n", rssi);
    return;
  }

  tampilkanPaket(p, rssi);

  // Jeda acak sebelum menyiarkan ulang. Kalau nanti ada dua relay yang
  // menerima paket yang sama bersamaan, tanpa jeda ini keduanya akan
  // menyiarkan di saat yang sama dan saling menghancurkan.
  unsigned long jeda = random(BACKOFF_MIN_MS, BACKOFF_MAX_MS);
  delay(jeda);

  String diteruskan = gantiLastHop(raw, NODE_ID);

  LoRa.beginPacket();
  LoRa.print(diteruskan);
  LoRa.endPacket();

  jumlahDiteruskan++;
  Serial.printf("TERUSKAN: %s   (jeda %lu ms, total %lu)\n\n",
                diteruskan.c_str(), jeda, jumlahDiteruskan);
}

void tampilkanPaket(const Paket &p, int rssi) {
  Serial.printf("DITERIMA: dari node %d, lastHop %d, RSSI %d dBm\n",
                p.sourceID, p.lastHopID, rssi);

  if (p.tipe == TIPE_STATUS) {
    Serial.printf("  STATUS %s | %.6f, %.6f | %s\n",
                  namaStatus(p.status), p.lat, p.lon, namaKualitas(p.kualitas));
  } else if (p.tipe == TIPE_PESAN) {
    Serial.printf("  PESAN  \"%s\"\n", p.teks.c_str());
  }
}
