#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <RTCZero.h>
#include <TimeLib.h>
#include <MQTT.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <DHT_U.h>

#include "config.h"

#define DHT_PIN   6
#define DHT_TYPE  DHT22

#define FW_VERSION "0.0.2"

// MKR1010 onboard RTC
RTCZero rtc = RTCZero();

WiFiUDP ntpUDP = WiFiUDP();
WiFiClient net = WiFiClient();

NTPClient ntp = NTPClient(ntpUDP, "us.pool.ntp.org");
MQTTClient mqtt = MQTTClient(512);
DHT dht(DHT_PIN, DHT_TYPE);

char clientId[13];

char temperature_config_topic[60];
char humidity_config_topic[60];
char base_topic[60];
char state_topic[60];
char availability_topic[60];

char sensor_id_t[16];
char sensor_id_h[16];

String temperature_disc_payload;
String humidity_disc_payload;

unsigned long last_update_ms = 0;
unsigned long update_interval_ms = 60000;

void setup() {
  setRgbLed(0xFF, 0xFF, 0x0);

  Serial.begin(9600);
  delay(3000);
  Serial.println();

  byte mac[6];
  WiFi.macAddress(mac);
  mac2Char(mac, clientId);
  Serial.print("Client ID: ");
  Serial.println(clientId);

  buildSensorIds();
  buildTopicNames();
  buildDiscoveryPayloads();

  wifiConnect();
  ntpClockUpdate();
  syncClockToRtc();
  mqttConnect();

  // Systems go.
  setRgbLed(0, 0xFF, 0);

  dht.begin();
}

void loop() {
  mqtt.loop();

  if (!mqtt.connected()) {
    wifiConnect();
    mqttConnect();
  }

  if (millis() - last_update_ms > update_interval_ms) {
    last_update_ms = millis();

    StaticJsonDocument<200> doc;
    doc["time"] = iso8601_date();
    doc["temperature"] = dht.readTemperature();
    doc["humidity"] = dht.readHumidity();

    String msg;
    serializeJson(doc, msg);
    mqtt.publish(state_topic, msg);
    Serial.println(msg);
  }

  delay(10);
}

void setRgbLed(uint8_t red, uint8_t green, uint8_t blue) {
  WiFiDrv::pinMode(25, OUTPUT);
  WiFiDrv::pinMode(26, OUTPUT);
  WiFiDrv::pinMode(27, OUTPUT);
  WiFiDrv::analogWrite(25, green);
  WiFiDrv::analogWrite(26, red);
  WiFiDrv::analogWrite(27, blue);
}

time_t syncClock() {
  return rtc.getEpoch();
}

void mac2Char(byte mac[], char str[]) {
  for (int i = 5; i >= 0; --i) {
    char buf[3];
    sprintf(buf, "%2X", mac[i]);
    strcat(str, buf);
  }
}

bool wifiConnect() {
  Serial.print("Connecting to WiFI network..");

  int maxWifiAttempts = 30;
  int i = 0;
  while ( WiFi.status() != WL_CONNECTED && i < maxWifiAttempts) {
    WiFi.disconnect();
    delay(1000);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    delay (1000);
    Serial.print(".");
    i++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    setRgbLed(0xFF, 0x0, 0x0);
    Serial.println("Failed");
    return false;
  }

  setRgbLed(0xFF, 0, 0xFF);
  Serial.println("Success!");

  return true;
}

void ntpClockUpdate() {
  Serial.print("Setting current time via NTP..");
  ntp.begin();
  ntp.update();

  //@todo: Check to see if update successful?
  rtc.begin();
  rtc.setEpoch(ntp.getEpochTime());

  // Release the NTP session.
  ntp.end();
}

void syncClockToRtc() {
  setSyncInterval(300); // Seconds between resync of system clock to RTC
  setSyncProvider(syncClock); // Tells system clock how to sync

  timeStatus() == timeSet ? Serial.println("Success!") : Serial.println("Failed");

  setRgbLed(0, 0xFF, 0xFF);
}

bool mqttConnect() {
  Serial.print("Connecting to messaging server..");

  mqtt.begin(MQTT_HOST, MQTT_PORT, net);
  mqtt.setKeepAlive(300);
  mqtt.setWill(availability_topic, "offline", true, 1);

  int max_attempts = 30;
  int j = 0;
  while (!mqtt.connect(clientId, MQTT_USER, MQTT_PASS) && j < max_attempts) {
    Serial.print(".");
    delay(1000);
    j++;
  }

  if (!mqtt.connected()) {
    setRgbLed(0xFF, 0, 0);
    Serial.println("Failed");
    return false;
  }

  Serial.println("Success!");

  mqttPublishDiscovery();
  mqtt.publish(availability_topic, "online", true, 1);

  return true;
}

void mqttPublishDiscovery() {
  if (!mqtt.connected()) {
    Serial.println("Can't publish discovery because mqtt isn't connected.");
    return;
  }

  mqtt.publish(temperature_config_topic, temperature_disc_payload, true, 1);
  mqtt.publish(humidity_config_topic, humidity_disc_payload, true, 1);
}

void buildTopicNames() {
  strcat(temperature_config_topic, "homeassistant/sensor/logger_");
  strcat(temperature_config_topic, clientId);
  strcat(temperature_config_topic, "_t/config");

  strcat(humidity_config_topic, "homeassistant/sensor/logger_");
  strcat(humidity_config_topic, clientId);
  strcat(humidity_config_topic, "_h/config");

  strcat(base_topic, "homeassistant/sensor/logger_");
  strcat(base_topic, clientId);

  strcat(state_topic, "homeassistant/sensor/logger_");
  strcat(state_topic, clientId);
  strcat(state_topic, "/state");

  strcat(availability_topic, "homeassistant/sensor/logger_");
  strcat(availability_topic, clientId);
  strcat(availability_topic, "/availability");
}

void buildDiscoveryPayloads() {
  StaticJsonDocument<512> temperature_doc;
  temperature_doc["~"] = base_topic;
  temperature_doc["dev_cla"] = "temperature"; //device_class
  temperature_doc["name"] = "Logger Temperature";
  temperature_doc["stat_t"] = "~/state"; //state_topic
  temperature_doc["unit_of_meas"] = "°C"; //unit_of_measurement
  temperature_doc["val_tpl"] = "{{value_json.temperature}}"; //value_template
  temperature_doc["avty_t"] = "~/availability"; //availability_topic
  temperature_doc["uniq_id"] = sensor_id_t; //unique_id
  JsonObject temperature_dev = temperature_doc.createNestedObject("dev"); //device
  temperature_dev["name"] = "Logger"; //name
  temperature_dev["mf"] = "Jeremy Meier"; //manufacturer
  temperature_dev["mdl"] = "Tri-Sensor"; //model
  temperature_dev["sw"] = FW_VERSION; //sw_version
  JsonArray temperature_ids = temperature_dev.createNestedArray("ids"); //identifiers
  temperature_ids.add(clientId);

  serializeJson(temperature_doc, temperature_disc_payload);

  Serial.println("Temperature discovery:");
  Serial.println(temperature_disc_payload);

  StaticJsonDocument<512> humidity_doc;
  humidity_doc["~"] = base_topic;
  humidity_doc["dev_cla"] = "humidity"; //device_class
  humidity_doc["name"] = "Logger Humidity";
  humidity_doc["stat_t"] = "~/state"; //state_topic
  humidity_doc["unit_of_meas"] = "%"; //unit_of_measurement
  humidity_doc["val_tpl"] = "{{value_json.humidity}}"; //value_template
  humidity_doc["avty_t"] = "~/availability"; //availability_topic
  humidity_doc["uniq_id"] = sensor_id_h; //unique_id
  JsonObject humidity_dev = humidity_doc.createNestedObject("dev"); //device
  humidity_dev["name"] = "Logger"; //name
  humidity_dev["mf"] = "Jeremy Meier"; //manufacturer
  humidity_dev["mdl"] = "Tri-Sensor"; //model
  humidity_dev["sw"] = FW_VERSION; //sw_version
  JsonArray humidity_ids = humidity_dev.createNestedArray("ids"); //identifiers
  humidity_ids.add(clientId);

  serializeJson(humidity_doc, humidity_disc_payload);

  Serial.println("Humidity discovery:");
  Serial.println(humidity_disc_payload);
}

void buildSensorIds() {
  strcat(sensor_id_t, clientId);
  strcat(sensor_id_t, "_t");

  strcat(sensor_id_h, clientId);
  strcat(sensor_id_h, "_h");
}
