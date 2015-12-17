# ESPmanager
Full wifi and OTA manager for ESP8266 Arduino IDE


Super early stuff.  Likely to be bugs.

Uses Jquerymobile and AJAX to run everything, with cache control should mean fairly snappy handling. 

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

There is ONE bug i've not fixed, not sure if it is ESP or JS related.  
The first time you open the page, when it must download everything, it seems to bork and fail. 
If you hit refresh it then works fine.. this only happens on an empty cache.  
If someone knows how to fix this... i suspect it is the browser putting out too many file requests... 
but im not sure... 

Feedback welcome... 
