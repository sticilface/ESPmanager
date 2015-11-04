/*--------------------------------------------------------------------
Settings Manager for Arduino ESP8266
Andrew Melvin - Sticilface

Requires data folder to be uploaded 

ToDo

1) Save settings to SPIFFS
2) Integrate all wifi management still to do MAC address
4) Async wifi management using my own init callback instead of setup...for wifi services... 
5) Integrate HTTP upload of bin, via web form
6) Add ability to upload bin to SPIFFS and switch between them.
 
--------------------------------------------------------------------*/


#pragma once

#include "Arduino.h"

#include <ESP8266WiFi.h>
#include <FS.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>

#include <functional>

#define cache  ICACHE_FLASH_ATTR


const char _compile_date_time[] = __DATE__ " " __TIME__;
const char version[] = "SettingsManager 1.0";


namespace fs {
class FS;
}

class Settingsmanager  
{
public:
	ArduinoOTA* ota_server;

	Settingsmanager(ESP8266WebServer* HTTP, fs::FS* fs = &SPIFFS, const char* host = NULL, const char* ssid = NULL, const char* pass = NULL); 
	~Settingsmanager();
	void begin();
	void handle();
	static String IPtoString(IPAddress address);
	static IPAddress StringtoIP(const String IP_string);

 	void printdiagnositics();
 	bool Wifistart();

private:

//typedef struct settings_s {
	const char * _host = NULL;
	const char * _ssid = NULL;
	const char * _pass = NULL;
	      char * _APpass = NULL; 
	char * _OTAhost = NULL; 

	String _APssid;
	fs::FS * _fs; 

	uint8_t _APchannel = 1;
//	WiFiMode _mode;     // WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3
//};
	ESP8266WebServer * _HTTP = NULL; 
	bool _DHCP = true; 
	uint8_t _wifinetworksfound = 0; 

	void HandleDataRequest();
	void InitialiseFeatures(); 
	void InitialiseSoftAP();

	bool _APhidden = false; 
	bool _APenabled = false; 
	bool _OTAenabled = true; 



};




