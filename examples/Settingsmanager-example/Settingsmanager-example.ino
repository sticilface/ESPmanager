/*-------------------------------------------------------------------------------------------------------


							Example config software.. 

Uses around 2K heap 
Takes care of OTA
Password management

--------------------------------------------------------------------------------------------------------*/

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>

                      
#include <Settingsmanager.h>


const char * host = "Melvide-ESP";
const char * ssid = "SKY";
const char * pass = "wellcometrust";


#define DBG_OUTPUT_PORT Serial


ESP8266WebServer HTTP(80);
Settingsmanager settings(&HTTP, &SPIFFS, host, ssid, pass);


void setup() {

 
  delay(500);

  Serial.begin(115200);


  SPIFFS.begin();
  
  delay(500);

  Serial.println("");
  Serial.println(F("Example ESPconfig"));

  Serial.printf("Sketch size: %u\n", ESP.getSketchSize());
  Serial.printf("Free size: %u\n", ESP.getFreeSketchSpace());

  Serial.println("SPIFFS");
    {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {    
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      DBG_OUTPUT_PORT.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    DBG_OUTPUT_PORT.printf("\n");
  }

  

  settings.begin(); 


  if(WiFi.waitForConnectResult() == WL_CONNECTED || WiFi.getMode() == WIFI_AP_STA){



//SERVER INIT
  //list directory
  HTTP.on("/list", HTTP_GET, handleFileList);
  //load editor
  HTTP.on("/edit", HTTP_GET, [](){
    if(!handleFileRead("/edit.htm")) HTTP.send(404, "text/plain", "FileNotFound");
  });
  //create file
  HTTP.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  HTTP.on("/edit", HTTP_DELETE, handleFileDelete);
  //called after file upload
  HTTP.on("/edit", HTTP_POST, [](){ HTTP.send(200, "text/plain", ""); });
  //called when a file is received inside POST data
  HTTP.onFileUpload(handleFileUpdate);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  HTTP.onNotFound([](){
    if(!handleFileRead(HTTP.uri()))
      HTTP.send(404, "text/plain", "FileNotFound");
  });

    HTTP.begin();
    Serial.println(F("Services Started...."));
  } else {
    Serial.println(F("Services NOT Started...."));
  }

  Serial.print(F("Free Heap: "));
  Serial.println(ESP.getFreeHeap());



}


void loop() {

  settings.handle();

  HTTP.handleClient();
  
  yield(); 


 static uint32_t heap_timer = 0;

 if (millis() - heap_timer > 10000) {
  Serial.print("Heap = ");
  Serial.println(ESP.getFreeHeap());
  heap_timer = millis(); 
 }
  

}







