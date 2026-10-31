
#include <WiFi.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <BH1750.h>

// =====================================
// KONFIGURASI WIFI
// =====================================
const char* WIFI_SSID = "NAMA_WIFI";
const char* WIFI_PASSWORD = "PASSWORD_WIFI";

// =====================================
// KONFIGURASI SERVER FASTAPI
// =====================================
// Gunakan IP komputer/server, bukan localhost
const char* API_URL =
  "http://192.168.1.10:8000/api/sensors/data";

// Harus sama dengan SENSOR_API_KEY di backend/.env
const char* DEVICE_KEY = "GANTI_DENGAN_API_KEY";

// ID setiap ruangan
// ESP32 pertama: kelas-1
// ESP32 kedua:   kelas-2
// Sampai kelas-6
const char* DEVICE_ID = "kelas-1";

// =====================================
// KONFIGURASI PIN
// =====================================
#define DS18B20_PIN 4
#define SDA_PIN 21
#define SCL_PIN 22

// =====================================
// OBJEK SENSOR
// =====================================
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);

BH1750 bh1750;

// =====================================
// INTERVAL PENGIRIMAN
// =====================================
unsigned long previousMillis = 0;
const unsigned long interval = 10000;

// =====================================
// WIFI CONNECTION
// =====================================
void connectWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.println("Menghubungkan WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retry = 0;

  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi terhubung");
    Serial.print("IP ESP32: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi gagal terhubung");
  }
}

// =====================================
// BACA SENSOR
// =====================================
bool readSensors(float &temperature, float &lux) {

  // Baca suhu DS18B20
  ds18b20.requestTemperatures();

  temperature = ds18b20.getTempCByIndex(0);

  // Baca intensitas cahaya BH1750
  lux = bh1750.readLightLevel();

  // Validasi pembacaan suhu
  if (temperature == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: DS18B20 tidak terdeteksi");
    return false;
  }

  // Validasi pembacaan cahaya
  if (lux < 0) {
    Serial.println("Error: BH1750 gagal membaca");
    return false;
  }

  return true;
}

// =====================================
// KIRIM DATA KE FASTAPI
// =====================================
void sendSensorData(float temperature, float lux) {

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Data tidak dikirim: WiFi terputus");
    return;
  }

  HTTPClient http;

  http.setTimeout(5000);
  http.begin(API_URL);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", DEVICE_KEY);

  // Format JSON
  String payload = "{";

  payload += "\"device_id\":\"";
  payload += DEVICE_ID;
  payload += "\",";

  payload += "\"temperature\":";
  payload += String(temperature, 2);
  payload += ",";

  payload += "\"light_level\":";
  payload += String(lux, 2);

  payload += "}";

  Serial.println("Mengirim data:");
  Serial.println(payload);

  int httpCode = http.POST(payload);

  if (httpCode > 0) {

    Serial.print("HTTP Response: ");
    Serial.println(httpCode);

    Serial.println(http.getString());

  } else {

    Serial.print("Gagal mengirim: ");
    Serial.println(http.errorToString(httpCode));

  }

  http.end();
}

// =====================================
// SETUP
// =====================================
void setup() {

  Serial.begin(115200);

  Serial.println();
  Serial.println("ESP32 ROOM MONITORING");
  Serial.println("=====================");

  // Inisialisasi DS18B20
  ds18b20.begin();

  // Inisialisasi I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // Inisialisasi BH1750
  if (!bh1750.begin(
        BH1750::CONTINUOUS_HIGH_RES_MODE,
        0x23,
        &Wire)) {

    Serial.println("BH1750 tidak ditemukan!");
  } else {

    Serial.println("BH1750 siap");
  }

  // Hubungkan WiFi
  connectWiFi();
}

// =====================================
// LOOP
// =====================================
void loop() {

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {

    previousMillis = currentMillis;

    float temperature;
    float lux;

    Serial.println();
    Serial.println("Membaca sensor...");

    if (readSensors(temperature, lux)) {

      Serial.print("Device: ");
      Serial.println(DEVICE_ID);

      Serial.print("Suhu: ");
      Serial.print(temperature);
      Serial.println(" C");

      Serial.print("Cahaya: ");
      Serial.print(lux);
      Serial.println(" Lux");

      sendSensorData(temperature, lux);

    } else {

      Serial.println("Pembacaan sensor gagal");

    }
  }
}
