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
#include "ESPMAN.h"

#define SETTINGS_FILE "/espman/settings.txt"
// #define SALT_LENGTH 20
#define ESPMANVERSION "1.0-async"
#define SETTINGS_FILE_VERSION 2
#define ESPMAN_USE_UPDATER 1 

//#define Debug_ESPManager Serial

#if defined(Debug_ESPManager)
 #define ESPMan_Debug(x)    Debug_ESPManager.print(x)
 #define ESPMan_Debugln(x)  Debug_ESPManager.println(x)
 #define ESPMan_Debugf(...) Debug_ESPManager.printf(__VA_ARGS__)

 //#define ESPMan_Debugf(_1, ...) Debug_ESPManager.printf_P( PSTR(_1), ##__VA_ARGS__) //  this saves around 5K RAM...

 //#define ESPMan_Debugf_P(...) DEBUG_ESP_PORT.printf_P(__VA_ARGS__)
   #pragma message("DEBUG enabled for ESPManager.")
#else
 #define ESPMan_Debug(x)    {}
 #define ESPMan_Debugln(x)  {}
 #define ESPMan_Debugf(...) {}
 //#define ESPMan_Debugf_P(...) {}
#endif


// #ifdef Debug_ESPManager
//   #define DISABLE_MANIFEST
// #endif



static const char _compile_date_time[] = __DATE__ " " __TIME__;
//struct tm;

using namespace ESPMAN;


class ESPmanager
{
public:
        ESPmanager(AsyncWebServer & HTTP, FS & fs = SPIFFS);
        ~ESPmanager();
        int begin();
        void handle();
        static String formatBytes(size_t bytes);
        static bool StringtoMAC(uint8_t *mac, const String &input);
        static void urldecode(char *dst, const char *src);   // need to check it works to decode the %03... for :
        static String file_md5 (File& f);
        template <class T = JsonObject> static void sendJsontoHTTP( const T& root, AsyncWebServerRequest *request);
        String getHostname();
        uint32_t trueSketchSize();
        String getSketchMD5();
        AsyncEventSource & getEvent();
        size_t event_printf(const char * topic, const char * format, ... );
        void upgrade(String path = String());
        void factoryReset(); 
//        struct tm * getCompileTime();
        int save();
        // void resetManifest() {
        //   _randomvalue = random(0,25000000);
        // }

        bool portal() {
          return _dns;
        }

        void enablePortal();
        void disablePortal(); 

private:

        void _HandleDataRequest(AsyncWebServerRequest *request);
//        void _handleManifest(AsyncWebServerRequest *request);
        void _handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
        void _upgrade(const char * path);

#ifdef ESPMAN_USE_UPDATER
        int   _DownloadToSPIFFS(const char * url, const char * path, const char * md5 = nullptr);
        int   _parseUpdateJson(uint8_t *& buff, DynamicJsonBuffer & jsonBuffer, JsonObject *& root, const char * path);
        void  _HandleSketchUpdate(AsyncWebServerRequest *request);
#endif

        int _getAllSettings(); //  gets settings to settings ptr, uses new if it doesn't exist.  overwrite current data
        int _getAllSettings(settings_t & set); //only populates the set... used to retrieve certain vailue...
        int _saveAllSettings(settings_t & set);
        int _initialiseAP( settings_t::AP_t & settings);
        int _initialiseAP(bool override = false); //  reads the settings from SPIFFS....  then calls _initialiseAP(ESPMAN::AP_settings_t settings);
        int _initialiseSTA(); //  reads the settings from SPIFFS....  then calls _initialiseAP(ESPMAN::STA_settings_t settings);
        int _initialiseSTA( settings_t::STA_t & settings);
        //int _autoSDKconnect();
        int _emergencyMode(bool shutdown = false);
        //void _applyPermenent(settings_t & set);
        void _sendTextResponse(AsyncWebServerRequest * request, uint16_t code, const char * text); 

        void _removePreGzFiles(); 

        // String _hash(const char * pass);
        // bool _hashCheck(const char * password, const char * hash) ;


        FS & _fs;
        AsyncWebServer & _HTTP;
        AsyncEventSource _events;
        DNSServer * _dns{nullptr};
        bool save_flag {false};
        bool _mDNSenabled {true};
        uint32_t _APtimer {0};
        uint32_t _APtimer2 {0};
        int _wifinetworksfound {0};
        bool _OTAupload {true};
        std::function<bool(void)> _syncCallback {nullptr};
        ap_boot_mode_t _ap_boot_mode {NO_STA_BOOT};
        no_sta_mode_t _no_sta_mode {NO_STA_NOTHING};
        //bool _configured {false};
        // const char * _perminant_host {nullptr};
        // const char * _perminant_ssid {nullptr};
        // const char * _perminant_pass {nullptr};
        bool _APenabledAtBoot {false};
        settings_t * _settings {nullptr};
        uint32_t _updateFreq = 0;
        uint32_t _updateTimer = 0;

        uint32_t _randomvalue;

        const byte DNS_PORT = 53;
        int8_t WiFiresult = -1;

        //AsyncWebRewrite * _portalreWrite = nullptr; 


#ifdef Debug_ESPManager
        void _dumpSettings();
        void _dumpSTA(settings_t::STA_t & set);
        void _dumpAP(settings_t::AP_t & set);
        void _dumpGEN(settings_t::GEN_t & set);
#endif

};
