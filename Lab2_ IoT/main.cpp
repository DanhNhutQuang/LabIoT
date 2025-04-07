#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// Cấu hình DHT11
#define DHTPIN 14       
#define DHTTYPE DHT11  

DHT dht(DHTPIN, DHTTYPE);

// Cấu hình WiFi
const char* ssid = "DCN";
const char* password = "quangdeptrai@123";

const char* mqtt_server = "app.coreiot.io";
const int mqtt_port = 1883; 
const char* access_token = "3fgha6izbn00zfqdof6v";

WiFiClient espClient;
PubSubClient client(espClient);

float temperature = 0.0;
float humidity = 0.0;

void sendSensorData();

// Kết nối WiFi
void connectWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
}

// Kết nối MQTT với Core IoT
bool connectMQTT() {
  client.setServer(mqtt_server, mqtt_port);
  if (client.connect("ESP32_Client", access_token, "")) {
    Serial.println("Connected to Core IoT!");
    return true;
  } else {
    Serial.print("MQTT Connection Failed, rc=");
    Serial.print(client.state());
    Serial.println(" -> Retry in 5 seconds");
    return false;
  }
}

// Callback MQTT khi nhận dữ liệu
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  // Nếu nhận lệnh yêu cầu kiểm tra nhiệt độ, gửi ngay dữ liệu
  if (String(topic) == "v1/devices/me/attributes" && message.indexOf("check_temperature") != -1) {
    Serial.println("Scheduler yêu cầu kiểm tra nhiệt độ!");
    sendSensorData(); 
  }
}

// Gửi dữ liệu cảm biến lên Core IoT
void sendSensorData() {
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

// Tác vụ kết nối và duy trì MQTT
void mqttTask(void *pvParameters) {
  for (;;) {
    if (!client.connected()) {
      if (connectMQTT()) {
        client.subscribe("v1/devices/me/attributes");  
      }
    }
    client.loop();  
    vTaskDelay(pdMS_TO_TICKS(1000));  
  }
}

// Tác vụ đọc dữ liệu cảm biến DHT11
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

// Tác vụ gửi dữ liệu lên Core IoT
void sendDataTask(void *pvParameters) {
  for (;;) {
    sendSensorData();
    vTaskDelay(pdMS_TO_TICKS(5000));  
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  connectWiFi();

  // Tạo các task cho FreeRTOS
  xTaskCreate(readDHTDataTask, "Read DHT Data", 2048, NULL, 1, NULL);
  xTaskCreate(sendDataTask, "Send Data", 2048, NULL, 1, NULL);
  xTaskCreate(mqttTask, "MQTT Task", 2048, NULL, 1, NULL);
}

void loop() {
  
}