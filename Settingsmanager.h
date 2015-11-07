/*--------------------------------------------------------------------
Settings Manager for Arduino ESP8266
Andrew Melvin - Sticilface

Requires data folder to be uploaded 

ToDo

0) Sort out text string and memory storage of variables.... 
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
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <functional>

#ifdef DEBUG_YES
	#define Debug(x)    Serial.print(x)
	#define Debugln(x)  Serial.println(x)
	#define Debugf(...) Serial.printf(__VA_ARGS__)
	#define Debugflush  Serial.flush
#else
	#define Debug(x)    {}
	#define Debugln(x)  {}
	#define Debugf(...) {}
	#define Debugflush  {}
#endif


#define cache  ICACHE_FLASH_ATTR


const char _compile_date_time[] = __DATE__ " " __TIME__;
const char version[] = "SettingsManager 1.0";


namespace fs {
class FS;
}

class Settingsmanager  
{
public:
	ArduinoOTA* ota_server = NULL;

	Settingsmanager(ESP8266WebServer* HTTP, fs::FS* fs = &SPIFFS, const char* host = NULL, const char* ssid = NULL, const char* pass = NULL); 
	~Settingsmanager();
	void begin();
	void handle();
	static String IPtoString(IPAddress address);
	static IPAddress StringtoIP(const String IP_string);
 	static void printdiagnositics();

 	bool Wifistart();

private:


	void HandleDataRequest();
	void InitialiseFeatures(); 
	void InitialiseSoftAP();
	void LoadSettings();
	void SaveSettings(); 
	void PrintVariables();

    const char * C_true = "true";
    const char * C_false = "false";
    const char * C_null = ""; 

	const char * _host = NULL;
	const char * _ssid = NULL;
	const char * _pass = NULL;
	const char * _APpass = NULL; 
	const char * _APssid = NULL; 


	fs::FS * _fs = NULL; 
	ESP8266WebServer * _HTTP = NULL; 

	uint8_t _APchannel = 1;


	bool _APhidden = false; 
	bool _APenabled = false; 
	bool _OTAenabled = true; 
	bool save_flag = false; 
	bool _DHCP = true; 
	bool _manageWiFi = true; 
	bool _mDNSenabled = true; 


	struct IPconfigs_t {
		IPAddress IP;
		IPAddress GW;
		IPAddress SN;
	};

	IPconfigs_t * _IPs = NULL; // will hold IPs if they are set by the user..  

};




