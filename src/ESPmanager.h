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
   9) OTA password using md5...


   NEW TODO...

   3)  change download serial output to work without Debug_ESPManager, if debug output is enambled.
   4)  Some form of versioning output / control....


   To Upload

   upload the contents of the data folder with MkSPIFFS Tool ("ESP8266 Sketch Data Upload" in Tools menu in Arduino IDE)
   or you can upload the contents of a folder if you CD in that folder and run the following command:
   for file in `ls -A1`; do curl -F "file=@$PWD/$file" X.X.X.X/espman/upload; done


   --------------------------------------------------------------------*/


#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <DNSServer.h>
#include <FS.h>
#include <functional>
#include <ArduinoJson.h>
#include "Tasker/src/Tasker.h"
#include "ESPMAN.h"   //  has to go after


//  These are the Features that can be turned off to save more FLASH and RAM space. 
#define ESPMANAGER_SYSLOG /* 16 Bytes */
#define ESPMANAGER_SAVESTACK /* 0 Bytes - */
#define ESPMAN_USE_UPDATER   /* 912 Bytes  */ 
//#define ESPMANAGER_LOG   /*  experimental logging */ 
//#define Debug_ESPManager Serial /* 1760 bytes  */ 

#include <ESPmanSysLog.h>

#ifdef ESPMANAGER_SAVESTACK
#include "SaveStack.h"
#endif

#define SETTINGS_FILE "/espman/settings.json"
// #define SALT_LENGTH 20
#define ESPMANVERSION "2.2-async"
#define SETTINGS_FILE_VERSION 2


//  New logging methods... just send the message to the logging function, otherwise squash it all... 
#ifdef ESPMANAGER_LOG
  #define ESP_LOG(a,b) { _log(a,b) ; }
#else 
 #define ESP_LOG(a,b) { }
#endif

#if defined(Debug_ESPManager)
static File _DebugFile;
//#define ESPMan_Debugf(...) Debug_ESPManager.printf(__VA_ARGS__) //  33,268 K RAM left
#define ESPMan_Debugf(_1, ...) { Debug_ESPManager.printf_P( PSTR( "[%s, line %u] " _1), __func__, __LINE__ ,##__VA_ARGS__); } //  this saves around 5K RAM...  39,604 K ram left
#pragma message("DEBUG enabled for ESPManager.")
#else
#define ESPMan_Debugf(...) {}  // leaves 40,740 K, so flash debug uses 1.1K of ram... 
#endif

static const char _compile_date_time[] = __DATE__ " " __TIME__;


using namespace ESPMAN;


class ESPmanager
{
public:
  ESPmanager(AsyncWebServer & HTTP, FS & fs = SPIFFS);
  ~ESPmanager();
  ESPMAN_ERR_t begin();
  void handle();
  static String formatBytes(size_t bytes);
  static bool StringtoMAC(uint8_t *mac, const String & input);
  static void urldecode(char *dst, const char *src);   // need to check it works to decode the %03... for :
  static String file_md5 (File & f);
  template <class T = JsonObject> static void sendJsontoHTTP( const T & root, AsyncWebServerRequest * request);
  String getHostname();
  myString getError(ESPMAN_ERR_t err); 
  myString getError(int err) { return getError( (ESPMAN_ERR_t)err ); }

  inline uint32_t trueSketchSize();
  inline String getSketchMD5();

  AsyncEventSource & getEvent();
  size_t event_printf(const char * topic, const char * format, ... ) __attribute__((format(printf, 3, 4)));
  size_t event_printf_P(const char * topic, PGM_P format, ... ) __attribute__((format(printf, 3, 4)));

  ESPMAN_ERR_t upgrade(String path = String());
  void factoryReset();
  int save();
  bool portal() { return _dns; }

  ESPMAN_ERR_t enablePortal();
  void disablePortal();

  ASyncTasker & getTaskManager() { return _tasker; }
  ASyncTasker & tasker() { return _tasker; }

#ifdef ESPMANAGER_SYSLOG

  SysLog * logger() { return _syslog; }

  bool log(myString  msg); 
  bool log(uint16_t pri, myString  msg); 
  bool log(myString appName, myString  msg);
  bool log(uint16_t pri, myString appName, myString  msg);

#endif


private:

  void _HandleDataRequest(AsyncWebServerRequest * request);
  void _handleFileUpload(AsyncWebServerRequest * request, String filename, size_t index, uint8_t *data, size_t len, bool final);
  
#ifdef ESPMAN_USE_UPDATER
  ESPMAN_ERR_t _upgrade(const char * path);
  ESPMAN_ERR_t   _DownloadToSPIFFS(const char * url, const char * path, const char * md5 = nullptr, bool overwrite = false);
  ESPMAN_ERR_t   _parseUpdateJson(uint8_t *& buff, DynamicJsonBuffer & jsonBuffer, JsonObject *& root, const char * path);
  void  _HandleSketchUpdate(AsyncWebServerRequest * request);
#else 
  ESPMAN_ERR_t _upgrade(const char * path) {}
#endif

  SysLog * _syslog{nullptr};


  ESPMAN_ERR_t _getAllSettings(); //  gets settings to settings ptr, uses new if it doesn't exist.  overwrite current data
  ESPMAN_ERR_t _getAllSettings(settings_t & set); //only populates the set... used to retrieve certain vailue...
  ESPMAN_ERR_t _saveAllSettings(settings_t & set);
  ESPMAN_ERR_t _initialiseAP( settings_t::AP_t & settings);
  ESPMAN_ERR_t _initialiseAP(bool override = false); //  reads the settings from SPIFFS....  then calls _initialiseAP(ESPMAN::AP_settings_t settings);
  ESPMAN_ERR_t _initialiseSTA(); //  reads the settings from SPIFFS....  then calls _initialiseAP(ESPMAN::STA_settings_t settings);
  ESPMAN_ERR_t _initialiseSTA( settings_t::STA_t & settings);
  ESPMAN_ERR_t _emergencyMode(bool shutdown = false);
  void _sendTextResponse(AsyncWebServerRequest * request, uint16_t code, const char * text);

  void _removePreGzFiles();
  void _initialiseTasks();
  void _APlogic(Task & t);

  void _log(uint16_t pri, myString  msg); 

  // String _hash(const char * pass);
  // bool _hashCheck(const char * password, const char * hash) ;


  FS & _fs;
  AsyncWebServer & _HTTP;
  AsyncEventSource _events;
  DNSServer * _dns{nullptr};
  bool save_flag {false};
//       bool _mDNSenabled {true};
  uint32_t _APtimer {0};
  uint32_t _APtimer2 {0};
  int _wifinetworksfound {0};
//       bool _OTAupload {true};
//       std::function<bool(void)> _syncCallback {nullptr};
  ap_boot_mode_t _ap_boot_mode {NO_STA_BOOT};
  no_sta_mode_t _no_sta_mode {NO_STA_NOTHING};


  bool _APenabledAtBoot {false};
  settings_t * _settings {nullptr};
//        uint32_t _updateFreq = 0;
//        uint32_t _updateTimer = 0;

//        uint32_t _randomvalue;

  const byte DNS_PORT = 53;
  int8_t WiFiresult = -1;

  //AsyncWebRewrite * _portalreWrite = nullptr;

  ASyncTasker _tasker;

  Task * _dnsTask{nullptr};



#ifdef Debug_ESPManager

  void _dumpSettings();
  void _dumpSTA(settings_t::STA_t & set);
  void _dumpAP(settings_t::AP_t & set);
  void _dumpGEN(settings_t::GEN_t & set);
#endif

};
