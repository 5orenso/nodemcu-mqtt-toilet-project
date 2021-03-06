# NodeMCU toilet project integrated with AWS IoT

__This is work in progress. I've just uploaded my CRUD project files for now. Please be patient with me.__

```ascii
 --------     ---------     --------     -----     ----------     -------------
| sensor |-->| NodeMCU |   | MQTT   |   | AWS |   | AWS      |   | AWS         |
 --------   ^| ESP8266 |-->| Broker |-->| IoT |-->| IoT Rule |-->| Elasticache |
 --------  /  ---------     --------     -----     ----------     -------------
| sensor |/
 --------
```

### TL;DR

![Toilet project sketch](img/fritzing-toilet-project.jpg)

* Hook up your sensor: `SWITCH` = `D5`, `LIGHT` = `A0` and `MOTION` = `D6`.
* Run a couple of commands:
```bash
$ cp ./platformio-dist.ini platformio.ini
$ pio lib install PubSubClient

# My own libraries from Platformio.org:
$ pio lib install GenericAnalogSensor
$ pio lib install MqttUtil
$ pio lib install PirSensor
$ pio lib install SwitchSensor
```

All custom libraries can be found in this repo: https://github.com/5orenso/arduino-libs

I've registered all inside PlatformIO, but I'm waiting for approval.


Edit `platformio.ini` with your credentials and other settings.

Upload and run your code:
```bash
$ pio run -e nodemcuv2 -t upload && pio serialports monitor -b 115200
```

Weee :-)

... more to come.

## Other resources

* [Getting started with NodeMCU, Arduino or Wemos D1 mini](https://github.com/5orenso/arduino-getting-started)
* [NodeMCU code for the toilet sensors integrated with AWS IoT](https://github.com/5orenso/nodemcu-mqtt-toilet-project)
* [NodeMCU code for the toilet lights integrated with AWS IoT](https://github.com/5orenso/nodemcu-mqtt-toilet-project-display)
* [NodeMCU code for home sensors integrated with AWS IoT](https://github.com/5orenso/nodemcu-mqtt-home-sensors)
* [NodeMCU code for BME280 sensor integrated with AWS IoT](https://github.com/5orenso/nodemcu-mqtt-bme280)
* [NodeMCU code for DallasTemperature sensor integrated with AWS IoT](https://github.com/5orenso/nodemcu-mqtt-dallastemperature)
* [Arduino code for Neopixel animations](https://github.com/5orenso/nodemcu-neopixel-animations)
* [AMI for MQTT Broker](https://github.com/5orenso/aws-ami-creation)
* [All PlatforIO.org libs by Sorenso](http://platformio.org/lib/search?query=author%253A%2522Sorenso%2522)

## Contribute

Please contribute with pull-requests.
