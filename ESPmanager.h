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


NEW TODO... 

3)  change download serial output to work without Debug_ESPManager, if debug output is enambled. 
4)  Some form of versioning output / control.... 


To Upload

  upload the contents of the data folder with MkSPIFFS Tool ("ESP8266 Sketch Data Upload" in Tools menu in Arduino IDE)
  or you can upload the contents of a folder if you CD in that folder and run the following command:
  for file in `ls -A1`; do curl -F "file=@$PWD/$file" X.X.X.X/espman/upload; done


--------------------------------------------------------------------*/


#pragma once

#include "Arduino.h"
#include <ESP8266WiFi.h>


#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#include <FS.h>
#include <functional>
#include <ArduinoJson.h>


#define SETTINGS_FILE "/espman/settings.txt"
#define ESPMANVERSION "1.2-async"


#define USE_WEB_UPDATER 



#define Debug_ESPManager

#if defined(DEBUG_ESP_PORT) && defined(Debug_ESPManager)
	#define ESPMan_Debug(x)    DEBUG_ESP_PORT.print(x)
	#define ESPMan_Debugln(x)  DEBUG_ESP_PORT.println(x)
	#define ESPMan_Debugf(...) DEBUG_ESP_PORT.printf(__VA_ARGS__)
  	#pragma message("DEBUG enabled for ESPManager.")
#else
	#define ESPMan_Debug(x)    {}
	#define ESPMan_Debugln(x)  {}
	#define ESPMan_Debugf(...) {}
#endif

using namespace std::placeholders;


static const char _compile_date_time[] = __DATE__ " " __TIME__;

//  These are used to place files into the /espman/directoy...  Directory is used as it has lower memory overhead than 5 seperate requesthandlers.
 static const uint8_t file_no = 6;
// static const char * _jq1 =  "/jq1.11.1.js.gz";
// static const char * _jq2 =  "/jqm1.4.5.css.gz";
// static const char * _jq3 =  "/jqm1.4.5.js.gz";
// static const char * _jq4 =  "/configjava.js";
// static const char * _gif1 = "/ajax-loader.gif";
// static const char * _htm1 = "/config.htm";
// static const char * fileslist[file_no] = {_jq1, _jq2, _jq3, _jq4, _htm1, _gif1} ;

// These are used to search for the presence of the required files
static const char * __jq1 =  "/jquery/jq1.11.1.js.gz";
static const char * __jq2 =  "/jquery/jqm1.4.5.css.gz";
static const char * __jq3 =  "/jquery/jqm1.4.5.js.gz";
static const char * __jq4 =  "/espman/espman.js";
static const char * __gif1 = "/jquery/images/ajax-loader.gif";
static const char * __htm1 = "/espman/index.htm";
static const char * TRUEfileslist[file_no] = {__jq1, __jq2, __jq3, __jq4, __htm1, __gif1} ;

#ifdef USE_WEB_UPDATER
// update path
static const char * __updateserver = "http://sticilface.github.io";
static const char * __updatepath = "/espmanupdate.json";
#endif


class ESPmanager
{
public:
	ESPmanager(AsyncWebServer & HTTP, FS & fs = SPIFFS, const char* host = NULL, const char* ssid = NULL, const char* pass = NULL);
	~ESPmanager();
	void begin();
	void handle();

	const char * deviceName() { return _host; }
	
	static String formatBytes(size_t bytes);
	static bool StringtoMAC(uint8_t *mac, const String &input);
	static void urldecode(char *dst, const char *src); // need to check it works to decode the %03... for :
	template <class T> static void sendJsontoHTTP( const T& root, AsyncWebServerRequest *request) ;

	bool Wifistart();

	const char * getHostname() { return _host; };

	static String _file_md5 (File& f);

	void upgrade(String path); 
	enum version_state  { lower = -1, current = 0, higher = 1, failed = 2 };
	static version_state CheckVersion( String current, String check); 



private:

	void _HandleDataRequest(AsyncWebServerRequest *request);
	void InitialiseFeatures();
	void InitialiseSoftAP();
	bool LoadSettings();
	void SaveSettings();
	void PrintVariables();

	bool _FilesCheck(bool initwifi = true);

#ifdef USE_WEB_UPDATER
	bool _DownloadToSPIFFS(const char * url , const char * path, const char * md5 = nullptr);
	bool _upgrade();
	//bool _upgradewrapper(uint8_t * buff); 

	//  new functions to handle updates of individual spiffs files and sketch...
	bool _parseUpdateJson(uint8_t *& buff, DynamicJsonBuffer & jsonBuffer, JsonObject *& root, String path); 
	void _HandleSketchUpdate(AsyncWebServerRequest *request);
#endif

	void _extractkey(JsonObject & root, const char * name, char *& ptr ); 
	//void _NewFilesCheck();
	void handleFileUpload();  // Thank to Me-No-Dev and the FSBrowser for this function .

//	void _WiFiEventCallback(WiFiEvent_t event); 

	const char * C_null = "";

	char * _host = nullptr;
	char * _ssid = nullptr;
	char * _pass = nullptr;

	const char * _ssid_hardcoded = nullptr;
	const char * _pass_hardcoded = nullptr;

	char * _APpass = nullptr;
	char * _APssid = nullptr;
	char * _OTApassword = nullptr;
	
	uint8_t * _STAmac = nullptr;
	uint8_t * _APmac = nullptr;


	FS & _fs;
	AsyncWebServer & _HTTP;
	//ESP8266HTTPUpdateServer httpUpdater;
	HTTPClient* httpclient{nullptr}; 

	uint8_t _APchannel{1};
	bool _APhidden{false};
	bool _APenabled{false};
	bool _STAenabled{true}; 
	bool _OTAenabled{true};
	bool save_flag{false};
	bool _DHCP{true};
	bool _manageWiFi{true};
	bool _mDNSenabled{true};
	uint8_t _APrestartmode{2};   // 1 = none, 2 = 5min, 3 = 10min, 4 = whenever : 0 is reserved for unset...
	uint32_t _APtimer{0};
	int _wifinetworksfound{0};
	//AsyncWebServerRequest * _wifiRequestHandler{nullptr}; 

	std::function<bool(void)> _syncCallback{nullptr}; 

	struct IPconfigs_t {
		IPAddress IP;
		IPAddress GW;
		IPAddress SN;
	};

	IPconfigs_t * _IPs = NULL; // will hold IPs if they are set by the user..

//  Strings.... 
	const char * _pdeviceid = "deviceid"; 


};


/*


upgrade template

{
	"filecount" : no_of_file,
	"files" : [
		{
			"index" : index,
			"location" : "relative web path",
			"saveto" : "SPIFFS location",
			"md5" : "checksum"
		}
	]

}


*/



