/*--------------------------------------------------------------------
Settings Manager for Arduino ESP8266
Andrew Melvin - Sticilface

Requires data folder to be uploaded 

ToDo

*** AP timeout options / what do do when WiFi fails 

1) Log Serial output to File and back
2) 
4) Async wifi management using my own init callback instead of setup...for wifi services... 
5) Add character checking to SSID / HOST 
6) Add ability to upload bin to SPIFFS and switch between them.
7) Download from HTTP
8) ? File Manager
9) 
 
--------------------------------------------------------------------*/


#pragma once

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <FS.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <functional>


#define DEBUG_YES
#define SETTINGS_FILE "/espman/settings.txt"
#define ESPMANVERSION "1.0"

#ifdef DEBUG_YES
	#define Debug(x)    Serial.print(x)
	#define Debugln(x)  Serial.println(x)
	#define Debugf(...) Serial.printf(__VA_ARGS__)
#else
	#define Debug(x)    {}
	#define Debugln(x)  {}
	#define Debugf(...) {}
#endif


#define cache  ICACHE_FLASH_ATTR


static const char _compile_date_time[] = __DATE__ " " __TIME__;


namespace fs {
class FS;
}

class Settingsmanager  
{
public:
	//ArduinoOTA* ota_server = NULL;
	Settingsmanager(ESP8266WebServer & HTTP, fs::FS & fs = SPIFFS, const char* host = NULL, const char* ssid = NULL, const char* pass = NULL); 
	~Settingsmanager();
	void begin();
	void handle();
	static String IPtoString(IPAddress address);
	static IPAddress StringtoIP(const String IP_string);
 	static void printdiagnositics();
    static String formatBytes(size_t bytes); 
    static bool StringtoMAC(uint8_t *mac, const String &input); 
    static void urldecode(char *dst, const char *src); // need to check it works to decode the %03... for : 
	static void sendJsonObjecttoHTTP( const JsonObject & root, ESP8266WebServer & _HTTP); 
 	bool Wifistart();

private:

    static const uint8_t file_no = 1; 
    // const char * htm2 = "/edit.htm.gz";
    // const char * htm3 = "/index.htm";
    const char * jq1 =  "/jquery-1.11.1.min.js.gz"; 
    const char * jq2 =  "/jquery.mobile-1.4.5.min.css.gz"; 
    const char * jq3 =  "/jquery.mobile-1.4.5.min.js.gz"; 
    const char * jq4 =  "/configjava.js"; 
    // const char * htm1 = "/config.htm"; 
    // const char * htm2 = "/edit.htm.gz";
    const char * htm3 = "/index.htm";
    const char * items[file_no] = {htm3} ; // ,jq4,htm1,htm2,htm3}; 


	void HandleDataRequest();
	void InitialiseFeatures(); 
	void InitialiseSoftAP();
	void LoadSettings();
	void SaveSettings(); 
	void PrintVariables();
	bool FilesCheck(bool initwifi = true); 
	bool DownloadtoSPIFFS(const char * remotehost, const char * path, const char * file);
	//bool HTTPSDownloadtoSPIFFS(const char * remotehost, const char * fingerprint, const char * path, const char * file); 
    //WiFiClientSecure * SecClient;
    void NewFileCheck(); 
    const char * C_null = ""; 

	const char * _host = NULL;
	const char * _ssid = NULL;
	const char * _pass = NULL;
	const char * _APpass = NULL; 
	const char * _APssid = NULL; 
	uint8_t * _STAmac = NULL; 
	uint8_t * _APmac = NULL; 


	fs::FS & _fs;  
	ESP8266WebServer & _HTTP; 


	uint8_t _APchannel = 1;
	bool _APhidden = false; 
	bool _APenabled = false; 
	bool _OTAenabled = true; 
	bool save_flag = false; 
	bool _DHCP = true; 
	bool _manageWiFi = true; 
	bool _mDNSenabled = true; 
	uint8_t _APrestartmode = 1; 
	uint32_t _APtimer = 0; 

	struct IPconfigs_t {
		IPAddress IP;
		IPAddress GW;
		IPAddress SN;
	};

	IPconfigs_t * _IPs = NULL; // will hold IPs if they are set by the user..  

};


template<size_t CAPACITY>
class BufferedPrint : public Print
{
public:
    BufferedPrint(ESP8266WebServer & HTTP) : _HTTP(HTTP), _size(0)
    {
    }

    virtual size_t write(uint8_t c)
    {
        _buffer[_size++] = c;

        if (_size + 1 == CAPACITY)
        {
            flush();
        }
    }

    void flush()
    {
        _buffer[_size] = '\0';
        _HTTP.sendContent( String(_buffer)); 
        _size = 0;
    }

private:
    ESP8266WebServer & _HTTP; 
    size_t _size;
    char _buffer[CAPACITY];
};


