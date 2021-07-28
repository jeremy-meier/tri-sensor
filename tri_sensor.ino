#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <RTCZero.h>
#include <TimeLib.h>
#include <MQTT.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ArduinoLowPower.h>
#include <Battery.h>

#include "config.h"

#define DHT_PIN 7
#define DHT_TYPE DHT22

#define FW_VERSION "0.1.0"

RTCZero rtc = RTCZero();

WiFiUDP ntpUDP = WiFiUDP();
WiFiClient net = WiFiClient();

NTPClient ntp = NTPClient(ntpUDP, "us.pool.ntp.org");
MQTTClient mqtt = MQTTClient(512);
DHT dht(DHT_PIN, DHT_TYPE);

char clientId[13];

char base_topic[60];
char state_topic[60];
char availability_topic[60];

char sensor_id_t[16];
char sensor_id_h[16];
char sensor_id_i[16];
char sensor_id_b[16];

char temperature_config_topic[60];
char humidity_config_topic[60];
char illuminance_config_topic[60];
char battery_config_topic[60];

String temperature_disc_payload;
String humidity_disc_payload;
String illuminance_disc_payload;
String battery_disc_payload;

unsigned long update_interval_ms = 5 * 60 * 1000; // 5 minutes

int BAT_MIN_MV = 3200;
int BAT_MAX_MV = 4100;
float DIVIDER_RATIO = (1200 + 330) / 1200 // From MKR1010 Schematic -> See R8 & R9
int ADC_REF_VOLTAGE = 3300;

Battery battery(BAT_MIN_MV, BAT_MAX_MV, ADC_BATTERY);

void setup() {
  Serial.begin(9600);
  delay(3000);
  Serial.println();

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.print("WiFi Firmware: ");
  Serial.println(WiFi.firmwareVersion());

  byte mac[6];
  WiFi.macAddress(mac);
  mac2Char(mac, clientId);
  Serial.print("Client ID: ");
  Serial.println(clientId);

  buildSensorIds();
  buildTopicNames();
  buildDiscoveryPayloads();

  wifiConnect();

  if (WiFi.status() == WL_CONNECTED) {
    ntpClockUpdate();
    syncClockToRtc();
    mqttConnect();
    mqttPublishDiscovery();
  }
  else {
    Serial.println("Can't start due to failure to connect to WiFi network.");
    wifiDisconnect();
    LowPower.deepSleep();
  }
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  
  battery = Battery(BAT_MIN_MV, BAT_MAX_MV, ADC_BATTERY);
  battery.begin(ADC_REF_VOLTAGE, DIVIDER_RATIO, &sigmoidal);
  
  mqtt.loop();

  if (WiFi.status() != WL_CONNECTED) {
    wifiConnect();
  }
  
  if (WiFi.status() == WL_CONNECTED && !mqtt.connected()) {
    mqttConnect();
  }

  if (mqtt.connected()) {
    syncClockToRtc();
    
    dht.begin();
    
    StaticJsonDocument<200> doc;
    
    int illum_val = 100 * analogRead(A1) / 1023;
    char ill_str[6];
    
    dtostrf(illum_val, 5, 1, ill_str);
    
    doc["time"] = iso8601_date();
    doc["temperature"] = dht.readTemperature();
    doc["humidity"] = dht.readHumidity();
    doc["illuminance"] = ill_str; // Just a percentage of full scale for now.
    doc["battery"] = battery.level();
    
    String msg;
    serializeJson(doc, msg);
    mqtt.publish(state_topic, msg);
    Serial.print("Publishing message: ");
    Serial.println(msg);
  }

  wifiDisconnect();
  digitalWrite(LED_BUILTIN, LOW);

  LowPower.deepSleep(update_interval_ms);
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

void wifiConnect() {
  Serial.print("Connecting to WiFi network..");

  WiFi.lowPowerMode();
  
  int maxWifiAttempts = 5;
  
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
    Serial.println("Failed");
    Serial.print("Reason code: ");
    Serial.println(WiFi.reasonCode());
    return;
  }

  Serial.println("Success!");
}

void wifiDisconnect() {
  Serial.println("Disconnecting from WiFi network.");
  WiFi.disconnect();
  delay(1000);
  WiFi.end();
  delay(1000);
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
  Serial.print("Sync system clock to RTC..");
  setSyncInterval(43200); // Seconds between resync of system clock to RTC.  43200 = 12h
  setSyncProvider(syncClock); // Tells system clock how to sync

  timeStatus() == timeSet ? Serial.println("Success!") : Serial.println("Failed");
}

bool mqttConnect() {
  Serial.print("Connecting to messaging server..");

  mqtt.begin(MQTT_HOST, MQTT_PORT, net);
  mqtt.setKeepAlive(1800); // 1800 seconds = 30 minutes
  // The will message here tells Home Assistant the device status is 'offline' if it can't be reached.
  mqtt.setWill(availability_topic, "offline", true, 1);

  int max_attempts = 30;
  int j = 0;
  while (!mqtt.connect(clientId, MQTT_USER, MQTT_PASS) && j < max_attempts) {
    Serial.print(".");
    delay(1000);
    j++;
  }

  if (!mqtt.connected()) {
    Serial.println("Failed");
    return false;
  }

  Serial.println("Success!");

  mqtt.publish(availability_topic, "online", true, 1);

  return true;
}

void mqttPublishDiscovery() {
  Serial.println("Publishing MQTT Discovery payloads for sensor.");
  
  if (!mqtt.connected()) {
    Serial.println("Can't publish discovery because mqtt isn't connected.");
    return;
  }

  mqtt.publish(temperature_config_topic, temperature_disc_payload, true, 1);
  mqtt.publish(humidity_config_topic, humidity_disc_payload, true, 1);
  mqtt.publish(illuminance_config_topic, illuminance_disc_payload, true, 1);
  mqtt.publish(battery_config_topic, battery_disc_payload, true, 1);
}

void buildTopicNames() {
  strcat(temperature_config_topic, "homeassistant/sensor/logger_");
  strcat(temperature_config_topic, clientId);
  strcat(temperature_config_topic, "_t/config");

  strcat(humidity_config_topic, "homeassistant/sensor/logger_");
  strcat(humidity_config_topic, clientId);
  strcat(humidity_config_topic, "_h/config");

  strcat(illuminance_config_topic, "homeassistant/sensor/logger_");
  strcat(illuminance_config_topic, clientId);
  strcat(illuminance_config_topic, "_i/config");

  strcat(battery_config_topic, "homeassistant/sensor/logger_");
  strcat(battery_config_topic, clientId);
  strcat(battery_config_topic, "_b/config");

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
  temperature_doc["name"] = "Tri-Sensor Temperature";
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

  Serial.print("Temperature discovery:");
  Serial.println(temperature_disc_payload);

  StaticJsonDocument<512> humidity_doc;
  humidity_doc["~"] = base_topic;
  humidity_doc["dev_cla"] = "humidity"; //device_class
  humidity_doc["name"] = "Tri-Sensor Humidity";
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

  Serial.print("Humidity discovery:");
  Serial.println(humidity_disc_payload);

  StaticJsonDocument<512> illuminance_doc;
  illuminance_doc["~"] = base_topic;
  illuminance_doc["dev_cla"] = "illuminance"; //device_class
  illuminance_doc["name"] = "Tri-Sensor Illuminance";
  illuminance_doc["stat_t"] = "~/state"; //state_topic
  illuminance_doc["unit_of_meas"] = "%"; //unit_of_measurement
  illuminance_doc["val_tpl"] = "{{value_json.illuminance}}"; //value_template
  illuminance_doc["avty_t"] = "~/availability"; //availability_topic
  illuminance_doc["uniq_id"] = sensor_id_i; //unique_id
  JsonObject illuminance_dev = illuminance_doc.createNestedObject("dev"); //device
  illuminance_dev["name"] = "Logger"; //name
  illuminance_dev["mf"] = "Jeremy Meier"; //manufacturer
  illuminance_dev["mdl"] = "Tri-Sensor"; //model
  illuminance_dev["sw"] = FW_VERSION; //sw_version
  JsonArray illuminance_ids = illuminance_dev.createNestedArray("ids"); //identifiers
  illuminance_ids.add(clientId);

  serializeJson(illuminance_doc, illuminance_disc_payload);

  Serial.print("Illuminance discovery:");
  Serial.println(illuminance_disc_payload);

  StaticJsonDocument<512> battery_doc;
  battery_doc["~"] = base_topic;
  battery_doc["dev_cla"] = "battery"; //device_class
  battery_doc["name"] = "Tri-Sensor Battery";
  battery_doc["stat_t"] = "~/state"; //state_topic
  battery_doc["unit_of_meas"] = "%"; //unit_of_measurement
  battery_doc["val_tpl"] = "{{value_json.battery}}"; //value_template
  battery_doc["avty_t"] = "~/availability"; //availability_topic
  battery_doc["uniq_id"] = sensor_id_b; //unique_id
  JsonObject battery_dev = battery_doc.createNestedObject("dev"); //device
  battery_dev["name"] = "Logger"; //name
  battery_dev["mf"] = "Jeremy Meier"; //manufacturer
  battery_dev["mdl"] = "Tri-Sensor"; //model
  battery_dev["sw"] = FW_VERSION; //sw_version
  JsonArray battery_ids = battery_dev.createNestedArray("ids"); //identifiers
  battery_ids.add(clientId);

  serializeJson(battery_doc, battery_disc_payload);

  Serial.print("Battery discovery:");
  Serial.println(battery_disc_payload);  
}

void buildSensorIds() {
  strcat(sensor_id_t, clientId);
  strcat(sensor_id_t, "_t");

  strcat(sensor_id_h, clientId);
  strcat(sensor_id_h, "_h");

  strcat(sensor_id_i, clientId);
  strcat(sensor_id_i, "_i");

  strcat(sensor_id_b, clientId);
  strcat(sensor_id_b, "_b");
}
