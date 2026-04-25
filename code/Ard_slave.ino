#include <Wire.h>  // Library untuk komunikasi I2C
#include <Servo.h> // Library untuk mengontrol motor servo

Servo myServo;    // Membuat objek servo untuk mengunci/membuka brankas
int systemState = 0; // Status Sistem: 0:Idle, 1:Open, 2:Closing, 3:Denied, 4:Lockdown, 5:Breach
int failCount = 0;   // Menghitung jumlah kesalahan input PIN
volatile bool dataReceived = false; // Flag untuk menandakan data I2C telah diterima
int receivedData[4]; // Array untuk menyimpan angka PIN yang diterima (ter-enkripsi)

// PIN yang benar (setelah didekripsi di sisi slave, nilai asli adalah 1, 2, 3, 4 jika dienkripsi master +5)
// Master mengirim (X + 5), jadi jika input 1, slave terima 6.
const int correctPIN[4] = {6, 7, 8, 9}; 

// Definisi pin komponen
const int p_pir = 2;      // Sensor Gerak PIR
const int p_buzzer = 3;   // Buzzer Aktif
const int p_red = 4;      // LED Merah (Status Terkunci/Lockdown)
const int p_blue = 5;    // LED Biru
const int p_green = 6;   // LED Hijau (Status Terbuka)
const int p_vib = A0;     // Sensor Getaran (Piezo/Vibration)

void setup() {
  Wire.begin(8);                // Memulai I2C sebagai Slave dengan alamat 8
  Wire.onReceive(receiveEvent); // Fungsi yang dipanggil saat menerima data dari Master
  Wire.onRequest(requestEvent); // Fungsi yang dipanggil saat Master meminta data
  
  Serial.begin(9600);           // Serial untuk debugging
  myServo.attach(10);           // Hubungkan servo ke pin 10
  myServo.write(0);             // Posisi awal servo (Terkunci)

  // Konfigurasi Input/Output pin
  pinMode(p_pir, INPUT); 
  pinMode(p_buzzer, OUTPUT);
  pinMode(p_red, OUTPUT); 
  pinMode(p_blue, OUTPUT); 
  pinMode(p_green, OUTPUT);
  
  digitalWrite(p_red, HIGH);    // Nyalakan LED merah saat standby
  Serial.println(F("[BOOT] Security Kernel Active. Ready to validate."));
}

void loop() {
  // Membaca input analog dari sensor getaran
  int vibLevel = analogRead(p_vib);

  // Jika getaran melebihi ambang batas (Percobaan Pembobolan Fisik)
  if (vibLevel > 850) {
    if (systemState != 5) {
      Serial.println(F("[ALARM] Physical Breach in Progress!"));
      systemState = 5; // Ubah status ke Breach (Siaga 1)
    }
    // Bunyi Buzzer frekuensi tinggi selama getaran terdeteksi
    tone(p_buzzer, 1200); 
  } 
  else {
    // Jika sebelumnya terjadi Breach dan sekarang getaran sudah hilang
    if (systemState == 5) {
      noTone(p_buzzer); // Matikan buzzer
      systemState = 0;  // Kembalikan status ke Normal
      Serial.println(F("[ALARM] Area Secured. G-Sensor Normalized."));
    }
  }

  // Proses validasi PIN jika ada data masuk dan tidak sedang dalam kondisi Breach
  if (dataReceived && systemState != 5) {
    validate();         // Panggil fungsi validasi
    dataReceived = false; // Reset flag data
  }
}

// Fungsi yang dipanggil saat Master meminta data (PIR, Status, FailCount)
void requestEvent() {
  byte packet[3] = {
    (byte)((digitalRead(p_pir) == HIGH) ? 1 : 0), // Status PIR
    (byte)systemState,                            // Status Sistem saat ini
    (byte)failCount                               // Jumlah kegagalan PIN
  };
  Wire.write(packet, 3); // Kirim 3 byte balik ke Master
}

// Fungsi untuk memvalidasi PIN yang diterima
void validate() {
  bool match = true;
  // Bandingkan setiap angka yang diterima dengan PIN yang benar
  for (int i = 0; i < 4; i++) {
    if (receivedData[i] != correctPIN[i]) match = false;
  }

  if (match) {
    executeOpen();   // Jika cocok, buka brankas
  } else {
    executeDenied(); // Jika salah, tolak akses
  }
}

// Prosedur membuka brankas
void executeOpen() {
  Serial.println(F("[AUTH] PIN Verified. Access Granted."));
  failCount = 0; // Reset hitungan gagal
  
  // Bunyi bip 2x tanda akses diterima
  for (int i = 0; i < 2; i++) { tone(p_buzzer, 1800, 100); delay(150); }
  
  systemState = 1;          // Status: Membuka
  digitalWrite(p_red, LOW); // Matikan LED merah
  
  // Gerakkan servo perlahan dari 0 ke 90 derajat
  for (int pos = 0; pos <= 90; pos += 5) {
    myServo.write(pos);
    // Kedipkan LED kuning (merah + hijau)
    digitalWrite(p_red, !digitalRead(p_red)); 
    digitalWrite(p_green, !digitalRead(p_green)); 
    delay(50);
  }
  
  digitalWrite(p_red, LOW); // matikan LED merah agar lampu berubah hijau
  delay(5000); // Tunggu 5 detik (sinkron dengan tampilan Master)

  systemState = 2;             // Status: Menutup
  digitalWrite(p_green, LOW);  // Matikan LED hijau
  
  // Kembalikan servo ke posisi 0 (Mengunci)
  for (int pos = 90; pos >= 0; pos -= 5) {
    myServo.write(pos); 
    digitalWrite(p_red, !digitalRead(p_red)); // Kedipkan LED merah
    delay(50);
  }
  
  digitalWrite(p_red, HIGH);    // Nyalakan LED merah (Standby)
  systemState = 0;              // Kembali ke status Idle
}

// Prosedur menolak akses
void executeDenied() {
  failCount++;       // Tambah hitungan kesalahan
  systemState = 3;   // Status: Akses Ditolak
  Serial.print(F("[AUTH] Incorrect PIN. Fails: ")); Serial.println(failCount);
  
  // Buzzer bunyi panjang frekuensi rendah 1x
  tone(p_buzzer, 200, 1500); 
  delay(2000); 

  // Jika sudah salah 3 kali, aktifkan sistem Lockdown
  if (failCount >= 3) {
    systemState = 4; // Status: Lockdown
    Serial.println(F("[SECURITY] Threshold reached. Initiating Lockdown."));
    digitalWrite(p_red, HIGH);
    delay(10000);    // Kunci sistem selama 10 detik
    failCount = 0;   // Reset hitungan setelah lockdown selesai
  }
  systemState = 0;   // Kembali ke status Idle
}

// Fungsi yang dipanggil secara otomatis saat menerima data via I2C
void receiveEvent(int how) {
  // Ambil 4 byte data dari Master (digit PIN)
  for (int i = 0; i < 4; i++) {
    if (Wire.available()) receivedData[i] = Wire.read();
  }
  dataReceived = true; // Set flag bahwa data siap diproses
}

// Fungsi alarm (cadangan jika ingin dipanggil secara spesifik)
void triggerAlarm() {
  Serial.println(F("[ALARM] Physical Breach Detected!"));
  for (int i = 0; i < 5; i++) { 
    tone(p_buzzer, 1000); delay(100); 
    tone(p_buzzer, 600);  delay(100); 
  }
  noTone(p_buzzer);
}