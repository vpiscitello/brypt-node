# Brypt Node

Installing Arduino IDE:

- https://www.arduino.cc/en/main/software

Setting up the IDE:

1) Open up the Arduino IDE and go to File -> Preferences (CTRL+Comma)
2) Paste the following into Additional Boards Manager URLs: 
- https://adafruit.github.io/arduino-board-index/package_adafruit_index.json,http://arduino.esp8266.com/stable/package_esp8266com_index.json
3) Click 'OK'
4) Go to Tools -> Board -> Boards Manager
5) Install the following:
- esp8266 
- Adafruit AVR Boards
- Adafruit SAMD Boards

# Programming the boards

For the Feather 32u4 Bluefruit LE:

- In Arduino IDE Board Manager select: Adafruit Feather 32u4

- For additional details on using Arduino IDE with this board, please refer to: https://learn.adafruit.com/adafruit-feather-32u4-bluefruit-le/using-with-arduino-ide

For the Feather 32u4 with LoRa Radio: 

- In Arduino IDE Board Manager select: Adafruit Feather 32u4

- For additional details on using Arduino IDE with this board, please refer to: https://learn.adafruit.com/adafruit-feather-32u4-radio-with-lora-radio-module/using-with-arduino-ide

For the Feather M0 Wifi ATWINC1500:

- In Arduino IDE Board Manager select: Adafruit Feather M0

- For additional details on using Arduino IDE with this board, please refer to: https://learn.adafruit.com/adafruit-feather-m0-wifi-atwinc1500/using-with-arduino-ide

For the Feather HUZZAH ESP8266:

- In Arduino IDE Board Manager select: Adafruit Feather HUZZAH ESP8266

- For using the Arduino IDE for this board, please refer to: https://learn.adafruit.com/adafruit-feather-huzzah-esp8266/using-arduino-ide
