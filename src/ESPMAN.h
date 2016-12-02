
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <IPAddress.h>
#include <functional>
#include <FS.h>


namespace ESPMAN {

        enum ESPMAN_ERR : int8_t {
                UNKNOWN_ERROR            = -20,//  start at -20 as we use httpupdate errors
                NO_UPDATE_URL            = -21,
                SPIFFS_FILES_ABSENT      = -22,
                FILE_NOT_CHANGED         = -23,
                MD5_CHK_ERROR            = -24,
                HTTP_ERROR               = -25,
                JSON_PARSE_ERROR         = -26,
                JSON_OBJECT_ERROR        = -27,
                CONFIG_FILE_ERROR        = -28,
                UPDATOR_ERROR            = -29,
                JSON_TOO_LARGE           = -30,
                MALLOC_FAIL              = -31,
                MANIFST_FILE_ERROR       = -32,
                UNKNOWN_NUMBER_OF_FILES  = -33,
                SPIFFS_INFO_FAIL         = -34,
                SPIFFS_FILENAME_TOO_LONG = -35,
                SPIFFS_FILE_OPEN_ERROR   = -36,
                FILE_TOO_LARGE           = -37,
                INCOMPLETE_DOWNLOAD      = -38,
                CRC_ERROR                = -39,
                JSON_KEY_MISSING         = -40,
                EMPTY_BUFFER             = -41,
                AP_DISABLED              = -42,
                ERROR_ENABLING_AP        = -43,
                ERROR_DISABLING_AP       = -44,
                ERROR_SETTING_CONFIG     = -45,
                ERROR_ENABLING_STA       = -46,
                FAILED_SET_AUTOCONNECT   = -47,
                FAILED_SET_AUTORECONNECT = -48,
                WIFI_CONFIG_ERROR        = -49,
                NO_STA_SSID              = -50,
                ERROR_WIFI_BEGIN         = -60,
                NO_SSID_AVAIL            = -70,
                CONNECT_FAILED           = -80,
                UNITITIALISED            = -81,
                ERROR_SPIFFS_MOUNT       = -82,
                AUTO_CONNECTED_STA       = -83,
                ERROR_DISABLING_STA      = -84,
                STA_DISABLED             = -85,
                SETTINGS_NOT_IN_MEMORY   = -86,
                ERROR_SETTING_MAC        = -87,
                PASSWORD_MISMATCH        = -88,
                NO_CHANGES               = -89,
                PASSWOROD_INVALID        = -90,
                WRONG_SETTINGS_FILE_VERSION = -91
        };

        enum ap_boot_mode_t : int8_t {
                DISABLED = -1,
                NO_STA_BOOT = 0,
                NO_STA_BOOT_AP_5 = 5,
                NO_STA_BOOT_AP_10 = 10,
                NO_STA_BOOT_AP_30 = 30,
                NO_STA_BOOT_AP_60 = 60
        };

        enum no_sta_mode_t : int8_t {
                NO_STA_NOTHING = -2,
                NO_STA_REBOOT = -1,
                NO_STA_START_AP = 0,
                NO_STA_START_AP_5 = 5,
                NO_STA_START_AP_10 = 10,
                NO_STA_START_AP_30 = 30,
                NO_STA_START_AP_60 = 60
        };


        static const char * string_CORS = "Access-Control-Allow-Origin";
        static const char * string_CACHE_CONTROL = "Cache-Control";
        static const char * string_UPGRADE = "upgrade";
        static const char * string_UPDATE = "update";
        static const char * string_CONSOLE = "console";
        static const char * string_ERROR = "ERROR [%i]";
        static const char * string_ERROR2 = "ERROR [%i] [%i]";
        static const char * string_deviceid = "deviceid";
        static const char * string_settingsversion = "settingversion";

        static const char * string_host = "host";
        static const char * string_General = "General";
        static const char * string_mDNS = "mDNS";
        static const char * string_updateURL = "updateURL";
        static const char * string_updateFreq = "updateFreq";
        static const char * string_OTApassword = "OTApassword";
        static const char * string_GUIusername = "GUIusername";
        static const char * string_GUIpassword = "GUIpassword";
        static const char * string_GUIhash = "GUIhash";
        static const char * string_OTAport = "OTAport";
        static const char * string_STA = "STA";
        static const char * string_AP = "AP";
        static const char * string_enabled = "enabled";
        static const char * string_ssid = "ssid";
        static const char * string_pass = "pass";
        static const char * string_changed = "changed";
        static const char * string_usePerminantSettings = "usePerminantSettings";

        static const char * string_IP = "IP";
        static const char * string_GW = "GW";
        static const char * string_SN = "SN";
        static const char * string_MAC = "MAC";
        static const char * string_autoconnect = "autoconnect";
        static const char * string_autoreconnect = "autoreconnect";
        static const char * string_mode = "mode";
        static const char * string_ap_boot_mode = "ap_boot_mode";
        static const char * string_no_sta_mode = "no_sta_mode";
        static const char * string_OTAupload = "OTAupload";

        static const char * string_visible = "visible";
        static const char * string_channel = "channel";
        static const char * string_saveandreboot = "Save and Reboot to Apply";
        static const char * string_yes = "yes";

        static const char * string_DNS1 = "DNS1";
        static const char * string_DNS2 = "DNS2";

        // static const char * string_
        // static const char * string_
        // static const char * string_
        // static const char * string_

        static const int MAX_BUFFER_SIZE = 2048;
        static const int MAX_SSID_LENGTH = 32;
        static const int MAX_PASS_LENGTH = 64;
        static const int AP_START_DELAY = 2 * 60 * 1000;
        static const int SETTINGS_MEMORY_TIMEOUT = 1 * 60 * 1000;

        class JSONpackage {

private:
                DynamicJsonBuffer _jsonBuffer;
                JsonVariant _root;
                std::unique_ptr<char[]> _data;
                bool _isArray {false};

public:
                JSONpackage(bool isArray = false) {
                        if(isArray) {
                                _isArray = true;
                                _root = _jsonBuffer.createArray();
                        } else {
                                _isArray = false;
                                _root = _jsonBuffer.createObject();
                        }
                }
                ~JSONpackage() {
                }

                JsonVariant & getRoot() {
                        return _root;
                }

                int parseSPIFS(const char * file, FS & fs = SPIFFS);
                int parse(char * data, int size);
                static void mergejson(JsonObject& dest, JsonObject& src);
                bool save(const char * file); 

        };


        class myString {
public:
                myString() : buffer(nullptr) {
                }
                myString(const char *cstr);
                myString(const myString &str);

                ~myString();

                myString & operator =(const char *cstr);
                myString & operator =(const myString &str);
                // untested ==
                bool operator ==(const myString &rhs);
                bool operator !=(const myString &rhs){
                        return !(*this == rhs);
                }
                bool operator ==( char * cstr);
                bool operator !=( char * cstr){
                        return !(*this == cstr);
                }
                const char * operator()() const {
                        return static_cast<const char *>(buffer);
                };
                operator bool() const;


protected:
                char *buffer {nullptr};

        };

        struct settings_t {
                settings_t() {
                        start_time = millis();
                }

                uint32_t start_time {0};
                bool configured {false};
                bool changed {false};

                struct AP_t {
                        bool enabled {false};
                        bool hasConfig {false};
                        bool hasMAC {false};
                        myString ssid;
                        myString pass;
                        IPAddress IP;
                        IPAddress GW;
                        IPAddress SN;
                        uint8_t MAC[6] = {'\0'};
                        bool visible {true};
                        uint8_t channel {1};
                } AP;

                struct STA_t {
                        bool enabled {false};
                        bool hasConfig {false};
                        bool hasMAC {false};
                        bool dhcp {true};
                        myString ssid;
                        myString pass;
                        IPAddress IP;
                        IPAddress GW;
                        IPAddress SN;
                        IPAddress DNS1;
                        IPAddress DNS2;
                        uint8_t MAC[6] = {'\0'};
                        bool autoConnect {true};
                        bool autoReconnect {true};
                } STA;

                struct GEN_t {
                        bool mDNSenabled {true};
                        myString host;
                        myString updateURL;
                        uint32_t updateFreq {0};
                        uint16_t OTAport {8266};
                        myString OTApassword;
                        myString GUIhash;
                        //bool usePerminantSettings {true};
                        ESPMAN::ap_boot_mode_t ap_boot_mode {ESPMAN::NO_STA_BOOT};
                        ESPMAN::no_sta_mode_t no_sta_mode {ESPMAN::NO_STA_NOTHING};
                        bool OTAupload {true};
                        bool portal{true}; 
                } GEN;

        };

        struct password_t {
          myString pass;
          myString salt;
          myString hash;
        };

};
