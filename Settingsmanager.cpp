
#include "Settingsmanager.h"

extern "C" {
#include "user_interface.h"
}


Settingsmanager::Settingsmanager(ESP8266WebServer * HTTP, fs::FS * fs, const char * host, const char * ssid , const char * pass) {

	_HTTP = HTTP; 
	_fs = fs; 
	//This sets the default fallback options... not in use yet
	if (host && ( strlen(host) < 32) ) { _host = strdup(host); } 
	if (ssid && ( strlen(ssid) < 32) ) { _ssid = strdup(ssid); }
	if (pass && ( strlen(pass) < 63) ) { _pass = strdup(pass); }

}

Settingsmanager::~Settingsmanager() {

  if (ota_server) { delete ota_server; ota_server = NULL; }; 

  if (_host)   { free ( (void *) _host  ); _host = NULL; };
  if (_pass)   { free ( (void *) _pass  ); _pass = NULL; };
  if (_ssid)   { free ( (void *) _ssid  ); _ssid = NULL; };
  if (_APpass) { free ( (void *) _APpass); _APpass = NULL; } ; 
  if (_APssid) { free ( (void *) _APssid); _APssid = NULL; } ; 
  if (_IPs) { delete _IPs; _IPs = NULL; } ; 


}

void cache Settingsmanager::begin() {

    if(SPIFFS.begin()) { 
      Serial.println(F("File System mounted sucessfully"));
      LoadSettings(); 
    } else {
      Serial.println(F("File System mount failed"));
    }



	if(_host)  {
		if (WiFi.hostname(_host)) {
		Serial.print("Host Name Set: ");
		Serial.println(_host);
    }
	} else {
    _host = strdup((const char *)WiFi.hostname().c_str()); 
	}

	if (!_APssid) 
  {
		_APssid = strdup(wifi_station_get_hostname()); 
	}


   if(!Wifistart() ) {
    WiFiMode(WIFI_AP_STA);
    WiFi.softAP(_APssid, _APpass, (int)_APchannel, (int)_APhidden);
    Serial.println(F("WiFi Failed:  Starting AP")); 

    //  to add timeout on AP... 
   }

    printdiagnositics(); 

    InitialiseFeatures(); 

    _HTTP->on("/data.esp", std::bind(&Settingsmanager::HandleDataRequest, this)); 
    _HTTP->serveStatic("/config.htm", *_fs, "/config.htm"); 
	   
    
  
}

void cache Settingsmanager::LoadSettings() {
      Serial.print("OLD ----- "); 

      PrintVariables();

      DynamicJsonBuffer jsonBuffer;

      File f = SPIFFS.open("/settings.txt", "r");   
      
      if (!f) {
         Serial.println(F("Settings file open failed!"));
         return; 
      }

      f.seek(0,SeekSet); 

      char data[f.size()]; 

      for (int i = 0; i < f.size(); i++) {
        data[i] = f.read(); 
      }

      f.close(); 

      JsonObject& root = jsonBuffer.parseObject(data);

      if (!root.success())
      {
        Serial.println(F("Parsing settings file Failed!"));
        return; 
      }
      //root.prettyPrintTo(Serial);
  
  if (root.containsKey("host")) {
      const char* host = root["host"]; 
      if (_host) { free((void*)_host); _host = NULL; }; 
      _host = strdup (host);       
  }

  if (root.containsKey("ssid")) {
      const char* ssid = root["ssid"]; 
      if (_ssid) { free((void*)_ssid); _ssid = NULL; };
      _ssid = strdup (ssid); 
  }
  
  if (root.containsKey("pass")) {
      const char* pass = root["pass"]; 
      if (_pass) { free((void*)_pass); _pass = NULL; }; 
      _pass = strdup (pass); 
  }

  if (root.containsKey("APpass")) {
      const char* APpass = root["APpass"]; 
      if (_APpass) { free((void*)_APpass); _APpass = NULL; };
      _APpass = strdup (APpass); 
  }
  
  if (root.containsKey("APpass")) {
      const char* APssid = root["APssid"]; 
      if (_APssid) { free((void*)_APssid); _APssid = NULL; }; 
      _APssid = strdup (APssid); 
  }

  if (root.containsKey("APpass")) {
      long APchannel = root["APchannel"];
      if (APchannel < 13 && APchannel > 0) _APchannel = (uint8_t)APchannel; 
    }

  if (root.containsKey("DHCP")) { 
    const char* dhcp = root["DHCP"];
    _DHCP = (strcmp(dhcp,"true") == 0)? true : false ; 
  }
  
  if (root.containsKey("APenabled")) {  
    const char* APenabled = root["APenabled"]; 
    _APenabled = (strcmp(APenabled,"true") == 0)? true : false ; 
  }
  
  if (root.containsKey("APhidden")) { 
    const char* APhidden = root["APhidden"];
    _APhidden = (strcmp(APhidden,"true") == 0)? true : false ; 
  }
  
  if (root.containsKey("OTAenabled")) { 
    const char* OTAenabled = root["OTAenabled"];
    _OTAenabled = (strcmp(OTAenabled, "true") == 0)? true: false ; 
  }

  if (root.containsKey("IPaddress") && root.containsKey("Gateway") && root.containsKey("Subnet")) {
      const char * ip = root["IPaddress"]; 
      const char * gw = root["Gateway"]; 
      const char * sn = root["Subnet"]; 
      
    if (!_DHCP) { // only bother to allocate memory if dhcp is NOT being used. 
      if (_IPs) { delete _IPs; _IPs = NULL;}; 
      _IPs = new IPconfigs_t;
      _IPs->IP = StringtoIP(String(ip)); 
      _IPs->GW = StringtoIP(String(gw));
      _IPs->SN = StringtoIP(String(sn));
    }
  }
      Serial.print("NEW ----- "); 
      PrintVariables();



}


void cache Settingsmanager::PrintVariables() {

      Serial.println(F("VARIABLE STATES: ")); 
      Serial.printf("_host = %s\n", _host); 
      Serial.printf("_ssid = %s\n", _ssid); 
      Serial.printf("_pass = %s\n", _pass); 
      Serial.printf("_APpass = %s\n", _APpass); 
      Serial.printf("_APssid = %s\n", _APssid); 
      Serial.printf("_APchannel = %u\n", _APchannel); 
      (_DHCP)? Serial.println(F("_DHCP = true")): Serial.println(F("_DHCP = false")); 
      (_APenabled)? Serial.println(F("_APenabled = true")): Serial.println(F("_APenabled = false")); 
      (_APhidden)? Serial.println(F("_APhidden = true")): Serial.println(F("_APhidden = false")); 
      (_OTAenabled)? Serial.println(F("_OTAenabled = true")): Serial.println(F("_OTAenabled = false")); 

      if (_IPs) {
      Serial.print(F("IPs->IP = "));
      Serial.println(IPtoString(_IPs->IP)); 
      Serial.print(F("IPs->GW = "));
      Serial.println(IPtoString(_IPs->GW)); 
      Serial.print(F("IPs->SN = "));
      Serial.println(IPtoString(_IPs->SN)); 
      } else Serial.println(F("NO IPs held in memory")); 

}

void cache Settingsmanager::SaveSettings() {
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

      root["host"] = (_host)? _host: C_null;
      root["ssid"] = (_ssid)? _ssid: C_null;
      root["pass"] = (_pass)? _pass: C_null;
      root["APpass"] = (_APpass)? _APpass: C_null;
      root["APssid"] = (_APssid)? _APssid: C_null; 
      root["DHCP"] = (_DHCP)? C_true : C_false; 
      root["APchannel"] = (_APchannel)? C_true : C_false; 
      root["APenabled"] = (_APenabled)? C_true : C_false; 
      root["APhidden"] = (_APhidden)? C_true : C_false; 
      root["OTAenabled"] = (_OTAenabled)? C_true : C_false; 
      
      char IP[30];
      String ip = IPtoString(WiFi.localIP());
      ip.toCharArray(IP, ip.length() + 3 );
      char GW[30];
      String gw = IPtoString(WiFi.gatewayIP()); 
      gw.toCharArray(GW, gw.length() + 3 );
      char SN[30];
      String sn = IPtoString(WiFi.subnetMask()); 
      sn.toCharArray(SN, sn.length() + 3 );
      
      root["IPaddress"] = IP; 
      root["Gateway"] = GW; 
      root["Subnet"] = SN; 


      //Serial.printf("IP = %s, GW = %s, SN = %s\n", IP, GW, SN); 
      File f = SPIFFS.open("/settings.txt", "w");   
      if (!f) {
         Serial.println(F("Settings file save failed!"));
         return; 
      }

      root.prettyPrintTo(f);
      f.close(); 

      Serial.printf("Save done. %ums\n", millis() - starttime); 

}

void cache Settingsmanager::handle() {

 if(ota_server) ota_server->handle();

 if(save_flag) {
  SaveSettings();
  save_flag = false; 
 }

  //
  //
  // Async handle WiFi reconnects... 
  //
  //

}

void cache Settingsmanager::InitialiseFeatures() {

  if (_OTAenabled) {
    char OTAhost[strlen(_host) + 2];
    strcpy(OTAhost, _host);
    OTAhost[strlen(_host)] = '-';
    OTAhost[strlen(_host)+1] = 0;
    if (ota_server) { delete ota_server; ota_server = NULL; };  
    ota_server = new ArduinoOTA(OTAhost, 8266, true);
    ota_server->setup();
  }
    
    WiFi.hostname(_host);

}

 void cache Settingsmanager::InitialiseSoftAP() {

      WiFiMode mode = WiFi.getMode();

      if (mode == WIFI_AP_STA || mode == WIFI_AP) {

        WiFi.softAP(_APssid, _APpass, _APchannel, _APhidden);

      }

 }

bool cache Settingsmanager::Wifistart() {
  //WiFiMode(WIFI_AP_STA);
  //WiFi.begin(_ssid,_pass);

  if (WiFi.getMode() == WIFI_AP ) return false; 

  WiFi.begin();

  Serial.print("WiFi init");
  uint8_t i = 0; 
  while (WiFi.status() != WL_CONNECTED ) {

        delay(500);
        i++;
        Serial.print(".");
        if (i == 30) {
          Serial.print(F("Auto connect failed..Trying stored credentials...")); 
            WiFi.begin ( _ssid, _pass );
                while (WiFi.status() != WL_CONNECTED ) {
                  delay(500);
                  i++;
                  Serial.print(".");
                  if (i == 60) {
                      Serial.print(F("Failed... setting up AP"));
                      WiFi.mode(WIFI_AP_STA);
                      WiFi.softAP(_APssid, _APpass);                       
                      break; 
                    }
                  }
           
        } ;
        if (i > 60) break; 
  }

  if (WiFi.status() == WL_CONNECTED ) { 
    return true; 
    Serial.printf("\nconnected: SSID = %s, pass = %s\n", WiFi.SSID().c_str(), WiFi.psk().c_str()); 

  }
     else return false ;

};

void cache Settingsmanager::printdiagnositics() {

	Serial.println(F("\n------------------"));
	WiFi.printDiag(Serial);

	Serial.print(F("ssid: "));
	Serial.println( WiFi.SSID()   );
	Serial.print(F("psk: "));
	Serial.println( WiFi.psk()  );
	
	Serial.print(F("host: "));
	Serial.println( WiFi.hostname()  );

	Serial.print(F("BSSID: "));
	Serial.println(  WiFi.BSSIDstr() );

	Serial.print(F("IP: "));
	Serial.println( WiFi.localIP()  );

	Serial.print(F("Subnet: "));
	Serial.println(  WiFi.subnetMask() );

	Serial.print(F("gateway: "));
	Serial.println(  WiFi.gatewayIP() );

	Serial.print(F("DHCP: "));

	uint8_t dhcpstate =  wifi_station_dhcpc_status(); 
	Serial.println(dhcpstate);

	Serial.println(F("--------END-------"));


}

String cache Settingsmanager::IPtoString(IPAddress address) {

  String IP = "";
  for (int i = 0; i < 4; i++)
    { 
      IP += String(address[i]); 
      if (i < 3) IP += ".";
    }

  return IP;  
}

IPAddress cache Settingsmanager::StringtoIP(const String IP_string){

  char inputbuffer[IP_string.length() + 1];
  strcpy(inputbuffer, IP_string.c_str());
  char * IP = &inputbuffer[0] ;
  char * pch;
  pch = strtok (IP,".");
  uint8_t position = 0;
  IPAddress returnIP;

  while (pch != NULL && position < 4)
  {
  	returnIP[position++] = atoi(pch);
    pch = strtok (NULL, ".");
  }
  return returnIP;
}



void cache Settingsmanager::HandleDataRequest() {


 String buf;
  uint8_t _wifinetworksfound = 0; 

   String args = F("ARG: %u, \"%s\" = \"%s\"\n");
        for (uint8_t i = 0; i <  _HTTP->args(); i++ ) {
          Serial.printf(args.c_str(), i,  _HTTP->argName(i).c_str(),  _HTTP->arg(i).c_str());
        }

/*------------------------------------------------------------------------------------------------------------------
                                  				Reboot command
------------------------------------------------------------------------------------------------------------------*/

  if ( _HTTP->arg("plain") == "reboot") { Serial.println(F("Rebooting...")); ESP.restart(); } ;

/*------------------------------------------------------------------------------------------------------------------
                                  WiFi Scanning and Sending of WiFi networks found at boot
------------------------------------------------------------------------------------------------------------------*/
  if ( _HTTP->arg("plain") == F("WiFiDetails") ||  _HTTP->arg("plain") == F("PerformWiFiScan")) {


    if ( _HTTP->arg("plain") == F("PerformWiFiScan")) {
      Serial.print(F("Performing Wifi Network Scan..."));
      _wifinetworksfound = WiFi.scanNetworks(); 
      Serial.println(F("done"));
    }

	    DynamicJsonBuffer jsonBuffer;    
      JsonObject& root = jsonBuffer.createObject();



    //String connected; 

    //buf = F("{");

    if (_wifinetworksfound) {
      
      JsonArray&  Networkarray  = root.createNestedArray("networks");
    
    //buf +=  F("\"networks\": [");

    for (int i = 0; i < _wifinetworksfound; ++i)
  {
      JsonObject& ssidobject = Networkarray.createNestedObject();
    //    if(WiFi.status() == WL_CONNECTED && strcmp( config.ssid, WiFi.SSID(i).c_str()) == 0)
    //if(WiFi.status() == WL_CONNECTED && WiFi.SSID(i) == WiFi.SSID()) { connected = F("true"); } else { connected = F("false"); }
    bool connectedbool = (WiFi.status() == WL_CONNECTED && WiFi.SSID(i) == WiFi.SSID())? true : false; 

    uint8_t encryptiontype = WiFi.encryptionType(i);
    //String encryptionString; 

    const char * encryptionchar = "OPEN"; 



      ssidobject["ssid"] = WiFi.SSID(i); 
      ssidobject["rssi"] = WiFi.RSSI(i); 
      ssidobject["connected"] = connectedbool; 
      ssidobject["channel"] = WiFi.channel(i); 
      switch (encryptiontype) {
         case ENC_TYPE_NONE:
          //encryptionString = F("OPEN");
          ssidobject["encyrpted"] = "OPEN"; 
          break;
         case ENC_TYPE_WEP:
          //encryptionString = F("WEP");
          ssidobject["encyrpted"] = "WEP"; 
          break;
         case ENC_TYPE_TKIP:
          //encryptionString = F("WPA_PSK");
          ssidobject["encyrpted"] = "WPA_PSK"; 
          break;
         case ENC_TYPE_CCMP:
          //encryptionString = F("WPA2_PSK");
          ssidobject["encyrpted"] = "WPA2_PSK"; 
          break;
         case ENC_TYPE_AUTO:
          //encryptionString = F("AUTO");
          ssidobject["encyrpted"] = "AUTO";          
          break;
      }
      
      ssidobject["BSSID"] = WiFi.BSSIDstr(i); 

    // if (i > 0) buf += ",";
    // buf += F("{\"ssid\":\""); 
    // buf += String(WiFi.SSID(i)); 
    // buf += F("\",\"rssi\":\"" );
    // buf += String(WiFi.RSSI(i)) ;
    // buf += F("\",\"connected\":\""); 
    // buf += connected ;
    // buf += F("\",\"channel\":\"" );
    // buf += String(WiFi.channel(i)) ;
    // buf += F("\",\"encyrpted\":\"" );
    // buf += encryptionString ;
    // buf += F("\",\"BSSID\":\""); 
    // buf += WiFi.BSSIDstr(i) ;
    // buf += F("\"}");
    
     }

    //if (_wifinetworksfound == 0 ) buf += F("{\"ssid\":\"\",\"rssi\":\"\",\"connected\":\"\",\"channel\":\"\",\"encyrpted\":\"\",\"BSSID\":\"\"}");
 
     // buf += F("],");
	
   }

    WiFiMode mode = WiFi.getMode();

    // String APstate = (mode == WIFI_AP || mode == WIFI_AP_STA ) ? F("ENABLED") : F("DISABLED");
    // String STAstate = (mode == WIFI_STA || mode == WIFI_AP_STA ) ? F("ENABLED") : F("DISABLED");
    // String dhcpstate = (_DHCP)? F("on") : F("off");
    // String isHidden = (_APhidden)? F("true") : F("false"); 
    // String APip = (WiFi.softAPIP()[0] == 0 && WiFi.softAPIP()[1] == 0 && WiFi.softAPIP()[2] == 0 && WiFi.softAPIP()[3] == 0)? F("192.168.4.1") : IPtoString(WiFi.softAPIP()); 
    // String otastate = (_OTAenabled)? F("enabled") : F("disabled");

    JsonObject& generalobject = root.createNestedObject("general");

    // buf += F("\"general\":{");
    // buf += "\"deviceid\":\"" + String(_host) + "\",";    
    // buf += "\"otaenabled\":\"" + otastate + "\"";
    // buf += F("},");

    generalobject["deviceid"] = _host; 
    generalobject["OTAenabled"] = (_OTAenabled)? true : false;

    // buf += F("\"STA\":{");
    // buf += F("\"dhcp\":\""); 
    // buf += dhcpstate + "\",";
    // buf += F("\"state\":\"" );
    // buf += STAstate + "\",";
    // buf += F("\"IP\":\"" );
    // buf += IPtoString(WiFi.localIP()) + "\",";
    // buf += F("\"gateway\":\"" );
    // buf += IPtoString(WiFi.gatewayIP()) + "\",";
    // buf += F("\"subnet\":\"" );
    // buf += IPtoString(WiFi.subnetMask()) + "\",";
    // buf += F("\"MAC\":\"" );
    // buf += WiFi.macAddress() +"\"";
    //buf += "\"DNS\":\"" + WiFi.  DNS not implemeneted...

    JsonObject& STAobject = root.createNestedObject("STA");

    STAobject["dhcp"] = (_DHCP)? true : false; 
    STAobject["state"] = (mode == WIFI_STA || mode == WIFI_AP_STA )? true : false ; 
    STAobject["IP"] =  IPtoString(WiFi.localIP()); 
    STAobject["gateway"] =  IPtoString(WiFi.gatewayIP()); 
    STAobject["subnet"] =  IPtoString(WiFi.subnetMask()); 
    STAobject["MAC"] = WiFi.macAddress() ; 


   //  buf += F("},");

   //  buf += F("\"AP\":{");
	  // buf += "\"ssid\":\"" + String(_APssid) + "\",";
   //  buf += "\"state\":\"" + APstate + "\",";
   //  buf += "\"IP\":\"" + APip + "\",";
   //  buf += "\"hidden\":\"" + isHidden + "\",";
   //  if(_APpass) buf += "\"password\":\"" + String(_APpass) + "\","; else 
   //  			buf += "\"password\":\"\",";
   //  buf += "\"channel\":\"" + String(_APchannel) + "\",";
   //  buf += "\"MAC\":\"" + WiFi.softAPmacAddress() + "\"";
   //  buf += "}}";

    JsonObject& APobject = root.createNestedObject("AP");
    APobject["ssid"] = _APssid ;
    APobject["state"] = (mode == WIFI_AP || mode == WIFI_AP_STA ) ? true : false;   ;
    APobject["IP"] =   (WiFi.softAPIP()[0] == 0 && WiFi.softAPIP()[1] == 0 && WiFi.softAPIP()[2] == 0 && WiFi.softAPIP()[3] == 0)? F("192.168.4.1") : IPtoString(WiFi.softAPIP()) ;
    APobject["hidden"] =  (_APhidden)? true : false  ;
    APobject["password"] = (_APpass)? _APpass: "" ;
    APobject["channel"] =  _APchannel  ;
    APobject["MAC"] =   WiFi.softAPmacAddress()   ;



   //_HTTP->send(200, "text/json", buf);
    
    root.prettyPrintTo(Serial);
    size_t jsonlength = root.measureLength(); 
    _HTTP->setContentLength(jsonlength);
    _HTTP->send(200, "text/json"); 
    WiFiClient c = _HTTP->client(); 
    root.printTo(c);


  	WiFi.scanDelete();
  	_wifinetworksfound = 0;


  return; 


  }

/*------------------------------------------------------------------------------------------------------------------
                                  Send about page details... 
------------------------------------------------------------------------------------------------------------------*/
  if ( _HTTP->arg("plain") == "AboutPage") {

    const uint8_t bufsize = 10; 
    int sec = millis() / 1000;
    int min = sec / 60;
    int hr = min / 60;
    int Vcc = analogRead(A0); 
    
    char Up_time[bufsize]; 
    snprintf ( Up_time, bufsize, "%02d:%02d:%02d", hr, min % 60, sec % 60 );

    DynamicJsonBuffer jsonBuffer;    
    JsonObject& root = jsonBuffer.createObject();

    root["version_var"] = version;
    root["compiletime_var"] = _compile_date_time;
    root["sdk_var"] = ESP.getSdkVersion();
    root["heap_var"] = ESP.getFreeHeap();
    root["flashsize_var"] = ESP.getFlashChipSize();
    root["chipid_var"] = ESP.getChipId();
    root["flashid_var"] = ESP.getFlashChipId();
    root["sketchsize_var"] = ESP.getSketchSize();
    root["freespace_var"] = ESP.getFreeSketchSpace();
    root["millis_var"] = millis();
    root["uptime_var"] = Up_time;
    root["vcc_var"] = ESP.getVcc();
    root["rssi_var"] = WiFi.RSSI();
    root["cpu_var"] = ESP.getCpuFreqMHz(); 

    //root.prettyPrintTo(Serial);
    size_t jsonlength = root.measureLength(); 
    _HTTP->setContentLength(jsonlength);
    _HTTP->send(200, "text/json"); 
    WiFiClient c = _HTTP->client(); 
    root.printTo(c);

    // buf = "{";
    // buf += F("\"version_var\":\""); 
    // buf += String(version) + "\","; // to remove this... change to host
    // buf += F("\"compiletime_var\":\""); 
    // buf += String(_compile_date_time) + "\",";
    // buf += F("\"sdk_var\":\"");
    // buf += String(ESP.getSdkVersion()) + "\",";
    // buf += F("\"heap_var\":\"");
    // buf += String(ESP.getFreeHeap()) + "\",";  
    // buf += F("\"flashsize_var\":\"");
    // buf += String(ESP.getFlashChipSize()) + "\",";   
    // buf += F("\"chipid_var\":\"");
    // buf += String(ESP.getChipId()) + "\",";
    // buf += F("\"flashid_var\":\"");
    // buf += String(ESP.getFlashChipId()) + "\",";   
    // buf += F("\"sketchsize_var\":\"");
    // buf += String(ESP.getSketchSize()) + "\",";
    // buf += F("\"freespace_var\":\"");
    // buf += String(ESP.getFreeSketchSpace()) + "\",";    
    // buf += F("\"millis_var\":\"");
    // buf += String(millis()) + "\",";   
    // buf += F("\"uptime_var\":\"");
    // buf += String(Up_time) + "\",";
    // buf += F("\"vcc_var\":\"");
    // buf += String(ESP.getVcc() ) + "\",";
    // buf += F("\"rssi_var\":\"");
    // buf += String(WiFi.RSSI()) + "\",";
    // buf += F("\"cpu_var\":\"");
    // buf += String(ESP.getCpuFreqMHz()) + "\"";
    // buf += "}";

    // _HTTP->send(200, "text/json", buf);
    return;

  }

/*------------------------------------------------------------------------------------------------------------------
                                  SSID handles... 
------------------------------------------------------------------------------------------------------------------*/

static int8_t WiFiresult = -1; 

    if ( _HTTP->hasArg("ssid") &&  _HTTP->hasArg("pass") ) {
      if ( _HTTP->arg("ssid").length() > 0 ) { // _HTTP->arg("pass").length() > 0) {  0 length passwords should be ok.. for open networks.
        if ( _HTTP->arg("ssid").length() < 33 &&  _HTTP->arg("pass").length() < 33 ) {
          if (  _HTTP->arg("ssid") != WiFi.SSID() ||  _HTTP->arg("pass") != WiFi.psk() ) {
          save_flag = true; 

        WiFiresult = -1; 
        char old_ssid[33];
        char old_pass[33];
        strcpy(old_pass, (const char *)WiFi.psk().c_str() ) ; 
        strcpy(old_ssid,  (const char *)WiFi.SSID().c_str() ); 

        _HTTP->send(200, "text", "accepted"); // important as it defines entry to the wait loop on client

        Serial.print(F("Disconnecting.."));

        WiFi.disconnect();
        Serial.print(F("done \nInit..."));


   //     _ssid = strdup((const char *)_HTTP->arg("ssid").c_str()); 
   //     _pass = strdup((const char *)_HTTP->arg("pass").c_str());

/*

        ToDo....  put in check for OPEN networks that enables the AP for 5 mins.... 

*/

        //  First try does not change the vars
        WiFi.begin( (const char *)_HTTP->arg("ssid").c_str(),  (const char *)_HTTP->arg("pass").c_str() );
        
        uint8_t i = 0;

        while (WiFi.status() != WL_CONNECTED && _HTTP->arg("removesaftey") == "No") {
            WiFiresult = 0; 
            delay(500);
            i++;
            Serial.print(".");
            if (i == 30) { 

              Serial.print(F("Unable to join network...restore old settings")); 
              save_flag = false; 
              WiFiresult = 2;
              
          //    strcpy(_ssid,old_ssid);
          //    strcpy(_pass,old_pass);

              WiFi.disconnect();
              WiFi.begin(_ssid, _pass);

              while (WiFi.status() != WL_CONNECTED ) {
                  delay(500);
                  Serial.print(".");          
              } ;
              Serial.print(F("done")); 
        break; 
        }

      }

      Serial.printf("\nconnected: SSID = %s, pass = %s\n", WiFi.SSID().c_str(), WiFi.psk().c_str()); 
        if (WiFiresult == 0) WiFiresult = 1; // not sure why i did this.. think it is the client end. 
        if (WiFiresult) {

            if(_ssid) { free((void*)_ssid); _ssid = NULL;};
            if(_pass) { free((void*)_pass); _pass = NULL;};
            _ssid = strdup((const char *)_HTTP->arg("ssid").c_str()); 
            _pass = strdup((const char *)_HTTP->arg("pass").c_str());          
            Serial.println(F("New SSID and PASS work:  copied to system vars")); 
        }
        return; 

          }
        }
      } 
    } 

//  This is outside the loop...  wifiresult is a static to return previous result... 
if ( _HTTP->arg("plain") == "WiFiresult") {   _HTTP->send(200, "text", String(WiFiresult)); return ; }
/*------------------------------------------------------------------------------------------------------------------

                                 STA config
------------------------------------------------------------------------------------------------------------------*/

if ( _HTTP->hasArg("enable-STA")) {
    save_flag = true; 

	IPAddress localIP, gateway, subnet; 
    WiFiMode mode = WiFi.getMode();
    bool reinit = false; 

	if (_HTTP->arg("enable-STA") == "off" && (mode == WIFI_STA || mode == WIFI_AP_STA )) {
		WiFi.mode(WIFI_AP); // always fall back to AP mode... 
		WiFi.softAP(_APssid, _APpass, (int)_APchannel, (int)_APhidden);
		Serial.println(F("STA-disabled: falling back to AP mode."));
	} else if (_HTTP->arg("enable-STA") == "on" && mode == WIFI_AP) {
		WiFi.mode(WIFI_AP_STA); 
		Serial.println(F("Enabling STA mode"));
		reinit = true; 
	}

	


	 if (_HTTP->arg("enable-dhcp") == "on") { 

		_DHCP = true;
    save_flag = true;
		bool dhcpresult = wifi_station_dhcpc_start(); // requires userinterface... 

		Serial.println(F("------- RESARTING--------"));
		Serial.print(F("DHCP result: "));
		Serial.println(dhcpresult);
		reinit = true;

		} else if (_HTTP->arg("enable-dhcp") == "off") {
      save_flag = true; 
			_DHCP = false;

			//printdiagnositics();

			bool ok = true;

		if(_HTTP->hasArg("setSTAsetip") ) {
			localIP = StringtoIP(_HTTP->arg("setSTAsetip"));
			Serial.print(F("IP = "));
			Serial.println(localIP);
		} else ok = false; 
		
		if(_HTTP->hasArg("setSTAsetgw") ) {
			gateway = StringtoIP(_HTTP->arg("setSTAsetgw"));
			Serial.print(F("gateway = "));
			Serial.println(gateway);
		} else ok = false; 

		if(_HTTP->hasArg("setSTAsetsn") ) {
			subnet =  StringtoIP(_HTTP->arg("setSTAsetsn"));
			Serial.print(F("subnet = "));
			Serial.println(subnet);
		} else ok = false; 

		if (ok) {
			WiFi.config(localIP, gateway, subnet);
			reinit = true;
			}

		} 

		if (reinit && _HTTP->arg("enable-STA") == "on") Wifistart();

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

if ( _HTTP->hasArg("enable-AP")) {
    save_flag = true; 

    WiFiMode mode = WiFi.getMode();
    // WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3
    //if (mode == WIFI_STA || mode == WIFI_AP_STA ) STAstate = "ENABLED"; else STAstate = "DISABLED";
    //if (mode == WIFI_AP || mode == WIFI_AP_STA ) APstate = "ENABLED"; else APstate = "DISABLED";

  		if (_HTTP->arg("setAPsetssid").length() != 0 ) { 

        if (_APssid) { free( (void*)_APssid ); _APssid = NULL; } 
			  _APssid = strdup((const char *)_HTTP->arg("setAPsetssid").c_str()); 
		}; 

  		if (_HTTP->arg("setAPsetpass").length() != 0 && _HTTP->arg("setAPsetpass").length() < 63) { 
			//_APpass = (const char *)_HTTP->arg("setAPsetpass").c_str(); 
			
        if (_APpass) { free( (void*)_APpass); _APpass = NULL;  };  // free memory if AP pass has been allocated. 

        _APpass = strdup( (const char *)_HTTP->arg("setAPsetpass").c_str());

		} else if (_HTTP->arg("setAPsetpass").length() == 0 && _APpass) {

        if (_APpass) { free( (void*)_APpass); _APpass = NULL;  };  // free memory if AP pass has been allocated. 
 
		};



	if (_HTTP->arg("enable-AP") == "on") { 

		  uint8_t channel = _HTTP->arg("setAPsetchannel").toInt(); 		
		  if (channel > 13) channel = 13; 
		  _APchannel = channel; 

		  Serial.print(F("Enable AP channel: "));
		  Serial.println(_APchannel); 

	    if (mode == WIFI_STA ) WiFi.mode(WIFI_AP_STA);

      InitialiseSoftAP(); 
	    printdiagnositics();

		} else if (_HTTP->arg("enable-AP") == "off") {

		  Serial.println(F("Disable AP"));

	    if (mode == WIFI_AP_STA || mode == WIFI_AP ) WiFi.mode(WIFI_STA);

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

	if (_HTTP->hasArg("deviceid")) { 
		if (_HTTP->arg("deviceid").length() != 0 && _HTTP->arg("deviceid").length() < 32) {			
        save_flag = true; 

//			if (_host) free( (void*)_host);

        if (strcmp(_host, _APssid) == 0) {  
            if (_APssid) { free( (void*)_APssid ); _APssid = NULL; } 
              _APssid = strdup ( (const char *)_HTTP->arg("deviceid").c_str() ); 
            } 
            
        if (_host) { free( (void*)_host ); _host = NULL; } 
			  _host = strdup ( (const char *)_HTTP->arg("deviceid").c_str() ); 
             //  might need to add in here, wifireinting... 
        WiFi.hostname(_host); 
        InitialiseFeatures(); 
        InitialiseSoftAP(); 

		}

	}
/*------------------------------------------------------------------------------------------------------------------

                                 Restart wifi
------------------------------------------------------------------------------------------------------------------*/

  if ( _HTTP->arg("plain") == "resetwifi" ) {

    WiFi.disconnect(); 
    ESP.restart();

  }

/*------------------------------------------------------------------------------------------------------------------

                                 AP config
------------------------------------------------------------------------------------------------------------------*/
  if ( _HTTP->hasArg("otaenable") ) {
    save_flag = true; 

    bool command = (_HTTP->arg("otaenable") == "on") ? true: false; 

    if (command != _OTAenabled) {
      _OTAenabled = command; 
      long before = ESP.getFreeHeap(); 

      if (_OTAenabled) {

        InitialiseFeatures(); 
        String insert = F("OTA Enabled, heap used: %u \n");
        Serial.printf(insert.c_str(), before - ESP.getFreeHeap() );

      } else  {

        if (ota_server) { delete ota_server; ota_server = NULL; } ;
        
        Serial.printf( "OTA deactivated, heap reclaimed: %u \n", ESP.getFreeHeap() - before );

      }

    }
  } // end of OTA enable 

/*
ARG: 0, "enable-AP" = "on"
ARG: 1, "setAPsetip" = "0.0.0.0"
ARG: 2, "setAPsetmac" = "1A%3AFE%3A34%3AA4%3A4C%3A73"
*/


 _HTTP->send(200, "text", "OK");  // return ok to speed up AJAX stuff 




}
