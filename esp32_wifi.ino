#include <WiFi.h>
#include <FirebaseESP32.h>

// Hostspot 
#define WIFI_SSID     "Renard"
#define WIFI_PASSWORD "idk12356"

// Firebase DB URL
#define FIREBASE_HOST "embeddedenr-default-rtdb.firebaseio.com"
// Legacy database secret key
#define FIREBASE_AUTH "3fj0TZr1ZbvRulTBVb1yF40XccXTtxI0BVYWcU0a"

FirebaseData firebaseData;
FirebaseConfig config;
FirebaseAuth auth;

// ESP32 pins used to connect to STM32 USART2
#define RXD2 16   // ESP32 RX2 -> STM32 TX
#define TXD2 17   // ESP32 TX2 (unused if STM32 only sends)

// BAUD RATE
#define STM32_BAUD 115200

// Thresholds 
#define VAC_ON_THRESHOLD 100.0f   // vacuum ON current threshold
#define LED_ON_THRESHOLD  10.0f   // LED ON current threshold

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("ESP32 Energy Monitor Bridge");
  Serial.println("---------------------------");

  // Start UART that listens to STM32
  Serial2.begin(STM32_BAUD, SERIAL_8N1, RXD2, TXD2);
  Serial.print("Serial2 started at baud ");
  Serial.println(STM32_BAUD);

  // Wi-Fi connection
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("Connected via IP: ");
  Serial.println(WiFi.localIP());

  // Firebase configuration
  // connection to firebase database
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("Firebase initialized.");
}

// Main loop
void loop() {
  // loop for stm32 read
  static unsigned long lastBeat = 0;
  if (millis() - lastBeat > 3000) {
    lastBeat = millis();
    Serial.println("Loop alive, waiting for STM32...");
  }

  // byte readings
  if (Serial2.available()) {
    
    String line = Serial2.readStringUntil('\n');
    line.trim(); 

    if (line.length() == 0) {
      Serial.println("Received empty line from STM32, ignoring.");
      return;
    }
    Serial.print("RAW from STM32: ");
    Serial.println(line);

    // Parse 
    int commaIndex = line.indexOf(',');
    if (commaIndex < 0) {
      Serial.println("Parse error");
      return;
    }

    String vacStr = line.substring(0, commaIndex);
    String ledStr = line.substring(commaIndex + 1);
    vacStr.trim();
    ledStr.trim();

    float vacCurr = vacStr.toFloat();
    float ledCurr = ledStr.toFloat();

    Serial.print("Vacuum current (mA): ");
    Serial.println(vacCurr);
    Serial.print("LED current (mA): ");
    Serial.println(ledCurr);

    // ON/OFF state thresholds
    int vacStat = (vacCurr > VAC_ON_THRESHOLD) ? 1 : 0;
    int ledStat = (ledCurr > LED_ON_THRESHOLD) ? 1 : 0;

    Serial.print("Vacuum status: ");
    Serial.println(vacStat ? "ON" : "OFF");
    Serial.print("LED status: ");
    Serial.println(ledStat ? "ON" : "OFF");

    FirebaseJson json;
    json.set("vac_curr", vacCurr);
    json.set("vac_stat", vacStat);
    json.set("led_curr", ledCurr);
    json.set("led_stat", ledStat);

    // Send JSON to Firebase
    if (Firebase.setJSON(firebaseData, "/live_readings", json)) {
      Serial.println("Sent to Firebase successfully!");
    } else {
      Serial.print("Firebase Error: ");
      Serial.println(firebaseData.errorReason());
    }
  }
}
