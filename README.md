[![Build Status](https://travis-ci.org/sticilface/ESPmanager.svg?branch=master)](https://travis-ci.org/sticilface/ESPmanager)

## ESPmanager
Full wifi and OTA manager for ESP8266 Arduino IDE

Uses Jquerymobile and AJAX to run everything, with AppCache should mean fairly snappy handling. 

V2.1 Released please see [Changelog](https://github.com/sticilface/ESPmanager/blob/master/ChangeLog.md)  
There are several changes that are breaking, and it is quite different. 
Many new feature, including single URL updating of sketch and SPIFFS. 

## Dependancies
+ [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
+ [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP) 
+ [ArduinoJson](https://github.com/bblanchon/ArduinoJson)

## Features 
This project uses SPIFFS, JqueryMobile, AJAX, ArduinoJson and handles lots of aspects of ESP8266 management. Including OTA, WiFi Networks, Setting device name, enabling mDNS, you can upload files to SPIFFS, format SPIFFS, reboot the device, enable/disable the AP, enable/disable Station mode, set AP password, channel (if not in station mode), set the MAC address of STA. There is an About page that gives loads of variables regarding WiFi, SPIFFS, uptime, etc etc.  Captive Portal now works on first boot. 

## Instructions. 

- Download to your libraries Folder for Arduino.  As per normal lib. 
- Open the example sketch located in the examples folder - ESPmanager-example.ino.   
- Upload sketch data directory as shown [here](http://esp8266.github.io/Arduino/versions/2.3.0/doc/filesystem.html#uploading-files-to-file-system)   
- Upload the sketch via serial.  
- Reboot the ESP.  
- Join the ESP AP. 
- Now visit any URL (www.a.com) and it should redirect you to the setup wizard.  Follow the iunstruction. 
- Visit http://x.x.x.x/espman/ if you want to change further settings. 

Feedback welcome... 
