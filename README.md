[![Build Status](https://travis-ci.org/sticilface/ESPmanager.svg?branch=master)](https://travis-ci.org/sticilface/ESPmanager)

## ESPmanager
Full wifi and OTA manager for ESP8266 Arduino IDE, with integrated update manager to perform autoupdating over HTTP. 

Uses Jquerymobile and AJAX to run everything, except the PROGMEM served emergency AP. 

V3.1alpha
+ [Changes](https://github.com/sticilface/ESPmanager/blob/V3.1/ChangeLog.md#v30)

## Cool new features:
+ Mostly stuff removed.  Just because you can does not mean you should. 
+ Setup is now via /espman/quick, simple, no jquery, DOES NOT require a working filesystem.  
+ Authentication
+ optional PROGMEM served JQUERY
+ Fixed loss of STA handling. 
+ ArduinoJSON V6
+ LittleFS

V2.2 Released please see 
+ [Changes](https://github.com/sticilface/espmanager/blob/master/ChangeLog.md) 
+ [Changes](https://github.com/sticilface/espmanager/blob/master) 

## Cool new features:
+ Syslogging - not entirely up and running yet. (REMOVED)
+ Saving crashdump to SPIFFS file (REMOVED)
 
## Dependancies
+ [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
+ [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP) 
+ [ArduinoJson](https://github.com/bblanchon/ArduinoJson)

## Features 
This project uses LittleFS, JqueryMobile, AJAX, ArduinoJson and handles lots of aspects of ESP8266 management. Including OTA, WiFi Networks, Setting device name, enabling mDNS, you can upload files to FS, format FS, reboot the device, enable/disable the AP, enable/disable Station mode, set AP password, channel (if not in station mode). There is an About page that gives loads of variables regarding WiFi, FS, uptime, etc etc.

## Instructions 

- Download to your libraries Folder for Arduino.  As per normal lib. 
- Open the example sketch located in the examples folder - ESPmanager-example.ino.   
- Upload sketch data directory as shown [here](http://esp8266.github.io/Arduino/versions/2.3.0/doc/filesystem.html#uploading-files-to-file-system)   
- Upload the sketch via serial.  
- Reboot the ESP.  
- Join the ESP AP, default password is `esprocks`, user `admin` pass `esprocks` 
- Now visit any URL (www.a.com) and it will redirect you to the setup wizard.  Follow the instructions....... then click launch. 
- One gotya might be if the wifi channel you are joining is different to the wifi channel the AP is in.  In which case a warning will popup, and after 10 seconds or so your computer will disconnect and most likely reconnect to your home wifi.  You need to reconnect to the ESP access point, for the config to continue.  This is expected behaviour!  
- Visit http://x.x.x.x/espman/ if you want to change further settings. 

Feedback welcome... 
