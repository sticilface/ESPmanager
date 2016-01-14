# ESPmanager
Full wifi and OTA manager for ESP8266 Arduino IDE


Super early stuff.  Likely to be bugs.

Uses Jquerymobile and AJAX to run everything, with cache control should mean fairly snappy handling. 

Requirements <br>
1) ESP8266 & Arduino IDE <br>
2) The following header files, must be in your sketch. <br>
```
#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson 

#include <ESPmanager.h>
```

Features <br>
This project uses SPIFFS, JqueryMobile, AJAX, ArduinoJson and handles lots of aspects of ESP8266 management. Including OTA, WiFi Networks, Setting device name, enabling mDNS, you can upload files to SPIFFS, format SPIFFS, reboot the device, enable/disable the AP, enable/disable Station mode, set AP password, channel (if not in station mode), set the MAC address of both AP and STA. There is an About page that gives loads of variables regarding WiFi, SPIFFS, uptime, etc etc... 

Instructions. 

1) Download to your libraries Folder for Arduino <br>
2) Open the example sketch located in the examples folder - ESPmanager-example.ino <br>
3) Add in your WiFi details if you want (will allow you to upload the SPIFFS stuff via web)  <br>
4) If you do NOT put in you WiFi details, you must upload the data directoy to SPIFFS. <br>
5) If your ESP now connects to your WiFi network, put the IP in this command <br>
 ``for file in `ls -A1`; do curl -F "file=@$PWD/$file" X.X.X.X/espman/upload; done `` <br>
  whilst in the data directory of the example. <br>
6) Reboot the ESP.. it will copy the required files to a SPIFFS folder called espman <br>
7) Now visit http://X.X.X.X/espman and it should work... <br>



Feedback welcome... 
