[![Build Status](https://travis-ci.org/sticilface/ESPmanager.svg?branch=master)](https://travis-ci.org/sticilface/ESPmanager)

## ESPmanager
Full wifi and OTA manager for ESP8266 Arduino IDE

Uses Jquerymobile and AJAX to run everything, with AppCache should mean fairly snappy handling. 

V2 Released please see [Changelog] (https://github.com/sticilface/ESPmanager/ChangeLog.md) 
There are several changes that are breaking, and it is quite different. 
Many new feature, including single URL updating of sketch and SPIFFS. 

## Dependancies
+ [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
+ [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP) 
+ [ArduinoJson](https://github.com/bblanchon/ArduinoJson)

## Features 
This project uses SPIFFS, JqueryMobile, AJAX, ArduinoJson and handles lots of aspects of ESP8266 management. Including OTA, WiFi Networks, Setting device name, enabling mDNS, you can upload files to SPIFFS, format SPIFFS, reboot the device, enable/disable the AP, enable/disable Station mode, set AP password, channel (if not in station mode), set the MAC address of STA. There is an About page that gives loads of variables regarding WiFi, SPIFFS, uptime, etc etc... 

## Instructions. 

1. Download to your libraries Folder for Arduino.  As per normal lib. 
2. Open the example sketch located in the examples folder - ESPmanager-example.ino.  
3. Add in your WiFi details if wanted.  This will provide sketch credentials.  This is not necessary. 
4. Upload sketch data directory as shown [here](http://esp8266.github.io/Arduino/versions/2.3.0/doc/filesystem.html#uploading-files-to-file-system)   
5. Upload the sketch via serial or OTA if previous sketch supports it. 
6. Reboot the ESP.  
7. Now visit http://X.X.X.X/espman/ and it should work.  (the trailing / is important for jquery to fetch the data from the ESP. )  
8. Save settings by pressing Save. 

Feedback welcome... 
