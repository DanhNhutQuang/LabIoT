#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// Cấu hình cảm biến DHT11
#define DHTPIN 14
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Thông tin WiFi
const char* ssid = "DCN";
const char* password = "quangdeptrai@123";

const char* mqtt_server = "app.coreiot.io";
const int mqtt_port = 1883; 

const char* access_token = "3fgha6izbn00zfqdof6v";

// WiFi & MQTT Client
WiFiClient espClient;
PubSubClient client(espClient);

// Kết nối WiFi
void connectWiFi() {
  Serial.print(" Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n WiFi Connected!");
}

// Kết nối MQTT với Core IoT
bool connectMQTT() {
  client.setServer(mqtt_server, mqtt_port);

  if (client.connect("ESP32_Client", access_token, "")) {
    Serial.println(" Connected to Core IoT!");
    return true;
  } else {
    Serial.print(" MQTT Connection Failed, rc=");
    Serial.print(client.state());
    Serial.println(" -> Retry in 5 seconds");
    return false;
  }
}

// Gửi dữ liệu cảm biến lên Core IoT
void sendSensorData() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println(" Failed to read from DHT sensor!");
    return;
  }

  // Định dạng dữ liệu JSON
  String payload = "{";
  payload += "\"temperature\":" + String(temperature) + ",";
  payload += "\"humidity\":" + String(humidity);
  payload += "}";

  // Kiểm tra kết nối MQTT 
  if (!client.connected()) {
    Serial.println(" Reconnecting to MQTT...");
    if (!connectMQTT()) return;
  }

  // Gửi dữ liệu lên Core IoT
  client.publish("v1/devices/me/telemetry", payload.c_str());
  Serial.println(" Sent to Core IoT: " + payload);
  Serial.println("=====================");
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  
  connectWiFi();
  connectMQTT();
}

void loop() {
  sendSensorData();
  delay(5000);  
}
