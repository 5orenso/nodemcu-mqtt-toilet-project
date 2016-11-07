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

#define SLEEP_DELAY_IN_SECONDS  30

#define SWITCH_SENSOR_PIN D5
#define LIGHT_SENSOR_PIN A0
#define MOTION_SENSOR_PIN D6
#define SHOCK_SENSOR_PIN D7

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

const char* mqtt_server = MQTT_SERVER;
const int   mqtt_port = MQTT_PORT;

const char* outTopic = MQTT_OUT_TOPIC;
const char* inTopic = MQTT_IN_TOPIC;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[150];
int chipId = ESP.getChipId();

int pirState = LOW;             // we start, assuming no motion detected


int knockVal = HIGH; // This is where we record our shock measurement
boolean bAlarm = false;
unsigned long lastKnockTime; // Record the time that we measured a shock
int knockAlarmTime = 500; // Number of milli seconds to keep the knock alarm high


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
    pinMode(SWITCH_SENSOR_PIN, INPUT);
    pinMode(LIGHT_SENSOR_PIN, INPUT);
    pinMode(MOTION_SENSOR_PIN, INPUT);
    pinMode(SHOCK_SENSOR_PIN, INPUT);
    Serial.begin(115200);
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
    char valueLightChar[10];
    char valueSwitchChar[10];
    char valueMotionChar[10];

    knockVal = digitalRead(SHOCK_SENSOR_PIN);
    if (knockVal == LOW) {
        lastKnockTime = millis(); // record the time of the shock
        // The following is so you don't scroll on the output screen
        if (!bAlarm){
            Serial.println("KNOCK, KNOCK");
            bAlarm = true;
        }
    } else {
        if((millis()-lastKnockTime) > knockAlarmTime  &&  bAlarm){
            Serial.println("no knocks");
            bAlarm = false;
        }
    }


    if (now - lastMsg > 1000) {
        lastMsg = now;
        float valueLight = analogRead(LIGHT_SENSOR_PIN);
        Serial.print("Light: "); Serial.print(valueLight);

        float valueSwitch = digitalRead(SWITCH_SENSOR_PIN);
        Serial.print(", Micro Switch: "); Serial.print(valueSwitch);

        int valueMotion = 0;            // variable for reading the pin status
        valueMotion = digitalRead(MOTION_SENSOR_PIN);   // read input value
        if (valueMotion == HIGH) {             // check if the input is HIGH
            // digitalWrite(ledPin, HIGH);  // turn LED ON
            if (pirState == LOW) {
                // we have just turned on
                Serial.println(", Motion detected!");
                // We only want to print on the output change, not state
                pirState = HIGH;
            }
            } else {
                // digitalWrite(ledPin, LOW); // turn LED OFF
                if (pirState == HIGH){
                    // we have just turned of
                    Serial.println(", Motion ended!");
                    // We only want to print on the output change, not state
                    pirState = LOW;
                }
        }

        Serial.println();

        // Prepare and send to MQTT
        // dtostrf(valueLight, 3, 2, valueLightChar); // first 2 is the width including the . (1.) and the 2nd 2 is the precision (.23)
        // dtostrf(valueSwitch, 3, 2, valueSwitchChar);
        // dtostrf(valueMotion, 3, 2, valueMotionChar);
        // snprintf (msg, 150, "{ \"chipId\": %d, \"light\": %s, \"switch\": %s, \"motion\": %s }", chipId, valueLightChar, valueSwitchChar, valueMotionChar);
        // Serial.print("Publish message: ");
        // Serial.println(msg);
        // client.publish(outTopic, msg);

        // Do deep sleep if you want to:
        // Serial.print("Entering deep sleep mode for ");
        // Serial.print(SLEEP_DELAY_IN_SECONDS);
        // Serial.println(" seconds...");
        // ESP.deepSleep(SLEEP_DELAY_IN_SECONDS * 1000000, WAKE_RF_DEFAULT);
    }
}

