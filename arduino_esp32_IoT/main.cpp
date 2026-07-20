#include <WiFi.h>
#include <PubSubClient.h>

#define LED_GPIO 2
const char* ssid = "EVG";
const char* password = "19741998";
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* topic_pub = "esp32/hello";
const char* topic_sub = "esp32/led";

WiFiClient espClient;
PubSubClient client(espClient);

uint8_t led_state = 0;
uint32_t counter = 0;

//Переменные для задержкиs
unsigned long lastPublishTime = 0;
const unsigned long publishInterval = 20000; //20 секунд

//MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT DATA [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (strcmp(topic, topic_sub) == 0) {
    if ((char)payload[0] == 'O' && (char)payload[1] == 'N') {
      led_state = 1;
      digitalWrite(LED_GPIO, led_state);
      Serial.println("LED ON");
    } else if ((char)payload[0] == 'O' && (char)payload[1] == 'F' && (char)payload[2] == 'F') {
      led_state = 0;
      digitalWrite(LED_GPIO, led_state);
      Serial.println("LED OFF");
    }
  }
}

//MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("MQTT подключение...");
    if (client.connect("ESP32_Client")) {
      Serial.println(" Подключено!");
      client.subscribe(topic_sub);
      Serial.print("Подписан на топик: ");
      Serial.println(topic_sub);
    } else {
      Serial.print(" Ошибка, rc=");
      Serial.print(client.state());
      Serial.println(" Пробуем через 5с...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_GPIO, OUTPUT);
  digitalWrite(LED_GPIO, led_state); //Выключаем светодиод при старте

  //Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("WiFi подключение");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi подключён!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  //MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  //Всегда поддерживаем MQTT-соединение
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  //Публикуем данные только когда прошло 20 секунд
  unsigned long now = millis();
  if (now - lastPublishTime >= publishInterval) {
    lastPublishTime = now;
    char payload[128];
    snprintf(payload, sizeof(payload), "{\"counter\":%lu, \"led\":%d}", counter++, led_state);
    client.publish(topic_pub, payload);
    Serial.print("Опубликовано: ");
    Serial.println(payload);
  }
}