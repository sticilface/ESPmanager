/** @file
    @brief ESPmanager implementations.
*/

#include "ESPmanager.h"

#include <WiFiUdp.h>
#include <flash_utils.h>

#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <MD5Builder.h>
#include <AsyncJson.h>
#include <MD5Builder.h>
#include <stdio.h>
#include <Hash.h>
#include <list>
#include "Tasker/src/Tasker.h"
#include "FlashWriter.h"
#include "StreamString.h"
// #include "staticHandler/src/staticHandler.h"
// #include "staticHandler/src/jqueryStatic.h"
#include "pgmspace.h"
#include <cstdlib>
#include "QuickSetup.h"
#include "HTTPfile.h"

extern "C"
{
#include "user_interface.h"
}

static const char _compile_date_time[] = __DATE__ " " __TIME__;
static const uint16_t _ESPdeviceFinderPort = 8888;
static const uint32_t _ESPdeviceTimeout = 1200000; // 300000;  //  when is the devicefinder deleted.

namespace
{
    using namespace ESPMAN;
};

#ifndef ESPMANAGER_GIT_TAG
#define ESPMANAGER_GIT_TAG "NOT DEFINED"
#endif

namespace ESPMAN
{
    uint32_t allocateJSON()
    {
        uint32_t value = ESP.getMaxFreeBlockSize() - 512;
        if (value > MAX_BUFFER_SIZE)
        {
            return MAX_BUFFER_SIZE;
        }
        else
        {
            return value;
        }
    }
}; // namespace ESPMAN
/**
 *
 * @param [HTTP] pass an instance of AsyncWebServer. Optional.
 * @param [fs] pass an instance of FS file system.  Defaults to FS, as per arduino. Optional.
 *
 */
ESPmanager::ESPmanager(
    AsyncWebServer &HTTP, FS &fs)
    : _HTTP(HTTP), _fs(fs), _events("/espman/events"){
//  These need to be added here to force linking...
#ifdef USESTATICPROGMEMJQUERY
// _staticHandlerInstance.add(JQUERYdata::JQdata);
// _staticHandlerInstance.add(JQUERYdata::JQMdata);
// _staticHandlerInstance.add(JQUERYdata::JQMCSSdata);
// _staticHandlerInstance.enableRedirect() = true;
#else
//_staticHandlerInstance.enableRedirect() = true;
#endif

                            }

      ESPmanager::~ESPmanager()
{

    if (_settings)
    {
        delete _settings;
    }

    if (_devicefinder)
    {
        delete _devicefinder;
    }
}

/**
 * To be called during setup() and only called once.
 * @return ESPMAN::ESPMAN_ERR_t.
 */
ESPMAN_ERR_t ESPmanager::begin()
{

    using namespace std::placeholders;
    //using namespace ESPMAN;
    /*
    Debug Output
*/
    DEBUGESPMANAGERF("Settings Manager V" ESPMANVERSION "\n");
    DEBUGESPMANAGERF("True Sketch Size: %u\n", trueSketchSize());
    DEBUGESPMANAGERF("Sketch MD5: %s\n", getSketchMD5().c_str());
    DEBUGESPMANAGERF("Device MAC: %s\n", WiFi.macAddress().c_str());
    /*
    Init RTC and boot state variables
*/
    { //  Boot state Variables..
        _bootState.RTCvalid = _rtc.begin(true);
        if (_bootState.RTCvalid)
        {
            _bootState.RebootOnly = _RebootOnly();
        }

        //  save current set up regardless to RTC.
        DEBUGESPMANAGERF("Saved current MD5 to RTC\n");
        _convertMD5StringtoArray(ESP.getSketchMD5(), _rtc[0].sketchMD5);
        _rtc.save();

        wifi_set_sleep_type(NONE_SLEEP_T); // workaround no modem sleep.

#ifdef DEBUGESPMANAGER
        DEBUGESPMANAGER.println(F("FS FILES:"));
        {
            FSDirIterator(_fs, "/", [](File &f) {
                DEBUGESPMANAGER.printf_P(PSTR(" File: %-35s [%8uB]\n"), f.fullName(), f.size());
            });
        }
        File f = _fs.open(SETTINGS_FILE, "r");
        DEBUGESPMANAGER.printf_P(PSTR("ESP MANAGER Settings [%u]B:\n"), f.size());
        if (f)
        {
            while (f.available())
            {
                DEBUGESPMANAGER.write(f.read());
            }
            DEBUGESPMANAGER.println();
            f.close();
        }
        else
        {
            DEBUGESPMANAGER.println("No Config File opened");
        }
#endif

        if (_getAllSettings() == SUCCESS)
        {
            _bootState.ValidConfig = true;
            _bootState.STAvalid = _settings->STA.enabled;
            _bootState.APvalid = _settings->AP.enabled;
        }
        else
        {
            _bootState.ValidConfig = false;
            if (_settings)
            {
                _settings->changed = true; //  give save button at first boot if no settings file
            }
        }

        DEBUGESPMANAGERF("Boot State RTC Valid = %u, RebootOnly =%u, validConfig=%u, STAValid=%u, APValid=%u\n",
                         _bootState.RTCvalid, _bootState.RebootOnly, _bootState.ValidConfig, _bootState.STAvalid, _bootState.APvalid);
    }

    /*
        WiFi Callbacks regardless of state... 
    */

    if (!_stationConnectedHandler)
    {
        _stationConnectedHandler = WiFi.onStationModeConnected(std::bind(&ESPmanager::_onWiFiConnected, this, _1));
    }

    if (!_stationDisconnectedHandler)
    {
        _stationDisconnectedHandler = WiFi.onStationModeDisconnected(std::bind(&ESPmanager::_onWiFiDisconnected, this, _1));
    }

    if (!_stationGotIPHandler)
    {
        _stationGotIPHandler = WiFi.onStationModeGotIP(std::bind(&ESPmanager::_onWiFgotIP, this, _1));
    }

    /*
        Start the WiFi Services
        Checks for actual connection are done later to give a chance... 
    */

    auto STAinitResult = _initialiseSTA();
    auto APinitResult = _initialiseAP();

    if (!_bootState.STAvalid && !_bootState.APvalid)
    {
        DEBUGESPMANAGERF("CONNECTIVITY PROBLEM:  No valid AP or STA configuration\n");
        _emergencyMode();
    }
    else if ((_bootState.STAvalid && STAinitResult != SUCCESS) && (_bootState.APvalid && APinitResult != SUCCESS))
    {
        DEBUGESPMANAGERF("CONNECTIVITY PROBLEM:  Unable to init valid STA or AP config\n");
        _emergencyMode();
    };

    /*
        Start Other services OTS, mDNS, 
    */

    if (_settings->GEN.OTAupload)
    {
        _initOTA();
    }
    else
    {
        DEBUGESPMANAGERF("OTA DISABLED\n");
    }

    if (_settings->GEN.mDNSenabled)
    {
        MDNS.addService("http", "tcp", 80);
    }

    _initHTTP();

    if (_settings->GEN.updateFreq)
    {
        _tasker.add([this](Task &t) {
                   DEBUGESPMANAGERF("Performing update check\n");
                   _getAllSettings();
                   if (_settings)
                   {
                       _upgrade(_settings->GEN.updateURL.c_str());
                   }
               })
            .setRepeat(true)
            .setTimeout(_settings->GEN.updateFreq * 60000);
    }

    _tasker.add(std::bind(&ESPmanager::_deleteSettingsTask, this, _1))
        .setTimeout(10000)
        .setRepeat(true);

    //_tasker.add(std::bind(&ESPmanager::_APlogic, this, _1)).setRepeat(true).setTimeout(500);

    _initAutoDiscover();

    return ESPMAN_ERR_t::SUCCESS;
}

/**
 * Allows you to override and settings file, mainly for testing purposes as you can't use settings stored
 * in the config.json file.
 * @param [ssid] desired default ssid to connect to.  Can be `const char *`, `String`, `myString` or `F()`.
 * @param [pass] desired default password to connect to ssid.  Can be `const char *`, `String`, `myString` or `F()`.
 * @return ESPMAN::ESPMAN_ERR_t
 */
ESPMAN_ERR_t ESPmanager::begin(const String &ssid, const String &pass)
{
    DEBUGESPMANAGERF("ssid = %s, pass = %s\n", ssid.c_str(), pass.c_str());

    _getAllSettings();

    if (_settings)
    {
        _settings->STA.ssid = ssid;
        _settings->STA.pass = pass;
        _settings->STA.enabled = true;
        _settings->configured = true;
        _settings->changed = true;
    }
    else
    {
        return SETTINGS_NOT_IN_MEMORY;
    }

    return begin();
}

/**
 *  Loop task, must be included in loop();
 */
void ESPmanager::handle()
{
    _tasker.run();
}

//format bytes thanks to @me-no-dev

/**
 * Thanks to me-no-dev.
 * @param [bytes] Number of Bytes to convert to String.
 * @return String with formated bytes.
 */
String ESPmanager::formatBytes(size_t bytes)
{
    if (bytes < 1024)
    {
        return String(bytes) + "B";
    }
    else if (bytes < (1024 * 1024))
    {
        return String(bytes / 1024.0) + "KB";
    }
    else if (bytes < (1024 * 1024 * 1024))
    {
        return String(bytes / 1024.0 / 1024.0) + "MB";
    }
    else
    {
        return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
    }
}

/**
 * Converts a String to a byte array.
 * @param [mac] `*uint8_t` to byte array to output to.
 * @param [input]  input String to convert
 * @return
 */
bool ESPmanager::StringtoMAC(uint8_t *mac, const String &input)
{

    char tempbuffer[input.length() + 1];
    urldecode(tempbuffer, input.c_str());
    String decodedMAC = String(tempbuffer);
    String buf;
    uint8_t pos = 0;
    char tempbuf[5];
    bool remaining = true;

    do
    {
        buf = decodedMAC.substring(0, decodedMAC.indexOf(':'));
        remaining = (decodedMAC.indexOf(':') != -1) ? true : false;
        decodedMAC = decodedMAC.substring(decodedMAC.indexOf(':') + 1, decodedMAC.length());
        buf.toCharArray(tempbuf, buf.length() + 1);
        mac[pos] = (uint8_t)strtol(tempbuf, NULL, 16);
        //Serial.printf("position %u = %s ===>%u \n", pos, tempbuf, mac[pos]);
        pos++;
    } while (remaining);

    if (pos == 6)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/**
 *  URI Decoding function
 *  Does not check if dst buffer is big enough to receive string so
 *  use same size as src is a recommendation.
 * @param [dst] destination buffer
 * @param [src] source buffer
 */
void ESPmanager::urldecode(char *dst, const char *src)
{
    char a, b, c;
    if (dst == NULL)
    {
        return;
    }
    while (*src)
    {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b)))
        {
            if (a >= 'a')
            {
                a -= 'a' - 'A';
            }
            if (a >= 'A')
            {
                a -= ('A' - 10);
            }
            else
            {
                a -= '0';
            }
            if (b >= 'a')
            {
                b -= 'a' - 'A';
            }
            if (b >= 'A')
            {
                b -= ('A' - 10);
            }
            else
            {
                b -= '0';
            }
            *dst++ = 16 * a + b;
            src += 3;
        }
        else
        {
            c = *src++;
            if (c == '+')
            {
                c = ' ';
            }
            *dst++ = c;
        }
    }
    *dst++ = '\0';
}

/**
 * Returns an md5 string of the input file.
 * @param [f] Input file.
 * @return String
 */
String ESPmanager::file_md5(File &f)
{
    // Md5 check

    if (!f)
    {
        return String();
    }

    if (f.seek(0, SeekSet))
    {

        MD5Builder md5;
        md5.begin();
        md5.addStream(f, f.size());
        md5.calculate();
        return md5.toString();
    }
    else
    {
        DEBUGESPMANAGERF("Seek failed on file\n");
    }
    return String();
}

/**

 * Templated functon to allow sending of a json to an AsyncWebServerRequest.
 * It adds the CORS header, and no-store to prevent caching.
 * Only works for json lengths under 4k.
 * @todo Add return bool so it does not fail silently.
 * @param [root] Either JsonObject or JsonObject
 * @param [request] AsyncWebServerRequest* to send the json to.
 */
template <class T>
void ESPmanager::sendJsontoHTTP(const T &root, AsyncWebServerRequest *request)
{
    int len = measureJson(root);
    if (len < 4000)
    {

        AsyncResponseStream *response = request->beginResponseStream("text/json");

        if (response)
        {
            //response->addHeader(FPSTR(ESPMAN::fstring_CORS), "*");
            response->addHeader(FPSTR(ESPMAN::fstring_CACHE_CONTROL), "no-store");
            //root.printTo(*response);
            serializeJson(root, *response);
            request->send(response);
        }
        else
        {
            //Serial.println("ERROR: No Stream Response");
        }
    }
    else
    {

        //DEBUGESPMANAGERF("JSON to long\n");

        // AsyncJsonResponse * response = new AsyncJsonResponse();
        // response->addHeader(ESPMAN::string_CORS, "*");
        // response->addHeader(ESPMAN::string_CACHE_CONTROL, "no-store");
        // JsonObject& root = response->getRoot();
        // root["heap"] = ESP.getFreeHeap();
        // root["ssid"] = WiFi.SSID();
        // response->setLength();
        // request->send(response);
    }
}

/**
 * Returns the current hostname set in config.json.
 * Opens the settings file if settings are not in memory.
 * @return String
 */
String ESPmanager::getHostname()
{

    settings_t set;

    if (_settings)
    {
        set = *_settings;
    }

    int ERROR = _getAllSettings(set);

    DEBUGESPMANAGERF("error = %i\n", ERROR);

    if (!ERROR && set.GEN.host)
    {
        return set.GEN.host;
    }
    else
    {
        char tmp[33] = {'\0'};
        snprintf_P(tmp, 32, PSTR("esp8266-%06x"), ESP.getChipId());
        return String(tmp);
    }
}

/**
 * Allows update of ESP8266 binary and SPIFFS files.
 * Downloads the file at path.
 * example config.json
 * @code{json}
 * {
 *  "files":[
 *    {
 *      "saveto":"sketch", // used for binary
 *      "location":"/data/firmware.bin",
 *      "md5":"bbec8986eea6a5836c7446d08c719923"
 *    },
 *    {
 *      "saveto":"/index.htm.gz",
 *      "location":"/data/index.htm.gz", //  can be relative url, or absolute http://  location
 *      "md5":"6816935f51673e61f76afd788e457400"  //md5 of the file
 *    },
 *    {
 *      "saveto":"/espman/ajax-loader.gif",
 *      "location":"/data/espman/ajax-loader.gif",
 *      "md5":"8fd7e719b06cd3f701c791adb62bd7a6"
 *    }
 *  ],
 * }
 * @endcode
 * Optional parameters for config json:
 * "overwrite": bool  -> overwrite existing files
 * "formatSPIFFS": bool -> format spiffs before download, keeps the ESPManager settings file.  Good for a defrag....
 * "clearWiFi": bool -> reset wifin settings
 *
 * @param [path] A url to a json file containing the upgrade instructions.
 * @param [runasync] `bool` required if upgrade is being called from an interrupt.
 * @return ESPMAN::ESPMAN_ERR_t
 */
ESPMAN_ERR_t ESPmanager::upgrade(String path, bool runasync)
{

    if (path.length())
    {
        DEBUGESPMANAGERF("Upgrade Called: path = %s\n", path.c_str());
    }

    _getAllSettings();

    String newpath;

    if (!_settings)
    {
        return CONFIG_FILE_ERROR;
    }

    if (path.length() == 0)
    {

        if (_settings->GEN.updateURL)
        {
            newpath = _settings->GEN.updateURL;
        }
        else
        {
            char buf[10]{0};
            snprintf(buf, 20, "[%i]", NO_UPDATE_URL);
            event_send(FPSTR(fstring_UPGRADE), String(buf));
            return NO_UPDATE_URL;
        }
    }
    else
    {
        newpath = path.c_str();
    }

    if (runasync)
    {
        _tasker.add([newpath, this](Task &t) {
            this->_upgrade(newpath.c_str());
        });
    }
    else
    {
        return _upgrade(newpath.c_str());
    }

    return SUCCESS;
}

#ifdef ESPMANAGER_UPDATER
ESPMAN_ERR_t ESPmanager::_upgrade(const char *path)
{

    _getAllSettings();

    if (!_settings)
    {
        return CONFIG_FILE_ERROR;
    }

    if (!path || strlen(path) == 0)
    {

        if (_settings->GEN.updateURL)
        {
            path = _settings->GEN.updateURL.c_str();
        }
        else
        {

            //**             event_send( FPSTR(fstring_UPGRADE), myStringf_P( PSTR("[%i]"), NO_UPDATE_URL  ));
            return NO_UPDATE_URL;
        }
    }
    else
    {
        DEBUGESPMANAGERF("Path sent in: %s\n", path);
    }

    int __attribute__((unused)) files_expected = 0;
    int file_count = 1;
    int firmwareIndex = -1;
    bool overwriteFiles = false;
    //DynamicJsonDocument json(ESP.getMaxFreeBlockSize() - 512);
    DynamicJsonDocument json(allocateJSON());

    event_send( FPSTR(fstring_UPGRADE) , F("begin")) ;
    DEBUGESPMANAGERF("Checking for Updates: %s\n", path);
    DEBUGESPMANAGERF("Free Heap: %u\n", ESP.getFreeHeap());
    String Spath = String(path);
    String rooturi = Spath.substring(0, Spath.lastIndexOf('/'));
    event_send( FPSTR(fstring_CONSOLE) , String(path) ) ;
    DEBUGESPMANAGERF("rooturi=%s\n", rooturi.c_str());

    //  save new update path for future update checks...  (if done via url only for example)
    if (_settings->GEN.updateURL)
    {
        if (_settings->GEN.updateURL != path)
        {
            _settings->GEN.updateURL = path;
            //save_flag = true;
        }
    }
    else
    {
        _settings->GEN.updateURL = path;
        //save_flag = true;
    }

    int ret = _parseUpdateJson(json, path);

    if (ret)
    {
        DEBUGESPMANAGERF("MANIFEST ERROR part 1 [%u]\n", MANIFST_FILE_ERROR);
        DEBUGESPMANAGERF("MANIFEST ERROR part 2 [%u]\n", ret);
        String err = "ERROR" + String(MANIFST_FILE_ERROR); 
        event_send( FPSTR(fstring_UPGRADE),  err ) ; 
        DEBUGESPMANAGERF("MANIFEST ERROR [%i]\n", ret);
        return MANIFST_FILE_ERROR;
    }

    json.shrinkToFit();

    DEBUGESPMANAGERF("_parseUpdateJson success\n");

    if (!json.is<JsonObject>())
    {
        //**         event_send( FPSTR(fstring_UPGRADE), myStringf_P( fstring_ERROR_toString, getError(JSON_OBJECT_ERROR).c_str() ) );
        DEBUGESPMANAGERF("JSON ERROR [%i]\n", JSON_OBJECT_ERROR);
        return JSON_PARSE_ERROR;
    }

    JsonObject root = json.as<JsonObject>();

    /**
     *      Global settings for upgrade
     *
     */

    if (root.containsKey(F("formatSPIFFS")))
    {
        if (root[F("formatSPIFFS")] == true)
        {
            DEBUGESPMANAGERF("Formatting FS");
            _getAllSettings();
            _fs.format();
            if (_settings)
            {
                _saveAllSettings(*_settings);
            }

            DEBUGESPMANAGERF("done\n ");
        }
    }

    if (root.containsKey(F("clearWiFi")))
    {
        if (root[F("clearWiFi")] == true)
        {
            DEBUGESPMANAGERF("Erasing WiFi Config ....");
            DEBUGESPMANAGERF("done\n");
        }
    }

    if (root.containsKey(F("overwrite")))
    {
        overwriteFiles = root["overwrite"].as<bool>();
        DEBUGESPMANAGERF("overwrite files set to %s\n", (overwriteFiles) ? "true" : "false");
    }

    if (root.containsKey(F("files")))
    {

        JsonArray array = root[F("files")];
        files_expected = array.size();

        for (JsonArray::iterator it = array.begin(); it != array.end(); ++it)
        {
            JsonObject item = *it;
            String remote_path = String();

            //  if the is url is set to true then don't prepend the rootUri...
            if (remote_path.startsWith("http://"))
            {
                remote_path = String(item["location"].as<const char *>());
            }
            else
            {
                remote_path = rooturi + String(item["location"].as<const char *>());
            }

            const char *md5 = item[F("md5")];
            String filename = item[F("saveto")];

            if (remote_path.endsWith("bin") && filename == "sketch")
            {
                firmwareIndex = file_count - 1; //  true index vs counted = -1
                DEBUGESPMANAGERF("[%u/%u] BIN Updated pending, index %i\n", file_count, files_expected, firmwareIndex);
                file_count++;
                continue;
            }

#ifdef DEBUGESPMANAGER
            DEBUGESPMANAGER.print("\n\n");
#endif

            DEBUGESPMANAGERF("[%u/%u] Downloading (%s)..\n", file_count, files_expected, filename.c_str());

            //           MDNS.stop();

            int ret = _DownloadToFS(remote_path.c_str(), filename.c_str(), md5, overwriteFiles);

            if (ret == 0 || ret == FILE_NOT_CHANGED)
            {
                //**                event_send( FPSTR(fstring_CONSOLE), myStringf_P( PSTR("[%u/%u] (%s) : %s"), file_count, files_expected, filename.c_str(), (!ret) ? "Downloaded" : "Not changed" ) );
            }
            else
            {
                //**                 event_send( FPSTR(fstring_CONSOLE), myStringf_P( PSTR("[%u/%u] (%s) : ERROR [%i]") , file_count, files_expected, filename.c_str(), ret ) );
            }

            event_send( FPSTR(fstring_UPGRADE), String (  (uint8_t ) (( (float)file_count / (float)files_expected) * 100.0f)  ) );

#if defined(DEBUGESPMANAGER)
            if (ret == 0)
            {
                DEBUGESPMANAGERF("SUCCESS \n");
            }
            else if (ret == FILE_NOT_CHANGED)
            {
                DEBUGESPMANAGERF("FILE NOT CHANGED \n");
            }
            else
            {
                DEBUGESPMANAGERF("FAILED [%i]\n", ret);
            }
#endif

            file_count++;
        }
    }
    else
    {
        //**         event_send( FPSTR(fstring_UPGRADE), myStringf_P( fstring_ERROR_toString, getError(MANIFST_FILE_ERROR).c_str() ) );
        DEBUGESPMANAGERF("ERROR [%i]\n", MANIFST_FILE_ERROR);
    }

    //  this removes any duplicate files if a compressed file exists
    //_removePreGzFiles();

    if (firmwareIndex != -1)
    {

        //  for (JsonArray::iterator it = array.begin(); it != array.end(); ++it) {
        JsonArray array = root["files"];
        JsonObject item = array.getElement(firmwareIndex);

        String remote_path = rooturi + String(item["location"].as<const char *>());
        String filename = item[F("saveto")];
        String commit = root[F("commit")];

        if (remote_path.endsWith("bin") && filename == "sketch")
        {
            if (item["md5"].as<String>() != getSketchMD5())
            {
                DEBUGESPMANAGERF("Current Sketch MD5 = %s\n", getSketchMD5().c_str());
                DEBUGESPMANAGERF("New     Sketch MD5 = %s\n", item["md5"].as<String>().c_str());

                DEBUGESPMANAGERF("START SKETCH DOWNLOAD (%s)\n", remote_path.c_str());
                event_send( FPSTR(fstring_UPGRADE), F("firmware"));
                delay(10);
                _events.send("Upgrading sketch", nullptr, 0, 5000);
                delay(10);
                ESPhttpUpdate.rebootOnUpdate(false);

                //              MDNS.stop();
                WiFiClient client;
                t_httpUpdate_return ret = ESPhttpUpdate.update(client, remote_path);

                switch (ret)
                {

                case HTTP_UPDATE_FAILED:
                    DEBUGESPMANAGERF("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                    delay(100);
                    //**                   event_send( FPSTR(fstring_UPGRADE), myStringf_P( PSTR("ERROR [%s]"), ESPhttpUpdate.getLastErrorString().c_str()  ));
                    delay(100);
                    break;

                case HTTP_UPDATE_NO_UPDATES:
                    DEBUGESPMANAGERF("HTTP_UPDATE_NO_UPDATES");
                    delay(100);
                    event_send( FPSTR(fstring_UPGRADE), F("ERROR no update") );
                    delay(100);
                    break;

                case HTTP_UPDATE_OK:
                    DEBUGESPMANAGERF("HTTP_UPDATE_OK");
                    event_send( FPSTR(fstring_UPGRADE), F("firmware-end") );
                    delay(100);
                    _events.close();
                    delay(1000);
                    ESP.restart();
                    break;
                }
            }
            else
            {
                event_send( FPSTR(fstring_CONSOLE), F("No Change to firmware") );
                DEBUGESPMANAGERF("BINARY HAS SAME MD5 as current (%s)\n", item["md5"].as<const char *>());
            }
        }
        else
        {
            //  json object does not contain valid binary.
        }
    }

    event_send(FPSTR(fstring_UPGRADE), F("end"));

    //   MDNS.restart();

    return ESPMAN_ERR_t::SUCCESS;
}

#endif
/**
 * Get the size of the existing sketch in bytes.
 * @return uint32_t
 */
uint32_t ESPmanager::trueSketchSize()
{
    return ESP.getSketchSize();
}
/**
 * Get the current sketch md5.  This is used to compare updates
 * @return
 */
String ESPmanager::getSketchMD5()
{
    return ESP.getSketchMD5();
}
/**
 * Returns the events instance, allowing sketches to access browsers that have events opened.
 * @return AsyncEventSource &
 */
AsyncEventSource &ESPmanager::getEvent()
{
    return _events;
}

void ESPmanager::event_send(const String &topic, const String &msg)
{
    DEBUGESPMANAGERF("EVENT: top = %s, msg = %s\n", topic.c_str(), msg.c_str());
    _events.send(msg.c_str(), topic.c_str(), millis(), 5000);
}

/**
 * Saves settings to FS.
 * @return ESPMAN::ESPMAN_ERR_t
 */
ESPMAN_ERR_t ESPmanager::save()
{

    _getAllSettings();

    if (_settings)
    {
        return _saveAllSettings(*_settings);
    }
    else
    {
        return SETTINGS_NOT_IN_MEMORY;
    }
}

#ifdef ESPMANAGER_UPDATER

ESPMAN_ERR_t ESPmanager::_DownloadToFS(const char *url, const char *filename_c, const char *md5_true, bool overwrite)
{

    String filename = filename_c;
    HTTPClient http;
    FSInfo _FSinfo;
    int freeBytes = 0;
    bool success = false;
    ESPMAN_ERR_t ERROR = SUCCESS;

    DEBUGESPMANAGERF("URL = %s, filename = %s, md5 = %s, overwrite = %s\n", url, filename_c, md5_true, (overwrite) ? "true" : "false");

    if (!overwrite && _fs.exists(filename))
    {

        DEBUGESPMANAGERF("Checking for existing file.\n");
        File Fcheck = _fs.open(filename, "r");
        String crc = file_md5(Fcheck);

        if (crc == String(md5_true))
        {
            Fcheck.close();
            return FILE_NOT_CHANGED;
        }

        Fcheck.close();
    }

    if (!_fs.info(_FSinfo))
    {
        return FS_INFO_FAIL;
    }

    freeBytes = _FSinfo.totalBytes - _FSinfo.usedBytes;

    DEBUGESPMANAGERF("totalBytes = %u, usedBytes = %u, freebytes = %u\n", _FSinfo.totalBytes, _FSinfo.usedBytes, freeBytes);

    //  filename is
    if (filename.length() > _FSinfo.maxPathLength)
    {
        return FS_FILENAME_TOO_LONG;
    }

    File f = _fs.open("/tempfile", "w+"); //  w+ is to allow read operations on file.... otherwise crc gets 255!!!!!

    if (!f)
    {

        return FS_FILE_OPEN_ERROR;
    }

    WiFiClient client;
    http.begin(client, url);

    int httpCode = http.GET();

    if (httpCode == 200)
    {

        int len = http.getSize();

        if (len > 0 && len < freeBytes)
        {

            WiFiUDP::stopAll();
            WiFiClient::stopAllExcept(http.getStreamPtr());
            //DEBUGESPMANAGERF("got to here\n"); 
            yield(); 

            wifi_set_sleep_type(NONE_SLEEP_T);

            FlashWriter writer;
            int byteswritten = 0;

            if (writer.begin(len))
            {
                //DEBUGESPMANAGERF("got to here 2\n"); 
                //uint32_t start_time = millis();
                byteswritten = http.writeToStream(&writer); //  this writes to the 1Mb Flash partition for the OTA upgrade.  zero latency...
                if (byteswritten > 0 && byteswritten == len)
                {
                    //uint32_t start_time = millis();
                    byteswritten = writer.writeToStream(&f); //  contains a yield to allow networking.  Can take minutes to complete.
                }
                else
                {
                    DEBUGESPMANAGERF("HTTP to Flash error, byteswritten = %i\n", byteswritten);
                }
            }
            else
            {

                DEBUGESPMANAGERF("Try Old method and write direct to file\n");

                byteswritten = http.writeToStream(&f);
            }

            http.end();

            if (f.size() == (uint)len && byteswritten == len)
            { // byteswritten > 0 means no error writing.   ,len = -1 means server did not provide length...

                if (md5_true)
                {
                    String crc = file_md5(f);

                    if (crc == String(md5_true))
                    {
                        success = true;
                    }
                    else
                    {
                        ERROR = CRC_ERROR;
                    }
                }
                else
                {
                    DEBUGESPMANAGERF("\n  [ERROR] CRC not provided \n");
                    success = true; // set to true if no CRC provided...
                }
            }
            else
            {
                DEBUGESPMANAGERF("\n  [ERROR] Failed Download: length = %i, byteswritten = %i, f.size() = %i\n", len, byteswritten, f.size());
                ERROR = INCOMPLETE_DOWNLOAD;
            }
        }
        else
        {
            DEBUGESPMANAGERF("\n  [ERROR] Not enough free space \n");
            ERROR = FILE_TOO_LARGE;
        }
    }
    else
    {
        DEBUGESPMANAGERF("\n  [ERROR] HTTP code = %i \n", ERROR);
        ERROR = static_cast<ESPMAN_ERR_t>(httpCode);
    }

    f.close();

    if (success)
    {

        if (_fs.exists(filename))
        {
            _fs.remove(filename);
        }

        if (filename.endsWith(".gz"))
        {
            String withOutgz = filename.substring(0, filename.length() - 3);
            DEBUGESPMANAGERF("NEW File ends in .gz: without = %s...\n", withOutgz.c_str());

            if (_fs.remove(withOutgz))
            {
                DEBUGESPMANAGERF("%s DELETED...\n", withOutgz.c_str());
            }
        }

        if (_fs.exists(filename + ".gz"))
        {
            if (_fs.remove(filename + ".gz"))
            {
                DEBUGESPMANAGERF("%s.gz DELETED...\n", filename.c_str());
            }
        }

        if (!_fs.rename("/tempfile", filename))
        {
            ERROR = FILE_RENAME_FAILED;
            _fs.remove("/tempfile");
        }
        else
        {
            if (!_fs.exists(filename))
            {
                ERROR = FILE_RENAME_FAILED;
            }
        }
    }
    else
    {
        _fs.remove("/tempfile");
    }

    return ERROR;
}

/*
 *      Takes POST request with url parameter for the json
 *
 *
 */

ESPMAN_ERR_t ESPmanager::_parseUpdateJson(DynamicJsonDocument &json, const char *path)
{

    DEBUGESPMANAGERF("path = %s\n", path);
    WiFiClient client;
    HTTPClient http;
    http.begin(client, path); //HTTP
    int httpCode = http.GET();

    if (httpCode != 200)
    {
        DEBUGESPMANAGERF("HTTP code: %i\n", httpCode);
        return static_cast<ESPMAN_ERR_t>(httpCode);
    }

    DEBUGESPMANAGERF("Connected downloading json\n");

    size_t len = http.getSize();
    //const size_t length = len;

    if (len > MAX_BUFFER_SIZE)
    {
        DEBUGESPMANAGERF("Receive update length too big.  Increase buffer");
        return JSON_TOO_LARGE;
    }

    // get tcp stream
    WiFiClient *stream = http.getStreamPtr();

    if (!stream) {
        return HTTP_ERROR; 
    }

    auto jsonError = deserializeJson(json, *stream);

    //int ret = json.parseStream(*stream);
    http.end();

    if (jsonError == DeserializationError::Ok)
    {
        DEBUGESPMANAGERF("root->success() = true\n");
        return SUCCESS;
    }
    else
    {
        DEBUGESPMANAGERF("root->success() = false\n");
        return JSON_PARSE_ERROR;
    }
}

void ESPmanager::_HandleSketchUpdate(AsyncWebServerRequest *request)
{
    if (request->hasParam(F("url"), true))
    {
        const String &path = request->getParam(F("url"), true)->value();
        DEBUGESPMANAGERF("path = %s\n", path.c_str());
        _tasker.add([=](Task &t) {
            _upgrade(path.c_str());
        });
    }

    _sendTextResponse(request, 200, FPSTR(fstring_OK));
}

#endif // #webupdate

void ESPmanager::_handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    //char msgdata[100] = {'\0'};
    bool _uploadAuthenticated = true;
    if (!index)
    {
        _uploadAuthenticated = true; // not bothering just yet...
        if (!filename.startsWith("/"))
        {
            filename = "/" + filename;
        }
        request->_tempFile = _fs.open(filename, "w");
        DEBUGESPMANAGERF("UploadStart: %s\n", filename.c_str());
        String reply(F("UploadStart: "));
        reply.concat(filename);
        event_send("", reply);
    }

    if (_uploadAuthenticated && request->_tempFile && len)
    {
        ESP.wdtDisable();
        request->_tempFile.write(data, len);
        ESP.wdtEnable(10);
    }

    if (_uploadAuthenticated && final)
    {
        if (request->_tempFile)
        {
            request->_tempFile.close();
        }
        DEBUGESPMANAGERF("UploadEnd: %s, %u B\n", filename.c_str(), index + len);
        char buf[100]{0};
        snprintf_P(buf, 100, PSTR("UploadFinished:%s (%u)"), filename.c_str(), request->_tempFile.size());
        event_send("", buf);
    }
}

void ESPmanager::_HandleDataRequest(AsyncWebServerRequest *request)
{
#if defined(DEBUGESPMANAGER)
    //List all collected headers
    // int params = request->params(true);

    // int i;
    // for (i = 0; i < params; i++) {
    //     AsyncWebParameter* h = request->getParam(i, true);
    //     DEBUGESPMANAGER.printf("[ESPmanager::_HandleDataRequest] [%s]: %s\n", h->name().c_str(), h->value().c_str());
    // }

    int params = request->params();
    for (int i = 0; i < params; i++)
    {
        AsyncWebParameter *p = request->getParam(i);
        if (p->isFile())
        { //p->isPost() is also true
            DEBUGESPMANAGER.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
        }
        else if (p->isPost())
        {
            DEBUGESPMANAGER.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
        else
        {
            DEBUGESPMANAGER.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
    }

#endif

    String buf;
    DynamicJsonDocument jsonBuffer(allocateJSON());
    JsonObject root = jsonBuffer.to<JsonObject>();

    bool sendsaveandreboot = false;

    if (!_settings)
    {
        _getAllSettings();
    }

    if (!_settings)
    {
        return;
    }

    settings_t &set = *_settings;
    set.start_time = millis(); //  resets the start time... to keep them in memory if being used.

#ifdef DEBUGESPMANAGER
    if (request->hasParam(F("body"), true) && request->getParam(F("body"), true)->value() == "diag")
    {

        // String pass = request->getParam("diag")->value();
        //
        // String result = _hash(pass.c_str());
        //
        // if (_hashCheck(pass.c_str(), result.c_str() )) {
        //
        //         Serial.println("CORRECT PASSWORD");
        //
        // }

        //_dumpSettings();
    }
#endif

    // if (request->hasParam(F("purgeunzipped")))
    // {
    //     // if (request->getParam("body")->value() == "purgeunzipped") {
    //     DEBUGESPMANAGERF("PURGE UNZIPPED FILES\n");
    //     _removePreGzFiles();
    // }

    /*------------------------------------------------------------------------------------------------------------------
                                                                    Reboot command
       ------------------------------------------------------------------------------------------------------------------*/
    if (request->hasParam(F("body"), true))
    {

        //ESPMan_Debugln(F("Has Body..."));

        const String &plainCommand = request->getParam(F("body"), true)->value();

        // Serial.printf("Plaincommand = %s\n", plainCommand.c_str());

        if (plainCommand == F("generalpage"))
        {

            DEBUGESPMANAGERF("Got body\n");
        }

        if (plainCommand == F("save"))
        {
            DEBUGESPMANAGERF("Saving Settings File....\n");
            if (_settings)
            {

                int ERROR = _saveAllSettings(*_settings);

#if defined(DEBUGESPMANAGER)
                File f = _fs.open(SETTINGS_FILE, "r");
                DEBUGESPMANAGER.printf("ESP MANAGER Settings [%u]B:\n", f.size());
                if (f)
                {
                    for (uint i = 0; i < f.size(); i++)
                    {
                        DEBUGESPMANAGER.write(f.read());
                    }
                    DEBUGESPMANAGER.println();
                    f.close();
                }
#endif

                if (ERROR)
                {
                    event_send("", String(ERROR));
                }
                else
                {

                    //event_printf(NULL, "Settings Saved", ERROR);
                    event_send("", F("Settings Saved"));

                    set.changed = false;
                }
            }
        }

        if (plainCommand == F("reboot") || plainCommand == F("restart"))
        {
            DEBUGESPMANAGERF("Rebooting...\n");

            _sendTextResponse(request, 200, FPSTR(fstring_OK));

            _tasker.add([this](Task &t) {
                event_send("", F("Rebooting"));
                delay(100);
                _events.close();
                delay(100);
                ESP.restart();
                delay(100000);
            });
        };

        /*------------------------------------------------------------------------------------------------------------------
                                          WiFi Scanning and Sending of WiFi networks found at boot
           ------------------------------------------------------------------------------------------------------------------*/
        if (plainCommand == F("WiFiDetails") || plainCommand == F("PerformWiFiScan") || plainCommand == "generalpage")
        {

            //************************
            if (plainCommand == F("PerformWiFiScan"))
            {

                int wifiScanState = WiFi.scanComplete();

                DynamicJsonDocument jsonBuffer(allocateJSON());
                JsonObject root = jsonBuffer.to<JsonObject>();

                if (wifiScanState == -2)
                {
                    WiFi.scanNetworks(true);
                    event_send("", F("WiFi Scan Started"));
                    root[F("scan")] = "started";
                }
                else if (wifiScanState == -1)
                {
                    root[F("scan")] = "running";
                }
                else if (wifiScanState > 0)
                {
                    _wifinetworksfound = wifiScanState;
                    std::list<std::pair<int, int>> _container;

                    for (int i = 0; i < _wifinetworksfound; i++)
                    {
                        _container.emplace_back(i, WiFi.RSSI(i)); //  use emplace... less copy/move semantics.
                    }

                    _container.sort([](const std::pair<int, int> &first, const std::pair<int, int> &second) {
                        return (first.second > second.second);
                    });

                    JsonArray Networkarray = root.createNestedArray("networks");

                    if (_wifinetworksfound > MAX_WIFI_NETWORKS)
                    {
                        _wifinetworksfound = MAX_WIFI_NETWORKS;
                    }
                    String reply = String(_wifinetworksfound) + " Networks Found";
                    event_send("", reply);

                    std::list<std::pair<int, int>>::iterator it;

                    int counter = 0;

                    for (it = _container.begin(); it != _container.end(); it++)
                    {
                        if (counter == MAX_WIFI_NETWORKS)
                        {
                            break;
                        }

                        int i = it->first;

                        JsonObject ssidobject = Networkarray.createNestedObject();

                        bool connectedbool = (WiFi.status() == WL_CONNECTED && WiFi.SSID(i) == WiFi.SSID()) ? true : false;
                        uint8_t encryptiontype = WiFi.encryptionType(i);
                        ssidobject[F("ssid")] = WiFi.SSID(i);
                        ssidobject[F("rssi")] = WiFi.RSSI(i);
                        ssidobject[F("connected")] = connectedbool;
                        ssidobject[F("channel")] = WiFi.channel(i);
                        switch (encryptiontype)
                        {
                        case ENC_TYPE_NONE:
                            ssidobject[F("enc")] = "OPEN";
                            break;
                        case ENC_TYPE_WEP:
                            break;
                        case ENC_TYPE_TKIP:
                            ssidobject[F("enc")] = "WPA_PSK";
                            break;
                        case ENC_TYPE_CCMP:
                            ssidobject[F("enc")] = "WPA2_PSK";
                            break;
                        case ENC_TYPE_AUTO:
                            ssidobject[F("enc")] = "AUTO";
                            break;
                        }

                        ssidobject[F("BSSID")] = WiFi.BSSIDstr(i);

                        counter++;
                    }
                }

                sendJsontoHTTP<JsonObject>(root, request);

                if (wifiScanState > 0)
                {
                    WiFi.scanDelete();
                }

                _wifinetworksfound = 0;
                return;
            }
            //*************************

            WiFiMode mode = WiFi.getMode();

            JsonObject generalobject = root.createNestedObject(FPSTR(fstring_General));

            generalobject[FPSTR(fstring_deviceid)] = getHostname();
            generalobject[FPSTR(fstring_OTApassword)] = (set.GEN.OTApassword) ? true : false;
            generalobject[FPSTR(fstring_OTAport)] = set.GEN.OTAport;
            generalobject[FPSTR(fstring_mDNS)] = (set.GEN.mDNSenabled) ? true : false;
            generalobject[FPSTR(fstring_OTAupload)] = (set.GEN.OTAupload) ? true : false;
            generalobject[FPSTR(fstring_updateURL)] = set.GEN.updateURL;
            generalobject[FPSTR(fstring_updateFreq)] = set.GEN.updateFreq;

            JsonObject GenericObject = root.createNestedObject(F("generic"));

            GenericObject[F("channel")] = WiFi.channel();
            GenericObject[F("sleepmode")] = (int)WiFi.getSleepMode();
            GenericObject[F("phymode")] = (int)WiFi.getPhyMode();

            JsonObject STAobject = root.createNestedObject(FPSTR(fstring_STA));

            STAobject[F("connectedssid")] = WiFi.SSID();
            STAobject[F("dhcp")] = (set.STA.dhcp) ? true : false;
            STAobject[F("state")] = (mode == WIFI_STA || mode == WIFI_AP_STA) ? true : false;
            STAobject[FPSTR(fstring_channel)] = WiFi.channel();
            STAobject[F("RSSI")] = WiFi.RSSI();

            //String ip;
            STAobject[FPSTR(fstring_IP)] = WiFi.localIP().toString();
            STAobject[FPSTR(fstring_GW)] = WiFi.gatewayIP().toString();
            STAobject[FPSTR(fstring_SN)] = WiFi.subnetMask().toString();
            STAobject[FPSTR(fstring_DNS1)] = WiFi.dnsIP(0).toString();
            STAobject[FPSTR(fstring_DNS2)] = WiFi.dnsIP(1).toString();
            STAobject[FPSTR(fstring_MAC)] = WiFi.macAddress();

            JsonObject APobject = root.createNestedObject(F("AP"));

            APobject[FPSTR(fstring_ssid)] = (set.AP.ssid.length()) ? set.AP.ssid : set.GEN.host;
            APobject[F("state")] = set.AP.enabled;
            APobject[FPSTR(fstring_IP)] = (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) ? F("192.168.4.1") : WiFi.softAPIP().toString();
            APobject[FPSTR(fstring_visible)] = (set.AP.visible) ? true : false;
            APobject[FPSTR(fstring_pass)] = set.AP.pass;

            softap_config config;

            if (wifi_softap_get_config(&config))
            {

                APobject[FPSTR(fstring_channel)] = config.channel;
            }

            APobject[FPSTR(fstring_MAC)] = WiFi.softAPmacAddress();
            APobject[F("StationNum")] = WiFi.softAPgetStationNum();
        }

        /*------------------------------------------------------------------------------------------------------------------
                                          Send about page details...
           ------------------------------------------------------------------------------------------------------------------*/
        if (plainCommand == F("AboutPage"))
        {

            FSInfo info;
            _fs.info(info);

            const uint8_t bufsize = 50;
            uint32_t sec = millis() / 1000;
            uint32_t min = sec / 60;
            uint32_t hr = min / 60;
            uint32_t day = hr / 24;
            //int Vcc = analogRead(A0);

            char Up_time[bufsize];
            snprintf(Up_time, bufsize, "%02d days %02d hours (%02d:%02d) m:s", (uint32_t)day, uint32_t(hr % 24), uint32_t(min % 60), uint32_t(sec % 60));

            root[F("version_var")] = "Settings Manager V" ESPMANVERSION;
            root[F("compiletime_var")] = _compile_date_time;

            root[F("chipid_var")] = ESP.getChipId();
            root[F("cpu_var")] = ESP.getCpuFreqMHz();
            root[F("sdk_var")] = ESP.getSdkVersion();
            root[F("core_var")] = ESP.getCoreVersion();
            root[F("bootverion_var")] = ESP.getBootVersion();
            root[F("bootmode_var")] = ESP.getBootMode();

            root[F("heap_var")] = ESP.getFreeHeap();
            root[F("millis_var")] = millis();
            root[F("uptime_var")] = String(Up_time);

            root[F("flashid_var")] = ESP.getFlashChipId();
            root[F("flashsize_var")] = formatBytes(ESP.getFlashChipSize());
            String sketchsize = formatBytes(ESP.getSketchSize()); //+ " ( " + String(ESP.getSketchSize()) +  " Bytes)";
            root[F("sketchsize_var")] = sketchsize;
            String freesketchsize = formatBytes(ESP.getFreeSketchSpace()); //+ " ( " + String(ESP.getFreeSketchSpace()) +  " Bytes)";
            root[F("freespace_var")] = freesketchsize;
            root[F("rssi_var")] = WiFi.RSSI();

            JsonObject FSobject = root.createNestedObject("FS"); //  currently everywhere will be still using SPIIFFS
            /*

               struct FSInfo {
                size_t totalBytes;
                size_t usedBytes;
                size_t blockSize;
                size_t pageSize;
                size_t maxOpenFiles;
                size_t maxPathLength;
               };
             */
            FSobject[F("totalBytes")] = formatBytes(info.totalBytes);
            FSobject[F("usedBytes")] = formatBytes(info.usedBytes);

            JsonObject Resetobject = root.createNestedObject("reset");

            Resetobject[F("reason")] = ESP.getResetReason();
            Resetobject[F("info")] = ESP.getResetInfo();
        }

        /*------------------------------------------------------------------------------------------------------------------
                                          Update handles...
           ------------------------------------------------------------------------------------------------------------------*/

        if (plainCommand == F("UpdateDetails"))
        {
            root[FPSTR(fstring_updateURL)] = set.GEN.updateURL;
            root[FPSTR(fstring_updateFreq)] = set.GEN.updateFreq;
        }

        if (plainCommand == F("formatSPIFFS"))
        {
            DEBUGESPMANAGERF("Format SPIFFS\n");
            event_send("", F("Formatting FS"));
            _sendTextResponse(request, 200, FPSTR(fstring_OK));

            _tasker.add([this](Task &t) {
                _getAllSettings();
                _fs.format();
                if (_settings)
                {
                    _saveAllSettings(*_settings);
                }
                DEBUGESPMANAGERF(" done\n");
                event_send("", F("Formatting done"));
            });
        }

        if (plainCommand == F("deletesettings"))
        {

            DEBUGESPMANAGERF("Delete Settings File\n");
            if (_fs.remove(SETTINGS_FILE))
            {
                DEBUGESPMANAGERF(" done");
                event_send("", F("Settings File Removed"));
            }
            else
            {
                DEBUGESPMANAGERF(" failed");
            }
        }

        if (plainCommand == F("resetwifi"))
        {

            _tasker.add([this](Task &t) {
                event_send("", F("Settings File Removed"));
                delay(100);
                _events.close();
                delay(100);

                WiFi.disconnect();
                ESP.eraseConfig();
                ESP.restart();
            });
        }

        if (plainCommand == F("factoryReset"))
        {

            _sendTextResponse(request, 200, "Factory Reset Done");

            _tasker.add([this](Task &t) {
                factoryReset();
                delay(100);
                ESP.restart();
                while (1)
                    ;
            });
            return;
        }

        if (plainCommand == F("discover"))
        {

            DEBUGESPMANAGERF("Discover Devices\n");

            if (_devicefinder && !_deviceFinderTimer)
            {
                _devicefinder->cacheResults(true);
                _devicefinder->ping();
                _deviceFinderTimer = millis();

                _tasker.add([this](Task &t) {
                           if (millis() - _deviceFinderTimer > _ESPdeviceTimeout)
                           {
                               t.setRepeat(false);
                               if (_devicefinder)
                               {
                                   uint32_t __attribute__((unused)) pre_heap = ESP.getFreeHeap();
                                   _devicefinder->cacheResults(false);
                                   DEBUGESPMANAGERF("Removing Found Devices after %us freeing %u\n", _ESPdeviceTimeout / 1000, ESP.getFreeHeap() - pre_heap);
                               }
                               _deviceFinderTimer = 0;
                           }
                       })
                    .setTimeout(1000)
                    .setRepeat(true);
            }

            if (_devicefinder)
            {
                _devicefinder->ping();
                _populateFoundDevices(root);
            }
        }

        if (plainCommand == F("getDevices"))
        {

            _populateFoundDevices(root);

            //  reset the timer so as to not delete the results.
            if (_deviceFinderTimer)
            {
                _deviceFinderTimer = millis();
            }
        }

    } //  end of if plaincommand

    /*
                Individual responsces...   THIS NEEDS LOOKING AT VERY CLOSELY>>  new used a lot...  channel change from wizard present... 
    */

    if (request->hasParam(FPSTR(fstring_ssid), true) && request->hasParam(FPSTR(fstring_pass), true))
    {

        bool APChannelchange = false;
        int channel = -1;

        const String &ssid = request->getParam(FPSTR(fstring_ssid), true)->value();
        const String &psk = request->getParam(FPSTR(fstring_pass), true)->value();

        if (ssid.length() > 0)
        {
            // _HTTP.arg("pass").length() > 0) {  0 length passwords should be ok.. for open
            // networks.
            if (ssid.length() < 33 && psk.length() < 33)
            {

                if (ssid != WiFi.SSID() || psk != WiFi.psk() || !set.STA.enabled)
                {

                    bool safety = false;

                    if (request->hasParam(F("removesaftey"), true))
                    {
                        safety = (request->getParam(F("removesaftey"), true)->value() == "No") ? false : true;
                    }

                    settings_t::STA_t *newsettings = new settings_t::STA_t(set.STA);

                    newsettings->ssid = ssid.c_str();
                    newsettings->pass = psk.c_str();
                    newsettings->enabled = true;

                    DEBUGESPMANAGERF("applied new ssid & psk to tmp obj ssid =%s, pass=%s\n", newsettings->ssid.c_str(), newsettings->pass.c_str());

                    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)
                    {

                        int currentchannel = WiFi.channel();

                        if (request->hasParam(F("STAchannel_desired"), true))
                        {

                            int desired_channal = request->getParam(F("STAchannel_desired"), true)->value().toInt();

                            if (desired_channal != currentchannel && desired_channal >= 0 && currentchannel >= 0)
                            {

                                DEBUGESPMANAGERF("AP Channel change required: current = %i, desired = %i\n", currentchannel, desired_channal);
                                APChannelchange = true;
                                channel = desired_channal;
                            }
                            else
                            {
                                DEBUGESPMANAGERF("AP Channel change NOT required: current = %i, desired = %i\n", currentchannel, desired_channal);
                            }
                        }

                        //
                    }

                    _tasker.add([safety, newsettings, request, this, APChannelchange, channel](Task &t) {
                        //_syncCallback = [safety, newsettings, request, this, APChannelchange, channel]() {

                        WiFiresult = 0;
                        uint32_t starttime = millis();

                        if (APChannelchange)
                        {

                            uint8_t connected_station_count = WiFi.softAPgetStationNum();

                            DEBUGESPMANAGERFRAW("Changing AP channel to %u :", channel);

                            event_send("", F("Changing AP Channel..."));

                            /*   struct softap_config {
                                uint8 ssid[32];
                                uint8 password[64];
                                uint8 ssid_len;
                                uint8 channel;  // support 1 ~ 13
                                uint8 authmode;
                                uint8 ssid_hidden;
                                uint8 max_connection;
                                uint16 beacon_interval;  // 100 ~ 60000 ms, default 100
                            */

                            //bool result = _emergencyMode(true, channel);
                            bool result = false;

                            struct softap_config *_currentconfig = new softap_config;

                            if (_currentconfig && wifi_softap_get_config(_currentconfig))
                            {
                                _currentconfig->channel = channel;
                                result = wifi_softap_set_config_current(_currentconfig);
                            }

                            if (result)
                            {
                                DEBUGESPMANAGERFRAW("Waiting For AP reconnect\n");
                                starttime = millis();

                                uint32_t dottimer = millis();

                                while (WiFi.softAPgetStationNum() < connected_station_count)
                                {

                                    // if (_dns)
                                    // {
                                    //     _dns->processNextRequest();
                                    // }

                                    //yield();
                                    delay(10);
                                    if (millis() - dottimer > 1000)
                                    {

                                        DEBUGESPMANAGERFRAW(".");

                                        dottimer = millis();
                                    }

                                    if (millis() - starttime > 60000)
                                    {
                                        DEBUGESPMANAGERFRAW("Error waiting for AP reconnect\n");
                                        WiFiresult = 2;
                                        WiFi.enableSTA(false);
                                        if (newsettings)
                                        {
                                            delete newsettings;
                                        }

                                        return true;
                                        break;
                                    }
                                }
                            }
                            else
                            {
                                DEBUGESPMANAGERFRAW("Error: %i\n", result);
                            }
                        }

                        starttime = millis(); // reset the start timer....
                        event_send("", F("Updating WiFi Settings"));

                        delay(10);

                        int ERROR = _initialiseSTA(*newsettings);

                        if (!ERROR)
                        {
                            DEBUGESPMANAGERFRAW("CALLBACK: Settings successfull\n");
                            WiFiresult = 1;

                            if (!_settings)
                            {
                                _getAllSettings();
                            }

                            if (_settings && newsettings)
                            {
                                _settings->STA = *newsettings;
                                _settings->changed = true;
                                DEBUGESPMANAGERFRAW("CALLBACK: Settings Applied\n");
                            }
                        }
                        else if (ERROR == NO_CHANGES)
                        {
                            DEBUGESPMANAGERFRAW("CALLBACK: No changes....\n");
                            WiFiresult = 1;
                        }
                        else
                        {
                            WiFiresult = 2;
                            DEBUGESPMANAGERFRAW("ERROR: %i\n", ERROR);
                            if (_settings)
                            {
                                if (!_initialiseSTA(_settings->STA))
                                { //  go back to old settings...
                                    event_send("", F("Old Settings Restored"));
                                }
                            }
                        }

                        event_send("", F("WiFi Settings Updated"));

                        if (newsettings)
                        {
                            delete newsettings;
                        }

                        return true;
                    }); //  end of lambda...

                    _sendTextResponse(request, 200, F("accepted"));

                    return;
                }
            }
        }
    }
    //*******************************************

    //  This is outside the loop...  wifiresult is a static to return previous result...
    if (request->hasParam(F("body"), true) && request->getParam(F("body"), true)->value() == F("WiFiresult"))
    {

        if (WiFiresult == 1 && WiFi.localIP() != INADDR_NONE)
        {
            WiFiresult = 4; // connected
        }

        DEBUGESPMANAGERF("WiFiResult = %i [%s]\n", WiFiresult, WiFi.localIP().toString().c_str());
        _sendTextResponse(request, 200, String(WiFiresult).c_str());
        return;
    }
    /*------------------------------------------------------------------------------------------------------------------

                                     STA config
       ------------------------------------------------------------------------------------------------------------------*/

    if (request->hasParam(F("enable-STA"), true))
    {

        bool changes = false;
        settings_t::STA_t *newsettings = new settings_t::STA_t(set.STA);

        if (newsettings)
        {

            /*
                    ENABLED
             */

            bool enable = request->getParam(F("enable-STA"), true)->value().equals("on");

            if (enable != newsettings->enabled)
            {
                newsettings->enabled = enable;
                changes = true;
            }

            /*
                    DHCP and Config
             */
            if (request->hasParam(F("enable-dhcp"), true))
            {

                bool dhcp = request->getParam(F("enable-dhcp"), true)->value().equals("on");

                //

                if (dhcp)
                {
                    DEBUGESPMANAGERF("dhcp = on\n");

                    if (!_settings->STA.dhcp)
                    {
                        changes = true;
                    }

                    newsettings->dhcp = true;
                    newsettings->hasConfig = false;
                    newsettings->IP = INADDR_NONE;
                    newsettings->GW = INADDR_NONE;
                    newsettings->SN = INADDR_NONE;
                    newsettings->DNS1 = INADDR_NONE;
                    newsettings->DNS2 = INADDR_NONE;
                }
                else
                {
                    DEBUGESPMANAGERF("dhcp = off\n");

                    if (_settings->STA.dhcp)
                    {
                        changes = true;
                    }

                    bool IPres{false};
                    bool GWres{false};
                    bool SNres{false};
                    bool DNSres{false};

                    if (request->hasParam(FPSTR(fstring_IP), true) &&
                        request->hasParam(FPSTR(fstring_GW), true) &&
                        request->hasParam(FPSTR(fstring_SN), true) &&
                        request->hasParam(FPSTR(fstring_DNS1), true))
                    {

                        IPres = newsettings->IP.fromString(request->getParam(FPSTR(fstring_IP), true)->value());
                        GWres = newsettings->GW.fromString(request->getParam(FPSTR(fstring_GW), true)->value());
                        SNres = newsettings->SN.fromString(request->getParam(FPSTR(fstring_SN), true)->value());
                        DNSres = newsettings->DNS1.fromString(request->getParam(FPSTR(fstring_DNS1), true)->value());
                    }

                    if (IPres && GWres && SNres && DNSres)
                    {

                        //  apply settings if any of these are different to current settings...
                        if (newsettings->IP != _settings->STA.IP || newsettings->GW != _settings->STA.GW || newsettings->SN != _settings->STA.SN || newsettings->DNS1 != _settings->STA.DNS1)
                        {
                            changes = true;
                        }
                        DEBUGESPMANAGERF("Config Set\n");
                        newsettings->hasConfig = true;
                        newsettings->dhcp = false;

                        if (request->hasParam(FPSTR(fstring_DNS2), true))
                        {

                            bool res = newsettings->DNS2.fromString(request->getParam(FPSTR(fstring_DNS2), true)->value());
                            if (res)
                            {
                                DEBUGESPMANAGERF("DNS 2 %s\n", newsettings->DNS2.toString().c_str());
                                if (newsettings->DNS2 != _settings->STA.DNS2)
                                {
                                    changes = true;
                                }
                            }
                        }
                    }

                    DEBUGESPMANAGERF("IP %s, GW %s, SN %s\n", (IPres) ? "set" : "error", (GWres) ? "set" : "error", (SNres) ? "set" : "error");
                }

                //}
            }

            if (changes)
            {

                _tasker.add([this, newsettings](Task &t) {
                    event_send("", F("Updating WiFi Settings"));

                    delay(10);

                    DEBUGESPMANAGERF("*** CALLBACK: dhcp = %s\n", (newsettings->dhcp) ? "true" : "false");
                    DEBUGESPMANAGERF("*** CALLBACK: hasConfig = %s\n", (newsettings->hasConfig) ? "true" : "false");

                    int ERROR = _initialiseSTA(*newsettings);

                    DEBUGESPMANAGERF("*** CALLBACK: ERROR = %i\n", ERROR);

                    //WiFi.printDiag(Serial);

                    if (!ERROR || (ERROR == STA_DISABLED && newsettings->enabled == false))
                    {
                        DEBUGESPMANAGERF("CALLBACK: Settings successfull\n");

                        if (!_settings)
                        {
                            _getAllSettings();
                        }

                        if (_settings)
                        {
                            _settings->STA = *newsettings;
                            _settings->changed = true;
                            DEBUGESPMANAGERF("CALLBACK: Settings Applied\n");
                            event_send("", F("Success"));

                            //save_flag = true;
                        }
                        else
                        {
                            event_send("", String(SETTINGS_NOT_IN_MEMORY));
                        }
                    }
                    else
                    {
                        DEBUGESPMANAGERF("ERORR: Settings NOT applied successfull %i\n", ERROR);
                        event_send("", String(ERROR));

                        _getAllSettings();
                        if (_settings)
                        {
                            if (_initialiseSTA(_settings->STA))
                            {
                                DEBUGESPMANAGERF("OLD settings reapplied\n");
                            }
                        }
                    }

                    delete newsettings;

                    return true;
                });
            }
            else
            {
                event_send("", F("No Changes Made"));
                DEBUGESPMANAGERF("No changes Made\n");
            }
        }
    }

    /*------------------------------------------------------------------------------------------------------------------

                                     AP config
       ------------------------------------------------------------------------------------------------------------------*/

    if (request->hasParam(F("enable-AP"), true))
    {

        bool changes = false;
        bool abortchanges = false;

        settings_t::AP_t *newsettings = new settings_t::AP_t(set.AP); // creates a copy of current settings using new... smart_Ptr don't work well yet for lambda captures

        if (newsettings)
        {

            /*
                    ENABLED
             */

            newsettings->ssid = set.GEN.host;

            bool enabled = request->getParam(F("enable-AP"), true)->value().equals("on");

            if (enabled != newsettings->enabled)
            {
                newsettings->enabled = enabled;
                changes = true;
            }

            if (request->hasParam(FPSTR(fstring_pass), true))
            {

                const String &pass = request->getParam(FPSTR(fstring_pass), true)->value();

                if (pass.length() > 8 && pass.length() < 63)
                {
                    // fail passphrase to long or short!
                    DEBUGESPMANAGERF("[AP] fail passphrase to long or short!\n");
                    event_send("", String(PASSWOROD_INVALID));
                    abortchanges = true;
                }

                if (pass.length() && newsettings->pass != pass)
                {
                    newsettings->pass = pass;
                    DEBUGESPMANAGERF("New AP pass = %s\n", newsettings->pass.c_str());
                    changes = true;
                }
            }

            if (request->hasParam(FPSTR(fstring_channel), true))
            {
                int channel = request->getParam(FPSTR(fstring_channel), true)->value().toInt();

                if (channel > 13)
                {
                    channel = 13;
                }

                if (channel != newsettings->channel)
                {
                    newsettings->channel = channel;
                    changes = true;
                    DEBUGESPMANAGERF("New Channel = %u\n", newsettings->channel);
                }
            }

            if (request->hasParam(FPSTR(fstring_IP), true))
            {

                IPAddress newIP;
                bool result = newIP.fromString(request->getParam(FPSTR(fstring_IP), true)->value());

                if (result)
                {

                    changes = true;

                    if (newIP == IPAddress(192, 168, 4, 1))
                    {
                        newsettings->hasConfig = false;
                        newsettings->IP = newIP;
                        newsettings->GW = INADDR_NONE;
                        newsettings->SN = INADDR_NONE;
                    }
                    else
                    {
                        newsettings->hasConfig = true;
                        newsettings->IP = newIP;
                        newsettings->GW = newIP;
                        newsettings->SN = IPAddress(255, 255, 255, 0);
                    }

                    DEBUGESPMANAGERF("New AP IP = %s\n", newsettings->IP.toString().c_str());
                }
            }

            if (changes && !abortchanges)
            {

                _tasker.add([this, newsettings](Task &t) {
                    //_syncCallback = [this, newsettings] () {

                    //event_printf(NULL, "Updating AP Settings");
                    //event_printf_P(NULL, PSTR("Updating AP Settings"));
                    event_send("", F("Updating AP Settings"));

                    delay(10);

                    int ERROR = _initialiseAP(*newsettings);

                    if (!ERROR || (ERROR == AP_DISABLED && newsettings->enabled == false))
                    {
                        DEBUGESPMANAGERF("AP CALLBACK: Settings successfull\n");

                        if (!_settings)
                        {
                            _getAllSettings();
                        }

                        if (_settings)
                        {
                            _settings->AP = *newsettings;
                            _settings->changed = true;
                            DEBUGESPMANAGERF("CALLBACK: Settings Applied\n");
                            event_send("", F("Success"));
                        }
                        else
                        {
                            event_send("", String(SETTINGS_NOT_IN_MEMORY));
                        }
                    }
                    else
                    {

                        _getAllSettings();

                        if (_settings)
                        {
                            DEBUGESPMANAGERF("Restoring old settings ERROR = %is\n", ERROR);

                            _initialiseAP(_settings->AP);
                        }
                        event_send("", String(ERROR));
                    }

                    delete newsettings;

                    return true;
                });
            }
            else
            {
                event_send("", F("No Changes Made"));

                DEBUGESPMANAGERF("No changes Made\n");
            }
        }
    } //  end of enable-AP

    /*------------------------------------------------------------------------------------------------------------------

                                     Device Name
       ------------------------------------------------------------------------------------------------------------------*/

    if (request->hasParam(FPSTR(fstring_deviceid), true))
    {

        const String &newid = request->getParam(FPSTR(fstring_deviceid), true)->value();

        DEBUGESPMANAGERF("Device ID func hit %s\n", newid.c_str());

        if (newid.length() && newid.length() < 32 && newid != set.GEN.host)
        {

            set.GEN.host = newid;
            set.changed = true;
            //event_printf(NULL, "Device ID: %s", set.GEN.host() );
            String reply = "Device ID: " + set.GEN.host;
            event_send("", reply);
            sendsaveandreboot = true;
        }
    }
    /*------------------------------------------------------------------------------------------------------------------

                                     Restart wifi
       ------------------------------------------------------------------------------------------------------------------*/

    /*------------------------------------------------------------------------------------------------------------------

                                     OTA config
       ------------------------------------------------------------------------------------------------------------------*/
    if (request->hasParam(FPSTR(fstring_OTAupload), true))
    {

        // save_flag = true;

        bool command = request->getParam(FPSTR(fstring_OTAupload), true)->value().equals("on");

        if (command != set.GEN.OTAupload)
        {

            //_OTAupload = command;
            set.GEN.OTAupload = command;
            set.changed = true;

            DEBUGESPMANAGERF("_OTAupload = %s\n", (set.GEN.OTAupload) ? "enabled" : "disabled");
        }

    } // end of OTA enable

    if (request->hasParam(FPSTR(fstring_OTApassword), true))
    {

        char pass_confirm[40] = {0};

        strncpy_P(pass_confirm, fstring_OTApassword, 30);
        strncat_P(pass_confirm, PSTR("_confirm"), 9);

        if (request->hasParam(pass_confirm, true))
        {

            const String &S_pass = request->getParam(FPSTR(fstring_OTApassword), true)->value();
            const String &S_confirm = request->getParam(pass_confirm, true)->value();

            Serial.printf("S_pass = %s, len = %u", S_pass.c_str(), S_pass.length());

            const char *pass = S_pass.c_str();
            const char *confirm = S_confirm.c_str();

            if (pass && confirm && !strncmp(pass, confirm, 40))
            {

                DEBUGESPMANAGERF("Passwords Match\n");
                set.changed = true;
                MD5Builder md5;
                md5.begin();
                md5.add(pass);
                md5.calculate();
                set.GEN.OTApassword = md5.toString().c_str();

                if (S_pass.length() == 0)
                {
                    set.GEN.OTApassword = "";
                    Serial.println("Password set but making null");
                }
                //set.GEN.OTApassword = pass;

                sendsaveandreboot = true;
            }
            else
            {
                //event_printf(nullptr, string_ERROR_toString, getError(PASSWORD_MISMATCH).c_str() ) ;

                event_send("", String(PASSWORD_MISMATCH));
            }
        }

    } // end of OTApass

    /*
       ARG: 0, "enable-AP" = "on"
       ARG: 1, "setAPsetip" = "0.0.0.0"
       ARG: 2, "setAPsetmac" = "1A%3AFE%3A34%3AA4%3A4C%3A73"
     */

    if (request->hasParam(FPSTR(fstring_mDNS), true))
    {

        //save_flag = true;

        bool command = request->getParam(FPSTR(fstring_mDNS), true)->value().equals("on");

        if (command != set.GEN.mDNSenabled)
        {
            set.GEN.mDNSenabled = command;
            set.changed = true;
            DEBUGESPMANAGERF("mDNS set to : %s\n", (command) ? "on" : "off");
            sendsaveandreboot = true;
            //  InitialiseFeatures();
        }
    } // end of OTA enable

    /*------------------------------------------------------------------------------------------------------------------

                                            New UPGRADE
       ------------------------------------------------------------------------------------------------------------------*/

    if (request->hasParam(FPSTR(fstring_updateURL), true))
    {

        const String &newpath = request->getParam(FPSTR(fstring_updateURL), true)->value();

        DEBUGESPMANAGERF("UpgradeURL: %s\n", newpath.c_str());

        if (newpath && newpath.length() && set.GEN.updateURL != newpath)
        {

            set.GEN.updateURL = newpath;
            set.changed = true;
        }
    }

    if (request->hasParam(FPSTR(fstring_updateFreq), true))
    {

        uint updateFreq = request->getParam(FPSTR(fstring_updateFreq), true)->value().toInt();

        if (updateFreq < 0)
        {
            updateFreq = 0;
        }

        if (updateFreq != set.GEN.updateFreq)
        {
            set.GEN.updateFreq = updateFreq;
            set.changed = true;
        }
    }

    if (request->hasParam(F("PerformUpdate"), true))
    {

        String path = String();

        if (set.GEN.updateURL)
        {
            path = set.GEN.updateURL;
        }

        _tasker.add([this, path](Task &t) {
            _upgrade(path.c_str());
        });
    }

    root[FPSTR(fstring_changed)] = (set.changed) ? true : false;
    root[F("heap")] = ESP.getFreeHeap();

#ifdef ESPMANAGER_GIT_TAG
    root[F("espmanagergittag")] = ESPMANAGER_GIT_TAG;
#endif

    if (_fs.exists("/crashlog.txt"))
    {
        root[F("crashlog")] = true;
    }
    else
    {
        root[F("crashlog")] = false;
    }

    sendJsontoHTTP<JsonObject>(root, request);

    if (sendsaveandreboot)
    {
        event_send("", FPSTR(fstring_saveandreboot));
    }
}

ESPMAN_ERR_t ESPmanager::_initialiseAP()
{

    //int ERROR = 0;

    //  get the settings from SPIFFS if settings PTR is null
    if (!_settings)
    {
        _getAllSettings();
    }

    if (!_settings->AP.ssid.length())
    {
        DEBUGESPMANAGERF("No SSID specified: defaulting to host\n");
        _settings->AP.ssid = _settings->GEN.host;
    }

    return _initialiseAP(_settings->AP);
}

ESPMAN_ERR_t ESPmanager::_initialiseAP(settings_t::AP_t &settings)
{

#ifdef DEBUGESPMANAGER
    DEBUGESPMANAGERF("-------  PRE CONFIG ------\n");
    _dumpAP(settings);
    DEBUGESPMANAGERF("--------------------------\n");
#endif

    if (settings.enabled == false)
    {
        DEBUGESPMANAGERF("AP DISABLED\n");
        if (WiFi.enableAP(false))
        {
            return SUCCESS;
        }
        else
        {
            return ERROR_DISABLING_AP;
        }
    }

    if (!WiFi.enableAP(true))
    {
        return ERROR_ENABLING_AP;
    }

    if (settings.hasConfig)
    {

        bool result = WiFi.softAPConfig(settings.IP, settings.GW, settings.SN);

        if (!result)
        {
            return ERROR_SETTING_CONFIG;
        }
    }

    if (!settings.ssid)
    {
        settings.ssid = "ESP";
    }

    DEBUGESPMANAGERF("ENABLING AP : channel %u, name %s, channel = %u, hidden = %u, pass = %s \n", settings.channel, settings.ssid.c_str(), settings.channel, !settings.visible, settings.pass.c_str());

    if (!WiFi.softAP(settings.ssid.c_str(), (settings.pass) ? settings.pass.c_str() : nullptr, settings.channel, !settings.visible))
    {
        return ERROR_ENABLING_AP;
    }

    return SUCCESS;
}

/*


      STA  stuff


 */

ESPMAN_ERR_t ESPmanager::_initialiseSTA()
{

    ESPMAN_ERR_t ERROR = SUCCESS;

    if (!_settings)
    {
        _getAllSettings();
    }

    if (_settings)
    {
        ERROR = _initialiseSTA(_settings->STA);
        if (!ERROR)
        {
            if (_settings->GEN.host && !WiFi.hostname(_settings->GEN.host.c_str()))
            {
                DEBUGESPMANAGERF("ERROR setting Hostname\n");
            }
            else
            {
                DEBUGESPMANAGERF("Hostname set : %s\n", _settings->GEN.host.c_str());
            }
            DEBUGESPMANAGERF("IP = %s\n", WiFi.localIP().toString().c_str());
            return SUCCESS;
        }
        else
        {
            return ERROR;
        }
    }
    else
    {
        return MALLOC_FAIL;
    }
}

ESPMAN_ERR_t ESPmanager::_initialiseSTA(settings_t::STA_t &set)
{

#ifdef DEBUGESPMANAGER
    DEBUGESPMANAGERF("-------  PRE CONFIG ------\n");
    _dumpSTA(set);
    DEBUGESPMANAGERF("--------------------------\n");
#endif

    if (!set.enabled)
    {
        if (WiFi.enableSTA(false))
        {
            return SUCCESS;
        }
        else
        {
            return ERROR_DISABLING_STA;
        }
    }

    if (!set.ssid)
    {
        return NO_STA_SSID;
    }
    //  this appears to be a work around for a corrupted flash causing endless boot loops.  https://github.com/esp8266/Arduino/issues/1997
    //  https://github.com/esp8266/Arduino/pull/6965
    //  this still causes issues...
    if (_bootState.RebootOnly == false)
    {
        DEBUGESPMANAGERF("TEMP FIX for SDK applied\n");
        ESP.eraseConfig();
        WiFi.persistent(false);
        WiFi.disconnect(true);
    }
    else
    {
        DEBUGESPMANAGERF("Set to Persistent...\n");
        WiFi.persistent(true);
    }

    //https://github.com/esp8266/Arduino/blob/81a10a48af8f619f232f5a2c93d2562be6bdb296/libraries/ESP8266WiFi/src/ESP8266WiFiGeneric.cpp#L388

    if (!WiFi.enableSTA(true))
    {
        return ERROR_ENABLING_STA;
    }

    if (set.hasConfig && set.IP != INADDR_NONE && set.GW != INADDR_NONE && set.SN != INADDR_NONE)
    {
        //      bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = (uint32_t)0x00000000, IPAddress dns2 = (uint32_t)0x00000000);
        DEBUGESPMANAGERF("IP %s\n", set.IP.toString().c_str());
        DEBUGESPMANAGERF("GW %s\n", set.GW.toString().c_str());
        DEBUGESPMANAGERF("SN %s\n", set.SN.toString().c_str());
        DEBUGESPMANAGERF("DNS1 %s\n", set.DNS1.toString().c_str());
        DEBUGESPMANAGERF("DNS2 %s\n", set.DNS2.toString().c_str());

        WiFi.begin();

        // check if they are valid...
        if (!WiFi.config(set.IP, set.GW, set.SN, set.DNS1, set.DNS2))
        //if (!WiFi.config( settings.IP, settings.GW, settings.SN ))
        {
            return WIFI_CONFIG_ERROR;
        }
        else
        {
            set.dhcp = false;
            DEBUGESPMANAGERF("Config Applied\n");
        }
    }
    else
    {
        set.dhcp = true;
        WiFi.config(INADDR_ANY, INADDR_ANY, INADDR_ANY);
    }

    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);

    if (set.ssid && set.pass)
    {
        DEBUGESPMANAGERF("Using ssid = %s, pass = %s\n", set.ssid.c_str(), set.pass.c_str());
        if (!WiFi.begin(set.ssid.c_str(), set.pass.c_str()))
        {
            return ERROR_WIFI_BEGIN;
        }
    }
    else if (set.ssid)
    {
        DEBUGESPMANAGERF("Using ssid = %s\n", set.ssid.c_str());
        if (!WiFi.begin(set.ssid.c_str()))
        {
            return ERROR_WIFI_BEGIN;
        }
    }

    DEBUGESPMANAGERF("Begin Done: Now Connecting\n");

    return SUCCESS;
}

//  allows creating of a seperate config
//  need to add in captive portal to setttings....
ESPMAN_ERR_t ESPmanager::_emergencyMode(bool shutdown, int channel)
{

    DEBUGESPMANAGERF("***** EMERGENCY mode **** \n");

    IPAddress testIP = WiFi.softAPIP();

    DEBUGESPMANAGERF("Preinit AP IP:[%s], isSet=%s\n", testIP.toString().c_str(), (testIP.isSet()) ? "true" : "false");

    if (channel == -1)
    {
        channel = WiFi.channel();
        channel = 1;
    }

    if (shutdown)
    {
        WiFi.disconnect(true); //  Disable STA. makes AP more stable, stops 1sec reconnect
    }

    //_APtimer = millis();

    //  creats a copy of settings so they are not changed...
    settings_t set;
    _getAllSettings(set);

    set.AP.ssid = set.GEN.host;
    set.AP.channel = channel;

    if (!set.AP.pass && set.STA.pass)
    {
        set.AP.pass = set.STA.pass;
    }
    else if (!set.AP.pass)
    {
        set.AP.pass = F(DEFAULT_AP_PASS);
    }

    set.AP.enabled = true;
    DEBUGESPMANAGERF("*****  Debug:  WiFi channel in EMERGENCY mode channel = %u\n", set.AP.channel);
    auto result = _initialiseAP(set.AP);
    testIP = WiFi.softAPIP();
    DEBUGESPMANAGERF("Postinit AP IP:[%s], isSet=%s\n", testIP.toString().c_str(), (testIP.isSet()) ? "true" : "false");

    return result;
}

ESPMAN_ERR_t ESPmanager::_getAllSettings()
{

    if (!_settings)
    {
        _settings = new settings_t;
    }

    if (!_settings)
    {
        return MALLOC_FAIL;
    }

    if (_settings->changed)
    {
        return SUCCESS; // dont overwrite changes already in memory...
    }

    ESPMAN_ERR_t ERROR = SUCCESS;

    ERROR = _getAllSettings(*_settings);

    if (!ERROR)
    {
        _settings->configured = true;
    }
    else
    {
        _settings->configured = false;
    }

    if (!_settings->GEN.host.length())
    {
        char tmp[33] = {'\0'};
        snprintf(tmp, 32, "esp8266-%06x", ESP.getChipId());
        _settings->GEN.host = tmp;
    }

    WiFi.hostname(_settings->GEN.host.c_str());

    return ERROR;
}

ESPMAN_ERR_t ESPmanager::_getAllSettings(settings_t &set)
{

    DynamicJsonDocument json(allocateJSON());
    uint8_t settingsversion = 0;

    ESPMAN_ERR_t ERROR = SUCCESS;

    File f = _fs.open(SETTINGS_FILE, "r");

    if (!f)
    {
        return FS_FILE_OPEN_ERROR;
    }

    auto jsonError = deserializeJson(json, f);

    if (jsonError != DeserializationError::Ok)
    {
        return ERROR;
    }

    json.shrinkToFit();

    JsonObject root = json.as<JsonObject>();

    /*
          General Settings
     */

    if (root.containsKey(FPSTR(fstring_General)))
    {

        JsonObject settingsJSON = root[FPSTR(fstring_General)];

        if (settingsJSON.containsKey(FPSTR(fstring_settingsversion)))
        {
            settingsversion = settingsJSON[FPSTR(fstring_settingsversion)].as<uint8_t>();
        }

        if (settingsJSON.containsKey(FPSTR(fstring_host)))
        {
            set.GEN.host = settingsJSON[FPSTR(fstring_host)].as<const char *>();
        }

        if (settingsJSON.containsKey(FPSTR(fstring_mDNS)))
        {
            set.GEN.mDNSenabled = settingsJSON[FPSTR(fstring_mDNS)];
        }

        if (settingsJSON.containsKey(FPSTR(fstring_updateURL)))
        {
            set.GEN.updateURL = settingsJSON[FPSTR(fstring_updateURL)].as<const char *>();
        }

        if (settingsJSON.containsKey(FPSTR(fstring_updateFreq)))
        {
            set.GEN.updateFreq = settingsJSON[FPSTR(fstring_updateFreq)];
        }

        if (settingsJSON.containsKey(FPSTR(fstring_OTApassword)))
        {
            set.GEN.OTApassword = settingsJSON[FPSTR(fstring_OTApassword)].as<const char *>();
        }

        if (settingsJSON.containsKey(FPSTR(fstring_OTAupload)))
        {
            set.GEN.OTAupload = settingsJSON[FPSTR(fstring_OTAupload)];
        }
    }

    /*
           STA settings
     */

    if (root.containsKey(FPSTR(fstring_STA)))
    {

        JsonObject STAjson = root[FPSTR(fstring_STA)];

        if (STAjson.containsKey(FPSTR(fstring_enabled)))
        {
            set.STA.enabled = STAjson[FPSTR(fstring_enabled)];
        }

        if (STAjson.containsKey(FPSTR(fstring_ssid)))
        {
            if (strnlen(STAjson[FPSTR(fstring_ssid)], 100) < MAX_SSID_LENGTH)
            {

                set.STA.ssid = STAjson[FPSTR(fstring_ssid)].as<const char *>();
                //strncpy( &settings.ssid[0], STAjson["ssid"], strlen(STAjson["ssid"]) );
            }
        }

        if (STAjson.containsKey(FPSTR(fstring_pass)))
        {
            if (strnlen(STAjson[FPSTR(fstring_pass)], 100) < MAX_PASS_LENGTH)
            {
                set.STA.pass = STAjson[FPSTR(fstring_pass)].as<const char *>();
                //strncpy( &settings.pass[0], STAjson["pass"], strlen(STAjson["pass"]) );
            }
        }

        if (STAjson.containsKey(FPSTR(fstring_IP)) && STAjson.containsKey(FPSTR(fstring_GW)) && STAjson.containsKey(FPSTR(fstring_SN)) && STAjson.containsKey(FPSTR(fstring_DNS1)))
        {
            //set.STA.hasConfig = true;
            set.STA.IP = IPAddress(STAjson[FPSTR(fstring_IP)][0], STAjson[FPSTR(fstring_IP)][1], STAjson[FPSTR(fstring_IP)][2], STAjson[FPSTR(fstring_IP)][3]);
            set.STA.GW = IPAddress(STAjson[FPSTR(fstring_GW)][0], STAjson[FPSTR(fstring_GW)][1], STAjson[FPSTR(fstring_GW)][2], STAjson[FPSTR(fstring_GW)][3]);
            set.STA.SN = IPAddress(STAjson[FPSTR(fstring_SN)][0], STAjson[FPSTR(fstring_SN)][1], STAjson[FPSTR(fstring_SN)][2], STAjson[FPSTR(fstring_SN)][3]);
            set.STA.DNS1 = IPAddress(STAjson[FPSTR(fstring_DNS1)][0], STAjson[FPSTR(fstring_DNS1)][1], STAjson[FPSTR(fstring_DNS1)][2], STAjson[FPSTR(fstring_DNS1)][3]);

            if (STAjson.containsKey(FPSTR(fstring_DNS2)))
            {
                set.STA.DNS2 = IPAddress(STAjson[FPSTR(fstring_DNS2)][0], STAjson[FPSTR(fstring_DNS2)][1], STAjson[FPSTR(fstring_DNS2)][2], STAjson[FPSTR(fstring_DNS2)][3]);
            }

            if (set.STA.IP == INADDR_NONE)
            {
                set.STA.dhcp = true;
                set.STA.hasConfig = false;
            }
            else
            {
                set.STA.dhcp = false;
                set.STA.hasConfig = true;
            }
        }
        else
        {
            set.STA.dhcp = true;
            set.STA.hasConfig = false;
        }
    }

    /*
           AP settings
     */
    if (root.containsKey(FPSTR(fstring_AP)))
    {

        JsonObject APjson = root[FPSTR(fstring_AP)];

        if (APjson.containsKey(FPSTR(fstring_enabled)))
        {
            set.AP.enabled = APjson[FPSTR(fstring_enabled)];
            //Serial.printf("set.AP.enabled = %s\n", (set.AP.enabled)? "true": "false");
        }

        if (APjson.containsKey(FPSTR(fstring_pass)))
        {
            //settings.hasPass = true;
            if (strnlen(APjson[FPSTR(fstring_pass)], 100) < MAX_PASS_LENGTH)
            {

                set.AP.pass = APjson[FPSTR(fstring_pass)].as<const char *>();
            }
        }

        if (APjson.containsKey(FPSTR(fstring_IP)))
        {
            set.STA.hasConfig = true;
            set.STA.IP = IPAddress(APjson[FPSTR(fstring_IP)][0], APjson[FPSTR(fstring_IP)][1], APjson[FPSTR(fstring_IP)][2], APjson[FPSTR(fstring_IP)][3]);
            set.STA.GW = IPAddress(APjson[FPSTR(fstring_GW)][0], APjson[FPSTR(fstring_GW)][1], APjson[FPSTR(fstring_GW)][2], APjson[FPSTR(fstring_GW)][3]);
            set.STA.SN = IPAddress(APjson[FPSTR(fstring_SN)][0], APjson[FPSTR(fstring_SN)][1], APjson[FPSTR(fstring_SN)][2], APjson[FPSTR(fstring_SN)][3]);
        }

        if (APjson.containsKey(FPSTR(fstring_visible)))
        {
            set.AP.visible = true;
        }

        if (APjson.containsKey(FPSTR(fstring_channel)))
        {
            set.AP.channel = APjson[FPSTR(fstring_channel)];
        }
    }

    if (settingsversion != SETTINGS_FILE_VERSION)
    {
        DEBUGESPMANAGERF("Settings File Version Wrong expecting v%u got v%u\n", SETTINGS_FILE_VERSION, settingsversion);
        // serializeJsonPretty(settingsJSON, Serial);
        // Serial.println();
        return WRONG_SETTINGS_FILE_VERSION;
    }

    return SUCCESS;
}

ESPMAN_ERR_t ESPmanager::_saveAllSettings(settings_t &set)
{

    DynamicJsonDocument jsonBuffer(allocateJSON());
    JsonObject root = jsonBuffer.to<JsonObject>();

    /*
            General Settings
     */
    JsonObject settingsJSON = root.createNestedObject(FPSTR(fstring_General));

    settingsJSON[FPSTR(fstring_mDNS)] = set.GEN.mDNSenabled;

    settingsJSON[FPSTR(fstring_settingsversion)] = SETTINGS_FILE_VERSION;

    if (set.GEN.host)
    {
        settingsJSON[FPSTR(fstring_host)] = set.GEN.host;
    }

    if (set.GEN.updateURL)
    {
        settingsJSON[FPSTR(fstring_updateURL)] = set.GEN.updateURL;
    }

    settingsJSON[FPSTR(fstring_updateFreq)] = set.GEN.updateFreq;

    if (set.GEN.OTApassword)
    {
        settingsJSON[FPSTR(fstring_OTApassword)] = set.GEN.OTApassword;
    }

    settingsJSON[FPSTR(fstring_OTAupload)] = set.GEN.OTAupload;

    /*****************************************
            STA Settings
    *****************************************/

    JsonObject STAjson = root.createNestedObject(FPSTR(fstring_STA));

    STAjson[FPSTR(fstring_enabled)] = set.STA.enabled;

    if (set.STA.ssid)
    {
        STAjson[FPSTR(fstring_ssid)] = set.STA.ssid;
    }

    if (set.STA.pass)
    {
        STAjson[FPSTR(fstring_pass)] = set.STA.pass;
    }

    if (set.STA.hasConfig)
    {

        JsonArray IP = STAjson.createNestedArray(FPSTR(fstring_IP));
        IP.add(set.STA.IP[0]);
        IP.add(set.STA.IP[1]);
        IP.add(set.STA.IP[2]);
        IP.add(set.STA.IP[3]);
        JsonArray GW = STAjson.createNestedArray(FPSTR(fstring_GW));
        GW.add(set.STA.GW[0]);
        GW.add(set.STA.GW[1]);
        GW.add(set.STA.GW[2]);
        GW.add(set.STA.GW[3]);
        JsonArray SN = STAjson.createNestedArray(FPSTR(fstring_SN));
        SN.add(set.STA.SN[0]);
        SN.add(set.STA.SN[1]);
        SN.add(set.STA.SN[2]);
        SN.add(set.STA.SN[3]);
        JsonArray DNS1 = STAjson.createNestedArray(FPSTR(fstring_DNS1));
        DNS1.add(set.STA.DNS1[0]);
        DNS1.add(set.STA.DNS1[1]);
        DNS1.add(set.STA.DNS1[2]);
        DNS1.add(set.STA.DNS1[3]);

        if (set.STA.DNS2 != INADDR_NONE)
        {
            JsonArray DNS2 = STAjson.createNestedArray(FPSTR(fstring_DNS2));
            DNS2.add(set.STA.DNS2[0]);
            DNS2.add(set.STA.DNS2[1]);
            DNS2.add(set.STA.DNS2[2]);
            DNS2.add(set.STA.DNS2[3]);
        }
    }

    /****************************************
            AP Settings
    ****************************************/

    JsonObject APjson = root.createNestedObject(FPSTR(fstring_AP));

    APjson[FPSTR(fstring_enabled)] = set.AP.enabled;

    if (set.AP.pass)
    {
        APjson[FPSTR(fstring_pass)] = set.AP.pass;
    }

    if (set.AP.hasConfig)
    {

        JsonArray IP = APjson.createNestedArray(FPSTR(fstring_IP));
        IP.add(set.AP.IP[0]);
        IP.add(set.AP.IP[1]);
        IP.add(set.AP.IP[2]);
        IP.add(set.AP.IP[3]);
        JsonArray GW = APjson.createNestedArray(FPSTR(fstring_GW));
        GW.add(set.AP.GW[0]);
        GW.add(set.AP.GW[1]);
        GW.add(set.AP.GW[2]);
        GW.add(set.AP.GW[3]);
        JsonArray SN = APjson.createNestedArray(FPSTR(fstring_SN));
        SN.add(set.AP.SN[0]);
        SN.add(set.AP.SN[1]);
        SN.add(set.AP.SN[2]);
        SN.add(set.AP.SN[3]);
    }

    APjson[FPSTR(fstring_visible)] = set.AP.visible;
    APjson[FPSTR(fstring_channel)] = set.AP.channel;

    File f = _fs.open(SETTINGS_FILE, "w");

    if (!f)
    {
        return FS_FILE_OPEN_ERROR;
    }

    //root.prettyPrintTo(f);
    serializeJsonPretty(root, f);
    f.close();
    return SUCCESS;
}

#ifdef DEBUGESPMANAGER

void ESPmanager::_dumpGEN(settings_t::GEN_t &settings)
{

    DEBUGESPMANAGERFRAW("---- GEN ----\n");
    DEBUGESPMANAGERFRAW("host = %s\n", (settings.host) ? settings.host.c_str() : "null");
    DEBUGESPMANAGERFRAW("updateURL = %s\n", (settings.updateURL) ? settings.updateURL.c_str() : "null");
    DEBUGESPMANAGERFRAW("updateFreq = %u\n", (uint32_t)settings.updateFreq);
    DEBUGESPMANAGERFRAW("OTAport = %u\n", (uint32_t)settings.OTAport);
    DEBUGESPMANAGERFRAW("mDNSenabled = %s\n", (settings.mDNSenabled) ? "true" : "false");

    DEBUGESPMANAGERFRAW("OTApassword = %s\n", (settings.OTApassword.c_str()) ? settings.OTApassword.c_str() : "null");
    DEBUGESPMANAGERFRAW("IDEupload = %s\n", (settings.OTAupload) ? "true" : "false");
}

void ESPmanager::_dumpAP(settings_t::AP_t &settings)
{

    DEBUGESPMANAGERFRAW("---- AP ----\n");
    DEBUGESPMANAGERFRAW("enabled = %s\n", (settings.enabled) ? "true" : "false");
    DEBUGESPMANAGERFRAW("ssid = %s\n", (settings.ssid) ? settings.ssid.c_str() : "null");
    DEBUGESPMANAGERFRAW("pass = %s\n", (settings.pass) ? settings.pass.c_str() : "null");
    DEBUGESPMANAGERFRAW("hasConfig = %s\n", (settings.hasConfig) ? "true" : "false");
    DEBUGESPMANAGERFRAW("IP = %s\n", settings.IP.toString().c_str());
    DEBUGESPMANAGERFRAW("GW = %s\n", settings.GW.toString().c_str());
    DEBUGESPMANAGERFRAW("SN = %s\n", settings.SN.toString().c_str());
    DEBUGESPMANAGERFRAW("visible = %s\n", (settings.visible) ? "true" : "false");
    DEBUGESPMANAGERFRAW("channel = %u\n", settings.channel);
}

void ESPmanager::_dumpSTA(settings_t::STA_t &settings)
{

    DEBUGESPMANAGERFRAW("---- STA ----\n");
    DEBUGESPMANAGERFRAW("enabled = %s\n", (settings.enabled) ? "true" : "false");
    DEBUGESPMANAGERFRAW("ssid = %s\n", (settings.ssid.c_str()) ? settings.ssid.c_str() : "null");
    DEBUGESPMANAGERFRAW("pass = %s\n", (settings.pass.c_str()) ? settings.pass.c_str() : "null");
    DEBUGESPMANAGERFRAW("dhcp = %s\n", (settings.dhcp) ? "true" : "false");
    DEBUGESPMANAGERFRAW("hasConfig = %s\n", (settings.hasConfig) ? "true" : "false");
    DEBUGESPMANAGERFRAW("IP = %s\n", settings.IP.toString().c_str());
    DEBUGESPMANAGERFRAW("GW = %s\n", settings.GW.toString().c_str());
    DEBUGESPMANAGERFRAW("SN = %s\n", settings.SN.toString().c_str());
    DEBUGESPMANAGERFRAW("DNS1 = %s\n", settings.DNS1.toString().c_str());
    DEBUGESPMANAGERFRAW("DNS2 = %s\n", settings.DNS2.toString().c_str());
}

void ESPmanager::_dumpSettings()
{
    _getAllSettings();

    if (_settings)
    {
        DEBUGESPMANAGERFRAW(" IP Addr %u.%u.%u.%u\n", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
        DEBUGESPMANAGERFRAW("---- Settings ----\n");
        DEBUGESPMANAGERFRAW("configured = %s\n", (_settings->configured) ? "true" : "false");
        DEBUGESPMANAGERFRAW("changed = %s\n", (_settings->changed) ? "true" : "false");

        _dumpGEN(_settings->GEN);
        _dumpSTA(_settings->STA);
        _dumpAP(_settings->AP);
    }
}

#endif

/**
 *  Resets the ESP to a non-configured state.
 *  Erases the config file. 
 */
void ESPmanager::factoryReset()
{
    DEBUGESPMANAGERF("FACTORY RESET\n");
    WiFi.disconnect();
    ESP.eraseConfig();
    _fs.remove(SETTINGS_FILE);
}

void ESPmanager::_sendTextResponse(AsyncWebServerRequest *request, uint16_t code, const String &text)
{
    AsyncWebServerResponse *response = request->beginResponse(code, "text/plain", text.c_str());
    //response->addHeader(FPSTR(ESPMAN::fstring_CORS), "*");
    response->addHeader(FPSTR(ESPMAN::fstring_CACHE_CONTROL), "no-store");
    request->send(response);
}

void ESPmanager::_populateFoundDevices(JsonObject &root)
{

#ifdef ESPMANAGER_DEVICEFINDER

    if (_devicefinder)
    {

        String host = getHostname();

        root[F("founddevices")] = _devicefinder->count();

        if (_devicefinder->count())
        {
            JsonArray devicelist = root.createNestedArray(F("devices"));
            JsonObject listitem = devicelist.createNestedObject();
            listitem[F("name")] = host;
            listitem[F("IP")] = WiFi.localIP().toString();
            for (uint8_t i = 0; i < _devicefinder->count(); i++)
            {
                JsonObject listitem = devicelist.createNestedObject();
                const char *name = _devicefinder->getName(i);
                IPAddress IP = _devicefinder->getIP(i);
                listitem[F("name")] = name;
                listitem[F("IP")] = IP.toString();
                //DEBUGESPMANAGERF("Found [%s]@ %s\n", name, IP.toString().c_str());
            }
        }
        else
        {
            DEBUGESPMANAGERF("No Devices Found\n");
        }
    }

#endif
}

void ESPmanager::FSDirIterator(FS &fs, const char *dirName, std::function<void(File &f)> Cb)
{
    Dir dir = fs.openDir(dirName);
    while (dir.next())
    {
        if (dir.isFile())
        {
            File f = dir.openFile("r");
            if (Cb)
            {
                Cb(f);
            }
        }
        else
        {
            FSDirIterator(fs, dir.fileName().c_str(), Cb);
        }
    }
}

bool ESPmanager::_convertMD5StringtoArray(const String &in, uint8_t *out) const
{
    if (in.length() != 32)
    {
        return false;
    }
    for (uint i = 0; i < 16; i++)
    {
        char str[3]{0};
        char **ptr{nullptr};
        str[0] = in[2 * i];
        str[1] = in[(2 * i) + 1];
        out[i] = strtol(str, ptr, 16);
    }
    return true;
}

bool ESPmanager::_RebootOnly()
{
    uint8_t cur[16]{0};
    _convertMD5StringtoArray(ESP.getSketchMD5(), cur);
    DEBUGESPMANAGERF("Current Sketch MD5  : %s\n", ESP.getSketchMD5().c_str());
    uint8_t *sto = _rtc[0].sketchMD5;
    DEBUGESPMANAGERF("HEX ARRAY Stored MD5: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                     sto[0], sto[1], sto[2], sto[3], sto[4], sto[5], sto[6], sto[7],
                     sto[8], sto[9], sto[10], sto[11], sto[12], sto[13], sto[14], sto[15]);

    for (uint8_t i = 0; i < 16; i++)
    {
        if (cur[i] != sto[i])
        {
            return false;
        }
    }
    return true;
}

// https://github.com/esp8266/Arduino/blob/273f4000f0dcb936e457cba3e71d824a7dfb9007/libraries/ESP8266WiFi/src/ESP8266WiFiType.h#L107

void ESPmanager::_onWiFiConnected(const WiFiEventStationModeConnected &data)
{
    DEBUGESPMANAGERF("WiFi STA connected\n");
}

void ESPmanager::_onWiFgotIP(const WiFiEventStationModeGotIP &data)
{
    DEBUGESPMANAGERF("WiFi gotIP %s\n", data.ip.toString().c_str());
    _hasWifiEverBeenConnected = true;
    _disconnectCounter = 0;
    //  only deactivate AP if 1) emergency mode was activated.. and 2) no one is connected to it..
    if (_emergencyModeActivated && !WiFi.softAPgetStationNum())
    {
        DEBUGESPMANAGERF("Emergency AP deactivated\n");
        WiFi.enableAP(false);
        _emergencyModeActivated = false;
        WiFi.setAutoReconnect(true);
    }
}

/*
    WiFi behavious
    - If after boot with no connection to STA and no active AP it makes an emergency AP to prevent lock out
    - If STA previously connected during this boot and now not available it tries to reconnect every second, unless AP is also
        active in which case it slows down the reconnects after 10 seconds to allow the AP to function. 
    - 



*/
void ESPmanager::_onWiFiDisconnected(const WiFiEventStationModeDisconnected &data)
{
    static uint32_t time = millis();
    static Task *task = nullptr;
    DEBUGESPMANAGERF("WiFi STA disconnected @[%05us]  since: [%05us]\n", millis(), (millis() - time) / 1000);

    if (time && millis() - time < 10000)
    {
        return;
    }
    else if (!time)
    {
        time = millis();
        return;
    }

    if (!_hasWifiEverBeenConnected && !_emergencyModeActivated && !WiFi.softAPIP().isSet())
    {
        DEBUGESPMANAGERF("Enabling emergency AP - No WiFi since boot\n");
        _emergencyMode(true);
        _emergencyModeActivated = true;
        WiFi.enableSTA(false);
    }

    if (!task && WiFi.softAPIP().isSet())
    {
        DEBUGESPMANAGERF("Disabling reconnect: slowing down reconnection attempts to 60s\n");
        WiFi.setAutoReconnect(false);
        _getAllSettings();
        if (_settings && _settings->STA.enabled)
        {
            String ap = _settings->STA.ssid;
            String psk = _settings->STA.pass;
            task = &_tasker.add([ap, psk](Task &t) {
                               if (!WiFi.localIP().isSet())
                               {
                                   DEBUGESPMANAGERF("Calling WiFi.enableSTA(true) with ssid=%s, psk=%s\n", ap.c_str(), psk.c_str());
                                   WiFi.enableSTA(true);
                                   WiFi.begin(ap, psk);
                               }
                               else
                               {
                                   t.setRepeat(false).setDelete(true);
                                   task = nullptr;
                                   WiFi.setAutoReconnect(true);
                                   time = 0;
                                   DEBUGESPMANAGERF("Wifi Reconnect - reenable reconnect as connected\n");
                               }
                           })
                        .setRepeat(true)
                        .setTimeout(60000)
                        .setName("WiFi reconnect Task")
                        .onEnd([]() {
                            DEBUGESPMANAGERF("Wifi Reconnect Slow task ended\n");
                        });
        }
    }
}

void ESPmanager::_initOTA()
{
    using namespace std::placeholders;
    if (!_settings)
    {
        return;
    }
    DEBUGESPMANAGERF("Enabling OTA....\n");
    _tasker.add([](Task &t) {
               ArduinoOTA.handle();
           })
        .setRepeat(true)
        .setTimeout(100)
        .setName("OTA");
    ArduinoOTA.setHostname(_settings->GEN.host.c_str());
    ArduinoOTA.setPort(_settings->GEN.OTAport);
    if (_settings->GEN.OTApassword.length())
    {
        DEBUGESPMANAGERF("OTApassword: %s\n", _settings->GEN.OTApassword.c_str());
        ArduinoOTA.setPassword(_settings->GEN.OTApassword.c_str());
    } else {
        DEBUGESPMANAGERF("OTApassword not set\n");
    }

    ArduinoOTA.onStart(std::bind(&ESPmanager::_OTAonStart, this));
    ArduinoOTA.onProgress(std::bind(&ESPmanager::_OTAonProgress, this, _1, _2));
    ArduinoOTA.onEnd(std::bind(&ESPmanager::_OTAonEnd, this));
    ArduinoOTA.onError(std::bind(&ESPmanager::_OTAonError, this, _1));

    ArduinoOTA.begin(); 
}

void ESPmanager::_OTAonStart()
{
    if (ArduinoOTA.getCommand() == U_FS)
    {
        _fs.end();
    }

    _rtc.erase();

    event_send(F("update"), F("begin"));

    delay(10); 

#ifdef DEBUGESPMANAGER
    DEBUGESPMANAGER.print(F("[              Performing OTA Upgrade              ]\n["));
    //                     ("[--------------------------------------------------]\n ");
#endif
}

void ESPmanager::_OTAonProgress(unsigned int progress, unsigned int total)
{
    static uint8_t done = 0;
    uint8_t percent = (progress / (total / 100));
    if (percent % 2 == 0 && percent != done)
    {
#ifdef DEBUGESPMANAGER
        DEBUGESPMANAGER.print("-");
#endif
        event_send(F("update"), String(percent));
        done = percent;
    }
}

void ESPmanager::_OTAonEnd()
{
    event_send(F("update"), F("end"));
    delay(100);
    _events.close();
    delay(1000);
#ifdef DEBUGESPMANAGER
    DEBUGESPMANAGER.println(F("]\nOTA End"));
    ESP.restart();
#endif
}

void ESPmanager::_OTAonError(ota_error_t error)
{
    StreamString ss;
    Update.printError(ss);

    if (error)
    {
        event_send(FPSTR(fstring_UPDATE), ss);
        DEBUGESPMANAGERF("Updater err:%s\n", ss.c_str());
        delay(100);
    }
}

void ESPmanager::_initHTTP()
{
    using namespace std::placeholders;

    //_HTTP.rewrite("/espman/images/ajax-loader.gif", "/espman/ajax-loader.gif");
    _HTTP.on("/espman/data.esp", std::bind(&ESPmanager::_HandleDataRequest, this, _1));
    _HTTP.on("/espman/quick", std::bind(&ESPmanager::_HandleQuickSTAsetup, this, _1)).setAuthentication("admin", "esprocks");
    _HTTP.rewrite("/espman", "/espman/index.htm");
    _HTTP.rewrite("/espman/", "/espman/index.htm");

    _WebHandler = &_HTTP.on("/espman/index.htm", [](AsyncWebServerRequest *request) { 
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", http_file_htm_gz, http_file_htm_gz_len); 
        if (response) {
            response->addHeader( F("Content-Encoding"), F("gzip")); 
            request->send(response); 
        } else {
            request->send(500); 
        }
    }).setAuthentication("admin", "esprocks");
    //.addInterestingHeader("blah");
    //.setLastModified("Mon, 20 Jun 2016 14:00:00 GMT");


    _HTTP.on(
        "/espman/upload", HTTP_POST, [this](AsyncWebServerRequest *request) {
            request->send(200);
        },
        std::bind(&ESPmanager::_handleFileUpload, this, _1, _2, _3, _4, _5, _6));

    //_WebHandler = &_HTTP.serveStatic("/espman/", _fs, "/espman/").setDefaultFile("index.htm").setLastModified(__DATE__ " " __TIME__ " GMT").setAuthentication("admin", "esprocks");
    //_HTTP.addHandler(&_staticHandlerInstance);
    _HTTP.addHandler(&_events);

    _events.onConnect([](AsyncEventSourceClient *client) {
        client->send(NULL, NULL, 0, 1000);
    });

    // _HTTP.on("/*.jq1.11.1.js", [](AsyncWebServerRequest *request) { request->redirect("http://code.jquery.com/jquery-1.11.1.min.js"); });
    // _HTTP.on("/*.jqm1.4.5.js", [](AsyncWebServerRequest *request) { request->redirect("http://code.jquery.com/mobile/1.4.5/jquery.mobile-1.4.5.min.js"); });
    // _HTTP.on("/*.jqm1.4.5.css", [](AsyncWebServerRequest *request) { request->redirect("http://code.jquery.com/mobile/1.4.5/jquery.mobile-1.4.5.min.css"); });



    //  /*                      */     need to test these

    _HTTP.onNotFound([](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_OPTIONS)
        {
            DEBUGESPMANAGERF("HTTP_OPTIONS sent 200\n");
            request->send(200);
        }
        else
        {
            DEBUGESPMANAGERF("HTTP_OPTIONS sent 404\n");
            request->send(404);
        }
    });

   // DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

#ifdef ESPMANAGER_UPDATER
    _HTTP.on("/espman/update", std::bind(&ESPmanager::_HandleSketchUpdate, this, _1));
#endif
}

void ESPmanager::_initAutoDiscover()
{
#ifdef ESPMANAGER_DEVICEFINDER

    if (_devicefinder)
    {
        delete _devicefinder;
    }

    _devicefinder = new ESPdeviceFinder;

    if (_devicefinder)
    {

        String host = getHostname();
        _devicefinder->cacheResults(false);
        _devicefinder->setAppName("ESPmanager");
        _devicefinder->begin(host.c_str(), _ESPdeviceFinderPort);

        Task *repeat_task = &_tasker.add([this](Task &t) {
                                        if (_devicefinder)
                                        {
                                            _devicefinder->loop();
                                        }
                                    })
                                 .setTimeout(10)
                                 .setRepeat(true)
                                 .setMicros(true);

        _tasker.add([repeat_task](Task &t) {
            repeat_task->setTimeout(1000);
        });
    }

#endif
}

void ESPmanager::_deleteSettingsTask(Task &task)
{
    if (_settings && !_settings->changed)
    {
        if (millis() - _settings->start_time > SETTINGS_MEMORY_TIMEOUT)
        {
            uint32_t __attribute__((unused)) startheap = ESP.getFreeHeap();
            delete _settings;
            _settings = nullptr;
            DEBUGESPMANAGERF("Deleting Settings.  Heap freed = %u (%u)\n", ESP.getFreeHeap() - startheap, ESP.getFreeHeap());
        }
    }
}

//  192.168.1.119/espman/quick?ssid=fyffest&pass=wellcometrust
//  Process a GET request to set the SSID and PSK for the device..
// only allow these when in _emergencymode... or maybe with FS corruption..
//  AsyncWebServerResponse *beginResponse_P(int code, const String& contentType, PGM_P content, AwsTemplateProcessor callback=nullptr);

void ESPmanager::_HandleQuickSTAsetup(AsyncWebServerRequest *request)
{

    // #if defined(DEBUGESPMANAGER)
    //     int params = request->params();
    //     for (int i = 0; i < params; i++)
    //     {
    //         AsyncWebParameter *p = request->getParam(i);
    //         if (p->isFile())
    //         { //p->isPost() is also true
    //             DEBUGESPMANAGER.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
    //         }
    //         else if (p->isPost())
    //         {
    //             DEBUGESPMANAGER.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    //         }
    //         else
    //         {
    //             DEBUGESPMANAGER.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
    //         }
    //     }
    // #endif

    const bool onAP = ON_AP_FILTER(request);

    if (!onAP && !_emergencyModeActivated)
    {
        DEBUGESPMANAGERF("Request Denied: not via AP or _emergency mode\n");
        request->send(403);
        return;
    }

    if (request->params() > 0)
    {

        String ssid;
        String pass;
        if (request->hasParam(F("sta"), true))
        {
            ssid = request->getParam(F("sta"), true)->value();
        }

        if (request->hasParam(F("pass"), true))
        {
            pass = request->getParam(F("pass"), true)->value();
        }

        if (ssid.length())
        {
            DEBUGESPMANAGERF("configuring STA using ssid=%s, pass=%s\n", ssid.c_str(), pass.c_str());
            WiFi.begin(ssid, pass);
        }

        if (request->hasParam(F("save"), true))
        {

            _getAllSettings();

            if (_settings)
            {
                _settings->STA.ssid = WiFi.SSID();
                _settings->STA.pass = WiFi.psk();
                _settings->STA.enabled = true;
                _settings->changed = true;

                if (_saveAllSettings(*_settings) == SUCCESS)
                {
                    DEBUGESPMANAGERF("Saved ssid=%s, pass=%s\n", _settings->STA.ssid.c_str(), _settings->STA.pass.c_str());
                    String reply = "saved new IP = " + WiFi.localIP().toString();
                    request->send(200, "text/plain", reply);
                    return;
                }
            }

            request->send(500, "text/plain", "error");
            return;
        }
    }
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", quick_setup_htm, quick_setup_htm_len, [](const String &var) {
        if (var == "IP")
        {
            return WiFi.localIP().toString();
        }
        else if (var == "STA")
        {
            return WiFi.SSID();
        }
        return String();
    });
    request->send(response);
}

//  sets the password on the GUI.  MUST be called after begin();
bool ESPmanager::setAuthentication(const String &username, const String &password)
{
    if (!_WebHandler)
    {
        return false;
    }
    else
    {
        _WebHandler->setAuthentication(username.c_str(), password.c_str());
        return true;
    }
}