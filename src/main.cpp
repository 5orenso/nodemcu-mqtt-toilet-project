#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <PubSubClient.h>
#include <LightSensor.h>
#include <PirSensor.h>
#include <SwitchSensor.h>

const char* PACKAGE_NAME = "nodemcu-mqtt-toilet-project";

#define DEBUG false
#define VERBOSE true

#define DEEP_SLEEP false

#define PUBLISH_INTERVAL 30
#define SLEEP_DELAY_IN_SECONDS  30

#define SWITCH_SENSOR_PIN D5
#define SWITCH_SENSOR_WOMEN_PIN D7
#define LIGHT_SENSOR_PIN A0
#define MOTION_SENSOR_PIN D6

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

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
char msg[150];
int nodemcuChipId = ESP.getChipId(); // returns the ESP8266 chip ID as a 32-bit integer.

LightSensor light = LightSensor(LIGHT_SENSOR_PIN, 30, false);
PirSensor motion = PirSensor(MOTION_SENSOR_PIN, 1, false, false);
SwitchSensor mySwitch = SwitchSensor(SWITCH_SENSOR_PIN, 1, false, false);
SwitchSensor mySwitchWomen = SwitchSensor(SWITCH_SENSOR_WOMEN_PIN, 1, false, false);

long motionSessionStart, switchSensorStart, switchWomenSensorStart;

void setupWifi() {
    delay(10);
    // WiFi.disconnect();
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    // OTA wifi setting
    WiFi.mode(WIFI_STA);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print("."); Serial.print(ssid);
        delay(500);
    }
    randomSeed(micros());
    Serial.println("");
    Serial.print("WiFi connected with IP: ");
    Serial.println(WiFi.localIP());
}

void setupOTA() {
    // OTA start
    // Port defaults to 8266
    // ArduinoOTA.setPort(8266);

    // Hostname defaults to esp8266-[ChipID]
    // ArduinoOTA.setHostname("myesp8266");

    // No authentication by default
    // ArduinoOTA.setPassword("admin");

    // Password can be set with it's md5 value as well
    // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
    // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else { // U_SPIFFS
            type = "filesystem";
        }
        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    // OTA end
}


void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    // Switch on the LED if an 1 was received as first character
    if ((char)payload[0] == '1') {
        // it is acive low on the ESP-01)
    } else { }
}

void sendControllerInfo() {
    if (client.connected()) {
        // --[ Publish this device to AWS IoT ]----------------------------------------
        String nodemcuResetReason = ESP.getResetReason(); // returns String containing the last reset resaon in human readable format.
        int nodemcuFreeHeapSize = ESP.getFreeHeap(); // returns the free heap size.
        // Several APIs may be used to get flash chip info:
        int nodemcuFlashChipId = ESP.getFlashChipId(); // returns the flash chip ID as a 32-bit integer.
        int nodemcuFlashChipSize = ESP.getFlashChipSize(); // returns the flash chip size, in bytes, as seen by the SDK (may be less than actual size).
        // int nodemcuFlashChipSpeed = ESP.getFlashChipSpeed(void); // returns the flash chip frequency, in Hz.
        // int nodemcuCycleCount = ESP.getCycleCount(); // returns the cpu instruction cycle count since start as an unsigned 32-bit. This is useful for accurate timing of very short actions like bit banging.
        // WiFi.macAddress(mac) is for STA, WiFi.softAPmacAddress(mac) is for AP.
        // int nodemcuIP; // = WiFi.localIP(); // is for STA, WiFi.softAPIP() is for AP.

        char msg[150];
        char resetReason[25];
        strcpy(resetReason, nodemcuResetReason.c_str());
        uint32 ipAddress;
        char ipAddressFinal[16];
        ipAddress = WiFi.localIP();
        if (ipAddress) {
            const int NBYTES = 4;
            uint8 octet[NBYTES];
            for(int i = 0 ; i < NBYTES ; i++) {
                octet[i] = ipAddress >> (i * 8);
            }
            sprintf(ipAddressFinal, "%d.%d.%d.%d", octet[0], octet[1], octet[2], octet[3]);
        }
        snprintf(msg, 150, "{ \"chipId\": %d, \"freeHeap\": %d, \"ip\": \"%s\", \"ssid\": \"%s\" }",
            nodemcuChipId, nodemcuFreeHeapSize, ipAddressFinal, ssid
        );
        if (VERBOSE) {
            Serial.print("Publish message: "); Serial.println(msg);
        }
        client.publish(outTopic, msg);

        // More info about the software.
        snprintf(msg, 150, "{ \"chipId\": %d, \"ip\": \"%s\", \"sw\": \"%s\" }",
            nodemcuChipId, ipAddressFinal, PACKAGE_NAME
        );
        if (VERBOSE) {
            Serial.print("Publish message: "); Serial.println(msg);
        }
        client.publish(outTopic, msg);

        // More info about the software.
        // 0 -> normal startup by power on
        // 1 -> hardware watch dog reset
        // 2 -> software watch dog reset (From an exception)
        // 3 -> software watch dog reset system_restart (Possibly unfed wd got angry)
        // 4 -> soft restart (Possibly with a restart command)
        // 5 -> wake up from deep-sleep
        snprintf(msg, 150, "{ \"chipId\": %d, \"wifiOfflinePeriode\": %d, \"resetReason\": \"%s\" }",
            nodemcuChipId, wifiDisconnectedPeriode, resetReason
        );
        if (VERBOSE) {
            Serial.print("Publish message: "); Serial.println(msg);
        }
        client.publish(outTopic, msg);
    }
}

void reconnectMqtt() {
    // Loop until we're reconnected
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Create a random client ID
        String clientId = "ESP8266Client-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect
        if (client.connect(clientId.c_str())) {
            Serial.println("connected");
            // Once connected, publish an announcement...
            // client.publish(outTopic, "{ \"chipId\": chipId, \"ping\": \"hello world\" }");
            // ... and resubscribe
            client.subscribe(inTopic);
            sendControllerInfo();
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void setup(void) {
    Serial.begin(115200);
    gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event) {
        wifiConnected = true;
        wifiDisconnectedPeriode = millis() - wifiDisconnectedPeriodeStart;
        Serial.print("Station connected, IP: ");
        Serial.println(WiFi.localIP());
    });
    disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
        wifiConnected = false;
        wifiDisconnectedPeriodeStart = millis();
        Serial.println("Station disconnected...");
    });
    setupWifi();
    setupOTA();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

    light.begin();
    motion.begin();
    mySwitch.begin();
    mySwitchWomen.begin();
}

void loop() {
    delay(0); // Allow internal stuff to be executed.
    if (!wifiConnected) {
        delay(1000);
        return;
    }
    if (!client.connected()) {
        reconnectMqtt();
    }
    client.loop();

    // Listen for OTA requests
    ArduinoOTA.handle();

    long now = millis();

    // Stuff to do as soon as possible and as often as possible.
    light.sampleValue();

    // React to motion events
    int motionStateChange = motion.sampleValue();
    if (motionStateChange >= 0) {
        if (DEBUG) {
            Serial.print(" --> motionStateChange: "); Serial.println(motionStateChange);
        }
        if (motionStateChange == 1) {
            motionSessionStart = millis();
            snprintf(msg, 150, "{ \"chipId\": %d, \"motion\": %d }", nodemcuChipId, motionStateChange);
        } else {
            long motionSessionLength = millis() - motionSessionStart;
            snprintf(msg, 150, "{ \"chipId\": %d, \"motion\": %d, \"motionSessionLength\": %d }", nodemcuChipId, motionStateChange, motionSessionLength);
        }
        if (VERBOSE) {
            Serial.print("Publish message: "); Serial.println(msg);
        }
        client.publish(outTopic, msg);
    }

    // React to switch events
    int switchStateChange = mySwitch.sampleValue();
    if (switchStateChange >= 0) {
        if (DEBUG) {
            Serial.print(" --> switchStateChange: "); Serial.println(switchStateChange);
        }
        if (switchStateChange == 1) {
            switchSensorStart = millis();
            snprintf(msg, 150, "{ \"chipId\": %d, \"switch\": %d }", nodemcuChipId, 1);
        } else {
            long switchSessionLength = millis() - switchSensorStart;
            snprintf(msg, 150, "{ \"chipId\": %d, \"switch\": %d, \"switchSessionLength\": %d }", nodemcuChipId, 0, switchSessionLength);
        }
        if (VERBOSE) {
            Serial.print("Publish message: "); Serial.println(msg);
        }
        client.publish(outTopic, msg);
    }

    // React to switchWomen events
    int switchWomenStateChange = mySwitchWomen.sampleValue();
    if (switchWomenStateChange >= 0) {
        if (DEBUG) {
            Serial.print(" --> switchWomenStateChange: "); Serial.println(switchWomenStateChange);
        }
        if (switchWomenStateChange == 1) {
            switchWomenSensorStart = millis();
            snprintf(msg, 150, "{ \"chipId\": %d, \"switchWomen\": %d }", nodemcuChipId, 1);
        } else {
            long switchWomenSessionLength = millis() - switchWomenSensorStart;
            snprintf(msg, 150, "{ \"chipId\": %d, \"switchWomen\": %d, \"switchWomenSessionLength\": %d }", nodemcuChipId, 0, switchWomenSessionLength);
        }
        if (VERBOSE) {
            Serial.print("Publish message: "); Serial.println(msg);
        }
        client.publish(outTopic, msg);
    }

    // Stuff to do at given time intervals.
    if (now - lastRun > (PUBLISH_INTERVAL * 1000)) {
        lastRun = now;
        char valueLightChar[10];
        float valueLight = light.readValue();
        dtostrf(valueLight, 3, 1, valueLightChar);
        // if (DEBUG) {
        //     Serial.print(", Light: "); Serial.print(valueLightChar);
        // }

        // Sending to MQTT
        // snprintf (msg, 150, "{ \"chipId\": %d, \"light\": %s, \"motion\": %d, \"switch\": %d }",
        //     chipId, valueLightChar, valueMotion, valueSwitch);
        snprintf (msg, 150, "{ \"chipId\": %d, \"light\": %s }", nodemcuChipId, valueLightChar);
        if (VERBOSE) {
            Serial.print("Publish message: "); Serial.println(msg);
        }
        client.publish(outTopic, msg);

        if (DEEP_SLEEP) {
            Serial.print("Entering deep sleep mode for "); Serial.print(SLEEP_DELAY_IN_SECONDS); Serial.println(" seconds...");
            ESP.deepSleep(SLEEP_DELAY_IN_SECONDS * 1000000, WAKE_RF_DEFAULT);
        }
    }
}

