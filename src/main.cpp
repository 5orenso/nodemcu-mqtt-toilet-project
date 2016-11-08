/*
 It connects to an MQTT server then:
  - publishes "hello world" to the topic "outTopic" every two seconds
  - subscribes to the topic "inTopic", printing out any messages
    it receives. NB - it assumes the received payloads are strings not binary
  - If the first character of the topic "inTopic" is an 1, switch ON the ESP Led,
    else switch it off
 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.
*/
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <LightSensor.h>
#include <PirSensor.h>
#include <SwitchSensor.h>

#define DEBUG false
#define VERBOSE true

#define DEEP_SLEEP false

#define PUBLISH_INTERVAL 30
#define SLEEP_DELAY_IN_SECONDS  30

#define SWITCH_SENSOR_PIN D5
#define LIGHT_SENSOR_PIN A0
#define MOTION_SENSOR_PIN D6

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

const char* mqtt_server = MQTT_SERVER;
const int   mqtt_port = MQTT_PORT;

const char* outTopic = MQTT_OUT_TOPIC;
const char* inTopic = MQTT_IN_TOPIC;

WiFiClient espClient;
PubSubClient client(espClient);
long lastRun = millis();
char msg[150];
int chipId = ESP.getChipId();

LightSensor light = LightSensor(LIGHT_SENSOR_PIN, 30, false);
PirSensor motion = PirSensor(MOTION_SENSOR_PIN, 2, false, false);
SwitchSensor mySwitch = SwitchSensor(SWITCH_SENSOR_PIN, 1, false, false);

long motionSessionStart, switchSensorStart;

void setup_wifi() {
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print("."); Serial.print(ssid);
        delay(500);
    }

    randomSeed(micros());

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
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


void reconnect() {
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
    light.begin();
    motion.begin();
    mySwitch.begin();
    setup_wifi();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

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
            snprintf(msg, 150, "{ \"chipId\": %d, \"motion\": %d }", chipId, motionStateChange);
        } else {
            long motionSessionLength = millis() - motionSessionStart;
            snprintf(msg, 150, "{ \"chipId\": %d, \"motion\": %d, \"motionSessionLength\": %d }", chipId, motionStateChange, motionSessionLength);
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
            snprintf(msg, 150, "{ \"chipId\": %d, \"switch\": %d }", chipId, switchStateChange);
        } else {
            long switchSessionLength = millis() - switchSensorStart;
            snprintf(msg, 150, "{ \"chipId\": %d, \"switch\": %d, \"switchSessionLength\": %d }", chipId, switchStateChange, switchSessionLength);
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
        if (DEBUG) {
            Serial.print(", Light: "); Serial.print(valueLightChar);
        }

        // int valueMotion = round(motion.readValue());
        // if (DEBUG) {
        //     Serial.print("Motion: "); Serial.println(valueMotion);
        // }

        // int valueSwitch = round(mySwitch.readValue());
        // if (DEBUG) {
        //     Serial.print("Switch: "); Serial.println(valueSwitch);
        // }

        // Sending to MQTT
        // snprintf (msg, 150, "{ \"chipId\": %d, \"light\": %s, \"motion\": %d, \"switch\": %d }",
        //     chipId, valueLightChar, valueMotion, valueSwitch);
        snprintf (msg, 150, "{ \"chipId\": %d, \"light\": %s }", chipId, valueLightChar);
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

