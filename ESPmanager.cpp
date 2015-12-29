
#include "ESPmanager.h"

extern "C" {
#include "user_interface.h"
}


ESPmanager::ESPmanager(
    ESP8266WebServer & HTTP, FS & fs, const char* host, const char* ssid, const char* pass) : _HTTP(HTTP), _fs(fs)
{
    httpUpdater.setup(&_HTTP);


    // This sets the default fallback options... not in use yet
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

void cache ESPmanager::begin()
{

    ESPMan_Debugln("Settings Manager V" ESPMANVERSION);

    wifi_set_sleep_type(NONE_SLEEP_T); // workaround no modem sleep.
    WiFi.persistent(true); // new changes to wifi mode.  assume user wants persistent...

    if (_fs.begin()) {
        ESPMan_Debugln(F("File System mounted sucessfully"));

        if (!_FilesCheck(true)) {
            ESPMan_Debugln(F("Major FAIL, required files are NOT in SPIFFS, please upload required files"));
        }

        _NewFilesCheck();

        if ( LoadSettings() ) {
            ESPMan_Debugln("Load settings returned true");
        } else {
            ESPMan_Debugln("Load Settings returned false");
        }

    } else {
        ESPMan_Debugln(F("File System mount failed"));
    }

    if (_manageWiFi) {

        Serial.print("Connecting to WiFi...");
        WiFi.mode(WIFI_STA);

        if (!Wifistart()) {

            if (_APrestartmode > 1) { // 1 = none, 2 = 5min, 3 = 10min, 4 = whenever : 0 is reserved for unset...

                _APtimer = millis();
                ESPMan_Debug(F("WiFi Failed: "));
                if (!_APenabled) {
                    InitialiseSoftAP();
                    ESPMan_Debug(F("Starting AP"));
                }
                ESPMan_Debugln();
            }
        } else {
            Serial.print(F("Success\nConnected to "));
            Serial.print(WiFi.SSID());
            Serial.print(" (");
            Serial.print(WiFi.localIP());
            Serial.println(")");
        }
    }


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

    if (!_APssid) {
        // if (wifi_station_get_hostname())
        //     _APssid = strdup(wifi_station_get_hostname());
        // else
        _APssid = _host;
    }

    if (_APenabled) {
        ESPMan_Debugln(F("Soft AP enabled by config"));
        InitialiseSoftAP();
    } else ESPMan_Debugln(F("Soft AP disbaled by config"));



    // printdiagnositics();

    InitialiseFeatures();

    _HTTP.on("/espman/data.esp", std::bind(&ESPmanager::HandleDataRequest, this));
    _HTTP.on("/espman/upload", HTTP_POST , [this]() { _HTTP.send(200, "text/plain", ""); }, std::bind(&ESPmanager::handleFileUpload, this)  );
    _HTTP.serveStatic("/espman", _fs, "/espman", "max-age=86400");

}

bool cache ESPmanager::LoadSettings()
{

    DynamicJsonBuffer jsonBuffer(1000);
    File f = _fs.open(SETTINGS_FILE, "r");
    if (!f) {
        ESPMan_Debugln(F("Settings file open failed!"));
        return false;
    }

    f.seek(0, SeekSet);

    char data[f.size()];

    for (int i = 0; i < f.size(); i++) {
        data[i] = f.read();
    }

    f.close();

    JsonObject& root = jsonBuffer.parseObject(data);

    if (!root.success()) {
        ESPMan_Debugln(F("Parsing settings file Failed!"));
        return false;
    }

    if (root.containsKey("host")) {
        const char* host = root[F("host")];
        if (_host) {
            free((void*)_host);
            _host = nullptr;
        };
        if (host) { _host = strdup(host); } else { _host = nullptr ; } ;
    }

    if (root.containsKey("ssid")) {
        const char* ssid = root["ssid"];
        if (_ssid) {
            free((void*)_ssid);
            _ssid = nullptr;
        };
        if (ssid) { _ssid = strdup(ssid); } else { _ssid = nullptr ; } ;
    }

    if (root.containsKey("pass")) {
        const char* pass = root["pass"];
        if (_pass) {
            free((void*)_pass);
            _pass = nullptr;
        };
        if (pass) { _pass = strdup(pass); } else { _pass = nullptr; } ;
    }

    if (root.containsKey("APpass")) {
        const char* APpass = root["APpass"];
        if (_APpass) {
            free((void*)_APpass);
            _APpass = nullptr;
        };
        if (APpass) { _APpass = strdup(APpass); } else { _APpass = nullptr; } ;
    }

    if (root.containsKey("APssid")) {
        const char* APssid = root["APssid"];
        if (_APssid) {
            free((void*)_APssid);
            _APssid = nullptr;
        };
        if (APssid) { _APssid = strdup(APssid); } else {_APssid = nullptr; } ;
    }

    if (root.containsKey("APchannel")) {
        long APchannel = root["APchannel"];
        if (APchannel < 13 && APchannel > 0)
            _APchannel = (uint8_t)APchannel;
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

    if (root.containsKey("OTApassword")) {
        const char* OTApassword = root["OTApassword"];
        if (_OTApassword) {
            free((void*)_OTApassword);
            _OTApassword = nullptr;
        };
        if (OTApassword) { _OTApassword = strdup(OTApassword); } else {_OTApassword = nullptr; } ;
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
            _IPs->IP = StringtoIP(String(ip));
            _IPs->GW = StringtoIP(String(gw));
            _IPs->SN = StringtoIP(String(sn));
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


    ESPMan_Debugln(F("----- Saved Variables -----"));
    PrintVariables();
    ESPMan_Debugln(F("---------------------------"));
    return true;
}


void cache ESPmanager::PrintVariables()
{

#ifdef DEBUG_YES
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

    if (_IPs) {
        ESPMan_Debug(F("IPs->IP = "));
        ESPMan_Debugln(IPtoString(_IPs->IP));
        ESPMan_Debug(F("IPs->GW = "));
        ESPMan_Debugln(IPtoString(_IPs->GW));
        ESPMan_Debug(F("IPs->SN = "));
        ESPMan_Debugln(IPtoString(_IPs->SN));
    } else
        ESPMan_Debugln(F("NO IPs held in memory"));
    if (_STAmac) {
        ESPMan_Debug(F("STA MAC = "));
        ESPMan_Debugf("%02X:%02X:%02X:%02X:%02X:%02X\n", _STAmac[0],  _STAmac[1], _STAmac[2], _STAmac[3], _STAmac[4], _STAmac[5]);

    } else ESPMan_Debugln("STA MAC not held in memory");
    if (_APmac) {
        ESPMan_Debug(F("AP MAC = "));
        ESPMan_Debugf("%02X:%02X:%02X:%02X:%02X:%02X\n", _APmac[0],  _APmac[1], _APmac[2], _APmac[3], _APmac[4], _APmac[5]);
    } else ESPMan_Debugln("AP MAC not held in memory");
#endif
}

void cache ESPmanager::SaveSettings()
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

    //root[F("OTAusechipID")] = (_OTAusechipID) ? true : false;
    root[F("mDNSenable")] = (_mDNSenabled) ? true : false;
    root[F("WiFimanage")] = (_manageWiFi) ? true : false;

    char IP[30];
    String ip = IPtoString(WiFi.localIP());
    ip.toCharArray(IP, ip.length() + 3);
    char GW[30];
    String gw = IPtoString(WiFi.gatewayIP());
    gw.toCharArray(GW, gw.length() + 3);
    char SN[30];
    String sn = IPtoString(WiFi.subnetMask());
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

void cache ESPmanager::handle()
{

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
        if (_APrestartmode == 2) timer = 5 * 60 * 1000;
        if (_APrestartmode == 3) timer = 10 * 60 * 1000;
        if (millis() - _APtimer > timer) {
            WiFi.mode(WIFI_STA); //  == WIFI_AP
            ESPMan_Debugln("AP Stopped");
        }
    }

    if (WiFi.status() != WL_CONNECTED && _APrestartmode == 4) {
        ESPMan_Debugln(F("WiFi Disconnected:  Starting AP"));
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(_APssid, _APpass, (int)_APchannel, (int)_APhidden);
        ESPMan_Debugln(F("Done"));
    }


}

void cache ESPmanager::InitialiseFeatures()
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

    ArduinoOTA.onStart([]() {
        Serial.print(F(   "[              Performing OTA Upgrade              ]\n["));
//                       ("[--------------------------------------------------]\n ");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println(F("]\nOTA End"));
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static uint8_t done = 0;
        uint8_t percent = (progress / (total / 100) );
        if ( percent % 2 == 0  && percent != done ) {
            Serial.print("-");
            done = percent;
        }
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
        else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
        else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
        else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
        else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
    });

    ArduinoOTA.begin();


// not sure this is needed now.

    if (_mDNSenabled) {
        MDNS.addService("http", "tcp", 80);
    }



    WiFi.hostname(_host);
}

// bool cache ESPmanager::HTTPSDownloadtoSPIFFS(const char * remotehost, const char * fingerprint, const char * path, const char * file) {

//     const size_t buf_size = 1024;
//     uint8_t buf[buf_size];
//     const int httpsPort = 443;
//     WiFiClientSecure SecClient;

//     size_t totalbytes = 0;

//     File f = _fs.open("/textfile", "w");

//     if (!f) {
//         Serial.println("file open failed");
//         return false;
//     } else {
//         Serial.println("File Created");
//         delay(100);

//         Serial.printf("HOST: %s:%u\n", remotehost, httpsPort);

//         if (!SecClient.connect(remotehost, httpsPort)) {
//             Serial.println("Connection failed");
//             return false;
//         } else {
//             Serial.printf("Connected to %s\n", remotehost);

//             if (SecClient.verify(fingerprint, remotehost)) {
//                 Serial.println("certificate matches");
//             } else {
//                 Serial.println("certificate doesn't match");
//                 return false;
//             }
//             // send GET request

//             String request = "GET " + String(path) + String(file) + " HTTP/1.1\r\n";
//             request += "Host: " + String(remotehost) + "\r\n";
//             request += "User-Agent: BuildFailureDetectorESP8266\r\n";
//             request += "Accept: */*\r\n";
//             request += "Connection: close\r\n\r\n";
//             SecClient.print(request);
//             //Serial.print(request);
//   // client.print(String("GET ") + url + " HTTP/1.1\r\n" +
//   //              "Host: " + host + "\r\n" +
//   //              "User-Agent: BuildFailureDetectorESP8266\r\n" +
//   //              "Connection: close\r\n\r\n");
// /*


// > GET /sticilface/ESPmanager/fixcrashing/examples/Settingsmanager-example/data/jquery.mobile-1.4.5.min.css.gz HTTP/1.1
// > Host: raw.githubusercontent.com
// > User-Agent: curl/7.43.0
// > Accept:

// */

//            // wait up to 5 seconds for server response
//             Serial.println("Waiting for server response: ");
//             int i = 0;
//             while ((!SecClient.available()) && (i < 500)) {
//                 delay(100);
//                 i++;
//                 yield();
//                 if ( i % 10 == 0) Serial.print(".");
//             }

//            //  // return if no connection

//            // if (!client.available()) return false;

//            //  Serial.println("Download begun");
//            //  // go though header...  change this to get content length
//             // while (client.available()) {
//             //     if (client.find("\r\n\r\n")) break;
//             //     delay(100);
//             // }

//             // uint8_t i = 0;
//             //  while (client.connected()) {
//             //     String line = client.readStringUntil('\n');
//             //     Serial.print(i++);
//             //     Serial.print(" : ");
//             //     Serial.print(line);
//             //     if (line == "\r") {
//             //         Serial.println("headers received");
//             //             break;
//             //         }
//             //     }

//             yield();
//             //Serial.println("Recieved data:");
//             while (SecClient.available()) {
//                 memset(buf, 0, buf_size);;
//                 size_t length = SecClient.available();
//                 length = (length > buf_size)? buf_size: length;
//                 totalbytes += length;
//                 SecClient.read(buf, length);
//                 f.write(buf, length);
//                 delay(1);
//                 //Serial.print(buf[0],length);

//             }
//             //Serial.println("Recieve end");

//             Serial.printf("File %s %u Bytes\n", file, totalbytes);
//             SecClient.stop();
//             f.close();
//             return true;

//         } // is connected to remote host
//     } // managed to open file

// }

// bool cache ESPmanager::DownloadtoSPIFFS(const char * remotehost, const char * path, const char * file)
// {

//     const size_t buf_size = 1024;
//     uint8_t buf[buf_size];
//     WiFiClient client;
//     const int httpPort = 80;
//     size_t totalbytes = 0;

//     File f = _fs.open(file, "w");

//     if (!f) {
//         Serial.println("file open failed");
//         return false;
//     } else {
//         Serial.println("File Created");

//         delay(100);
//         Serial.printf("HOST: %s:%u\n", remotehost, httpPort);

//         if (!client.connect(remotehost, httpPort)) {
//             Serial.println("Connection failed");
//             return false;
//         } else {
//             Serial.printf("Connected to %s\n", remotehost);


//             // send GET request

//             String request = "GET " + String(path) + String(file) + " HTTP/1.0\r\n";
//             request += "Host: " + String(remotehost) + "\r\n";
//             request += "Connection: close\r\n\r\n";
//             client.print(request);

//             // wait up to 5 seconds for server response
//             Serial.println("Waiting for server response: ");
//             int i = 0;
//             while ((!client.available()) && (i < 500)) {
//                 delay(10);
//                 i++;
//                 yield();
//                 //if ( i % 10 == 0) Serial.print(".");
//             }

//             // return if no connection

//             if (!client.available()) return false;

//             Serial.println("Download begun");
//             // go though header...  change this to get content length
//             while (client.available() > 50) {
//                 if (client.find("\r\n\r\n")) break;
//                 delay(10);
//             }

//             yield();

//             while (client.available()) {
//                 memset(buf, 0, buf_size);;
//                 size_t length = (client.available() > buf_size) ? buf_size : client.available();
//                 totalbytes += length;
//                 client.readBytes(buf, length);
//                 f.write(buf, length);
//                 Serial.print(".");
//                 delay(100);

//             }


//             Serial.printf("\nFile %s %u Bytes\n", file, totalbytes);
//             client.stop();
//             f.close();
//             return true;

//         } // is connected to remote host
//     } // managed to open file

// }

bool cache ESPmanager::_upgrade()
{
    static const uint16_t httpPort = 80;
    static const size_t bufsize = 1024;
//  get files list into json

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
                    // read up to 128 byte
                    int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
                    if (len > 0) {
                        len -= c;
                    }
                }
                delay(1);
            }
            http.end();

            DynamicJsonBuffer jsonBuffer;
            JsonArray& root = jsonBuffer.parseArray( (char*)buff );

            if (!root.success()) {
                ESPMan_Debugln("Parse JSON failed");
                return false;
            } else {
                uint8_t count = 0;
                for (JsonArray::iterator it = root.begin(); it != root.end(); ++it) {

                    const char* value = *it;
                    String fullpathtodownload = String(__updateserver) + String(value);
                    String filename = fullpathtodownload.substring( fullpathtodownload.lastIndexOf("/"), fullpathtodownload.length() );
                    ESPMan_Debugf("[%u] %s -> %s\n", count++, filename.c_str(), fullpathtodownload.c_str());

                    _DownloadToSPIFFS(fullpathtodownload.c_str() , filename.c_str() );


                    // File f = _fs.open("/tempfile", "w");

                    // if (!f) {
                    //     ESPMan_Debugln("file open failed");
                    //     return false;
                    // } else {
                    //     http.begin(fullpathtodownload);
                    //     int httpCode = http.GET();
                    //     if (httpCode) {
                    //         if (httpCode == HTTP_CODE_OK) {
                    //             size_t len = http.getSize();
                    //             size_t byteswritten = http.writeToStream(&f);
                    //             ESPMan_Debugf("%s downloaded\n", formatBytes(byteswritten).c_str() ) ;
                    //             if (f.size() == len) {
                    //                 f.close();
                    //                 ESPMan_Debugln("Download Successful");
                    //                 _fs.rename("/tempfile", filename);
                    //             } else {
                    //                 ESPMan_Debugln("Download FAILED");
                    //                 _fs.remove("/tempfile");
                    //             }
                    //         } else { ESPMan_Debugf("HTTP code not correct [%d]\n", httpCode); f.close(); return false; }
                    //     } else { ESPMan_Debugf("HTTP code ERROR [%d]\n", httpCode); f.close(); return false; }
                    //     yield();
                    // }
                }
            }
        } else {
            ESPMan_Debugf("HTTP CODE [%d]", httpCode);
            http.end();
        }
    } else {
        ESPMan_Debugln("GET request Failed");
        http.end();
        return false;
    }
    return true;
}

bool cache ESPmanager::_DownloadToSPIFFS(const char * url , const char * filename )
{
    HTTPClient http;

    File f = _fs.open("/tempfile", "w");

    if (!f) {
        ESPMan_Debugln("file open failed");
        return false;
    } else {
        http.begin(url);
        int httpCode = http.GET();
        if (httpCode) {
            if (httpCode == 200) {
                size_t len = http.getSize();
                size_t byteswritten = http.writeToStream(&f);
                ESPMan_Debugf("%s downloaded\n", formatBytes(byteswritten).c_str() ) ;
                if (f.size() == len) {
                    f.close();
                    ESPMan_Debugln("Download Successful");
                    _fs.rename("/tempfile", filename);
                    return true;
                } else {
                    ESPMan_Debugln("Download FAILED");
                    _fs.remove("/tempfile");
                }
            } else { ESPMan_Debugf("HTTP code not correct [%d]\n", httpCode); }
        } else { ESPMan_Debugf("HTTP code ERROR [%d]\n", httpCode); }
        yield();
    }

    http.end();
    f.close();
    return false;
}

bool cache ESPmanager::_FilesCheck(bool startwifi)
{


// must remove.. deletes the required files..

    // for (uint8_t i = 0; i < file_no; i++) {
    //     _fs.remove(fileslist[i]);
    // }


    bool haserror = false;
    bool present[file_no];

    for (uint8_t i = 0; i < file_no; i++) {
        if (!_fs.exists(TRUEfileslist[i])) {
            present[i] = false;
            haserror = true;
            ESPMan_Debugf("ERROR %s does not exist\n", TRUEfileslist[i]);
        } else {present[i] = true; };
    }


    if (haserror ) {

        // try to start wifi
        WiFi.mode(WIFI_STA); //  == WIFI_AP

        if ( (startwifi && Wifistart() ) || WiFi.status() == WL_CONNECTED) {
            Serial.print("Connected to WiFi: ");
            Serial.println(WiFi.SSID());



            return _upgrade();

            // check everything
            // haserror = false;
            // for (uint8_t filequeue = 0; filequeue < file_no; filequeue++) {
            //     if (present[filequeue] == false) haserror = true;
            // }


        } else {

            ESPMan_Debugln(F("Attempted to download required files, failed no internet. Try hard coding credentials"));
        }

    }

    return !haserror;

}

void cache ESPmanager::InitialiseSoftAP()
{
    WiFiMode mode = WiFi.getMode();

    if (mode == WIFI_STA) {
        WiFi.mode(WIFI_AP_STA);
    }

    if (_APmac) {
        if ( wifi_set_macaddr(0x01, _APmac)) {
            ESPMan_Debugln("AP MAC applied succesfully");
        } else {
            ESPMan_Debugln("AP MAC FAILED");
        }
    }

    if (mode == WIFI_AP_STA || mode == WIFI_AP) {
        WiFi.softAP(_APssid, _APpass, _APchannel, _APhidden);
    }



}


bool cache ESPmanager::Wifistart()
{
    // WiFi.mode(WIFI_AP_STA);
    // WiFi.begin(_ssid,_pass);

    WiFi.disconnect();

    if (WiFi.getMode() == WIFI_AP) {
        ESPMan_Debugln("WiFi MODE in AP: NOT STARTING");
        return false;
    }

    ESPMan_Debugf("WiFi Mode = %u\n", WiFi.getMode());

    wl_status_t status = WiFi.status();

    ESPMan_Debugf("Pre init - WiFiStatus = %u, ssid %s, psk %s \n", status, WiFi.SSID().c_str(), WiFi.psk().c_str());


    if (!_DHCP && _IPs) {
        //     void config(IPAddress local_ip, IPAddress gateway, IPAddress subnet);
        ESPMan_Debugln(F("Using Stored IPs"));
        WiFi.config(_IPs->IP, _IPs->GW, _IPs->SN);
    }

    if (_STAmac) {
        if (wifi_set_macaddr(0x00, _STAmac)) {
            ESPMan_Debugln("STA MAC applied succesfully");
        } else {
            ESPMan_Debugln("STA MAC FAILED");
        }
    }



    WiFi.begin(); // This screws EVERYTHING up.  just leave it out!

    ESPMan_Debugln("WiFi init");
    uint8_t i = 0;
    uint32_t timeout = millis();


//  Try SDK connect first
    if (WiFi.SSID().length() > 0 && WiFi.psk().length() > 0 ) {
        ESPMan_Debugln("Length of ssid & psk > 0");
        while (status != WL_CONNECTED) {// && status != WL_NO_SSID_AVAIL && status != WL_CONNECT_FAILED) {
            delay(10);
            status = WiFi.status();
            if (millis() - timeout > 30000)  {
                ESPMan_Debugln("TIMEOUT");
                break;
            }
        }
    }

    status = WiFi.status();
    ESPMan_Debugf("Autoconnect WiFiStatus = %u \n", status);
// Try Hard coded if present


    if (status != WL_CONNECTED && _ssid_hardcoded && _pass_hardcoded) {

        ESPMan_Debug(F("Auto connect failed..\nTrying HARD CODED credentials...\n"));
        ESPMan_Debugf("Using ssid %s, psk %s \n", _ssid_hardcoded, _pass_hardcoded );

        WiFi.begin(_ssid_hardcoded, _pass_hardcoded);
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





//  Try Config_file Next
    if (status != WL_CONNECTED) {

        ESPMan_Debug(F("Auto connect failed..\nTrying stored credentials...\n"));
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


String cache ESPmanager::IPtoString(IPAddress address)
{

    String IP = "";
    for (int i = 0; i < 4; i++) {
        IP += String(address[i]);
        if (i < 3)
            IP += ".";
    }

    return IP;
}

IPAddress cache ESPmanager::StringtoIP(const String IP_string)
{

    char inputbuffer[IP_string.length() + 1];
    strcpy(inputbuffer, IP_string.c_str());
    char* IP = &inputbuffer[0];
    char* pch;
    pch = strtok(IP, ".");
    uint8_t position = 0;
    IPAddress returnIP;

    while (pch != NULL && position < 4) {
        returnIP[position++] = atoi(pch);
        pch = strtok(NULL, ".");
    }
    return returnIP;
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

void cache ESPmanager::_NewFilesCheck()
{


    for (uint8_t i = 0; i < file_no; i++) {

        if (_fs.exists(fileslist[i])) {
            String buf = "/espman";
            buf += fileslist[i];
            if (fileslist[i] == "/config.htm")  buf = "/espman/index.htm";
            if (fileslist[i] == "/ajax-loader.gif") buf = "/espman/images/ajax-loader.gif";
            if (_fs.exists(buf)) {_fs.remove(buf); };
            if (_fs.rename(fileslist[i], buf)) {
                ESPMan_Debugf("Found %s Renamed to %s\n", fileslist[i], buf.c_str());
            } else {
                ESPMan_Debugf("Failed to rename %s ==> %s\n", fileslist[i], buf.c_str());
            }
        }; //  else ESPMan_Debugf("%s : Not found\n", fileslist[i]);
    }
}

//URI Decoding function
//no check if dst buffer is big enough to receive string so
//use same size as src is a recommendation
void cache ESPmanager::urldecode(char *dst, const char *src)
{
    char a, b, c;
    if (dst == NULL) return;
    while (*src) {
        if ((*src == '%') &&
                ((a = src[1]) && (b = src[2])) &&
                (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a')
                a -= 'a' - 'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a' - 'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else {
            c = *src++;
            if (c == '+')c = ' ';
            *dst++ = c;
        }
    }
    *dst++ = '\0';
}

bool cache ESPmanager::StringtoMAC(uint8_t *mac, const String & input)
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

    if (pos == 6) return true; else return false;

}

template <class T> void ESPmanager::sendJsontoHTTP( const T & root, ESP8266WebServer & _HTTP)
{

    size_t jsonlength = root.measureLength();
    _HTTP.setContentLength(jsonlength);
    _HTTP.send(200, "text/json" );
    BufferedPrint<HTTP_DOWNLOAD_UNIT_SIZE> proxy(_HTTP);
    root.printTo(proxy);
    proxy.flush();
    proxy.stop();

}

void ESPmanager::handleFileUpload()
{
    if (_HTTP.uri() != "/espman/upload") return;

    static File * fsUploadFile;
    HTTPUpload& upload = _HTTP.upload();

    if (upload.status == UPLOAD_FILE_START) {
        if (fsUploadFile) {
            delete fsUploadFile;
            fsUploadFile = nullptr;
        };
        fsUploadFile = new File;
        String filename = upload.filename;
        filename.trim();
        if (!filename.startsWith("/")) filename = "/" + filename;
        // ESPMan_Debug("handleFileUpload Name: "); ESPMan_Debugln(filename);
        Serial.printf("Upload Name: %s\n", filename.c_str() );
        if (_fs.exists(filename)) _fs.remove(filename);

        *fsUploadFile = _fs.open(filename, "w+");

        filename = String();
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (*fsUploadFile) {
            fsUploadFile->write(upload.buf, upload.currentSize);
            ESPMan_Debug(".");
        };
    } else if (upload.status == UPLOAD_FILE_END) {
        fsUploadFile->close();
        if (fsUploadFile) {
            delete fsUploadFile;
            fsUploadFile = nullptr;
            Serial.printf("\nDone Size: %u\n", upload.totalSize);
        } else { ESPMan_Debug("ERROR"); };
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Serial.printf("\nAborted");
        String filename = String(fsUploadFile->name());
        fsUploadFile->close();
        if (fsUploadFile) {
            delete fsUploadFile;
            fsUploadFile = nullptr;
        }
        if (_fs.exists(filename)) {
            _fs.remove(filename);
        }
    }
}


void cache ESPmanager::HandleDataRequest()
{

    String buf;
    uint8_t _wifinetworksfound = 0;

    String args = F("ARG: %u, \"%s\" = \"%s\"\n");
    for (uint8_t i = 0; i < _HTTP.args(); i++) {
        ESPMan_Debugf(args.c_str(), i, _HTTP.argName(i).c_str(), _HTTP.arg(i).c_str());
    }

    /*------------------------------------------------------------------------------------------------------------------
                                                                    Reboot command
    ------------------------------------------------------------------------------------------------------------------*/

    if (_HTTP.arg("plain") == "reboot") {
        ESPMan_Debugln(F("Rebooting..."));
        _HTTP.send(200, "text", "OK"); // return ok to speed up AJAX stuff
        ESP.restart();
    };

    /*------------------------------------------------------------------------------------------------------------------
                                      WiFi Scanning and Sending of WiFi networks found at boot
    ------------------------------------------------------------------------------------------------------------------*/
    if (_HTTP.arg("plain") == F("WiFiDetails") || _HTTP.arg("plain") == F("PerformWiFiScan")) {



        if (_HTTP.arg("plain") == F("PerformWiFiScan")) {
            ESPMan_Debug(F("Performing Wifi Network Scan..."));
            _wifinetworksfound = WiFi.scanNetworks();
            ESPMan_Debugln(F("done"));
        }

        const int BUFFER_SIZE = JSON_OBJECT_SIZE( _wifinetworksfound * 6) + JSON_ARRAY_SIZE(_wifinetworksfound) + JSON_OBJECT_SIZE(22);

        DynamicJsonBuffer jsonBuffer(BUFFER_SIZE);
        JsonObject& root = jsonBuffer.createObject();

        if (_wifinetworksfound) {

            JsonArray& Networkarray = root.createNestedArray("networks");

            for (int i = 0; i < _wifinetworksfound; ++i) {
                JsonObject& ssidobject = Networkarray.createNestedObject();

                bool connectedbool
                    = (WiFi.status() == WL_CONNECTED && WiFi.SSID(i) == WiFi.SSID()) ? true : false;
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
        }

        WiFiMode mode = WiFi.getMode();

        JsonObject& generalobject = root.createNestedObject("general");

        generalobject[F("deviceid")] = _host;
        generalobject[F("OTAenabled")] = (_OTAenabled) ? true : false;
        generalobject[F("OTApassword")] = (_OTApassword) ? _OTApassword : C_null;

        generalobject[F("APrestartmode")] = _APrestartmode;
        //generalobject[F("OTAusechipID")] = _OTAusechipID;
        generalobject[F("mDNSenabled")] = (_mDNSenabled) ? true : false;

        JsonObject& STAobject = root.createNestedObject("STA");

        STAobject[F("connectedssid")] = WiFi.SSID();

        STAobject[F("dhcp")] = (_DHCP) ? true : false;
        STAobject[F("state")] = (mode == WIFI_STA || mode == WIFI_AP_STA) ? true : false;
        STAobject[F("IP")] = IPtoString(WiFi.localIP());
        STAobject[F("gateway")] = IPtoString(WiFi.gatewayIP());
        STAobject[F("subnet")] = IPtoString(WiFi.subnetMask());
        STAobject[F("MAC")] = WiFi.macAddress();

        JsonObject& APobject = root.createNestedObject("AP");

        APobject[F("ssid")] = _APssid;
        APobject[F("state")] = (mode == WIFI_AP || mode == WIFI_AP_STA) ? true : false;
        APobject[F("APenabled")] = _APenabled;

        APobject[F("IP")] = (WiFi.softAPIP()[0] == 0 && WiFi.softAPIP()[1] == 0
                             && WiFi.softAPIP()[2] == 0 && WiFi.softAPIP()[3] == 0)
                            ? F("192.168.4.1")
                            : IPtoString(WiFi.softAPIP());
        APobject[F("hidden")] = (_APhidden) ? true : false;
        APobject[F("password")] = (_APpass) ? _APpass : "";
        APobject[F("channel")] = _APchannel;
        APobject[F("MAC")] = WiFi.softAPmacAddress();


        sendJsontoHTTP(root, _HTTP);

        WiFi.scanDelete();
        _wifinetworksfound = 0;


        return;
    }

    /*------------------------------------------------------------------------------------------------------------------
                                      Send about page details...
    ------------------------------------------------------------------------------------------------------------------*/
    if (_HTTP.arg("plain") == "AboutPage") {

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
        root[F("flashchipmode_var")] = ESP.getFlashChipMode();

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

        sendJsontoHTTP(root, _HTTP);
        return;
    }

    /*------------------------------------------------------------------------------------------------------------------
                                      SSID handles...
    ------------------------------------------------------------------------------------------------------------------*/

    static int8_t WiFiresult = -1;

    if (_HTTP.hasArg("ssid") && _HTTP.hasArg("pass")) {
        if (_HTTP.arg("ssid").length() > 0) {
            // _HTTP.arg("pass").length() > 0) {  0 length passwords should be ok.. for open
            // networks.
            if (_HTTP.arg("ssid").length() < 33 && _HTTP.arg("pass").length() < 33) {
                if (_HTTP.arg("ssid") != WiFi.SSID() || _HTTP.arg("pass") != WiFi.psk()) {

                    if (WiFi.getMode() == WIFI_AP) { WiFi.mode(WIFI_AP_STA);  }; // if WiFi STA off, turn it on..


                    save_flag = true;
                    WiFiresult = -1;
                    char old_ssid[33];
                    char old_pass[33];
                    strcpy(old_pass, (const char*)WiFi.psk().c_str());
                    strcpy(old_ssid, (const char*)WiFi.SSID().c_str());

                    _HTTP.send(200, "text",
                               "accepted"); // important as it defines entry to the wait loop on client

                    ESPMan_Debug(F("Disconnecting.."));

                    WiFi.disconnect();
                    ESPMan_Debug(F("done \nInit..."));


                    //     _ssid = strdup((const char *)_HTTP.arg("ssid").c_str());
                    //     _pass = strdup((const char *)_HTTP.arg("pass").c_str());

                    /*

                            ToDo....  put in check for OPEN networks that enables the AP for 5
                       mins....

                    */

                    //  First try does not change the vars
                    WiFi.begin((const char*)_HTTP.arg("ssid").c_str(),
                               (const char*)_HTTP.arg("pass").c_str());

                    uint8_t i = 0;

                    while (WiFi.status() != WL_CONNECTED && _HTTP.arg("removesaftey") == "No") {
                        WiFiresult = 0;
                        delay(500);
                        i++;
                        ESPMan_Debug(".");
                        if (i == 30) {

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
                        }
                    }

                    ESPMan_Debugf("\nconnected: SSID = %s, pass = %s\n", WiFi.SSID().c_str(),

                                  WiFi.psk().c_str());
                    if (WiFiresult == 0)
                        WiFiresult = 1; // not sure why i did this.. think it is the client end.
                    if (WiFiresult) {

                        if (_ssid) {
                            free((void*)_ssid);
                            _ssid = NULL;
                        };
                        if (_pass) {
                            free((void*)_pass);
                            _pass = NULL;
                        };
                        _ssid = strdup((const char*)_HTTP.arg("ssid").c_str());
                        _pass = strdup((const char*)_HTTP.arg("pass").c_str());
                        ESPMan_Debugln(F("New SSID and PASS work:  copied to system vars"));
                    }
                    // return;
                }
            }
        }
    }

    //  This is outside the loop...  wifiresult is a static to return previous result...
    if (_HTTP.arg("plain") == "WiFiresult") {
        _HTTP.send(200, "text", String(WiFiresult));
        return;
    }
    /*------------------------------------------------------------------------------------------------------------------

                                     STA config
    ------------------------------------------------------------------------------------------------------------------*/

    if (_HTTP.hasArg("enable-STA")) {
        save_flag = true;

        // IPAddress localIP, gateway, subnet;
        WiFiMode mode = WiFi.getMode();
        bool reinit = false;

        if (_HTTP.arg("enable-STA") == "off" && (mode == WIFI_STA || mode == WIFI_AP_STA)) {
            WiFi.mode(WIFI_AP); // always fall back to AP mode...
            WiFi.softAP(_APssid, _APpass, (int)_APchannel, (int)_APhidden);
            ESPMan_Debugln(F("STA-disabled: falling back to AP mode."));
        } else if (_HTTP.arg("enable-STA") == "on" && mode == WIFI_AP) {
            WiFi.mode(WIFI_AP_STA);
            ESPMan_Debugln(F("Enabling STA mode"));
            reinit = true;
        }


        if (_HTTP.arg("enable-dhcp") == "on") {

            _DHCP = true;
            save_flag = true;
            bool dhcpresult = wifi_station_dhcpc_start(); // requires userinterface...

            ESPMan_Debugln(F("------- RESARTING--------"));
            ESPMan_Debug(F("DHCP result: "));
            ESPMan_Debugln(dhcpresult);
            reinit = true;
        } else if (_HTTP.arg("enable-dhcp") == "off") {

            save_flag = true;
            _DHCP = false;

            if (!_IPs)
                _IPs = new IPconfigs_t; // create memory for new IPs


            bool ok = true;

            if (_HTTP.hasArg("setSTAsetip")) {
                _IPs->IP = StringtoIP(_HTTP.arg("setSTAsetip"));
                ESPMan_Debug(F("IP = "));
                ESPMan_Debugln(_IPs->IP);
            } else
                ok = false;

            if (_HTTP.hasArg("setSTAsetgw")) {
                _IPs->GW = StringtoIP(_HTTP.arg("setSTAsetgw"));
                ESPMan_Debug(F("gateway = "));
                ESPMan_Debugln(_IPs->GW);
            } else
                ok = false;

            if (_HTTP.hasArg("setSTAsetsn")) {
                _IPs->SN = StringtoIP(_HTTP.arg("setSTAsetsn"));
                ESPMan_Debug(F("subnet = "));
                ESPMan_Debugln(_IPs->SN);
            } else
                ok = false;

            if (ok) {
                // WiFi.config(localIP, gateway, subnet);
                reinit = true;
            }

            if (_HTTP.hasArg("setSTAsetmac") && _HTTP.arg("setSTAsetmac").length() != 0) {

                uint8_t mac_addr[6];

                if ( StringtoMAC(mac_addr, _HTTP.arg("setSTAsetmac")) ) {


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

        if (reinit && _HTTP.arg("enable-STA") == "on")
            Wifistart();
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

    if (_HTTP.hasArg("enable-AP")) {
        save_flag = true;

        WiFiMode mode = WiFi.getMode();
        // WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3
        // if (mode == WIFI_STA || mode == WIFI_AP_STA ) STAstate = "ENABLED"; else STAstate =
        // "DISABLED";
        // if (mode == WIFI_AP || mode == WIFI_AP_STA ) APstate = "ENABLED"; else APstate =
        // "DISABLED";

        if (_HTTP.arg("setAPsetssid").length() != 0) {

            if (_APssid) {
                free((void*)_APssid);
                _APssid = NULL;
            }
            _APssid = strdup((const char*)_HTTP.arg("setAPsetssid").c_str());
        };

        if (_HTTP.arg("setAPsetpass").length() != 0 && _HTTP.arg("setAPsetpass").length() < 63) {
            //_APpass = (const char *)_HTTP.arg("setAPsetpass").c_str();

            if (_APpass) {
                free((void*)_APpass);
                _APpass = NULL;
            }; // free memory if AP pass has been allocated.

            _APpass = strdup((const char*)_HTTP.arg("setAPsetpass").c_str());
        } else if (_HTTP.arg("setAPsetpass").length() == 0 && _APpass) {

            if (_APpass) {
                free((void*)_APpass);
                _APpass = NULL;
            }; // free memory if AP pass has been allocated.
        };

        if (_HTTP.hasArg("setAPsetmac") && _HTTP.arg("setAPsetmac").length() != 0) {

            uint8_t mac_addr[6];

            if ( StringtoMAC(mac_addr, _HTTP.arg("setAPsetmac")) ) {


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


        if (_HTTP.arg("enable-AP") == "on") {

            uint8_t channel = _HTTP.arg("setAPsetchannel").toInt();

            if (channel > 13)
                channel = 13;
            _APchannel = channel;

            ESPMan_Debug(F("Enable AP channel: "));
            ESPMan_Debugln(_APchannel);

            _APenabled = true;

            InitialiseSoftAP();

            //printdiagnositics();
        } else if (_HTTP.arg("enable-AP") == "off") {

            ESPMan_Debugln(F("Disable AP"));
            _APenabled = false;

            if (mode == WIFI_AP_STA || mode == WIFI_AP)  WiFi.mode(WIFI_STA);

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

    if (_HTTP.hasArg("deviceid")) {
        if (_HTTP.arg("deviceid").length() != 0 &&
                _HTTP.arg("deviceid").length() < 32 &&
                _HTTP.arg("deviceid") != String(_host) ) {
            save_flag = true;
            ESPMan_Debugln(F("Device ID changed"));
            //      if (_host) free( (void*)_host);

            if (strcmp(_host, _APssid) == 0) {
                if (_APssid) {
                    free((void*)_APssid);
                    _APssid = NULL;
                }
                _APssid = strdup((const char*)_HTTP.arg("deviceid").c_str());
            }

            if (_host) {
                free((void*)_host);
                _host = NULL;
            }
            _host = strdup((const char*)_HTTP.arg("deviceid").c_str());
            //  might need to add in here, wifireinting...
            WiFi.hostname(_host);
            InitialiseFeatures();
            InitialiseSoftAP();
        }
    }
    /*------------------------------------------------------------------------------------------------------------------

                                     Restart wifi
    ------------------------------------------------------------------------------------------------------------------*/

    if (_HTTP.arg("plain") == "resetwifi") {
        ESP.eraseConfig(); // not sure if this is needed...

        WiFi.disconnect();
        ESP.restart();
    }

    /*------------------------------------------------------------------------------------------------------------------

                                     OTA config
    ------------------------------------------------------------------------------------------------------------------*/
    // if (_HTTP.hasArg("OTAusechipID")) {

    //     _OTAusechipID = (_HTTP.arg("OTAusechipID") == "Yes")? true : false;
    //     ESPMan_Debugln(F("OTA append ChipID to host"));
    //     save_flag = true;
    // }

    if (_HTTP.hasArg("otaenable")) {
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

    if (_HTTP.hasArg("mdnsenable")) {
        save_flag = true;

        bool command = (_HTTP.arg("mdnsenable") == "on") ? true : false;

        if (command != _mDNSenabled) {
            _mDNSenabled = command;
            InitialiseFeatures();
        }
    } // end of OTA enable
    /*------------------------------------------------------------------------------------------------------------------

                                       FORMAT SPIFFS
      ------------------------------------------------------------------------------------------------------------------*/
    if (_HTTP.arg("plain") == "formatSPIFFS" & _HTTP.method() == HTTP_POST) {
        ESPMan_Debug(F("Format SPIFFS"));
        _fs.format();
        ESPMan_Debugln(F(" done"));
    }

    if (_HTTP.arg("plain") == "upgrade" & _HTTP.method() == HTTP_POST) {
        static uint32_t timeout = 0;

        if (millis() - timeout > 30000 || timeout == 0 ) {
            Serial.print("Upgrade Started..");
            if (_upgrade()) {
                _HTTP.send(200, "SUCCESS");
                Serial.println("Download Finished.  Files Updated");
                _NewFilesCheck();
            } else {
                _HTTP.send(200, "FAILED");
                Serial.println("Error.  Try Again");
            }
            timeout = millis();
            return;

        }
        // ESPMan_Debug(F("Upgrade files"));

        //     Dir dir = _fs.openDir("/");
        //      while (dir.next()) {
        //         String fileName = dir.fileName();
        //             size_t fileSize = dir.fileSize();
        //             ESPMan_Debugf("     Deleting: %s\n", fileName.c_str());
        //             _fs.remove(fileName);
        //         }
        // ESPMan_Debugln(F(" done, rebooting"));
        // _HTTP.send(200, "text", "OK"); // return ok to speed up AJAX stuff
        // ESP.restart();
        //FilesCheck(false);
    }

    if (_HTTP.arg("plain") == "deletesettings" & _HTTP.method() == HTTP_POST) {

        ESPMan_Debug(F("Delete Settings File"));
        if (_fs.remove("/settings.txt")) {
            ESPMan_Debugln(F(" done"));
        } else {
            ESPMan_Debugln(F(" failed"));
        }
    }

    /*------------------------------------------------------------------------------------------------------------------

                                       MAC address STA + AP
      ------------------------------------------------------------------------------------------------------------------*/





    /*------------------------------------------------------------------------------------------------------------------

                                       AP reboot behaviour
      ------------------------------------------------------------------------------------------------------------------*/

    if (_HTTP.hasArg("select-AP-behaviour") ) {
        ESPMan_Debugln("Recieved AP behaviour request");
        int rebootvar = _HTTP.arg("select-AP-behaviour").toInt();

        if (rebootvar == 1 || rebootvar == 2 || rebootvar == 3 || rebootvar == 4 ) {
            _APrestartmode = rebootvar;
            save_flag = true;
        }

    }




    _HTTP.send(200, "text", "OK"); // return ok to speed up AJAX stuff
}














