/** @file
    @brief ESPmanager implementations.
*/

#include "ESPmanager.h"

#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <flash_utils.h>

#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <MD5Builder.h>
#include <AsyncJson.h>
#include <MD5Builder.h>
#include <umm_malloc/umm_malloc.h>
#include <time.h>
#include <stdio.h>
#include <WebAuthentication.h>
#include <Hash.h>
#include <list>
#include "Tasker/src/Tasker.h"
#include "FlashWriter.h"
#include "StreamString.h"

#include "staticHandler/src/staticHandler.h"
#include "staticHandler/src/jqueryStatic.h"

#include "pgmspace.h"

extern "C" {
#include "user_interface.h"
}

extern UMM_HEAP_INFO ummHeapInfo;

static const char _compile_date_time[] = __DATE__ " " __TIME__;
static const uint16_t _ESPdeviceFinderPort = 8888;
static const uint32_t _ESPdeviceTimeout = 1200000;// 300000;  //  when is the devicefinder deleted.



//#define LAST_MODIFIED_DATE "Mon, 20 Jun 2016 14:00:00 GMT"

// Stringifying the BUILD_TAG parameter
// #define TEXTIFY(A) #A
// #define ESCAPEQUOTE(A) TEXTIFY(A)

// //String buildTag = ESCAPEQUOTE(BUILD_TAG);
// String commitTag = ESCAPEQUOTE(TRAVIS_COMMIT);

// #ifndef BUILD_TAG
// #define BUILD_TAG Not Set
// #endif
// #ifndef COMMIT_TAG
// #define COMMIT_TAG Not Set
// #endif
// #ifndef BRANCH_TAG
// #define BRANCH_TAG Not Set
// #endif
// #ifndef SLUG_TAG
// #define SLUG_TAG Not Set
// #endif
//
// const char * buildTag = ESCAPEQUOTE(BUILD_TAG);
// const char * commitTag = ESCAPEQUOTE(COMMIT_TAG);
// const char * branchTag = ESCAPEQUOTE(BRANCH_TAG);

// const char * slugTag = ESCAPEQUOTE(SLUG_TAG);

#if defined(DEBUGESPMANAGER)
extern File _DebugFile;
#endif


#ifndef ESPMANAGER_GIT_TAG
#define ESPMANAGER_GIT_TAG "NOT DEFINED"
#endif

uint32_t jsonSize = 4096; 
//  move this out of global...  
uint32_t allocateJSON()
{
    uint32_t value = ESP.getMaxFreeBlockSize() - 512; 
    //Serial.printf("Json: Max Free Block = %u\n", value);
    if (value > jsonSize) {
        return jsonSize;
    } else {
        return value; 
    }
    
}

/**
 *
 * @param [HTTP] pass an instance of AsyncWebServer. Optional.
 * @param [fs] pass an instance of FS file system.  Defaults to FS, as per arduino. Optional.
 *
 */
ESPmanager::ESPmanager(
    AsyncWebServer & HTTP, FS & fs)
    : _HTTP(HTTP)
    , _fs(fs)
    , _events("/espman/events")
{
    //  These need to be added here to force linking...
#ifdef USESTATICPROGMEMJQUERY
    _staticHandlerInstance.add(JQUERYdata::JQdata); 
    _staticHandlerInstance.add(JQUERYdata::JQMdata); 
    _staticHandlerInstance.add(JQUERYdata::JQMCSSdata); 
#endif
    _staticHandlerInstance.enableRedirect() = false; 
}


ESPmanager::~ESPmanager()
{

    if (_settings) {
        delete _settings;
    }

#ifdef ESPMANAGER_SYSLOG
    // if (_syslogDeviceName) {
    //     free( (char*)_syslogDeviceName);
    //     _syslogDeviceName = nullptr;
    // }
#endif


}

/**
 * To be called during setup() and only called once.
 * @return ESPMAN::ESPMAN_ERR_t.
 */
ESPMAN_ERR_t ESPmanager::begin()
{

    using namespace std::placeholders;
    using namespace ESPMAN;


    bool wizard = false;

    DEBUGESPMANAGERF("Settings Manager V" ESPMANVERSION "\n");
    // DEBUGESPMANAGERF("REPO: %s\n",  slugTag );
    // DEBUGESPMANAGERF("BRANCH: %s\n",  branchTag );
    // DEBUGESPMANAGERF("BuildTag: %s\n",  buildTag );
    // DEBUGESPMANAGERF("commitTag: %s\n",  commitTag );
    //
    DEBUGESPMANAGERF("True Sketch Size: %u\n",  trueSketchSize() );
    DEBUGESPMANAGERF("Sketch MD5: %s\n",  getSketchMD5().c_str() );
    DEBUGESPMANAGERF("Device MAC: %s\n", WiFi.macAddress().c_str() );

    wifi_set_sleep_type(NONE_SLEEP_T); // workaround no modem sleep.

    if (_appName.length() == 0) {
        _appName = F("ESPManager");
    }

    if (!_fs.begin()) {
        return ERROR_FS_MOUNT;
    }

#ifdef DEBUGESPMANAGER
    _DebugFile  = _fs.open("/debugFile.txt", "a+");

    _DebugFile.println("\n --------            RESTARTED DEVICE         -------- \n\n");


    _DebugFile.println(ESP.getResetReason());
    _DebugFile.println(ESP.getResetInfo());

    _tasker.add( [this](Task & t) {

        if (_DebugFile.size() > 50000) {
            _DebugFile.close();
            if (_fs.exists("/debugFile.old.txt")) {
                _fs.remove("/debugFile.old.txt");
            }
            _fs.rename("/debugFile.txt", "/debugFile.old.txt");
            _DebugFile  = _fs.open("/debugFile.txt", "a+");

        }
    }).setTimeout(60000).setRepeat(true);

    DEBUGESPMANAGER.println(F("FS FILES:"));
    {
        FSDirIterator(_fs, "/", [](File & f){ 
            DEBUGESPMANAGER.printf_P(PSTR(" File: %-35s [%8uB]\n"), f.fullName(), f.size() );
        });
    }

    File f = _fs.open(SETTINGS_FILE, "r");
    DEBUGESPMANAGER.printf_P(PSTR("ESP MANAGER Settings [%u]B:\n"), f.size());
    if (f) {
        DEBUGESPMANAGER.write(f);
        // for (int i = 0; i < f.size(); i++) {
        //     DEBUGESPMANAGER.write(f.read());
        // }
        DEBUGESPMANAGER.println();
        f.close();
    }

#endif

    String old_settings_name = F("/espman/settings.txt");

    if (!_fs.exists(SETTINGS_FILE) && _fs.exists(old_settings_name.c_str())) {
        if (_fs.rename(old_settings_name.c_str(), SETTINGS_FILE)) {
            DEBUGESPMANAGERF("Settings file renamed to %s\n", SETTINGS_FILE);
        }
    }

    int getallERROR = _getAllSettings();
    DEBUGESPMANAGERF("= %i \n", getallERROR);

    if (!_settings) {
        DEBUGESPMANAGERF("Unable to MALLOC for settings. rebooting....\n");
        ESP.restart();
    }

    if (getallERROR == FS_FILE_OPEN_ERROR) {

        DEBUGESPMANAGERF("no settings file found\n");

        _settings->changed = true; //  give save button at first boot if no settings file

    }

    // if (getallERROR) {
    //     DEBUGESPMANAGERF("[ESPmanager::begin()] ERROR -> configured = false\n");
    //     settings->configured = false;
    // } else {
    //     settings->configured = true;
    // }

    if (!_settings->GEN.host) {
        DEBUGESPMANAGERF("Host NOT SET\n");
        char tmp[33] = {'\0'};
        snprintf(tmp, 32, "esp8266-%06x", ESP.getChipId());
        _settings->GEN.host = tmp;
    }
    //
    //
    // DEBUGESPMANAGERF("[ESPmanager::begin()] host = %s\n", _GEN_SETTINGS.host );
    //
    int AP_ERROR = 0;
    int STA_ERROR = 0;
    int AUTO_ERROR = 0;

    WiFi.hostname(_settings->GEN.host.c_str());


    if (_fs.exists("/.wizard")) {

        File f = _fs.open("/.wizard", "r");

        if (f && f.size() == sizeof(settings_t::AP_t)) {
            wizard = true;
            DEBUGESPMANAGERF("*** WIZARD MODE ENABLED ***\n");
            settings_t::AP_t ap;

            uint8_t * data = static_cast<uint8_t*>(static_cast<void*>(&ap));

            for (int i = 0; i < sizeof(ap); i++) {
                data[i] = f.read();
            }

            ap.enabled = true;
            ap.ssid = _settings->GEN.host;
            ap.channel = WiFi.channel();

            if (!ap.pass && _settings->STA.pass) {
                ap.pass = _settings->STA.pass;
            } else if (!ap.pass) {
                ap.pass = F(DEFAULT_AP_PASS);
            }

            if (!_initialiseAP(ap)) {
                if (_settings->GEN.portal) {
                    enablePortal();
                }
            }
        }

    } else if (_settings->configured) {



        DEBUGESPMANAGERF("settings->configured = true \n");
        AP_ERROR = _initialiseAP();
        DEBUGESPMANAGERF("_initialiseAP = %i \n", AP_ERROR);

        STA_ERROR = _initialiseSTA();

        DEBUGESPMANAGERF("_initialiseSTA = %i \n", STA_ERROR);

    } else {

        //  enable emergency mode and portal....

        ESP.eraseConfig(); //  clear everything when starting for first time...

        WiFi.mode(WIFI_OFF);

        _emergencyMode(true);

        if (_settings->GEN.portal) {
            enablePortal();
        } else {
            DEBUGESPMANAGERF("Portal DISABLED\n");

        }


    }

    if ( (STA_ERROR && STA_ERROR != STA_DISABLED) ||  (AP_ERROR == AP_DISABLED && STA_ERROR == STA_DISABLED ) ) {
        if (_ap_boot_mode != DISABLED) {
            _APenabledAtBoot = true;
            DEBUGESPMANAGERF("CONNECT FAILURE: emergency mode enabled for %i\n",  (int8_t)_ap_boot_mode * 60 * 1000 );
            _emergencyMode(true); // at boot if disconnected don't disable AP...
        }
    }


    if (_settings->GEN.OTAupload) {

        DEBUGESPMANAGERF("Enabling OTA....\n");

        _tasker.add([](Task & t) {
            ArduinoOTA.handle();
        }).setRepeat(true).setTimeout(100);

        ArduinoOTA.setHostname(_settings->GEN.host.c_str());
        //
        ArduinoOTA.setPort( _settings->GEN.OTAport);

        // if (set.GEN.OTApassword) {

        // }
        //
        if (_settings->GEN.OTApassword) {
            DEBUGESPMANAGERF("OTApassword: %s\n", _settings->GEN.OTApassword.c_str() );
            ArduinoOTA.setPassword( (const char *)_settings->GEN.OTApassword.c_str()  );


        }

        ArduinoOTA.onStart([this]() {
            //MDNS.stop();
            
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH) {
              type = "sketch";
            } else { // U_FS
              type = "filesystem";
              _fs.end();
            }

            // NOTE: if updating FS this would be the place to unmount FS using FS.end()
            //Serial.println("**** Start updating " + type);

            event_send( F("update") , F("begin"));
#ifdef DEBUGESPMANAGER
            DEBUGESPMANAGER.print(F(   "[              Performing OTA Upgrade              ]\n["));
//                                       ("[--------------------------------------------------]\n ");
#endif
        });
        ArduinoOTA.onEnd([this]() {
            //event_printf_P("update", PSTR("end"));
            event_send( F("update"), F("end"));

            delay(100);
            _events.close();
            delay(1000);
#ifdef DEBUGESPMANAGER
            DEBUGESPMANAGER.println(F("]\nOTA End"));
            ESP.restart();
#endif
        });


        ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
            static uint8_t done = 0;
            uint8_t percent = (progress / (total / 100) );
            if ( percent % 2 == 0  && percent != done ) {
#ifdef DEBUGESPMANAGER
                DEBUGESPMANAGER.print("-");
#endif
                //event_printf("update", "%u", percent);

 //**                event_send( F("update"), myStringf_P( PSTR("%u"), percent));
                //_events.send()

                done = percent;
            }
        });

        ArduinoOTA.onError([this](ota_error_t error) {

            using namespace ESPMAN;

#ifdef DEBUGESPMANAGER
            DEBUGESPMANAGER.printf("OTA Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) {  DEBUGESPMANAGER.println(F("Auth Failed")); }
            else if (error == OTA_BEGIN_ERROR) {  DEBUGESPMANAGER.println(F("Begin Failed")); }
            else if (error == OTA_CONNECT_ERROR) {  DEBUGESPMANAGER.println(F("Connect Failed")); }
            else if (error == OTA_RECEIVE_ERROR) {  DEBUGESPMANAGER.println(F("Receive Failed")); }
            else if (error == OTA_END_ERROR) {  DEBUGESPMANAGER.println(F("End Failed")); }
#endif

            StreamString ss;
            Update.printError(ss);
            
            if (error) {
                //event_printf(string_UPDATE, string_ERROR_toString, getError(error).c_str());
//**                 event_send( FPSTR(fstring_UPDATE), myStringf_P( fstring_ERROR_toString, getError(error).c_str()));
 //**                event_send( FPSTR(fstring_UPDATE), myStringf_P( fstring_ERROR_toString, ss.c_str()));
                DEBUGESPMANAGERF("Updater err:%s\n", ss.c_str()); 
                delay(100); 
            }
        });

        ArduinoOTA.begin();

    } else {
        DEBUGESPMANAGERF("OTA DISABLED\n");
    }

    if (_settings->GEN.mDNSenabled) {
        MDNS.addService("http", "tcp", 80);
    }

    _HTTP.rewrite("/espman/images/ajax-loader.gif", "/espman/ajax-loader.gif");
    _HTTP.rewrite("/espman/", "/espman/index.htm");
    _HTTP.rewrite("/espman", "/espman/index.htm");

    _HTTP.on("/espman/data.esp", std::bind(&ESPmanager::_HandleDataRequest, this, _1 ));

    //  kept to allow cached sites to refresh...
    _HTTP.on("/espman/site.appcache", HTTP_ANY, [](AsyncWebServerRequest * request) {
        request->send(200, "text/cache-manifest", "CACHE MANIFEST\nNETWORK:\n*\n\n");
    });

    _HTTP.on("/espman/upload", HTTP_POST, [this](AsyncWebServerRequest * request) {
        request->send(200);
    }, std::bind(&ESPmanager::_handleFileUpload, this, _1, _2, _3, _4, _5, _6)  );

    _HTTP.serveStatic("/espman/index.htm", _fs, "/espman/index.htm" ).setLastModified(__DATE__ " " __TIME__ " GMT");
    _HTTP.serveStatic("/espman/ajax-loader.gif", _fs, "/espman/ajax-loader.gif" ).setLastModified(__DATE__ " " __TIME__ " GMT");
    _HTTP.serveStatic("/espman/setup.htm", _fs, "/espman/setup.htm" ).setLastModified(__DATE__ " " __TIME__ " GMT");

    _HTTP.addHandler( &_staticHandlerInstance ); 





    // DEBUGESPMANAGERF("[staticHan] len = %u\n", staticHandleFileList.size()); 
    //     uint i = 0; 
    //     for (auto item : staticHandleFileList )
    //     {
    //         Serial.println( FPSTR(item->local));
    //     }


    _events.onConnect([](AsyncEventSourceClient * client) {
        client->send(NULL, NULL, 0, 1000);
    });

    _HTTP.addHandler(&_events);


#ifdef ESPMANAGER_UPDATER
    _HTTP.on("/espman/update", std::bind(&ESPmanager::_HandleSketchUpdate, this, _1 ));
#endif

    if (_settings->GEN.updateFreq) {

        _tasker.add([this](Task & t) {

            DEBUGESPMANAGERF("Performing update check\n");

            _getAllSettings();

            if (_settings) {
                _upgrade( _settings->GEN.updateURL.c_str() );
            }

        }).setRepeat(true).setTimeout(_settings->GEN.updateFreq * 60000);


    }

    /*

            Add delete _settings task.  deletes the settings held in memory...

    */


    _tasker.add( [this](Task & t) {

        if (_settings && !_settings->changed) {
            if (millis() - _settings->start_time > SETTINGS_MEMORY_TIMEOUT) {
                uint32_t startheap = ESP.getFreeHeap();
                delete _settings;
                _settings = nullptr;
                DEBUGESPMANAGERF("Deleting Settings.  Heap freed = %u (%u)\n", ESP.getFreeHeap() - startheap, ESP.getFreeHeap() );

            }
        }

    }).setTimeout(10000).setRepeat(true);


    _tasker.add( std::bind( &ESPmanager::_APlogic, this, _1 )).setRepeat(true).setTimeout(500);


    //  _tasker.add( [this](Task & t) {

    // //     ESP_LOG(LOG_DEBUG, "HELLO");
    // //     ESP_LOG(LOG_DEBUG, myStringf("HELLO %s", "sailor"));
    // //     ESP_LOG(LOG_DEBUG, myStringf_P( PSTR("HELLO %s from progmem"), "sailor"));
    //    static uint32_t num = 0;

    //  event_send( F("console") , myStringf_P( PSTR("test message %u"), num++));

    //  }).setRepeat(true).setTimeout(1000);


#if defined(DEBUGESPMANAGER)

    // if (WiFi.isConnected()) {

    //     configTime(0 * 3600, 0, "pool.ntp.org");

    //     DEBUGESPMANAGERF("Boot Time: ");

    //     uint32_t tc = millis();

    //     while (!time(nullptr)) {
    //         tc++;
    //         delay(100);
    //         if (millis() - tc > 30000) { break; }
    //     }

    //     time_t now = time(nullptr);

    //     DEBUGESPMANAGERF("%s", ctime(&now));
    //     //ESPMan_Debugln();

    // }

#endif


// #if defined(ESPMANAGER_SYSLOG)

//     if (_settings->GEN.usesyslog) {

//         DEBUGESPMANAGERF("Created syslog client\n");

//         _syslog = new SysLog( _settings->GEN.syslogIP, _settings->GEN.syslogPort, (_settings->GEN.syslogProto) ? SYSLOG_PROTO_BSD : SYSLOG_PROTO_IETF );  //SYSLOG_PROTO_BSD or SYSLOG_PROTO_IETF


//         if (_syslog) {
//             _syslog->setDeviceName( _settings->GEN.host ) ;
//             _syslog->log(LOG_INFO, F("Device Started"));

//             DEBUGESPMANAGERF("Address of syslog %p, ip = %s, port = %u, proto=%u, hostname =%s\n", _syslog, _settings->GEN.syslogIP.toString().c_str(), _settings->GEN.syslogPort , _settings->GEN.syslogProto, "ESPManager");

//         }
//     }

// #endif


//  autodiscover code
//
#ifdef ESPMANAGER_DEVICEFINDER

    _devicefinder = new ESPdeviceFinder;

    if (_devicefinder) {

        String host = getHostname();
        _devicefinder->cacheResults(false);
        _devicefinder->setAppName( "ESPmanager" );
        _devicefinder->begin(host.c_str(), _ESPdeviceFinderPort);

        Task * repeat_task = &_tasker.add( [this](Task & t) {

            if (_devicefinder) {
                _devicefinder->loop();
            }

        }).setTimeout(10).setRepeat(true).setMicros(true);

        _tasker.add([repeat_task](Task & t) {
            repeat_task->setTimeout(1000);
        });

    }

#endif


}

/**
 * Allows you to override and settings file, mainly for testing purposes as you can't use settings stored
 * in the config.json file.
 * @param [ssid] desired default ssid to connect to.  Can be `const char *`, `String`, `myString` or `F()`.
 * @param [pass] desired default password to connect to ssid.  Can be `const char *`, `String`, `myString` or `F()`.
 * @return ESPMAN::ESPMAN_ERR_t
 */
ESPMAN_ERR_t ESPmanager::begin(const String & ssid, const String & pass)
{
    DEBUGESPMANAGERF("ssid = %s, pass = %s\n", ssid.c_str() , pass.c_str() );

    if (!_fs.begin()) {
        return ERROR_FS_MOUNT;
    }

    _getAllSettings();

    if (_settings) {
        _settings->STA.ssid = ssid;
        _settings->STA.pass = pass;
        _settings->STA.enabled = true;
        _settings->configured = true;
        _settings->changed = true;
    } else {
        return SETTINGS_NOT_IN_MEMORY;
    }

    return begin();

}


void ESPmanager::_APlogic(Task & t)
{
    if ( _APtimer > 0 && !WiFi.softAPgetStationNum()) {

        int32_t time_total {0};

        if (_APenabledAtBoot) {
            time_total = (int8_t)_ap_boot_mode * 60 * 1000;
        } else {
            time_total = (int8_t)_no_sta_mode * 60 * 1000;
        }

        if (time_total > 0 && millis() - _APtimer > time_total ) {


            ESP.restart();    //  change behaviour to restart...  if still not connected then reboot and make an AP for set time... if STA connects then no problem...

        } else if (time_total > 0) {

#ifdef DEBUGESPMANAGER
            static uint32_t timeout_warn = 0;

            if (millis()  - timeout_warn > 10000) {
                timeout_warn = millis();
                DEBUGESPMANAGERF("Countdown to disabling AP %i of %i\n", (time_total -  (millis() - _APtimer) ) / 1000, time_total);
            }

#endif
        }
        // uint32_t timer = 0;
    }

    // triggered once when no timers.. and wifidisconnected
    if (!_APtimer && !_APtimer2 && WiFi.isConnected() == false) {
        //  if something is to be done... check  that action is not do nothing, or that AP is enabled, or action is reboot...
        if ( ( _no_sta_mode != NO_STA_NOTHING ) && ( WiFi.getMode() != WIFI_AP_STA || WiFi.getMode() != WIFI_AP || _no_sta_mode == NO_STA_REBOOT  ) ) {
            DEBUGESPMANAGERF("WiFi disconnected: starting AP Countdown\n" );
            _APtimer2 = millis();
        }
        //_ap_triggered = true;
    }

    //  only triggered once AP_start_delay has elapsed... and not reset...
    // this gives chance for a reconnect..
    if (_APtimer2 && !_APtimer && millis() - _APtimer2 > ESPMAN::AP_START_DELAY && !WiFi.isConnected()) {


        if (_no_sta_mode == NO_STA_REBOOT) {
            DEBUGESPMANAGERF("WiFi disconnected: REBOOTING\n" );
            ESP.restart();
        } else {
            DEBUGESPMANAGERF("WiFi disconnected: starting AP\n" );
            _emergencyMode();
        }

    }

    //  turn off only if these timers are enabled, but you are reconnected.. and settings have not changed...
    // this functions only work for a discconection and reconnection.. when settings have not changed...
    if ( (_APtimer2 || _APtimer ) && WiFi.isConnected() == true) {

        if ( !_settings || (_settings && !_settings->changed) && !WiFi.softAPgetStationNum() ) { // stops the AP being disabled if it is the result of changing stuff

            settings_t::AP_t APsettings;
            APsettings.enabled = false;
            _initialiseAP(APsettings);
            DEBUGESPMANAGERF("WiFi reconnected: disable AP\n" );
            //_ap_triggered = false;
            _APtimer = 0;
            _APtimer2 = 0;
            _APenabledAtBoot = false;
        }

    }
}

/**
 *  This function enables the captive portal, creating a DNS server that allows redirect.
 *  example:  To redirect root to page when portal is enabled.
 *  @code
 *  HTTP.rewrite("/", "/espman/setup.htm").setFilter( [](AsyncWebServerRequest * request) { return settings.portal(); });
 *  @endcode
 *  @return ESPMAN::ESPMAN_ERR_t
 */

ESPMAN_ERR_t ESPmanager::enablePortal()
{
    DEBUGESPMANAGERF("Enabling Portal\n");

    _dns = new DNSServer;
    //_portalreWrite = &_HTTP.rewrite("/", "/espman/setup.htm");

    IPAddress apIP(192, 168, 4, 1);

    if (_dns)   {
        /* Setup the DNS server redirecting all the domains to the apIP */
        _dns->setErrorReplyCode(DNSReplyCode::NoError);
        _dns->start(DNS_PORT, "*", apIP);
        DEBUGESPMANAGERF("Done\n");

        _dnsTask = & _tasker.add([this](Task & t) {
            this->_dns->processNextRequest();
        }).setRepeat(true).setTimeout(500).setMicros(true);

        return SUCCESS;

    } else {
        return MALLOC_FAIL;
    }

}

/**
 *  Disable the captive portal function.
 *
 */
void ESPmanager::disablePortal()
{
    DEBUGESPMANAGERF("Disabling Portal\n");

    if (_dns) {
        delete _dns;
        _dns = nullptr;
    }

    if (_dnsTask) {
        _tasker.remove(_dnsTask);
    }

    // if (_portalreWrite && _HTTP.removeRewrite(_portalreWrite))
    // {
    //     _portalreWrite = nullptr;
    // }

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
    if (bytes < 1024) {
        return String(bytes) + "B";
    } else if (bytes < (1024 * 1024)) {
        return String(bytes / 1024.0) + "KB";
    } else if (bytes < (1024 * 1024 * 1024)) {
        return String(bytes / 1024.0 / 1024.0) + "MB";
    } else {
        return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
    }
}

/**
 * Converts a String to a byte array.
 * @param [mac] `*uint8_t` to byte array to output to.
 * @param [input]  input String to convert
 * @return
 */
bool ESPmanager::StringtoMAC(uint8_t *mac, const String & input)
{

    char tempbuffer[input.length() + 1];
    urldecode(tempbuffer, input.c_str() );
    String decodedMAC = String(tempbuffer);
    String buf;
    uint8_t pos = 0;
    char tempbuf[5];
    bool remaining = true;

    do {
        buf = decodedMAC.substring(0, decodedMAC.indexOf(':'));
        remaining = (decodedMAC.indexOf(':') != -1) ? true : false;
        decodedMAC = decodedMAC.substring(decodedMAC.indexOf(':') + 1, decodedMAC.length());
        buf.toCharArray(tempbuf, buf.length() + 1);
        mac[pos] = (uint8_t)strtol (tempbuf, NULL, 16);
        //Serial.printf("position %u = %s ===>%u \n", pos, tempbuf, mac[pos]);
        pos++;
    } while (remaining);

    if (pos == 6) { return true; } else { return false; }

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
    if (dst == NULL) { return; }
    while (*src) {
        if ((*src == '%') &&
                ((a = src[1]) && (b = src[2])) &&
                (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') {
                a -= 'a' - 'A';
            }
            if (a >= 'A') {
                a -= ('A' - 10);
            } else {
                a -= '0';
            }
            if (b >= 'a') {
                b -= 'a' - 'A';
            }
            if (b >= 'A') {
                b -= ('A' - 10);
            } else {
                b -= '0';
            }
            *dst++ = 16 * a + b;
            src += 3;
        } else {
            c = *src++;
            if (c == '+') { c = ' '; }
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
String ESPmanager::file_md5 (File & f)
{
    // Md5 check

    if (!f) {
        return String();
    }

    if (f.seek(0, SeekSet)) {

        MD5Builder md5;
        md5.begin();
        md5.addStream(f, f.size());
        md5.calculate();
        return md5.toString();
    } else {
        DEBUGESPMANAGERF("Seek failed on file\n");
    }
}

/**

 * Templated functon to allow sending of a json to an AsyncWebServerRequest.
 * It adds the CORS header, and no-store to prevent caching.
 * Only works for json lengths under 4k.
 * @todo Add return bool so it does not fail silently.
 * @param [root] Either JsonObject or JsonObject
 * @param [request] AsyncWebServerRequest* to send the json to.
 */
template <class T> void ESPmanager::sendJsontoHTTP( const T & root, AsyncWebServerRequest *request)
{
    int len = measureJson(root);
    if (len < 4000) {

        AsyncResponseStream *response = request->beginResponseStream("text/json");

        if (response) {
            response->addHeader(  FPSTR( ESPMAN::fstring_CORS)  , "*");
            response->addHeader(  FPSTR(ESPMAN::fstring_CACHE_CONTROL) , "no-store");
            //root.printTo(*response);
            serializeJson(root, *response); 
            request->send(response);
        } else {
            //Serial.println("ERROR: No Stream Response");
        }


    } else {

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

    if (_settings) {
        set = *_settings;
    }

    int ERROR = _getAllSettings(set);

    DEBUGESPMANAGERF("error = %i\n", ERROR);

    if (!ERROR && set.GEN.host ) {
        return set.GEN.host;
    } else  {
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
    using namespace ESPMAN;

    if (path.length()) {
        DEBUGESPMANAGERF("Upgrade Called: path = %s\n", path.c_str());
    }

    _getAllSettings();

    String newpath;

    if (!_settings) {
        return CONFIG_FILE_ERROR;
    }

    if (path.length() == 0) {

        if (_settings->GEN.updateURL ) {
            newpath = _settings->GEN.updateURL;
        } else {
            char buf[10]{0};
            snprintf(buf, 20, "[%i]", NO_UPDATE_URL ); 
            event_send( FPSTR(fstring_UPGRADE), String(buf) );
            return NO_UPDATE_URL;
        }

    } else {
        newpath = path.c_str();
    }

    if (runasync) {
        _tasker.add([newpath, this](Task & t) {
            this->_upgrade(newpath.c_str() ) ;
        });
    } else {
        return _upgrade(newpath.c_str() ) ;
    }

    return SUCCESS;

}


#ifdef ESPMANAGER_UPDATER
ESPMAN_ERR_t ESPmanager::_upgrade(const char * path)
{
    using namespace ESPMAN;

    _getAllSettings();

    if (!_settings) {
        return CONFIG_FILE_ERROR;
    }

    if (!path || strlen(path) == 0 ) {

        if ( _settings->GEN.updateURL ) {
            path = _settings->GEN.updateURL.c_str();
        } else {

//**             event_send( FPSTR(fstring_UPGRADE), myStringf_P( PSTR("[%i]"), NO_UPDATE_URL  ));
            return NO_UPDATE_URL;
        }

    } else {
        DEBUGESPMANAGERF("Path sent in: %s\n", path);
    }

    int files_expected = 0;
    int file_count = 1;
    int firmwareIndex = -1;
    bool overwriteFiles = false;
    //DynamicJsonDocument json(ESP.getMaxFreeBlockSize() - 512); 
    DynamicJsonDocument json(allocateJSON()); 

//**     event_send( FPSTR(fstring_UPGRADE) , F("begin")) ;
    DEBUGESPMANAGERF("Checking for Updates: %s\n", path);
    String Spath = String(path);
    String rooturi = Spath.substring(0, Spath.lastIndexOf('/') );
 //**    event_send( FPSTR(fstring_CONSOLE) , myStringf("%s", path )) ;
    DEBUGESPMANAGERF("rooturi=%s\n", rooturi.c_str());

    //  save new update path for future update checks...  (if done via url only for example)
    if (_settings->GEN.updateURL ) {
        if (_settings->GEN.updateURL != path ) {
            _settings->GEN.updateURL = path;
            save_flag = true;
        }

    } else {
        _settings->GEN.updateURL = path;
        save_flag = true;
    }


    int ret = _parseUpdateJson(json, path);

    if (ret) {
        DEBUGESPMANAGERF("MANIFEST ERROR part 1 [%s]\n", getError(MANIFST_FILE_ERROR).c_str() );
        DEBUGESPMANAGERF("MANIFEST ERROR part 2 [%s]\n", getError(ret).c_str() );
//**         event_send( FPSTR(fstring_UPGRADE), myStringf_P( fstring_ERROR2_toString, getError(MANIFST_FILE_ERROR).c_str(), getError(ret).c_str()));
        DEBUGESPMANAGERF("MANIFEST ERROR [%i]\n", ret );
        return MANIFST_FILE_ERROR;
    }

    json.shrinkToFit(); 

    DEBUGESPMANAGERF("_parseUpdateJson success\n");

    if (!json.is<JsonObject>()) {
//**         event_send( FPSTR(fstring_UPGRADE), myStringf_P( fstring_ERROR_toString, getError(JSON_OBJECT_ERROR).c_str() ) );
        DEBUGESPMANAGERF("JSON ERROR [%i]\n", JSON_OBJECT_ERROR );
        return JSON_PARSE_ERROR;
    }

    JsonObject root = json.as<JsonObject>();

    /**
     *      Global settings for upgrade
     *
     */

    if (root.containsKey(F("formatSPIFFS"))) {
        if (root[F("formatSPIFFS")] == true) {
            DEBUGESPMANAGERF("Formatting FS");
            _getAllSettings();
            _fs.format();
            if (_settings) {
                _saveAllSettings(*_settings);
            }

            DEBUGESPMANAGERF("done\n ");
        }
    }

    if (root.containsKey(F("clearWiFi"))) {
        if (root[F("clearWiFi")] == true) {
            DEBUGESPMANAGERF("Erasing WiFi Config ....");
            DEBUGESPMANAGERF("done\n");
        }
    }

    if (root.containsKey(F("overwrite"))) {
        overwriteFiles = root["overwrite"].as<bool>();
        DEBUGESPMANAGERF("overwrite files set to %s\n", (overwriteFiles) ? "true" : "false");
    }

    if (root.containsKey(F("files"))) {

        JsonArray array = root[F("files")];
        files_expected = array.size();

        for (JsonArray::iterator it = array.begin(); it != array.end(); ++it) {
            JsonObject item = *it;
            String remote_path = String();

            //  if the is url is set to true then don't prepend the rootUri...
            if (remote_path.startsWith("http://")) {
                remote_path = String(item["location"].as<const char *>());
            } else {
                remote_path = rooturi + String(item["location"].as<const char *>());
            }

            const char* md5 = item[F("md5")];
            String filename = item[F("saveto")];

            if (remote_path.endsWith("bin") && filename == "sketch" ) {
                firmwareIndex = file_count - 1; //  true index vs counted = -1
                DEBUGESPMANAGERF("[%u/%u] BIN Updated pending, index %i\n", file_count, files_expected, firmwareIndex);
                file_count++;
                continue;
            }

#ifdef DEBUGESPMANAGER
            DEBUGESPMANAGER.print("\n\n");
#endif

            DEBUGESPMANAGERF("[%u/%u] Downloading (%s)..\n", file_count, files_expected, filename.c_str()  );

 //           MDNS.stop();

            int ret = _DownloadToFS(remote_path.c_str(), filename.c_str(), md5, overwriteFiles );

            if (ret == 0 || ret == FILE_NOT_CHANGED) {
 //**                event_send( FPSTR(fstring_CONSOLE), myStringf_P( PSTR("[%u/%u] (%s) : %s"), file_count, files_expected, filename.c_str(), (!ret) ? "Downloaded" : "Not changed" ) );
            } else {
//**                 event_send( FPSTR(fstring_CONSOLE), myStringf_P( PSTR("[%u/%u] (%s) : ERROR [%i]") , file_count, files_expected, filename.c_str(), ret ) );
            }

 //**            event_send( FPSTR(fstring_UPGRADE), myStringf_P( PSTR("%u") , (uint8_t ) (( (float)file_count / (float)files_expected) * 100.0f)  ) );

#if defined(DEBUGESPMANAGER)
            if (ret == 0) {
                DEBUGESPMANAGER.printf("SUCCESS \n");
            } else if (ret == FILE_NOT_CHANGED) {
                DEBUGESPMANAGER.printf("FILE NOT CHANGED \n");
            } else {
                DEBUGESPMANAGER.printf("FAILED [%i]\n", ret  );
            }
#endif

            file_count++;
        }

    } else {
//**         event_send( FPSTR(fstring_UPGRADE), myStringf_P( fstring_ERROR_toString, getError(MANIFST_FILE_ERROR).c_str() ) );
        DEBUGESPMANAGERF("ERROR [%i]\n", MANIFST_FILE_ERROR );
    }

    //  this removes any duplicate files if a compressed file exists
    _removePreGzFiles();

    if (firmwareIndex != -1) {

        //  for (JsonArray::iterator it = array.begin(); it != array.end(); ++it) {
        JsonArray array = root["files"];
        JsonObject item = array.getElement(firmwareIndex);

        String remote_path = rooturi + String(item["location"].as<const char *>());
        String filename = item[F("saveto")];
        String commit = root[F("commit")];

        if (remote_path.endsWith("bin") && filename == "sketch" ) {
            if ( item["md5"].as<String>() != getSketchMD5() ) {
                DEBUGESPMANAGERF("Current Sketch MD5 = %s\n", getSketchMD5().c_str()  );
                DEBUGESPMANAGERF("New     Sketch MD5 = %s\n",  item["md5"].as<String>().c_str()  );

                DEBUGESPMANAGERF("START SKETCH DOWNLOAD (%s)\n", remote_path.c_str()  );
 //**                event_send( FPSTR(fstring_UPGRADE), F("firmware"));
                delay(10);
                _events.send( "Upgrading sketch" , nullptr, 0, 5000);
                delay(10);
                ESPhttpUpdate.rebootOnUpdate(false);

  //              MDNS.stop(); 

                t_httpUpdate_return ret = ESPhttpUpdate.update(remote_path);

                switch (ret) {

                case HTTP_UPDATE_FAILED:
                    DEBUGESPMANAGERF("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                    delay(100);
 //**                   event_send( FPSTR(fstring_UPGRADE), myStringf_P( PSTR("ERROR [%s]"), ESPhttpUpdate.getLastErrorString().c_str()  ));
                    delay(100);
                    break;

                case HTTP_UPDATE_NO_UPDATES:
                    DEBUGESPMANAGERF("HTTP_UPDATE_NO_UPDATES");
                    delay(100);
//**                     event_send( FPSTR(fstring_UPGRADE), F("ERROR no update") );
                    delay(100);
                    break;

                case HTTP_UPDATE_OK:
                    DEBUGESPMANAGERF("HTTP_UPDATE_OK");
 //**                    event_send( FPSTR(fstring_UPGRADE), F("firmware-end") );
                    delay(100);
                    _events.close();
                    delay(1000);
                    ESP.restart();
                    break;
                }

            } else {
 //**                event_send( FPSTR(fstring_CONSOLE), F("No Change to firmware") );
                DEBUGESPMANAGERF("BINARY HAS SAME MD5 as current (%s)\n", item["md5"].as<const char *>()  );

            }
        } else {
            //  json object does not contain valid binary.
        }
    }

//**     event_send( FPSTR(fstring_UPGRADE), F("end"));

 //   MDNS.restart();

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
AsyncEventSource & ESPmanager::getEvent()
{
    return _events;
}

bool ESPmanager::event_send(const String & topic, const String & msg )
{
    _events.send(msg.c_str(), topic.c_str() , millis(), 5000);
    DEBUGESPMANAGERF("EVENT: top = %s, msg = %s\n", topic.c_str() , msg.c_str() );
}

/**
 * Saves settings to FS.
 * @return ESPMAN::ESPMAN_ERR_t
 */
ESPMAN_ERR_t ESPmanager::save()
{
    using namespace ESPMAN;
    _getAllSettings();

    if (_settings) {
        return _saveAllSettings(*_settings);
    } else {
        return SETTINGS_NOT_IN_MEMORY;
    }

}



#ifdef ESPMANAGER_UPDATER

ESPMAN_ERR_t ESPmanager::_DownloadToFS(const char * url, const char * filename_c, const char * md5_true, bool overwrite)
{
    using namespace ESPMAN;
    String filename = filename_c;
    HTTPClient http;
    FSInfo _FSinfo;
    int freeBytes = 0;
    bool success = false;
    ESPMAN_ERR_t ERROR = SUCCESS;

    DEBUGESPMANAGERF("URL = %s, filename = %s, md5 = %s, overwrite = %s\n", url, filename_c, md5_true, (overwrite) ? "true" : "false");

    if (!overwrite && _fs.exists(filename) ) {

        DEBUGESPMANAGERF("Checking for existing file.\n");
        File Fcheck = _fs.open(filename, "r");
        String crc = file_md5(Fcheck);

        if (crc == String(md5_true)) {
            Fcheck.close();
            return FILE_NOT_CHANGED;
        }

        Fcheck.close();
    }


    if (!_fs.info(_FSinfo)) {
        return FS_INFO_FAIL;

    }

    freeBytes = _FSinfo.totalBytes - _FSinfo.usedBytes;

    DEBUGESPMANAGERF("totalBytes = %u, usedBytes = %u, freebytes = %u\n", _FSinfo.totalBytes, _FSinfo.usedBytes, freeBytes);

    //  filename is 
    if (filename.length() > _FSinfo.maxPathLength) {
        return FS_FILENAME_TOO_LONG;
    }

    File f = _fs.open("/tempfile", "w+"); //  w+ is to allow read operations on file.... otherwise crc gets 255!!!!!

    if (!f) {

        return FS_FILE_OPEN_ERROR;
    }


    http.begin(url);

    int httpCode = http.GET();

    if (httpCode == 200) {

        int len = http.getSize();

        if (len > 0 && len < freeBytes) {

            WiFiUDP::stopAll();

            WiFiClient::stopAllExcept(http.getStreamPtr());
            wifi_set_sleep_type(NONE_SLEEP_T);

            FlashWriter writer;
            int byteswritten = 0;

            if (writer.begin(len)) {
                uint32_t start_time = millis();
                byteswritten = http.writeToStream(&writer);  //  this writes to the 1Mb Flash partition for the OTA upgrade.  zero latency...
                if (byteswritten > 0 && byteswritten == len) {
                    uint32_t start_time = millis();
                    byteswritten = writer.writeToStream(&f); //  contains a yield to allow networking.  Can take minutes to complete.
                } else {
                    DEBUGESPMANAGERF("HTTP to Flash error, byteswritten = %i\n", byteswritten);
                }

            } else {

                DEBUGESPMANAGERF("Try Old method and write direct to file\n");

                byteswritten = http.writeToStream(&f);
            }

            http.end();

            if (f.size() == (uint)len && byteswritten == len) { // byteswritten > 0 means no error writing.   ,len = -1 means server did not provide length...

                if (md5_true) {
                    String crc = file_md5(f);

                    if (crc == String(md5_true)) {
                        success = true;
                    } else {
                        ERROR = CRC_ERROR;
                    }

                } else {
                    DEBUGESPMANAGERF("\n  [ERROR] CRC not provided \n");
                    success = true;                             // set to true if no CRC provided...
                }

            } else {
                DEBUGESPMANAGERF("\n  [ERROR] Failed Download: length = %i, byteswritten = %i, f.size() = %i\n", len, byteswritten, f.size() );
                ERROR = INCOMPLETE_DOWNLOAD;
            }

        } else {
            DEBUGESPMANAGERF("\n  [ERROR] Not enough free space \n");
            ERROR = FILE_TOO_LARGE;
        }

    } else {
        DEBUGESPMANAGERF("\n  [ERROR] HTTP code = %i \n", ERROR);
        ERROR = static_cast<ESPMAN_ERR_t>(httpCode);
    }

    f.close();

    if (success) {

        if (_fs.exists(filename)) {
            _fs.remove(filename);
        }

        if (filename.endsWith(".gz") ) {
            String withOutgz = filename.substring(0, filename.length() - 3);
            DEBUGESPMANAGERF("NEW File ends in .gz: without = %s...\n", withOutgz.c_str());

            if (_fs.remove(withOutgz)) {
                DEBUGESPMANAGERF("%s DELETED...\n", withOutgz.c_str());
            }
        }

        if (_fs.exists(filename + ".gz")) {
            if (_fs.remove(filename + ".gz")) {
                DEBUGESPMANAGERF("%s.gz DELETED...\n", filename.c_str());
            }
        }

        if (!_fs.rename("/tempfile", filename)) {
            ERROR = FILE_RENAME_FAILED; 
             _fs.remove("/tempfile");
        } else {
            if (!_fs.exists(filename)) {
                ERROR = FILE_RENAME_FAILED; 
            }
        }



    } else {
        _fs.remove("/tempfile");
    }


    return ERROR;
}

/*
 *      Takes POST request with url parameter for the json
 *
 *
 */

ESPMAN_ERR_t ESPmanager::_parseUpdateJson(DynamicJsonDocument & json, const char * path)
{
    using namespace ESPMAN;
    DEBUGESPMANAGERF("path = %s\n", path);
    HTTPClient http;
    http.begin(path);  //HTTP
    int httpCode = http.GET();

    if (httpCode != 200) {
        DEBUGESPMANAGERF("HTTP code: %i\n", httpCode  );
        return static_cast<ESPMAN_ERR_t>(httpCode);
    }

    DEBUGESPMANAGERF("Connected downloading json\n");

    size_t len = http.getSize();
    const size_t length = len;

    if (len > MAX_BUFFER_SIZE) {
        DEBUGESPMANAGERF("Receive update length too big.  Increase buffer");
        return JSON_TOO_LARGE;
    }

    // get tcp stream
    WiFiClient * stream = http.getStreamPtr();

    auto jsonError = deserializeJson(json, *stream); 

    //int ret = json.parseStream(*stream);
    http.end();

    if (jsonError == DeserializationError::Ok) {
        DEBUGESPMANAGERF("root->success() = true\n");
        return SUCCESS;
    } else {
        DEBUGESPMANAGERF("root->success() = false\n");
        return JSON_PARSE_ERROR;
    }

}


void ESPmanager::_HandleSketchUpdate(AsyncWebServerRequest *request)
{
    if ( request->hasParam(F("url"), true)) {
        String path = request->getParam(F("url"), true)->value();
        DEBUGESPMANAGERF("path = %s\n", path.c_str());
        _tasker.add([ = ](Task & t) {
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
    if (!index) {
        _uploadAuthenticated = true; // not bothering just yet...
        if (!filename.startsWith("/")) { filename = "/" + filename; }
        request->_tempFile = _fs.open(filename, "w");
        DEBUGESPMANAGERF("UploadStart: %s\n", filename.c_str());
        String reply(F("UploadStart: ")); 
        reply.concat(filename); 
        event_send("" , reply  );
    }

    if (_uploadAuthenticated && request->_tempFile && len) {
        ESP.wdtDisable(); request->_tempFile.write(data, len); ESP.wdtEnable(10);
    }

    if (_uploadAuthenticated && final) {
        if (request->_tempFile) { request->_tempFile.close(); }
        DEBUGESPMANAGERF("UploadEnd: %s, %u B\n", filename.c_str(), index + len);
        char buf[100]{0};
        snprintf_P(buf, 100, PSTR("UploadFinished:%s (%u)") , filename.c_str(), request->_tempFile.size());         
        event_send("", buf );
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
    for (int i = 0; i < params; i++) {
        AsyncWebParameter* p = request->getParam(i);
        if (p->isFile()) { //p->isPost() is also true
            DEBUGESPMANAGER.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
        } else if (p->isPost()) {
            DEBUGESPMANAGER.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        } else {
            DEBUGESPMANAGER.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
    }

#endif


    using namespace ESPMAN;
    String buf;
    DynamicJsonDocument jsonBuffer(allocateJSON());
    JsonObject root = jsonBuffer.to<JsonObject>();

    static uint32_t last_handle_time = 0;
    bool sendsaveandreboot = false;

    //if (millis() - last_handle_time < 50) {

    //DEBUGESPMANAGER.printf("Time handle gap = %u\n", millis() - last_handle_time);

    //     return;
    // }

    last_handle_time = millis();

    if (!_settings) {
        _getAllSettings();
    }

    if (!_settings) {
        return;
    }

    settings_t & set = *_settings;

    set.start_time = millis(); //  resets the start time... to keep them in memory if being used.





#ifdef DEBUGESPMANAGER
    if (request->hasParam(F("body"), true) && request->getParam(F("body"), true)->value() == "diag") {

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


    if (request->hasParam(F("purgeunzipped"))) {
        // if (request->getParam("body")->value() == "purgeunzipped") {
        DEBUGESPMANAGERF("PURGE UNZIPPED FILES\n");
        _removePreGzFiles();
        //}

    }


    /*------------------------------------------------------------------------------------------------------------------
                                                                    Reboot command
       ------------------------------------------------------------------------------------------------------------------*/
    if (request->hasParam(F("body"), true)) {

        //ESPMan_Debugln(F("Has Body..."));


        const String & plainCommand = request->getParam(F("body"), true)->value();

        // Serial.printf("Plaincommand = %s\n", plainCommand.c_str());

        if (plainCommand == F("generalpage")) {

            DEBUGESPMANAGERF("Got body\n");


        }

        if (plainCommand == F("save")) {
            DEBUGESPMANAGERF("Saving Settings File....\n");
            if (_settings) {

                int ERROR = _saveAllSettings(*_settings);

#if defined(DEBUGESPMANAGER)

                File f = _fs.open(SETTINGS_FILE, "r");
                DEBUGESPMANAGER.printf("ESP MANAGER Settings [%u]B:\n", f.size());
                if (f) {
                    for (int i = 0; i < f.size(); i++) {
                        DEBUGESPMANAGER.write(f.read());
                    }
                    DEBUGESPMANAGER.println();
                    f.close();
                }

#endif

                if (ERROR) {
                    //event_printf(NULL, string_ERROR_toString, getError(ERROR).c_str());                   
                    event_send("", String(ERROR)  );
                    //event_printf(NULL, "There is an error %u\n", ERROR);
                } else {

                    //event_printf(NULL, "Settings Saved", ERROR);
                    event_send("", F("Settings Saved"));

                    set.changed = false;
                    if (_fs.remove("/.wizard")) {

                        _sendTextResponse(request, 200, FPSTR(fstring_OK));

                        _tasker.add( [this](Task & t) {
                            DEBUGESPMANAGERF("REBOOTING....\n");
                            delay(100);
                            ESP.restart();
                        });

                        return; //  stop request
                    }
                }
            }
        }

        if ( plainCommand == F("reboot") || plainCommand == F("restart")) {
            DEBUGESPMANAGERF("Rebooting...\n");

            _sendTextResponse(request, 200, FPSTR(fstring_OK));

            _tasker.add( [this](Task & t) {
                //event_printf(NULL, "Rebooting");
                //event_printf_P(NULL, PSTR("Rebooting"));
                event_send("", F("Rebooting"));

                delay(100);
                _events.close();
                delay(100);
                ESP.restart();
                delay(100000);
            });

            // _syncCallback = [this]() {
            //     _events.send("Rebooting", NULL, 0, 1000);
            //     delay(100);
            //     _events.close();

            //     delay(100);
            //     ESP.restart();
            //     delay(100000);
            //     return true;
            // };

        };

        /*------------------------------------------------------------------------------------------------------------------
                                          WiFi Scanning and Sending of WiFi networks found at boot
           ------------------------------------------------------------------------------------------------------------------*/
        if ( plainCommand == F("WiFiDetails") || plainCommand == F("PerformWiFiScan") || plainCommand == "generalpage" ) {

//************************
            if (plainCommand == F("PerformWiFiScan")) {

                int wifiScanState = WiFi.scanComplete();

                DynamicJsonDocument jsonBuffer(allocateJSON());
                JsonObject root = jsonBuffer.to<JsonObject>();

                if (wifiScanState == -2) {
                    WiFi.scanNetworks(true);
                    //_sendTextResponse(request, 200, "started");
                    event_send("", F("WiFi Scan Started") );
                    root[F("scan")] = "started";
                } else if (wifiScanState == -1) {
                    root[F("scan")] = "running";
                } else if (wifiScanState > 0) {

                    _wifinetworksfound = wifiScanState;

                    //using namespace std;

                    std::list < std::pair <int, int>> _container ;

                    for (int i = 0; i < _wifinetworksfound; i++) {

                        //_container.push_back(std::pair<int, int>(i, WiFi.RSSI(i)));
                        _container.emplace_back(i, WiFi.RSSI(i) ); //  use emplace... less copy/move semantics.
                    }

                    _container.sort([](const std::pair<int, int>& first, const std::pair<int, int>& second) {
                        return (first.second > second.second);
                    });

                    JsonArray Networkarray = root.createNestedArray("networks");


                    if (_wifinetworksfound > MAX_WIFI_NETWORKS) {
                        _wifinetworksfound = MAX_WIFI_NETWORKS;
                    }


                    // event_printf_P(NULL, PSTR("%u Networks Found"), _wifinetworksfound);
                    //event_printf(NULL, "%u Networks Found", _wifinetworksfound);
                    String reply = String(_wifinetworksfound) + "Networks Found"; 
                    event_send("",  reply );

                    std::list<std::pair <int, int>>::iterator it;

                    int counter = 0;

                    for (it = _container.begin(); it != _container.end(); it++) {
                        if (counter == MAX_WIFI_NETWORKS) {
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
                        switch (encryptiontype) {
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

                if (wifiScanState > 0) {
                    WiFi.scanDelete();
                }

                _wifinetworksfound = 0;
                return;

            }
//*************************


            WiFiMode mode = WiFi.getMode();
            //root[string_changed] = (set.changed) ? true : false;


            JsonObject generalobject = root.createNestedObject(FPSTR(fstring_General));

            generalobject[FPSTR(fstring_deviceid)] = getHostname();
            //generalobject[F("OTAenabled")] = (_OTAenabled) ? true : false;
            generalobject[FPSTR(fstring_OTApassword)] = (set.GEN.OTApassword) ? true : false;
            generalobject[FPSTR(fstring_GUIhash)] =  (set.GEN.GUIhash) ? true : false;
            generalobject[FPSTR(fstring_OTAport)] = set.GEN.OTAport;
            generalobject[FPSTR(fstring_ap_boot_mode)] = (int)_ap_boot_mode;
            generalobject[FPSTR(fstring_no_sta_mode)] = (int)_no_sta_mode;
            //generalobject[F("OTAusechipID")] = _OTAusechipID;
            generalobject[FPSTR(fstring_mDNS)] = (set.GEN.mDNSenabled) ? true : false;
            //generalobject[string_usePerminantSettings] = (set.GEN.usePerminantSettings) ? true : false;
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
            APobject[FPSTR(fstring_ssid)] = set.GEN.host;
            //APobject[F("state")] = (mode == WIFI_AP || mode == WIFI_AP_STA) ? true : false;
            APobject[F("state")] = set.AP.enabled;
            //APobject[F("APenabled")] = (int)set.AP.mode;
            //APobject[string_mode] = (int)_ap_mode;
            APobject[FPSTR(fstring_IP)] = (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) ? F("192.168.4.1") : WiFi.softAPIP().toString();
            APobject[FPSTR(fstring_visible)] = (set.AP.visible) ? true : false;
            APobject[FPSTR(fstring_pass)] = set.AP.pass; 

            softap_config config;

            if (wifi_softap_get_config( &config)) {

                APobject[FPSTR(fstring_channel)] = config.channel;

            }

            APobject[FPSTR(fstring_MAC)] = WiFi.softAPmacAddress();
            APobject[F("StationNum")] = WiFi.softAPgetStationNum();


        }

        /*------------------------------------------------------------------------------------------------------------------
                                          Send about page details...
           ------------------------------------------------------------------------------------------------------------------*/
        if (plainCommand == F("AboutPage")) {

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

            //const int BUFFER_SIZE = JSON_OBJECT_SIZE(30); // + JSON_ARRAY_SIZE(temphx.items);

            //      root[string_changed] = (set.changed) ? true : false;

            root[F("version_var")] = "Settings Manager V" ESPMANVERSION;
            root[F("compiletime_var")] = _compile_date_time;

            root[F("chipid_var")] = ESP.getChipId();
            root[F("cpu_var")] = ESP.getCpuFreqMHz();
            root[F("sdk_var")] = ESP.getSdkVersion();
            root[F("core_var")] = ESP.getCoreVersion();
            root[F("bootverion_var")] =  ESP.getBootVersion();
            root[F("bootmode_var")] =  ESP.getBootMode();

            root[F("heap_var")] = ESP.getFreeHeap();
            root[F("millis_var")] = millis();
            root[F("uptime_var")] = String(Up_time);

            root[F("flashid_var")] = ESP.getFlashChipId();
            root[F("flashsize_var")] = formatBytes( ESP.getFlashChipSize() );
            root[F("flashRealSize_var")] = formatBytes (ESP.getFlashChipRealSize() ); // not sure what the difference is here...
            root[F("flashchipsizebyid_var")] = formatBytes (ESP.getFlashChipSizeByChipId());
            root[F("flashchipmode_var")] = (uint32_t)ESP.getFlashChipMode();

            root[F("chipid_var")] = ESP.getChipId();
            String sketchsize = formatBytes(ESP.getSketchSize());//+ " ( " + String(ESP.getSketchSize()) +  " Bytes)";
            root[F("sketchsize_var")] = sketchsize;
            // root[PSTR("SketchMD5")] = getSketchMD5();
            String freesketchsize = formatBytes(ESP.getFreeSketchSpace());//+ " ( " + String(ESP.getFreeSketchSpace()) +  " Bytes)";
            root[F("freespace_var")] = freesketchsize;

            root[F("vcc_var")] = ESP.getVcc();
            root[F("rssi_var")] = WiFi.RSSI();

            JsonObject FSobject = root.createNestedObject("SPIFFS"); //  currently everywhere will be still using SPIIFFS
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
            FSobject[F("blockSize")] = formatBytes(info.blockSize);
            FSobject[F("pageSize")] = formatBytes(info.pageSize);
            //FSobject[F("allocatedPages")] = info.allocatedPages;
            //FSobject[F("deletedPages")] = info.deletedPages;
            FSobject[F("maxOpenFiles")] = info.maxOpenFiles;
            FSobject[F("maxPathLength")] = info.maxPathLength;

            // typedef struct UMM_HEAP_INFO_t {
            //   unsigned short int totalEntries;
            //   unsigned short int usedEntries;
            //   unsigned short int freeEntries;
            //
            //   unsigned short int totalBlocks;
            //   unsigned short int usedBlocks;
            //   unsigned short int freeBlocks;
            //
            //   unsigned short int maxFreeContiguousBlocks;
            // }

            JsonObject UMMobject = root.createNestedObject("UMM");
            UMMobject[F("totalEntries")] = ummHeapInfo.totalEntries;
            UMMobject[F("usedEntries")] = ummHeapInfo.usedEntries;
            UMMobject[F("freeEntries")] = ummHeapInfo.freeEntries;
            UMMobject[F("totalBlocks")] = ummHeapInfo.totalBlocks;
            UMMobject[F("usedBlocks")] = ummHeapInfo.usedBlocks;
            UMMobject[F("freeBlocks")] = ummHeapInfo.freeBlocks;
            UMMobject[F("maxFreeContiguousBlocks")] = ummHeapInfo.maxFreeContiguousBlocks;

            JsonObject Resetobject = root.createNestedObject("reset");

            Resetobject[F("reason")] = ESP.getResetReason();
            Resetobject[F("info")] = ESP.getResetInfo();

        }

        /*------------------------------------------------------------------------------------------------------------------
                                          Update handles...
           ------------------------------------------------------------------------------------------------------------------*/


        if ( plainCommand == F("UpdateDetails")) {


            //      root[string_changed] = (set.changed) ? true : false;

            // root[F( "REPO")] =  slugTag;
            // root[F("BRANCH")] = branchTag;

            // char shortcommit[8] = {0};
            // strncpy(shortcommit, commitTag, 7);
            // root[F( "COMMIT")] = shortcommit;
            root[FPSTR(fstring_updateURL)] = set.GEN.updateURL;
            root[FPSTR(fstring_updateFreq)] = set.GEN.updateFreq;
            //sendJsontoHTTP(root, request);
            //return;

        }

        if (plainCommand == F("formatSPIFFS")) {
            DEBUGESPMANAGERF("Format SPIFFS\n");

            //event_printf_P(NULL, PSTR("Formatting SPIFFS"));
            //event_printf(NULL, "Formatting SPIFFS");

            event_send("", F("Formatting FS"));
            _sendTextResponse(request, 200, FPSTR(fstring_OK));

            _tasker.add( [this](Task & t) {

                _getAllSettings();
                _fs.format();
                if (_settings) {
                    _saveAllSettings(*_settings);
                }
                DEBUGESPMANAGERF(" done\n");
                //event_printf_P(NULL, PSTR("Formatting done"));

                event_send("", F("Formatting done"));
                //event_printf(NULL, "Formatting done");


            });


            // _syncCallback = [this]() {
            //     _fs.format();
            //     ESPMan_Debugln(F(" done"));
            //     _events.send("Formatting done", nullptr, 0, 5000);
            //     return true;
            // };
        }

        if (plainCommand == F("deletesettings")) {

            DEBUGESPMANAGERF("Delete Settings File\n");
            if (_fs.remove(SETTINGS_FILE)) {
                DEBUGESPMANAGERF(" done");
                //event_printf_P(NULL, PSTR("Settings File Removed"));
                event_send("", F("Settings File Removed"));
                //event_printf(NULL, "Settings File Removed");

            } else {
                DEBUGESPMANAGERF(" failed");
            }
        }


        if ( plainCommand == F("resetwifi") ) {

            _tasker.add( [this](Task & t) {

                //event_printf(NULL, "Reset WiFi and Reboot");
                //event_printf_P(NULL, PSTR("Reset WiFi and Reboot"));
                event_send("", F("Settings File Removed"));

                delay(100);
                _events.close();
                delay(100);

                WiFi.disconnect();
                ESP.eraseConfig();
                ESP.restart();
                //return true;
            });

        }


        /*------------------------------------------------------------------------------------------------------------------

                                       wizard
        ------------------------------------------------------------------------------------------------------------------*/

        if (plainCommand == F("enterWizard")) {

            ESP.eraseConfig();

            DEBUGESPMANAGERF("Enter Wizard hit\n");

            File f = _fs.open("/.wizard", "w"); //  creates a file that overrides everything during initial config...

            uint8_t * data = static_cast<uint8_t*>(static_cast<void*>(&set.AP));

            if (f) {
                for (int i = 0; i < sizeof(set.AP); i++) {
                    f.write(  data[i]);
                }

                _sendTextResponse(request, 200, FPSTR(fstring_OK));
                return;
            } else {

                _sendTextResponse(request, 200, F("File Error") );

                return;
            }

        }


        if (plainCommand == F("cancelWizard")) {

            _fs.remove("/.wizard");
        }

        if (plainCommand == F("factoryReset")) {

            _sendTextResponse(request, 200, "Factory Reset Done");


            _tasker.add( [this](Task & t) {
                factoryReset();
                delay(100);
                ESP.restart();
                while (1);
                //return true;
            });
            return;
        }

        if (plainCommand == FPSTR(fstring_syslog)) {

            JsonObject syslogobject = root.createNestedObject( FPSTR(fstring_syslog));

            syslogobject[FPSTR(fstring_usesyslog)] = set.GEN.usesyslog;
            syslogobject[FPSTR(fstring_syslogIP)] = set.GEN.syslogIP.toString();
            syslogobject[FPSTR(fstring_syslogPort)] = set.GEN.syslogPort;
            syslogobject[FPSTR(fstring_syslogProto)] = set.GEN.syslogProto;


        }

        if (plainCommand == F("discover")) {

            DEBUGESPMANAGERF("Discover Devices\n");

            if (_devicefinder && !_deviceFinderTimer) {
                _devicefinder->cacheResults(true);
                _devicefinder->ping();
                _deviceFinderTimer = millis();

                _tasker.add( [this](Task & t) {

                    if (millis() - _deviceFinderTimer > _ESPdeviceTimeout) {
                        t.setRepeat(false);
                        if (_devicefinder) {
                            uint32_t pre_heap = ESP.getFreeHeap();
                            _devicefinder->cacheResults(false);
                            DEBUGESPMANAGERF("Removing Found Devices after %us freeing %u\n", _ESPdeviceTimeout / 1000, ESP.getFreeHeap() - pre_heap  );
                        }
                        _deviceFinderTimer = 0;
                    }
                }).setTimeout(1000).setRepeat(true);

            }

            if (_devicefinder) {

                _devicefinder->ping();
                _populateFoundDevices(root);
            }

        }

        if (plainCommand == F("getDevices")) {

            _populateFoundDevices(root);

            //  reset the timer so as to not delete the results.
            if (_deviceFinderTimer) {
                _deviceFinderTimer = millis();
            }


        }


    } //  end of if plaincommand





    /*



                Individual responsces...



    */

    if (request->hasParam( FPSTR(fstring_ssid), true) && request->hasParam( FPSTR(fstring_pass), true)) {

        bool APChannelchange  = false;
        int channel = -1;

        String ssid = request->getParam(FPSTR(fstring_ssid), true)->value();
        String psk = request->getParam(FPSTR(fstring_pass), true)->value();

        if (ssid.length() > 0) {
            // _HTTP.arg("pass").length() > 0) {  0 length passwords should be ok.. for open
            // networks.
            if (ssid.length() < 33 && psk.length() < 33) {

                if (ssid != WiFi.SSID() || psk != WiFi.psk() || !set.STA.enabled ) {

                    bool safety = false;

                    if (request->hasParam(F("removesaftey"), true))  {
                        safety = (request->getParam(F("removesaftey"), true)->value() == "No") ? false : true;
                    }

                    settings_t::STA_t * newsettings = new settings_t::STA_t(set.STA);

                    newsettings->ssid = ssid.c_str();
                    newsettings->pass = psk.c_str();
                    newsettings->enabled = true;

                    DEBUGESPMANAGERF("applied new ssid & psk to tmp obj ssid =%s, pass=%s\n", newsettings->ssid.c_str(), newsettings->pass.c_str() );

                    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA ) {

                        int currentchannel = WiFi.channel();


                        if (request->hasParam(F("STAchannel_desired"), true)) {


                            int desired_channal = request->getParam(F("STAchannel_desired"), true)->value().toInt();

                            if (desired_channal != currentchannel && desired_channal >= 0 && currentchannel >= 0) {

                                DEBUGESPMANAGERF("AP Channel change required: current = %i, desired = %i\n", currentchannel, desired_channal);
                                APChannelchange = true;
                                channel = desired_channal;

                            } else {
                                DEBUGESPMANAGERF("AP Channel change NOT required: current = %i, desired = %i\n", currentchannel, desired_channal);
                            }

                        }

                        //


                    }

                    _tasker.add( [safety, newsettings, request, this, APChannelchange, channel](Task & t) {

                        //_syncCallback = [safety, newsettings, request, this, APChannelchange, channel]() {

                        using namespace ESPMAN;

                        WiFiresult = 0;
                        uint32_t starttime = millis();

                        if (APChannelchange) {

                            uint8_t connected_station_count = WiFi.softAPgetStationNum();

                            DEBUGESPMANAGERFRAW("Changing AP channel to %u :", channel);

                            //event_printf(NULL, "Changing AP Channel...");
                            //event_printf_P(NULL, PSTR("Changing AP Channel..."));
                            event_send("", F("Changing AP Channel..."));

                            // delay(10);
                            // WiFi.enableAP(false);
                            // settings_t set;
                            // set.AP.ssid = set.GEN.host();
                            // set.AP.channel = channel;
                            // set.AP.enabled = true;
                            // bool result = _initialiseAP(set.AP);


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

                            struct softap_config * _currentconfig = new softap_config;

                            if (_currentconfig && wifi_softap_get_config(_currentconfig)) {
                                _currentconfig->channel = channel;
                                result = wifi_softap_set_config_current(_currentconfig);
                            }

                            if (result) {
                                DEBUGESPMANAGERFRAW("Waiting For AP reconnect\n");
                                starttime = millis();

                                uint32_t dottimer = millis();

                                while ( WiFi.softAPgetStationNum() < connected_station_count) {

                                    if (_dns) {
                                        _dns->processNextRequest();
                                    }

                                    //yield();
                                    delay(10);
                                    if (millis() - dottimer > 1000) {

                                        DEBUGESPMANAGERFRAW(".");

                                        dottimer = millis();
                                    }

                                    if (millis() - starttime > 60000) {
                                        DEBUGESPMANAGERFRAW("Error waiting for AP reconnect\n");
                                        WiFiresult = 2;
                                        WiFi.enableSTA(false);
                                        if (newsettings) {
                                            delete newsettings;
                                        }

                                        return true;
                                        break;
                                    }

                                }




                            } else {
                                DEBUGESPMANAGERFRAW("Error: %i\n", result);
                            }



                        }

                        starttime = millis() ;// reset the start timer....
                        //event_printf_P(NULL, PSTR("Updating WiFi Settings"));
                        event_send("", F("Updating WiFi Settings"));
                        //event_printf(NULL, "Updating WiFi Settings");

                        delay(10);

                        int ERROR = _initialiseSTA(*newsettings);

                        if (!ERROR) {
                            DEBUGESPMANAGERFRAW("CALLBACK: Settings successfull\n");
                            WiFiresult = 1;

                            if (!_settings) {
                                _getAllSettings();
                            }


                            if (_settings && newsettings) {
                                //Serial.print("\n\n\nsettings->STA = *newsettings;\n\n");
                                _settings->STA = *newsettings;
                                //Serial.print("\ndone\n\n");
                                _settings->changed = true;
                                DEBUGESPMANAGERFRAW("CALLBACK: Settings Applied\n");
                                save_flag = true;
                            }

                        } else if (ERROR == NO_CHANGES ) {
                            DEBUGESPMANAGERFRAW("CALLBACK: No changes....\n");
                            WiFiresult = 1;
                        } else {
                            WiFiresult = 2;
                            DEBUGESPMANAGERFRAW("ERROR: %i\n", ERROR);
                            //WiFi.enableSTA(false); //  turns it off....
                            if (_settings) {
                                if (!_initialiseSTA(_settings->STA)) { //  go back to old settings...
                                    event_send("", F("Old Settings Restored"));
                                }
                            }
                        }

                        //event_printf_P(NULL, PSTR("WiFi Settings Updated"));
                        event_send("", F("WiFi Settings Updated"));
                        //event_printf(NULL, "WiFi Settings Updated");

                        if (newsettings) {
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
    if (  request->hasParam(F("body"), true) && request->getParam(F("body"), true)->value() == F("WiFiresult")) {



        if (WiFiresult == 1 && WiFi.localIP() != INADDR_NONE) {
            WiFiresult = 4; // connected
        }

        DEBUGESPMANAGERF("WiFiResult = %i [%u.%u.%u.%u]\n", WiFiresult, WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);

        _sendTextResponse(request, 200, String(WiFiresult).c_str());

        // AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", String(WiFiresult));
        // response->addHeader(ESPMAN::string_CORS, "*");
        // response->addHeader(ESPMAN::string_CACHE_CONTROL, "no-store");
        // request->send(response);
        return;
    }
    /*------------------------------------------------------------------------------------------------------------------

                                     STA config
       ------------------------------------------------------------------------------------------------------------------*/


    if (request->hasParam(F("enable-STA"), true)) {

        bool changes = false;

        // Serial.println("SETTINGS COPIED INTO TEMP BUFFER");
        // _dumpSTA(set.STA);
        // Serial.println();

        settings_t::STA_t * newsettings = new settings_t::STA_t(set.STA);

        if (newsettings) {

            /*
                    ENABLED
             */

            bool enable = request->getParam(F("enable-STA"), true)->value().equals("on");

            if (enable != newsettings->enabled) {
                newsettings->enabled = enable;
                changes = true;
            }

            /*
                    DHCP and Config
             */
            if (request->hasParam(F("enable-dhcp"), true)) {

                bool dhcp = request->getParam(F("enable-dhcp"), true)->value().equals("on");

                //



                if (dhcp) {
                    DEBUGESPMANAGERF("dhcp = on\n" );

                    if (!_settings->STA.dhcp) {
                        changes = true;
                    }

                    newsettings->dhcp = true;
                    newsettings->hasConfig = false;
                    newsettings->IP = INADDR_NONE;
                    newsettings->GW = INADDR_NONE;
                    newsettings->SN = INADDR_NONE;
                    newsettings->DNS1 = INADDR_NONE;
                    newsettings->DNS2 = INADDR_NONE;



                } else {
                    DEBUGESPMANAGERF("dhcp = off\n" );

                    if (_settings->STA.dhcp) {
                        changes = true;
                    }

                    bool IPres {false};
                    bool GWres {false};
                    bool SNres {false};
                    bool DNSres {false};

                    if (request->hasParam( FPSTR(fstring_IP), true) &&
                            request->hasParam( FPSTR(fstring_GW), true) &&
                            request->hasParam( FPSTR(fstring_SN), true) &&
                            request->hasParam( FPSTR(fstring_DNS1), true) ) {

                        IPres = newsettings->IP.fromString( request->getParam(FPSTR(fstring_IP), true)->value() );
                        GWres = newsettings->GW.fromString( request->getParam(FPSTR(fstring_GW), true)->value() );
                        SNres = newsettings->SN.fromString( request->getParam(FPSTR(fstring_SN), true)->value() );
                        DNSres = newsettings->DNS1.fromString( request->getParam(FPSTR(fstring_DNS1), true)->value() );
                    }


                    if (IPres && GWres && SNres && DNSres) {

                        //  apply settings if any of these are different to current settings...
                        if (newsettings->IP != _settings->STA.IP ||  newsettings->GW != _settings->STA.GW || newsettings->SN != _settings->STA.SN || newsettings->DNS1 != _settings->STA.DNS1 ) {
                            changes = true;
                        }
                        DEBUGESPMANAGERF("Config Set\n");
                        newsettings->hasConfig = true;
                        newsettings->dhcp = false;

                        if (request->hasParam(FPSTR(fstring_DNS2), true)) {

                            bool res = newsettings->DNS2.fromString ( request->getParam(FPSTR(fstring_DNS2), true)->value() );
                            if (res) {
                                DEBUGESPMANAGERF("DNS 2 %s\n",  newsettings->DNS2.toString().c_str() );
                                if (newsettings->DNS2 != _settings->STA.DNS2 ) {
                                    changes = true;
                                }
                            }

                        }

                    }

                    DEBUGESPMANAGERF("IP %s, GW %s, SN %s\n", (IPres) ? "set" : "error", (GWres) ? "set" : "error", (SNres) ? "set" : "error"  );
                }


                //}
            }
            /*
                    autoconnect and reconnect
             */
            if (request->hasParam(FPSTR(fstring_autoconnect), true)) {

                bool autoconnect = request->getParam(FPSTR(fstring_autoconnect), true)->value().equals("on");

                if (autoconnect != newsettings->autoConnect) {
                    newsettings->autoConnect = autoconnect;
                    changes = true;
                }
            }

            if (request->hasParam(FPSTR(fstring_autoreconnect), true)) {
                bool autoreconnect = request->getParam(FPSTR(fstring_autoreconnect), true)->value().equals("on");

                if (autoreconnect != newsettings->autoReconnect) {
                    newsettings->autoReconnect = autoreconnect;
                    changes = true;
                }
            }

            if (request->hasParam(FPSTR(fstring_MAC), true) && request->getParam(FPSTR(fstring_MAC), true)->value().length() != 0) {



                if ( StringtoMAC(newsettings->MAC, request->getParam(FPSTR(fstring_MAC), true)->value() ) ) {


                    DEBUGESPMANAGERF("New STA MAC parsed sucessfully\n");
                    DEBUGESPMANAGERF("[%u]:[%u]:[%u]:[%u]:[%u]:[%u]\n", newsettings->MAC[0], newsettings->MAC[1], newsettings->MAC[2], newsettings->MAC[3], newsettings->MAC[4], newsettings->MAC[5]);

                    // compare MACS..

                    uint8_t currentmac[6];
                    WiFi.macAddress(&currentmac[0]);

                    if (memcmp(&(currentmac[0]), newsettings->MAC, 6)) {
                        DEBUGESPMANAGERF("New  MAC is different\n");
                        newsettings->hasMAC = true;
                        changes = true;
                    } else {
                        DEBUGESPMANAGERF("New MAC = Old MAC\n");
                        newsettings->hasMAC = false;
                        for (uint8_t i = 0; i < 6; i++) {
                            newsettings->MAC[i] = '\0';
                        }
                    }


                } else {
                    newsettings->hasMAC = false;
                    for (uint8_t i = 0; i < 6; i++) {
                        newsettings->MAC[i] = '\0';
                    }
                    DEBUGESPMANAGERF("New STA MAC parsed FAILED\n");
                }
            }

            if (changes) {

                _tasker.add( [this, newsettings](Task & t) {

                    //_syncCallback = [this, newsettings] () {

                    using namespace ESPMAN;

                    //event_printf(NULL, "Updating WiFi Settings");
                    //event_printf_P(NULL, PSTR("Updating WiFi Settings"));
                    event_send("", F("Updating WiFi Settings"));

                    delay(10);

                    DEBUGESPMANAGERF("*** CALLBACK: dhcp = %s\n", (newsettings->dhcp) ? "true" : "false");
                    DEBUGESPMANAGERF("*** CALLBACK: hasConfig = %s\n", (newsettings->hasConfig) ? "true" : "false");


                    int ERROR = _initialiseSTA(*newsettings);

                    DEBUGESPMANAGERF("*** CALLBACK: ERROR = %i\n", ERROR);


                    //WiFi.printDiag(Serial);

                    if (!ERROR || (ERROR == STA_DISABLED && newsettings->enabled == false)) {
                        DEBUGESPMANAGERF("CALLBACK: Settings successfull\n");

                        if (!_settings) {
                            _getAllSettings();
                        }


                        if (_settings) {
                            _settings->STA = *newsettings;
                            _settings->changed = true;
                            DEBUGESPMANAGERF("CALLBACK: Settings Applied\n");
                            // _dumpSettings();
                            //event_printf(NULL, "Success");
                            //event_printf_P(NULL, PSTR("Success"));
                            event_send("", F("Success"));

                            //save_flag = true;
                        } else {
                            //event_printf(NULL, string_ERROR_toString, getError(SETTINGS_NOT_IN_MEMORY).c_str());
                            event_send("", getError(SETTINGS_NOT_IN_MEMORY));
                        }

                    } else {
                        DEBUGESPMANAGERF("ERORR: Settings NOT applied successfull %i\n", ERROR);
                        //event_printf(NULL, string_ERROR_toString, getError(ERROR).c_str());
                        event_send("",   getError(ERROR));

                        _getAllSettings();
                        if (_settings) {
                            if (_initialiseSTA(_settings->STA)) {
                                DEBUGESPMANAGERF("OLD settings reapplied\n");
                            }
                        }
                    }

                    delete newsettings;

                    return true;
                });
            } else {
                //event_printf(NULL, "No Changes Made");
                //event_printf_P(NULL, PSTR("No Changes Made"));
                event_send("", F("No Changes Made"));

                DEBUGESPMANAGERF("No changes Made\n");

            }
        }
    }

    /*------------------------------------------------------------------------------------------------------------------

                                     AP config
       ------------------------------------------------------------------------------------------------------------------*/

    if (request->hasParam(F("enable-AP"), true)) {

        bool changes = false;
        bool abortchanges = false;

        settings_t::AP_t * newsettings = new settings_t::AP_t(set.AP); // creates a copy of current settings using new... smart_Ptr don't work well yet for lambda captures

        if (newsettings) {

            /*
                    ENABLED
             */

            newsettings->ssid = set.GEN.host;

            bool enabled = request->getParam(F("enable-AP"), true)->value().equals("on");

            if (enabled != newsettings->enabled) {
                newsettings->enabled = enabled;
                changes = true;
            }

            if (request->hasParam(FPSTR(fstring_pass), true)) {

                const String pass = request->getParam(FPSTR(fstring_pass), true)->value();

                if (pass.length() > 8 && pass.length() < 63 ) {
                    // fail passphrase to long or short!
                    DEBUGESPMANAGERF("[AP] fail passphrase to long or short!\n");
                    //event_printf(nullptr, string_ERROR_toString, getError(PASSWOROD_INVALID).c_str());

                    event_send("", getError(PASSWOROD_INVALID));
                    abortchanges = true;
                }

                if (pass.length() && newsettings->pass != pass ) {
                    newsettings->pass = pass;
                    DEBUGESPMANAGERF("New AP pass = %s\n", newsettings->pass.c_str() );
                    changes = true;
                }

            }


            if (request->hasParam(FPSTR(fstring_channel), true)) {
                int channel = request->getParam(FPSTR(fstring_channel), true)->value().toInt();

                if (channel > 13) {
                    channel = 13;
                }

                if (channel != newsettings->channel) {
                    newsettings->channel = channel;
                    changes = true;
                    DEBUGESPMANAGERF("New Channel = %u\n", newsettings->channel );
                }


            }

            if (request->hasParam(FPSTR(fstring_IP), true)) {

                IPAddress newIP;
                bool result = newIP.fromString(request->getParam(FPSTR(fstring_IP), true)->value());


                if (result) {

                    changes = true;

                    if (newIP == IPAddress(192, 168, 4, 1)) {
                        newsettings->hasConfig = false;
                        newsettings->IP = newIP;
                        newsettings->GW = INADDR_NONE;
                        newsettings->SN = INADDR_NONE;

                    } else {
                        newsettings->hasConfig = true;
                        newsettings->IP = newIP;
                        newsettings->GW = newIP;
                        newsettings->SN = IPAddress(255, 255, 255, 0);

                    }

                    DEBUGESPMANAGERF("New AP IP = %s\n", newsettings->IP.toString().c_str() );
                }


            }

            /*******************************************************************************************************************************
                                      AP MAC changes disabled  for now.  bit more complex....
            *******************************************************************************************************************************/
            // if (request->hasParam(string_MAC, true) && request->getParam(string_MAC, true)->value().length() != 0) {
            //
            //         //changes = true;
            //
            //         if ( StringtoMAC(newsettings->MAC, request->getParam(string_MAC, true)->value() ) ) {
            //
            //                 //newsettings->hasMAC = true;
            //                 ESPMan_Debugln("New AP MAC parsed sucessfully");
            //                 DEBUGESPMANAGERF("[%u]:[%u]:[%u]:[%u]:[%u]:[%u]\n", newsettings->MAC[0], newsettings->MAC[1], newsettings->MAC[2], newsettings->MAC[3], newsettings->MAC[4], newsettings->MAC[5]);
            //                 // compare MACS..
            //
            //                 uint8_t currentmac[6];
            //                 WiFi.softAPmacAddress(&currentmac[0]);
            //
            //                 if (memcmp(&(currentmac[0]), newsettings->MAC,6)) {
            //                         ESPMan_Debugln("New  MAC is different");
            //                         newsettings->hasMAC = true;
            //                         changes = true;
            //                 } else {
            //                         ESPMan_Debugln("New MAC = Old MAC");
            //                         newsettings->hasMAC = false;
            //                         for (uint8_t i = 0; i < 6; i++) {
            //                                 newsettings->MAC[i] = '\0';
            //                         }
            //
            //                 }
            //
            //
            //         } else {
            //                 newsettings->hasMAC = false;
            //                 for (uint8_t i = 0; i < 6; i++) {
            //                         newsettings->MAC[i] = '\0';
            //                 }
            //                 ESPMan_Debugln("New AP MAC parsed FAILED");
            //         }
            // }

            if (changes && !abortchanges) {

                _tasker.add( [this, newsettings](Task & t) {

                    //_syncCallback = [this, newsettings] () {

                    using namespace ESPMAN;

                    //event_printf(NULL, "Updating AP Settings");
                    //event_printf_P(NULL, PSTR("Updating AP Settings"));
                    event_send("", F("Updating AP Settings") );

                    delay(10);

                    int ERROR = _initialiseAP(*newsettings);

                    if (!ERROR || (ERROR == AP_DISABLED && newsettings->enabled == false)) {
                        DEBUGESPMANAGERF("AP CALLBACK: Settings successfull\n");

                        if (!_settings) {
                            _getAllSettings();
                        }


                        if (_settings) {
                            _settings->AP = *newsettings;
                            _settings->changed = true;
                            DEBUGESPMANAGERF("CALLBACK: Settings Applied\n");
                            //_dumpSettings();

                            //event_printf(NULL, "Success");
                            //event_printf_P(NULL, PSTR("Success"));
                            event_send("", F("Success"));
                            //save_flag = true;
                        } else {
                            //event_printf(NULL, string_ERROR_toString, getError(SETTINGS_NOT_IN_MEMORY).c_str());
                            event_send("", getError(SETTINGS_NOT_IN_MEMORY) );
                        }

                    } else {

                        _getAllSettings();

                        if (_settings) {
                            DEBUGESPMANAGERF("Restoring old settings ERROR = %i, %s\n", ERROR , getError(ERROR).c_str() );

                            _initialiseAP(_settings->AP);

                        }


                        //event_printf(NULL, string_ERROR_toString, getError(ERROR).c_str() ) ;
                        event_send("", getError(ERROR) );
                    }

                    delete newsettings;

                    return true;
                });
            } else {
                //event_printf(NULL, "No Changes Made");
                //event_printf_P(NULL, PSTR("No Changes Made"));
                event_send("", F("No Changes Made"));

                DEBUGESPMANAGERF("No changes Made\n");

            }
        }
    } //  end of enable-AP

    /*------------------------------------------------------------------------------------------------------------------

                                     Device Name
       ------------------------------------------------------------------------------------------------------------------*/

    if ( request->hasParam(FPSTR(fstring_deviceid), true)) {

        String newid = request->getParam(FPSTR(fstring_deviceid), true)->value();

        DEBUGESPMANAGERF( "Device ID func hit %s\n", newid.c_str()  );

        if (newid.length() && newid.length() < 32 && newid != set.GEN.host ) {

            set.GEN.host = newid;
            set.changed = true;
            //event_printf(NULL, "Device ID: %s", set.GEN.host() );
            String reply = "Device ID: " + set.GEN.host; 
            event_send("",  reply ) ;
            sendsaveandreboot = true;
        }

    }
    /*------------------------------------------------------------------------------------------------------------------

                                     Restart wifi
       ------------------------------------------------------------------------------------------------------------------*/



    /*------------------------------------------------------------------------------------------------------------------

                                     OTA config
       ------------------------------------------------------------------------------------------------------------------*/
    // if (_HTTP.hasArg("OTAusechipID")) {

    //     _OTAusechipID = (_HTTP.arg("OTAusechipID") == string_yes)? true : false;
    //     ESPMan_Debugln(F("OTA append ChipID to host"));
    //     save_flag = true;
    // }

    if ( request->hasParam(FPSTR(fstring_OTAupload), true)) {

        // save_flag = true;

        bool command =  request->getParam(FPSTR(fstring_OTAupload), true)->value().equals( "on");

        if (command != set.GEN.OTAupload) {

            //_OTAupload = command;
            set.GEN.OTAupload = command;
            set.changed = true;

            DEBUGESPMANAGERF("_OTAupload = %s\n", (set.GEN.OTAupload) ? "enabled" : "disabled");


        }

    } // end of OTA enable


    if ( request->hasParam(FPSTR(fstring_OTApassword), true)  ) {

        char pass_confirm[40] = {0};

        strncpy_P(pass_confirm , fstring_OTApassword, 30);
        strncat_P(pass_confirm , PSTR("_confirm"), 9);

        if (request->hasParam(pass_confirm, true) ) {

            String S_pass = request->getParam(FPSTR(fstring_OTApassword), true)->value();
            String S_confirm = request->getParam(pass_confirm, true)->value();

            Serial.printf("S_pass = %s, len = %u", S_pass.c_str(), S_pass.length()); 

            const char * pass = S_pass.c_str();
            const char * confirm = S_confirm.c_str();

            if (pass && confirm && !strncmp(pass, confirm, 40))  {

                DEBUGESPMANAGERF("Passwords Match\n");
                set.changed = true;
                MD5Builder md5;
                md5.begin();
                md5.add( pass) ;
                md5.calculate();
                set.GEN.OTApassword = md5.toString().c_str() ;

                if (S_pass.length() == 0 ) { 
                    set.GEN.OTApassword = ""; 
                    Serial.println("Password set but making null"); 
                }
                //set.GEN.OTApassword = pass;

                sendsaveandreboot = true;

            } else {
                //event_printf(nullptr, string_ERROR_toString, getError(PASSWORD_MISMATCH).c_str() ) ;
                
                event_send( "", getError(PASSWORD_MISMATCH) );


            }
        }

    } // end of OTApass



    /*
       ARG: 0, "enable-AP" = "on"
       ARG: 1, "setAPsetip" = "0.0.0.0"
       ARG: 2, "setAPsetmac" = "1A%3AFE%3A34%3AA4%3A4C%3A73"
     */

    if ( request->hasParam(FPSTR(fstring_mDNS), true)) {


        save_flag = true;

        bool command = request->getParam(FPSTR(fstring_mDNS), true)->value().equals("on");

        if (command != set.GEN.mDNSenabled ) {
            set.GEN.mDNSenabled = command;
            set.changed = true;
            DEBUGESPMANAGERF("mDNS set to : %s\n", (command) ? "on" : "off");
            sendsaveandreboot = true;
            //  InitialiseFeatures();
        }
    } // end of OTA enable
    /*------------------------------------------------------------------------------------------------------------------

                                       PORTAL
       ------------------------------------------------------------------------------------------------------------------*/

    if ( request->hasParam(F("enablePortal"), true)) {

        // save_flag = true;

        bool command =  request->getParam(F("enablePortal"), true)->value().equals( "on");

        if (command != _settings->GEN.portal) {

            _settings->GEN.portal = command;
            set.changed = true;

            DEBUGESPMANAGERF("settings->GEN.portal = %s\n", (command) ? "enabled" : "disabled");


        }

    } // end of OTA enable
    /*------------------------------------------------------------------------------------------------------------------

                                       SysLog

    POST[usesyslog]: on
    POST[syslogIP]: 192.168.1.188
    POST[syslogPort]: 5014
    POST[syslogProto]: 1

       ------------------------------------------------------------------------------------------------------------------*/

// #ifdef ESPMANAGER_SYSLOG

//     if ( request->hasParam(FPSTR(fstring_usesyslog), true)) {

//         bool value = request->getParam(FPSTR(fstring_usesyslog), true)->value().equals( "on");

//         if (value != _settings->GEN.usesyslog) {
//             _settings->GEN.usesyslog = value;
//             set.changed = true;
//             //DEBUGESPMANAGERF("[ESPmanager::handle()] settings->GEN.portal = %s\n", (command) ? "enabled" : "disabled");

//             sendsaveandreboot = true;
//         }

//     }

//     if ( request->hasParam(FPSTR(fstring_syslogIP), true)) {

//         IPAddress value;
//         bool result = value.fromString(request->getParam(FPSTR(fstring_syslogIP), true)->value()) ;

//         if (result && value != _settings->GEN.syslogIP) {
//             _settings->GEN.syslogIP = value;
//             set.changed = true;
//             sendsaveandreboot = true;
//         }

//     }

//     if ( request->hasParam(FPSTR(fstring_syslogPort), true)) {

//         int value = request->getParam(FPSTR(fstring_syslogPort), true)->value().toInt();
//         if (value != _settings->GEN.syslogPort) {
//             _settings->GEN.syslogPort = value;
//             set.changed = true;

//             sendsaveandreboot = true;
//         }
//     }

//     if ( request->hasParam(FPSTR(fstring_syslogProto), true)) {

//         int value = request->getParam(FPSTR(fstring_syslogProto), true)->value().toInt();
//         if (value != _settings->GEN.syslogProto) {
//             _settings->GEN.syslogProto = value;
//             set.changed = true;

//             sendsaveandreboot = true;
//         }

//     }


// #endif

    /*------------------------------------------------------------------------------------------------------------------

                                       AP reboot behaviour
       ------------------------------------------------------------------------------------------------------------------*/

    if (request->hasParam(FPSTR(fstring_ap_boot_mode), true) ) {

        int rebootvar = request->getParam(FPSTR(fstring_ap_boot_mode), true)->value().toInt();


        ap_boot_mode_t value = (ap_boot_mode_t)rebootvar;

        if (value != set.GEN.ap_boot_mode) {
            DEBUGESPMANAGERF("Recieved AP behaviour set to: %i\n", rebootvar);
            set.GEN.ap_boot_mode = value;
            _ap_boot_mode = value;
            set.changed = true;

        }
    }

    if (request->hasParam(FPSTR(fstring_no_sta_mode), true) ) {

        int var = request->getParam(FPSTR(fstring_no_sta_mode), true)->value().toInt();

        no_sta_mode_t value = (no_sta_mode_t)var;

        if (value != set.GEN.no_sta_mode) {

            DEBUGESPMANAGERF("Recieved WiFi Disconnect behaviour set to: %i\n", var);
            set.GEN.no_sta_mode = value;
            _no_sta_mode = value;
            set.changed = true;

        }
    }

    /*------------------------------------------------------------------------------------------------------------------

                                            New UPGRADE
       ------------------------------------------------------------------------------------------------------------------*/


    if (request->hasParam(FPSTR(fstring_updateURL), true) ) {

        String newpath = request->getParam(FPSTR(fstring_updateURL), true)->value();

        DEBUGESPMANAGERF("UpgradeURL: %s\n", newpath.c_str() );

        if (newpath && newpath.length() && set.GEN.updateURL != newpath ) {

            set.GEN.updateURL = newpath;
            set.changed = true;
        }
    }

    if (request->hasParam(FPSTR(fstring_updateFreq), true) ) {

        uint updateFreq = request->getParam(FPSTR(fstring_updateFreq), true)->value().toInt();

        if (updateFreq < 0) {
            updateFreq = 0;
        }

        if (updateFreq != set.GEN.updateFreq) {
            set.GEN.updateFreq = updateFreq;
            set.changed = true;
        }


    }

    if (request->hasParam(F("PerformUpdate"), true) ) {

        String path = String();

        if (set.GEN.updateURL) {
            path = set.GEN.updateURL;
        }

        _tasker.add( [this, path](Task & t) {
            _upgrade(path.c_str());
        });

    }

    root[FPSTR(fstring_changed)] = (set.changed) ? true : false;
    root[F("heap")] = ESP.getFreeHeap();


#ifdef ESPMANAGER_GIT_TAG
    root[F("espmanagergittag")] = ESPMANAGER_GIT_TAG;
#endif

    if (_fs.exists("/crashlog.txt")) {
        root[F("crashlog")] = true;
    } else {
        root[F("crashlog")] = false;
    }

    sendJsontoHTTP<JsonObject>(root, request);

    if (sendsaveandreboot) {
        event_send("", FPSTR(fstring_saveandreboot ));
    }

}


ESPMAN_ERR_t ESPmanager::_initialiseAP(bool override)
{
    using namespace ESPMAN;
    //int ERROR = 0;

    //  get the settings from SPIFFS if settings PTR is null
    if (!_settings) {
        _getAllSettings();
    }

    _settings->AP.ssid = _settings->GEN.host;

    //  return error code if override is false
    if (override) {
        DEBUGESPMANAGERF("**** OVERRIDING AP SETTINGS AND TURNING AP ON!! ****\n");
        _settings->AP.enabled = true;
    }

    return _initialiseAP(_settings->AP);

}

ESPMAN_ERR_t ESPmanager::_initialiseAP( settings_t::AP_t & settings )
{
    using namespace ESPMAN;


#ifdef DEBUGESPMANAGER
    DEBUGESPMANAGERF("-------  PRE CONFIG ------\n");
    _dumpAP(settings);
    DEBUGESPMANAGERF("--------------------------\n");
#endif

    if (settings.enabled == false  ) {
        DEBUGESPMANAGERF("AP DISABLED\n");
        if (WiFi.enableAP(false)) {
            return AP_DISABLED;
        } else {
            return ERROR_DISABLING_AP;
        }
    }

    //settings.channel = 1;


    if (settings.hasMAC) {
        bool result = wifi_set_macaddr(0x01, (unsigned char*)&settings.MAC);

        if (!result) {
            return ERROR_SETTING_MAC;
        }

    }

    if (!WiFi.enableAP(true)) {
        return ERROR_ENABLING_AP;
    }

    if (settings.hasConfig) {

        bool result =  WiFi.softAPConfig( settings.IP, settings.GW, settings.SN);

        if (!result) {
            return ERROR_SETTING_CONFIG;
        }
    }



    if (!settings.ssid) {
        char buf[33] = {'\0'};
        snprintf_P(&buf[0], 32, PSTR("esp8266-%06x"), ESP.getChipId());
        settings.ssid = buf;
    }

    DEBUGESPMANAGERF("ENABLING AP : channel %u, name %s, channel = %u, hidden = %u, pass = %s \n", settings.channel, settings.ssid.c_str(), settings.channel , !settings.visible , settings.pass.c_str() );


    if (!WiFi.softAP(settings.ssid.c_str(), (settings.pass) ? settings.pass.c_str() : nullptr , settings.channel, !settings.visible )) {
        return ERROR_ENABLING_AP;
    }


    return SUCCESS;

}


/*


      STA  stuff


 */

ESPMAN_ERR_t ESPmanager::_initialiseSTA()
{
    using namespace ESPMAN;
    ESPMAN_ERR_t ERROR = SUCCESS;

    if (!_settings) {
        _getAllSettings();
    }
    


    if (_settings) {
        ERROR = _initialiseSTA(_settings->STA);
        if (!ERROR) {
            if ( _settings->GEN.host && !WiFi.hostname( _settings->GEN.host.c_str() ) ) {
                DEBUGESPMANAGERF("ERROR setting Hostname\n");
            } else {
                DEBUGESPMANAGERF("Hostname set : %s\n", _settings->GEN.host.c_str() );
            }
            DEBUGESPMANAGERF("IP = %s\n", WiFi.localIP().toString().c_str() );
            return SUCCESS;
        } else {
            return ERROR;
        }
    } else {
        return MALLOC_FAIL;
    }

}

ESPMAN_ERR_t ESPmanager::_initialiseSTA( settings_t::STA_t & set)
{
    using namespace ESPMAN;
    //ESPMAN_ERR_t ERROR = SUCCESS;
    bool portal_enabled = _dns;

    if (portal_enabled) {
        disablePortal();
    }

#ifdef DEBUGESPMANAGER
    DEBUGESPMANAGERF("-------  PRE CONFIG ------\n");
    _dumpSTA(set);
    DEBUGESPMANAGERF("--------------------------\n");
#endif

    if (!set.enabled) {
        if (WiFi.enableSTA(false)) {
            return STA_DISABLED;
        } else {
            return ERROR_DISABLING_STA;
        }
    }

    if (!set.ssid) {
        return NO_STA_SSID;
    }

    if (set.hasMAC) {
        bool result = wifi_set_macaddr(0x00, (unsigned char *)&set.MAC);

        if (!result) {
            return ERROR_SETTING_MAC;
        }

    }


    if (!WiFi.enableSTA(true)) {
        return ERROR_ENABLING_STA;
    }

    //     WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3

    // if (WiFi.getMode() == WIFI_AP_STA) {
    //     APchannel = WiFi.softAPgetStationNum();

    // }


    if ( set.hasConfig && set.IP != INADDR_NONE && set.GW != INADDR_NONE  && set.SN != INADDR_NONE  ) {
        //      bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = (uint32_t)0x00000000, IPAddress dns2 = (uint32_t)0x00000000);
        DEBUGESPMANAGERF("IP %s\n", set.IP.toString().c_str() );
        DEBUGESPMANAGERF("GW %s\n", set.GW.toString().c_str());
        DEBUGESPMANAGERF("SN %s\n", set.SN.toString().c_str());
        DEBUGESPMANAGERF("DNS1 %s\n", set.DNS1.toString().c_str());
        DEBUGESPMANAGERF("DNS2 %s\n", set.DNS2.toString().c_str());

        WiFi.begin();

        // check if they are valid...
        if (!WiFi.config( set.IP, set.GW, set.SN, set.DNS1, set.DNS2))
            //if (!WiFi.config( settings.IP, settings.GW, settings.SN ))
        {
            return WIFI_CONFIG_ERROR;
        } else {
            set.dhcp = false;
            DEBUGESPMANAGERF("Config Applied\n");
        }

    } else {
        set.dhcp = true;
        //WiFi.config( INADDR_NONE, INADDR_NONE, INADDR_NONE);
        WiFi.config( INADDR_ANY, INADDR_ANY, INADDR_ANY);

    }



    if (set.autoConnect) {
        if (!WiFi.setAutoConnect(true)) {
            return FAILED_SET_AUTOCONNECT;
        }
    } else {
        if (!WiFi.setAutoConnect(false)) {
            return FAILED_SET_AUTOCONNECT;
        }
    }

    if (set.autoReconnect) {
        if (!WiFi.setAutoReconnect(true)) {
            return FAILED_SET_AUTORECONNECT;
        }
    } else {
        if (!WiFi.setAutoReconnect(false)) {
            return FAILED_SET_AUTORECONNECT;
        }
    }


    // wl_status_t begin(char* ssid, char *passphrase = NULL, int32_t channel = 0, const uint8_t* bssid = NULL, bool connect = true);
    // Serial.println("-------  POST CONFIG ------");
    // WiFi.printDiag(Serial);

    if (WiFi.isConnected() && WiFi.SSID() == set.ssid && WiFi.psk() == set.pass  ) {
        DEBUGESPMANAGERF( "Reconnecting WiFi... \n" );
        return NO_CHANGES;
    } else if ( WiFi.isConnected() && WiFi.SSID() == set.ssid ) {
        DEBUGESPMANAGERF( "Already connected to this network... \n" );
        //WiFi.reconnect();
        return NO_CHANGES;
    } else {

        //  this appears to be a work around for a corrupted flash causing endless boot loops.  https://github.com/esp8266/Arduino/issues/1997 
        //  https://github.com/esp8266/Arduino/pull/6965
        
        WiFi.persistent(false);
        WiFi.disconnect(true);

        if (  set.ssid && set.pass  ) {
            DEBUGESPMANAGERF( "Using ssid = %s, pass = %s\n", set.ssid.c_str(), set.pass.c_str()  );
            if (!WiFi.begin( set.ssid.c_str(), set.pass.c_str())) {    
                return ERROR_WIFI_BEGIN;
            }
        } else if ( set.ssid ) {
            DEBUGESPMANAGERF( "Using ssid = %s\n", set.ssid.c_str());
            if (!WiFi.begin( set.ssid.c_str())) {
                return ERROR_WIFI_BEGIN;
            }
        }
    }

    DEBUGESPMANAGERF("Begin Done: Now Connecting\n");

    uint32_t start_time = millis();

    uint8_t result = WL_DISCONNECTED;

    while (result = WiFi.waitForConnectResult(), result != WL_CONNECTED) {
        delay(10);
        if (millis() - start_time > 30000) {
            DEBUGESPMANAGERF("ABORTING CONNECTION TIMEOUT\n");
            result = CONNECT_FAILED;
            break;

        }
    }

    if (portal_enabled) {
        enablePortal();
    }

    DEBUGESPMANAGERF("connRes = %u, time = %ums\n", result, millis() - start_time);

    if ( result == WL_CONNECTED ) {

        return SUCCESS;
    }

    return CONNECT_FAILED;


}

// #ifdef ESPMANAGER_SYSLOG

// /**
//  * Syslog:  Send msg to configured syslog server.
//  * @param [msg] message
//  * @return bool
//  * @warning not implemented
//  */
// bool ESPmanager::log(myString msg)
// {
//     if (_syslog) {
//         return _syslog->log(std::move(msg) );
//     }
//     return false;
// }

// /**
//  * @param [pri] priority
//  * @param [msg] message
//  * @return bool
//  * @warning not implemented
//  */
// bool ESPmanager::log(uint16_t pri, myString  msg)
// {
//     if (_syslog) {
//         return _syslog->log(pri, std::move(msg) );
//     }
//     return false;
// }

// /**
//  * @param
//  * @param
//  * @return bool
//  * @warning not implemented
//  */
// bool ESPmanager::log(myString appName, myString  msg)
// {
//     if (_syslog) {
//         return _syslog->log( std::move(appName), std::move(msg) );
//     }
//     return false;
// }
// /**
//  * @param
//  * @param
//  * @param
//  * @return bool
//  * @warning not implemented
//  */
// bool ESPmanager::log(uint16_t pri, myString appName, myString  msg)
// {
//     if (_syslog) {
//         return _syslog->log(pri, std::move(appName), std::move(msg));
//     }
//     return false;
// }

// /**
//  * @param
//  * @param
//  * @warning not implemented
//  */
// void ESPmanager::_log(uint16_t pri, myString  msg)
// {
//     log(pri, msg);
//     event_send( F("LOG"), myStringf( F("[%3u] %s"), pri, msg.c_str()));

// }


// #endif



//  allows creating of a seperate config
//  need to add in captive portal to setttings....
ESPMAN_ERR_t ESPmanager::_emergencyMode(bool shutdown, int channel)
{
    using namespace ESPMAN;
    DEBUGESPMANAGERF("***** EMERGENCY mode **** \n");

    if (channel == -1) {
        channel = WiFi.channel();
        channel = 1;
    }

    if (shutdown) {
        WiFi.disconnect(true); //  Disable STA. makes AP more stable, stops 1sec reconnect
    }

    _APtimer = millis();

    //  creats a copy of settings so they are not changed...
    settings_t set;
    _getAllSettings(set);

    set.AP.ssid = set.GEN.host;
    set.AP.channel = channel;

    if (!set.AP.pass && set.STA.pass) {
        set.AP.pass = set.STA.pass;
    } else if (!set.AP.pass) {
        set.AP.pass = F(DEFAULT_AP_PASS);
    }

    // if (set.GEN.usePerminantSettings && _perminant_host) {
    //     set.AP.ssid = _perminant_host;
    // }

    set.AP.enabled = true;

    DEBUGESPMANAGERF("*****  Debug:  WiFi channel in EMERGENCY mode = %u\n", set.AP.channel);

    return _initialiseAP(set.AP);


}


ESPMAN_ERR_t ESPmanager::_getAllSettings()
{

    using namespace ESPMAN;


    if (!_settings) {
        _settings = new settings_t;
    }

    if (!_settings) {
        return MALLOC_FAIL;
    }

    if (_settings->changed) {
        return SUCCESS; // dont overwrite changes already in memory...
    }

    ESPMAN_ERR_t ERROR = SUCCESS;

    

    ERROR =  _getAllSettings(*_settings);



    if (!ERROR) {

        _ap_boot_mode = _settings->GEN.ap_boot_mode;
        _no_sta_mode = _settings->GEN.no_sta_mode;
        //_updateFreq = _settings->GEN.updateFreq;
        //_OTAupload = _settings->GEN.OTAupload;

        _settings->configured = true;
        //DEBUGESPMANAGERF("[ESPmanager::_getAllSettings()] _ap_boot_mode = %i, _no_sta_mode = %i, _updateFreq = %u, IDEupload = %s\n", (int)_ap_boot_mode, (int)_no_sta_mode, _updateFreq, (_OTAupload) ? "enabled" : "disabled" );
    } else {
        _settings->configured = false;
    }

    //_applyPermenent(*settings);

    if (!_settings->GEN.host.length()) {
        char tmp[33] = {'\0'};
        snprintf(tmp, 32, "esp8266-%06x", ESP.getChipId());
        _settings->GEN.host = tmp;
    }

    return ERROR;


}

ESPMAN_ERR_t ESPmanager::_getAllSettings(settings_t & set)
{

    using namespace ESPMAN;
    //DynamicJsonDocument json(ESP.getMaxFreeBlockSize() - 512);
    DynamicJsonDocument json(allocateJSON()); 
    uint8_t settingsversion = 0;
    uint32_t start_heap = ESP.getFreeHeap();

    ESPMAN_ERR_t ERROR = SUCCESS;

    File f = _fs.open(SETTINGS_FILE, "r");
    int totalBytes = f.size();

    if (!f) {
        return FS_FILE_OPEN_ERROR;
    }


    auto jsonError = deserializeJson(json, f); 

    // ERROR = static_cast<ESPMAN_ERR_t> (json.parseSPIFS(SETTINGS_FILE));

    if (jsonError != DeserializationError::Ok) {
        return ERROR;
    }

    json.shrinkToFit();

    JsonObject root = json.as<JsonObject>();

    /*
          General Settings
     */


    if (root.containsKey(FPSTR(fstring_General))) {

        JsonObject settingsJSON = root[FPSTR(fstring_General)];

        if (settingsJSON.containsKey(FPSTR(fstring_settingsversion))) {
            settingsversion = settingsJSON[FPSTR(fstring_settingsversion)].as<uint8_t>();
        }

        if (settingsJSON.containsKey(FPSTR(fstring_host))) {
            set.GEN.host = settingsJSON[FPSTR(fstring_host)].as<const char *>();
        }

        if (settingsJSON.containsKey(FPSTR(fstring_mDNS))) {
            set.GEN.mDNSenabled = settingsJSON[FPSTR(fstring_mDNS)];
        }

        if (settingsJSON.containsKey(FPSTR(fstring_updateURL))) {
            set.GEN.updateURL = settingsJSON[FPSTR(fstring_updateURL)].as<const char *>();
        }

        if (settingsJSON.containsKey(FPSTR(fstring_updateFreq))) {
            set.GEN.updateFreq = settingsJSON[FPSTR(fstring_updateFreq)];
        }

        if (settingsJSON.containsKey(FPSTR(fstring_OTApassword))) {
            set.GEN.OTApassword = settingsJSON[FPSTR(fstring_OTApassword)].as<const char *>();
        }

        // if (settingsJSON.containsKey(string_GUIusername)) {
        //         if (set.GEN.GUIusername ) { free(set.GEN.GUIusername); };
        //         set.GEN.GUIusername = strdup(settingsJSON[string_GUIusername]);
        // }
        //
        // if (settingsJSON.containsKey(string_GUIpassword)) {
        //         if (set.GEN.GUIpassword) { free(set.GEN.GUIpassword); };
        //         set.GEN.GUIpassword = strdup(settingsJSON[string_GUIpassword]);
        // }
        if (settingsJSON.containsKey(FPSTR(fstring_GUIhash))) {
            set.GEN.GUIhash = settingsJSON[FPSTR(fstring_GUIhash)].as<const char *>();
        }

        // if (settingsJSON.containsKey(string_usePerminantSettings)) {
        //     set.GEN.usePerminantSettings = settingsJSON[string_usePerminantSettings];
        // } else if ( _perminant_host || _perminant_ssid || _perminant_pass ) {
        //     set.GEN.usePerminantSettings = true;
        // } else {
        //     set.GEN.usePerminantSettings = false;
        // }

        if (settingsJSON.containsKey(FPSTR(fstring_ap_boot_mode))) {

            int val = settingsJSON[FPSTR(fstring_ap_boot_mode)];
            set.GEN.ap_boot_mode = (ap_boot_mode_t)val;
            //DEBUGESPMANAGERF("[_getAllSettings] set.GEN.ap_boot_mode = %i\n", val);
        }
        if (settingsJSON.containsKey(FPSTR(fstring_no_sta_mode))) {
            int val = settingsJSON[FPSTR(fstring_no_sta_mode)];
            set.GEN.no_sta_mode = (no_sta_mode_t)val;
            //DEBUGESPMANAGERF("[_getAllSettings] set.GEN.no_sta_mode = %i\n", val);
        }

        if (settingsJSON.containsKey(FPSTR(fstring_OTAupload))) {
            set.GEN.OTAupload = settingsJSON[FPSTR(fstring_OTAupload)];
        }

// #ifdef ESPMANAGER_SYSLOG

//         if (settingsJSON.containsKey( FPSTR(fstring_usesyslog))) {

//             set.GEN.usesyslog = settingsJSON[FPSTR(fstring_usesyslog)];

//             if (set.GEN.usesyslog) {

//                 if (settingsJSON.containsKey(FPSTR(fstring_syslogIP)) &&  settingsJSON.containsKey(FPSTR(fstring_syslogPort)) ) {

//                     set.GEN.syslogPort = settingsJSON[FPSTR(fstring_syslogPort)];

//                     for (uint8_t i = 0; i < 4; i++) {
//                         set.GEN.syslogIP[i] = settingsJSON[ FPSTR(fstring_syslogIP)][i];
//                     }
//                 }

//                 if (settingsJSON.containsKey(FPSTR(fstring_syslogProto))) {

//                     set.GEN.syslogProto = settingsJSON[FPSTR(fstring_syslogProto)];
//                 }



//             }
//         }

// #endif

    } 

    /*
           STA settings
     */

    if (root.containsKey(FPSTR(fstring_STA))) {


        JsonObject STAjson = root[FPSTR(fstring_STA)];

        if (STAjson.containsKey(FPSTR(fstring_enabled))) {
            set.STA.enabled = STAjson[FPSTR(fstring_enabled)];
        }

        if (STAjson.containsKey(FPSTR(fstring_ssid))) {
            if (strnlen(STAjson[FPSTR(fstring_ssid)], 100) < MAX_SSID_LENGTH) {

                set.STA.ssid = STAjson[FPSTR(fstring_ssid)].as<const char *>();
                //strncpy( &settings.ssid[0], STAjson["ssid"], strlen(STAjson["ssid"]) );
            }
        }

        if (STAjson.containsKey(FPSTR(fstring_pass))) {
            if (strnlen(STAjson[FPSTR(fstring_pass)], 100) < MAX_PASS_LENGTH) {
                set.STA.pass = STAjson[FPSTR(fstring_pass)].as<const char *>();
                //strncpy( &settings.pass[0], STAjson["pass"], strlen(STAjson["pass"]) );
            }
        }

        if (STAjson.containsKey(FPSTR(fstring_IP)) && STAjson.containsKey(FPSTR(fstring_GW)) && STAjson.containsKey(FPSTR(fstring_SN)) && STAjson.containsKey(FPSTR(fstring_DNS1))) {
            //set.STA.hasConfig = true;
            set.STA.IP = IPAddress( STAjson[FPSTR(fstring_IP)][0], STAjson[FPSTR(fstring_IP)][1], STAjson[FPSTR(fstring_IP)][2], STAjson[FPSTR(fstring_IP) ][3] );
            set.STA.GW = IPAddress( STAjson[FPSTR(fstring_GW)][0], STAjson[FPSTR(fstring_GW)][1], STAjson[FPSTR(fstring_GW)][2], STAjson[FPSTR(fstring_GW) ][3] );
            set.STA.SN = IPAddress( STAjson[FPSTR(fstring_SN)][0], STAjson[FPSTR(fstring_SN)][1], STAjson[FPSTR(fstring_SN)][2], STAjson[FPSTR(fstring_SN) ][3] );
            set.STA.DNS1 = IPAddress( STAjson[FPSTR(fstring_DNS1)][0], STAjson[FPSTR(fstring_DNS1)][1], STAjson[FPSTR(fstring_DNS1)][2], STAjson[ FPSTR(fstring_DNS1) ][3] );

            if ( STAjson.containsKey(FPSTR(fstring_DNS2))) {
                set.STA.DNS2 = IPAddress( STAjson[FPSTR(fstring_DNS2)][0], STAjson[FPSTR(fstring_DNS2)][1], STAjson[FPSTR(fstring_DNS2)][2], STAjson[FPSTR(fstring_DNS2)][3] );
            }

            if (set.STA.IP == INADDR_NONE) {
                set.STA.dhcp = true;
                set.STA.hasConfig = false;
            } else {
                set.STA.dhcp = false;
                set.STA.hasConfig = true;
            }

        } else {
            set.STA.dhcp = true;
            set.STA.hasConfig = false;
        }

        if (STAjson.containsKey(FPSTR(fstring_autoconnect))) {
            set.STA.autoConnect = STAjson[FPSTR(fstring_autoconnect)];
        }

        if (STAjson.containsKey(FPSTR(fstring_autoreconnect))) {
            set.STA.autoReconnect = STAjson[FPSTR(fstring_autoreconnect)];
        }

        if (STAjson.containsKey(FPSTR(fstring_MAC))) {

            set.STA.hasMAC = true;

            for (uint8_t i = 0; i < 6; i++) {
                set.STA.MAC[i] = STAjson[FPSTR(fstring_MAC)][i];
            }

        }

    }


    /*
           AP settings
     */
    if (root.containsKey(FPSTR(fstring_AP))) {

        JsonObject APjson = root[FPSTR(fstring_AP)];

        if (APjson.containsKey(FPSTR(fstring_enabled))) {
            set.AP.enabled = APjson[FPSTR(fstring_enabled)];
            //Serial.printf("set.AP.enabled = %s\n", (set.AP.enabled)? "true": "false");
        }

        if (APjson.containsKey( FPSTR(fstring_pass))) {
            //settings.hasPass = true;
            if (strnlen(APjson[FPSTR(fstring_pass)], 100) < MAX_PASS_LENGTH) {

                set.AP.pass = APjson[FPSTR(fstring_pass)].as<const char *>();
            }
        }

        if (APjson.containsKey(FPSTR(fstring_IP))) {
            set.STA.hasConfig = true;
            set.STA.IP = IPAddress( APjson[FPSTR(fstring_IP)][0], APjson[FPSTR(fstring_IP)][1], APjson[FPSTR(fstring_IP)][2], APjson[FPSTR(fstring_IP)][3] );
            set.STA.GW = IPAddress( APjson[FPSTR(fstring_GW)][0], APjson[FPSTR(fstring_GW)][1], APjson[FPSTR(fstring_GW)][2], APjson[FPSTR(fstring_GW)][3] );
            set.STA.SN = IPAddress( APjson[FPSTR(fstring_SN)][0], APjson[FPSTR(fstring_SN)][1], APjson[FPSTR(fstring_SN)][2], APjson[FPSTR(fstring_SN)][3] );


        }

        if (APjson.containsKey(FPSTR(fstring_visible))) {
            set.AP.visible = true;
        }

        if (APjson.containsKey(FPSTR(fstring_channel))) {
            set.AP.channel = APjson[FPSTR(fstring_channel)];
        }

        if (APjson.containsKey(FPSTR(fstring_MAC))) {

            set.AP.hasMAC = true;

            for (uint8_t i = 0; i < 6; i++) {
                set.AP.MAC[i] = APjson[FPSTR(fstring_MAC)][i];
            }

        }

    }

    if (settingsversion != SETTINGS_FILE_VERSION) {
        DEBUGESPMANAGERF("Settings File Version Wrong expecting v%u got v%u\n", SETTINGS_FILE_VERSION, settingsversion);
        // serializeJsonPretty(settingsJSON, Serial);
        // Serial.println();
        return WRONG_SETTINGS_FILE_VERSION;
    }

    return SUCCESS;

}

ESPMAN_ERR_t ESPmanager::_saveAllSettings(settings_t & set)
{

    using namespace ESPMAN;

    DynamicJsonDocument jsonBuffer(allocateJSON());
    JsonObject root = jsonBuffer.to<JsonObject>();

    /*
            General Settings
     */
    JsonObject settingsJSON = root.createNestedObject(FPSTR(fstring_General));

    settingsJSON[FPSTR(fstring_mDNS)] = set.GEN.mDNSenabled;

    settingsJSON[FPSTR(fstring_settingsversion)] = SETTINGS_FILE_VERSION;

    if (set.GEN.host) {
        settingsJSON[FPSTR(fstring_host)] = set.GEN.host;
    }

    if (set.GEN.updateURL) {
        settingsJSON[FPSTR(fstring_updateURL)] = set.GEN.updateURL;
    }

    settingsJSON[FPSTR(fstring_updateFreq)] = set.GEN.updateFreq;

    if (set.GEN.OTApassword) {
        settingsJSON[FPSTR(fstring_OTApassword)] = set.GEN.OTApassword;
    }

    // if (set.GEN.GUIusername) {
    //         settingsJSON[string_GUIusername] = set.GEN.GUIusername;
    // }
    //
    // if (set.GEN.GUIpassword) {
    //         settingsJSON[string_GUIpassword] = set.GEN.GUIpassword;
    // }

    if (set.GEN.GUIhash) {
        settingsJSON[FPSTR(fstring_GUIhash)] = set.GEN.GUIhash;
    }

    settingsJSON[FPSTR(fstring_usesyslog)] = set.GEN.usesyslog;

    if (set.GEN.usesyslog) {
        JsonArray IP = settingsJSON.createNestedArray(FPSTR(fstring_syslogIP));
        IP.add(set.GEN.syslogIP[0]);
        IP.add(set.GEN.syslogIP[1]);
        IP.add(set.GEN.syslogIP[2]);
        IP.add(set.GEN.syslogIP[3]);
        settingsJSON[FPSTR(fstring_syslogPort)] = set.GEN.syslogPort;

        settingsJSON[FPSTR(fstring_syslogProto)] = set.GEN.syslogProto;

    }

    settingsJSON[FPSTR(fstring_ap_boot_mode)] = (int)set.GEN.ap_boot_mode;
    settingsJSON[FPSTR(fstring_no_sta_mode)] = (int)set.GEN.no_sta_mode;
    settingsJSON[FPSTR(fstring_OTAupload)] = set.GEN.OTAupload;

    /*****************************************
            STA Settings
    *****************************************/

    JsonObject STAjson = root.createNestedObject(FPSTR(fstring_STA));

    STAjson[FPSTR(fstring_enabled)] = set.STA.enabled;

    if (set.STA.ssid) {
        STAjson[FPSTR(fstring_ssid)] = set.STA.ssid;
    }

    if (set.STA.pass) {
        STAjson[FPSTR(fstring_pass)] = set.STA.pass;

    }

    if (set.STA.hasConfig) {

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

        if (set.STA.DNS2 != INADDR_NONE) {
            JsonArray DNS2 = STAjson.createNestedArray(FPSTR(fstring_DNS2));
            DNS2.add(set.STA.DNS2[0]);
            DNS2.add(set.STA.DNS2[1]);
            DNS2.add(set.STA.DNS2[2]);
            DNS2.add(set.STA.DNS2[3]);

        }


    }

    if (set.STA.hasMAC) {
        JsonArray MAC = STAjson.createNestedArray(FPSTR(fstring_MAC));

        for (uint8_t i = 0; i < 6; i++) {
            MAC.add(set.STA.MAC[i]);
        }

    }


    STAjson[FPSTR(fstring_autoconnect)] = set.STA.autoConnect;
    STAjson[FPSTR(fstring_autoreconnect)] = set.STA.autoReconnect;



    /****************************************
            AP Settings
    ****************************************/

    JsonObject APjson = root.createNestedObject(FPSTR(fstring_AP));

    APjson[FPSTR(fstring_enabled)] = set.AP.enabled;

    //  disbale this for now.. all set via host.
    //
    // if (set.AP.ssid()) {
    //         APjson[string_ssid] = set.AP.ssid();
    // }

    if (set.AP.pass) {
        APjson[FPSTR(fstring_pass)] = set.AP.pass;

    }

    if (set.AP.hasConfig) {

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

    if (set.AP.hasMAC) {
        JsonArray MAC = APjson.createNestedArray(FPSTR(fstring_MAC));

        for (uint8_t i = 0; i < 6; i++) {
            MAC.add(set.AP.MAC[i]);
        }

    }

    APjson[FPSTR(fstring_visible)] = set.AP.visible;
    APjson[FPSTR(fstring_channel)] = set.AP.channel;

    File f = _fs.open(SETTINGS_FILE, "w");

    if (!f) {
        return FS_FILE_OPEN_ERROR;
    }

    //root.prettyPrintTo(f);
    serializeJsonPretty(root, f); 
    f.close();
    return SUCCESS;

}



#ifdef DEBUGESPMANAGER

void ESPmanager::_dumpGEN(settings_t::GEN_t & settings)
{

    DEBUGESPMANAGERFRAW("---- GEN ----\n");
    DEBUGESPMANAGERFRAW("host = %s\n", (settings.host) ? settings.host.c_str() : "null" );
    DEBUGESPMANAGERFRAW("updateURL = %s\n", (settings.updateURL) ? settings.updateURL.c_str() : "null" );
    DEBUGESPMANAGERFRAW("updateFreq = %u\n", (uint32_t)settings.updateFreq );
    DEBUGESPMANAGERFRAW("OTAport = %u\n", (uint32_t)settings.OTAport );
    DEBUGESPMANAGERFRAW("mDNSenabled = %s\n", (settings.mDNSenabled) ? "true" : "false" );

    DEBUGESPMANAGERFRAW("OTApassword = %s\n", (settings.OTApassword.c_str()) ? settings.OTApassword.c_str() : "null" );
    DEBUGESPMANAGERFRAW("GUIhash = %s\n", (settings.GUIhash.c_str()) ? settings.GUIhash.c_str() : "null" );
    DEBUGESPMANAGERFRAW("ap_boot_mode = %i\n", (int8_t)settings.ap_boot_mode );
    DEBUGESPMANAGERFRAW("no_sta_mode = %i\n", (int8_t)settings.no_sta_mode );
    DEBUGESPMANAGERFRAW("IDEupload = %s\n", (settings.OTAupload) ? "true" : "false" );

// #ifdef ESPMANAGER_SYSLOG
//     DEBUGESPMANAGERFRAW("usesyslog = %s\n", (settings.usesyslog) ? "true" : "false" );
//     DEBUGESPMANAGERFRAW("syslogIP = %u.%u.%u.%u\n", settings.syslogIP[0], settings.syslogIP[1], settings.syslogIP[2], settings.syslogIP[3] );
//     DEBUGESPMANAGERFRAW("syslogPort = %u\n", settings.syslogPort );
//     DEBUGESPMANAGERFRAW("syslogProto = %u\n", settings.syslogProto );

// #endif

}


void ESPmanager::_dumpAP(settings_t::AP_t & settings)
{

    DEBUGESPMANAGERFRAW("---- AP ----\n");
    DEBUGESPMANAGERFRAW("enabled = %s\n", (settings.enabled) ? "true" : "false" );
    DEBUGESPMANAGERFRAW("ssid = %s\n", (settings.ssid) ? settings.ssid.c_str() : "null" );
    DEBUGESPMANAGERFRAW("pass = %s\n", (settings.pass) ? settings.pass.c_str() : "null" );
    DEBUGESPMANAGERFRAW("hasConfig = %s\n", (settings.hasConfig) ? "true" : "false" );
    DEBUGESPMANAGERFRAW("IP = %s\n", settings.IP.toString().c_str() );
    DEBUGESPMANAGERFRAW("GW = %s\n", settings.GW.toString().c_str() );
    DEBUGESPMANAGERFRAW("SN = %s\n", settings.SN.toString().c_str() );
    DEBUGESPMANAGERFRAW("visible = %s\n", (settings.visible) ? "true" : "false" );
    DEBUGESPMANAGERFRAW("channel = %u\n", settings.channel );


}

void ESPmanager::_dumpSTA(settings_t::STA_t & settings)
{

    DEBUGESPMANAGERFRAW("---- STA ----\n");
    DEBUGESPMANAGERFRAW("enabled = %s\n", (settings.enabled) ? "true" : "false" );
    DEBUGESPMANAGERFRAW("ssid = %s\n", (settings.ssid.c_str()) ? settings.ssid.c_str() : "null" );
    DEBUGESPMANAGERFRAW("pass = %s\n", (settings.pass.c_str()) ? settings.pass.c_str() : "null" );
    DEBUGESPMANAGERFRAW("dhcp = %s\n", (settings.dhcp) ? "true" : "false" );
    DEBUGESPMANAGERFRAW("hasConfig = %s\n", (settings.hasConfig) ? "true" : "false" );
    DEBUGESPMANAGERFRAW("IP = %s\n", settings.IP.toString().c_str() );
    DEBUGESPMANAGERFRAW("GW = %s\n", settings.GW.toString().c_str() );
    DEBUGESPMANAGERFRAW("SN = %s\n", settings.SN.toString().c_str() );
    DEBUGESPMANAGERFRAW("DNS1 = %s\n", settings.DNS1.toString().c_str() );
    DEBUGESPMANAGERFRAW("DNS2 = %s\n", settings.DNS2.toString().c_str() );
    DEBUGESPMANAGERFRAW("autoConnect = %s\n", (settings.autoConnect) ? "true" : "false" );
    DEBUGESPMANAGERFRAW("autoReconnect = %s\n", (settings.autoReconnect) ? "true" : "false" );

}


void ESPmanager::_dumpSettings()
{
    _getAllSettings();

    if (_settings) {
        DEBUGESPMANAGERFRAW(" IP Addr %u.%u.%u.%u\n", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
        DEBUGESPMANAGERFRAW("---- Settings ----\n");
        DEBUGESPMANAGERFRAW("configured = %s\n", (_settings->configured) ? "true" : "false" );
        DEBUGESPMANAGERFRAW("changed = %s\n", (_settings->changed) ? "true" : "false" );
        //DEBUGESPMANAGERF("usePerminantSettings = %s\n", (settings->GEN.usePerminantSettings) ? "true" : "false" );

        _dumpGEN(_settings->GEN);
        _dumpSTA(_settings->STA);
        _dumpAP(_settings->AP);

    }
}

#endif

/**
 *  Resets the ESP to a non-configured state.
 *  Erases the config file, and removes the wizard flag if it is there.
 */
void ESPmanager::factoryReset()
{
    DEBUGESPMANAGERF("FACTORY RESET\n");
    WiFi.disconnect();
    ESP.eraseConfig();
    _fs.remove(SETTINGS_FILE);
    _fs.remove("/.wizard");
}

void ESPmanager::_sendTextResponse(AsyncWebServerRequest * request, uint16_t code, const String & text)
{
    AsyncWebServerResponse *response = request->beginResponse(code, "text/plain", text.c_str() );
    response->addHeader(  FPSTR(ESPMAN::fstring_CORS)  , "*");
    response->addHeader(  FPSTR(ESPMAN::fstring_CACHE_CONTROL) , "no-store");
    request->send(response);
}

void ESPmanager::_removePreGzFiles()
{

    Dir dir = _fs.openDir("/");
    while (dir.next()) {
        String fileName = dir.fileName();

        if (fileName.endsWith(".gz")) {

            String withOutgz = fileName.substring(0, fileName.length() - 3 );

            if (_fs.exists(withOutgz)) {
                DEBUGESPMANAGERF("_removePreGzFiles() : Removing unzipped file %s\n", withOutgz.c_str());
                _fs.remove(withOutgz);
            }

        }

    }

}

/**
 * Returns error code as a String.
 * @param
 * @return
 */
const String ESPmanager::getError(ESPMAN_ERR_t err)
{
    switch (err) {
    case UNKNOWN_ERROR:
        return F("Unkown Error"); break;
    case NO_UPDATE_URL:
        return F("No Update Url"); break;
    case FS_FILES_ABSENT:
        return F("FS files missing"); break;
    case FILE_NOT_CHANGED:
        return F("File not changed"); break;
    case MD5_CHK_ERROR:
        return F("MD5 check Error"); break;
    case HTTP_ERROR:
        return F("HTTP error"); break;
    case JSON_PARSE_ERROR:
        return F("JSON parse ERROR"); break;
    case JSON_OBJECT_ERROR:
        return F("JSON Object ERROR"); break;
    case CONFIG_FILE_ERROR:
        return F("Config File ERROR"); break;
    case UPDATER_ERROR:
        return F("Updater ERROR"); break;
    case JSON_TOO_LARGE:
        return F("JSON too large"); break;
    case MALLOC_FAIL:
        return F("Malloc Fail"); break;
    case MANIFST_FILE_ERROR:
        return F("Manifest file ERROR"); break;
    case UNKNOWN_NUMBER_OF_FILES:
        return F("Unknown number of files"); break;
    case FS_INFO_FAIL:
        return F("FS info fail"); break;
    case FS_FILENAME_TOO_LONG:
        return F("Filename too long"); break;
    case FS_FILE_OPEN_ERROR:
        return F("FS file open ERROR"); break;
    case FILE_TOO_LARGE:
        return F("File too large"); break;
    case INCOMPLETE_DOWNLOAD:
        return F("Incomplete Download"); break;
    case CRC_ERROR:
        return F("CRC ERROR"); break;
    case JSON_KEY_MISSING:
        return F("JSON key missing"); break;
    case EMPTY_BUFFER:
        return F("Empty buffer"); break;
    case AP_DISABLED:
        return F("AP Disabled"); break;
    case ERROR_ENABLING_AP:
        return F("ERROR enabling AP"); break;
    case ERROR_DISABLING_AP:
        return F("ERROR disabling AP"); break;
    case ERROR_SETTING_CONFIG:
        return F("Settings Config ERROR"); break;
    case ERROR_ENABLING_STA:
        return F("ERROR enabling STA"); break;
    case FAILED_SET_AUTOCONNECT:
        return F("Failed to set Autoconnect"); break;
    case FAILED_SET_AUTORECONNECT:
        return F("Failed to set Autoreconnect"); break;
    case WIFI_CONFIG_ERROR:
        return F("WiFi config ERROR"); break;
    case NO_STA_SSID:
        return F("No SSID specified"); break;
    case ERROR_WIFI_BEGIN:
        return F("ERROR starting WiFi"); break;
    case NO_SSID_AVAIL:
        return F("SSID not available "); break;
    case CONNECT_FAILED:
        return F("Connect Failed"); break;
    case UNITITIALISED:
        return F("Uninitialised"); break;
    case ERROR_FS_MOUNT:
        return F("FS mount FAIL"); break;
    case AUTO_CONNECTED_STA:
        return F("Auto connected to STA"); break;
    case ERROR_DISABLING_STA:
        return F("ERROR disabling STA"); break;
    case STA_DISABLED:
        return F("STA disabled"); break;
    case SETTINGS_NOT_IN_MEMORY:
        return F("Settings not in memory"); break;
    case ERROR_SETTING_MAC:
        return F("ERROR setting MAC"); break;
    case PASSWORD_MISMATCH:
        return F("Password Mismatch"); break;
    case NO_CHANGES:
        return F("No Changes"); break;
    case PASSWOROD_INVALID:
        return F("Password invalid"); break;
    case WRONG_SETTINGS_FILE_VERSION:
        return F("Wrong Settings File Version"); break;
    default:
        return String(err); break;
    }
}



void ESPmanager::_populateFoundDevices(JsonObject & root)
{

#ifdef ESPMANAGER_DEVICEFINDER

    if (_devicefinder) {

        String host = getHostname();

        root[F("founddevices")] = _devicefinder->count();

        if (_devicefinder->count()) {
            JsonArray  devicelist = root.createNestedArray(F("devices"));
            JsonObject  listitem = devicelist.createNestedObject();
            listitem[F("name")] = host;
            listitem[F("IP")] = WiFi.localIP().toString();
            for (uint8_t i = 0; i < _devicefinder->count(); i++) {
                JsonObject listitem = devicelist.createNestedObject();
                const char * name = _devicefinder->getName(i);
                IPAddress IP = _devicefinder->getIP(i);
                listitem[F("name")] = name;
                listitem[F("IP")] = IP.toString();
                //DEBUGESPMANAGERF("Found [%s]@ %s\n", name, IP.toString().c_str());
            }
        } else {
            DEBUGESPMANAGERF("No Devices Found\n");
        }
    }

#endif

}


void ESPmanager::FSDirIterator(FS & fs, const char * dirName, std::function<void(File & f)> Cb )
{
    Dir dir = fs.openDir(dirName);
    while (dir.next()) {
      if (dir.isFile()) {
        File f =  dir.openFile("r");
        if (Cb) {
          Cb(f); 
        }
      } else {
        FSDirIterator(fs, dir.fileName().c_str() , Cb);
      }
    }
}



