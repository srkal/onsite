/*
 Name:		onsite-8266.ino
 Created:	09/09/2017
 Author:	srkal@quick.cz
*/

#include <TimeLib.h>
#include "WiFiConfig.h"
#include <NtpClientLib.h>
#include <ESP8266WiFi.h>
#include <Task.h>
#include <ArduinoLog.h>
#include "LiquidCrystal_I2C.h"
#include <Wire.h>
#include <MFRC522.h>
#include "HTTPSRedirect.h"
#include "DebugMacros.h"

#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H
#define YOUR_WIFI_SSID "fill SSID here and comment out include WiFiConfig.h if WiFiConfig.h does not exist"
#define YOUR_WIFI_PASSWD "Your Password"
#endif

TaskManager taskManager;
LiquidCrystal_I2C lcd(39, 16, 2); //16 chars, 2 rows
MFRC522 mfrc522(15, 0);  //15=D8, 0=D3
String lastCardReaded = "";
const String host = "script.google.com";
const String url = "/macros/s/AKfycbx8QSHbZ-a4OXdKQ4CR6QT2ArNe_MyrpBMnVqnDvAizsRrLPTYg/exec?cid=";
const int httpsPort = 443;
HTTPSRedirect* client = nullptr;

void OnUpdateTaskGetTime(uint32_t deltaTime); 
void OnUpdateTaskScanI2C(uint32_t deltaTime);
void OnUpdateTaskSendData(uint32_t deltaTime);
FunctionTask taskGetTime(OnUpdateTaskGetTime, MsToTaskTime(2000));
FunctionTask taskScanI2C(OnUpdateTaskScanI2C, MsToTaskTime(10000));
FunctionTask taskSendData(OnUpdateTaskSendData, MsToTaskTime(2000));

void onSTAGotIP(WiFiEventStationModeGotIP ipInfo) {
	Log.notice("Got IP: %s" CR, ipInfo.ip.toString().c_str());
	NTP.begin("pool.ntp.org", 1, true);
	NTP.setInterval(360);
	digitalWrite(LED_BUILTIN, LOW);
}

void onSTADisconnected(WiFiEventStationModeDisconnected event_info) {
	Log.notice("Disconnected from SSID: %s" CR, event_info.ssid.c_str());
	Log.notice("Reason: %d" CR, event_info.reason);
	digitalWrite(LED_BUILTIN, HIGH);
}

void OnUpdateTaskGetTime(uint32_t deltaTime) {
    Log.notice ( "%s " CR , NTP.getTimeDateString().c_str());
    lcd.home();
    lcd.print(NTP.getTimeDateString().substring(0,14).c_str());
    taskManager.StartTask(&taskGetTime);
}

void OnUpdateTaskSendData(uint32_t deltaTime) {
    if (lastCardReaded.length() > 0) {
      Log.notice("Sending data about card %s" CR, lastCardReaded.c_str());
      if (client->connect(host.c_str(), httpsPort) == 1) {
        Log.notice("Sending data to Google OK" CR);
        if (client->GET((url+lastCardReaded).c_str(), host.c_str())) {
          lastCardReaded = "";
          lcd.setCursor(0, 1);
          lcd.print(client->getResponseBody().substring(0,client->getResponseBody().length()-2)+"          ");
        } else {
          Log.notice("Sending data to Goole ERROR %i" CR, client->getStatusCode());
          lcd.setCursor(0, 1);
          lcd.print("Http Status: " + client->getStatusCode());
        }
      } else {
        Log.notice("Sending data to Goole ERROR" CR);
      }
    }
    
    taskManager.StartTask(&taskSendData);
}

void OnUpdateTaskScanI2C(uint32_t deltaTime) {
  byte error, address;
  Log.notice( "Scanning... " CR);
  Wire.begin(4,5);
  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
 
    if (error == 0) {
      Log.notice("I2C device found at address %d." CR, address);
    } else if (error == 4) {
      Log.notice("Unknow error at address %d." CR, address);
    }    
  }
  Log.notice("Scan done." CR);
  taskManager.StartTask(&taskScanI2C);
}

void setup() {
	static WiFiEventHandler e1, e2;

	Serial.begin(115200);
  while(!Serial && !Serial.available()){}
  // LOG_LEVEL_SILENT, LOG_LEVEL_FATAL, LOG_LEVEL_ERROR, LOG_LEVEL_WARNING, LOG_LEVEL_NOTICE, LOG_LEVEL_VERBOSE
  Log.begin(LOG_LEVEL_NOTICE, &Serial);
	WiFi.mode(WIFI_STA);
	WiFi.begin(YOUR_WIFI_SSID, YOUR_WIFI_PASSWD);
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);

	NTP.onNTPSyncEvent([](NTPSyncEvent_t ntpEvent) {
		if (ntpEvent) {
      Log.error ( "Time Sync error: ");
			if (ntpEvent == noResponse)
         Log.error ("NTP server not reachable" CR);
			else if (ntpEvent == invalidAddress)
         Log.error ("Invalid NTP server address" CR);
		} else {
      Log.notice("Got NTP time: %s" CR , NTP.getTimeDateString(NTP.getLastNTPSync()).c_str());
		}
	});
	WiFi.onEvent([](WiFiEvent_t e) {
    Log.notice ( "Event wifi -----> %d" CR , e);
	});
	e1 = WiFi.onStationModeGotIP(onSTAGotIP);// As soon WiFi is connected, start NTP Client
	e2 = WiFi.onStationModeDisconnected(onSTADisconnected);

  lcd.begin(4,5);  //use dafault i2c pins SDA=4, SCL=5
  lcd.backlight();
  lcd.home();                // At column=0, row=0
  lcd.print("STARTING         ");   
  SPI.begin();
  mfrc522.PCD_Init();   // Init MFRC522
  PCD_DumpVersion();  // Show details of PCD - MFRC522 Card Reader details

  //Google rest communication
  client = new HTTPSRedirect(httpsPort);
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json"); 

  taskManager.StartTask(&taskGetTime);
  taskManager.StartTask(&taskSendData);
  //taskManager.StartTask(&taskScanI2C);
}

/**
 * Dumps debug info about the connected PCD to display.
 */
void PCD_DumpVersion() {
  byte v = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
  lcd.setCursor(0, 1);
  if ((v == 0x00) || (v == 0xFF)) {
    lcd.print("RFID init Error                ");
  } else {
    lcd.print("RFID init OK          ");
  }
}

String PCD_GetCardId(MFRC522::Uid *uid) {
  char const hex_chars[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
  String id = "";
  for (byte i = 0; i < uid->size; i++) {
    char const byte = uid->uidByte[i];
    id += hex_chars[ ( byte & 0xF0 ) >> 4 ];
    id += hex_chars[ ( byte & 0x0F ) >> 0 ];  
  }
  return id;
}

void loop() {
  taskManager.Loop();
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  lastCardReaded = PCD_GetCardId(&(mfrc522.uid));
  lcd.setCursor(0, 1);
  Log.notice("UID: %s" CR, lastCardReaded.c_str());
  lcd.print("UID: " + lastCardReaded + "              ");
}

