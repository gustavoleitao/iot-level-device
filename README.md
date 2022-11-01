# iot-level-device

ESP8266 module to collect data from Jsn-sr04t and send to Iot platforms through MQTT protocol.

## Getting start

The device will start in AP mode. To configure the paramters, connect to Logique Device AP SSID Network with password 11235813. Configure all the paramters and wait for reboot.

The following table shows the meaning of the led:

| Led          | Description                                  |
|--------------|----------------------------------------------|
| Single blink | The data was collected from Jsn-sr04t sensor |
| Double blink | The data was sent to IoT platform.           |

## Supported platforms

This device supports sending data to ThingsBoard and Ubidots platforms.


