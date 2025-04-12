#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

//================== Cấu hình cảm biến DHT11 ==================//
#define DHTPIN 14
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

//================== Cấu hình WiFi và MQTT ==================//
const char* ssid = "DCN";
const char* password = "quangdeptrai@123";
const char* mqtt_server = "app.coreiot.io";
const int mqtt_port = 1883;
const char* access_token = "3fgha6izbn00zfqdof6v";

//================== Biến toàn cục ==================//
WiFiClient espClient;
PubSubClient client(espClient);
WebServer server(80);  // Web server cho OTA
float temperature = 0.0;
float humidity = 0.0;

//================== Hàm gửi dữ liệu cảm biến ==================//
void sendSensorData() {
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Skip sending: Invalid sensor data.");
    return;
  }

  String payload = "{";
  payload += "\"temperature\":" + String(temperature) + ",";
  payload += "\"humidity\":" + String(humidity);
  payload += "}";

  if (client.connected()) {
    client.publish("v1/devices/me/telemetry", payload.c_str());
    Serial.println("Sent to Core IoT: " + payload);
  } else {
    Serial.println("MQTT not connected! Attempting to reconnect.");
  }

}

//================== Kết nối WiFi ==================//
void connectWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

//================== Kết nối MQTT ==================//
bool connectMQTT() {
  client.setServer(mqtt_server, mqtt_port);
  if (client.connect("ESP32_Client", access_token, "")) {
    Serial.println("Connected to Core IoT!");
    client.subscribe("v1/devices/me/attributes");
    return true;
  } else {
    Serial.print("MQTT Connection Failed, rc=");
    Serial.println(client.state());
    return false;
  }
}

//================== Callback khi nhận dữ liệu MQTT ==================//
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  if (String(topic) == "v1/devices/me/attributes" && message.indexOf("check_temperature") != -1) {
    sendSensorData();
  }
}

//================== Tác vụ FreeRTOS ==================//
void mqttTask(void *pvParameters) {
  for (;;) {
    if (!client.connected()) {
      connectMQTT();
    }
    client.loop();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void readDHTDataTask(void *pvParameters) {
  for (;;) {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Failed to read from DHT sensor!");
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void sendDataTask(void *pvParameters) {
  for (;;) {
    sendSensorData();
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

//================== Thiết lập OTA ==================//
void setupOTA() {
  if (!MDNS.begin("esp32")) {
    Serial.println("Error setting up MDNS responder!");
    return;
  }

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html",
      "<form method='POST' action='/update' enctype='multipart/form-data'>"
      "<input type='file' name='update'><input type='submit' value='Update'></form>");
  });

  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin()) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %u bytes\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.begin();
  Serial.println("HTTP OTA Web Server started at /");
}

//================== Setup ==================//
void setup() {
  Serial.begin(115200);
  dht.begin();
  connectWiFi();
  setupOTA();

  client.setCallback(callback);

  xTaskCreate(readDHTDataTask, "Read DHT Data", 2048, NULL, 1, NULL);
  xTaskCreate(sendDataTask, "Send Data", 2048, NULL, 1, NULL);
  xTaskCreate(mqttTask, "MQTT Task", 2048, NULL, 1, NULL);
}

//================== Loop ==================//
void loop() {
  server.handleClient();  // Phục vụ OTA HTTP
}
