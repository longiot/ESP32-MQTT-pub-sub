#include <WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ============== CẤU HÌNH ==============
const int arletLED = 2;

const char* ssid = "GREENFEED Guest";
const char* password = "";

const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "GF/ESP32/DATA";
const char* mqtt_sub = "esp32/control";

// ============== THÔNG SỐ CẢM BIẾN ==============
float Temp = 27;
float DO = 7.0;
float pH = 7.5;
bool ledStatus = false;

// Hàm cập nhật nhiệt độ giả lập
void updateSimulatedTemp() {
  // Tạo sự thay đổi nhỏ ngẫu nhiên (-0.5 đến +0.5)
  float change = (random(-50, 51)) / 100.0;
  Temp += change;
  
  // Giới hạn nhiệt độ trong khoảng 24-30
  if (Temp > 30) Temp = 30;
  if (Temp < 24) Temp = 24;
}

// ============== MQTT CLIENT ==============
WiFiClient espClient;
PubSubClient client(espClient);

// ============== TASK OTA CORE 0 ==============
// void otaTask(void *pvParameters) {
//   for (;;) {
//     ArduinoOTA.handle();  // xử lý OTA
//     vTaskDelay(10 / portTICK_PERIOD_MS);  // 10ms
//   }
// }

// ============== WIFI ==============
void WIFI_setup() {
  Serial.begin(115200);
  pinMode(arletLED, OUTPUT);
  digitalWrite(arletLED, LOW);

  Serial.print("WiFi Connecting...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print(WiFi.localIP());
  Serial.println(" Connected!");
}

// ============== MQTT ==============
void connectMQTT() {
  while (!client.connected()) {
    Serial.print("MQTT Connecting...");
    String clientId = "ESP32-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("OK");

      client.subscribe(mqtt_sub, 1);
      Serial.print("Subscribed to: ");
      Serial.println(mqtt_sub);

    } else {
      Serial.print("fail: ");
      Serial.print(client.state());
      Serial.println(" retry in 5s");
      delay(5000);
    }
  }
}

void sendData() {
  StaticJsonDocument<256> doc;
  doc["device"] = "ESP32_01";
  doc["temperature"] = Temp;
  doc["dissolvedOxygen"] = DO;
  doc["pH"] = pH;
  doc["ledStatus"] = ledStatus;

  char jsonBuffer[256];
  size_t n = serializeJson(doc, jsonBuffer);

  if (client.publish(mqtt_topic, jsonBuffer)) {
    Serial.print("Sent JSON (");
    Serial.print(n);
    Serial.print(" bytes): ");
    Serial.println(jsonBuffer);
  } else {
    Serial.println("Send failed!");
  }
}

void callback(char* topic, byte* payload, unsigned int length) {             
  Serial.print("Data nhận được từ Topic: ");
  Serial.println(topic);

  // 1. Chuyển đổi Payload (data) sang String
  String dataString = "";
  for (int i = 0; i < length; i++) {
    dataString += (char)payload[i];
  }
  
  Serial.print("Payload: ");
  Serial.println(dataString);

  // 2. LOGIC XỬ LÝ ON/OFF
  if (strcmp(topic, "esp32/control") == 0) {
    if (dataString == "ON") {
      digitalWrite(2, HIGH); // Bật LED
      Serial.println("Đã BẬT thiết bị!");
    } else if (dataString == "OFF") {
      digitalWrite(2, LOW);  // Tắt LED
      Serial.println("Đã TẮT thiết bị!");
    }
  }
  // Thêm các if/else if cho các topic khác nếu cần...
}

// ============== SETUP ==============
void setup() {
  WIFI_setup();
  // ================= OTA =================
  // ArduinoOTA.setHostname("ESP32_OTA");
  // ArduinoOTA.begin();
  // Serial.println("OTA Begin!");
  // Serial.println(WiFi.localIP());

  // === CHẠY OTA TRÊN CORE 0 ===
  // xTaskCreatePinnedToCore(
  //   otaTask,      // Func
  //   "OTA_Task",   // Tên task
  //   4096,         // Stack size (byte)
  //   NULL,
  //   1,            // Ưu tiên
  //   NULL,
  //   0             // CORE
  // );

  // === KHỞI ĐỘNG MQTT ===
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);   // Hàm xử lý khi nhận dữ liệu
  connectMQTT();
}

// ============== LOOP (CORE 1) ==============
void loop() {
  if (!client.connected()) {
    connectMQTT();
  }
  client.loop();

  // === GỬI DỮ LIỆU MỖI 5S ===
  static unsigned long lastSend = 0;
  if (millis() - lastSend >= 10000) {
    lastSend = millis();
    // Cập nhật nhiệt độ giả lập
    updateSimulatedTemp();
    sendData();
  }
  delay(1);
}