#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <MqttUtil.h>
#include <GenericAnalogSensor.h>
#include <PirSensor.h>
#include <SwitchSensor.h>
#include <TeliaWifiClient.h>

const char* PACKAGE_NAME = "nodemcu_mqtt_toilet_project";

#define DEBUG false
#define VERBOSE true
#define DEEP_SLEEP false

#define WIFI_RESET_TIMER_SECONDS 60
#define MQTT_RESET_TIMER_SECONDS 60

#define PUBLISH_INTERVAL 30
#define SLEEP_DELAY_IN_SECONDS  30

#define SWITCH_SENSOR_PIN D5
#define SWITCH_SENSOR_WOMEN_PIN D7
#define LIGHT_SENSOR_PIN A0
#define MOTION_SENSOR_PIN D6

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

const char* teliaWifiUser = TELIA_WIFI_USER;
const char* teliaWifiPassword = TELIA_WIFI_PASSWORD;

const char* mqtt_server = MQTT_SERVER;
const int   mqtt_port = MQTT_PORT;

const char* outTopic = MQTT_OUT_TOPIC;
const char* inTopic = MQTT_IN_TOPIC;

WiFiEventHandler gotIpEventHandler, disconnectedEventHandler;
bool wifiConnected = false;
long wifiDisconnectedPeriode, wifiDisconnectedPeriodeStart;

WiFiClient espClient;
PubSubClient client(espClient);
bool controllerInfoNeedsToBeSent = false;

long lastRun = millis();
int nodemcuChipId = ESP.getChipId(); // returns the ESP8266 chip ID as a 32-bit integer.
long motionSessionStart, switchSensorStart, switchWomenSensorStart;

MqttUtil mqttUtil = MqttUtil(nodemcuChipId, PACKAGE_NAME, ssid, inTopic, outTopic, false);

GenericAnalogSensor light = GenericAnalogSensor(LIGHT_SENSOR_PIN, 30, false);
PirSensor motion = PirSensor(MOTION_SENSOR_PIN, 1, false, false);
SwitchSensor mySwitch = SwitchSensor(SWITCH_SENSOR_PIN, 1, false, false);
SwitchSensor mySwitchWomen = SwitchSensor(SWITCH_SENSOR_WOMEN_PIN, 1, false, false);

void setupWifi() {
    long loopTimer = millis();
    Serial.print("Connecting to "); Serial.println(ssid);
    WiFi.begin(ssid);
    while (WiFi.status() != WL_CONNECTED) {
        long now = millis();
        if (now - loopTimer > (WIFI_RESET_TIMER_SECONDS * 1000)) {
            ESP.restart();
        }
        Serial.print("."); Serial.print(ssid);
        delay(1000);
    }
    randomSeed(micros());
    Serial.println(""); Serial.print("WiFi connected with IP: "); Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived ["); Serial.print(topic); Serial.print("] ");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();
    // Switch on the LED if an 1 was received as first character
    if ((char)payload[0] == '1') {
        // it is acive low on the ESP-01)
    } else {
    }
}

void reconnectMqtt(uint32 ipAddress, long wifiDisconnectedPeriode) {
    long loopTimer = millis();
    while (!client.connected()) {
        String clientId = "ESP8266Client-";
        clientId += String(random(0xffff), HEX);
        Serial.print("Attempting MQTT connection: ["); Serial.print(clientId); Serial.print("] : ");
        if (client.connect(clientId.c_str())) {
            Serial.println("Connected to MQTT!");
            client.subscribe(inTopic);
            mqttUtil.sendControllerInfo(client, ipAddress, wifiDisconnectedPeriode);
        } else {
            long now = millis();
            if (now - loopTimer > (MQTT_RESET_TIMER_SECONDS * 1000)) {
                ESP.restart();
            }
            Serial.print("failed, rc="); Serial.print(client.state()); Serial.println(" try again in 5 seconds...");
            delay(5000);
        }
    }
}

void setup(void) {
    Serial.begin(115200);
    gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event) {
        wifiConnected = true;
        wifiDisconnectedPeriode = millis() - wifiDisconnectedPeriodeStart;
        Serial.print("Wifi connected, IP: "); Serial.println(WiFi.localIP());
    });
    disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
        wifiConnected = false;
        wifiDisconnectedPeriodeStart = millis();
        Serial.println("Wifi disconnected...");
    });
    setupWifi();
    TeliaWifi.connect(teliaWifiUser, teliaWifiPassword);
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

    light.begin();
    motion.begin();
    mySwitch.begin();
    mySwitchWomen.begin();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        setupWifi();
        TeliaWifi.connect(teliaWifiUser, teliaWifiPassword);
        return;
    }
    if (!client.connected()) {
        reconnectMqtt(WiFi.localIP(), wifiDisconnectedPeriode);
    }
    client.loop();
    long now = millis();

    // Stuff to do as soon as possible and as often as possible.
    light.sampleValue();

    // React to motion events
    int motionStateChange = motion.sampleValue();
    if (motionStateChange >= 0) {
        if (motionStateChange == 1) {
            motionSessionStart = millis();
            mqttUtil.publishKeyValueInt(client, "motion", motionStateChange);
        } else {
            long motionSessionLength = millis() - motionSessionStart;
            mqttUtil.publishKeyValueInt(client, "motion", motionStateChange, "motionSessionLength", motionSessionLength);

        }
    }

    // React to switch events
    int switchStateChange = mySwitch.sampleValue();
    if (switchStateChange >= 0) {
        if (switchStateChange == 1) {
            switchSensorStart = millis();
            mqttUtil.publishKeyValueInt(client, "switch", switchStateChange);
        } else {
            long switchSessionLength = millis() - switchSensorStart;
            mqttUtil.publishKeyValueInt(client, "switch", switchStateChange, "switchSessionLength", switchSessionLength);
        }
    }

    // React to switchWomen events
    int switchWomenStateChange = mySwitchWomen.sampleValue();
    if (switchWomenStateChange >= 0) {
        if (switchWomenStateChange == 1) {
            switchWomenSensorStart = millis();
            mqttUtil.publishKeyValueInt(client, "switchWomen", switchWomenStateChange);
        } else {
            long switchWomenSessionLength = millis() - switchWomenSensorStart;
            mqttUtil.publishKeyValueInt(client, "switchWomen", switchWomenStateChange, "switchWomenSessionLength", switchWomenSessionLength);
        }
    }

    // Stuff to do at given time intervals.
    if (now - lastRun > (PUBLISH_INTERVAL * 1000)) {
        lastRun = now;
        float valueLight = light.readValue();
        mqttUtil.publishKeyValueFloat(client, "light", valueLight, 3, 1);

        if (DEEP_SLEEP) {
            Serial.print("Entering deep sleep mode for "); Serial.print(SLEEP_DELAY_IN_SECONDS); Serial.println(" seconds...");
            ESP.deepSleep(SLEEP_DELAY_IN_SECONDS * 1000000, WAKE_RF_DEFAULT);
        }
    }

    delay(10); // Allow internal stuff to be executed.
}

