# ChangeLog

## To Do
 - Add multi WiFi implementaion to search for and join a number of WiFi Networks. 

## V3.0
- Major rework, optimised for efficiency, code size and actual utility. 
- LittleFS as standard. 
- ArduinoJson V6 as standard. 
- Authentication added.  default user:admin, pass:esprocks. 
- Authentication can only be set in the sketch, must be called after begin().
- WiFi AP and STA logic completely overhauled with all the unnecesary complexity removed. 
- If WiFi STA is lost it just tries to reconnect. Every second if only STA active, every 60 seconds if AP is active. 
- If rebooted and there is NO access to device ie. no AP and no STA it will make an AP. If WiFi STA is configured it will keep attempting to connect. 
- Added a simple PROGMEM webpage at /espman/quick to be served in AP mode to connect to AP.  This removes any dependencey on jquery. 
- Added a staticHandler web class that allows flexibility of how static web pages are served.  SPIFFS/PROGMEM/REDIRECT
- WiFi logic now uses the correct callback API - no polling
- new RTC class that allows structs/arrays to be written to the RTC. 
- sketch MD5 is now written to RTC and compared on boot so the sketch knows when it has been upgraded, this allows the dreaded wifi config crash to be handled correctly.  no more rebooting after an upgrade. 
- autoconnect and autoreconnect options removed.  wiFi now just connects... otherwise what is the point of the lib, and autoreconnect is true. 


- Bugfixes
  - ESP device finder re-init logic
  - Fixed wifi reconnect.  I had not actually tested it corretly, now it works and it will reconnect to STA after power outage, or WiFi loss. 
  - Fixed incorrect jscript host path generation that required /espman/ as path.  now both work. 

- REMOVED
  - myString
  - crazy and pointless boot AP/STA modes. 
  - Removed the endless configuration options that were not used. 
  - Removed the endless settings reported in about.  just because you can.... 
  - many endless settings and config options removed. 
  - String definitions of errors removed.  just report the number, then look it up, or upload with debug enabled. 
  - crash dump removed
  - wifi wizard removed - nice but needs jquery which is bulky and makes a working FS a requirement.  
  - removed syslog - pointless
  - Captive portal.  just use /espman
  - Removed setting of MAC address


# V2.2
## New Features
- Save Crash Dump to SPIFFS.  Enabled by default, using `#define ESPMANAGER_SAVESTACK` uncomment in ESPmanager.h to stop. Will add it as a web setting at some point.  
- Created FlashWriter class, to save downloaded files to the OTA flash section before copying them to SPIFFS.  This is because as SPIFFS gets fragmented it can take 200ms to find free block, and the network times out.  Fixes https://github.com/sticilface/ESPmanager/issues/17.
- Added in Jsonupdater manifest example. https://github.com/sticilface/ESPmanager/issues/16.
- Add Error to String responses. `myString getError(code)`
- Big improvements to myString.  Lots of implicit convertions, use with FLASH strings, and PSTR(). Derived classes that allow printf and printf_P with convertion back to myString.  Addition of c_str(). move semantics to allow efficient copying of tempories. implicit convertion to String. 
- Added SysLog implementation.  Allows sending of data, and priorities and unique appNames to a syslog server from sketch.  Configured in web interface
```cpp
bool ESPManager::log(myString  msg); 
bool ESPManager::log(uint16_t pri, myString  msg); 
bool ESPManager::log(myString appName, myString  msg);
bool ESPManager::log(uint16_t pri, myString appName, myString  msg);
```
You can use const char *, printf and printf_P to send log messages as follows
```cpp
manager.log( "Simple Error" );
manager.log( myStringf( "MSG: %s, error code %u", "String Error", 120) ); 
manager.log( myStringf_P( PSTR("MSG: %s, error code %u"), "String Error", 120) );  //  String stored in FLASH. 
```
- Added manual SSID specification. Fixes https://github.com/sticilface/ESPmanager/issues/13

## Improvements
- Use `strnlen` instead of `strlen`.  Avoid buffer overruns. 
- Use ArduinoJson Stream as input, rather than read SPIFFS file into memory. 
- Remove myString to const char * comparison.  Use `myString(const char *)` instead. 
- add `myString.c_str()` method to mimic String impl.
- add `nullptr_t` initiliasation for myString so you can pass it `nullptr` to create empty string. 
- `events_send` replaces `events_printf` and `events_printf_P` you now pass it two myString instances which allows use of the derived classes myStringf and myStringf_P. 
- myStringf supports `__FlashStringHelper`
- All strings are now moved to PROGMEM.  Saves approx 400 Bytes of RAM. 

## Breaking Changes
- New default password for AP and setup wizard.  This is because by default the ESP was unsecured.  If your WiFi failed then anyone could join it, and potentially as the WiFi password is stored in plain text on the SPIFFS partition this could lead to a security problem.  Now the default password is `esprocks` and if the ESP is configured to join a WiFi network the AP password is the same as the STA password, unless you have specified an AP password. 


# V2.1

## New Features
- Added Captive Portal mode when device has not been configured previously.  Not active normally, just for the first config of the device to aid setup.
- New wizard to make device set up easier
- The Site is now just one file, which makes it a lot faster to load.  Does not require appCache now. 
- On upgrade it will automatically delete files from SPIFFS that contain a zipped .gz equivilent.  This is to stop the webserver from serving them instead of the .gz file which now is the default file.  This might bork your site if you depend on files that have the same name ie.. index.htm and index.htm.gz.  So I advise against that... let me know if this is a problem...

## Breaking changes
- Sketch credentials has been removed.  It is now no longer possible to specify a network to join from the sketch.  This just made things too complicated, with the wifi logic.
- autoConnect() has also been removed.  This does not really change things except that now you must configure the device first, even if it has a successfull config stored in the memory.  Now no matter what, when you flash ESPManager for the first time, you get the config wizard. 

## Bug fixes
- WifiScan now works fully async.  previously it held the request open so this would lead to crashes if the request was closed by lwip. 
- When joinging an WiFi network if the network is on a different channel the ESP waits for the client to reconnect to the AP.  This makes the process a lot smoother, as previosly the computer or the phone would loose the connection to the ESP, as it can only function on one channel.  For example if the AP on the ESP was channel 1, and you wanted to join a WiFi network on channel 3.  The ESP would change its AP channel to 3, and you would drop the AP connection to the ESP.  Now it waits for reconnect before proceeding. 
-Appcache has been removed.  No longer needed with single site files. 



# V2.0.0-rc1

## Breaking changes
- Old settings files are no longer compatible, on first boot it will restore connection to existing wifi, you must click SAVE button in top right to save settings.
- Had two methods for returning host, removed one.  const char * is no longer suitable as the buffer goes out of scope. So it now returns String. `String getHostname();`
- autochecking of SPIFFS files have been removed.  You must now upload the data folder from example/data folder. 

## New Features
- Completely changed the underlying memory and data structure.  Moved to dynamic model, where data struct is populated when needed and held in memory for 1 min, or when modified, to be saved by user action in the webgui.  This is a trial that 'should' save RAM, as most of the data, hostname, update urls, IPs etc are  not needed after start and can be retrieved from SPIFFS when required.  Enabling and disabling STA and AP are done via passing structs around by reference, if you're interested... minimal sketch RAM after setup is 36768 (using 2.3.0).
- Settings are no longer saved by default.  When changed a save button appears, have to save changes for them to persist!
- Completely redesigned update mechanism with live progress for connected users using webEvents. Update the ESP with one URL, including SPIFFS files.  All files have MD5 checked, and only updated if changed.  Files updated/changed logged to 'Console' in webgui. Use buildmanifest.py to generate a suitable manifest file.  
`python buildmanifest.py pathtodata pathtooutputmanifest`.  Copy binary to this folder, rename the file firmware.bin to include it in update.
- Update frequency in min can be specified in webGui.
- Credentials entered in sketch at initialization override all other settings, this can be disabled with Use Sketch Credentials (on/off).  This prevents Host, SSID and PSK being used from sketch.    
- Moved to [asyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) :)
- By default when no STA connection, the AP is enabled allowing connection and configuration. It also disables the STA.  I've found that whilst searching for the STA, the ESP is performing a WiFi scan every second, and this makes connecting reliably to the AP very difficult.  Thus if there is no STA on boot, it enables AP.  If STA connection is established on boot, but then lost the ESP will continue to try to reconnect (unless reconnect is set to false, and autoReconnect).
- Setting the AP SSID has been removed, it is now the deviceID.
- Web Events give connected web users info as to the state of updates, and changes, giving confirmation.
- handle() only works every 500ms, so very little burden to main sketch.
- Lots of changes to web interface. Including appcache support, so it is very fast loading. can be disabled with `#DISABLE_MANIFEST`
- Wrote my own mini string class, for the fun of it; `myString`.   
- Moved many strings to static variables so they are reused as much as possible.
- DHCP now handled natively via setting IPAddress(0,0,0,0).
- DNS are settable. DNS2 is optional.
- UMM information now presented in about page.
- ToolTip helpers on items on front page.
- OTApassword now working.  Password stored in plaintext for now... will change when next stable is released.
- Enable disable OTAupload.

## Bugfixes
- Many!
- Lots of bug fixes to AP logic, and tested.
- WiFi connection logic much improved.  Uses waitforconnection() result.  Super fast, much more reliable.

# ToDo

- Sort out buildmanifest directory structure... 

- Create alert with link for successful STA connection.
- Add Authentication

- OTA Page
  - Add enable/disable OTA @ boot

- Add file browser + uploader + file options... rename etc...
