/*-------------------------------------------------------------------------------------------------------


							Example config software.. 

Uses around 2K heap 
Takes care of OTA
Password management

--------------------------------------------------------------------------------------------------------*/

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>

#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <time.h>
                      
#include <ESPmanager.h>
#include <FSBrowser.h>

//#define MANAGE_DYNAMIC





ESP8266WebServer HTTP(80);
FSBrowser fsbrowser(HTTP); 

#ifdef MANAGE_DYNAMIC
  ESPmanager * settings = NULL; 
#else
  ESPmanager settings(HTTP, SPIFFS);
#endif


void setup() {

  Serial.begin(115200);
  SPIFFS.begin();
  //Serial.setDebugOutput(true);
  delay(500);

  Serial.println("");
  Serial.println(F("Example ESPconfig"));

  Serial.printf("Sketch size: %u\n", ESP.getSketchSize());
  Serial.printf("Free size: %u\n", ESP.getFreeSketchSpace());




#ifndef MANAGE_DYNAMIC
  settings.begin(); 
#endif

  fsbrowser.begin(); 

  HTTP.begin();

  Serial.print(F("Free Heap: "));
  Serial.println(ESP.getFreeHeap());

}


void loop() {


  HTTP.handleClient();
  
  yield(); 


 static uint32_t heap_timer = 0;

 if (millis() - heap_timer > 60000) {
  Serial.print("Heap = ");
  Serial.println(ESP.getFreeHeap());
  heap_timer = millis(); 
 }
  
#ifdef MANAGE_DYNAMIC

 if (millis() > 1000) {
  static bool triggered = false; 
  if (!triggered) {
    Serial.println("Settingsmanager started..");
      uint32_t before = ESP.getFreeHeap(); 
      settings = new ESPmanager(HTTP); 
      settings->begin(); 
      Serial.printf("Heap used by settings Manager: %u\n", before - ESP.getFreeHeap());      
      triggered = true; 
  }

  if (settings) settings->handle();

  }

#else

settings.handle(); 

#endif



}








