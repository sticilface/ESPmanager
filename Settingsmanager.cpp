
#include "Settingsmanager.h"

extern "C" {
#include "user_interface.h"
}


Settingsmanager::Settingsmanager(ESP8266WebServer * HTTP, fs::FS * fs, const char * host, const char * ssid , const char * pass) {

	_HTTP = HTTP; 
	_fs = fs; 
	//This sets the default fallback options... not in use yet
	if (host && ( strlen(host) < 32) ) { _host = strdup(host); } 
	if (ssid && ( strlen(ssid) < 32) ) { _ssid = ssid; }
	if (pass && ( strlen(pass) < 63) ) { _pass = pass; }

}

Settingsmanager::~Settingsmanager() {
	if (_APpass) free (_APpass); 
	_APpass = NULL; 
	//if (_host) free ((void*)_host);
	_host = NULL;

  if (ota_server) { delete ota_server; ota_server = NULL; }; 
  if (_host) { free ( (void *) _host); _host = NULL; };
  if (_pass) { free ( (void *) _pass); _pass = NULL; };


}

void cache Settingsmanager::begin() {

	
	if(_host)  {
		if (WiFi.hostname(_host)) {
		Serial.print("Host Name Set: ");
		Serial.println(_host);
    }
	} else {
    _host = strdup((const char *)WiFi.hostname().c_str()); 
	}

	if (_APssid = "")
	{
		_APssid = WiFi.hostname();
	}

	//if (!_host) {
		//_host = (const char *)WiFi.hostname().c_str();
	//	_host = (const char *)malloc( WiFi.hostname().length() + 1); 
	//	strcpy ( (char*)_host, (const char *)WiFi.hostname().c_str());
	//}


   if(!Wifistart() ) {
    WiFiMode(WIFI_AP_STA);
    WiFi.softAP((const char *)_APssid.c_str(), _APpass, (int)_APchannel, (int)_APhidden);
    Serial.println(F("WiFi Failed:  Starting AP")); 
   }

    printdiagnositics(); 

    InitialiseFeatures(); 

    _HTTP->on("/data.esp", std::bind(&Settingsmanager::HandleDataRequest, this)); 
    _HTTP->serveStatic("/config.htm", *_fs, "/config.htm"); 
	   
    
  
}

void cache Settingsmanager::handle() {

 if(ota_server) ota_server->handle();

  //
  //
  // Async handle WiFi reconnects... 
  //
  //

}

void cache Settingsmanager::InitialiseFeatures() {

  if (_OTAenabled) {
    if (_OTAhost) delete _OTAhost;
    _OTAhost = NULL; 
    _OTAhost = new char[strlen(_host) + 2];
    strcpy(_OTAhost, _host);
    _OTAhost[strlen(_host)] = '-';
    _OTAhost[strlen(_host)+1] = 0;
    //Serial.printf("OTA host = %s\n", _OTAhost);
    if (ota_server) { delete ota_server; ota_server = NULL; };  
    ota_server = new ArduinoOTA(_OTAhost, 8266, true);
    ota_server->setup();
  }
    
    WiFi.hostname(_host);

}

 void cache Settingsmanager::InitialiseSoftAP() {

      WiFiMode mode = WiFi.getMode();

      if (mode == WIFI_AP_STA || mode == WIFI_AP) {

        WiFi.softAP((const char *)_APssid.c_str(), _APpass, _APchannel, _APhidden);

      }

 }

bool cache Settingsmanager::Wifistart() {
  //WiFiMode(WIFI_AP_STA);
  //WiFi.begin(_ssid,_pass);

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
                      WiFi.softAP(_host);                       
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

	Serial.println(F("------------------"));
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



   String mmm = F("ARG: %u, \"%s\" = \"%s\"\n");
        for (uint8_t i = 0; i <  _HTTP->args(); i++ ) {
          Serial.printf(mmm.c_str(), i,  _HTTP->argName(i).c_str(),  _HTTP->arg(i).c_str());
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

	

	//_wifinetworksfound = WiFi.scanNetworks();


    String connected; 

    buf = F("{");

    if (_wifinetworksfound) {

    buf +=  F("\"networks\": [");

    for (int i = 0; i < _wifinetworksfound; ++i)
  {

    //    if(WiFi.status() == WL_CONNECTED && strcmp( config.ssid, WiFi.SSID(i).c_str()) == 0)
    if(WiFi.status() == WL_CONNECTED && WiFi.SSID(i) == WiFi.SSID()) { connected = F("true"); } else { connected = F("false"); }

    uint8_t encryptiontype = WiFi.encryptionType(i);
    String encryptionString; 

    switch (encryptiontype) {
         case ENC_TYPE_NONE:
          encryptionString = F("OPEN");
          break;
         case ENC_TYPE_WEP:
          encryptionString = F("WEP");
          break;
         case ENC_TYPE_TKIP:
          encryptionString = F("WPA_PSK");
          break;
         case ENC_TYPE_CCMP:
          encryptionString = F("WPA2_PSK");
          break;
         case ENC_TYPE_AUTO:
          encryptionString = F("AUTO");
          break;
      }

    if (i > 0) buf += ",";
    buf += "{\"ssid\":\"" + String(WiFi.SSID(i)) + "\",\"rssi\":\"" + String(WiFi.RSSI(i)) +"\",\"connected\":\"" + connected 
    + "\",\"channel\":\"" + String(WiFi.channel(i)) +"\",\"encyrpted\":\"" + encryptionString +"\",\"BSSID\":\"" + WiFi.BSSIDstr(i) +"\"}"; 
    }

    //if (_wifinetworksfound == 0 ) buf += F("{\"ssid\":\"\",\"rssi\":\"\",\"connected\":\"\",\"channel\":\"\",\"encyrpted\":\"\",\"BSSID\":\"\"}");
 
     buf += F("],");
	
   }

    WiFiMode mode = WiFi.getMode();

    String APstate = (mode == WIFI_AP || mode == WIFI_AP_STA ) ? "ENABLED" : "DISABLED";
    String STAstate = (mode == WIFI_STA || mode == WIFI_AP_STA ) ? "ENABLED" : "DISABLED";
    String dhcpstate = (_DHCP)? "on" : "off";
    String isHidden = (_APhidden)? "true" : "false"; 
    String APip = (WiFi.softAPIP()[0] == 0 && WiFi.softAPIP()[1] == 0 && WiFi.softAPIP()[2] == 0 && WiFi.softAPIP()[3] == 0)? F("192.168.4.1") : IPtoString(WiFi.softAPIP()); 

    String otastate = (_OTAenabled)? "enabled" : "disabled";
    // WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA
    // Serial.print("enable/disable debug: Mode = ");
    // Serial.println(mode);

   // if (mode == WIFI_STA || mode == WIFI_AP_STA ) STAstate = "ENABLED"; else STAstate = "DISABLED";
   // if (mode == WIFI_AP || mode == WIFI_AP_STA ) APstate = "ENABLED"; else APstate = "DISABLED";
    buf += F("\"general\":{");
    buf += "\"deviceid\":\"" + String(_host) + "\",";    
    buf += "\"otaenabled\":\"" + otastate + "\"";

    buf += F("},");


    buf += F("\"STA\":{");
    buf += F("\"dhcp\":\""); 
    buf += dhcpstate + "\",";
    buf += F("\"state\":\"" );
    buf += STAstate + "\",";
    buf += F("\"IP\":\"" );
    buf += IPtoString(WiFi.localIP()) + "\",";
    buf += F("\"gateway\":\"" );
    buf += IPtoString(WiFi.gatewayIP()) + "\",";
    buf += F("\"subnet\":\"" );
    buf += IPtoString(WiFi.subnetMask()) + "\",";
    buf += F("\"MAC\":\"" );
    buf += WiFi.macAddress() +"\"";
    //buf += "\"DNS\":\"" + WiFi.  DNS not implemeneted...

    buf += F("},");

    buf += F("\"AP\":{");
	buf += "\"ssid\":\"" + String(_APssid) + "\",";
    buf += "\"state\":\"" + APstate + "\",";
    buf += "\"IP\":\"" + APip + "\",";
    buf += "\"hidden\":\"" + isHidden + "\",";
    if(_APpass) buf += "\"password\":\"" + String(_APpass) + "\","; else 
    			buf += "\"password\":\"\",";
    buf += "\"channel\":\"" + String(_APchannel) + "\",";
    buf += "\"MAC\":\"" + WiFi.softAPmacAddress() + "\"";
    buf += "}}";

   _HTTP->send(200, "text/json", buf);
  	
  	WiFi.scanDelete();
  	_wifinetworksfound = 0;


  return; 


  }

/*------------------------------------------------------------------------------------------------------------------
                                  Send about page details... 
------------------------------------------------------------------------------------------------------------------*/
  if ( _HTTP->arg("plain") == "AboutPage") {
  	Serial.print("Heap = ");
  	Serial.println(ESP.getFreeHeap());
    const uint8_t bufsize = 10; 
    int sec = millis() / 1000;
    int min = sec / 60;
    int hr = min / 60;
    int Vcc = analogRead(A0); 
    
    char Up_time[bufsize]; 
    snprintf ( Up_time, bufsize, "%02d:%02d:%02d", hr, min % 60, sec % 60 );

    buf = "{";
    buf += F("\"version_var\":\""); 
    buf += String(version) + "\","; // to remove this... change to host
    buf += F("\"compiletime_var\":\""); 
    buf += String(_compile_date_time) + "\",";
    buf += F("\"sdk_var\":\"");
    buf += String(ESP.getSdkVersion()) + "\",";
    buf += F("\"heap_var\":\"");
    buf += String(ESP.getFreeHeap()) + "\",";  
    buf += F("\"flashsize_var\":\"");
    buf += String(ESP.getFlashChipSize()) + "\",";   
    buf += F("\"chipid_var\":\"");
    buf += String(ESP.getChipId()) + "\",";
    buf += F("\"flashid_var\":\"");
    buf += String(ESP.getFlashChipId()) + "\",";   
    buf += F("\"sketchsize_var\":\"");
    buf += String(ESP.getSketchSize()) + "\",";
    buf += F("\"freespace_var\":\"");
    buf += String(ESP.getFreeSketchSpace()) + "\",";    
    buf += F("\"millis_var\":\"");
    buf += String(millis()) + "\",";   
    buf += F("\"uptime_var\":\"");
    buf += String(Up_time) + "\",";
    buf += F("\"vcc_var\":\"");
    buf += String(ESP.getVcc() ) + "\",";
    buf += F("\"rssi_var\":\"");
    buf += String(WiFi.RSSI()) + "\",";
    buf += F("\"cpu_var\":\"");
    buf += String(ESP.getCpuFreqMHz()) + "\"";
    buf += "}";

     _HTTP->send(200, "text/json", buf);
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
        
        WiFiresult = -1; 
        char old_ssid[33];
        char old_pass[33];
        strcpy(old_pass, (const char *)WiFi.psk().c_str() ) ; 
        strcpy(old_ssid,  (const char *)WiFi.SSID().c_str() ); 

        _HTTP->send(200, "text", "accepted"); // important as it defines entry to the wait loop on client

        Serial.print(F("Disconnecting.."));

        WiFi.disconnect();
        Serial.print(F("done \nInit..."));

        _ssid = strdup((const char *)_HTTP->arg("ssid").c_str()); 
        _pass = strdup((const char *)_HTTP->arg("pass").c_str() );

/*

        ToDo....  put in check for OPEN networks that enables the AP for 5 mins.... 

*/


        WiFi.begin( _ssid,  _pass);

        
        uint8_t i = 0;

        while (WiFi.status() != WL_CONNECTED && _HTTP->arg("removesaftey") == "No") {
            delay(500);
              i++;
            Serial.print(".");
          if (i == 30) { 

          Serial.print(F("Unable to join network...restore old settings")); 
          WiFiresult = 2;

          WiFi.disconnect();
          WiFi.begin(old_ssid, old_pass);

              while (WiFi.status() != WL_CONNECTED ) {
                  delay(500);
                  Serial.print(".");          
              } ;
              Serial.print(F("done")); 
        break; 
        }

      }

      Serial.printf("\nconnected: SSID = %s, pass = %s\n", WiFi.SSID().c_str(), WiFi.psk().c_str()); 
        if (WiFiresult == 0) WiFiresult = 1;

        return; 

          }
        }
      } 
    } 

if ( _HTTP->arg("plain") == "WiFiresult") {   _HTTP->send(200, "text", String(WiFiresult)); return ; }
/*------------------------------------------------------------------------------------------------------------------

                                 STA config
------------------------------------------------------------------------------------------------------------------*/

if ( _HTTP->hasArg("enable-STA")) {

	IPAddress localIP, gateway, subnet; 
    WiFiMode mode = WiFi.getMode();
    bool reinit = false; 

	if (_HTTP->arg("enable-STA") == "off" && (mode == WIFI_STA || mode == WIFI_AP_STA )) {
		WiFi.mode(WIFI_AP); // always fall back to AP mode... 
		WiFi.softAP((const char *)_APssid.c_str(), _APpass, (int)_APchannel, (int)_APhidden);
		Serial.println(F("STA-disabled: falling back to AP mode."));
	} else if (_HTTP->arg("enable-STA") == "on" && mode == WIFI_AP) {
		WiFi.mode(WIFI_AP_STA); 
		Serial.println(F("Enabling STA mode"));
		reinit = true; 
	}

	


	 if (_HTTP->arg("enable-dhcp") == "on") { 

		_DHCP = true;

		bool dhcpresult = wifi_station_dhcpc_start(); // requires userinterface... 

		Serial.println(F("------- RESARTING--------"));
		Serial.print(F("DHCP result: "));
		Serial.println(dhcpresult);
		reinit = true;

		} else if (_HTTP->arg("enable-dhcp") == "off") {

			_DHCP = false;

			//printdiagnositics();

			bool ok = true;

		if(_HTTP->hasArg("setSTAsetip") ) {
			localIP = StringtoIP(_HTTP->arg("setSTAsetip"));
			Serial.print("IP = ");
			Serial.println(localIP);
		} else ok = false; 
		
		if(_HTTP->hasArg("setSTAsetgw") ) {
			gateway = StringtoIP(_HTTP->arg("setSTAsetgw"));
			Serial.print("gateway = ");
			Serial.println(gateway);
		} else ok = false; 

		if(_HTTP->hasArg("setSTAsetsn") ) {
			subnet =  StringtoIP(_HTTP->arg("setSTAsetsn"));
			Serial.print("subnet = ");
			Serial.println(subnet);
		} else ok = false; 

		if (ok) {
			WiFi.config(localIP, gateway, subnet);
			reinit = true;
			}

		} 

		if (reinit && _HTTP->arg("enable-STA") == "on") Wifistart();

		printdiagnositics();

	//_HTTP->on("/data.esp", std::bind(&Settingsmanager::HandleDataRequest, this)); 
    //_HTTP->serveStatic("/config.htm", *_fs, "/config.htm"); 

	//	_HTTP->begin();

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

    WiFiMode mode = WiFi.getMode();
    // WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3
    //if (mode == WIFI_STA || mode == WIFI_AP_STA ) STAstate = "ENABLED"; else STAstate = "DISABLED";
    //if (mode == WIFI_AP || mode == WIFI_AP_STA ) APstate = "ENABLED"; else APstate = "DISABLED";

  		if (_HTTP->arg("setAPsetssid").length() != 0 ) { 
			_APssid = _HTTP->arg("setAPsetssid"); 
		};

  		if (_HTTP->arg("setAPsetpass").length() != 0 && _HTTP->arg("setAPsetpass").length() < 63) { 
			//_APpass = (const char *)_HTTP->arg("setAPsetpass").c_str(); 
			if (!_APpass) {
				_APpass = (char*)malloc(64);
			}
			strcpy(_APpass, (const char *)_HTTP->arg("setAPsetpass").c_str());
		} else if (_HTTP->arg("setAPsetpass").length() == 0 && _APpass) {
			free (_APpass);
			_APpass = NULL; 
		};



	if (_HTTP->arg("enable-AP") == "on") { 

		uint8_t channel = _HTTP->arg("setAPsetchannel").toInt(); 		
		if (channel > 13) channel = 13; 
		_APchannel = channel; 

		Serial.print(F("Enable AP channel: "));
		Serial.println(_APchannel); 

	    if (mode == WIFI_STA ) WiFi.mode(WIFI_AP_STA);

	    // if (_HTTP->arg("setAPsetpass").length() == 0) {
	    // 	// 		    void softAP(const char* ssid, const char* passphrase, int channel = 1, int ssid_hidden = 0);
	    // 	WiFi.softAP((const char *)_APssid.c_str(), NULL , (int)_APchannel, (int)_APhidden);
	    // 	Serial.println(F("Password not set for SSID"));
	    // } else {
	    // 	WiFi.softAP((const char *)_APssid.c_str(), _APpass, (int)_APchannel, (int)_APhidden);
	    // 	Serial.printf("Password applied for SSID %s\n", _APpass);

	    // }

      InitialiseSoftAP(); 


	    printdiagnositics();

		} else if (_HTTP->arg("enable-AP") == "off") {

		Serial.println("Disable AP");
	    if (mode == WIFI_AP_STA || mode == WIFI_AP ) WiFi.mode(WIFI_STA);
	    
	    printdiagnositics();

		} 
//

	} //  end of enable-AP 

/*------------------------------------------------------------------------------------------------------------------

                                 Device Name
------------------------------------------------------------------------------------------------------------------*/

	if (_HTTP->hasArg("deviceid")) { 
		if (_HTTP->arg("deviceid").length() != 0 && _HTTP->arg("deviceid").length() < 32) {			
			if (_host) free( (void*)_host);

        if (strcmp(_host, _APssid.c_str()) == 0) { _APssid = _HTTP->arg("deviceid"); } 

			_host = strdup ( (const char *)_HTTP->arg("deviceid").c_str() ); 


      WiFiMode mode = WiFi.getMode();


      InitialiseFeatures(); 

      InitialiseSoftAP(); 


			// if (!_host) {
			// 	_host = (const char *)malloc( _HTTP-arg("deviceid").length() + 1); 
			// } else {
			// 	free ((void*)_host);
			// 	_host = NULL; 
			// 	_host = (const char *)malloc( _HTTP-arg("deviceid").length() + 1); 
			// }


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

    bool command = (_HTTP->arg("otaenable") == "on") ? true: false; 

    if (command != _OTAenabled) {
      _OTAenabled = command; 
        long before = ESP.getFreeHeap(); 

      if (_OTAenabled) {

        InitialiseFeatures(); 
        Serial.printf("OTA deactivated, heap reclaimed: %u \n", before - ESP.getFreeHeap() );

      } else  {
        delete ota_server;
        ota_server = NULL; 
        delete _OTAhost;
        _OTAhost = NULL; 
        Serial.printf("OTA deactivated, heap reclaimed: %u \n", ESP.getFreeHeap() - before );

      }

    }
  } // end of OTA enable 

/*
ARG: 0, "enable-AP" = "on"
ARG: 1, "setAPsetip" = "0.0.0.0"
ARG: 2, "setAPsetmac" = "1A%3AFE%3A34%3AA4%3A4C%3A73"
*/


 _HTTP->send(200, "text", "OK");

}