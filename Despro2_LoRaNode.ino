#include <SPI.h>
#include <LoRa.h>
//deklarasi node iD, sesuaikan dengan node
#define NODE_ID "01"

// deklarasi pin sesuai rangkaian fisik kalian
#define LORA_SCK  18
#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 26

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 2000;

void setup() {
  Serial.begin(115200);

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init gagal, cek wiring!");
    while (1);
  }//check jika inisialisasi 433Mhz gagal
}

void loop() {
    // ---- BAGIAN TERIMA ----
  int to_read = LoRa.parsePacket(); //read paket masuk
  if (to_read != 0) {
    int rssi = LoRa.packetRssi(); //jika paket masuk, baca kuat sinyalnya

    String message = ""; 
    while (to_read != 0) {
      char byte_read = (char)LoRa.read();
      message += byte_read;
      to_read--;
    } //loop untuk menyimpan isi paket yang masuk dalam satu variabel (String)

    Serial.print("Received: "); //mencetak pesan dari paket dan sinyal rssi
    Serial.println(message);
    Serial.print("RSSI: ");
    Serial.println(rssi);
  }

  // ---- BAGIAN KIRIM ----
  unsigned long now = millis(); //set timer/batasan dalam mengirim pesan karena kedua node saling menerima dan mengirim
  if (now - lastSendTime >= sendInterval) {
    lastSendTime = now;

    LoRa.beginPacket();
    LoRa.print("HELLO FROM NODE "); //mengirim pesan "Hello" dengan node id
    LoRa.print(NODE_ID);
    LoRa.endPacket();

    Serial.println("Sent: HELLO FROM NODE " + String(NODE_ID));
  }
}
