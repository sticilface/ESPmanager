/*-------------------------------------------------------------------------------------------------------


							Example config software..

BeerWare Licence, just give due credits

--------------------------------------------------------------------------------------------------------*/
#include <FS.h> //  Settings saved to SPIFFS
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>

#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h> // required for settings file to make it readable

#include <ESPmanager.h>




ESP8266WebServer HTTP(80);

ESPmanager settings(HTTP, SPIFFS);

//  Or specify devicename, SSID, PASS
// ESPmanager settings(HTTP, SPIFFS, "ESPManager", "SSID", "PASS");

void setup()
{

	Serial.begin(115200);
	SPIFFS.begin();

	Serial.println("");
	Serial.println(F("Example ESPconfig"));

	Serial.printf("Sketch size: %u\n", ESP.getSketchSize());
	Serial.printf("Free size: %u\n", ESP.getFreeSketchSpace());

	settings.begin();

	HTTP.begin();

	Serial.print(F("Free Heap: "));
	Serial.println(ESP.getFreeHeap());

}


void loop()
{


	HTTP.handleClient();

	yield();

	settings.handle();

}








