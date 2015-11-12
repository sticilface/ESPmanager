
#include "Settingsmanager.h"

extern "C" {
#include "user_interface.h"
}


Settingsmanager::Settingsmanager(
    ESP8266WebServer* HTTP, fs::FS* fs, const char* host, const char* ssid, const char* pass)
{

    _HTTP = HTTP;
    _fs = fs;
    // This sets the default fallback options... not in use yet
    if (host && (strlen(host) < 32))
    {
        _host = strdup(host);
    }
    if (ssid && (strlen(ssid) < 32))
    {
        _ssid = strdup(ssid);
    }
    if (pass && (strlen(pass) < 63))
    {
        _pass = strdup(pass);
    }
}

Settingsmanager::~Settingsmanager()
{

    // if (ota_server)
    // {
    //     delete ota_server;
    //     ota_server = NULL;
    // };

    if (_host)
    {
        free((void*)_host);
        _host = NULL;
    };
    if (_pass)
    {
        free((void*)_pass);
        _pass = NULL;
    };
    if (_ssid)
    {
        free((void*)_ssid);
        _ssid = NULL;
    };
    if (_APpass)
    {
        free((void*)_APpass);
        _APpass = NULL;
    };
    if (_APssid)
    {
        free((void*)_APssid);
        _APssid = NULL;
    };
    if (_IPs)
    {
        delete _IPs;
        _IPs = NULL;
    };
}

void cache Settingsmanager::begin()
{

    Debugln("Settings Manager V1.0");

    if (SPIFFS.begin())
    {
        Debugln(F("File System mounted sucessfully"));
       
        // if (!FilesCheck()) {
        //     Debugln(F("Major FAIL, required files are NOT in SPIFFS, please upload required files")); 
        //     return; 
        // }

        NewFileCheck();

        LoadSettings();
    }
    else
    {
        Debugln(F("File System mount failed"));
    }


    if (_host)
    {
        if (WiFi.hostname(_host))
        {
            Debug("Host Name Set: ");
            Debugln(_host);
        }
    }
    else
    {
        _host = strdup((const char*)WiFi.hostname().c_str());
    }

    if (!_APssid)
    {
        _APssid = strdup(wifi_station_get_hostname());
    }


    if (!Wifistart())
    {
        WiFiMode(WIFI_AP_STA);
        WiFi.softAP(_APssid, _APpass, (int)_APchannel, (int)_APhidden);
        Debugln(F("WiFi Failed:  Starting AP"));

        //  to add timeout on AP...
    }

    if (WiFi.status() == WL_CONNECTED ) {
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP()); 
    }
    // printdiagnositics();

    InitialiseFeatures();

    _HTTP->on("/espman/data.esp", std::bind(&Settingsmanager::HandleDataRequest, this));


    _HTTP->serveStatic("/espman", *_fs, "/espman","max-age=86400"); // allows all files on SPIFFS to be served to webserver... might wish to change this

  
    // _HTTP->serveStatic("/config.htm", *_fs, "/config.htm","86400");
    // _HTTP->serveStatic("/images/ajax-loader.gif", *_fs, "/ajax-loader.gif", "86400"); // temp fix

    // _HTTP->serveStatic("/jquery-1.11.1.min.js", *_fs, "/jquery-1.11.1.min.js","86400"); 
    // _HTTP->serveStatic("/jquery.mobile-1.4.5.min.css", *_fs, "/jquery.mobile-1.4.5.min.css","86400"); 
    // _HTTP->serveStatic("/jquery.mobile-1.4.5.min.js", *_fs, "/jquery.mobile-1.4.5.min.js","86400");

    // _HTTP->serveStatic("/configjava.js", *_fs, "/configjava.js");


    //_HTTP->serveStatic("/index.htm", *_fs, "/index.htm");


    // SPIFFS.rename(jq1, "/espman/jq1.11.1.js.gz" ); 
    // SPIFFS.rename(jq2, "/espman/jqm1.4.5.css.gz" ); 
    // SPIFFS.rename(jq3, "/espman/jqm1.4.5.js.gz" ); 
    // SPIFFS.rename(jq4, "/espman/configjava.js" ); 
    // SPIFFS.rename(htm1,"/espman/index.htm" ); 



}

void cache Settingsmanager::LoadSettings()
{
    // Debug("OLD ----- ");

    // PrintVariables();

    DynamicJsonBuffer jsonBuffer;
    File f = SPIFFS.open(SETTINGS_FILE, "r");
    if (!f)
    {
        Debugln(F("Settings file open failed!"));
        return;
    }

    f.seek(0, SeekSet);
    char data[f.size()];
    for (int i = 0; i < f.size(); i++)
    {
        data[i] = f.read();
    }

    f.close();
    JsonObject& root = jsonBuffer.parseObject(data);

    if (!root.success())
    {
        Debugln(F("Parsing settings file Failed!"));
        return;
    }
    // root.prettyPrintTo(Serial);

    if (root.containsKey("host"))
    {
        const char* host = root[F("host")];
        if (_host)
        {
            free((void*)_host);
            _host = NULL;
        };
        if(host) { _host = strdup(host); } else { _host = C_null ; } ; 
    }

    if (root.containsKey("ssid"))
    {
        const char* ssid = root["ssid"];
        if (_ssid)
        {
            free((void*)_ssid);
            _ssid = NULL;
        };
        if (ssid) { _ssid = strdup(ssid); } else { _ssid = C_null ; } ; 
    }

    if (root.containsKey("pass"))
    {
        const char* pass = root["pass"];
        if (_pass)
        {
            free((void*)_pass);
            _pass = NULL;
        };
        if (pass) { _pass = strdup(pass); } else { _pass = C_null; } ; 
    }

    if (root.containsKey("APpass"))
    {
        const char* APpass = root["APpass"];
        if (_APpass)
        {
            free((void*)_APpass);
            _APpass = NULL;
        };
        if (APpass) { _APpass = strdup(APpass); } else { _APpass = C_null; } ; 
    }

    if (root.containsKey("APpass"))
    {
        const char* APssid = root["APssid"];
        if (_APssid)
        {
            free((void*)_APssid);
            _APssid = NULL;
        };
        if (APssid) { _APssid = strdup(APssid); } else {_APssid = C_null; } ;
    }

    if (root.containsKey("APpass"))
    {
        long APchannel = root["APchannel"];
        if (APchannel < 13 && APchannel > 0)
            _APchannel = (uint8_t)APchannel;
    }

    if (root.containsKey("DHCP"))
    {
        _DHCP = root["DHCP"];
        //_DHCP = (strcmp(dhcp, "true") == 0) ? true : false;
    }

    if (root.containsKey("APenabled"))
    {
        _APenabled = root[F("APenabled")];
        //_APenabled = (strcmp(APenabled, "true") == 0) ? true : false;
    }

    if (root.containsKey("APhidden"))
    {
        _APhidden = root["APhidden"];
        //_APhidden = (strcmp(APhidden, "true") == 0) ? true : false;
    }

    if (root.containsKey("OTAenabled"))
    {
        _OTAenabled = root["OTAenabled"];
        //_OTAenabled = (strcmp(OTAenabled, "true") == 0) ? true : false;
    }

    if (root.containsKey("mdnsenable"))
    {
        _mDNSenabled = root["mDNSenable"];
        //_mDNSenabled = (strcmp(mDNSenabled, "true") == 0) ? true : false;
    }

    if (root.containsKey("wifimanage"))
    {
         _manageWiFi = root["WiFimanage"];
        //_manageWiFi = (strcmp(manageWiFi, "true") == 0) ? true : false;
    }


    if (root.containsKey("IPaddress") && root.containsKey("Gateway") && root.containsKey("Subnet"))
    {
        const char* ip = root[F("IPaddress")];
        const char* gw = root[F("Gateway")];
        const char* sn = root[F("Subnet")];

        if (!_DHCP)
        { // only bother to allocate memory if dhcp is NOT being used.
            if (_IPs)
            {
                delete _IPs;
                _IPs = NULL;
            };
            _IPs = new IPconfigs_t;
            _IPs->IP = StringtoIP(String(ip));
            _IPs->GW = StringtoIP(String(gw));
            _IPs->SN = StringtoIP(String(sn));
        }
    }
    Debugln(F("----- Saved Variables -----"));
    PrintVariables();
    Debugln(F("---------------------------"));
}


void cache Settingsmanager::PrintVariables()
{

#ifdef DEBUG_YES
    Debugln(F("VARIABLE STATES: "));
    Debugf("_host = %s\n", _host);
    Debugf("_ssid = %s\n", _ssid);
    Debugf("_pass = %s\n", _pass);
    Debugf("_APpass = %s\n", _APpass);
    Debugf("_APssid = %s\n", _APssid);
    Debugf("_APchannel = %u\n", _APchannel);
    (_DHCP) ? Debugln(F("_DHCP = true")) : Debugln(F("_DHCP = false"));
    (_APenabled) ? Debugln(F("_APenabled = true")) : Debugln(F("_APenabled = false"));
    (_APhidden) ? Debugln(F("_APhidden = true")) : Debugln(F("_APhidden = false"));
    (_OTAenabled) ? Debugln(F("_OTAenabled = true")) : Debugln(F("_OTAenabled = false"));

    if (_IPs)
    {
        Debug(F("IPs->IP = "));
        Debugln(IPtoString(_IPs->IP));
        Debug(F("IPs->GW = "));
        Debugln(IPtoString(_IPs->GW));
        Debug(F("IPs->SN = "));
        Debugln(IPtoString(_IPs->SN));
    }
    else
        Debugln(F("NO IPs held in memory"));
#endif
}

void cache Settingsmanager::SaveSettings()
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
    root[F("APpass")] = (_APpass) ? _APpass : C_null;
    root[F("APssid")] = (_APssid) ? _APssid : C_null;
    root[F("DHCP")] = (_DHCP) ? true : false;
    root[F("APchannel")] = (_APchannel) ? true : false;
    root[F("APenabled")] = (_APenabled) ? true : false;
    root[F("APhidden")] = (_APhidden) ? true : false;
    root[F("OTAenabled")] = (_OTAenabled) ? true : false;
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


    // Debugf("IP = %s, GW = %s, SN = %s\n", IP, GW, SN);
    File f = SPIFFS.open(SETTINGS_FILE, "w");
    if (!f)
    {
        Debugln(F("Settings file save failed!"));
        return;
    }

    root.prettyPrintTo(f);
    f.close();

    // Debugf("Save done. %ums\n", millis() - starttime);
}

void cache Settingsmanager::handle()
{

    // if (ota_server)
    //     ota_server->handle();

      ArduinoOTA.handle();


    if (save_flag)
    {
        SaveSettings();
        save_flag = false;
    }

    //
    //
    // Async handle WiFi reconnects...
    //
    //
}

void cache Settingsmanager::InitialiseFeatures()
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

        char OTAhost[33];
        strcpy(OTAhost, _host);
        OTAhost[strlen(_host)] = '-';
        OTAhost[strlen(_host) + 1] = 0;
        char tmp[15];
        sprintf(tmp, "%02x", ESP.getChipId());
        strcat(OTAhost, tmp); 

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
   ArduinoOTA.setHostname(OTAhost);

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");


        ArduinoOTA.onStart([]() {
            Serial.println("OTA Start");
        });
        ArduinoOTA.onEnd([]() {
            Serial.println("OTA End");
        });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("OTA Progress: %u%%\n", (progress / (total / 100)));
        });
        ArduinoOTA.onError([](ota_error_t error) {
            Serial.printf("OTA Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
            else if (error == OTA_RECIEVE_ERROR) Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR) Serial.println("End Failed");
        });
        
        ArduinoOTA.begin();


// not sure this is needed now. 

    // if (_mDNSenabled)
    // {
    //     if (!_OTAenabled)
    //     {
    //         MDNS.begin(_host);
    //     }
    //     MDNS.addService("http", "tcp", 80);
    // }

    WiFi.hostname(_host);
}

// bool cache Settingsmanager::HTTPSDownloadtoSPIFFS(const char * remotehost, const char * fingerprint, const char * path, const char * file) {
            
//     const size_t buf_size = 1024;
//     uint8_t buf[buf_size]; 
//     const int httpsPort = 443;
//     //WiFiClientSecure SecClient;

//     size_t totalbytes = 0; 
        
//     File f = SPIFFS.open(file, "w");

//     if (!f) {
//         Serial.println("file open failed");
//         return false; 
//     } else { 
//         Serial.println("File Created");
//         delay(100);
//         Serial.printf("HOST: %s:%u\n", remotehost, httpsPort);

//         if (!SecClient->connect(remotehost, httpsPort)) {
//             Serial.println("Connection failed");
//             return false;
//         } else {
//             Serial.printf("Connected to %s\n", remotehost); 

//             if (SecClient->verify(fingerprint, remotehost)) {
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
//             SecClient->print(request);
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
//             while ((!SecClient->connected()) && (i < 500)) {
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
//             while (SecClient->available()) {
//                 memset(buf, 0, buf_size);;
//                 size_t length = SecClient->available(); 
//                 length = (length > buf_size)? buf_size: length;  
//                 totalbytes += length; 
//                 SecClient->readBytes(buf, length);
//                 delay(20);                
//                 f.write(buf, length);  
//                 delay(20);
//                 Serial.print(buf[0],length);
    
//             }
//             //Serial.println("Recieve end");

//             Serial.printf("File %s %u Bytes\n", file, totalbytes);
//             SecClient->stop(); 
//             f.close();
//             return true; 

//         } // is connected to remote host
//     } // managed to open file

// }

bool cache Settingsmanager::DownloadtoSPIFFS(const char * remotehost, const char * path, const char * file) {
            
            const size_t buf_size = 1024;
            uint8_t buf[buf_size]; 
            WiFiClient client;
            const int httpPort = 80;
            size_t totalbytes = 0; 
        
  File f = SPIFFS.open(file, "w");

    if (!f) {
        Serial.println("file open failed");
        return false; 
    } else { 
        Serial.println("File Created");
        
        delay(100);
        Serial.printf("HOST: %s:%u\n", remotehost, httpPort);

        if (!client.connect(remotehost, httpPort)) {
            Serial.println("Connection failed");
            return false;
        } else {
            Serial.printf("Connected to %s\n", remotehost); 


            // send GET request

            String request = "GET " + String(path) + String(file) + " HTTP/1.0\r\n"; 
            request += "Host: " + String(remotehost) + "\r\n";
            request += "Connection: close\r\n\r\n";
            client.print(request);

            // wait up to 5 seconds for server response
            Serial.println("Waiting for server response: ");
            int i = 0;
            while ((!client.available()) && (i < 500)) {
                delay(10);
                i++;
                yield(); 
                //if ( i % 10 == 0) Serial.print(".");
            }

            // return if no connection

           if (!client.available()) return false; 

            Serial.println("Download begun"); 
            // go though header...  change this to get content length
            while (client.available() > 50) { 
                if (client.find("\r\n\r\n")) break;
                delay(10);
            }


    
            yield();

            while (client.available()) {
                memset(buf, 0, buf_size);;                
                size_t length = (client.available() > buf_size)? buf_size: client.available();  
                totalbytes += length; 
                client.readBytes(buf, length);
                f.write(buf, length);  
                Serial.print("."); 
                delay(100);
    
            }


            Serial.printf("\nFile %s %u Bytes\n", file, totalbytes);
            client.stop(); 
            f.close();
            return true; 

        } // is connected to remote host
    } // managed to open file

}

bool cache Settingsmanager::FilesCheck(bool startwifi) {

    //  http://raw.githubusercontent.com/sticilface/ESPmanager/fixcrashing/examples/Settingsmanager-example/data/jquery.mobile-1.4.5.min.js.gz


 const char * remotehost = "192.168.1.115"; 
 const char * path = "";

 const char * remotehost_git = "raw.githubusercontent.com"; 
 const char * path_git = "/sticilface/ESPmanager/fixcrashing/examples/Settingsmanager-example/data";
 const char * raw_github_fingerprint = "B0 74 BB EF 10 C2 DD 70 89 C8 EA 58 A2 F9 E1 41 00 D3 38 82";


    //const char * file = "jquery.mobile-1.4.5.min.js.gz"; 
    //const char * file = htm1; 

    for (uint8_t i = 0; i < file_no; i++) {
        SPIFFS.remove(items[i]); 
    }


        bool haserror = false; 
        bool present[file_no];

    for (uint8_t i = 0; i < file_no; i++) {

        if (!SPIFFS.exists(items[i])) {
            present[i] = false; 
            haserror = true; 
            Debugf("ERROR %s does not exist\n", items[i]); 
        } else {present[i] = true; };
    }


    if (haserror ) {

        // try to start wifi
        WiFi.mode(WIFI_STA); //  == WIFI_AP

        if ( (startwifi && Wifistart() ) || WiFi.status() == WL_CONNECTED) {
            Serial.print("Connected to WiFi: ");
            Serial.println(WiFi.SSID());



//    SecClient = new WiFiClientSecure; 


  for (uint8_t filequeue = 0; filequeue < file_no; filequeue++) {
    
    //delay(1000);
    Serial.println("===================== START ==================="); 
        
        const char * current_file = items[filequeue]; 

        if (DownloadtoSPIFFS(remotehost, path, current_file)) { 
        //if (HTTPSDownloadtoSPIFFS(remotehost_git, raw_github_fingerprint, path_git, current_file)) {
            Serial.printf("%s has been downloaded\n",current_file);
            present[filequeue] = true;
      } else {
        Serial.printf("%s FAILED to download\n", current_file);
      }


    } // file loop.. 
    
    // check everything
    haserror = false; 
      for (uint8_t filequeue = 0; filequeue < file_no; filequeue++) {
        if (present[filequeue] == false) haserror = true; 
      }


    } else {
        
        Serial.println(F("Attempted to download required files, failed no internet. Try hard coding credentials")); 
    } 

    }
    
    return !haserror; 

}

void cache Settingsmanager::InitialiseSoftAP()
{

    WiFiMode mode = WiFi.getMode();
    if (mode == WIFI_AP_STA || mode == WIFI_AP)
    {
        WiFi.softAP(_APssid, _APpass, _APchannel, _APhidden);
    }
}

bool cache Settingsmanager::Wifistart()
{
    // WiFiMode(WIFI_AP_STA);
    // WiFi.begin(_ssid,_pass);

    if (WiFi.getMode() == WIFI_AP)
        return false;

    WiFi.begin();

    if (!_DHCP && _IPs)
    {
        //     void config(IPAddress local_ip, IPAddress gateway, IPAddress subnet);
        Debugln(F("Using Stored IPs"));
        WiFi.config(_IPs->IP, _IPs->GW, _IPs->SN);
    }

    Debug("WiFi init");
    uint8_t i = 0;
    while (WiFi.status() != WL_CONNECTED)
    {

        delay(500);
        i++;
        Debug(".");
        if (i == 30)
        {
            Debug(F("Auto connect failed..\nTrying stored credentials..."));
            WiFi.begin(_ssid, _pass);
            while (WiFi.status() != WL_CONNECTED)
            {
                delay(500);
                i++;
                Debug(".");
                if (i == 60)
                {
                    Debug(F("Failed... setting up AP"));
                    WiFi.mode(WIFI_AP_STA);
                    WiFi.softAP(_APssid, _APpass);
                    break;
                }
            }
        };
        if (i > 60)
            break;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Debugln();
        return true;

        // Debugf("\nconnected: SSID = %s, pass = %s\n", WiFi.SSID().c_str(), WiFi.psk().c_str());
    }

    else
    {
        Debugln();

        return false;
    }
};

void cache Settingsmanager::printdiagnositics()
{

    // Debugln(F("\n------------------"));
    // WiFi.printDiag(Serial);

    // Debug(F("ssid: "));
    // Debugln( WiFi.SSID()   );
    // Debug(F("psk: "));
    // Debugln( WiFi.psk()  );

    // Debug(F("host: "));
    // Debugln( WiFi.hostname()  );

    // Debug(F("BSSID: "));
    // Debugln(  WiFi.BSSIDstr() );

    // Debug(F("IP: "));
    // Debugln( WiFi.localIP()  );

    // Debug(F("Subnet: "));
    // Debugln(  WiFi.subnetMask() );

    // Debug(F("gateway: "));
    // Debugln(  WiFi.gatewayIP() );

    // Debug(F("DHCP: "));

    // uint8_t dhcpstate =  wifi_station_dhcpc_status();
    // Debugln(dhcpstate);

    // Debugln(F("--------END-------"));
}

String cache Settingsmanager::IPtoString(IPAddress address)
{

    String IP = "";
    for (int i = 0; i < 4; i++)
    {
        IP += String(address[i]);
        if (i < 3)
            IP += ".";
    }

    return IP;
}

IPAddress cache Settingsmanager::StringtoIP(const String IP_string)
{

    char inputbuffer[IP_string.length() + 1];
    strcpy(inputbuffer, IP_string.c_str());
    char* IP = &inputbuffer[0];
    char* pch;
    pch = strtok(IP, ".");
    uint8_t position = 0;
    IPAddress returnIP;

    while (pch != NULL && position < 4)
    {
        returnIP[position++] = atoi(pch);
        pch = strtok(NULL, ".");
    }
    return returnIP;
}


void cache Settingsmanager::NewFileCheck() {

    static const uint8_t file_no_2 = 5; 

    static const char * _jq1 =  "/jq1.11.1.js.gz"; 
    static const char * _jq2 =  "/jqm1.4.5.css.gz"; 
    static const char * _jq3 =  "/jqm1.4.5.js.gz"; 
    static const char * _jq4 =  "/configjava.js"; 
    static const char * _htm1 = "/config.htm"; 

     const char * items2[file_no_2] = {_jq1,_jq2,_jq3,_jq4,_htm1} ; // ,jq4,htm1,htm2,htm3}; 


    for (uint8_t i = 0; i < file_no_2; i++) {


        if (SPIFFS.exists(items2[i])) {
            String buf = "/espman"; 
            buf += items2[i];        
            if (i == 4)  buf = "/espman/index.htm\n"; 
            SPIFFS.rename(buf,items2[i]); 
            Serial.printf("%s ==> %s\n", items2[i], buf.c_str()); 
        } else Serial.printf("%s : Not found\n", items2[i]); 


    }



}


void cache Settingsmanager::HandleDataRequest()
{

    String buf;
    uint8_t _wifinetworksfound = 0;

    String args = F("ARG: %u, \"%s\" = \"%s\"\n");
    for (uint8_t i = 0; i < _HTTP->args(); i++)
    {
        Debugf(args.c_str(), i, _HTTP->argName(i).c_str(), _HTTP->arg(i).c_str());
    }

    /*------------------------------------------------------------------------------------------------------------------
                                                                    Reboot command
    ------------------------------------------------------------------------------------------------------------------*/

    if (_HTTP->arg("plain") == "reboot")
    {
        Debugln(F("Rebooting..."));
        _HTTP->send(200, "text", "OK"); // return ok to speed up AJAX stuff
        ESP.restart();
    };

    /*------------------------------------------------------------------------------------------------------------------
                                      WiFi Scanning and Sending of WiFi networks found at boot
    ------------------------------------------------------------------------------------------------------------------*/
    if (_HTTP->arg("plain") == F("WiFiDetails") || _HTTP->arg("plain") == F("PerformWiFiScan"))
    {


        if (_HTTP->arg("plain") == F("PerformWiFiScan"))
        {
            Debug(F("Performing Wifi Network Scan..."));
            _wifinetworksfound = WiFi.scanNetworks();
            Debugln(F("done"));
        }

        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.createObject();

        if (_wifinetworksfound)
        {

            JsonArray& Networkarray = root.createNestedArray("networks");

            for (int i = 0; i < _wifinetworksfound; ++i)
            {
                JsonObject& ssidobject = Networkarray.createNestedObject();

                bool connectedbool
                    = (WiFi.status() == WL_CONNECTED && WiFi.SSID(i) == WiFi.SSID()) ? true : false;
                uint8_t encryptiontype = WiFi.encryptionType(i);
                ssidobject[F("ssid")] = WiFi.SSID(i);
                ssidobject[F("rssi")] = WiFi.RSSI(i);
                ssidobject[F("connected")] = connectedbool;
                ssidobject[F("channel")] = WiFi.channel(i);
                switch (encryptiontype)
                {
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
        ;
        APobject[F("IP")] = (WiFi.softAPIP()[0] == 0 && WiFi.softAPIP()[1] == 0
                                && WiFi.softAPIP()[2] == 0 && WiFi.softAPIP()[3] == 0)
            ? F("192.168.4.1")
            : IPtoString(WiFi.softAPIP());
        APobject[F("hidden")] = (_APhidden) ? true : false;
        APobject[F("password")] = (_APpass) ? _APpass : "";
        APobject[F("channel")] = _APchannel;
        APobject[F("MAC")] = WiFi.softAPmacAddress();


        size_t jsonlength = root.measureLength() + 1;
        char buffer[jsonlength + 1];
        root.printTo(buffer, jsonlength + 1);
        _HTTP->send(200, "text/json", String(buffer));
        Debugf("Min Heap: %u\n", ESP.getFreeHeap());

        WiFi.scanDelete();
        _wifinetworksfound = 0;


        return;
    }

    /*------------------------------------------------------------------------------------------------------------------
                                      Send about page details...
    ------------------------------------------------------------------------------------------------------------------*/
    if (_HTTP->arg("plain") == "AboutPage")
    {

        const uint8_t bufsize = 10;
        int sec = millis() / 1000;
        int min = sec / 60;
        int hr = min / 60;
        int Vcc = analogRead(A0);

        char Up_time[bufsize];
        snprintf(Up_time, bufsize, "%02d:%02d:%02d", hr, min % 60, sec % 60);

        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.createObject();

        root[F("version_var")] = version;
        root[F("compiletime_var")] = _compile_date_time;
        root[F("sdk_var")] = ESP.getSdkVersion();
        root[F("heap_var")] = ESP.getFreeHeap();
        root[F("flashsize_var")] = ESP.getFlashChipSize();
        root[F("chipid_var")] = ESP.getChipId();
        root[F("flashid_var")] = ESP.getFlashChipId();
        root[F("sketchsize_var")] = ESP.getSketchSize();
        root[F("freespace_var")] = ESP.getFreeSketchSpace();
        root[F("millis_var")] = millis();
        root[F("uptime_var")] = Up_time;
        root[F("vcc_var")] = ESP.getVcc();
        root[F("rssi_var")] = WiFi.RSSI();
        root[F("cpu_var")] = ESP.getCpuFreqMHz();

        size_t jsonlength = root.measureLength() + 1;
        char buffer[jsonlength + 1];
        root.printTo(buffer, jsonlength + 1);
        _HTTP->send(200, "text/json", String(buffer));
        Debugf("Min Heap: %u\n", ESP.getFreeHeap());

        return;
    }

    /*------------------------------------------------------------------------------------------------------------------
                                      SSID handles...
    ------------------------------------------------------------------------------------------------------------------*/

    static int8_t WiFiresult = -1;

    if (_HTTP->hasArg("ssid") && _HTTP->hasArg("pass"))
    {
        if (_HTTP->arg("ssid").length() > 0)
        { // _HTTP->arg("pass").length() > 0) {  0 length passwords should be ok.. for open
          // networks.
            if (_HTTP->arg("ssid").length() < 33 && _HTTP->arg("pass").length() < 33)
            {
                if (_HTTP->arg("ssid") != WiFi.SSID() || _HTTP->arg("pass") != WiFi.psk())
                {
                    
                    if(WiFi.getMode() == WIFI_AP) { WiFi.mode(WIFI_AP_STA);  };  // if WiFi STA off, turn it on.. 


                    save_flag = true;
                    WiFiresult = -1;
                    char old_ssid[33];
                    char old_pass[33];
                    strcpy(old_pass, (const char*)WiFi.psk().c_str());
                    strcpy(old_ssid, (const char*)WiFi.SSID().c_str());

                    _HTTP->send(200, "text",
                        "accepted"); // important as it defines entry to the wait loop on client

                    Debug(F("Disconnecting.."));

                    WiFi.disconnect();
                    Debug(F("done \nInit..."));


                    //     _ssid = strdup((const char *)_HTTP->arg("ssid").c_str());
                    //     _pass = strdup((const char *)_HTTP->arg("pass").c_str());

                    /*

                            ToDo....  put in check for OPEN networks that enables the AP for 5
                       mins....

                    */

                    //  First try does not change the vars
                    WiFi.begin((const char*)_HTTP->arg("ssid").c_str(),
                        (const char*)_HTTP->arg("pass").c_str());

                    uint8_t i = 0;

                    while (WiFi.status() != WL_CONNECTED && _HTTP->arg("removesaftey") == "No")
                    {
                        WiFiresult = 0;
                        delay(500);
                        i++;
                        Debug(".");
                        if (i == 30)
                        {

                            Debug(F("Unable to join network...restore old settings"));
                            save_flag = false;
                            WiFiresult = 2;

                            //    strcpy(_ssid,old_ssid);
                            //    strcpy(_pass,old_pass);

                            WiFi.disconnect();
                            WiFi.begin(_ssid, _pass);

                            while (WiFi.status() != WL_CONNECTED)
                            {
                                delay(500);
                                Debug(".");
                            };
                            Debug(F("done"));
                            break;
                        }
                    }

                    Debugf("\nconnected: SSID = %s, pass = %s\n", WiFi.SSID().c_str(),

                        WiFi.psk().c_str());
                    if (WiFiresult == 0)
                        WiFiresult = 1; // not sure why i did this.. think it is the client end.
                    if (WiFiresult)
                    {

                        if (_ssid)
                        {
                            free((void*)_ssid);
                            _ssid = NULL;
                        };
                        if (_pass)
                        {
                            free((void*)_pass);
                            _pass = NULL;
                        };
                        _ssid = strdup((const char*)_HTTP->arg("ssid").c_str());
                        _pass = strdup((const char*)_HTTP->arg("pass").c_str());
                        Debugln(F("New SSID and PASS work:  copied to system vars"));
                    }
                    // return;
                }
            }
        }
    }

    //  This is outside the loop...  wifiresult is a static to return previous result...
    if (_HTTP->arg("plain") == "WiFiresult")
    {
        _HTTP->send(200, "text", String(WiFiresult));
        return;
    }
    /*------------------------------------------------------------------------------------------------------------------

                                     STA config
    ------------------------------------------------------------------------------------------------------------------*/

    if (_HTTP->hasArg("enable-STA"))
    {
        save_flag = true;

        // IPAddress localIP, gateway, subnet;
        WiFiMode mode = WiFi.getMode();
        bool reinit = false;

        if (_HTTP->arg("enable-STA") == "off" && (mode == WIFI_STA || mode == WIFI_AP_STA))
        {
            WiFi.mode(WIFI_AP); // always fall back to AP mode...
            WiFi.softAP(_APssid, _APpass, (int)_APchannel, (int)_APhidden);
            Debugln(F("STA-disabled: falling back to AP mode."));
        }
        else if (_HTTP->arg("enable-STA") == "on" && mode == WIFI_AP)
        {
            WiFi.mode(WIFI_AP_STA);
            Debugln(F("Enabling STA mode"));
            reinit = true;
        }


        if (_HTTP->arg("enable-dhcp") == "on")
        {

            _DHCP = true;
            save_flag = true;
            bool dhcpresult = wifi_station_dhcpc_start(); // requires userinterface...

            Debugln(F("------- RESARTING--------"));
            Debug(F("DHCP result: "));
            Debugln(dhcpresult);
            reinit = true;
        }
        else if (_HTTP->arg("enable-dhcp") == "off")
        {

            save_flag = true;
            _DHCP = false;

            if (!_IPs)
                _IPs = new IPconfigs_t; // create memory for new IPs


            bool ok = true;

            if (_HTTP->hasArg("setSTAsetip"))
            {
                _IPs->IP = StringtoIP(_HTTP->arg("setSTAsetip"));
                Debug(F("IP = "));
                Debugln(_IPs->IP);
            }
            else
                ok = false;

            if (_HTTP->hasArg("setSTAsetgw"))
            {
                _IPs->GW = StringtoIP(_HTTP->arg("setSTAsetgw"));
                Debug(F("gateway = "));
                Debugln(_IPs->GW);
            }
            else
                ok = false;

            if (_HTTP->hasArg("setSTAsetsn"))
            {
                _IPs->SN = StringtoIP(_HTTP->arg("setSTAsetsn"));
                Debug(F("subnet = "));
                Debugln(_IPs->SN);
            }
            else
                ok = false;

            if (ok)
            {
                // WiFi.config(localIP, gateway, subnet);
                reinit = true;
            }
        }

        if (reinit && _HTTP->arg("enable-STA") == "on")
            Wifistart();
            printdiagnositics();


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

    if (_HTTP->hasArg("enable-AP"))
    {
        save_flag = true;

        WiFiMode mode = WiFi.getMode();
        // WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3
        // if (mode == WIFI_STA || mode == WIFI_AP_STA ) STAstate = "ENABLED"; else STAstate =
        // "DISABLED";
        // if (mode == WIFI_AP || mode == WIFI_AP_STA ) APstate = "ENABLED"; else APstate =
        // "DISABLED";

        if (_HTTP->arg("setAPsetssid").length() != 0)
        {

            if (_APssid)
            {
                free((void*)_APssid);
                _APssid = NULL;
            }
            _APssid = strdup((const char*)_HTTP->arg("setAPsetssid").c_str());
        };

        if (_HTTP->arg("setAPsetpass").length() != 0 && _HTTP->arg("setAPsetpass").length() < 63)
        {
            //_APpass = (const char *)_HTTP->arg("setAPsetpass").c_str();

            if (_APpass)
            {
                free((void*)_APpass);
                _APpass = NULL;
            }; // free memory if AP pass has been allocated.

            _APpass = strdup((const char*)_HTTP->arg("setAPsetpass").c_str());
        }
        else if (_HTTP->arg("setAPsetpass").length() == 0 && _APpass)
        {

            if (_APpass)
            {
                free((void*)_APpass);
                _APpass = NULL;
            }; // free memory if AP pass has been allocated.
        };


        if (_HTTP->arg("enable-AP") == "on")
        {

            uint8_t channel = _HTTP->arg("setAPsetchannel").toInt();
            if (channel > 13)
                channel = 13;
            _APchannel = channel;

            Debug(F("Enable AP channel: "));
            Debugln(_APchannel);

            if (mode == WIFI_STA)
                WiFi.mode(WIFI_AP_STA);

            InitialiseSoftAP();
            printdiagnositics();
        }
        else if (_HTTP->arg("enable-AP") == "off")
        {

            Debugln(F("Disable AP"));

            if (mode == WIFI_AP_STA || mode == WIFI_AP)
                WiFi.mode(WIFI_STA);

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

    if (_HTTP->hasArg("deviceid"))
    {
        if (_HTTP->arg("deviceid").length() != 0 && 
            _HTTP->arg("deviceid").length() < 32 && 
            _HTTP->arg("deviceid") != String(_host) )
        {
            save_flag = true;
            Debugln(F("Device ID changed"));
            //      if (_host) free( (void*)_host);

            if (strcmp(_host, _APssid) == 0)
            {
                if (_APssid)
                {
                    free((void*)_APssid);
                    _APssid = NULL;
                }
                _APssid = strdup((const char*)_HTTP->arg("deviceid").c_str());
            }

            if (_host)
            {
                free((void*)_host);
                _host = NULL;
            }
            _host = strdup((const char*)_HTTP->arg("deviceid").c_str());
            //  might need to add in here, wifireinting...
            WiFi.hostname(_host);
            InitialiseFeatures();
            InitialiseSoftAP();
        }
    }
    /*------------------------------------------------------------------------------------------------------------------

                                     Restart wifi
    ------------------------------------------------------------------------------------------------------------------*/

    if (_HTTP->arg("plain") == "resetwifi")
    {

        WiFi.disconnect();
        ESP.restart();
    }

    /*------------------------------------------------------------------------------------------------------------------

                                     AP config
    ------------------------------------------------------------------------------------------------------------------*/
    if (_HTTP->hasArg("otaenable"))
    {
        Debugln(F("Depreciated")); 
        // save_flag = true;

        // bool command = (_HTTP->arg("otaenable") == "on") ? true : false;

        // if (command != _OTAenabled)
        // {
        //     _OTAenabled = command;


        //     if (_OTAenabled)
        //     {
        //         // long before = ESP.getFreeHeap();
        //         Debugln(F("Enable OTA"));

        //         InitialiseFeatures();
        //         // String insert = F("OTA Enabled, heap used: %u \n");
        //         // Debugf(insert.c_str(), before - ESP.getFreeHeap() );
        //     }
        //     else
        //     {

        //         // long before = ESP.getFreeHeap();
        //         Debugln(F("Disable OTA"));

        //         if (ota_server)
        //         {
        //             delete ota_server;
        //             ota_server = NULL;
        //         };

        //         // Debugf( "OTA deactivated, heap reclaimed: %u \n", ESP.getFreeHeap() - before );
        //     }
        // }
    } // end of OTA enable

    /*
    ARG: 0, "enable-AP" = "on"
    ARG: 1, "setAPsetip" = "0.0.0.0"
    ARG: 2, "setAPsetmac" = "1A%3AFE%3A34%3AA4%3A4C%3A73"
    */

    if (_HTTP->hasArg("mdnsenable"))
    {
        save_flag = true;

        bool command = (_HTTP->arg("mdnsenable") == "on") ? true : false;

        if (command != _mDNSenabled)
        {
            _mDNSenabled = command;

            InitialiseFeatures();


            if (_mDNSenabled)
            {
                // long before = ESP.getFreeHeap();
                Debugln(F("Enable mDNS"));

                // String insert = F("OTA Enabled, heap used: %u \n");
                // Debugf(insert.c_str(), before - ESP.getFreeHeap() );
            }
            else
            {

                // long before = ESP.getFreeHeap();
                Debugln(F("Disable mDNS"));


                // Debugf( "OTA deactivated, heap reclaimed: %u \n", ESP.getFreeHeap() - before );
            }
        }
    } // end of OTA enable
  /*------------------------------------------------------------------------------------------------------------------

                                     FORMAT SPIFFS
    ------------------------------------------------------------------------------------------------------------------*/
     if (_HTTP->arg("plain") == "formatSPIFFS" & _HTTP->method() == HTTP_POST) {
        Debug(F("Format SPIFFS"));
        //SPIFFS.format(); 
        Debugln(F(" done"));
     }

     if (_HTTP->arg("plain") == "upgrade" & _HTTP->method() == HTTP_POST) {
        // Debug(F("Upgrade files"));

        //     Dir dir = SPIFFS.openDir("/");
        //      while (dir.next()) {    
        //         String fileName = dir.fileName();
        //             size_t fileSize = dir.fileSize();
        //             Debugf("     Deleting: %s\n", fileName.c_str());
        //             SPIFFS.remove(fileName);
        //         }
        // Debugln(F(" done, rebooting"));
        // _HTTP->send(200, "text", "OK"); // return ok to speed up AJAX stuff
        // ESP.restart(); 
        FilesCheck(false);
     }     

     if (_HTTP->arg("plain") == "deletesettings" & _HTTP->method() == HTTP_POST) {

        Debug(F("Delete Settings File"));
            if (SPIFFS.remove("/settings.txt")) {
                Debugln(F(" done"));
            } else {
                Debugln(F(" failed"));
            }
    }



    _HTTP->send(200, "text", "OK"); // return ok to speed up AJAX stuff
}
