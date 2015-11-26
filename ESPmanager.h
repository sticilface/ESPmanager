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

To Upload

  upload the contents of the data folder with MkSPIFFS Tool ("ESP8266 Sketch Data Upload" in Tools menu in Arduino IDE)
  or you can upload the contents of a folder if you CD in that folder and run the following command:
  for file in `ls -A1`; do curl -F "file=@$PWD/$file" X.X.X.X/espman/upload; done

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
#include <ESP8266HTTPUpdateServer.h>


//#define DEBUG_YES
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

//  These are used to place files into the /espman/directoy...  Directory is used as it has lower memory overhead than 5 seperate requesthandlers.
static const uint8_t file_no = 6;
static const char * _jq1 =  "/jq1.11.1.js.gz";
static const char * _jq2 =  "/jqm1.4.5.css.gz";
static const char * _jq3 =  "/jqm1.4.5.js.gz";
static const char * _jq4 =  "/configjava.js";
static const char * _gif1 = "/ajax-loader.gif"; 
static const char * _htm1 = "/config.htm";
static const char * fileslist[file_no] = {_jq1, _jq2, _jq3, _jq4, _htm1, _gif1} ; // ,jq4,htm1,htm2,htm3};


class ESPmanager
{
public:
	//ArduinoOTA* ota_server = NULL;
	ESPmanager(ESP8266WebServer & HTTP, FS & fs = SPIFFS, const char* host = NULL, const char* ssid = NULL, const char* pass = NULL);
	~ESPmanager();
	void begin();
	void handle();
	static String IPtoString(IPAddress address);
	static IPAddress StringtoIP(const String IP_string);
	static void printdiagnositics();
	static String formatBytes(size_t bytes);
	static bool StringtoMAC(uint8_t *mac, const String &input);
	static void urldecode(char *dst, const char *src); // need to check it works to decode the %03... for :
	//static void sendJsonObjecttoHTTP( const JsonObject & root, ESP8266WebServer & _HTTP);
	template <class T> static void sendJsontoHTTP( const T& root, ESP8266WebServer & _HTTP) ;

	bool Wifistart();

private:




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

	void handleFileUpload();  // Thank to Me-No-Dev and the FSBrowser for this function .




	const char * C_null = "";
	const char * _host = NULL;
	const char * _ssid = NULL;
	const char * _pass = NULL;
	const char * _APpass = NULL;
	const char * _APssid = NULL;
	uint8_t * _STAmac = NULL;
	uint8_t * _APmac = NULL;


	FS & _fs;
	ESP8266WebServer & _HTTP;
	ESP8266HTTPUpdateServer httpUpdater;


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
	//File * fsUploadFile;

};


template<size_t CAPACITY>
class BufferedPrint : public Print
{
public:
	BufferedPrint(ESP8266WebServer & HTTP) : _size(0)
	{
		_client = HTTP.client();
	}

	BufferedPrint(WiFiClient & client) : _client(client), _size(0)
	{
	}

	~BufferedPrint() {
		_client.stop();
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
		_client.write( (const char *)_buffer, _size);
		_size = 0;
	}

private:
	WiFiClient _client;
	size_t _size;
	char _buffer[CAPACITY];
};


