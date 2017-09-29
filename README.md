# onsite
Workplace arrival/departure detector using ESP 8266 and card reader.
## Features
* configurable working modes / display information using menu
* display local time and date synchronized by NTP protocol
* display actual IP address
* display SSID of connected network
* read UID of RFID card and send it to Google Script
* Google Script will translate UID to user name and store the date + time of first and last
visit during day to Google Sheet
* display the card owner name and duration between first and last visit
* card cloning mode, separated to Read and Write phases

## Used hardware parts
* ESP8266 NodeMcu WIFI IoT board D1 mini with 4MB Flash
* PCF8574P DIP-16 8-bit I/O Expander
* RFID module RC522 Kit 13.56 Mhz
* 16x2 Character LCD Display Module 1602/HD44780 Controller blue backlight
* I2C Logic Level Converter Bi-Directional Module 5V to 3.3V
* 2.54mm 2 x 40 Pin Male Double Row Pin Header Strip
* Double side Prototype PCB Tinned Universal board 5x7 cm
* Rotary Encoder with Push Button Switch
* 6mm Shaft Hole Grip Knob
* color wires
* 2x 10KOhm resistor, 1x 5KOhm small potenciometer

Note: it is better purchase the LCD with integrated I2C interface, than to
build own using PFC8574P Expander. Own solution works not with 3.3V. It
was necessary power up display with 5V and separate the voltage of I2C
bus using Level Covertor (ESP8266 pins are not 5V tolerant).

## Used development IDE and libraries
Arduino IDE 1.8 or later, installed support for ESP8266 Development boards

Library manager used to install build-in libraries:
* Multiple ESP8266 libs installed with development board support (ESP8266mDNS, ...)
* Encoder by Paul Stoffregen 1.4.1
* NtpClientLib by German Martin 2.0.5
* Pushbutton by Polohu 2.0.0
* Time by Michael Margolis 1.5.0
* Task by Makuna 1.1.4

Other used libraries (directly merged to project sources)
* [MENWIZ](https://github.com/brunialti/MENWIZ) Menu wizard for LCD displays
* [HttpsRedirect](https://github.com/electronicsguy/ESP8266/tree/master/HTTPSRedirect) 
Used to SSL communication with Google API
* [LiquidCrystalI2C](https://github.com/agnunez/ESP8266-I2C-LCD1602) LCD library
modified for ESP8266
* [RemoteDebug](https://github.com/JoaoLopesF/ESP8266-RemoteDebug-Telnet) Provide
debug messages over Telnet server


## How it works

Google script is published as Web Application. It should be called with single parameter
cid containing the UID of RFID card as value:
<pre>
https://script.google.com/macros/s/AKfycbzd8PhytMnEhjU1GKAiXakhd8NDdnDEe2lBbhGbaVh1mNV3lQ/exec?cid=C3E602E9
</pre>
The script is using linked [Google sheet](https://docs.google.com/spreadsheets/d/189UJD2kwukoMbj9b04qCmCyUebviKi_xK6nt08bwLWs/edit#gid=0) as database. 

First sheet "map" contains
mapping from RFID UID to user name. For 16 character display is name limited to 10 characters.
The sheet "template" serve as template for creating new sheets (each month one).

If unknown card ID is delivered, script will create new mapping with default name
"new-cardid".

Script sends back the name of user and the duration between first and last stored 
time as response. The response is displayed on LCD display.

