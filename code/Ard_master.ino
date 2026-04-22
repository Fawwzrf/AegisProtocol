#include <Wire.h>              // Library untuk komunikasi I2C
#include <Adafruit_LiquidCrystal.h> // Library untuk mengontrol LCD melalui I2C atau pengaya
#include <Keypad.h>            // Library untuk antarmuka keypad 4x4

// Inisialisasi LCD pada alamat 0
Adafruit_LiquidCrystal lcd(0);

// Konfigurasi Baris dan Kolom Keypad
const byte ROWS = 4; // Empat baris
const byte COLS = 4; // Empat kolom
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// Hubungkan pin baris dan kolom keypad ke pin digital Arduino
byte rowPins[ROWS] = {2, 3, 4, 5}; 
byte colPins[COLS] = {6, 7, 8, 9}; 

// Inisialisasi objek keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Variabel sistem
String inputPIN = "";      // Menyimpan input PIN dari pengguna
bool isAwake = false;      // Status apakah sistem sedang aktif (bangun)
int lastState = 0;         // Menyimpan status terakhir sistem untuk deteksi perubahan

void setup() {
  Serial.begin(9600);      // Memulai komunikasi serial untuk debugging
  delay(500);
  Wire.begin();            // Memulai komunikasi I2C sebagai Master
  lcd.begin(16, 2);        // Inisialisasi LCD 16x2
  lcd.setBacklight(LOW);   // Matikan lampu latar saat awal (mode hemat daya)
  
  Serial.println(F("[BOOT] Master Terminal Initialized."));
  Serial.println(F("[BOOT] I2C Scanner: Searching for Security Kernel..."));
}

void loop() {
  // Meminta 3 byte data dari Slave (alamat 8): [0]=PIR, [1]=Status Sistem, [2]=Jumlah Gagal
  Wire.requestFrom(8, 3);
  if (Wire.available() >= 3) {
    byte pirStatus = Wire.read();   // Membaca status sensor PIR dari slave
    byte currentState = Wire.read(); // Membaca status operasional sistem
    byte failCount = Wire.read();    // Membaca jumlah kesalahan input PIN
    
    // Jika PIR mendeteksi gerakan dan sistem masih mati
    if (pirStatus == 1 && !isAwake) {
      isAwake = true;
      lcd.setBacklight(HIGH);        // Nyalakan layar
      Serial.println(F("[PIR] Motion Detected: System Waking Up."));
      showIdle();                    // Tampilkan menu utama
    }

    // Jika terjadi perubahan status pada slave, perbarui tampilan LCD master
    if (currentState != lastState) {
      handleStateChange(currentState, failCount);
    }
    lastState = currentState;
  }

  // Jika sistem aktif dan dalam kondisi Idle (0) atau Access Denied (3), baca keypad
  if (isAwake && (lastState == 0 || lastState == 3)) {
    char key = keypad.getKey();
    if (key) handleKeypad(key);      // Proses tombol yang ditekan
  }
}

// Fungsi untuk menangani perubahan tampilan berdasarkan status dari Slave
void handleStateChange(byte state, byte fails) {
  if (state == 5) { // Status: Physical Breach (Terjadi Getaran/Paksaan)
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("!! WARNING !!");
    lcd.setCursor(0, 1); lcd.print("PHYSICAL BREACH");
    // Backlight berkedip untuk efek alarm visual
    lcd.setBacklight(millis() % 400 < 200 ? HIGH : LOW); 
  }
  else if (state == 1) { // Status: PIN Benar, Brankas Terbuka
    lcd.setBacklight(HIGH);
    lcd.clear(); lcd.print("BRANKAS OPEN!");
    // Hitung mundur penutupan brankas
    for (int i = 5; i > 0; i--) {
      lcd.setCursor(0, 1); lcd.print("Closing in: "); lcd.print(i); lcd.print("s ");
      delay(1000);
    }
  } 
  else if (state == 2) { // Status: Sedang Menutup
    lcd.setBacklight(HIGH);
    lcd.clear(); lcd.print("BRANKAS CLOSED");
    delay(2000); showIdle();
  } 
  else if (state == 3) { // Status: Akses Ditolak (PIN Salah)
    lcd.clear(); lcd.print("ACCESS DENIED");
    lcd.setCursor(0, 1); lcd.print("Try: "); lcd.print(fails); lcd.print("/3");
    delay(2000); showIdle();
  } 
  else if (state == 4) { // Status: Lockdown (Terlalu banyak salah PIN)
    lcd.clear(); lcd.print("LOCKDOWN!");
    for (int i = 10; i > 0; i--) {
      lcd.setCursor(0, 1); lcd.print("Wait: "); lcd.print(i); lcd.print("s ");
      delay(1000);
    }
    showIdle();
  }
  else if (state == 0) { // Status: Kembali ke Siaga
    lcd.setBacklight(HIGH);
    showIdle();
  }
}

// Fungsi untuk memproses input dari Keypad
void handleKeypad(char key) {
  if (key == '#') { // Tombol '#' digunakan untuk konfirmasi/kirim PIN
    Serial.print(F("[I2C] Transmitting Cipher to Slave... "));
    sendEncryptedData(inputPIN); // Kirim PIN yang sudah dienkripsi sederhana ke Slave
    inputPIN = "";               // Reset input
  } else if (key == '*') { // Tombol '*' digunakan untuk membatalkan/reset input
    inputPIN = ""; showIdle();
  } else if (inputPIN.length() < 4) { // Memasukkan angka PIN (maksimal 4 digit)
    inputPIN += key;
    // Tampilkan '*' di LCD untuk keamanan
    lcd.setCursor(5 + inputPIN.length() - 1, 1); lcd.print('*');
  }
}

// Fungsi menampilkan teks awal "ENTER PIN"
void showIdle() {
  lcd.clear(); lcd.print("ENTER PIN:");
  lcd.setCursor(0, 1); lcd.print("PIN: ");
}

// Fungsi enkripsi sederhana dan pengiriman data ke Slave melalui I2C
void sendEncryptedData(String pin) {
  Wire.beginTransmission(8); // Mulai transmisi ke alamat Slave 8
  for (int i = 0; i < 4; i++) {
    // Enkripsi Caesar Cipher sederhana: (nilai angka + 5)
    Wire.write((pin[i] - '0') + 5);
  }
  Wire.endTransmission();     // Akhiri transmisi
  Serial.println(F("Done."));
}