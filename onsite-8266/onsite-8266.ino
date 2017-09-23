/*
 Name:		onsite-8266.ino
 Created:	09/09/2017
 Author:	srkal@quick.cz
*/

#include <TimeLib.h>
#include "WiFiConfig.h"
#include <NtpClientLib.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <Task.h>
#include "LiquidCrystal_I2C.h"
#include <Wire.h>
#include <MFRC522.h>
#include "HTTPSRedirect.h"
#include "DebugMacros.h"
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include "RemoteDebug.h"
#include <Encoder.h>
#include <Pushbutton.h>
#include "MENWIZ.h"

#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H
#define YOUR_WIFI_SSID "fill SSID here and comment out include WiFiConfig.h if WiFiConfig.h does not exist"
#define YOUR_WIFI_PASSWD "Your Password"
#endif

#define ClockPin 3  // labeled either as CLK or as A
#define DataPin 1   // labeled either as DT or as B
#define ButtonPin 2 // labeled as SW

//used library objects
TaskManager taskManager;
LiquidCrystal_I2C lcd(39, 16, 2); //16 chars, 2 rows
MFRC522 mfrc522(15, 0);  //15=D8, 0=D3
HTTPSRedirect* client = nullptr;
RemoteDebug Debug;
Encoder rotEncoder(ClockPin, DataPin);
Pushbutton rotButton(ButtonPin);
menwiz menu;

char displayBuffer[36];
int displayMode = 1;  //selected mode displayed information no first row
unsigned long msInterval=0;  //detection of long press of rotary encoder button
String lastCardReaded = "";  //some card has been readed, need to send this information to google
String secondRow = "";       //last system message prepared to display on second row
const String host = "script.google.com";
const String url = "/macros/s/AKfycbx8QSHbZ-a4OXdKQ4CR6QT2ArNe_MyrpBMnVqnDvAizsRrLPTYg/exec?cid=";
const int httpsPort = 443;
const char *hostName = "onsite";
long rotPosition  = -999; //rotary encoder position

void OnUpdateTaskScanI2C(uint32_t deltaTime);
void OnUpdateTaskSendData(uint32_t deltaTime);

FunctionTask taskScanI2C(OnUpdateTaskScanI2C, MsToTaskTime(10000));
FunctionTask taskSendData(OnUpdateTaskSendData, MsToTaskTime(2000));


void onSTAGotIP(WiFiEventStationModeGotIP ipInfo) {
  if (Debug.isActive(Debug.DEBUG)) {
    Debug.println("Got IP: " + ipInfo.ip.toString());
  }
  NTP.begin("pool.ntp.org", 1, true);
	NTP.setInterval(3600);
	digitalWrite(LED_BUILTIN, LOW);

  if (!MDNS.begin(hostName)) {
    secondRow = "DNS ERROR";
  } else {
    secondRow = "DNS OK";   
    MDNS.addService("telnet", "tcp", 23);
  }
 
}

void onSTADisconnected(WiFiEventStationModeDisconnected event_info) {
  if (Debug.isActive(Debug.ERROR)) {
    Debug.println("Disconnected from SSID: " + event_info.ssid);
    Debug.println("Reason: " + event_info.reason);
  }
  digitalWrite(LED_BUILTIN, HIGH);
}

void OnUpdateTaskSendData(uint32_t deltaTime) {
    if (lastCardReaded.length() > 0) {
      if (Debug.isActive(Debug.DEBUG)) {
        Debug.println("Sending data about card " + lastCardReaded);
      }
      
      if (client->connect(host.c_str(), httpsPort) == 1) {
        if (Debug.isActive(Debug.DEBUG)) {
          Debug.println("Sending data to Google OK ");
        }
        
        if (client->GET((url+lastCardReaded).c_str(), host.c_str())) {
          lastCardReaded = "";
          secondRow = client->getResponseBody().substring(0,client->getResponseBody().length()-2);
        } else {
          if (Debug.isActive(Debug.ERROR)) {
            Debug.println("Sending data to Goole ERROR " + client->getStatusCode());
          }
          secondRow = "Http Status: " + client->getStatusCode();
        }
      } else {
        if (Debug.isActive(Debug.ERROR)) {
          Debug.println("Sending data to Goole ERROR");
        }
      }
    }
    
    taskManager.StartTask(&taskSendData);
}

void OnUpdateTaskScanI2C(uint32_t deltaTime) {
  byte error, address;
  if (Debug.isActive(Debug.DEBUG)) {
    Debug.println("Scanning... ");
  }
  
  Wire.begin(4,5);
  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
 
    if (error == 0) {
      if (Debug.isActive(Debug.INFO)) {
        Debug.println("I2C device found at address " + address);
      }
    } else if (error == 4) {
      if (Debug.isActive(Debug.ERROR)) {
        Debug.println("Unknow error at address " + address);
      }
    }    
  }
  if (Debug.isActive(Debug.DEBUG)) {
    Debug.println("Scan done.");
  }
  taskManager.StartTask(&taskScanI2C);
}

void setup() {
	static WiFiEventHandler e1, e2;

	WiFi.mode(WIFI_STA);
  WiFi.hostname(hostName);
	WiFi.begin(YOUR_WIFI_SSID, YOUR_WIFI_PASSWD);
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);

	NTP.onNTPSyncEvent([](NTPSyncEvent_t ntpEvent) {
		if (ntpEvent) {
      if (Debug.isActive(Debug.ERROR)) {
        Debug.print("Time Sync error: ");
      }
      
			if (ntpEvent == noResponse) {
        if (Debug.isActive(Debug.ERROR)) {
          Debug.println("NTP server not reachable");
        }
			} else if (ntpEvent == invalidAddress)
        if (Debug.isActive(Debug.ERROR)) {
          Debug.println("Invalid NTP server address ");
        }
		} else {
      if (Debug.isActive(Debug.DEBUG)) {
        Debug.println("Got NTP time: " + NTP.getTimeDateString(NTP.getLastNTPSync()));
      }
		}
	});

	WiFi.onEvent([](WiFiEvent_t e) {
    if (Debug.isActive(Debug.DEBUG)) {
      Debug.println("Event wifi: " + e);
    }
	});
	e1 = WiFi.onStationModeGotIP(onSTAGotIP);// As soon WiFi is connected, start NTP Client
	e2 = WiFi.onStationModeDisconnected(onSTADisconnected);

  Debug.begin("onsite");
  Debug.setResetCmdEnabled(true);
  
  lcd.begin(4,5);  //use dafault i2c pins SDA=4, SCL=5
  lcd.backlight();
  lcd.home();                // At column=0, row=0
  secondRow = "STARTING";
  menu.begin(&lcd,16,2);
  initMenu();
  SPI.begin();
  mfrc522.PCD_Init();   // Init MFRC522
  PCD_DumpVersion();  // Show details of PCD - MFRC522 Card Reader details

  //Google rest communication
  client = new HTTPSRedirect(httpsPort);
  client->setContentTypeHeader("application/json"); 

  taskManager.StartTask(&taskSendData);
  //taskManager.StartTask(&taskScanI2C);
}

/**
 * Menu is activated by any movement of rotary encoder.
 * Menu is plain - single level depth, selected mode must be confirmed by clicking the button.
 */
void initMenu() {
  _menu *r,*s1,*s2;
  _var *v; 
  r=menu.addMenu(MW_ROOT,NULL,F("Menu"));
    s1=menu.addMenu(MW_VAR,r, F("Time Date"));
      s1->addVar(MW_ACTION,myFuncTime);
      s1->setBehaviour(MW_ACTION_CONFIRM, false);
    s1=menu.addMenu(MW_VAR,r, F("IP address"));
      s1->addVar(MW_ACTION,myFuncIP);
      s1->setBehaviour(MW_ACTION_CONFIRM, false);
    s1=menu.addMenu(MW_VAR,r, F("SSID"));
      s1->addVar(MW_ACTION,myFuncSSID);
      s1->setBehaviour(MW_ACTION_CONFIRM, false);

    menu.addUsrNav(simulateNavigation,4);
    menu.addUsrScreen(userDisplay, 3000);
}

/**
 * Basic information screen displayed after 3 seconds of menu inactivity.
 * First row is configurable, second contains last system message.
 */
void userDisplay(){
  String displayText = "";
  switch (displayMode) {
    case 1: displayText += NTP.getTimeDateString().substring(0,14) + "\n";
      break;
    case 2: displayText += WiFi.localIP().toString() + "\n";
      break;
    case 3: displayText += WiFi.SSID() + "\n";
      break;
  }
  displayText += secondRow + "\n";
  if (Debug.isActive(Debug.DEBUG)) {
    Debug.println(displayText);
  }
  displayText.toCharArray(displayBuffer, displayText.length());
  menu.drawUsrScreen(displayBuffer);
}

/**
 * Use rotary encoder states and rotary encoder button to drive the menu.
 * Long press of button simulate escape key.
 */
int simulateNavigation() {
  int pressed = MW_BTNULL;
  long position = rotEncoder.read();
  if (position != rotPosition) {
    if ((position > rotPosition) && (position - rotPosition > 2)) {
      if (Debug.isActive(Debug.DEBUG)) {
        Debug.println("Doprava");
      }
      pressed = MW_BTD;
      rotPosition = position;
    }  else {
      if (rotPosition -position > 2) {
        pressed = MW_BTU;
        rotPosition = position;
        if (Debug.isActive(Debug.DEBUG)) {
          Debug.println("Doleva");
        }
      }
    }
  }
  if (rotButton.isPressed()) {
    unsigned long now = millis();
    if (msInterval == 0) {
      msInterval = now;
    }
    if (now - msInterval > 800) {
      pressed = MW_BTE;
      msInterval = now;
      if (Debug.isActive(Debug.DEBUG)) {
        Debug.println("Button ESCAPE");
      }
    }
  } else {
    msInterval = 0;
  }
  if (rotButton.getSingleDebouncedPress()) {
    if (Debug.isActive(Debug.DEBUG)) {
      Debug.println("Button pressed");
    }
    pressed = MW_BTC;
  }

  return pressed;
}

void myFuncTime() {
  if (Debug.isActive(Debug.DEBUG)) {
    Debug.println("MENU TIME");
  }
  displayMode = 1;
}

void myFuncIP() {
  if (Debug.isActive(Debug.DEBUG)) {
    Debug.println("MENU IP");
  }
  displayMode = 2;
}

void myFuncSSID() {
  if (Debug.isActive(Debug.DEBUG)) {
    Debug.println("MENU SSID");
  }
  displayMode = 3;
}

/**
 * Dumps debug info about the connected PCD to display.
 */
void PCD_DumpVersion() {
  byte v = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
  if ((v == 0x00) || (v == 0xFF)) {
    secondRow = "RFID init Error";
  } else {
    secondRow = "RFID init OK";
  }
}

/**
 * Helper to format and display id of card.
 */
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

/**
 * Processing loop called endless.
 */
void loop() {
  taskManager.Loop();
  Debug.handle();
  menu.draw();

  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  lastCardReaded = PCD_GetCardId(&(mfrc522.uid));
  secondRow = "UID: " + lastCardReaded;
  if (Debug.isActive(Debug.DEBUG)) {
    Debug.println("UID: " + lastCardReaded);
  }
}

