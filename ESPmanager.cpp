
#include "ESPmanager.h"

#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <MD5Builder.h>
#include <AsyncJson.h>
#include "FileFallbackHandler.h"


extern "C" {
#include "user_interface.h"
}

// Stringifying the BUILD_TAG parameter
#define TEXTIFY(A) #A
#define ESCAPEQUOTE(A) TEXTIFY(A)

// //String buildTag = ESCAPEQUOTE(BUILD_TAG);
// String commitTag = ESCAPEQUOTE(TRAVIS_COMMIT);

#ifndef BUILD_TAG
#define BUILD_TAG "Not Set"
#endif
#ifndef COMMIT_TAG
#define COMMIT_TAG "Not Set"
#endif
#ifndef BRANCH_TAG
#define BRANCH_TAG "Not Set"
#endif
#ifndef SLUG_TAG
#define SLUG_TAG "Not Set"
#endif

const char * buildTag = ESCAPEQUOTE(BUILD_TAG);
const char * commitTag = ESCAPEQUOTE(COMMIT_TAG);
const char * branchTag = ESCAPEQUOTE(BRANCH_TAG);
const char * slugTag = ESCAPEQUOTE(SLUG_TAG);

ESPmanager::ESPmanager(
    AsyncWebServer & HTTP, FS & fs, const char* host, const char* ssid, const char* pass)
    : _HTTP(HTTP)
    , _fs(fs)
{
    //httpUpdater.setup(&_HTTP);


    // This sets the default fallback options...
    if (host && (strlen(host) < 32)) {
        _host = strdup(host);
    }
    if (ssid && (strlen(ssid) < 32)) {
        _ssid_hardcoded = ssid;
    }
    if (pass && (strlen(pass) < 63)) {
        _pass_hardcoded = pass;
    }

    _manageWiFi = true;
}

ESPmanager::~ESPmanager()
{

    // if (ota_server)
    // {
    //     delete ota_server;
    //     ota_server = NULL;
    // };

    if (_host) {
        free((void*)_host);
        _host = nullptr;
    };
    if (_pass) {
        free((void*)_pass);
        _pass = nullptr;
    };
    if (_ssid) {
        free((void*)_ssid);
        _ssid = nullptr;
    };
    if (_APpass) {
        free((void*)_APpass);
        _APpass = nullptr;
    };
    if (_APssid) {
        free((void*)_APssid);
        _APssid = nullptr;
    };
    if (_IPs) {
        delete _IPs;
        _IPs = nullptr;
    };
    if (_APmac) {
        delete _APmac;
        _APmac = nullptr;
    }
    if (_STAmac) {
        delete _STAmac;
        _STAmac = nullptr;
    }

    if (_OTApassword) {
        delete _OTApassword;
        _OTApassword = nullptr;
    }
}

// void ESPmanager::_WiFiEventCallback(WiFiEvent_t event)
// {

//     ESPMan_Debugf("[ESPmanager::WiFi Event] : %u\n", (uint8_t)event);


// }

using namespace std::placeholders;

void  ESPmanager::begin()
{

    ESPMan_Debugln("Settings Manager V" ESPMANVERSION);
    ESPMan_Debugf("REPO: %s\n",  slugTag );
    ESPMan_Debugf("BRANCH: %s\n",  branchTag );
    ESPMan_Debugf("BuildTag: %s\n",  buildTag );
    ESPMan_Debugf("commitTag: %s\n",  commitTag ) ;



    wifi_set_sleep_type(NONE_SLEEP_T); // workaround no modem sleep.

    //uint32_t value = WiFi.onEvent( std::bind( &ESPmanager::_WiFiEventCallback, this, _1 ) );


    //WiFi.removeEvent( value );


    if (_fs.begin()) {
        ESPMan_Debugln(F("File System mounted sucessfully"));

#ifdef DEBUG_ESP_PORT
#ifdef ESPMan_Debug
        DEBUG_ESP_PORT.println("SPIFFS FILES:");
        {
            Dir dir = SPIFFS.openDir("/");
            while (dir.next()) {
                String fileName = dir.fileName();
                size_t fileSize = dir.fileSize();
                DEBUG_ESP_PORT.printf("     FS File: %s\n", fileName.c_str());
            }
            DEBUG_ESP_PORT.printf("\n");
        }
#endif
#endif

//       _NewFilesCheck();

        if (!_FilesCheck(true)) {
            ESPMan_Debugln(F("Major FAIL, required files are NOT in SPIFFS, please upload required files"));
        } else {
//           _NewFilesCheck();
        }


        if ( LoadSettings() ) {
            ESPMan_Debugln("Load settings returned true");
        } else {
            ESPMan_Debugln("Load Settings returned false");
        }

    } else {
        ESPMan_Debugln(F("File System mount failed"));
    }

    // needs to be set before WiFi.Begin() and DHCP request
    if (_host) {
        if (WiFi.hostname(_host)) {
            ESPMan_Debug(F("Host Name Set: "));
            ESPMan_Debugln(_host);
        }
    } else {
        char tmp[15];
        sprintf(tmp, "esp8266-%06x", ESP.getChipId());
        _host = strdup(tmp);
        ESPMan_Debug(F("Default Host Name: "));
        ESPMan_Debugln(_host);
    }

    if (_manageWiFi) {


        if (!_APssid) {
            _APssid = strdup(_host);
        }

        if (_APenabled) {
#ifdef DEBUG_ESP_PORT
            DEBUG_ESP_PORT.printf("Creating AP (%s)\n", _APssid);
#endif
            InitialiseSoftAP();
        }

        if (_STAenabled) {

            WiFi.mode(WIFI_STA);

#ifdef DEBUG_ESP_PORT
            DEBUG_ESP_PORT.print("Connecting to WiFi...");
#endif
            if (!Wifistart()) {
                ESPMan_Debug(F("WiFi Failed: "));
                if (_APrestartmode > 1) { // 1 = none, 2 = 5min, 3 = 10min, 4 = whenever : 0 is reserved for unset...

                    _APtimer = millis();
                    ESPMan_Debugf("Starting AP: ssid (%s)", _APssid);

                    // if (!_APenabled) {
                    InitialiseSoftAP();
                    // }

                    ESPMan_Debugln();
                } else {
                    ESPMan_Debugln(F("Soft AP disbaled by config"));
                }
            } else {
#ifdef DEBUG_ESP_PORT
                DEBUG_ESP_PORT.print(F("Success\nConnected to "));
                DEBUG_ESP_PORT.print(WiFi.SSID());
                DEBUG_ESP_PORT.print(" (");
                DEBUG_ESP_PORT.print(WiFi.localIP());
                DEBUG_ESP_PORT.println(")");
#endif
            }

        }





    }


    // if (_APrestartmode) {
    //     ESPMan_Debugln(F("Soft AP enabled by config"));
    //     InitialiseSoftAP();
    // } else { ESPMan_Debugln(F("Soft AP disbaled by config")); }


    InitialiseFeatures();

    _HTTP.on("/espman/data.esp", std::bind(&ESPmanager::_HandleDataRequest, this, _1 ));
    //_HTTP.on("/espman/upload", HTTP_POST , [this]() { _HTTP.send(200, "text/plain", ""); }, std::bind(&ESPmanager::handleFileUpload, this)  );
    _HTTP.serveStatic("/espman", _fs, "/espman", "max-age=86400");

//   _HTTP.serveStatic("/jquery", _fs, "/jquery", "max-age=86400");

    _HTTP.addHandler( new FileFallbackHandler(_fs, "/jquery/jqm1.4.5.css", "/jquery/jqm1.4.5.css", "http://code.jquery.com/mobile/1.4.5/jquery.mobile-1.4.5.min.css",  "max-age=86400"));
    _HTTP.addHandler( new FileFallbackHandler(_fs, "/jquery/jq1.11.1.js" , "/jquery/jq1.11.1.js" ,   "http://code.jquery.com/jquery-1.11.1.min.js",  "max-age=86400"));
    _HTTP.addHandler( new FileFallbackHandler(_fs, "/jquery/jqm1.4.5.js" , "/jquery/jqm1.4.5.js" ,   "http://code.jquery.com/mobile/1.4.5/jquery.mobile-1.4.5.min.js",  "max-age=86400"));


    _HTTP.on("/espman/update", std::bind(&ESPmanager::_HandleSketchUpdate, this, _1 ));
}




void ESPmanager::_extractkey(JsonObject& root, const char * name, char *& ptr )
{

    if (name && root.containsKey(name)) {

        const char* temp = root[name];
        if (ptr) {
            free(ptr);
            ptr = nullptr;
        };

        if (!ptr && temp) {
            ptr = strdup(temp);
        }
    }
}


bool  ESPmanager::LoadSettings()
{

    DynamicJsonBuffer jsonBuffer(1000);
    File f = _fs.open(SETTINGS_FILE, "r");
    if (!f) {
        ESPMan_Debugln(F("Settings file open failed!"));
        return false;
    }


    char * data = new char[f.size()];
    // prevent nullptr exception if can't allocate
    if (data) {

        //  This method give a massive improvement in file reading speed for SPIFFS files..
        // 2K file down to 1-2ms from 60ms

        int bytesleft = f.size();
        int position = 0;
        while ((f.available() > -1) && (bytesleft > 0)) {

            // get available data size
            int sizeAvailable = f.available();
            if (sizeAvailable) {
                int readBytes = sizeAvailable;

                // read only the asked bytes
                if (readBytes > bytesleft) {
                    readBytes = bytesleft ;
                }

                // get new position in buffer
                char * buf = &data[position];
                // read data
                int bytesread = f.readBytes(buf, readBytes);
                bytesleft -= bytesread;
                position += bytesread;

            }
            // time for network streams
            delay(0);
        }
        //////

        f.close();

        JsonObject& root = jsonBuffer.parseObject(data);

        if (!root.success()) {
            ESPMan_Debugln(F("Parsing settings file Failed!"));
            return false;
        }

        _extractkey(root, "host", _host);
        _extractkey(root, "ssid", _ssid);
        _extractkey(root, "pass", _pass);
        _extractkey(root, "APpass", _APpass);
        _extractkey(root, "APssid", _APssid);

        // if (root.containsKey("host")) {
        //     const char* host = root[F("host")];
        //     if (_host) {
        //         free((void*)_host);
        //         _host = nullptr;
        //     };
        //     if (host) { _host = strdup(host); } else { _host = nullptr ; } ;
        // }

        // if (root.containsKey("ssid")) {
        //     const char* ssid = root["ssid"];
        //     if (_ssid) {
        //         free((void*)_ssid);
        //         _ssid = nullptr;
        //     };
        //     if (ssid) { _ssid = strdup(ssid); } else { _ssid = nullptr ; } ;
        // }

        // if (root.containsKey("pass")) {
        //     const char* pass = root["pass"];
        //     if (_pass) {
        //         free((void*)_pass);
        //         _pass = nullptr;
        //     };
        //     if (pass) { _pass = strdup(pass); } else { _pass = nullptr; } ;
        // }

        // if (root.containsKey("APpass")) {
        //     const char* APpass = root["APpass"];
        //     if (_APpass) {
        //         free((void*)_APpass);
        //         _APpass = nullptr;
        //     };
        //     if (APpass) { _APpass = strdup(APpass); } else { _APpass = nullptr; } ;
        // }

        // if (root.containsKey("APssid")) {
        //     const char* APssid = root["APssid"];
        //     if (_APssid) {
        //         free((void*)_APssid);
        //         _APssid = nullptr;
        //     };
        //     if (APssid) { _APssid = strdup(APssid); } else {_APssid = nullptr; } ;
        // }

        if (root.containsKey("APchannel")) {
            long APchannel = root["APchannel"];
            if (APchannel < 13 && APchannel > 0) {
                _APchannel = (uint8_t)APchannel;
            }
        }

        if (root.containsKey("DHCP")) {
            _DHCP = root["DHCP"];
        }

        if (root.containsKey("APenabled")) {
            _APenabled = root["APenabled"];
        }

        if (root.containsKey("APrestartmode")) {
            _APrestartmode = root["APrestartmode"];
        }

        if (root.containsKey("APhidden")) {
            _APhidden = root["APhidden"];
        }

        if (root.containsKey("OTAenabled")) {
            _OTAenabled = root["OTAenabled"];
        }

        _extractkey(root, "OTApassword", _OTApassword);

        // if (root.containsKey("OTApassword")) {
        //     const char* OTApassword = root["OTApassword"];
        //     if (_OTApassword) {
        //         free((void*)_OTApassword);
        //         _OTApassword = nullptr;
        //     };
        //     if (OTApassword) { _OTApassword = strdup(OTApassword); } else {_OTApassword = nullptr; } ;
        // }

        if (root.containsKey("STAenabled")) {
            _STAenabled = root["STAenabled"];
        }

        if (root.containsKey("mDNSenable")) {
            _mDNSenabled = root["mDNSenable"];
        }

        if (root.containsKey("WiFimanage")) {
            _manageWiFi = root["WiFimanage"];
        }

        // if (root.containsKey("OTAusechipID"))
        // {
        //      _OTAusechipID = root["OTAusechipID"];
        //     //_manageWiFi = (strcmp(manageWiFi, "true") == 0) ? true : false;
        // }


        if (root.containsKey("IPaddress") && root.containsKey("Gateway") && root.containsKey("Subnet")) {
            const char* ip = root[F("IPaddress")];
            const char* gw = root[F("Gateway")];
            const char* sn = root[F("Subnet")];

            if (!_DHCP) {
                // only bother to allocate memory if dhcp is NOT being used.
                if (_IPs) {
                    delete _IPs;
                    _IPs = NULL;
                };
                _IPs = new IPconfigs_t;
                _IPs->IP.fromString(String(ip));
                _IPs->GW.fromString(String(gw));
                _IPs->SN.fromString(String(sn));
            }
        }

        if (root.containsKey("STAmac")) {
            uint8_t savedmac[6] = {0};
            uint8_t currentmac[6] = {0};
            WiFi.macAddress(currentmac);

            for (uint8_t i = 0; i < 6; i++) {
                savedmac[i] = root["STAmac"][i];
            }
            if (memcmp( (const void *)savedmac, (const void *) currentmac, 6 ) != 0 ) {
                ESPMan_Debugln("Saved STA MAC does not equal native mac");
                if (_STAmac) {
                    delete _STAmac;
                    _STAmac = nullptr;
                }
                _STAmac = new uint8_t[6];
                memcpy(_STAmac, savedmac, 6);
            }
        }

        if (root.containsKey("APmac")) {
            uint8_t savedmac[6] = {0};
            uint8_t currentmac[6] = {0};
            WiFi.softAPmacAddress(currentmac);

            for (uint8_t i = 0; i < 6; i++) {
                savedmac[i] = root["APmac"][i];
            }
            if (memcmp( (const void *)savedmac, (const void *) currentmac, 6 ) != 0 ) {
                ESPMan_Debugln("Saved AP MAC does not equal native mac");
                if (_APmac) {
                    delete _APmac;
                    _APmac = nullptr;
                }
                _APmac = new uint8_t[6];
                memcpy(_APmac, savedmac, 6);
            }
        }

#ifdef Debug_ESPManager
#ifdef DEBUG_ESP_PORT

        DEBUG_ESP_PORT.println("JSON Settings file: ");
        root.prettyPrintTo(DEBUG_ESP_PORT);
        DEBUG_ESP_PORT.println();
#endif
#endif

        ESPMan_Debugln(F("----- Saved Variables -----"));
        PrintVariables();
        ESPMan_Debugln(F("---------------------------"));

        delete[] data; //  OK to delete as it is wrapped in if (data)
    } //  end of if data...
    return true;
}


void  ESPmanager::PrintVariables()
{

#ifdef Debug_ESPManager
#ifdef DEBUG_ESP_PORT

    ESPMan_Debugln(F("VARIABLE STATES: "));
    ESPMan_Debugf("_host = %s\n", _host);
    ESPMan_Debugf("_ssid = %s\n", _ssid);
    ESPMan_Debugf("_pass = %s\n", _pass);
    ESPMan_Debugf("_APpass = %s\n", _APpass);
    ESPMan_Debugf("_APssid = %s\n", _APssid);
    ESPMan_Debugf("_APchannel = %u\n", _APchannel);
    (_DHCP) ? ESPMan_Debugln(F("_DHCP = true")) : ESPMan_Debugln(F("_DHCP = false"));
    (_APenabled) ? ESPMan_Debugln(F("_APenabled = true")) : ESPMan_Debugln(F("_APenabled = false"));
    (_APhidden) ? ESPMan_Debugln(F("_APhidden = true")) : ESPMan_Debugln(F("_APhidden = false"));
    (_OTAenabled) ? ESPMan_Debugln(F("_OTAenabled = true")) : ESPMan_Debugln(F("_OTAenabled = false"));
    (_STAenabled) ? ESPMan_Debugln(F("_STAenabled = true")) : ESPMan_Debugln(F("_STAenabled = false"));


    if (_IPs) {
        ESPMan_Debug(F("IPs->IP = "));
        ESPMan_Debugln(   (_IPs->IP).toString() );
        ESPMan_Debug(F("IPs->GW = "));
        ESPMan_Debugln( (_IPs->GW).toString() );
        ESPMan_Debug(F("IPs->SN = "));
        ESPMan_Debugln( (_IPs->SN).toString() );
    } else {
        ESPMan_Debugln(F("NO IPs held in memory"));
    }
    if (_STAmac) {
        ESPMan_Debug(F("STA MAC = "));
        ESPMan_Debugf("%02X:%02X:%02X:%02X:%02X:%02X\n", _STAmac[0],  _STAmac[1], _STAmac[2], _STAmac[3], _STAmac[4], _STAmac[5]);

    } else { ESPMan_Debugln("STA MAC not held in memory"); }
    if (_APmac) {
        ESPMan_Debug(F("AP MAC = "));
        ESPMan_Debugf("%02X:%02X:%02X:%02X:%02X:%02X\n", _APmac[0],  _APmac[1], _APmac[2], _APmac[3], _APmac[4], _APmac[5]);
    } else { ESPMan_Debugln("AP MAC not held in memory"); }
#endif
#endif
}

void  ESPmanager::SaveSettings()
{
    /*
        Settings to save

        bool _APhidden = false;
        bool _APenabled = false;
        bool _OTAenabled = true;
        bool _DHCP = true;
        uint8_t _APchannel = 1;

      const char * _host = NULL;
      const char * _ssid = NULL;
      const char * _pass = NULL;
      const char * _APpass = NULL;

        WiFi.localIP()
        WiFi.gatewayIP()
        WiFi.subnetMask()) + "\",";

    */

    ESPMan_Debugf("[ESPmanager::SaveSettings] CALLED\n");
    long starttime = millis();

    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();

    root[F("host")] = (_host) ? _host : C_null;
    root[F("ssid")] = (_ssid) ? _ssid : C_null;
    root[F("pass")] = (_pass) ? _pass : C_null;
    root[F("APrestartmode")] = _APrestartmode;
    root[F("APpass")] = (_APpass) ? _APpass : C_null;
    root[F("APssid")] = (_APssid) ? _APssid : C_null;
    root[F("DHCP")] = (_DHCP) ? true : false;
    root[F("APchannel")] = (_APchannel) ? true : false;
    root[F("APenabled")] = (_APenabled) ? true : false;
    root[F("APhidden")] = (_APhidden) ? true : false;
    root[F("OTAenabled")] = (_OTAenabled) ? true : false;
    root[F("OTApassword")] = (_OTApassword) ? _OTApassword : C_null;
    root[F("STAenabled")] = (_STAenabled) ? true : false;

    //root[F("OTAusechipID")] = (_OTAusechipID) ? true : false;
    root[F("mDNSenable")] = (_mDNSenabled) ? true : false;
    root[F("WiFimanage")] = (_manageWiFi) ? true : false;

    char IP[30];
    String ip = WiFi.localIP().toString() ;
    ip.toCharArray(IP, ip.length() + 3);
    char GW[30];
    String gw = WiFi.gatewayIP().toString() ;
    gw.toCharArray(GW, gw.length() + 3);
    char SN[30];
    String sn = WiFi.subnetMask().toString() ;
    sn.toCharArray(SN, sn.length() + 3);

    root[F("IPaddress")] = IP;
    root[F("Gateway")] = GW;
    root[F("Subnet")] = SN;

    JsonArray& macarray = root.createNestedArray("STAmac");
    uint8_t mac[6];
    WiFi.macAddress(mac);
    for (uint8_t i = 0; i < 6; i ++) {
        macarray.add(mac[i]);
    }

    JsonArray& macAParray = root.createNestedArray("APmac");
    uint8_t apmac[6];
    WiFi.softAPmacAddress(apmac);
    for (uint8_t i = 0; i < 6; i ++) {
        macAParray.add(apmac[i]);
    }

    // ESPMan_Debugf("IP = %s, GW = %s, SN = %s\n", IP, GW, SN);
    File f = _fs.open(SETTINGS_FILE, "w");
    if (!f) {
        ESPMan_Debugln(F("Settings file save failed!"));
        return;
    }

    root.prettyPrintTo(f);
    f.close();
}

void  ESPmanager::handle()
{
    static bool triggered = false;

    // if (ota_server)
    //     ota_server->handle();

    ArduinoOTA.handle();

    if (save_flag) {
        SaveSettings();
        save_flag = false;
    }


    if (_APtimer > 0) {
        uint32_t timer = 0;
        _APtimer = 0;
        if (_APrestartmode == 2) { timer = 5 * 60 * 1000; }
        if (_APrestartmode == 3) { timer = 10 * 60 * 1000; }
        if (millis() - _APtimer > timer) {
            WiFi.mode(WIFI_STA); //  == WIFI_AP
            ESPMan_Debugln("AP Stopped");
        }
    }


//  need to work on this...
    // reset trigger if wifi is reconnected...
    if (WiFi.status() == WL_CONNECTED && _APrestartmode == 4 && triggered) {
        triggered = false;
    }

    // AP should only be activated for option 4
    if (WiFi.status() != WL_CONNECTED && _APrestartmode == 4 && !triggered) {
        triggered = true;
        static uint32_t _wait = 0;
        ESPMan_Debugln(F("WiFi Disconnected:  Starting AP"));
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(_APssid, _APpass, (int)_APchannel, (int)_APhidden);
        ESPMan_Debugln(F("Done"));
        _APtimer = millis();
    }


    if (_syncCallback) {
        if (_syncCallback()) {
            _syncCallback = nullptr;
        };

    }


}

void  ESPmanager::InitialiseFeatures()
{

    // if (_OTAenabled)
    // {
    //     char OTAhost[strlen(_host) + 2];
    //     strcpy(OTAhost, _host);
    //     OTAhost[strlen(_host)] = '-';
    //     OTAhost[strlen(_host) + 1] = 0;
    //     // if (ota_server)
    //     // {
    //     //     delete ota_server;
    //     //     ota_server = NULL;
    //     // };
    //     ArduinoOTA.begin();

    //     ota_server = new ArduinoOTA(OTAhost, 8266, true);
    //     ota_server->setup();
    // }
    // else
    // {
    //     if (ota_server)
    //     {
    //         delete ota_server;
    //         ota_server = NULL;
    //     };
    // }



    // if (_OTAusechipID) {
    //     char OTAhost[33];
    //     strcpy(OTAhost, _host);
    //     OTAhost[strlen(_host)] = '-';
    //     OTAhost[strlen(_host) + 1] = 0;
    //     char tmp[15];
    //     sprintf(tmp, "%02x", ESP.getChipId());
    //     strcat(OTAhost, tmp);

    //     ArduinoOTA.setHostname(OTAhost);

    //     ESPMan_Debugf("OTA host = %s\n", OTAhost);

    // } else {
    if (_host) {
        ArduinoOTA.setHostname(_host);
        ESPMan_Debugf("OTA host = %s\n", _host);
    };
    //}
    // Port defaults to 8266
    // ArduinoOTA.setPort(8266);

    // Hostname defaults to esp8266-[ChipID]
    // ArduinoOTA.setHostname(OTAhost);

    // No authentication by default
    // ArduinoOTA.setPassword((const char *)"123");
#ifdef DEBUG_ESP_PORT
    ArduinoOTA.onStart([]() {
        DEBUG_ESP_PORT.print(F(   "[              Performing OTA Upgrade              ]\n["));
//                       ("[--------------------------------------------------]\n ");
    });
    ArduinoOTA.onEnd([]() {
        DEBUG_ESP_PORT.println(F("]\nOTA End"));
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static uint8_t done = 0;
        uint8_t percent = (progress / (total / 100) );
        if ( percent % 2 == 0  && percent != done ) {
            DEBUG_ESP_PORT.print("-");
            done = percent;
        }
    });
    ArduinoOTA.onError([](ota_error_t error) {
        DEBUG_ESP_PORT.printf("OTA Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) { DEBUG_ESP_PORT.println(F("Auth Failed")); }
        else if (error == OTA_BEGIN_ERROR) { DEBUG_ESP_PORT.println(F("Begin Failed")); }
        else if (error == OTA_CONNECT_ERROR) { DEBUG_ESP_PORT.println(F("Connect Failed")); }
        else if (error == OTA_RECEIVE_ERROR) { DEBUG_ESP_PORT.println(F("Receive Failed")); }
        else if (error == OTA_END_ERROR) { DEBUG_ESP_PORT.println(F("End Failed")); }
    });
#endif
    ArduinoOTA.begin();


// not sure this is needed now.

    if (_mDNSenabled) {
        MDNS.addService("http", "tcp", 80);
    }
}

#ifdef USE_WEB_UPDATER


bool  ESPmanager::_upgrade()
{
    return false; // disable for now....

    static const uint16_t httpPort = 80;
    static const size_t bufsize = 1024;
//  get files list into json
    uint8_t files_recieved = 0;
    uint8_t files_expected = 0;

    HTTPClient http;
    String path = String(__updateserver) + String(__updatepath);
    http.begin(path); //HTTP

    int httpCode = http.GET();

    if (httpCode) {
        if (httpCode == 200) {

            size_t len = http.getSize();
            if (len > bufsize) {
                ESPMan_Debugln("Receive update length too big.  Increase buffer");
                return false;
            }
            uint8_t buff[bufsize] = { 0 }; // max size of input buffer. Don't use String, as arduinoJSON doesn't like it!

            // get tcp stream
            WiFiClient * stream = http.getStreamPtr();

            // read all data from server
            while (http.connected() && (len > 0 || len == -1)) {
                // get available data size
                size_t size = stream->available();

                if (size) {
                    int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
                    if (len > 0) {
                        len -= c;
                    }
                }
                delay(1);
            }
            http.end();
            yield();

            DynamicJsonBuffer jsonBuffer;
            JsonArray& root = jsonBuffer.parseArray( (char*)buff );

            if (!root.success()) {
                ESPMan_Debugln("Parse JSON failed");
                return false;
            } else {
                uint8_t count = 0;
                for (JsonArray::iterator it = root.begin(); it != root.end(); ++it) {
                    files_expected++;
                    JsonObject& item = *it;
                    const char* value = item["file"];
                    const char* md5 = item["md5"];
                    ESPMan_Debugf("[%u] ", files_expected);
                    String fullpathtodownload = String(__updateserver) + String(value);
                    String filename = fullpathtodownload.substring( fullpathtodownload.lastIndexOf("/"), fullpathtodownload.length() );
                    bool downloaded = _DownloadToSPIFFS(fullpathtodownload.c_str() , filename.c_str(), md5 );
                    if (downloaded) {
                        ESPMan_Debugf("Download SUCCESS (%s)\n", fullpathtodownload.c_str()  );
                        files_recieved++;
                    } else {
                        ESPMan_Debugf("Download FAILED (%s)\n", fullpathtodownload.c_str() );
                    }
                    delay(1);
                }
            }
        } else {
            ESPMan_Debugf("HTTP CODE [%d]", httpCode);
            http.end();
            return false;
        }
    } else {
        ESPMan_Debugln("GET request Failed");
        http.end();
        return false;
    }

    if (files_recieved == files_expected) {
        ESPMan_Debugf("Update Successful [%u/%u] downloaded\n", files_recieved, files_expected);
        return true;
    } else {
        ESPMan_Debugf("Update Error [%u/%u] downloaded succesfully\n", files_recieved, files_expected);
        return false;
    }
}

bool  ESPmanager::_DownloadToSPIFFS(const char * url , const char * filename, const char * md5_true )
{
    HTTPClient http;
    FSInfo _FSinfo;
    int freeBytes = 0;


    if ( _fs.exists(filename) ) {

        File Fcheck = _fs.open(filename, "r");

        String crc = _file_md5(Fcheck);

        if (crc == String(md5_true)) {
            ESPMan_Debugf("\n  [ERROR] File exists with same CRC \n");
            Fcheck.close();
            return false;
        }

        Fcheck.close();

    }


    if (_fs.info(_FSinfo)) {
        freeBytes = _FSinfo.totalBytes - _FSinfo.usedBytes;
    }

    if (strlen(filename) > _FSinfo.maxPathLength) {
        ESPMan_Debugf("\n  [ERROR] file name too long \n");
        return false;
    }

    File f = _fs.open("/tempfile", "w+"); //  w+ is to allow read operations on file.... otherwise crc gets 255!!!!!

    if (!f) {
        ESPMan_Debugf("\n  [ERROR] tempfile open failed\n");
        return false;
    } else {


        http.begin(url);

        int httpCode = http.GET();

        if (http.hasHeader("x-MD5")) {
            ESPMan_Debugf(" \n[httpUpdate]  - MD5: %s\n", http.header("x-MD5").c_str());
        }

        if (httpCode > 0) {
            if (httpCode == 200) {
                int len = http.getSize();

                if (len < freeBytes) {

                    size_t byteswritten = http.writeToStream(&f);
//                ESPMan_Debugf("%s downloaded, expected (%s) \n", formatBytes(byteswritten).c_str(), formatBytes(len).c_str() ) ;
                    bool success = false;

                    if (f.size() == len || len == -1 ) {

                        if (md5_true) {
                            String crc = _file_md5(f);

                            if (crc == String(md5_true)) {
                                success = true;
                            }

                        } else {
                            ESPMan_Debugf("\n  [ERROR] CRC not provided \n");
                            success = true; // set to true if no CRC provided...
                        }

                        f.close();
                        if (success) {
                            _fs.rename("/tempfile", filename);
                            return true;
                        } else {
                            ESPMan_Debugf("\n  [ERROR] CRC mismatch\n");
                            //_fs.remove("/tempfile");
                        }

                    } else {
                        ESPMan_Debugf("\n  [ERROR] Download FAILED %s downloaded (%s required) \n", formatBytes(byteswritten).c_str(), formatBytes(http.getSize()).c_str() );
                        //_fs.remove("/tempfile");
                    }
                } else {
                    ESPMan_Debugf("\n  [ERROR] Download FAILED File too big \n");
                    //_fs.remove("/tempfile");
                }

            } else {
                ESPMan_Debugf("\n  [ERROR] HTTP code not correct [%d] \n", httpCode);
                //_fs.remove("/tempfile");
            }
        } else {
            ESPMan_Debugf("\n  [ERROR] HTTP code ERROR [%d] \n", httpCode);
            //_fs.remove("/tempfile");
        }
        yield();
    }

    _fs.remove("/tempfile");
    http.end();
    f.close();
    return false;
}

/*
 *      Takes POST request with url parameter for the json
 *
 *
 */


void ESPmanager::_HandleSketchUpdate(AsyncWebServerRequest *request)
{


    ESPMan_Debugf("[ESPmanager::_HandleSketchUpdate] HIT\n" );


    if ( request->hasParam("url", true)) {

        String path = request->getParam("url", true)->value();

        ESPMan_Debugf("[ESPmanager::_HandleSketchUpdate] path = %s\n", path.c_str());

        _syncCallback = [ = ]() {

            upgrade(path);
            return true;

        };

    }

    request->send(200, "Started");

}

void ESPmanager::upgrade(String path)
{

    int files_expected = 0;
    int files_recieved = 0;
    int file_count = 0;
    DynamicJsonBuffer jsonBuffer;

    String rooturi = path.substring(0, path.lastIndexOf('/') );

    ESPMan_Debugf("[ESPmanager::upgrade] rooturi=%s\n", rooturi.c_str());


    JsonObject * p_root = nullptr;
    uint8_t * buff = nullptr;
    bool updatesketch = false;

    if (_parseUpdateJson(buff, jsonBuffer, p_root, path)) {

        ESPMan_Debugf("[ESPmanager::_HandleSketchUpdate] _parseUpdateJson success\n");
        JsonObject & root = *p_root;
        files_expected = root["filecount"];
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
                ESP.eraseConfig();
                ESPMan_Debugf("done\n");
            }
        }


        // if (root.containsKey("rooturi")) {
        //     ESPMan_Debugf("[ESPmanager::_HandleSketchUpdate] Using root uri : %s\n" , rooturi.c_str());
        //     rooturi = String(root["rooturi"].asString());
        // }


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
                files_recieved++; //  add one to keep count in order...
#if defined(DEBUG_ESP_PORT)
                DEBUG_ESP_PORT.printf("[%u/%u] BIN Updated pending (%s)\n", file_count, files_expected , remote_path.c_str()  );
#endif
                continue;
            }
#if defined(DEBUG_ESP_PORT)
            DEBUG_ESP_PORT.printf("[%u/%u] Downloading (%s)..", file_count, files_expected , remote_path.c_str()  );
#endif

            bool downloaded = _DownloadToSPIFFS(remote_path.c_str(), filename.c_str(), md5 );

#if defined(DEBUG_ESP_PORT)

            if (downloaded) {
                DEBUG_ESP_PORT.printf("SUCCESS \n", remote_path.c_str()  );
                //files_recieved++;
            } else {
#if !defined(ESPMan_Debug)
                DEBUG_ESP_PORT.printf("FAILED \n", remote_path.c_str()  );
#endif
            }
#endif

            delay(1);
        }

        if (updatesketch) {

            for (JsonArray::iterator it = array.begin(); it != array.end(); ++it) {
                JsonObject& item = *it;
                String remote_path = rooturi + String(item["location"].asString());
                String filename = item["saveto"];
                String commit = root["commit"];



                if (remote_path.endsWith("bin") && filename == "sketch" ) {
                    if (commit != String(commitTag)) {

                        ESPMan_Debugf("START SKETCH DOWNLOAD (%s)\n", remote_path.c_str()  );

                        t_httpUpdate_return ret = ESPhttpUpdate.update(remote_path);

                        switch (ret) {
                        case HTTP_UPDATE_FAILED:
                            ESPMan_Debugf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                            break;

                        case HTTP_UPDATE_NO_UPDATES:
                            ESPMan_Debugf("HTTP_UPDATE_NO_UPDATES");
                            break;

                        case HTTP_UPDATE_OK:
                            ESPMan_Debugf("HTTP_UPDATE_OK");
                            ESP.restart();

                            break;
                        }

                        return ; // shouldn't get here...
                    } else {
                        ESPMan_Debugf("SKETCH HAS SAME COMMIT (%s)\n", commitTag  );

                    }
                }
            }
        }



    } else {

        ESPMan_Debugf("[ESPmanager::_HandleSketchUpdate] _parseUpdateJson FAILED\n");

    }

    if (buff) {
        delete[] buff;
    }

}


bool ESPmanager::_parseUpdateJson(uint8_t *& buff, DynamicJsonBuffer & jsonBuffer, JsonObject *& root, String path)
{
    const int bufsize = 2048;

    ESPMan_Debugf("[ESPmanager::_parseUpdateJson] path = %s\n", path.c_str());

    HTTPClient http;

    http.begin(path); //HTTP

    int httpCode = http.GET();

    if (httpCode > 0)  {

        if (httpCode == 200) {

            {
                ESPMan_Debugln("[ESPmanager::_parseUpdateJson] Connected downloading json");
            }

            size_t len = http.getSize();
            const size_t length = len;

            if (len > bufsize) {
                ESPMan_Debugln("[ESPmanager::_parseUpdateJson] Receive update length too big.  Increase buffer");
                return false;
            }

            //uint8_t buff[bufsize] = { 0 }; // max size of input buffer. Don't use String, as arduinoJSON doesn't like it!
            buff = nullptr;

            buff = new uint8_t[bufsize];

            //String payload = http.getString();

            if (buff) {
                // get tcp stream
                WiFiClient * stream = http.getStreamPtr();
                int position = 0;

                // read all data from server
                while (http.connected() && (len > 0 || len == -1)) {
                    // get available data size
                    size_t size = stream->available();
                    uint8_t * b = & buff[position];

                    if (size) {
                        int c = stream->readBytes(b, ((size > sizeof(buff)) ? sizeof(buff) : size));
                        position += c;
                        if (len > 0) {
                            len -= c;
                        }
                    }
                    delay(1);
                }
            } else {
                ESPMan_Debugf("[ESPmanager::_parseUpdateJson] failed to allocate buff\n");
            }

            http.end();


            // Serial.println("buf = ");
            // Serial.write(buff, length);
            // Serial.println();

            root = &jsonBuffer.parseObject( (char*)buff, length );

            if (root->success()) {
                ESPMan_Debugf("[ESPmanager::_parseUpdateJson] root->success() = true\n");
                return true;
            } else {
                ESPMan_Debugf("[ESPmanager::_parseUpdateJson] root->success() = false\n");
                return false;
            }

        } else {
            ESPMan_Debugf("[ESPmanager::_parseUpdateJson] HTTP code: %u\n", httpCode  );
        }

    } else {
        ESPMan_Debugf("[ESPmanager::_parseUpdateJson] get Failed code: %s\n", http.errorToString(httpCode).c_str());
    }

    return false;

}




#endif // #webupdate


String  ESPmanager::_file_md5 (File & f)
{
    // Md5 check

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

bool  ESPmanager::_FilesCheck(bool startwifi)
{

    bool haserror = false;
    bool present[file_no];

    for (uint8_t i = 0; i < file_no; i++) {
        if (!_fs.exists(TRUEfileslist[i])) {
            present[i] = false;
            haserror = true;
            ESPMan_Debugf("ERROR %s does not exist\n", TRUEfileslist[i]);
        } else {
            present[i] = true;
        }
    }


    if (haserror ) {

        // try to start wifi
        WiFi.mode(WIFI_STA); //  == WIFI_AP

        if ( (startwifi && Wifistart() ) || WiFi.status() == WL_CONNECTED) {
#ifdef DEBUG_ESP_PORT
            DEBUG_ESP_PORT.print("Connected to WiFi: ");
            DEBUG_ESP_PORT.println(WiFi.SSID());
#endif

// need this.. taken out tempararily
#ifdef USE_WEB_UPDATER
            return _upgrade();
#endif
        } else {

            ESPMan_Debugln(F("Attempted to download required files, failed no internet. Try hard coding credentials"));
        }

    }

    return !haserror;

}

void  ESPmanager::InitialiseSoftAP()
{
    ESPMan_Debugln("[ESPmanager::InitialiseSoftAP]");

    WiFiMode mode = WiFi.getMode();

    if (!WiFi.enableAP(true)) {
        WiFi.mode(WIFI_AP_STA);
    }

    mode = WiFi.getMode();

    if (_APmac) {
        if ( wifi_set_macaddr(0x01, _APmac)) {
            ESPMan_Debugln("AP MAC applied succesfully");
        } else {
            ESPMan_Debugln("AP MAC FAILED");
        }
    }

    if (mode == WIFI_AP_STA || mode == WIFI_AP) {

        ESPMan_Debugln("[ESPmanager::InitialiseSoftAP] Right mode detected");


        if ( _APssid && _APpass && _APchannel && _APhidden ) {
            ESPMan_Debugf("[ESPmanager::InitialiseSoftAP] ssid = %s, psk = %s, channel = %u, _APhidden = %s\n", _APssid, _APpass, _APchannel, (_APhidden) ? "true" : "false" );
            WiFi.softAP(_APssid, _APpass, _APchannel, _APhidden);

        } else if (_APssid && _APpass) {
            WiFi.softAP(_APssid, _APpass );
            ESPMan_Debugf("[ESPmanager::InitialiseSoftAP] ssid = %s, psk = %s\n", _APssid, _APpass);
        } else {
            ESPMan_Debugf("[ESPmanager::InitialiseSoftAP] ssid = %s\n", _APssid);
            WiFi.softAP(_APssid);
        }


        _APenabled = true;
    }

}


bool  ESPmanager::Wifistart()
{

    if (!WiFi.enableSTA(true)) {
        ESPMan_Debugln("[ESPmanager::Wifistart] Could not active STA mode");
        return false;
    }

    ESPMan_Debugf("[ESPmanager::Wifistart]  WiFi Mode = %u\n", WiFi.getMode());

    wl_status_t status = WiFi.status();

    ESPMan_Debugf("[ESPmanager::Wifistart]  Pre init - WiFiStatus = %u, ssid %s, psk %s \n", status, WiFi.SSID().c_str(), WiFi.psk().c_str());


    if (!_DHCP && _IPs) {
        //     void config(IPAddress local_ip, IPAddress gateway, IPAddress subnet);
        ESPMan_Debugln(F("[ESPmanager::Wifistart]  Using Stored IPs"));
        WiFi.config(_IPs->IP, _IPs->GW, _IPs->SN);
    }

    if (_STAmac) {
        if (wifi_set_macaddr(0x00, _STAmac)) {
            ESPMan_Debugln("[ESPmanager::Wifistart] STA MAC applied succesfully");
        } else {
            ESPMan_Debugln("[ESPmanager::Wifistart]  STA MAC FAILED");
        }
    }

    WiFi.begin(); // This screws EVERYTHING up.  just leave it out!

    ESPMan_Debugln("[ESPmanager::Wifistart]  WiFi init");
    uint8_t i = 0;
    uint32_t timeout = millis();

//  Try SDK connect first
    if (WiFi.SSID().length() > 0 && WiFi.psk().length() > 0 ) {
        ESPMan_Debugf("[ESPmanager::Wifistart]  waiting for SDK auto Connect\n");
        while (status != WL_CONNECTED) {// && status != WL_NO_SSID_AVAIL && status != WL_CONNECT_FAILED) {
            delay(10);
            status = WiFi.status();
            if (millis() - timeout > 30000)  {
                ESPMan_Debugln("[ESPmanager::Wifistart]  TIMEOUT");
                break;
            }
        }
    }

    status = WiFi.status();
    ESPMan_Debugf("[ESPmanager::Wifistart]  Autoconnect WiFiStatus = %u \n", status);

// Try Hard coded if present

    if (status != WL_CONNECTED && _ssid_hardcoded && _pass_hardcoded) {

        ESPMan_Debug(F("[ESPmanager::Wifistart]  Auto connect failed..\nTrying HARD CODED credentials...\n"));
        ESPMan_Debugf("[ESPmanager::Wifistart]  Using ssid %s, psk %s \n", _ssid_hardcoded, _pass_hardcoded );

        WiFi.begin(_ssid_hardcoded, _pass_hardcoded);
        timeout = millis();

        while (status != WL_CONNECTED) { //} && status != WL_NO_SSID_AVAIL && status != WL_CONNECT_FAILED) {
            delay(10);
            status = WiFi.status();
            if (millis() - timeout > 30000)  {
                ESPMan_Debugln("[ESPmanager::Wifistart]  TIMEOUT");
                break;
            }
        }

        if (status == WL_CONNECTED) {
            ESPMan_Debugf("[ESPmanager::Wifistart] Connected copying settigns accross\n");
            if (_ssid) {
                free((void*)_ssid);
                _ssid = nullptr;
            };
            if (_pass) {
                free((void*)_pass);
                _pass = nullptr;
            };

            _ssid = strdup(_ssid_hardcoded);
            _pass = strdup(_pass_hardcoded);

            SaveSettings();

        }



    }

//  Try Config_file Last
    if (status != WL_CONNECTED && _ssid && _pass) {

        ESPMan_Debug(F("Auto connect failed..\nTrying SPIFFS credentials...\n"));
        ESPMan_Debugf("Using ssid %s, psk %s \n", _ssid, _pass );

        WiFi.begin(_ssid, _pass);
        timeout = millis();

        while (status != WL_CONNECTED) { //} && status != WL_NO_SSID_AVAIL && status != WL_CONNECT_FAILED) {
            delay(10);
            status = WiFi.status();
            if (millis() - timeout > 30000)  {
                ESPMan_Debugln("TIMEOUT");
                break;
            }
        }
    }

    status = WiFi.status();
    ESPMan_Debugf("Stored Credentials WiFiStatus = %u \n", status);

    status = WiFi.status();

    if (status == WL_CONNECTED) {
        return true;
    } else {
        return false;
    }
};


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

// void  ESPmanager::_NewFilesCheck()
// {
//     bool found = false;

//     for (uint8_t i = 0; i < file_no; i++) {

//         if (_fs.exists(fileslist[i])) {
//             found = true;
//             String buf = "/espman";
//             buf += fileslist[i];
//             if (fileslist[i] == "/config.htm") { buf = "/espman/index.htm"; }
//             if (fileslist[i] == "/ajax-loader.gif") { buf = "/espman/images/ajax-loader.gif"; }
//             if (_fs.exists(buf)) {_fs.remove(buf); };
//             if (_fs.rename(fileslist[i], buf)) {
//                 ESPMan_Debugf("Found %s Renamed to %s\n", fileslist[i], buf.c_str());
//             } else {
//                 ESPMan_Debugf("Failed to rename %s ==> %s\n", fileslist[i], buf.c_str());
//             }
//         }; //  else ESPMan_Debugf("%s : Not found\n", fileslist[i]);
//     }

//     if (found) {
//         ESPMan_Debugf("[ESPmanager::_NewFilesCheck] Files Renames: Rebooting\n");
//         ESP.restart();
//     }

// }

//URI Decoding function
//no check if dst buffer is big enough to receive string so
//use same size as src is a recommendation
void  ESPmanager::urldecode(char *dst, const char *src)
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

bool  ESPmanager::StringtoMAC(uint8_t *mac, const String & input)
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

template <class T> void ESPmanager::sendJsontoHTTP( const T & root, AsyncWebServerRequest *request)
{
    AsyncResponseStream *response = request->beginResponseStream("text/json");
    root.printTo(*response);
    request->send(response);
}

void ESPmanager::handleFileUpload()
{
    // if (_HTTP.uri() != "/espman/upload") { return; }

    // static File * fsUploadFile;
    // HTTPUpload& upload = _HTTP.upload();

    // if (upload.status == UPLOAD_FILE_START) {
    //     if (fsUploadFile) {
    //         delete fsUploadFile;
    //         fsUploadFile = nullptr;
    //     };
    //     fsUploadFile = new File;
    //     String filename = upload.filename;
    //     filename.trim();
    //     if (!filename.startsWith("/")) { filename = "/" + filename; }
    //     // ESPMan_Debug("handleFileUpload Name: "); ESPMan_Debugln(filename);
    //     Serial.printf("Upload Name: %s\n", filename.c_str() );
    //     if (_fs.exists(filename)) { _fs.remove(filename); }

    //     *fsUploadFile = _fs.open(filename, "w+");

    //     filename = String();
    // } else if (upload.status == UPLOAD_FILE_WRITE) {
    //     if (*fsUploadFile) {
    //         fsUploadFile->write(upload.buf, upload.currentSize);
    //         ESPMan_Debug(".");
    //     };
    // } else if (upload.status == UPLOAD_FILE_END) {
    //     fsUploadFile->close();
    //     if (fsUploadFile) {
    //         delete fsUploadFile;
    //         fsUploadFile = nullptr;
    //         Serial.printf("\nDone Size: %u\n", upload.totalSize);
    //     } else { ESPMan_Debug("ERROR"); };
    // } else if (upload.status == UPLOAD_FILE_ABORTED) {
    //     Serial.printf("\nAborted");
    //     String filename = String(fsUploadFile->name());
    //     fsUploadFile->close();
    //     if (fsUploadFile) {
    //         delete fsUploadFile;
    //         fsUploadFile = nullptr;
    //     }
    //     if (_fs.exists(filename)) {
    //         _fs.remove(filename);
    //     }
    // }
}


void  ESPmanager::_HandleDataRequest(AsyncWebServerRequest *request)
{

    String buf;

#ifdef DEBUG_ESP_PORT
#ifdef ESPMan_Debug

//List all collected headers
    int params = request->params();
    int i;
    for (i = 0; i < params; i++) {
        AsyncWebParameter* h = request->getParam(i);
        DEBUG_ESP_PORT.printf("[ESPmanager::_HandleDataRequest] [%s]: %s\n", h->name().c_str(), h->value().c_str());
    }
#endif
#endif

    /*------------------------------------------------------------------------------------------------------------------
                                                                    Reboot command
    ------------------------------------------------------------------------------------------------------------------*/
    if (request->hasParam("body", true)) {

        //ESPMan_Debugln(F("Has Body..."));


        String plainCommand = request->getParam("body", true)->value();

        if ( plainCommand == F("reboot") || plainCommand == F("restart")) {
            ESPMan_Debugln(F("Rebooting..."));
            request->send(200, "text", "OK"); // return ok to speed up AJAX stuff
            ESP.restart();
            return;
        };

        /*------------------------------------------------------------------------------------------------------------------
                                          WiFi Scanning and Sending of WiFi networks found at boot
        ------------------------------------------------------------------------------------------------------------------*/
        if ( plainCommand == F("WiFiDetails") || plainCommand == F("PerformWiFiScan")) {

            DynamicJsonBuffer jsonBuffer;
            JsonObject& root = jsonBuffer.createObject();

//************************
            //  might have to do this ASYNC
            if (plainCommand == F("PerformWiFiScan")) {
                ESPMan_Debug(F("Performing Wifi Network Scan..."));
                _wifinetworksfound = WiFi.scanNetworks(true);


                _syncCallback = [request, this]() {

                    if (_wifinetworksfound == WIFI_SCAN_RUNNING) {
                        //ESPMan_Debug("SCANNING: ");
                        static uint32_t last_check = 0;

                        if (millis() - last_check > 500) {

                            ESPMan_Debug("Checking WiFiScan State: ");

                            _wifinetworksfound = WiFi.scanComplete();


                            last_check = millis();

                            if (_wifinetworksfound < 0) {
                                ESPMan_Debugln("in progress.....");
                            }

                        }

                        if (_wifinetworksfound > 0) {
                            ESPMan_Debug("Done : Found ");
                            ESPMan_Debug(_wifinetworksfound);
                            ESPMan_Debugln("networks");


                            DynamicJsonBuffer jsonBuffer;
                            JsonObject& root = jsonBuffer.createObject();

                            JsonArray& Networkarray = root.createNestedArray("networks");

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
                                    ssidobject[F("encyrpted")] = "OPEN";
                                    break;
                                case ENC_TYPE_WEP:
                                    ssidobject[F("encyrpted")] = "WEP";
                                    break;
                                case ENC_TYPE_TKIP:
                                    ssidobject[F("encyrpted")] = "WPA_PSK";
                                    break;
                                case ENC_TYPE_CCMP:
                                    ssidobject[F("encyrpted")] = "WPA2_PSK";
                                    break;
                                case ENC_TYPE_AUTO:
                                    ssidobject[F("encyrpted")] = "AUTO";
                                    break;
                                }

                                ssidobject[F("BSSID")] = WiFi.BSSIDstr(i);
                            }

                            if (request) {
                                sendJsontoHTTP(root, request);
                                //_syncCallback = nullptr;
                                WiFi.scanDelete();
                                _wifinetworksfound = 0;
                                return true;
                            }
                        }

                        return false; //  scan not complete yet..

                    }
                };

                ESPMan_Debug("Found :");
                ESPMan_Debug(_wifinetworksfound);
                ESPMan_Debugln(" networks");
                return;
            }
//*************************



            //const int BUFFER_SIZE = JSON_OBJECT_SIZE( _wifinetworksfound * 6) + JSON_ARRAY_SIZE(_wifinetworksfound) + JSON_OBJECT_SIZE(22);

            WiFiMode mode = WiFi.getMode();

            JsonObject& generalobject = root.createNestedObject("general");

            generalobject[_pdeviceid] = _host;
            generalobject[F("OTAenabled")] = (_OTAenabled) ? true : false;
            generalobject[F("OTApassword")] = (_OTApassword) ? _OTApassword : C_null;

            generalobject[F("APrestartmode")] = _APrestartmode;
            //generalobject[F("OTAusechipID")] = _OTAusechipID;
            generalobject[F("mDNSenabled")] = (_mDNSenabled) ? true : false;

            JsonObject& STAobject = root.createNestedObject("STA");

            STAobject[F("connectedssid")] = WiFi.SSID();

            STAobject[F("dhcp")] = (_DHCP) ? true : false;
            STAobject[F("state")] = (mode == WIFI_STA || mode == WIFI_AP_STA) ? true : false;
            //String ip;

            STAobject[F("IP")] = WiFi.localIP().toString();

            STAobject[F("gateway")] = WiFi.gatewayIP().toString() ;
            STAobject[F("subnet")] = WiFi.subnetMask().toString() ;
            STAobject[F("MAC")] = WiFi.macAddress();

            JsonObject& APobject = root.createNestedObject("AP");

            APobject[F("ssid")] = _APssid;
            APobject[F("state")] = (mode == WIFI_AP || mode == WIFI_AP_STA) ? true : false;
            APobject[F("APenabled")] = _APenabled;

            APobject[F("IP")] = (WiFi.softAPIP()[0] == 0 && WiFi.softAPIP()[1] == 0
                                 && WiFi.softAPIP()[2] == 0 && WiFi.softAPIP()[3] == 0)
                                ? F("192.168.4.1")
                                : WiFi.softAPIP().toString();
            APobject[F("hidden")] = (_APhidden) ? true : false;
            APobject[F("password")] = (_APpass) ? _APpass : "";
            APobject[F("channel")] = _APchannel;
            APobject[F("MAC")] = WiFi.softAPmacAddress();


            sendJsontoHTTP(root, request);

            //WiFi.scanDelete();
            //_wifinetworksfound = 0;


            return;
        }

        /*------------------------------------------------------------------------------------------------------------------
                                          Send about page details...
        ------------------------------------------------------------------------------------------------------------------*/
        if (plainCommand == "AboutPage") {

            FSInfo info;
            _fs.info(info);

            const uint8_t bufsize = 10;
            int sec = millis() / 1000;
            int min = sec / 60;
            int hr = min / 60;
            int Vcc = analogRead(A0);

            char Up_time[bufsize];
            snprintf(Up_time, bufsize, "%02d:%02d:%02d", hr, min % 60, sec % 60);

            const int BUFFER_SIZE = JSON_OBJECT_SIZE(30); // + JSON_ARRAY_SIZE(temphx.items);
            DynamicJsonBuffer jsonBuffer(BUFFER_SIZE);

            JsonObject& root = jsonBuffer.createObject();

            root[F("version_var")] = "Settings Manager V" ESPMANVERSION;
            root[F("compiletime_var")] = _compile_date_time;

            root[F("chipid_var")] = ESP.getChipId();
            root[F("cpu_var")] = ESP.getCpuFreqMHz();
            root[F("sdk_var")] = ESP.getSdkVersion();
            root[F("bootverion_var")] =  ESP.getBootVersion();
            root[F("bootmode_var")] =  ESP.getBootMode();

            root[F("heap_var")] = ESP.getFreeHeap();
            root[F("millis_var")] = millis();
            root[F("uptime_var")] = Up_time;

            root[F("flashid_var")] = ESP.getFlashChipId();
            root[F("flashsize_var")] = formatBytes( ESP.getFlashChipSize() );
            root[F("flashRealSize_var")] = formatBytes (ESP.getFlashChipRealSize() ); // not sure what the difference is here...
            root[F("flashchipsizebyid_var")] = formatBytes (ESP.getFlashChipSizeByChipId());
            root[F("flashchipmode_var")] = (uint32_t)ESP.getFlashChipMode();

            root[F("chipid_var")] = ESP.getChipId();
            String sketchsize = formatBytes(ESP.getSketchSize()) ;//+ " ( " + String(ESP.getSketchSize()) +  " Bytes)";
            root[F("sketchsize_var")] = sketchsize;
            String freesketchsize = formatBytes(ESP.getFreeSketchSpace()) ;//+ " ( " + String(ESP.getFreeSketchSpace()) +  " Bytes)";
            root[F("freespace_var")] = freesketchsize ;

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

            sendJsontoHTTP(root, request);
            return;
        }

        /*------------------------------------------------------------------------------------------------------------------
                                          SSID handles...
        ------------------------------------------------------------------------------------------------------------------*/
    } //  end of if plain

    static int8_t WiFiresult = -1;

    if (request->hasParam("ssid", true) && request->hasParam("pass", true)) {

        String ssid = request->getParam("ssid", true)->value();
        String psk = request->getParam("pass", true)->value();

        if (ssid.length() > 0) {
            // _HTTP.arg("pass").length() > 0) {  0 length passwords should be ok.. for open
            // networks.
            if (ssid.length() < 33 && psk.length() < 33) {

                if (ssid != WiFi.SSID() || psk != WiFi.psk()) {

                    bool safety = false;

                    if (request->hasParam("removesaftey", true))  {
                        safety = (request->getParam("removesaftey", true)->value() == "No") ? false : true;
                    }

                    _syncCallback = [safety, psk, ssid, request, this]() {

                        if (WiFi.getMode() == WIFI_AP) { WiFi.mode(WIFI_AP_STA);  }; // if WiFi STA off, turn it on..


                        save_flag = true;
                        WiFiresult = -1;
                        bool restore_old_settings = false;

                        char old_ssid[33] = {0};
                        char old_pass[33] = {0};

                        if (WiFi.status() == WL_CONNECTED) {
                            strcpy(old_pass, (const char*)WiFi.psk().c_str());
                            strcpy(old_ssid, (const char*)WiFi.SSID().c_str());
                            restore_old_settings = true;
                        }

                        //request->send(200, "text", "accepted"); // important as it defines entry to the wait loop on client
//***************************  again maybe need to go ASYNC
                        ESPMan_Debug(F("Disconnecting.."));

                        WiFi.disconnect();

                        ESPMan_Debug(F("done \nInit..."));


                        //     _ssid = strdup((const char *)_HTTP.arg("ssid").c_str());
                        //     _pass = strdup((const char *)_HTTP.arg("pass").c_str());

                        /*

                                ToDo....  put in check for OPEN networks that enables the AP for 5
                           mins....

                        */

                        //ESPMan_Debugf("[wifidebug] ssid = %s, psk %s\n", (const char*)ssid.c_str(), (const char*)psk.c_str() );

                        //WiFi.printDiag(Serial);

                        //  First try does not change the vars
                        if (psk.length() == 0 ) {
                            WiFi.begin((const char*)ssid.c_str());
                        } else {
                            WiFi.begin((const char*)ssid.c_str(), (const char*)psk.c_str());
                        }


                        uint8_t i = 0;

                        while (WiFi.status() != WL_CONNECTED && !safety) {
                            WiFiresult = 0;
                            delay(500);
                            i++;
                            ESPMan_Debug(".");
                            if (i == 30 && restore_old_settings) {

                                ESPMan_Debug(F("Unable to join network...restore old settings"));
                                save_flag = false;
                                WiFiresult = 2;

                                //    strcpy(_ssid,old_ssid);
                                //    strcpy(_pass,old_pass);

                                WiFi.disconnect();
                                WiFi.begin(_ssid, _pass);

                                while (WiFi.status() != WL_CONNECTED) {
                                    delay(500);
                                    ESPMan_Debug(".");
                                };
                                ESPMan_Debug(F("done"));
                                break;
                            } else if (i == 30 && !restore_old_settings ) { break; }
                        }

                        ESPMan_Debugf("\nconnected: SSID = %s, pass = %s\n", WiFi.SSID().c_str(), WiFi.psk().c_str());

                        if (WiFiresult == 0) {
                            WiFiresult = 1;    // not sure why i did this.. think it is the client end.
                        }

                        if (WiFiresult) {

                            if (_ssid) {
                                free((void*)_ssid);
                                _ssid = NULL;
                            };
                            if (_pass) {
                                free((void*)_pass);
                                _pass = NULL;
                            };
                            _ssid = strdup((const char*)ssid.c_str());
                            _pass = strdup((const char*)psk.c_str());
                            ESPMan_Debugln(F("New SSID and PASS work:  copied to system vars"));
                        }

                        //_syncCallback = nullptr;
                        return true;
                    }; //  end of lambda...

                    return;
                }
            }
        }
    }
//*******************************************

    //  This is outside the loop...  wifiresult is a static to return previous result...
    if (  request->hasParam("body", true) && request->getParam("body", true)->value() == "WiFiresult") {
        request->send(200, "text", String(WiFiresult));
        return;
    }
    /*------------------------------------------------------------------------------------------------------------------

                                     STA config
    ------------------------------------------------------------------------------------------------------------------*/

    if (request->hasParam("enable-STA", true)) {
        save_flag = true;

        // IPAddress localIP, gateway, subnet;
        WiFiMode mode = WiFi.getMode();
        bool reinit = false;

        String enable_sta = request->getParam("enable-STA", true)->value();

        if (enable_sta == "off" && (mode == WIFI_STA || mode == WIFI_AP_STA)) {
            _STAenabled = false;

            _syncCallback = [this]() {
                WiFi.mode(WIFI_AP);
                InitialiseSoftAP();
                return true;
            };

            // WiFi.mode(WIFI_AP); // always fall back to AP mode...
            // WiFi.softAP(_APssid, _APpass, (int)_APchannel, (int)_APhidden);
            ESPMan_Debugln(F("STA-disabled: falling back to AP mode."));
        } else if (enable_sta == "on" && mode == WIFI_AP) {
            _STAenabled = true;

            _syncCallback = [this]() {
                Wifistart();
                return true;
            };

            //WiFi.mode(WIFI_AP_STA);
            ESPMan_Debugln(F("Enabling STA mode"));
            reinit = true;
        }


        if ( request->hasParam("enable-dhcp", true) &&  request->getParam("enable-dhcp", true)->value() == "on") {

            _DHCP = true;
            save_flag = true;
            bool dhcpresult = wifi_station_dhcpc_start(); // requires userinterface...

            ESPMan_Debugln(F("------- RESARTING--------"));
            ESPMan_Debug(F("DHCP result: "));
            ESPMan_Debugln(dhcpresult);
            reinit = true;
        } else if (request->getParam("enable-dhcp", true)->value() == "off") {

            save_flag = true;
            _DHCP = false;

            if (!_IPs) {
                _IPs = new IPconfigs_t;    // create memory for new IPs
            }

            bool ok = true;

            if (request->hasParam("setSTAsetip", true)) {
                _IPs->IP.fromString( request->getParam("setSTAsetip", true)->value() );
                ESPMan_Debug(F("IP = "));
                ESPMan_Debugln(_IPs->IP);
            } else {
                ok = false;
            }

            if (request->hasParam("setSTAsetgw", true)) {
                _IPs->GW.fromString(request->getParam("setSTAsetgw", true)->value());
                ESPMan_Debug(F("gateway = "));
                ESPMan_Debugln(_IPs->GW);
            } else {
                ok = false;
            }

            if (request->hasParam("setSTAsetsn", true)) {
                _IPs->SN.fromString(request->getParam("setSTAsetsn", true)->value());
                ESPMan_Debug(F("subnet = "));
                ESPMan_Debugln(_IPs->SN);
            } else {
                ok = false;
            }

            if (ok) {
                // WiFi.config(localIP, gateway, subnet);
                reinit = true;
            }

            if ( request->hasParam("setSTAsetmac", true) && request->getParam("setSTAsetmac", true)->value().length() != 0) {

                uint8_t mac_addr[6];

                if ( StringtoMAC(mac_addr, request->getParam("setSTAsetmac", true)->value() ) ) {


                    ESPMan_Debugln("New MAC parsed sucessfully");
                    ESPMan_Debugf("[%u]:[%u]:[%u]:[%u]:[%u]:[%u]\n", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

                    if (!_STAmac) {
                        _STAmac = new uint8_t[6];
                    }
                    memcpy (_STAmac, mac_addr, 6);
                    save_flag = true;

                } else {
                    ESPMan_Debugln("New MAC parsed FAILED");
                }


            }

        }

        if (reinit &&  request->hasParam("enable-STA", true) &&   request->getParam("enable-STA", true)->value() == "on") {
            _syncCallback = [this]() {
                Wifistart();
                return true;
            };
        }
        //printdiagnositics();


        /*

        IPaddress.toCharArray()

        ARG: 0, "enable-dhcp" = "off"
        ARG: 1, "setSTAsetip" = "192.168.1.207"
        ARG: 2, "setSTAsetgw" = "192.168.1.1"
        ARG: 3, "setSTAsetsn" = "255.255.255.0"
        ARG: 4, "setSTAsetmac" = "18%3AFE%3A34%3AA4%3A4C%3A73"

        config(IPAddress local_ip, IPAddress gateway, IPAddress subnet);

        */
    }

    /*------------------------------------------------------------------------------------------------------------------

                                     AP config
    ------------------------------------------------------------------------------------------------------------------*/

    if (request->hasParam("enable-AP", true)) {
        save_flag = true;

        WiFiMode mode = WiFi.getMode();
        // WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3
        // if (mode == WIFI_STA || mode == WIFI_AP_STA ) STAstate = "ENABLED"; else STAstate =
        // "DISABLED";
        // if (mode == WIFI_AP || mode == WIFI_AP_STA ) APstate = "ENABLED"; else APstate =
        // "DISABLED";

        if ( request->hasParam("setAPsetssid-AP", true) && request->getParam("setAPsetssid-AP", true)->value().length() != 0) {

            if (_APssid) {
                free((void*)_APssid);
                _APssid = NULL;
            }
            _APssid = strdup((const char*)request->getParam("setAPsetssid-AP", true)->value().c_str());
        };

        if (request->hasParam("setAPsetpass", true)  && request->getParam("setAPsetpass", true)->value().length() != 0
                && request->getParam("setAPsetpass", true)->value().length() < 63) {
            //_APpass = (const char *)_HTTP.arg("setAPsetpass").c_str();

            if (_APpass) {
                free((void*)_APpass);
                _APpass = NULL;
            }; // free memory if AP pass has been allocated.

            _APpass = strdup((const char*)request->getParam("setAPsetpass", true)->value().c_str());
        } else if (request->hasParam("setAPsetpass", true) &&
                   request->getParam("setAPsetpass", true)->value().length() == 0 && _APpass) {

            if (_APpass) {
                free((void*)_APpass);
                _APpass = NULL;
            }; // free memory if AP pass has been allocated.
        };

        if (request->hasParam("setAPsetmac", true) && request->getParam("setAPsetmac", true)->value().length() != 0) {

            uint8_t mac_addr[6];

            if ( StringtoMAC(mac_addr, request->getParam("setAPsetmac", true)->value() ) ) {


                ESPMan_Debugln("New AP MAC parsed sucessfully");
                ESPMan_Debugf("[%u]:[%u]:[%u]:[%u]:[%u]:[%u]\n", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

                if (!_APmac) {
                    _APmac = new uint8_t[6];
                }
                memcpy (_APmac, mac_addr, 6);
                save_flag = true;

            } else {
                ESPMan_Debugln("New AP MAC parsed FAILED");
            }
        }


        if (  request->hasParam("enable-AP", true) && request->getParam("enable-AP", true)->value() == "on") {

            uint8_t channel = 1;

            if (request->hasParam("setAPsetchannel", true)) {
                channel = request->getParam("setAPsetchannel", true)->value().toInt();
            }

            if (channel > 13) {
                channel = 13;
            }

            _APchannel = channel;

            ESPMan_Debug(F("Enable AP channel: "));
            ESPMan_Debugln(_APchannel);

            _APenabled = true;

            InitialiseSoftAP();

            //printdiagnositics();
        } else if (request->hasParam("enable-AP", true) && request->getParam("enable-AP", true)->value() == "off") {

            ESPMan_Debugln(F("Disable AP"));
            _APenabled = false;

            if (mode == WIFI_AP_STA || mode == WIFI_AP) { WiFi.mode(WIFI_STA); }

            // if (WiFi.status() == WL_CONNECTED ) {
            //   //WiFi.softAPdisconnect(true);
            //  printdiagnositics();
            // } else  {


            // }
        }


    } //  end of enable-AP

    /*------------------------------------------------------------------------------------------------------------------

                                     Device Name
    ------------------------------------------------------------------------------------------------------------------*/

    if ( request->hasParam(_pdeviceid, true)) {

        ESPMan_Debugln(F("ID func hit"));
        String id = request->getParam(_pdeviceid, true)->value();

        _syncCallback = [id, this]() {

            if (id.length() != 0 &&
                    id.length() < 32 &&
                    id != String(_host) ) {
                save_flag = true;
                ESPMan_Debugln(F("Device ID changed"));
                //      if (_host) free( (void*)_host);

                if (_host && _APssid) {
                    if (strcmp(_host, _APssid) == 0) {

                        if (_APssid) {
                            free((void*)_APssid);
                            _APssid = NULL;
                        }
                        _APssid = strdup((const char*)id.c_str());
                    }
                }

                if (_host) {
                    free((void*)_host);
                    _host = NULL;
                }
                _host = strdup((const char*)id.c_str());
                //  might need to add in here, wifireinting...
                WiFi.hostname(_host);
                InitialiseFeatures();
                InitialiseSoftAP();
            }

            //_syncCallback = nullptr;
            return true;
        };

    }
    /*------------------------------------------------------------------------------------------------------------------

                                     Restart wifi
    ------------------------------------------------------------------------------------------------------------------*/

    if ( request->hasParam("body", true) && request->getParam("body", true)->value() == "resetwifi") {


        _syncCallback = []() {

            ESP.eraseConfig();
            WiFi.disconnect();
            ESP.restart();
            return true;
        };

    }

    /*------------------------------------------------------------------------------------------------------------------

                                     OTA config
    ------------------------------------------------------------------------------------------------------------------*/
    // if (_HTTP.hasArg("OTAusechipID")) {

    //     _OTAusechipID = (_HTTP.arg("OTAusechipID") == "Yes")? true : false;
    //     ESPMan_Debugln(F("OTA append ChipID to host"));
    //     save_flag = true;
    // }

    if ( request->hasParam("otaenable", true)) {
        ESPMan_Debugln(F("Depreciated"));
        // save_flag = true;

        // bool command = (_HTTP.arg("otaenable") == "on") ? true : false;

        // if (command != _OTAenabled)
        // {
        //     _OTAenabled = command;


        //     if (_OTAenabled)
        //     {
        //         // long before = ESP.getFreeHeap();
        //         ESPMan_Debugln(F("Enable OTA"));

        //         InitialiseFeatures();
        //         // String insert = F("OTA Enabled, heap used: %u \n");
        //         // ESPMan_Debugf(insert.c_str(), before - ESP.getFreeHeap() );
        //     }
        //     else
        //     {

        //         // long before = ESP.getFreeHeap();
        //         ESPMan_Debugln(F("Disable OTA"));

        //         if (ota_server)
        //         {
        //             delete ota_server;
        //             ota_server = NULL;
        //         };

        //         // ESPMan_Debugf( "OTA deactivated, heap reclaimed: %u \n", ESP.getFreeHeap() - before );
        //     }
        // }
    } // end of OTA enable

    /*
    ARG: 0, "enable-AP" = "on"
    ARG: 1, "setAPsetip" = "0.0.0.0"
    ARG: 2, "setAPsetmac" = "1A%3AFE%3A34%3AA4%3A4C%3A73"
    */

    if ( request->hasParam("mdnsenable", true)) {


        save_flag = true;

        bool command = ( request->getParam("mdnsenable", true)->value() == "on") ? true : false;

        if (command != _mDNSenabled) {
            _mDNSenabled = command;
            InitialiseFeatures();
        }
    } // end of OTA enable
    /*------------------------------------------------------------------------------------------------------------------

                                       FORMAT SPIFFS
      ------------------------------------------------------------------------------------------------------------------*/

    if (request->hasParam("body", true)) {

        String plaincmd = request->getParam("body", true)->value();
// ********************  may ned to be async
        if (plaincmd == "formatSPIFFS") {
            ESPMan_Debug(F("Format SPIFFS"));
            request->send(200, "text", "OK");

            _syncCallback = [this]() {
                _fs.format();
                ESPMan_Debugln(F(" done"));
                return true;
            };
        }
// **************************

#ifdef USE_WEB_UPDATER

        if (plaincmd == "upgrade") {
            static uint32_t timeout = 0;

            if (millis() - timeout > 30000 || timeout == 0 ) {
#ifdef DEBUG_ESP_PORT
                DEBUG_ESP_PORT.println("Upgrade Started..");
#endif
                if (_upgrade()) {
                    request->send(200, "SUCCESS");
#ifdef DEBUG_ESP_PORT
                    DEBUG_ESP_PORT.println("Download Finished.  Files Updated");
#endif
                    //_NewFilesCheck();
                } else {
                    request->send(200, "FAILED");
#ifdef DEBUG_ESP_PORT
                    DEBUG_ESP_PORT.println("Error.  Try Again");
#endif
                }
                timeout = millis();
                return;
            }
        }

#endif

        if (plaincmd == "deletesettings") {

            ESPMan_Debug(F("Delete Settings File"));
            if (_fs.remove(SETTINGS_FILE)) {
                ESPMan_Debugln(F(" done"));
            } else {
                ESPMan_Debugln(F(" failed"));
            }
        }
    }
    /*------------------------------------------------------------------------------------------------------------------

                                       MAC address STA + AP
      ------------------------------------------------------------------------------------------------------------------*/





    /*------------------------------------------------------------------------------------------------------------------

                                       AP reboot behaviour
      ------------------------------------------------------------------------------------------------------------------*/

    if (request->hasParam("select-AP-behaviour", true) ) {

        ESPMan_Debugln("Recieved AP behaviour request");
        int rebootvar = request->getParam("select-AP-behaviour", true)->value().toInt();

        if (rebootvar == 1 || rebootvar == 2 || rebootvar == 3 || rebootvar == 4 ) {
            _APrestartmode = rebootvar;
            save_flag = true;
        }

    }

    //_HTTP.setContentLength(2);
    request->send(200, "text", "OK"); // return ok to speed up AJAX stuff
}



ESPmanager::version_state ESPmanager::CheckVersion( String current, String check)
{
    int current_placeholders = current.indexOf(".");
    int check_placeholders = check.indexOf(".");


    int * current_array = new int[ current_placeholders ];
    int * check_array = new int[ check_placeholders];


    if (current_array && current_placeholders) {
        for (int i = 0; i < current_placeholders; i++) {

        }

        for (int i = 0; i < check_placeholders; i++) {

        }
    }

}








