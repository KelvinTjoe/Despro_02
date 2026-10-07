#!/usr/bin/env python3
"""
Data logger Despro2: membaca paket LoRa dari Command Node lewat USB serial
dan menyimpannya sebagai CSV di folder yang disinkronkan Google Drive.

Alur:
  Field/Relay Node memancar
    -> Command Node (ESP32) menerima, menyaring, mencetak baris "DATA,..."
    -> script ini menambahkan waktu laptop dan menulis ke CSV
    -> Google Drive for desktop meng-upload file itu sendiri

Yang ditulis ke file HANYA baris berawalan "DATA,". Semua pesan lain dari
Command Node (DITERIMA, POSISI, RUSAK, ASING, GEMA, DUPLIKAT, TERKIRIM, ...)
ditampilkan di konsol sebagai pemantauan tapi tidak masuk file.

Kolom hops dari Command Node langsung dipecah menjadi satu kolom per nilai
(hop1_dari, hop1_ke, hop1_rssi, hop1_snr, hop2_..., lihat MAKS_HOP), jadi
RSSI dan SNR setiap hop masing-masing punya kolom sendiri di CSV.

Satu kali script dijalankan = satu file (data_YYYY-MM-DD_HH-MM-SS.csv).
Setiap baris langsung ditulis ke disk, jadi kalau script dihentikan
(Ctrl+C, jendela ditutup, atau laptop mati) data yang sudah masuk tetap
tersimpan. Menjalankan script lagi selalu membuat file baru.

Jalankan:  python tools/logger.py
Pilih port lain:  python tools/logger.py --port COM7
Lihat daftar port:  python tools/logger.py --daftar-port
"""

import argparse
import csv
import os
import re
import sys
import time
from datetime import datetime

import serial
import serial.tools.list_ports

# ============================================================
# PENGATURAN. Ubah di sini saja.
# ============================================================

PORT = "COM9"
BAUD = 115200

# Folder ini sudah disinkronkan ke Google Drive lewat Google Drive for
# desktop, jadi script cukup menulis file biasa ke sini.
FOLDER_TUJUAN = r"C:\Users\ruben\Documents\Despro 2\DatasetDespro2"

# Penanda baris dari Command Node. Harus sama dengan yang dicetak firmware
# di MainCode/LoraCommandNode.cpp.
AWALAN_DATA = "DATA,"
AWALAN_META = "META,"

# Jumlah kolom pada baris DATA:
#   sourceID,lastHopID,seq,lat,lon,jumlah_hop,hops,teks
JUMLAH_KOLOM_DATA = 8

# Jumlah kelompok kolom hop di file (hop1_..., hop2_..., dst). Header CSV
# ditulis sekali di awal file, jadi jumlahnya harus tetap. Jalur sekarang
# field -> relay -> command = 2 hop; sisanya cadangan kalau relay ditambah.
# Paket yang hop-nya lebih banyak dari ini ditolak dengan pesan di konsol.
MAKS_HOP = 3

JEDA_RECONNECT_DETIK = 3

# Satu file per sesi: {waktu} diisi waktu script dijalankan, jadi setiap kali
# script dijalankan ulang selalu menghasilkan file baru.
POLA_NAMA_FILE = "data_{waktu}.csv"

# Tampilkan baris non-DATA dari Command Node di konsol. Matikan kalau ingin
# layar bersih dan hanya melihat data yang tercatat.
TAMPILKAN_BARIS_LAIN = True

# ============================================================

# Kolom hops dari Command Node (penerima:rssi:snr per hop, dipisah ';')
# tidak ditulis apa adanya, tapi dipecah menjadi satu kolom per nilai.
# Pengirim tiap hop tidak dikirim firmware dan dihitung di sini: pengirim
# hop pertama = sourceID, pengirim hop berikutnya = penerima hop sebelumnya.
KOLOM_HOP = []
for _i in range(1, MAKS_HOP + 1):
    KOLOM_HOP += [f"hop{_i}_dari", f"hop{_i}_ke", f"hop{_i}_rssi", f"hop{_i}_snr"]

HEADER = (
    ["waktu_laptop", "sourceID", "lastHopID", "seq", "lat", "lon", "jumlah_hop"]
    + KOLOM_HOP
    + ["teks", "teks_utuh"]
)

# Pola teks siaran periodik field node. teks_utuh = 0 kalau siaran tidak
# sesuai pola: tanda isi paket rusak di udara tapi lolos karena paket tidak
# memakai CRC, sehingga angka lain di baris yang sama ikut patut dicurigai.
POLA_SIARAN = re.compile(r"^siaran #\d+, \d+ satelit, (FIX|STALE|NOFIX)$")


def periksa_hops(nilai):
    """Pastikan tiap entri hops berbentuk penerima:rssi:snr yang berupa angka.

    Melempar ValueError kalau tidak, sama seperti int() dan float(), supaya
    baris yang rusak ditolak lewat jalur yang sama.
    """
    for entri in nilai.split(";"):
        penerima, rssi, snr = entri.split(":")  # ValueError kalau bukan 3 bagian
        int(penerima)
        int(rssi)
        float(snr)


# Pemeriksa tiap kolom baris DATA, dipakai untuk menolak baris yang
# bentuknya benar tapi isinya bukan angka (misal potongan pesan debug yang
# kebetulan mengandung koma). Kolom terakhir (teks) bebas, jadi tidak
# diperiksa.
PEMERIKSA_KOLOM = [int, int, int, float, float, int, periksa_hops]

PARAMETER_BELUM_ADA = (
    "belum diterima dari command node saat file ini dibuat; "
    "firmware mengulang baris META tiap 60 detik"
)


def daftar_port():
    """Tampilkan port serial yang terdeteksi beserta deskripsinya.

    Nomor COM di Windows berpindah antar board, dan proyek ini memakai tiga
    board ESP32. Jadi ketika port tujuan tidak ada, lebih berguna
    memperlihatkan apa yang memang terpasang daripada sekadar pesan error.
    """
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("  (tidak ada port serial terdeteksi sama sekali)")
        return
    for p in ports:
        print(f"  {p.device:<8} {p.description}")
        print(f"           {p.hwid}")


class PenulisSesi:
    """Mengelola satu file CSV untuk satu kali script dijalankan.

    Nama file ditentukan sekali saat script mulai. File baru benar-benar
    dibuat ketika baris DATA pertama masuk, supaya sesi yang tidak menerima
    data apa pun tidak meninggalkan file kosong. Kalau USB terputus lalu
    tersambung lagi dalam sesi yang sama, data tetap masuk ke file yang sama.
    """

    def __init__(self, folder):
        self.folder = folder
        self.path = self._buat_path()
        self.file = None
        self.writer = None
        self.sudah_dibuat = False

        # Parameter radio dari baris META. Dicatat sebagai baris komentar
        # pertama file: tanpa parameter radio, nilai RSSI dan SNR di file
        # tidak bisa dibandingkan antar sesi pengujian.
        self.parameter = None
        self.parameter_tertulis = False

        self.jumlah_tercatat = 0
        self.jumlah_diabaikan = 0

        os.makedirs(self.folder, exist_ok=True)

    # ---------- parameter radio ----------

    def set_parameter(self, teks):
        if teks == self.parameter:
            return
        self.parameter = teks
        print(f"[parameter radio] {teks}")

        # File hari ini sudah terbuka dengan baris placeholder: tambahkan
        # parameter sebagai komentar begitu diterima, supaya kondisi
        # pengujian tetap tercatat walau script dijalankan lebih dulu.
        if self.file is not None and not self.parameter_tertulis:
            self.file.write(f"# PARAMETER: {teks}\n")
            self._paksa_ke_disk()
            self.parameter_tertulis = True

    # ---------- file ----------

    def _buat_path(self):
        waktu = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        path = os.path.join(self.folder, POLA_NAMA_FILE.format(waktu=waktu))
        # Jaga-jaga kalau script dijalankan dua kali di detik yang sama:
        # jangan pernah menulis ke file sesi lain.
        nomor = 2
        while os.path.exists(path):
            nama = POLA_NAMA_FILE.format(waktu=f"{waktu}_{nomor}")
            path = os.path.join(self.folder, nama)
            nomor += 1
        return path

    def _pastikan_file(self):
        if self.file is not None:
            return

        self.file = open(self.path, "a", newline="", encoding="utf-8")
        self.writer = csv.writer(self.file, lineterminator="\n")

        if not self.sudah_dibuat:
            self.file.write(f"# PARAMETER: {self.parameter or PARAMETER_BELUM_ADA}\n")
            self.parameter_tertulis = self.parameter is not None
            self.writer.writerow(HEADER)
            self._paksa_ke_disk()
            self.sudah_dibuat = True
            print(f"[file baru] {self.path}")
        else:
            # Dibuka lagi setelah USB tersambung kembali dalam sesi yang sama.
            print(f"[lanjut file] {self.path}")
            if self.parameter is not None and not self.parameter_tertulis:
                self.file.write(f"# PARAMETER: {self.parameter}\n")
                self._paksa_ke_disk()
                self.parameter_tertulis = True

    def tulis(self, kolom):
        self._pastikan_file()
        self.writer.writerow(kolom)
        self._paksa_ke_disk()
        self.jumlah_tercatat += 1

    def _paksa_ke_disk(self):
        """Tulis benar-benar ke disk, jangan ditahan di buffer.

        Dua alasan: data tidak hilang kalau script ditutup paksa atau laptop
        mati, dan Google Drive for desktop baru melihat perubahan setelah
        file benar-benar berubah di disk.
        """
        self.file.flush()
        os.fsync(self.file.fileno())

    def tutup(self):
        if self.file is not None:
            self.file.close()
            self.file = None
            self.writer = None


def olah_baris(teks, penulis):
    """Proses satu baris dari serial."""
    if not teks:
        return

    if teks.startswith(AWALAN_META):
        penulis.set_parameter(teks[len(AWALAN_META):].strip())
        return

    if not teks.startswith(AWALAN_DATA):
        if TAMPILKAN_BARIS_LAIN:
            print(f"  . {teks}")
        return

    isi = teks[len(AWALAN_DATA):]

    # maxsplit dipakai supaya kolom teks yang berada paling akhir boleh
    # mengandung koma tanpa merusak pemisahan field.
    kolom = isi.split(",", JUMLAH_KOLOM_DATA - 1)
    if len(kolom) != JUMLAH_KOLOM_DATA:
        penulis.jumlah_diabaikan += 1
        print(f"  ! baris DATA jumlah kolomnya {len(kolom)}, diabaikan: {teks}")
        return

    for nilai, tipe in zip(kolom, PEMERIKSA_KOLOM):
        try:
            tipe(nilai)
        except ValueError:
            penulis.jumlah_diabaikan += 1
            print(f"  ! baris DATA isinya bukan angka, diabaikan: {teks}")
            return

    source, last_hop, seq, lat, lon, jumlah_hop, hops, isi_teks = kolom
    entri_hop = hops.split(";")
    if len(entri_hop) > MAKS_HOP:
        penulis.jumlah_diabaikan += 1
        print(f"  ! paket {len(entri_hop)} hop melebihi MAKS_HOP {MAKS_HOP}, "
              f"naikkan MAKS_HOP di logger.py. Diabaikan: {teks}")
        return

    nilai_hop = []
    dari = source
    for entri in entri_hop:
        ke, rssi, snr = entri.split(":")
        nilai_hop += [dari, ke, rssi, snr]
        dari = ke
    nilai_hop += [""] * (len(KOLOM_HOP) - len(nilai_hop))

    utuh = 0 if isi_teks.startswith("siaran") and not POLA_SIARAN.match(isi_teks) else 1

    waktu = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
    penulis.tulis([waktu, source, last_hop, seq, lat, lon, jumlah_hop]
                  + nilai_hop + [isi_teks, utuh])

    print(
        f"  + {waktu}  node {source} (lastHop {last_hop}) seq {seq}  "
        f"{lat},{lon}  {jumlah_hop} hop [{hops}]  \"{isi_teks}\""
        + ("" if utuh else "  (teks rusak)")
    )


def baca_sampai_terputus(port, baud, penulis):
    """Buka port dan baca sampai kabel dicabut atau terjadi error."""
    print(f"[menyambung] {port} @ {baud} baud")
    with serial.Serial(port, baud, timeout=1) as ser:
        print(f"[tersambung] {port}, menunggu data. Ctrl+C untuk berhenti.")
        while True:
            mentah = ser.readline()
            if not mentah:
                continue  # timeout 1 detik, bukan error
            # errors="replace" supaya satu byte kacau karena derau USB tidak
            # mematikan logger.
            olah_baris(mentah.decode("utf-8", errors="replace").strip(), penulis)


def main():
    ap = argparse.ArgumentParser(
        description="Logger paket LoRa Despro2 dari Command Node ke CSV."
    )
    ap.add_argument("--port", default=PORT, help=f"port serial (bawaan {PORT})")
    ap.add_argument("--baud", type=int, default=BAUD, help=f"baud rate (bawaan {BAUD})")
    ap.add_argument("--folder", default=FOLDER_TUJUAN, help="folder tujuan file CSV")
    ap.add_argument(
        "--daftar-port",
        action="store_true",
        help="tampilkan port serial yang terdeteksi lalu keluar",
    )
    arg = ap.parse_args()

    if arg.daftar_port:
        print("Port serial terdeteksi:")
        daftar_port()
        return 0

    print(f"Folder tujuan : {arg.folder}")
    print(f"Port          : {arg.port} @ {arg.baud} baud")
    print("Ingat: serial monitor PlatformIO harus DITUTUP, satu port tidak")
    print("bisa dipakai dua program sekaligus.\n")

    penulis = PenulisSesi(arg.folder)
    print(f"File sesi ini: {penulis.path}")
    print("(file dibuat saat baris DATA pertama masuk)\n")
    pernah_tersambung = False

    try:
        while True:
            try:
                baca_sampai_terputus(arg.port, arg.baud, penulis)
                pernah_tersambung = True
            except serial.SerialException as e:
                # Dua kasus: port belum pernah ada (board belum dicolok, atau
                # nomor COM-nya lain), dan port hilang di tengah jalan (kabel
                # dicabut). Keduanya ditangani sama: tunggu lalu coba lagi.
                print(f"\n[terputus] {e}")
                if not pernah_tersambung:
                    print("Port serial yang terdeteksi sekarang:")
                    daftar_port()
                    print(
                        "Kalau Command Node ada di port lain, jalankan dengan "
                        "--port COMx"
                    )
            except OSError as e:
                print(f"\n[error perangkat] {e}")

            # Tutup file saat terputus. Google Drive tidak selalu meng-upload
            # file yang masih dibuka program lain.
            penulis.tutup()
            print(f"[menunggu] mencoba lagi dalam {JEDA_RECONNECT_DETIK} detik\n")
            time.sleep(JEDA_RECONNECT_DETIK)

    except KeyboardInterrupt:
        print("\n[berhenti] dihentikan dari keyboard")
    finally:
        penulis.tutup()
        print(
            f"[ringkasan] {penulis.jumlah_tercatat} baris tercatat, "
            f"{penulis.jumlah_diabaikan} baris diabaikan"
        )
        if penulis.sudah_dibuat:
            print(f"[tersimpan] {penulis.path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
