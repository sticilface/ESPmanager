/** @file .
 *   @brief Main File ESPManager.
 *   
 *   
 *   This is the main file.  ESPManager is a full wifi and update lib for the ESP8266 designed on Arduino. 
 *
 *  
 */

/*--------------------------------------------------------------------
   Settings Manager for Arduino ESP8266
   Andrew Melvin - Sticilface

   Requires data folder to be uploaded

   ToDo

*** AP timeout options / what do do when WiFi fails

   0) Add required files.  then add ClearApp(), which wipes SPIFFS except for espconfig + html... 
   1) Log Serial output to File and back
   2)
   4) Async wifi management using my own init callback instead of setup...for wifi services...
   5) Add character checking to SSID / HOST
   6) Add ability to upload bin to SPIFFS and switch between them.
   7) Download from HTTP
   8) ? File Manager
   9) OTA password using md5...


   NEW TODO...

   3)  change download serial output to work without DEBUGESPMANAGER, if debug output is enambled.
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
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <FS.h>
#include <functional>
#include <ArduinoJson.h>
#include "Tasker/src/Tasker.h"
#include "ESPdeviceFinder/src/ESPdeviceFinder.h"
#include "ESPMAN.h" //  has to go after
#include <ESP8266RTCMemory/src/ESP8266RTCMemory.h>
#include "staticHandler/src/staticHandler.h"
#include "rtcStruct.h"

//  These are the Features that can be turned off to save more FLASH and RAM space.
#define ESPMANAGER_UPDATER      /**< @brief Enable the remote updater, update via http see ::upgrade, uses 1K heap */
#define ESPMANAGER_DEVICEFINDER /**< @brief Enable deviceFinder.  ESPmanager will now locate all other ESPmanager instances, see ::ESPdeviceFinder, uses 200 bytes heap */
//#define DEBUGESPMANAGER Serial /* 1760 bytes  */

#define DEFAULT_AP_PASS "esprocks"            ///< @brief Default password for the ESP AP.  Used for first connecting to the ESPManager.
#define MAX_WIFI_NETWORKS 10                  ///< @brief Max number of WiFi networks to report in the scan.  Too many here will crash the ESP.
#define SETTINGS_FILE "/espman/settings.json" /**< @brief Location of settings file on SPIFFS */
#define ESPMANVERSION "3.0-async"             /**< @brief Version of espmanager */
#define SETTINGS_FILE_VERSION 2               /**< @brief Settings file.  Version number increments are not backwards compatible. @todo implement version checking in settings file  */

#if defined(DEBUGESPMANAGER)
static File _DebugFile;
#define DEBUGESPMANAGERF(_1, ...)                                                                                                  \
  {                                                                                                                             \
    DEBUGESPMANAGER.printf_P(PSTR("[%-10u][%5.5s][%15.15s:L%-4u] " _1), millis(), "ESPMA", __func__, __LINE__, ##__VA_ARGS__); \
  } 
#define DEBUGESPMANAGERFRAW(_1, ...)                      \
  {                                                     \
    DEBUGESPMANAGER.printf_P(PSTR(_1), ##__VA_ARGS__); \
  }

#pragma message("DEBUG enabled for ESPManager.")
#else
#define DEBUGESPMANAGERF(...) \
  {                        \
  } // leaves 40,740 K, so flash debug uses 1.1K of ram...
#define DEBUGESPMANAGERFRAW(_1, ...) \
  {                                \
  }
#endif

namespace {
  using namespace ESPMAN;
}; 


  struct  bootState_t
  {
    bootState_t() 
    : RTCvalid(false)
    , RebootOnly(false)
    , WizardEnabled(false)
    , ValidConfig(false)
    , STAvalid(false)
    , APvalid(false)
    , EmergencyAP(false)
  {}
    bool RTCvalid      : 1; 
    bool RebootOnly    : 1; 
    bool WizardEnabled : 1; 
    bool ValidConfig   : 1; 
    bool STAvalid      : 1; 
    bool APvalid       : 1; 
    bool EmergencyAP   : 1; 
  }; 

/**
 * @brief Manager for ESP8266.
 *
 * Includes support for WiFi management, WiFi upgrades, device discovery and much more. 
 */
class ESPmanager
{
public:
  ESPmanager(AsyncWebServer &HTTP, FS &fs);
  ~ESPmanager();
  ESPMAN_ERR_t begin();
  ESPMAN_ERR_t begin(const String &ssid, const String &pass);
  void handle();
  static String formatBytes(size_t bytes);
  static bool StringtoMAC(uint8_t *mac, const String &input);
  static void urldecode(char *dst, const char *src); // need to check it works to decode the %03... for :
  static String file_md5(File &f);
  template <class T = JsonObject>
  static void sendJsontoHTTP(const T &root, AsyncWebServerRequest *request);
  String getHostname();
  //const String getError(ESPMAN_ERR_t err);
  //const String getError(int err) { return getError((ESPMAN_ERR_t)err); } /**< Returns error as String. @return ESPMAN::myString &  */
  inline uint32_t trueSketchSize();
  inline String getSketchMD5();
  AsyncEventSource &getEvent();
  static void FSDirIterator(FS &fs, const char *dirName, std::function<void(File &f)> Cb);
  bool setAuthentication(const String & username, const String & password); 

  void event_send(const String & topic, const String & msg);
  ESPMAN_ERR_t upgrade(String path = String(), bool runasync = true);
  ESPMAN_ERR_t upgrade(bool runasync) { return upgrade(String(), runasync); }
  void factoryReset();
  ESPMAN_ERR_t save();
  //bool portal() { return _dns; } /**< Returns if the portal is active or not. @return bool  */
  // ESPMAN_ERR_t enablePortal();
  // void disablePortal();

private:
  void _HandleDataRequest(AsyncWebServerRequest *request);
  void _HandleQuickSTAsetup(AsyncWebServerRequest * request); 
  void _handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

#ifdef ESPMANAGER_UPDATER
  ESPMAN_ERR_t _upgrade(const char *path);
  ESPMAN_ERR_t _DownloadToFS(const char *url, const char *path, const char *md5 = nullptr, bool overwrite = false);
  ESPMAN_ERR_t _parseUpdateJson(DynamicJsonDocument &json, const char *path);
  void _HandleSketchUpdate(AsyncWebServerRequest *request);
#else
  ESPMAN_ERR_t _upgrade(const char *path) {}; 
#endif

  ESPMAN_ERR_t _getAllSettings();                //  gets settings to settings ptr, uses new if it doesn't exist.  overwrite current data
  ESPMAN_ERR_t _getAllSettings(settings_t &set); //only populates the set... used to retrieve certain vailue...
  ESPMAN_ERR_t _saveAllSettings(settings_t &set);
  ESPMAN_ERR_t _initialiseAP(settings_t::AP_t &settings);
  ESPMAN_ERR_t _initialiseAP(); //  reads the settings from FS....  then calls _initialiseAP(ESPMAN::AP_settings_t settings);
  ESPMAN_ERR_t _initialiseSTA();                     //  reads the settings from FS....  then calls _initialiseAP(ESPMAN::STA_settings_t settings);
  ESPMAN_ERR_t _initialiseSTA(settings_t::STA_t &settings);
  ESPMAN_ERR_t _emergencyMode(bool shutdown = false, int channel = -1);
  
  void _sendTextResponse(AsyncWebServerRequest *request, uint16_t code, const String & text);
  void _populateFoundDevices(JsonObject &root);
  bool _convertMD5StringtoArray(const String & in, uint8_t * out) const; 
  AsyncWebServer &_HTTP;
  FS &_fs;
  AsyncEventSource _events;

  int8_t _wifinetworksfound{0};
  // ap_boot_mode_t _ap_boot_mode{NO_STA_BOOT};
  // no_sta_mode_t _no_sta_mode{NO_STA_NOTHING};
 // bool _APenabledAtBoot{false};
  settings_t *_settings{nullptr};
  const byte DNS_PORT = 53;
  int8_t WiFiresult = -1; //  used by the asynchandler to return the state of the wifi. can maybe be a static
  
  Task _tasker;
  //Task *_dnsTask{nullptr};

  ESPdeviceFinder *_devicefinder{nullptr};
  uint32_t _deviceFinderTimer{0};
  
  /*
      New Variables added V3
  */
  staticHandler _staticHandlerInstance; //  serves PROGMEM Javascript Files. 
  ESP8266RTCMemory<ESPMAN_rtc_data> _rtc;
  WiFiEventHandler _stationConnectedHandler;
  WiFiEventHandler _stationDisconnectedHandler;
  WiFiEventHandler _stationGotIPHandler;

  void _onWiFiConnected(const WiFiEventStationModeConnected &);
  void _onWiFiDisconnected(const WiFiEventStationModeDisconnected &);
  void _onWiFgotIP(const WiFiEventStationModeGotIP &data);

  void _initOTA(); 
  void _OTAonStart(); 
  void _OTAonProgress(unsigned int progress, unsigned int total); 
  void _OTAonEnd();
  void _OTAonError(ota_error_t error); 
  void _initHTTP(); 
  void _initAutoDiscover(); 

  // Tasks 

  void _deleteSettingsTask(Task & task); 


  bootState_t _bootState; 
  bool _RebootOnly(); 
  bool _hasWifiEverBeenConnected{false}; 
  uint32_t _disconnectCounter{0}; 
  bool _emergencyModeActivated{false};  
  AsyncWebHandler* _WebHandler{nullptr};


#ifdef DEBUGESPMANAGER
  void _dumpSettings();
  void _dumpSTA(settings_t::STA_t &set);
  void _dumpAP(settings_t::AP_t &set);
  void _dumpGEN(settings_t::GEN_t &set);
#endif
};
