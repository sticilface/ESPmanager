
#include "ESPmanager.h"

#include <WiFiUdp.h>
#include <ArduinoOTA.h>
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

extern "C" {
#include "user_interface.h"
}

extern UMM_HEAP_INFO ummHeapInfo;

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

ESPmanager::ESPmanager(
    AsyncWebServer & HTTP, FS & fs)
    : _HTTP(HTTP)
    , _fs(fs)
    , _events("/espman/events")
{

}

ESPmanager::~ESPmanager()
{


    if (_settings) {
        delete _settings;
    }
}

int ESPmanager::begin()
{
    using namespace std::placeholders;
    using namespace ESPMAN;

    bool wizard = false;


// #ifdef RANDOM_MANIFEST_ON_BOOT
//         _randomvalue = random(0,300000);
// #endif
    ESPMan_Debugln("Settings Manager V" ESPMANVERSION);
    // ESPMan_Debugf("REPO: %s\n",  slugTag );
    // ESPMan_Debugf("BRANCH: %s\n",  branchTag );
    // ESPMan_Debugf("BuildTag: %s\n",  buildTag );
    // ESPMan_Debugf("commitTag: %s\n",  commitTag );
    //
    ESPMan_Debugf("True Sketch Size: %u\n",  trueSketchSize() );
    ESPMan_Debugf("Sketch MD5: %s\n",  getSketchMD5().c_str() );
    ESPMan_Debugf("Device MAC: %s\n", WiFi.macAddress().c_str() );

    wifi_set_sleep_type(NONE_SLEEP_T); // workaround no modem sleep.

    if (!_fs.begin()) {
        return ERROR_SPIFFS_MOUNT;
    }


#ifdef Debug_ESPManager

    Debug_ESPManager.println("SPIFFS FILES:");
    {
        Dir dir = _fs.openDir("/");
        while (dir.next()) {
            String fileName = dir.fileName();
            size_t fileSize = dir.fileSize();
            Debug_ESPManager.printf("     FS File: %s\n", fileName.c_str());
        }
        Debug_ESPManager.printf("\n");
    }

    File f = _fs.open(SETTINGS_FILE, "r");
    Debug_ESPManager.printf("ESP MANAGER Settings [%u]B:\n", f.size());
    if (f) {
        for (int i = 0; i < f.size(); i++) {
            Debug_ESPManager.write(f.read());
        }
        Debug_ESPManager.println();
        f.close();
    }

#endif

    //_removePreGzFiles();

    int getallERROR = _getAllSettings();
    ESPMan_Debugf("[ESPmanager::begin()] _getAllSettings = %i \n", getallERROR);

    if (!_settings) {
        ESPMan_Debugf("[ESPmanager::begin()] Unable to MALLOC for settings. rebooting....\n");
        ESP.restart();
    }

    if (getallERROR == SPIFFS_FILE_OPEN_ERROR) {

        ESPMan_Debugf("[ESPmanager::begin()] no settings file found\n");

        _settings->changed = true; //  give save button at first boot if no settings file

    }

    // if (getallERROR) {
    //     ESPMan_Debugf("[ESPmanager::begin()] ERROR -> configured = false\n");
    //     settings->configured = false;
    // } else {
    //     settings->configured = true;
    // }

    if (!_settings->GEN.host()) {
        ESPMan_Debugf("[ESPmanager::begin()] Host NOT SET\n");
        char tmp[33] = {'\0'};
        snprintf(tmp, 32, "esp8266-%06x", ESP.getChipId());
        _settings->GEN.host = tmp;
    }
    //
    //
    // ESPMan_Debugf("[ESPmanager::begin()] host = %s\n", _GEN_SETTINGS.host );
    //
    int AP_ERROR = 0;
    int STA_ERROR = 0;
    int AUTO_ERROR = 0;

    WiFi.hostname(_settings->GEN.host());


    if (_fs.exists("/.wizard")) {

        File f = _fs.open("/.wizard", "r");

        if (f && f.size() == sizeof(settings_t::AP_t)) {
            wizard = true;
            ESPMan_Debugf("*** WIZARD MODE ENABLED ***\n");
            settings_t::AP_t ap;

            uint8_t * data = static_cast<uint8_t*>(static_cast<void*>(&ap));

            for (int i = 0; i < sizeof(ap); i++) {
                data[i] = f.read();
            }

            ap.enabled = true;
            ap.ssid = _settings->GEN.host;
            ap.channel = WiFi.channel();
            if (!_initialiseAP(ap)) {
                ESPMan_Debugf("AP re started\n");
                if (_settings->GEN.portal) {
                    enablePortal();

                }
            }
        }



    } else if (_settings->configured) {



        ESPMan_Debugf("[ESPmanager::begin()] settings->configured = true \n");
        AP_ERROR = _initialiseAP();
        ESPMan_Debugf("[ESPmanager::begin()] _initialiseAP = %i \n", AP_ERROR);
        STA_ERROR = _initialiseSTA();
        ESPMan_Debugf("[ESPmanager::begin()] _initialiseSTA = %i \n", STA_ERROR);

    } else {

        //  enable emergency mode and portal....

        ESP.eraseConfig(); //  clear everything when starting for first time...
        WiFi.mode(WIFI_OFF);

        _emergencyMode(true);

        if (_settings->GEN.portal) {
            enablePortal();
        } else {
            ESPMan_Debugf("[ESPmanager::enablePortal] Portal DISABLED\n");

        }


    }

    if ( (STA_ERROR && STA_ERROR != STA_DISABLED) ||  (AP_ERROR == AP_DISABLED && STA_ERROR == STA_DISABLED ) ) {
        if (_ap_boot_mode != DISABLED) {
            _APenabledAtBoot = true;
            ESPMan_Debugf("[ESPmanager::begin()] CONNECT FAILURE: emergency mode enabled for %i\n",  (int8_t)_ap_boot_mode * 60 * 1000 );
            _emergencyMode(true); // at boot if disconnected don't disable AP...
        }
    }


    if (_OTAupload) {

        ArduinoOTA.setHostname(_settings->GEN.host());
        //
        ArduinoOTA.setPort( _settings->GEN.OTAport);
        //
        if (_settings->GEN.OTApassword()) {
            ESPMan_Debugf("[ESPmanager::begin()] OTApassword: %s\n", _settings->GEN.OTApassword() );
            ArduinoOTA.setPassword( (const char *)_settings->GEN.OTApassword() );


        }


        ArduinoOTA.onStart([this]() {
            //_events.send("begin","update");
            event_printf("update", "begin");
#ifdef Debug_ESPManager
            Debug_ESPManager.print(F(   "[              Performing OTA Upgrade              ]\n["));
//                                       ("[--------------------------------------------------]\n ");
#endif
        });
        ArduinoOTA.onEnd([this]() {
            _events.send("end", "update", 0, 5000);
            delay(100);
            _events.close();
            delay(1000);
#ifdef Debug_ESPManager
            Debug_ESPManager.println(F("]\nOTA End"));
            ESP.restart();
#endif
        });


        ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
            static uint8_t done = 0;
            uint8_t percent = (progress / (total / 100) );
            if ( percent % 2 == 0  && percent != done ) {
#ifdef Debug_ESPManager
                Debug_ESPManager.print("-");
#endif
                event_printf("update", "%u", percent);
                done = percent;
            }
        });

        ArduinoOTA.onError([this](ota_error_t error) {

            using namespace ESPMAN;

#ifdef Debug_ESPManager
            Debug_ESPManager.printf("OTA Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) {  Debug_ESPManager.println(F("Auth Failed")); }
            else if (error == OTA_BEGIN_ERROR) {  Debug_ESPManager.println(F("Begin Failed")); }
            else if (error == OTA_CONNECT_ERROR) {  Debug_ESPManager.println(F("Connect Failed")); }
            else if (error == OTA_RECEIVE_ERROR) {  Debug_ESPManager.println(F("Receive Failed")); }
            else if (error == OTA_END_ERROR) {  Debug_ESPManager.println(F("End Failed")); }
#endif

            if (error) {
                event_printf(string_UPDATE, string_ERROR, error);
            }
        });



        ArduinoOTA.begin();

    } else {
        ESPMan_Debugf("[ESPmanager::begin()] OTA DISABLED\n");
    }

    if (_settings->GEN.mDNSenabled) {
        MDNS.addService("http", "tcp", 80);
    }

    _HTTP.rewrite("/espman/images/ajax-loader.gif", "/espman/ajax-loader.gif");
    _HTTP.rewrite("/espman/", "/espman/index.htm");
    _HTTP.on("/espman/data.esp", std::bind(&ESPmanager::_HandleDataRequest, this, _1 ));

    //  kept to allow cached sites to refresh...
    _HTTP.on("/espman/site.appcache", HTTP_ANY, [](AsyncWebServerRequest * request) {
        request->send(200, "text/cache-manifest", "CACHE MANIFEST\nNETWORK:\n*\n\n");
    });

    _HTTP.on("/espman/upload", HTTP_POST, [this](AsyncWebServerRequest * request) {
        request->send(200);
    }, std::bind(&ESPmanager::_handleFileUpload, this, _1, _2, _3, _4, _5, _6)  );

    _HTTP.serveStatic("/espman/index.htm", _fs, "/espman/index.htm" );

    _HTTP.serveStatic("/espman/setup.htm", _fs, "/espman/setup.htm" );

    // _HTTP.on("/espman/setup.htm", [this](AsyncWebServerRequest * request) {

    //     AsyncWebServerResponse *response = request->beginResponse(_fs, "/espman/setup.htm");
    //     //response->addHeader("Server","ESP Async Web Server");
    //     response->addHeader(ESPMAN::string_CORS, "*");
    //     request->send(response);

    // });



    _events.onConnect([](AsyncEventSourceClient * client) {
        client->send(NULL, NULL, 0, 1000);
    });

    _HTTP.addHandler(&_events);



    /*
       <link rel="stylesheet" href="http://code.jquery.com/mobile/1.4.5/jquery.mobile-1.4.5.min.css">
       <script src="http://code.jquery.com/jquery-1.11.1.min.js"></script>
       <script src="http://code.jquery.com/mobile/1.4.5/jquery.mobile-1.4.5.min.js"></script> -->
       <!--<link rel="stylesheet" href="/jquery/jqm1.4.5.css">-->
       <!--<script src="/jquery/jq1.11.1.js"></script>-->
       <!--<script src="/jquery/jqm1.4.5.js"></script>-->

     */

    //_HTTP.redirect("/jquery/jqm1.4.5.css", "http://code.jquery.com/mobile/1.4.5/jquery.mobile-1.4.5.min.css" ).setFilter(ON_STA_FILTER);
    //_HTTP.rewrite("/jquery/jq1.11.1.js", "http://code.jquery.com/jquery-1.11.1.min.js").setFilter(ON_STA_FILTER);
    //_HTTP.rewrite("/jquery/jqm1.4.5.js", "http://code.jquery.com/mobile/1.4.5/jquery.mobile-1.4.5.min.js").setFilter(ON_STA_FILTER);
    //  _HTTP.rewrite("/jquery/images/ajax-loader.gif", "")
//  This works  but redirects do not work with appcache....
    // _HTTP.on("/jquery/jqm1.4.5.css", HTTP_GET, [](AsyncWebServerRequest *request){
    //         request->redirect("http://code.jquery.com/mobile/1.4.5/jquery.mobile-1.4.5.min.css");
    // }).setFilter(ON_STA_FILTER);
    //
    // _HTTP.on("/jquery/jq1.11.1.js", HTTP_GET, [](AsyncWebServerRequest *request){
    //         request->redirect("http://code.jquery.com/jquery-1.11.1.min.js");
    // }).setFilter(ON_STA_FILTER);
    //
    // _HTTP.on("/jquery/jqm1.4.5.js", HTTP_GET, [](AsyncWebServerRequest *request){
    //         request->redirect("http://code.jquery.com/mobile/1.4.5/jquery.mobile-1.4.5.min.js");
    // }).setFilter(ON_STA_FILTER);


    //_HTTP.serveStatic("/espman/", _fs, "/espman/"); //.setLastModified(getCompileTime());

    // _HTTP.serveStatic("/jquery", _fs, "/jquery/").setCacheControl("max-age:86400").setFilter(ON_AP_FILTER);

    _HTTP.on("/espman/update", std::bind(&ESPmanager::_HandleSketchUpdate, this, _1 ));

    // _HTTP.on("/testindex.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    //   AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_htm_gz, index_htm_gz_len);
    //   response->addHeader("Content-Encoding", "gzip");
    //   request->send(response);
    //
    // } );


}


void ESPmanager::enablePortal()
{
    ESPMan_Debugf("[ESPmanager::enablePortal] Enabling Portal\n");

    _dns = new DNSServer;
    //_portalreWrite = &_HTTP.rewrite("/", "/espman/setup.htm");

    IPAddress apIP(192, 168, 4, 1);

    if (_dns)   {
        /* Setup the DNS server redirecting all the domains to the apIP */
        _dns->setErrorReplyCode(DNSReplyCode::NoError);
        _dns->start(DNS_PORT, "*", apIP);
        ESPMan_Debugf("[ESPmanager::enablePortal] Done\n");

    }

}

void ESPmanager::disablePortal()
{
    ESPMan_Debugf("[ESPmanager::enablePortal] Disabling Portal\n");

    if (_dns) {
        delete _dns;
        _dns = nullptr;
    }

    // if (_portalreWrite && _HTTP.removeRewrite(_portalreWrite))
    // {
    //     _portalreWrite = nullptr;
    // }

}

void ESPmanager::handle()
{
    using namespace ESPMAN;
    static uint32_t timeout = 0;

    if (_OTAupload) { ArduinoOTA.handle(); }

    if (_dns) {
        _dns->processNextRequest();
    }

    //  Ony handle manager code every 500ms...
    if (millis() - timeout < 500) {
        return;
    }

    timeout = millis();

    if (_syncCallback) {
        if (_syncCallback()) {
            _syncCallback = nullptr;
        };
    }

    if ( _APtimer > 0 && !WiFi.softAPgetStationNum()) {


        int32_t time_total {0};

        if (_APenabledAtBoot) {
            time_total = (int8_t)_ap_boot_mode * 60 * 1000;
        } else {
            time_total = (int8_t)_no_sta_mode * 60 * 1000;
        }

        if (time_total > 0 && millis() - _APtimer > time_total ) {
            ESPMan_Debugf("[ESPmanager::handle()] Disabling AP\n");

            bool result = WiFi.enableAP(false);

            if (result == false) {
                ESPMan_Debugf("[ESPmanager::handle()] ERROR disabling AP\n");
            }

            _APtimer = 0;
            _APtimer2 = 0;
            _APenabledAtBoot = false;
        } else if (time_total > 0) {

#ifdef Debug_ESPManager
            static uint32_t timeout_warn = 0;

            if (millis()  - timeout_warn > 10000) {
                timeout_warn = millis();
                ESPMan_Debugf("[ESPmanager::handle()] Countdown to disabling AP %i of %i\n", (time_total -  (millis() - _APtimer) ) / 1000, time_total);
            }

#endif
        }
        // uint32_t timer = 0;
    }

    // triggered once when no timers.. and wifidisconnected
    if (!_APtimer && !_APtimer2 && WiFi.isConnected() == false) {
        //  if something is to be done... check  that action is not do nothing, or that AP is enabled, or action is reboot...
        if ( ( _no_sta_mode != NO_STA_NOTHING ) && ( WiFi.getMode() != WIFI_AP_STA || WiFi.getMode() != WIFI_AP || _no_sta_mode == NO_STA_REBOOT  ) ) {
            ESPMan_Debugf("[ESPmanager::handle()] WiFi disconnected: starting AP Countdown\n" );
            _APtimer2 = millis();
        }
        //_ap_triggered = true;
    }

    //  only triggered once AP_start_delay has elapsed... and not reset...
    // this gives chance for a reconnect..
    if (_APtimer2 && !_APtimer && millis() - _APtimer2 > ESPMAN::AP_START_DELAY && !WiFi.isConnected()) {


        if (_no_sta_mode == NO_STA_REBOOT) {
            ESPMan_Debugf("[ESPmanager::handle()] WiFi disconnected: REBOOTING\n" );
            ESP.restart();
        } else {
            ESPMan_Debugf("[ESPmanager::handle()] WiFi disconnected: starting AP\n" );
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
            ESPMan_Debugf("[ESPmanager::handle()] WiFi reconnected: disable AP\n" );
            //_ap_triggered = false;
            _APtimer = 0;
            _APtimer2 = 0;
            _APenabledAtBoot = false;
        }

    }



    if (_updateFreq && millis() - _updateTimer > _updateFreq * 60000) {
        _updateTimer = millis();
        ESPMan_Debugf("Performing update check\n");
        //__events.send("Checking for updates", nullptr, 0, 5000);
        _getAllSettings();

        if (_settings) {
            _upgrade(_settings->GEN.updateURL());
        }

    }

    if (_settings && !_settings->changed) {
        if (millis() - _settings->start_time > SETTINGS_MEMORY_TIMEOUT) {
            uint32_t startheap = ESP.getFreeHeap();
            delete _settings;
            _settings = nullptr;
            ESPMan_Debugf("[ESPmanager::handle()] Deleting Settings.  Heap freed = %u (%u)\n", ESP.getFreeHeap() - startheap, ESP.getFreeHeap() );

        }
    }

}

//format bytes thanks to @me-no-dev

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



//URI Decoding function
//no check if dst buffer is big enough to receive string so
//use same size as src is a recommendation
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
        ESPMan_Debugln("Seek failed on file");
    }
}


template <class T> void ESPmanager::sendJsontoHTTP( const T & root, AsyncWebServerRequest *request)
{
    int len = root.measureLength();

    ESPMan_Debugf("JSON length: %u, heap = %u\n", len, ESP.getFreeHeap());

#ifdef Debug_ESPManager

    Debug_ESPManager.println("Begin:");
    root.prettyPrintTo(Serial);
    Debug_ESPManager.println("\nEnd");

#endif

    if (len < 4000) {

        AsyncResponseStream *response = request->beginResponseStream("text/json");
        response->addHeader(ESPMAN::string_CORS, "*");
        response->addHeader(ESPMAN::string_CACHE_CONTROL, "no-store");
        root.printTo(*response);
        request->send(response);

    } else {

        ESPMan_Debugf("JSON to long\n");

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


String ESPmanager::getHostname()
{

    settings_t set;

    if (_settings) {
        set = *_settings;
    }

    int ERROR = _getAllSettings(set);

    ESPMan_Debugf("[ESPmanager::getHostname()] error = %i\n", ERROR);

    if (!ERROR && set.GEN.host() && strlen(set.GEN.host()) > 0 ) {
        return String(set.GEN.host());
    } else  {
        char tmp[33] = {'\0'};
        snprintf(tmp, 32, "esp8266-%06x", ESP.getChipId());
        return String(tmp);
    }
}

void ESPmanager::upgrade(String path)
{

    using namespace ESPMAN;

    _getAllSettings();

    myString newpath;

    if (!_settings) {
        return;
    }

    if (path.length() == 0) {

        if (_settings->GEN.updateURL() && strlen(_settings->GEN.updateURL()) > 0 ) {
            newpath = _settings->GEN.updateURL();
        } else {
            event_printf(string_UPGRADE, "[%i]", NO_UPDATE_URL );
            return;
        }

    } else {
        newpath = path.c_str();
    }

    _syncCallback = [ newpath, this ]() {

        this->_upgrade(newpath());
        return true;

    };

}

void ESPmanager::_upgrade(const char * path)
{
    using namespace ESPMAN;

    _getAllSettings();

    if (!_settings) {
        return;
    }

    if (!path) {

        if (_settings->GEN.updateURL() && strlen(_settings->GEN.updateURL()) > 0 ) {
            path = _settings->GEN.updateURL();
        } else {
            event_printf(string_UPGRADE, "[%i]", NO_UPDATE_URL );
            return;
        }

    }


    int files_expected = 0;
    int files_recieved = 0;
    int file_count = 0;
    DynamicJsonBuffer jsonBuffer;
    JsonObject * p_root = nullptr;
    uint8_t * buff = nullptr;
    bool updatesketch = false;

    char msgdata[100];  //  delete me when done

    _events.send("begin", string_UPGRADE, 0, 5000);
    ESPMan_Debugf("[ESPmanager::_HandleDataRequest] Checking for Updates: %s\n", path);

    String Spath = String(path);
    String rooturi = Spath.substring(0, Spath.lastIndexOf('/') );


    event_printf(string_CONSOLE, "%s", path);
    ESPMan_Debugf("[ESPmanager::upgrade] rooturi=%s\n", rooturi.c_str());

    //  save new update path for future update checks...  (if done via url only for example)
    if (_settings->GEN.updateURL()) {
        if (strcmp(_settings->GEN.updateURL(), path) != 0) {
            _settings->GEN.updateURL = path;
            save_flag = true;
        }

    } else {
        _settings->GEN.updateURL = path;
        save_flag = true;
    }


    int ret = _parseUpdateJson(buff, jsonBuffer, p_root, path);

    if (ret) {
        event_printf(string_UPGRADE, string_ERROR2, MANIFST_FILE_ERROR, ret);
        ESPMan_Debugf("[ESPmanager::upgrade] MANIFEST ERROR [%i]\n", ret );
        return;
    }

    ESPMan_Debugf("[ESPmanager::_HandleSketchUpdate] _parseUpdateJson success\n");

    if (!p_root) {
        event_printf(string_UPGRADE, string_ERROR, JSON_OBJECT_ERROR);
        ESPMan_Debugf("[ESPmanager::upgrade] JSON ERROR [%i]\n", JSON_OBJECT_ERROR );
        return;
    }

    JsonObject & root = *p_root;
    files_expected = root["filecount"];

    if (!files_expected) {
        event_printf(string_UPGRADE, string_ERROR, UNKNOWN_NUMBER_OF_FILES);
        ESPMan_Debugf("[ESPmanager::upgrade] ERROR [%i]\n", UNKNOWN_NUMBER_OF_FILES );

    }


    JsonArray & array = root["files"];

    if (root.containsKey("formatSPIFFS")) {
        if (root["formatSPIFFS"] == true) {
            ESPMan_Debugf("[ESPmanager::_HandleSketchUpdate] Formatting SPIFFS....");
            _fs.format();
            ESPMan_Debugf("done\n");
        }
    }

    if (root.containsKey("clearWiFi")) {
        if (root["clearWiFi"] == true) {
            ESPMan_Debugf("[ESPmanager::_HandleSketchUpdate] Erasing WiFi Config ....");

            ESPMan_Debugf("done\n");
        }
    }


    for (JsonArray::iterator it = array.begin(); it != array.end(); ++it) {
        file_count++;
        JsonObject& item = *it;
        String remote_path = String();

        //  if the is url is set to true then don't prepend the rootUri...
        if (item["isurl"] == true) {
            remote_path = String(item["location"].asString());
        } else {
            remote_path = rooturi + String(item["location"].asString());
        }

        const char* md5 = item["md5"];
        String filename = item["saveto"];

        if (remote_path.endsWith("bin") && filename == "sketch" ) {
            updatesketch = true;
            files_recieved++;         //  add one to keep count in order...

            ESPMan_Debugf("[%u/%u] BIN Updated pending\n", file_count, files_expected);


            continue;
        }

        ESPMan_Debugf("[%u/%u] Downloading (%s)..", file_count, files_expected, filename.c_str()  );

        int ret = _DownloadToSPIFFS(remote_path.c_str(), filename.c_str(), md5 );

        if (ret == 0 || ret == FILE_NOT_CHANGED) {
            event_printf(string_CONSOLE, "[%u/%u] (%s) : %s", file_count, files_expected, filename.c_str(), (!ret) ? "Downloaded" : "Not changed");
        } else {
            event_printf(string_CONSOLE, "[%u/%u] (%s) : ERROR [%i]", file_count, files_expected, filename.c_str(), ret);
        }

        event_printf(string_UPGRADE, "%u", (uint8_t ) (( (float)file_count / (float)files_expected) * 100.0f) );

#if defined(Debug_ESPManager)
        if (ret == 0) {
            Debug_ESPManager.printf("SUCCESS \n");
            //files_recieved++;
        } else if (ret == FILE_NOT_CHANGED) {
            Debug_ESPManager.printf("FILE NOT CHANGED \n");
        } else {
            Debug_ESPManager.printf("FAILED [%i]\n", ret  );
        }
#endif
        delay(20);
    }

    //  this removes any duplicate files if a compressed 
    _removePreGzFiles(); 



    if (updatesketch) {

        for (JsonArray::iterator it = array.begin(); it != array.end(); ++it) {
            JsonObject& item = *it;
            String remote_path = rooturi + String(item["location"].asString());
            String filename = item["saveto"];
            String commit = root["commit"];

            if (remote_path.endsWith("bin") && filename == "sketch" ) {
                if ( String( item["md5"].asString() ) != getSketchMD5() ) {

                    ESPMan_Debugf("START SKETCH DOWNLOAD (%s)\n", remote_path.c_str()  );
                    _events.send("firmware", string_UPGRADE, 0, 5000);
                    delay(10);
                    _events.send("Upgrading sketch", nullptr, 0, 5000);
                    // _fs.end();
                    ESPhttpUpdate.rebootOnUpdate(false);

                    t_httpUpdate_return ret = ESPhttpUpdate.update(remote_path);

                    switch (ret) {

                    case HTTP_UPDATE_FAILED:
                        ESPMan_Debugf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                        // snprintf(msgdata, 100,"FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str() );
                        // _events.send(msgdata, "upgrade");
                        event_printf(string_UPGRADE, "ERROR [%s]", ESPhttpUpdate.getLastErrorString().c_str() );
                        break;

                    case HTTP_UPDATE_NO_UPDATES:
                        ESPMan_Debugf("HTTP_UPDATE_NO_UPDATES");
                        //_events.send("FAILED no update", "upgrade");
                        event_printf(string_UPGRADE, "ERROR no update");
                        break;

                    case HTTP_UPDATE_OK:
                        ESPMan_Debugf("HTTP_UPDATE_OK");
                        _events.send("firmware-end", string_UPGRADE, 0, 1000);
                        delay(100);
                        _events.close();
                        delay(1000);
                        ESP.reset();
                        break;
                    }

                } else {
                    _events.send("No Change to firmware", string_CONSOLE, 0, 5000 );
                    ESPMan_Debugf("BINARY HAS SAME MD5 as current (%s)\n", item["md5"].asString()  );

                }
            }
        }
    }

    if (buff) {
        delete[] buff;
    }

    _events.send("end", string_UPGRADE, 0, 5000);

}

uint32_t ESPmanager::trueSketchSize()
{
    return ESP.getSketchSize();
}

String ESPmanager::getSketchMD5()
{
    return ESP.getSketchMD5();
}

AsyncEventSource & ESPmanager::getEvent()
{
    return _events;
}

size_t ESPmanager::event_printf(const char * topic, const char * format, ... )
{
    va_list arg;
    va_start(arg, format);
    char temp[64];
    char* buffer = temp;
    size_t len = vsnprintf(temp, sizeof(temp), format, arg);
    va_end(arg);
    if (len > sizeof(temp) - 1) {
        buffer = new char[len + 1];
        if (!buffer) {
            return 0;
        }
        va_start(arg, format);
        vsnprintf(buffer, len + 1, format, arg);
        va_end(arg);
    }
    _events.send(buffer, topic, 0, 5000);
    if (buffer != temp) {
        delete[] buffer;
    }
    return len;
}

int ESPmanager::save()
{
    using namespace ESPMAN;
    _getAllSettings();

    if (_settings) {
        return _saveAllSettings(*_settings);
    } else {
        return SETTINGS_NOT_IN_MEMORY;
    }

}



#ifdef ESPMAN_USE_UPDATER

int ESPmanager::_DownloadToSPIFFS(const char * url, const char * filename_c, const char * md5_true )
{
    using namespace ESPMAN;
    String filename = filename_c;
    HTTPClient http;
    FSInfo _FSinfo;
    int freeBytes = 0;
    bool success = false;
    int ERROR = 0;

    if ( _fs.exists(filename) ) {

        File Fcheck = _fs.open(filename, "r");
        String crc = file_md5(Fcheck);

        if (crc == String(md5_true)) {
            Fcheck.close();
            return FILE_NOT_CHANGED;
        }

        Fcheck.close();
    }


    if (!_fs.info(_FSinfo)) {
        return SPIFFS_INFO_FAIL;
    }

    freeBytes = _FSinfo.totalBytes - _FSinfo.usedBytes;

    if (filename.length() > _FSinfo.maxPathLength) {
        return SPIFFS_FILENAME_TOO_LONG;
    }

    File f = _fs.open("/tempfile", "w+"); //  w+ is to allow read operations on file.... otherwise crc gets 255!!!!!

    if (!f) {

        return SPIFFS_FILE_OPEN_ERROR;
    }

    http.begin(url);

    int httpCode = http.GET();

    if (httpCode == 200) {

        int len = http.getSize();

        if (len < freeBytes) {


            size_t byteswritten = http.writeToStream(&f);

            http.end();

            if (f.size() == len ||
                    len == -1 ) { //  len = -1 means server did not provide length...

                if (md5_true) {
                    String crc = file_md5(f);

                    if (crc == String(md5_true)) {
                        success = true;
                    } else {
                        ERROR = CRC_ERROR;
                    }

                } else {
                    ESPMan_Debugf("\n  [ERROR] CRC not provided \n");
                    success = true;                             // set to true if no CRC provided...
                }

            } else {
                ERROR = INCOMPLETE_DOWNLOAD;
            }

        } else {
            ERROR = FILE_TOO_LARGE;
        }

    } else {
        ERROR = httpCode;
    }

    f.close();

    if (success) {

        if (_fs.exists(filename)) {
            _fs.remove(filename);
        }

        if (filename.endsWith(".gz") ) {
            String withOutgz = filename.substring(0, filename.length() - 3);
            ESPMan_Debugf("NEW File ends in .gz: without = %s...", withOutgz.c_str());
            if (_fs.remove(withOutgz)) {
                ESPMan_Debugf("%s DELETED...", withOutgz.c_str());
            }
        }

        if (_fs.exists(filename + ".gz")) {
            if (_fs.remove(filename + ".gz")) {
                ESPMan_Debugf("%s.gz DELETED...", filename.c_str());
            }
        }

        _fs.rename("/tempfile", filename);

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

int ESPmanager::_parseUpdateJson(uint8_t *& buff, DynamicJsonBuffer & jsonBuffer, JsonObject *& root, const char * path)
{
    using namespace ESPMAN;

    ESPMan_Debugf("[ESPmanager::_parseUpdateJson] path = %s\n", path);

    HTTPClient http;

    http.begin(path);  //HTTP

    int httpCode = http.GET();

    if (httpCode != 200) {
        ESPMan_Debugf("[ESPmanager::_parseUpdateJson] HTTP code: %i\n", httpCode  );
        return httpCode;
    }

    ESPMan_Debugln("[ESPmanager::_parseUpdateJson] Connected downloading json");

    size_t len = http.getSize();
    const size_t length = len;

    if (len > MAX_BUFFER_SIZE) {
        ESPMan_Debugln("[ESPmanager::_parseUpdateJson] Receive update length too big.  Increase buffer");
        return JSON_TOO_LARGE;
    }

    //uint8_t buff[bufsize] = { 0 }; // max size of input buffer. Don't use String, as arduinoJSON doesn't like it!
    buff = nullptr;
    buff = new uint8_t[len];

    if (!buff) {
        ESPMan_Debugf("[ESPmanager::_parseUpdateJson] failed to allocate buff\n");
        return MALLOC_FAIL;
    }


    // get tcp stream
    WiFiClient * stream = http.getStreamPtr();
    int position = 0;

    // read all data from server
    while (http.connected() && (len > 0 || len == -1)) {
        // get available data size
        size_t size = stream->available();
        uint8_t * b = &buff[position];

        if (size) {
            int c = stream->readBytes(b, ((size > sizeof(buff)) ? sizeof(buff) : size));
            position += c;
            if (len > 0) {
                len -= c;
            }
        }
        delay(0);
    }

    http.end();

    root = &jsonBuffer.parseObject( (char*)buff, length );

    if (root->success()) {
        ESPMan_Debugf("[ESPmanager::_parseUpdateJson] root->success() = true\n");
        return 0;
    } else {
        ESPMan_Debugf("[ESPmanager::_parseUpdateJson] root->success() = false\n");
        return JSON_PARSE_ERROR;
    }

}


void ESPmanager::_HandleSketchUpdate(AsyncWebServerRequest *request)
{


    ESPMan_Debugf("[ESPmanager::_HandleSketchUpdate] HIT\n" );


    if ( request->hasParam("url", true)) {

        String path = request->getParam("url", true)->value();

        ESPMan_Debugf("[ESPmanager::_HandleSketchUpdate] path = %s\n", path.c_str());

        _syncCallback = [ = ]() {

            _upgrade(path.c_str());
            return true;

        };

    }

    _sendTextResponse(request, 200, "OK");

    // AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
    // response->addHeader( ESPMAN::string_CORS, "*");
    // response->addHeader( ESPMAN::string_CACHE_CONTROL, "no-store");
    // request->send(response);

}

#endif // #webupdate


void ESPmanager::_handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    char msgdata[100] = {'\0'};
    bool _uploadAuthenticated = true;
    if (!index) {
        //if(!_username.length() || request->authenticate(_username.c_str(),_password.c_str()))
        //if(!_username.length() || request->authenticate(_username.c_str(),_password.c_str()))
        _uploadAuthenticated = true; // not bothering just yet...
        if (!filename.startsWith("/")) { filename = "/" + filename; }
        request->_tempFile = _fs.open(filename, "w");

        ESPMan_Debugf("UploadStart: %s\n", filename.c_str());
        snprintf(msgdata, 100, "UploadStart:%s",  filename.c_str());
        _events.send(msgdata, nullptr, 0, 5000);

        //_ws.printfAll_P( PSTR("File Upload Started"));
    }

    if (_uploadAuthenticated && request->_tempFile && len) {
        ESP.wdtDisable(); request->_tempFile.write(data, len); ESP.wdtEnable(10);
    }

    if (_uploadAuthenticated && final) {
        if (request->_tempFile) { request->_tempFile.close(); }
        //_ws.printfAll_P( PSTR("%s Upload Finished"), filename.c_str());
        ESPMan_Debugf("UploadEnd: %s, %u B\n", filename.c_str(), index + len);
        snprintf(msgdata, 100, "UploadFinished:%s (%u)",  filename.c_str(), request->_tempFile.size() );
        _events.send(msgdata, nullptr, 0, 5000);

    }


}


void ESPmanager::_HandleDataRequest(AsyncWebServerRequest *request)
{
#if defined(Debug_ESPManager)
//List all collected headers
    // int params = request->params(true);

    // int i;
    // for (i = 0; i < params; i++) {
    //     AsyncWebParameter* h = request->getParam(i, true);
    //     Debug_ESPManager.printf("[ESPmanager::_HandleDataRequest] [%s]: %s\n", h->name().c_str(), h->value().c_str());
    // }
    int params = request->params();
    for (int i = 0; i < params; i++) {
        AsyncWebParameter* p = request->getParam(i);
        if (p->isFile()) { //p->isPost() is also true
            Debug_ESPManager.printf("FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
        } else if (p->isPost()) {
            Debug_ESPManager.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        } else {
            Debug_ESPManager.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
    }

#endif


    using namespace ESPMAN;
    String buf;
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();


    if (!_settings) {
        _getAllSettings();
    }

    if (!_settings) {
        return;
    }

    settings_t & set = *_settings;

    set.start_time = millis(); //  resets the start time... to keep them in memory if being used.





#ifdef Debug_ESPManager
    if (request->hasParam("body", true) && request->getParam("body", true)->value() == "diag") {

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


    if (request->hasParam("purgeunzipped")) {
       // if (request->getParam("body")->value() == "purgeunzipped") {
            ESPMan_Debugf("PURGE UNZIPPED FILES\n"); 
            _removePreGzFiles(); 
        //}

    }


    /*------------------------------------------------------------------------------------------------------------------
                                                                    Reboot command
       ------------------------------------------------------------------------------------------------------------------*/
    if (request->hasParam("body", true)) {

        //ESPMan_Debugln(F("Has Body..."));


        String plainCommand = request->getParam("body", true)->value();

        if (plainCommand == "generalpage") {

            ESPMan_Debugf("Got body\n");


        }

        if (plainCommand == "save") {
            ESPMan_Debugf("Saving Settings File....\n");
            if (_settings) {

                int ERROR = _saveAllSettings(*_settings);

#if defined(Debug_ESPManager)

                File f = _fs.open(SETTINGS_FILE, "r");
                Debug_ESPManager.printf("ESP MANAGER Settings [%u]B:\n", f.size());
                if (f) {
                    for (int i = 0; i < f.size(); i++) {
                        Debug_ESPManager.write(f.read());
                    }
                    Debug_ESPManager.println();
                    f.close();
                }

#endif

                if (ERROR) {
                    event_printf(NULL, string_ERROR, ERROR);
                } else {
                    _events.send("Settings Saved", nullptr, 0, 5000);
                    set.changed = false;
                    if (_fs.remove("/.wizard")) {

                        _sendTextResponse(request, 200, "OK");

                        // AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
                        // response->addHeader(ESPMAN::string_CORS, "*");
                        // response->addHeader(ESPMAN::string_CACHE_CONTROL, "no-store");
                        // request->send(response);

                        _syncCallback = [this]() {
                            ESPMan_Debugf("REBOOTING....\n");
                            delay(100);
                            ESP.restart();
                            return true;
                        };

                        return; //  stop request
                    }
                }
            }
        }

        if ( plainCommand == F("reboot") || plainCommand == F("restart")) {
            ESPMan_Debugln(F("Rebooting..."));

            _sendTextResponse(request, 200, "OK");

            // AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
            // response->addHeader(ESPMAN::string_CORS, "*");
            // response->addHeader(ESPMAN::string_CACHE_CONTROL, "no-store");
            // request->send(response);
            _syncCallback = [this]() {
                _events.send("Rebooting", NULL, 0, 1000);
                delay(100);
                _events.close();

                delay(100);
                ESP.restart();
                delay(100000);
                return true;
            };

        };

        /*------------------------------------------------------------------------------------------------------------------
                                          WiFi Scanning and Sending of WiFi networks found at boot
           ------------------------------------------------------------------------------------------------------------------*/
        if ( plainCommand == F("WiFiDetails") || plainCommand == F("PerformWiFiScan") || plainCommand == "generalpage" ) {

//************************
            if (plainCommand == F("PerformWiFiScan")) {


                int wifiScanState = WiFi.scanComplete();

                DynamicJsonBuffer jsonBuffer;
                JsonObject& root = jsonBuffer.createObject();

                if (wifiScanState == -2) {
                    WiFi.scanNetworks(true);
                    //_sendTextResponse(request, 200, "started");
                    root["scan"] = "started";
                } else if (wifiScanState == -1) {
                    root["scan"] = "running";
                } else if (wifiScanState > 0) {

                    _wifinetworksfound = wifiScanState;

                    JsonArray& Networkarray = root.createNestedArray("networks");

                    /*
                    This only returns first 15 entries... to save memory...
                    Will work on sort func later...s
                    */

                    if (_wifinetworksfound > 10) {
                        _wifinetworksfound = 10;
                    }


                    event_printf(NULL, "%u Networks Found", _wifinetworksfound);


                    for (int i = 0; i < _wifinetworksfound; ++i) {
                        JsonObject& ssidobject = Networkarray.createNestedObject();

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


            JsonObject& generalobject = root.createNestedObject(string_General);

            generalobject[string_deviceid] = set.GEN.host();
            //generalobject[F("OTAenabled")] = (_OTAenabled) ? true : false;

            generalobject[string_OTApassword] = (set.GEN.OTApassword) ? true : false;
            generalobject[string_GUIhash] =  (set.GEN.GUIhash) ? true : false;
            generalobject[string_OTAport] = set.GEN.OTAport;
            generalobject[string_ap_boot_mode] = (int)_ap_boot_mode;
            generalobject[string_no_sta_mode] = (int)_no_sta_mode;
            //generalobject[F("OTAusechipID")] = _OTAusechipID;
            generalobject[string_mDNS] = (set.GEN.mDNSenabled) ? true : false;
            //generalobject[string_usePerminantSettings] = (set.GEN.usePerminantSettings) ? true : false;
            generalobject[string_OTAupload] = (set.GEN.OTAupload) ? true : false;
            generalobject[string_updateURL] = (set.GEN.updateURL) ? set.GEN.updateURL() : "";
            generalobject[string_updateFreq] = set.GEN.updateFreq;

            JsonObject& GenericObject = root.createNestedObject("generic");

            GenericObject["channel"] = WiFi.channel();
            GenericObject["sleepmode"] = (int)WiFi.getSleepMode();
            GenericObject["phymode"] = (int)WiFi.getPhyMode();


            JsonObject& STAobject = root.createNestedObject(string_STA);


            STAobject[F("connectedssid")] = WiFi.SSID();

            STAobject[F("dhcp")] = (set.STA.dhcp) ? true : false;

            STAobject[F("state")] = (mode == WIFI_STA || mode == WIFI_AP_STA) ? true : false;

            STAobject[string_channel] = WiFi.channel();

            STAobject[F("RSSI")] = WiFi.RSSI();

            //String ip;

            STAobject[string_IP] = WiFi.localIP().toString();

            STAobject[string_GW] = WiFi.gatewayIP().toString();

            STAobject[string_SN] = WiFi.subnetMask().toString();

            STAobject[string_DNS1] = WiFi.dnsIP(0).toString();

            STAobject[string_DNS2] = WiFi.dnsIP(1).toString();

            STAobject[string_MAC] = WiFi.macAddress();

            JsonObject& APobject = root.createNestedObject("AP");

            APobject[string_ssid] = set.GEN.host();
            APobject[F("state")] = (mode == WIFI_AP || mode == WIFI_AP_STA) ? true : false;
            //APobject[F("APenabled")] = (int)set.AP.mode;
            //APobject[string_mode] = (int)_ap_mode;


            APobject[string_IP] = (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) ? F("192.168.4.1") : WiFi.softAPIP().toString();


            APobject[string_visible] = (set.AP.visible) ? true : false;
            APobject[string_pass] = (set.AP.pass()) ? set.AP.pass() : "";

            softap_config config;

            if (wifi_softap_get_config( &config)) {

                APobject[string_channel] = config.channel;

            }

            APobject[string_MAC] = WiFi.softAPmacAddress();
            APobject[F("StationNum")] = WiFi.softAPgetStationNum();


        }

        /*------------------------------------------------------------------------------------------------------------------
                                          Send about page details...
           ------------------------------------------------------------------------------------------------------------------*/
        if (plainCommand == "AboutPage") {

            FSInfo info;
            _fs.info(info);

            const uint8_t bufsize = 50;
            uint32_t sec = millis() / 1000;
            uint32_t min = sec / 60;
            uint32_t hr = min / 60;
            uint32_t day = hr / 24;
            int Vcc = analogRead(A0);

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

            JsonObject& SPIFFSobject = root.createNestedObject("SPIFFS");
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
            SPIFFSobject[F("totalBytes")] = formatBytes(info.totalBytes);
            SPIFFSobject[F("usedBytes")] = formatBytes(info.usedBytes);
            SPIFFSobject[F("blockSize")] = formatBytes(info.blockSize);
            SPIFFSobject[F("pageSize")] = formatBytes(info.pageSize);
            SPIFFSobject[F("maxOpenFiles")] = info.maxOpenFiles;
            SPIFFSobject[F("maxPathLength")] = info.maxPathLength;

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

            JsonObject& UMMobject = root.createNestedObject("UMM");
            UMMobject[F("totalEntries")] = ummHeapInfo.totalEntries;
            UMMobject[F("usedEntries")] = ummHeapInfo.usedEntries;
            UMMobject[F("freeEntries")] = ummHeapInfo.freeEntries;
            UMMobject[F("totalBlocks")] = ummHeapInfo.totalBlocks;
            UMMobject[F("usedBlocks")] = ummHeapInfo.usedBlocks;
            UMMobject[F("freeBlocks")] = ummHeapInfo.freeBlocks;
            UMMobject[F("maxFreeContiguousBlocks")] = ummHeapInfo.maxFreeContiguousBlocks;

            JsonObject& Resetobject = root.createNestedObject("reset");

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
            root[string_updateURL] = set.GEN.updateURL();
            root[string_updateFreq] = set.GEN.updateFreq;
            //sendJsontoHTTP(root, request);
            //return;

        }

        if (plainCommand == "formatSPIFFS") {
            ESPMan_Debug(F("Format SPIFFS"));
            _events.send("Formatting SPIFFS", nullptr, 0, 5000);

            _sendTextResponse(request, 200, "OK");


            // AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
            // response->addHeader(ESPMAN::string_CORS, "*");
            // response->addHeader(ESPMAN::string_CACHE_CONTROL, "no-store");
            // request->send(response);


            _syncCallback = [this]() {
                _fs.format();
                ESPMan_Debugln(F(" done"));
                _events.send("Formatting done", nullptr, 0, 5000);
                return true;
            };
        }

        if (plainCommand == "deletesettings") {

            ESPMan_Debug(F("Delete Settings File"));
            if (_fs.remove(SETTINGS_FILE)) {
                ESPMan_Debugln(F(" done"));
                _events.send("Settings File Removed", nullptr, 0, 5000);
            } else {
                ESPMan_Debugln(F(" failed"));
            }
        }


        if ( plainCommand == "resetwifi" ) {


            _syncCallback = [this]() {

                _events.send("Reset WiFi and Reboot", NULL, 0, 1000);
                delay(100);
                _events.close();
                delay(100);

                WiFi.disconnect();
                ESP.eraseConfig();
                ESP.reset();
                return true;
            };

        }


        /*------------------------------------------------------------------------------------------------------------------

                                       wizard
        ------------------------------------------------------------------------------------------------------------------*/

        if (plainCommand == "enterWizard") {

            ESP.eraseConfig();

            ESPMan_Debugf("[ESPmanager::_HandleDataRequest] Enter Wizard hit\n");

            File f = _fs.open("/.wizard", "w"); //  creates a file that overrides everything during initial config...

            uint8_t * data = static_cast<uint8_t*>(static_cast<void*>(&set.AP));

            if (f) {
                for (int i = 0; i < sizeof(set.AP); i++) {
                    f.write(  data[i]);
                }

                _sendTextResponse(request, 200, "OK");


                // AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
                // response->addHeader(ESPMAN::string_CORS, "*");
                // response->addHeader(ESPMAN::string_CACHE_CONTROL, "no-store");
                // request->send(response);
                return;
            } else {

                _sendTextResponse(request, 200, "File Error");

                // AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "File Error");
                // response->addHeader(ESPMAN::string_CORS, "*");
                // response->addHeader(ESPMAN::string_CACHE_CONTROL, "no-store");
                // request->send(response);
                return;
            }

        }


        if (plainCommand == "cancelWizard") {

            _fs.remove("/.wizard");
        }

        if (plainCommand == "factoryReset") {

            _sendTextResponse(request, 200, "Factory Reset Done");


            // AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Factory Reset Done!");
            // response->addHeader(ESPMAN::string_CORS, "*");
            // response->addHeader(ESPMAN::string_CACHE_CONTROL, "no-store");
            // request->send(response);

            _syncCallback = [this]() {
                factoryReset();
                delay(100);
                ESP.restart();
                while (1);
                return true;
            };
            return;
        }




    } //  end of if plaincommand



    if (request->hasParam(string_ssid, true) && request->hasParam(string_pass, true)) {

        bool APChannelchange  = false;
        int channel = -1;

        String ssid = request->getParam(string_ssid, true)->value();
        String psk = request->getParam(string_pass, true)->value();

        if (ssid.length() > 0) {
            // _HTTP.arg("pass").length() > 0) {  0 length passwords should be ok.. for open
            // networks.
            if (ssid.length() < 33 && psk.length() < 33) {

                if (ssid != WiFi.SSID() || psk != WiFi.psk() || !set.STA.enabled ) {

                    bool safety = false;

                    if (request->hasParam("removesaftey", true))  {
                        safety = (request->getParam("removesaftey", true)->value() == "No") ? false : true;
                    }

                    settings_t::STA_t * newsettings = new settings_t::STA_t(set.STA);

                    newsettings->ssid = ssid.c_str();
                    newsettings->pass = psk.c_str();
                    newsettings->enabled = true;

                    ESPMan_Debugf("[ESPmanager::_HandleDataRequest()] applied new ssid & psk to tmp obj ssid =%s, pass=%s\n", newsettings->ssid(), newsettings->pass() );

                    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA ) {

                        int currentchannel = WiFi.channel();


                        if (request->hasParam("STAchannel_desired", true)) {


                            int desired_channal = request->getParam("STAchannel_desired", true)->value().toInt();

                            if (desired_channal != currentchannel && desired_channal >= 0 && currentchannel >= 0) {

                                ESPMan_Debugf("AP Channel change required: current = %i, desired = %i\n", currentchannel, desired_channal);
                                APChannelchange = true;
                                channel = desired_channal;

                            } else {
                                ESPMan_Debugf("AP Channel change NOT required: current = %i, desired = %i\n", currentchannel, desired_channal);
                            }

                        }

                        //


                    }


                    _syncCallback = [safety, newsettings, request, this, APChannelchange, channel]() {

                        using namespace ESPMAN;

                        WiFiresult = 0;
                        ESPMan_Debugf("[_syncCallback] Called\n");

                        uint32_t starttime = millis();

                        if (APChannelchange) {

                            uint8_t connected_station_count = WiFi.softAPgetStationNum();

                            ESPMan_Debugf("[_syncCallback] Changing AP channel to %u :", channel);
                            _events.send("Changing AP Channel...\n", nullptr, 0, 5000);
                            delay(10);
                            WiFi.enableAP(false);
                            settings_t set;
                            set.AP.ssid = set.GEN.host();
                            set.AP.channel = channel;
                            set.AP.enabled = true;
                            bool result = _initialiseAP(set.AP);

                            if (!result) {
                                ESPMan_Debugf("[_syncCallback] Waiting For AP reconnect\n");
                                starttime = millis();



                                uint32_t dottimer = millis();

                                while ( WiFi.softAPgetStationNum() < connected_station_count) {

                                    if (_dns) {
                                        _dns->processNextRequest();
                                    }

                                    //yield();
                                    delay(10);
                                    if (millis() - dottimer > 1000) {
                                        ESPMan_Debugf(".");
                                        dottimer = millis();
                                    }

                                    if (millis() - starttime > 60000) {
                                        ESPMan_Debugf("[_syncCallback] Error waiting for AP reconnect\n");
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
                                ESPMan_Debugf("[_syncCallback] Error: %i\n", result);
                            }



                        }

                        starttime = millis() ;// reset the start timer....

                        _events.send("Updating WiFi Settings", nullptr, 0, 5000);
                        delay(10);

                        int ERROR = _initialiseSTA(*newsettings);

                        if (!ERROR) {
                            ESPMan_Debugf("[ESPmanager::_HandleDataRequest()] CALLBACK: Settings successfull\n");
                            WiFiresult = 1;

                            if (!_settings) {
                                _getAllSettings();
                            }


                            if (_settings && newsettings) {
                                //Serial.print("\n\n\nsettings->STA = *newsettings;\n\n");
                                _settings->STA = *newsettings;
                                //Serial.print("\ndone\n\n");
                                _settings->changed = true;
                                ESPMan_Debugf("[ESPmanager::_HandleDataRequest()] CALLBACK: Settings Applied\n");
                                save_flag = true;
                            }

                        } else {
                            WiFiresult = 2;
                            ESPMan_Debugf("[ESPmanager::_HandleDataRequest()] ERROR: %i\n", ERROR);
                            WiFi.enableSTA(false); //  turns it off....
                        }

                        _events.send("WiFi Settings Updated", nullptr, 0, 5000);

                        if (newsettings) {
                            delete newsettings;
                        }


                        return true;

                    }; //  end of lambda...

                    _sendTextResponse(request, 200, "accepted");

                    // AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "accepted");
                    // response->addHeader( ESPMAN::string_CORS, "*");
                    // response->addHeader( ESPMAN::string_CACHE_CONTROL, "no-store");
                    // request->send(response);

                    return;
                }
            }
        }
    }
//*******************************************

    //  This is outside the loop...  wifiresult is a static to return previous result...
    if (  request->hasParam("body", true) && request->getParam("body", true)->value() == "WiFiresult") {



        if (WiFiresult == 1 && WiFi.localIP() != INADDR_NONE) {
            WiFiresult = 4; // connected
        }

        ESPMan_Debugf("[ESPmanager] WiFiResult = %i [%u.%u.%u.%u]\n", WiFiresult, WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);

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


    if (request->hasParam("enable-STA", true)) {

        bool changes = false;

        // Serial.println("SETTINGS COPIED INTO TEMP BUFFER");
        // _dumpSTA(set.STA);
        // Serial.println();

        settings_t::STA_t * newsettings = new settings_t::STA_t(set.STA);

        if (newsettings) {

            /*
                    ENABLED
             */

            bool enable = request->getParam("enable-STA", true)->value().equals("on");

            if (enable != newsettings->enabled) {
                newsettings->enabled = enable;
                changes = true;
            }

            /*
                    DHCP and Config
             */
            if (request->hasParam("enable-dhcp", true)) {

                bool dhcp = request->getParam("enable-dhcp", true)->value().equals("on");

                //



                if (dhcp) {
                    ESPMan_Debugf("[ESPmanager::_HandleDataRequest] dhcp = on\n" );

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
                    ESPMan_Debugf("[ESPmanager::_HandleDataRequest] dhcp = off\n" );

                    if (_settings->STA.dhcp) {
                        changes = true;
                    }

                    bool IPres {false};
                    bool GWres {false};
                    bool SNres {false};
                    bool DNSres {false};

                    if (request->hasParam(string_IP, true) &&
                            request->hasParam(string_GW, true) &&
                            request->hasParam(string_SN, true) &&
                            request->hasParam(string_DNS1, true) ) {

                        IPres = newsettings->IP.fromString( request->getParam(string_IP, true)->value() );
                        GWres = newsettings->GW.fromString( request->getParam(string_GW, true)->value() );
                        SNres = newsettings->SN.fromString( request->getParam(string_SN, true)->value() );
                        DNSres = newsettings->DNS1.fromString( request->getParam(string_DNS1, true)->value() );
                    }


                    if (IPres && GWres && SNres && DNSres) {

                        //  apply settings if any of these are different to current settings...
                        if (newsettings->IP != _settings->STA.IP ||  newsettings->GW != _settings->STA.GW || newsettings->SN != _settings->STA.SN || newsettings->DNS1 != _settings->STA.DNS1 ) {
                            changes = true;
                        }
                        ESPMan_Debugf("[ESPmanager::_HandleDataRequest] Config Set\n");
                        newsettings->hasConfig = true;
                        newsettings->dhcp = false;

                        if (request->hasParam(string_DNS2, true)) {

                            bool res = newsettings->DNS2.fromString ( request->getParam(string_DNS2, true)->value() );
                            if (res) {
                                ESPMan_Debugf("[ESPmanager::_HandleDataRequest] DNS 2 %s\n",  newsettings->DNS2.toString().c_str() );
                                if (newsettings->DNS2 != _settings->STA.DNS2 ) {
                                    changes = true;
                                }
                            }

                        }

                    }

                    ESPMan_Debugf("[ESPmanager::_HandleDataRequest] IP %s, GW %s, SN %s\n", (IPres) ? "set" : "error", (GWres) ? "set" : "error", (SNres) ? "set" : "error"  );
                }


                //}
            }
            /*
                    autoconnect and reconnect
             */
            if (request->hasParam(string_autoconnect, true)) {

                bool autoconnect = request->getParam(string_autoconnect, true)->value().equals("on");

                if (autoconnect != newsettings->autoConnect) {
                    newsettings->autoConnect = autoconnect;
                    changes = true;
                }
            }

            if (request->hasParam(string_autoreconnect, true)) {
                bool autoreconnect = request->getParam(string_autoreconnect, true)->value().equals("on");

                if (autoreconnect != newsettings->autoReconnect) {
                    newsettings->autoReconnect = autoreconnect;
                    changes = true;
                }
            }

            if (request->hasParam(string_MAC, true) && request->getParam(string_MAC, true)->value().length() != 0) {



                if ( StringtoMAC(newsettings->MAC, request->getParam(string_MAC, true)->value() ) ) {


                    ESPMan_Debugln("New STA MAC parsed sucessfully");
                    ESPMan_Debugf("[%u]:[%u]:[%u]:[%u]:[%u]:[%u]\n", newsettings->MAC[0], newsettings->MAC[1], newsettings->MAC[2], newsettings->MAC[3], newsettings->MAC[4], newsettings->MAC[5]);

                    // compare MACS..

                    uint8_t currentmac[6];
                    WiFi.macAddress(&currentmac[0]);

                    if (memcmp(&(currentmac[0]), newsettings->MAC, 6)) {
                        ESPMan_Debugln("New  MAC is different");
                        newsettings->hasMAC = true;
                        changes = true;
                    } else {
                        ESPMan_Debugln("New MAC = Old MAC");
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
                    ESPMan_Debugln("New STA MAC parsed FAILED");
                }
            }

            if (changes) {

                _syncCallback = [this, newsettings] () {

                    using namespace ESPMAN;

                    _events.send("Updating WiFi Settings", nullptr, 0, 5000);
                    delay(10);

                    ESPMan_Debugf("*** CALLBACK: dhcp = %s\n", (newsettings->dhcp) ? "true" : "false");
                    ESPMan_Debugf("*** CALLBACK: hasConfig = %s\n", (newsettings->hasConfig) ? "true" : "false");


                    int ERROR = _initialiseSTA(*newsettings);

                    ESPMan_Debugf("*** CALLBACK: ERROR = %i\n", ERROR);


                    //WiFi.printDiag(Serial);

                    if (!ERROR || (ERROR == STA_DISABLED && newsettings->enabled == false)) {
                        ESPMan_Debugf("[ESPmanager::_HandleDataRequest()] CALLBACK: Settings successfull\n");

                        if (!_settings) {
                            _getAllSettings();
                        }


                        if (_settings) {
                            _settings->STA = *newsettings;
                            _settings->changed = true;
                            ESPMan_Debugf("[ESPmanager::_HandleDataRequest()] CALLBACK: Settings Applied\n");
                            // _dumpSettings();
                            _events.send("Success", nullptr, 0, 5000);
                            //save_flag = true;
                        } else {
                            event_printf(NULL, string_ERROR, SETTINGS_NOT_IN_MEMORY);
                        }

                    } else {
                        ESPMan_Debugf("[ESPmanager::_HandleDataRequest()] ERORR: Settings NOT applied successfull %i\n", ERROR);
                        event_printf(NULL, string_ERROR, ERROR);
                        _getAllSettings();
                        if (_settings) {
                            if (_initialiseSTA(_settings->STA)) {
                                ESPMan_Debugf("[ESPmanager::_HandleDataRequest()] OLD settings reapplied\n");
                            }
                        }
                    }

                    delete newsettings;

                    return true;
                };
            } else {
                event_printf(NULL, "No Changes Made");
                ESPMan_Debugf("[ESPmanager::_HandleDataRequest()] No changes Made\n");

            }
        }
    }

    /*------------------------------------------------------------------------------------------------------------------

                                     AP config
       ------------------------------------------------------------------------------------------------------------------*/

    if (request->hasParam("enable-AP", true)) {

        bool changes = false;
        bool abortchanges = false;

        settings_t::AP_t * newsettings = new settings_t::AP_t(set.AP); // creates a copy of current settings using new... smart_Ptr don't work well yet for lambda captures

        if (newsettings) {

            /*
                    ENABLED
             */

            newsettings->ssid = set.GEN.host();

            bool enabled = request->getParam("enable-AP", true)->value().equals("on");

            if (enabled != newsettings->enabled) {
                newsettings->enabled = enabled;
                changes = true;
            }

            if (request->hasParam(string_pass, true)) {

                String S_pass = request->getParam(string_pass, true)->value();
                const char * pass = S_pass.c_str();

                if (pass && strlen(pass) > 0 && (strlen(pass) > 63 || strlen(pass) < 8)) {
                    // fail passphrase to long or short!
                    ESPMan_Debugf("[AP] fail passphrase to long or short!\n");
                    event_printf(nullptr, string_ERROR, PASSWOROD_INVALID);
                    abortchanges = true;
                }

                if (pass && newsettings->pass != pass) {
                    newsettings->pass = pass;
                    ESPMan_Debugf("[ESPmanager::_HandleDataRequest] New AP pass = %s\n", newsettings->pass() );
                    changes = true;
                }

            }


            if (request->hasParam(string_channel, true)) {
                int channel = request->getParam(string_channel, true)->value().toInt();

                if (channel > 13) {
                    channel = 13;
                }

                if (channel != newsettings->channel) {
                    newsettings->channel = channel;
                    changes = true;
                    ESPMan_Debugf("[ESPmanager::_HandleDataRequest] New Channel = %u\n", newsettings->channel );
                }


            }

            if (request->hasParam(string_IP, true)) {

                IPAddress newIP;
                bool result = newIP.fromString(request->getParam(string_IP, true)->value());


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

                    ESPMan_Debugf("[ESPmanager::_HandleDataRequest] New AP IP = %s\n", newsettings->IP.toString().c_str() );
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
            //                 ESPMan_Debugf("[%u]:[%u]:[%u]:[%u]:[%u]:[%u]\n", newsettings->MAC[0], newsettings->MAC[1], newsettings->MAC[2], newsettings->MAC[3], newsettings->MAC[4], newsettings->MAC[5]);
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

                _syncCallback = [this, newsettings] () {

                    using namespace ESPMAN;

                    _events.send("Updating AP Settings", nullptr, 0, 5000);
                    delay(10);

                    int ERROR = _initialiseAP(*newsettings);

                    if (!ERROR || (ERROR == AP_DISABLED && newsettings->enabled == false)) {
                        ESPMan_Debugf("[ESPmanager::_HandleDataRequest()] AP CALLBACK: Settings successfull\n");

                        if (!_settings) {
                            _getAllSettings();
                        }


                        if (_settings) {
                            _settings->AP = *newsettings;
                            _settings->changed = true;
                            ESPMan_Debugf("[ESPmanager::_HandleDataRequest()] CALLBACK: Settings Applied\n");
                            //_dumpSettings();
                            _events.send("Success", nullptr, 0, 5000);
                            //save_flag = true;
                        } else {
                            event_printf(NULL, string_ERROR, SETTINGS_NOT_IN_MEMORY);
                        }

                    } else {

                        _getAllSettings();

                        if (_settings) {
                            ESPMan_Debugf("[ESPmanager::_HandleDataRequest()] Restoring old settings ERROR = %i\n", ERROR);

                            _initialiseAP(_settings->AP);

                        }


                        event_printf(NULL, string_ERROR, ERROR);
                    }

                    delete newsettings;

                    return true;
                };
            } else {
                event_printf(NULL, "No Changes Made");
                ESPMan_Debugf("[ESPmanager::_HandleDataRequest()] No changes Made\n");

            }
        }
    } //  end of enable-AP

    /*------------------------------------------------------------------------------------------------------------------

                                     Device Name
       ------------------------------------------------------------------------------------------------------------------*/

    if ( request->hasParam(string_deviceid, true)) {

        String newidString = request->getParam(string_deviceid, true)->value();
        const char * newid = newidString.c_str();
        ESPMan_Debugf( "Device ID func hit %s\n", newid  );

        if (newid && strlen(newid) > 0 && strlen(newid) < 32 && set.GEN.host != newid) {

            set.GEN.host = newid;
            set.changed = true;
            event_printf(NULL, "Device ID: %s", set.GEN.host() );

            _events.send(string_saveandreboot, nullptr, 0, 5000);
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

    if ( request->hasParam(string_OTAupload, true)) {

        // save_flag = true;

        bool command =  request->getParam(string_OTAupload, true)->value().equals( "on");

        if (command != _OTAupload) {

            _OTAupload = command;
            set.GEN.OTAupload = command;
            set.changed = true;

            ESPMan_Debugf("[ESPmanager::handle()] _OTAupload = %s\n", (_OTAupload) ? "enabled" : "disabled");


        }

    } // end of OTA enable


    if ( request->hasParam(string_OTApassword, true)) {

        char pass_confirm[25];

        strcpy(pass_confirm, string_OTApassword);
        strcat(pass_confirm, "_confirm");

        if (request->hasParam(pass_confirm, true) ) {

            String S_pass = request->getParam(string_OTApassword, true)->value();
            String S_confirm = request->getParam(pass_confirm, true)->value();

            const char * pass = S_pass.c_str();
            const char * confirm = S_confirm.c_str();

            if (pass && confirm && !strcmp(pass, confirm))  {

                ESPMan_Debugf("[ESPmanager::_handle] Passwords Match\n");
                set.changed = true;
                // MD5Builder md5;
                // md5.begin();
                // md5.add( pass) ;
                // md5.calculate();
                // set.GEN.OTApassword = md5.toString().c_str() ;
                set.GEN.OTApassword = pass;

                event_printf(nullptr, string_saveandreboot);

            } else {
                event_printf(nullptr, string_ERROR, PASSWORD_MISMATCH);
            }
        }

    } // end of OTApass



    /*
       ARG: 0, "enable-AP" = "on"
       ARG: 1, "setAPsetip" = "0.0.0.0"
       ARG: 2, "setAPsetmac" = "1A%3AFE%3A34%3AA4%3A4C%3A73"
     */

    if ( request->hasParam(string_mDNS, true)) {


        save_flag = true;

        bool command = request->getParam(string_mDNS, true)->value().equals("on");

        if (command != set.GEN.mDNSenabled ) {
            set.GEN.mDNSenabled = command;
            set.changed = true;
            ESPMan_Debugf("mDNS set to : %s\n", (command) ? "on" : "off");
            //  _events.send(string_saveandreboot);
            //  InitialiseFeatures();
        }
    } // end of OTA enable
    /*------------------------------------------------------------------------------------------------------------------

                                       PORTAL
       ------------------------------------------------------------------------------------------------------------------*/

    if ( request->hasParam("enablePortal", true)) {

        // save_flag = true;

        bool command =  request->getParam("enablePortal", true)->value().equals( "on");

        if (command != _settings->GEN.portal) {

            _settings->GEN.portal = command;
            set.changed = true;

            ESPMan_Debugf("[ESPmanager::handle()] settings->GEN.portal = %s\n", (command) ? "enabled" : "disabled");


        }

    } // end of OTA enable
    /*------------------------------------------------------------------------------------------------------------------

                                       MAC address STA + AP
       ------------------------------------------------------------------------------------------------------------------*/





    /*------------------------------------------------------------------------------------------------------------------

                                       AP reboot behaviour
       ------------------------------------------------------------------------------------------------------------------*/

    if (request->hasParam(string_ap_boot_mode, true) ) {

        int rebootvar = request->getParam(string_ap_boot_mode, true)->value().toInt();


        ap_boot_mode_t value = (ap_boot_mode_t)rebootvar;

        if (value != set.GEN.ap_boot_mode) {
            ESPMan_Debugf("Recieved AP behaviour set to: %i\n", rebootvar);
            set.GEN.ap_boot_mode = value;
            _ap_boot_mode = value;
            set.changed = true;

        }
    }

    if (request->hasParam(string_no_sta_mode, true) ) {

        int var = request->getParam(string_no_sta_mode, true)->value().toInt();

        no_sta_mode_t value = (no_sta_mode_t)var;

        if (value != set.GEN.no_sta_mode) {

            ESPMan_Debugf("Recieved WiFi Disconnect behaviour set to: %i\n", var);
            set.GEN.no_sta_mode = value;
            _no_sta_mode = value;
            set.changed = true;

        }
    }



    // if (request->hasParam(string_usePerminantSettings, true) ) {

    //     bool var = request->getParam(string_usePerminantSettings, true)->value().equals("on");

    //     if (var != set.GEN.usePerminantSettings) {
    //         ESPMan_Debugf("Recieved usePerminantSettings Set To: %s\n", (var) ? "on" : "off");
    //         set.GEN.usePerminantSettings = var;
    //         set.changed = true;

    //     }
    // }

    /*------------------------------------------------------------------------------------------------------------------

                                            New UPGRADE
       ------------------------------------------------------------------------------------------------------------------*/

    if (request->hasParam(string_updateURL, true) ) {

        String S_newpath = request->getParam(string_updateURL, true)->value();

        const char * newpath = S_newpath.c_str();

        ESPMan_Debugf("[ESPmanager::_HandleDataRequest] UpgradeURL: %s\n", newpath);

        if (newpath && strlen(newpath) > 0 && set.GEN.updateURL != newpath) {

            set.GEN.updateURL = newpath;
            set.changed = true;
        }
    }

    if (request->hasParam(string_updateFreq, true) ) {

        int updateFreq = request->getParam(string_updateFreq, true)->value().toInt();

        if (updateFreq < 0) {
            updateFreq = 0;
        }

        if (updateFreq != set.GEN.updateFreq) {
            set.GEN.updateFreq = updateFreq;
            set.changed = true;
        }


    }

    if (request->hasParam("PerformUpdate", true) ) {

        myString path = set.GEN.updateURL;


        _syncCallback = [this, path ]() {
            _upgrade(path());
            return true;
        };

    }





    // DynamicJsonBuffer jsonbuffer;
    // JsonObject & root = jsonbuffer.createObject();

    root[string_changed] = (set.changed) ? true : false;
    root[F("heap")] = ESP.getFreeHeap();

    sendJsontoHTTP<JsonObject>(root, request);


    // AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
    // response->addHeader(ESPMAN::string_CORS,"*");
    // response->addHeader(ESPMAN::string_CACHE_CONTROL,"no-store");
    // request->send(response);
}








// struct tm * ESPmanager::getCompileTime(){
//         const char * months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
//         const char * compile_date = __DATE__;
//         const char * compile_time = __TIME__;
//
//         int year = atoi(compile_date+7),
//             month = 0,
//             day = atoi(compile_date+4),
//             hour = atoi(compile_time),
//             minute = atoi(compile_time+3),
//             second = atoi(compile_time+6);
//
//         int i;
//         for(i=0; i<12; i++) {
//
//                 if(memcmp(compile_date, months[i], 3) == 0) {
//                         month = i;
//                 }
//         }
//
//         static struct tm * timeinfo = (struct tm *)malloc(sizeof(struct tm));
//         timeinfo->tm_year = year - 1900;
//         timeinfo->tm_mon = month;
//         timeinfo->tm_mday = day;
//         timeinfo->tm_hour = hour;
//         timeinfo->tm_min = minute;
//         timeinfo->tm_sec = second;
//         mktime(timeinfo);
//         return timeinfo;
// }

// char * ESPmanager::buildTime(){
//   static char result[30];
//   struct tm * timeinfo = getCompileTime();
//   strftime (result,30,"%a, %d %b %Y %H:%M:%S %Z",timeinfo);
//   return result;
// }

// void ESPmanager::_handleManifest(AsyncWebServerRequest *request)
// {

// #ifdef DISABLE_MANIFEST
// #pragma message MANIFEST DISABLED
//     request->send(404);
//     return;
// #endif

//     AsyncResponseStream *response = request->beginResponseStream(F("text/cache-manifest")); //Sends 404 File Not Found
//     response->addHeader(ESPMAN::string_CACHE_CONTROL, F( "must-revalidate"));
//     response->print(F("CACHE MANIFEST\n"));
//     response->printf( "# %s\n", __DATE__ " " __TIME__ );

//     if (_randomvalue) {
//         response->printf(  "# %u\n", _randomvalue );
//     }

//     response->print(F("CACHE:\n"));
//     response->print(F("../jquery/jqm1.4.5.css\n"));
//     response->print(F("../jquery/jq1.11.1.js\n"));
//     response->print(F("../jquery/jqm1.4.5.js\n"));
//     response->print(F("../jquery/images/ajax-loader.gif\n"));
//     response->print(F("NETWORK:\n"));
//     response->print(F("index.htm\n"));
//     response->print(F("espman.js\n"));
//     response->print("*\n");
//     request->send(response);

// }


/*


      NEW FUNCTIONS FOR AP LOGIC.....

 */


/*


      AP  stuff


 */




int ESPmanager::_initialiseAP(bool override)
{
    using namespace ESPMAN;
    int ERROR = 0;

    //  get the settings from SPIFFS if settings PTR is null
    if (!_settings) {
        _getAllSettings();
    }

    _settings->AP.ssid = _settings->GEN.host;

    //  return error code if override is false
    if (override) {
        ESPMan_Debugf("**** OVERRIDING AP SETTINGS AND TURNING AP ON!! ****\n");
        _settings->AP.enabled = true;
    }

    return _initialiseAP(_settings->AP);

}

int ESPmanager::_initialiseAP( settings_t::AP_t & settings )
{
    using namespace ESPMAN;


#ifdef Debug_ESPManager
    ESPMan_Debugf("-------  PRE CONFIG ------\n");
    _dumpAP(settings);
    ESPMan_Debugf("--------------------------\n");
#endif

    if (settings.enabled == false  ) {
        ESPMan_Debugf("[ESPmanager::_initialiseAP( settings_t::AP_t & settings )] AP DISABLED\n");
        if (WiFi.enableAP(false)) {
            return AP_DISABLED;
        } else {
            return ERROR_DISABLING_AP;
        }
    }

    //settings.channel = 1;

    ESPMan_Debugf("[ESPmanager::_initialiseAP] ( settings_t::AP_t & settings )] ENABLING AP : channel %u\n", settings.channel);

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



    if (!settings.ssid() ) {
        char buf[33] = {'\0'};
        snprintf(&buf[0], 32, "esp8266-%06x", ESP.getChipId());
        settings.ssid = buf;
    }


    if (!WiFi.softAP(settings.ssid(), settings.pass(), settings.channel, !settings.visible )) {
        return ERROR_ENABLING_AP;
    }


    return 0;

}


/*


      STA  stuff


 */

int ESPmanager::_initialiseSTA()
{
    using namespace ESPMAN;
    int ERROR = 0;

    if (!_settings) {
        _getAllSettings();
    }

    if (_settings) {
        ERROR = _initialiseSTA(_settings->STA);
        if (!ERROR) {
            if (_settings->GEN.host() && !WiFi.hostname(_settings->GEN.host())) {
                ESPMan_Debugf("[ESPmanager::_initialiseSTA()] ERROR setting Hostname\n");
            } else {

                ESPMan_Debugf("[ESPmanager::_initialiseSTA()] Hostname set : %s\n", _settings->GEN.host() );
            }
            ESPMan_Debugf("[ESPmanager::_initialiseSTA()] IP = %s\n", WiFi.localIP().toString().c_str() );
            return 0;
        } else {
            return ERROR;
        }
    } else {
        return MALLOC_FAIL;
    }

}

int ESPmanager::_initialiseSTA( settings_t::STA_t & set)
{
    using namespace ESPMAN;
    int ERROR = 0;
    bool portal_enabled = _dns;

    if (portal_enabled) {
        disablePortal();
    }

#ifdef Debug_ESPManager
    ESPMan_Debugf("-------  PRE CONFIG ------\n");
    _dumpSTA(set);
    ESPMan_Debugf("--------------------------\n");
#endif

    if (!set.enabled) {
        if (WiFi.enableSTA(false)) {
            return STA_DISABLED;
        } else {
            return ERROR_DISABLING_STA;
        }
    }



    if (!set.ssid() || ( set.ssid() && strlen(set.ssid() ) == 0)) {
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
        ESPMan_Debugf("[ESPmanager::_initialiseSTA( settings_t::STA_t & settings)] IP %s\n", set.IP.toString().c_str() );
        ESPMan_Debugf("[ESPmanager::_initialiseSTA( settings_t::STA_t & settings)] GW %s\n", set.GW.toString().c_str());
        ESPMan_Debugf("[ESPmanager::_initialiseSTA( settings_t::STA_t & settings)] SN %s\n", set.SN.toString().c_str());
        ESPMan_Debugf("[ESPmanager::_initialiseSTA( settings_t::STA_t & settings)] DNS1 %s\n", set.DNS1.toString().c_str());
        ESPMan_Debugf("[ESPmanager::_initialiseSTA( settings_t::STA_t & settings)] DNS2 %s\n", set.DNS2.toString().c_str());

        WiFi.begin();

        // check if they are valid...
        if (!WiFi.config( set.IP, set.GW, set.SN, set.DNS1, set.DNS2))
            //if (!WiFi.config( settings.IP, settings.GW, settings.SN ))
        {
            return WIFI_CONFIG_ERROR;
        } else {
            set.dhcp = false;
            ESPMan_Debugf("[ESPmanager::_initialiseSTA( settings_t::STA_t & settings)] Config Applied\n");
        }

    } else {
        set.dhcp = true;
        WiFi.config( INADDR_NONE, INADDR_NONE, INADDR_NONE);
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

    if (WiFi.isConnected() && WiFi.SSID() == set.ssid() && WiFi.psk() == set.pass()  ) {
        ESPMan_Debugf( "Reconnecting WiFi... \n" );
        WiFi.reconnect();
    } else {

        if (  set.ssid() && set.pass()  ) {
            ESPMan_Debugf( "!!! ssid = %s, pass = %s\n", set.ssid(), set.pass());
            if (!WiFi.begin( set.ssid(), set.pass())) {
                return ERROR_WIFI_BEGIN;
            }

        } else if ( set.ssid() ) {
            ESPMan_Debugf( "ssid = %s\n", set.ssid());
            if (!WiFi.begin( set.ssid())) {
                return ERROR_WIFI_BEGIN;
            }
        }
    }

    ESPMan_Debugf("[ESPmanager::_initialiseSTA] Begin Done\n");

    uint32_t start_time = millis();

    uint8_t result = WL_DISCONNECTED;

    // if (_dns) {
    //     _dns->stop();
    // }

    //WiFiUDP::stopAll();

    while (result = WiFi.waitForConnectResult(), result != WL_CONNECTED) {
        delay(10);
        // if (_dns) {
        //     _dns->processNextRequest();
        // }
        if (millis() - start_time > 60000) {
            ESPMan_Debugf("[ESPmanager::_initialiseSTA] ABORTING CONNECTION TIMEOUT\n");
            break;

        }
    }

    // Serial.print("Actual SSID = ");
    // Serial.println(WiFi.SSID());
    // Serial.print("Actual PASS = ");
    // Serial.println(WiFi.psk());

    // if (_dns) {
    //     _dns->setErrorReplyCode(DNSReplyCode::NoError);
    //     _dns->start(DNS_PORT, "*", IPAddress(192,168,4,1));
    // }

    //result = WiFi.waitForConnectResult();

    // yield();

    // if (result != WL_CONNECTED) {
    //     ESPMan_Debugf("[ESPmanager::_initialiseSTA] Trying Second Time\n");
    //     while (result = WiFi.waitForConnectResult(), result != WL_CONNECTED) { yield(); }
    // }

    if (portal_enabled) {
        enablePortal();
    }

    ESPMan_Debugf("[ESPmanager::_initialiseSTA] connRes = %u, time = %ums\n", result, millis() - start_time);

    if ( result == WL_CONNECTED ) {

        return 0;
    }

    return CONNECT_FAILED;


}

// int ESPmanager::_autoSDKconnect()
// {
//     using namespace ESPMAN;

//     WiFi.begin();

//     wl_status_t status = (wl_status_t)WiFi.waitForConnectResult();

//     switch (status) {
//     case WL_CONNECTED:
//         return 0;
//         break;
//     case WL_NO_SSID_AVAIL:
//         return NO_SSID_AVAIL;
//         break;
//     case WL_CONNECT_FAILED:
//         return CONNECT_FAILED;
//         break;
//     default:
//         ESPMan_Debugf("[ESPmanager::_autoSDKconnect()] WiFi Error %i\n", status);
//         return UNKNOWN_ERROR;
//         break;
//     }

//     ESPMan_Debugf("[ESPmanager::_autoSDKconnect()] done\n");

// }




//  allows creating of a seperate config
//  need to add in captive portal to setttings....
int ESPmanager::_emergencyMode(bool shutdown)
{
    using namespace ESPMAN;
    ESPMan_Debugf("***** EMERGENCY mode **** \n");

    uint32_t channel = WiFi.channel(); // cache the channel for AP.

    channel = 1; // cache the channel for AP.

    if (shutdown) {
        WiFi.disconnect(true); //  Disable STA. makes AP more stable, stops 1sec reconnect
    }

    _APtimer = millis();

    //  creats a copy of settings so they are not changed...
    settings_t set;
    _getAllSettings(set);

    set.AP.ssid = set.GEN.host;
    set.AP.channel = channel;



    // if (set.GEN.usePerminantSettings && _perminant_host) {
    //     set.AP.ssid = _perminant_host;
    // }

    set.AP.enabled = true;

    ESPMan_Debugf("*****  Debug:  WiFi channel in EMERGENCY mode = %u\n", set.AP.channel);

    return _initialiseAP(set.AP);


}


// int ESPmanager::setSSID(const char * ssid, const char * pass)
// {
//   using namespace ESPMAN;
//
//
// }





/*


      Namespace ESPMAN  JSONpackage


 */



// ToDo......


int ESPmanager::_getAllSettings()
{

    using namespace ESPMAN;


    if (!_settings) {
        _settings = new settings_t;
    }

    if (!_settings) {
        return MALLOC_FAIL;
    }

    if (_settings->changed) {
        return 0; // dont overwrite changes already in memory...
    }

    int ERROR = 0;

    ERROR =  _getAllSettings(*_settings);

    if (!ERROR) {
        _ap_boot_mode = _settings->GEN.ap_boot_mode;
        _no_sta_mode = _settings->GEN.no_sta_mode;
        _updateFreq = _settings->GEN.updateFreq;
        _OTAupload = _settings->GEN.OTAupload;
        _settings->configured = true;
        ESPMan_Debugf("[ESPmanager::_getAllSettings()] _ap_boot_mode = %i, _no_sta_mode = %i, _updateFreq = %u, IDEupload = %s\n", (int)_ap_boot_mode, (int)_no_sta_mode, _updateFreq, (_OTAupload) ? "enabled" : "disabled" );
    } else {
        _settings->configured = false;
    }

    //_applyPermenent(*settings);

    if (!_settings->GEN.host()) {
        char tmp[33] = {'\0'};
        snprintf(tmp, 32, "esp8266-%06x", ESP.getChipId());
        _settings->GEN.host = tmp;
    }

    return ERROR;


}

int ESPmanager::_getAllSettings(settings_t & set)
{

    using namespace ESPMAN;

    JSONpackage json;
    uint8_t settingsversion = 0;
    uint32_t start_heap = ESP.getFreeHeap();

    int ERROR = 0;

    ERROR = json.parseSPIFS(SETTINGS_FILE);

    if (ERROR) {
        return ERROR;
    }

    JsonObject & root = json.getRoot();

    /*
          General Settings
     */

    if (root.containsKey(string_General)) {

        JsonObject & settingsJSON = root[string_General];

        if (settingsJSON.containsKey(string_settingsversion)) {
            settingsversion = settingsJSON[string_settingsversion];
        }

        if (settingsJSON.containsKey(string_host)) {
            set.GEN.host = settingsJSON[string_host].asString();
        }

        if (settingsJSON.containsKey(string_mDNS)) {
            set.GEN.mDNSenabled = settingsJSON[string_mDNS];
        }

        if (settingsJSON.containsKey(string_updateURL)) {
            set.GEN.updateURL = settingsJSON[string_updateURL].asString();
        }

        if (settingsJSON.containsKey(string_updateFreq)) {
            set.GEN.updateFreq = settingsJSON[string_updateFreq];
        }

        if (settingsJSON.containsKey(string_OTApassword)) {
            set.GEN.OTApassword = settingsJSON[string_OTApassword].asString();
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
        if (settingsJSON.containsKey(string_GUIhash)) {
            set.GEN.GUIhash = settingsJSON[string_GUIhash].asString();
        }

        // if (settingsJSON.containsKey(string_usePerminantSettings)) {
        //     set.GEN.usePerminantSettings = settingsJSON[string_usePerminantSettings];
        // } else if ( _perminant_host || _perminant_ssid || _perminant_pass ) {
        //     set.GEN.usePerminantSettings = true;
        // } else {
        //     set.GEN.usePerminantSettings = false;
        // }

        if (settingsJSON.containsKey(string_ap_boot_mode)) {

            int val = settingsJSON[string_ap_boot_mode];
            set.GEN.ap_boot_mode = (ap_boot_mode_t)val;
            //ESPMan_Debugf("[_getAllSettings] set.GEN.ap_boot_mode = %i\n", val);
        }
        if (settingsJSON.containsKey(string_no_sta_mode)) {
            int val = settingsJSON[string_no_sta_mode];
            set.GEN.no_sta_mode = (no_sta_mode_t)val;
            //ESPMan_Debugf("[_getAllSettings] set.GEN.no_sta_mode = %i\n", val);
        }

        if (settingsJSON.containsKey(string_OTAupload)) {
            set.GEN.OTAupload = settingsJSON[string_OTAupload];
        }

    }

    /*
           STA settings
     */

    if (root.containsKey(string_STA)) {


        JsonObject & STAjson = root[string_STA];

        if (STAjson.containsKey(string_enabled)) {
            set.STA.enabled = STAjson[string_enabled];
        }

        if (STAjson.containsKey(string_ssid)) {
            if (strlen(STAjson[string_ssid]) < MAX_SSID_LENGTH) {

                set.STA.ssid = STAjson[string_ssid].asString();
                //strncpy( &settings.ssid[0], STAjson["ssid"], strlen(STAjson["ssid"]) );
            }
        }

        if (STAjson.containsKey(string_pass)) {
            if (strlen(STAjson[string_pass]) < MAX_PASS_LENGTH) {
                set.STA.pass = STAjson[string_pass].asString();
                //strncpy( &settings.pass[0], STAjson["pass"], strlen(STAjson["pass"]) );
            }
        }

        if (STAjson.containsKey(string_IP) && STAjson.containsKey(string_GW) && STAjson.containsKey(string_SN) && STAjson.containsKey(string_DNS1)) {
            //set.STA.hasConfig = true;
            set.STA.IP = IPAddress( STAjson[string_IP][0], STAjson[string_IP][1], STAjson[string_IP][2], STAjson[string_IP][3] );
            set.STA.GW = IPAddress( STAjson[string_GW][0], STAjson[string_GW][1], STAjson[string_GW][2], STAjson[string_GW][3] );
            set.STA.SN = IPAddress( STAjson[string_SN][0], STAjson[string_SN][1], STAjson[string_SN][2], STAjson[string_SN][3] );
            set.STA.DNS1 = IPAddress( STAjson[string_DNS1][0], STAjson[string_DNS1][1], STAjson[string_DNS1][2], STAjson[string_DNS1][3] );

            if ( STAjson.containsKey(string_DNS2)) {
                set.STA.DNS2 = IPAddress( STAjson[string_DNS2][0], STAjson[string_DNS2][1], STAjson[string_DNS2][2], STAjson[string_DNS2][3] );
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
            ESPMan_Debugf("[ESPmanager::_getAllSettings(settings_t & set)] dhcp set true\n");
        }

        if (STAjson.containsKey(string_autoconnect)) {
            set.STA.autoConnect = STAjson[string_autoconnect];
        }

        if (STAjson.containsKey(string_autoreconnect)) {
            set.STA.autoReconnect = STAjson[string_autoreconnect];
        }

        if (STAjson.containsKey(string_MAC)) {

            set.STA.hasMAC = true;

            for (uint8_t i = 0; i < 6; i++) {
                set.STA.MAC[i] = STAjson[string_MAC][i];
            }

        }

    }


    /*
           AP settings
     */
    if (root.containsKey(string_AP)) {

        JsonObject & APjson = root[string_AP];

        if (APjson.containsKey(string_enabled)) {
            set.AP.enabled = APjson[string_enabled];
            //Serial.printf("set.AP.enabled = %s\n", (set.AP.enabled)? "true": "false");
        }

        if (APjson.containsKey(string_pass)) {
            //settings.hasPass = true;
            if (strlen(APjson[string_pass]) < MAX_PASS_LENGTH) {

                set.AP.pass = APjson[string_pass].asString();
            }
        }

        if (APjson.containsKey(string_IP)) {
            set.STA.hasConfig = true;
            set.STA.IP = IPAddress( APjson[string_IP][0], APjson[string_IP][1], APjson[string_IP][2], APjson[string_IP][3] );
            set.STA.GW = IPAddress( APjson[string_GW][0], APjson[string_GW][1], APjson[string_GW][2], APjson[string_GW][3] );
            set.STA.SN = IPAddress( APjson[string_SN][0], APjson[string_SN][1], APjson[string_SN][2], APjson[string_SN][3] );


        }

        if (APjson.containsKey(string_visible)) {
            set.AP.visible = true;
        }

        if (APjson.containsKey(string_channel)) {
            set.AP.channel = APjson[string_channel];
        }

        if (APjson.containsKey(string_MAC)) {

            set.AP.hasMAC = true;

            for (uint8_t i = 0; i < 6; i++) {
                set.AP.MAC[i] = APjson[string_MAC][i];
            }

        }

    }

    if (settingsversion != SETTINGS_FILE_VERSION) {
        ESPMan_Debugf("[ESPmanager::_getAllSettings(settings_t & set)] Settings File Version Wrong expecting:%u got:%u\n", SETTINGS_FILE_VERSION, settingsversion);
        return WRONG_SETTINGS_FILE_VERSION;
    }

}

int ESPmanager::_saveAllSettings(settings_t & set)
{

    using namespace ESPMAN;

    DynamicJsonBuffer jsonBuffer;
    JsonObject & root = jsonBuffer.createObject();

    /*
            General Settings
     */
    JsonObject & settingsJSON = root.createNestedObject(string_General);

    settingsJSON[string_mDNS] = set.GEN.mDNSenabled;

    settingsJSON[string_settingsversion] = SETTINGS_FILE_VERSION;

    if (set.GEN.host) {
        settingsJSON[string_host] = set.GEN.host();
    }

    if (set.GEN.updateURL) {
        settingsJSON[string_updateURL] = set.GEN.updateURL();
    }

    settingsJSON[string_updateFreq] = set.GEN.updateFreq;

    if (set.GEN.OTApassword) {
        settingsJSON[string_OTApassword] = set.GEN.OTApassword();
    }

    // if (set.GEN.GUIusername) {
    //         settingsJSON[string_GUIusername] = set.GEN.GUIusername;
    // }
    //
    // if (set.GEN.GUIpassword) {
    //         settingsJSON[string_GUIpassword] = set.GEN.GUIpassword;
    // }

    if (set.GEN.GUIhash) {
        settingsJSON[string_GUIhash] = set.GEN.GUIhash();
    }


    //settingsJSON[string_usePerminantSettings] = (set.GEN.usePerminantSettings) ? true : false;

    settingsJSON[string_ap_boot_mode] = (int)set.GEN.ap_boot_mode;
    settingsJSON[string_no_sta_mode] = (int)set.GEN.no_sta_mode;

    settingsJSON[string_OTAupload] = set.GEN.OTAupload;



    /*****************************************
            STA Settings
    *****************************************/

    JsonObject & STAjson = root.createNestedObject(string_STA);

    STAjson[string_enabled] = set.STA.enabled;

    if (set.STA.ssid) {
        STAjson[string_ssid] = set.STA.ssid();
    }

    if (set.STA.pass) {
        STAjson[string_pass] = set.STA.pass();

    }

    if (set.STA.hasConfig) {

        JsonArray & IP = STAjson.createNestedArray(string_IP);
        IP.add(set.STA.IP[0]);
        IP.add(set.STA.IP[1]);
        IP.add(set.STA.IP[2]);
        IP.add(set.STA.IP[3]);
        JsonArray & GW = STAjson.createNestedArray(string_GW);
        GW.add(set.STA.GW[0]);
        GW.add(set.STA.GW[1]);
        GW.add(set.STA.GW[2]);
        GW.add(set.STA.GW[3]);
        JsonArray & SN = STAjson.createNestedArray(string_SN);
        SN.add(set.STA.SN[0]);
        SN.add(set.STA.SN[1]);
        SN.add(set.STA.SN[2]);
        SN.add(set.STA.SN[3]);
        JsonArray & DNS1 = STAjson.createNestedArray(string_DNS1);
        DNS1.add(set.STA.DNS1[0]);
        DNS1.add(set.STA.DNS1[1]);
        DNS1.add(set.STA.DNS1[2]);
        DNS1.add(set.STA.DNS1[3]);

        if (set.STA.DNS2 != INADDR_NONE) {
            JsonArray & DNS2 = STAjson.createNestedArray(string_DNS2);
            DNS2.add(set.STA.DNS2[0]);
            DNS2.add(set.STA.DNS2[1]);
            DNS2.add(set.STA.DNS2[2]);
            DNS2.add(set.STA.DNS2[3]);

        }


    }

    if (set.STA.hasMAC) {
        JsonArray & MAC = STAjson.createNestedArray(string_MAC);

        for (uint8_t i = 0; i < 6; i++) {
            MAC.add(set.STA.MAC[i]);
        }

    }


    STAjson[string_autoconnect] = set.STA.autoConnect;
    STAjson[string_autoreconnect] = set.STA.autoReconnect;



    /****************************************
            AP Settings
    ****************************************/

    JsonObject & APjson = root.createNestedObject(string_AP);;

    APjson[string_enabled] = set.AP.enabled;

    //  disbale this for now.. all set via host.
    //
    // if (set.AP.ssid()) {
    //         APjson[string_ssid] = set.AP.ssid();
    // }

    if (set.AP.pass) {
        APjson[string_pass] = set.AP.pass();

    }

    if (set.AP.hasConfig) {

        JsonArray & IP = APjson.createNestedArray(string_IP);
        IP.add(set.AP.IP[0]);
        IP.add(set.AP.IP[1]);
        IP.add(set.AP.IP[2]);
        IP.add(set.AP.IP[3]);
        JsonArray & GW = APjson.createNestedArray(string_GW);
        GW.add(set.AP.GW[0]);
        GW.add(set.AP.GW[1]);
        GW.add(set.AP.GW[2]);
        GW.add(set.AP.GW[3]);
        JsonArray & SN = APjson.createNestedArray(string_SN);
        SN.add(set.AP.SN[0]);
        SN.add(set.AP.SN[1]);
        SN.add(set.AP.SN[2]);
        SN.add(set.AP.SN[3]);

    }

    if (set.AP.hasMAC) {
        JsonArray & MAC = APjson.createNestedArray(string_MAC);

        for (uint8_t i = 0; i < 6; i++) {
            MAC.add(set.AP.MAC[i]);
        }

    }

    APjson[string_visible] = set.AP.visible;
    APjson[string_channel] = set.AP.channel;


    File f = _fs.open(SETTINGS_FILE, "w");

    if (!f) {
        return SPIFFS_FILE_OPEN_ERROR;
    }

    root.prettyPrintTo(f);

    f.close();

    return 0;

}


// void ESPmanager::_applyPermenent(settings_t & set)
// {

//     if (set.GEN.usePerminantSettings) {

//         if (_perminant_host) {
//             ESPMan_Debugf("[ESPmanager::_getAllSettings] Host override: %s\n", _perminant_host);
//             set.GEN.host = _perminant_host;
//         }

//         if (_perminant_ssid) {
//             ESPMan_Debugf("[ESPmanager::_getAllSettings] SSID override: %s\n", _perminant_ssid);
//             set.STA.ssid = _perminant_ssid;
//             set.STA.enabled = true;
//         }

//         if (_perminant_pass) {
//             ESPMan_Debugf("[ESPmanager::_getAllSettings] PASS override: %s\n", _perminant_pass);
//             set.STA.pass = _perminant_pass;
//         }
//     }
// }


// const char * ESPmanager::_getError(ESPMAN::ESPMAN_ERR err)
// {
//         using namespace ESPMAN;
//
//         switch (err) {
//         case UNKNOWN_ERROR:
//                 return String(F("UNKNOWN_ERROR")).c_str();
//         case NO_UPDATE_URL:
//                 return String(F("NO_UPDATE_URL")).c_str();
//         case SPIFFS_FILES_ABSENT:
//                 return String(F("SPIFFS_FILES_ABSENT")).c_str();
//         case FILE_NOT_CHANGED:
//                 return String(F("FILE_NOT_CHANGED")).c_str();
//         case MD5_CHK_ERROR:
//                 return String(F("MD5_CHK_ERROR")).c_str();
//         case HTTP_ERROR:
//                 return String(F("HTTP_ERROR")).c_str();
//         case JSON_PARSE_ERROR:
//                 return String(F("JSON_PARSE_ERROR")).c_str();
//         case JSON_OBJECT_ERROR:
//                 return String(F("JSON_OBJECT_ERROR")).c_str();
//         case CONFIG_FILE_ERROR:
//                 return String(F("CONFIG_FILE_ERROR")).c_str();
//         case UPDATOR_ERROR:
//                 return String(F("UPDATOR_ERROR")).c_str();
//         case JSON_TOO_LARGE:
//                 return String(F("JSON_TOO_LARGE")).c_str();
//         case MALLOC_FAIL:
//                 return String(F("MALLOC_FAIL")).c_str();
//         case MANIFST_FILE_ERROR:
//                 return String(F("MANIFST_FILE_ERROR")).c_str();
//         case UNKNOWN_NUMBER_OF_FILES:
//                 return String(F("UNKNOWN_NUMBER_OF_FILES")).c_str();
//         case SPIFFS_INFO_FAIL:
//                 return String(F("SPIFFS_INFO_FAIL")).c_str();
//         case SPIFFS_FILENAME_TOO_LONG:
//                 return String(F("SPIFFS_FILENAME_TOO_LONG")).c_str();
//         case SPIFFS_FILE_OPEN_ERROR:
//                 return String(F("SPIFFS_FILE_OPEN_ERROR")).c_str();
//         case FILE_TOO_LARGE:
//                 return String(F("FILE_TOO_LARGE")).c_str();
//         case INCOMPLETE_DOWNLOAD:
//                 return String(F("INCOMPLETE_DOWNLOAD")).c_str();
//         case CRC_ERROR:
//                 return String(F("CRC_ERROR")).c_str();
//         case JSON_KEY_MISSING:
//                 return String(F("JSON_KEY_MISSING")).c_str();
//         case EMPTY_BUFFER:
//                 return String(F("EMPTY_BUFFER")).c_str();
//         case AP_DISABLED:
//                 return String(F("AP_DISABLED")).c_str();
//         case ERROR_ENABLING_AP:
//                 return String(F("ERROR_ENABLING_AP")).c_str();
//         default:
//                 return String(F("NO STRING CONVERTION")).c_str();
//
//                 // case: default
//                 //   return " ";
//
//         }
//
//
//         // UNKNOWN_ERROR            = -20,//  start at -20 as we use httpupdate errors
//         // NO_UPDATE_URL            = -21,
//         // SPIFFS_FILES_ABSENT      = -22,
//         // FILE_NOT_CHANGED         = -23,
//         // MD5_CHK_ERROR            = -24,
//         // HTTP_ERROR               = -25,
//         // JSON_PARSE_ERROR         = -26,
//         // JSON_OBJECT_ERROR        = -27,
//         // CONFIG_FILE_ERROR        = -28,
//         // UPDATOR_ERROR            = -29,
//         // JSON_TOO_LARGE           = -30,
//         // MALLOC_FAIL              = -31,
//         // MANIFST_FILE_ERROR       = -32,
//         // UNKNOWN_NUMBER_OF_FILES  = -33,
//         // SPIFFS_INFO_FAIL         = -34,
//         // SPIFFS_FILENAME_TOO_LONG = -35,
//         // SPIFFS_FILE_OPEN_ERROR   = -36,
//         // FILE_TOO_LARGE           = -37,
//         // INCOMPLETE_DOWNLOAD      = -38,
//         // CRC_ERROR                = -39,
//         // JSON_KEY_MISSING         = -40,
//         // EMPTY_BUFFER             = -41,
//         // AP_DISABLED              = -42,
//         // ERROR_ENABLING_AP        = -43,
//         // ERROR_DISABLING_AP       = -44,
//         // ERROR_SETTING_CONFIG     = -45,
//         // ERROR_ENABLING_STA       = -46,
//         // FAILED_SET_AUTOCONNECT   = -47,
//         // FAILED_SET_AUTORECONNECT = -48,
//         // WIFI_CONFIG_ERROR        = -49,
//         // NO_STA_SSID              = -50,
//         // ERROR_WIFI_BEGIN         = -60,
//         // NO_SSID_AVAIL            = -70,
//         // CONNECT_FAILED           = -80,
//         // UNITITIALISED            = -81,
//         // ERROR_SPIFFS_MOUNT       = -82,
//         // AUTO_CONNECTED_STA       = -83,
//         // ERROR_DISABLING_STA      = -84,
//         // STA_DISABLED             = -85,
//
// }

// const char * ESPmanager::_updateUrl()
// {
//         generateDigestHash("abc", "def", "fff");
//
//         settings_t set;
//         int ERROR = _getAllSettings(set);
//         ESPMan_Debugf("[ESPmanager::_updateUrl()] error = %i\n", ERROR);
//         if (!ERROR) {
//                 return set.GEN.updateURL();
//         } else {
//                 return nullptr;
//         }
//
// }







// ESPmanager::settings_t& ESPmanager::settings_t::operator=( const ESPmanager::settings_t &other ) {
//         // x = other.x;
//         // c = other.c;
//         // s = other.s;
//         return *this;
// }







#ifdef Debug_ESPManager

void ESPmanager::_dumpGEN(settings_t::GEN_t & settings)
{

    ESPMan_Debugf("---- GEN ----\n");
    ESPMan_Debugf("host = %s\n", (settings.host()) ? settings.host() : "null" );
    ESPMan_Debugf("updateURL = %s\n", (settings.updateURL()) ? settings.updateURL() : "null" );
    ESPMan_Debugf("updateFreq = %u\n", (uint32_t)settings.updateFreq );
    ESPMan_Debugf("OTAport = %u\n", (uint32_t)settings.OTAport );
    ESPMan_Debugf("mDNSenabled = %s\n", (settings.mDNSenabled) ? "true" : "false" );

    ESPMan_Debugf("OTApassword = %s\n", (settings.OTApassword()) ? settings.OTApassword() : "null" );
    ESPMan_Debugf("GUIhash = %s\n", (settings.GUIhash()) ? settings.GUIhash() : "null" );
    ESPMan_Debugf("ap_boot_mode = %i\n", (int8_t)settings.ap_boot_mode );
    ESPMan_Debugf("no_sta_mode = %i\n", (int8_t)settings.no_sta_mode );
    ESPMan_Debugf("IDEupload = %s\n", (settings.OTAupload) ? "true" : "false" );

}


void ESPmanager::_dumpAP(settings_t::AP_t & settings)
{

    ESPMan_Debugf("---- AP ----\n");
    ESPMan_Debugf("enabled = %s\n", (settings.enabled) ? "true" : "false" );
    ESPMan_Debugf("ssid = %s\n", (settings.ssid()) ? settings.ssid() : "null" );
    ESPMan_Debugf("pass = %s\n", (settings.pass()) ? settings.pass() : "null" );
    ESPMan_Debugf("hasConfig = %s\n", (settings.hasConfig) ? "true" : "false" );
    ESPMan_Debugf("IP = %s\n", settings.IP.toString().c_str() );
    ESPMan_Debugf("GW = %s\n", settings.GW.toString().c_str() );
    ESPMan_Debugf("SN = %s\n", settings.SN.toString().c_str() );
    ESPMan_Debugf("visible = %s\n", (settings.visible) ? "true" : "false" );
    ESPMan_Debugf("channel = %u\n", settings.channel );


}

void ESPmanager::_dumpSTA(settings_t::STA_t & settings)
{

    ESPMan_Debugf("---- STA ----\n");
    ESPMan_Debugf("enabled = %s\n", (settings.enabled) ? "true" : "false" );
    ESPMan_Debugf("ssid = %s\n", (settings.ssid()) ? settings.ssid() : "null" );
    ESPMan_Debugf("pass = %s\n", (settings.pass()) ? settings.pass() : "null" );
    ESPMan_Debugf("dhcp = %s\n", (settings.dhcp) ? "true" : "false" );
    ESPMan_Debugf("hasConfig = %s\n", (settings.hasConfig) ? "true" : "false" );
    ESPMan_Debugf("IP = %s\n", settings.IP.toString().c_str() );
    ESPMan_Debugf("GW = %s\n", settings.GW.toString().c_str() );
    ESPMan_Debugf("SN = %s\n", settings.SN.toString().c_str() );
    ESPMan_Debugf("DNS1 = %s\n", settings.DNS1.toString().c_str() );
    ESPMan_Debugf("DNS2 = %s\n", settings.DNS2.toString().c_str() );
    ESPMan_Debugf("autoConnect = %s\n", (settings.autoConnect) ? "true" : "false" );
    ESPMan_Debugf("autoReconnect = %s\n", (settings.autoReconnect) ? "true" : "false" );

}


void ESPmanager::_dumpSettings()
{
    _getAllSettings();

    if (_settings) {
        ESPMan_Debugf(" IP Addr %u.%u.%u.%u\n", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
        ESPMan_Debugf("---- Settings ----\n");
        ESPMan_Debugf("configured = %s\n", (_settings->configured) ? "true" : "false" );
        ESPMan_Debugf("changed = %s\n", (_settings->changed) ? "true" : "false" );
        //ESPMan_Debugf("usePerminantSettings = %s\n", (settings->GEN.usePerminantSettings) ? "true" : "false" );

        _dumpGEN(_settings->GEN);
        _dumpSTA(_settings->STA);
        _dumpAP(_settings->AP);

    }
}

#endif

void ESPmanager::factoryReset()
{
    ESPMan_Debugf("FACTORY RESET");
    WiFi.disconnect();
    ESP.eraseConfig();
    _fs.remove(SETTINGS_FILE);
    _fs.remove("/.wizard");
}

void ESPmanager::_sendTextResponse(AsyncWebServerRequest * request, uint16_t code, const char * text)
{
    AsyncWebServerResponse *response = request->beginResponse(code, "text/plain", text);
    response->addHeader(ESPMAN::string_CORS, "*");
    response->addHeader(ESPMAN::string_CACHE_CONTROL, "no-store");
    request->send(response);
}


// String ESPmanager::_hash(const char * pass){
//
//         char salt[SALT_LENGTH + 1];
//         for (uint8_t i = 0; i < SALT_LENGTH; i++) {
//                 salt[i] = random(33,123);
//         }
//         salt[SALT_LENGTH ] = '\0';
//         //ESPMan_Debugf("[ESPmanager::_hash] salt = %s\n", salt );
//         char * input = new char[ strlen(salt) + strlen(pass) + 1 ];
//
//         if (input) {
//                 strcpy(input, pass);
//                 strcat(input, salt);
//                 String hash = salt;
//                 //ESPMan_Debugf("[ESPmanager::_hashCheck] input = %s\n", input );
//                 hash += sha1( input, strlen(input));
//                 //ESPMan_Debugf("[ESPmanager::_hashCheck] output = %s\n", sha1( input, strlen(input)).c_str() );
//                 //ESPMan_Debugf("[ESPmanager::_hash] salt + hash = %s\n", hash.c_str() );
//                 delete input;
//                 return hash;
//         }
//
//         return String();
//
// }
//
//
//
// bool ESPmanager::_hashCheck(const char * password, const char * hash)
// {
//         // ESPMan_Debugf("[ESPmanager::_hashCheck] pasword = %s\n" , password);
//         // ESPMan_Debugf("[ESPmanager::_hashCheck] hash = %s\n" , hash);
//
//         bool result = false;
//         char salt[SALT_LENGTH + 1];
//
//         for (uint8_t i = 0; i < SALT_LENGTH; i++) {
//                 salt[i] = hash[i];
//         }
//
//         salt[SALT_LENGTH] = '\0';
//         //ESPMan_Debugf("[ESPmanager::_hashCheck] salt = %s\n" , salt);
//
//         char * truehash = new char[ strlen(hash) - SALT_LENGTH + 1 ];
//         char * input = new char [  strlen(salt) + strlen(password) + 1 ];
//         if (input && truehash) {
//                 strcpy(input, password);
//                 strcat(input, salt);
//                 memcpy( truehash, &hash[SALT_LENGTH], strlen(hash) - SALT_LENGTH );
//                 truehash[  strlen(hash) - SALT_LENGTH ] = '\0';
//                 //ESPMan_Debugf("[ESPmanager::_hashCheck] truehash = %s\n" , truehash);
//                 //ESPMan_Debugf("[ESPmanager::_hashCheck] input = %s\n" , input);
//                 String check = sha1(input, strlen(input));
//
//                 //ESPMan_Debugf("[ESPmanager::_hashCheck] checkhash = %s\n" , check.c_str() );
//
//                 if (  strcmp( check.c_str(), truehash) == 0) {
//                         //ESPMan_Debugf("[ESPmanager::_hashCheck] MATCH");
//                         result = true;
//
//                 }
//
//                 delete truehash;
//                 delete input;
//         }
//
//         return result;
//
// }

void ESPmanager::_removePreGzFiles() {

        Dir dir = _fs.openDir("/");
        while (dir.next()) {
            String fileName = dir.fileName();

            if (fileName.endsWith(".gz")) {

                String withOutgz = fileName.substring(0, fileName.length() - 3 );

                if (_fs.exists(withOutgz)) {
                    ESPMan_Debugf("_removePreGzFiles() : Removing unzipped file %s\n", withOutgz.c_str());
                    _fs.remove(withOutgz);
                }

            }

        }

}
