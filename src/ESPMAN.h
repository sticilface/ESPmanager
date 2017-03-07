
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <IPAddress.h>
#include <functional>
#include <FS.h>


namespace ESPMAN
{

enum ESPMAN_ERR_t : int8_t {
    SUCCESS                  = 0, 
    UNKNOWN_ERROR            = -20,//  start at -20 as we use httpupdate errors
    NO_UPDATE_URL            = -21,
    SPIFFS_FILES_ABSENT      = -22,
    FILE_NOT_CHANGED         = -23,
    MD5_CHK_ERROR            = -24,
    HTTP_ERROR               = -25,
    JSON_PARSE_ERROR         = -26,
    JSON_OBJECT_ERROR        = -27,
    CONFIG_FILE_ERROR        = -28,
    UPDATER_ERROR            = -29,
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
    NO_STA_BOOT_AP_60 = 60, 
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


// static const char * string_CORS = "Access-Control-Allow-Origin";
// static const char * string_CACHE_CONTROL = "Cache-Control";
// static const char * string_UPGRADE = "upgrade";
// static const char * string_UPDATE = "update";
// static const char * string_CONSOLE = "console";
// static const char * string_ERROR = "ERROR [%i]";
// static const char * string_ERROR2 = "ERROR [%i] [%i]";
// static const char * string_ERROR2_toString = "ERROR [%s] [%s]";
// static const char * string_ERROR_toString = "ERROR [%s]";
// static const char * string_deviceid = "deviceid";
// static const char * string_settingsversion = "settingversion";

// static const char * string_host = "host";
// static const char * string_General = "General";
// static const char * string_mDNS = "mDNS";
// static const char * string_updateURL = "updateURL";
// static const char * string_updateFreq = "updateFreq";
// static const char * string_OTApassword = "OTApassword";
// static const char * string_GUIusername = "GUIusername";
// static const char * string_GUIpassword = "GUIpassword";
// static const char * string_GUIhash = "GUIhash";
// static const char * string_OTAport = "OTAport";
// static const char * string_STA = "STA";
// static const char * string_AP = "AP";
// static const char * string_enabled = "enabled";
// static const char * string_ssid = "ssid";
// static const char * string_pass = "pass";
// static const char * string_changed = "changed";
// static const char * string_usePerminantSettings = "usePerminantSettings";

// static const char * string_IP = "IP";
// static const char * string_GW = "GW";
// static const char * string_SN = "SN";
// static const char * string_MAC = "MAC";
//static const char * string_autoconnect = "autoconnect";
//static const char * string_autoreconnect = "autoreconnect";
//static const char * string_mode = "mode";
//static const char * string_ap_boot_mode = "ap_boot_mode";
//static const char * string_no_sta_mode = "no_sta_mode";
//static const char * string_OTAupload = "OTAupload";

//static const char * string_visible = "visible";
//static const char * string_channel = "channel";
//static const char * string_saveandreboot = "Save and Reboot to Apply";
//static const char * string_yes = "yes";

//static const char * string_DNS1 = "DNS1";
//static const char * string_DNS2 = "DNS2";

//static const char * string_syslog = "syslog";
//static const char * string_usesyslog = "usesyslog";
//static const char * string_syslogIP = "syslogIP";
//static const char * string_syslogPort = "syslogPort";
//static const char * string_syslogProto = "syslogProto";

//  use flash Strings instead 

 const char fstring_CORS[] PROGMEM =  "Access-Control-Allow-Origin";
 const char fstring_CACHE_CONTROL[] PROGMEM =  "Cache-Control";
 const char fstring_UPGRADE[] PROGMEM =  "upgrade";
 const char fstring_UPDATE[] PROGMEM =  "update";
 const char fstring_CONSOLE[] PROGMEM =  "console";
 const char fstring_ERROR[] PROGMEM =  "ERROR [%i]";
 const char fstring_ERROR2[] PROGMEM =  "ERROR [%i] [%i]";
 const char fstring_ERROR2_toString[] PROGMEM =  "ERROR [%s] [%s]";
 const char fstring_ERROR_toString[] PROGMEM =  "ERROR [%s]";
 const char fstring_deviceid[] PROGMEM =  "deviceid";
 const char fstring_settingsversion[] PROGMEM =  "settingversion";

 const char fstring_host[] PROGMEM =  "host";
 const char fstring_General[] PROGMEM =  "General";
 const char fstring_mDNS[] PROGMEM =  "mDNS";
 const char fstring_updateURL[] PROGMEM =  "updateURL";
 const char fstring_updateFreq[] PROGMEM =  "updateFreq";
 const char fstring_OTApassword[] PROGMEM =  "OTApassword";
 const char fstring_GUIusername[] PROGMEM =  "GUIusername";
 const char fstring_GUIpassword[] PROGMEM =  "GUIpassword";
 const char fstring_GUIhash[] PROGMEM =  "GUIhash";
 const char fstring_OTAport[] PROGMEM =  "OTAport";
 const char fstring_STA[] PROGMEM =  "STA";
 const char fstring_AP[] PROGMEM =  "AP";
 const char fstring_enabled[] PROGMEM =  "enabled";
 const char fstring_ssid[] PROGMEM =  "ssid";
 const char fstring_pass[] PROGMEM =  "pass";
 const char fstring_changed[] PROGMEM =  "changed";
 const char fstring_usePerminantSettings[] PROGMEM =  "usePerminantSettings";

 const char fstring_IP[] PROGMEM =  "IP";
 const char fstring_GW[] PROGMEM =  "GW";
 const char fstring_SN[] PROGMEM =  "SN";
 const char fstring_MAC[] PROGMEM =  "MAC";
 const char fstring_autoconnect[] PROGMEM =  "autoconnect";
 const char fstring_autoreconnect[] PROGMEM =  "autoreconnect";
 const char fstring_mode[] PROGMEM =  "mode";
 const char fstring_ap_boot_mode[] PROGMEM =  "ap_boot_mode";
 const char fstring_no_sta_mode[] PROGMEM =  "no_sta_mode";
 const char fstring_OTAupload[] PROGMEM =  "OTAupload";

 const char fstring_visible[] PROGMEM =  "visible";
 const char fstring_channel[] PROGMEM =  "channel";
 const char fstring_saveandreboot[] PROGMEM =  "Save and Reboot to Apply";
 const char fstring_yes[] PROGMEM =  "yes";

 const char fstring_DNS1[] PROGMEM =  "DNS1";
 const char fstring_DNS2[] PROGMEM =  "DNS2";

 const char fstring_syslog[] PROGMEM =  "syslog";
 const char fstring_usesyslog[] PROGMEM =  "usesyslog";
 const char fstring_syslogIP[] PROGMEM =  "syslogIP";
 const char fstring_syslogPort[] PROGMEM =  "syslogPort";
 const char fstring_syslogProto[] PROGMEM =  "syslogProto";


// static const char * string_
// static const char * string_
// static const char * string_
// static const char * string_

static const int MAX_BUFFER_SIZE = 2048;
static const int MAX_SSID_LENGTH = 32;
static const int MAX_PASS_LENGTH = 64;
static const int AP_START_DELAY = 2 * 60 * 1000;
static const int SETTINGS_MEMORY_TIMEOUT = 1 * 60 * 1000;

class JSONpackage
{

private:
    DynamicJsonBuffer _jsonBuffer;
    JsonVariant _root;
    std::unique_ptr<char[]> _data;
    bool _isArray {false};

public:
    JSONpackage(bool isArray = false);
    ~JSONpackage() { }

    JsonVariant & getRoot() { return _root; }

    int parseSPIFS(const char * file, FS & fs = SPIFFS);
    int parse(char * data, int size);
    static void mergejson(JsonObject& dest, JsonObject& src);
    bool save(const char * file);

};


class myString
{
public:
    myString() : buffer(nullptr) { }
    myString(const char *cstr);
    myString(const myString &str);
    myString(myString &&str);
    myString(const __FlashStringHelper *str); 
    myString(String str);
    myString(nullptr_t ptr);


    virtual ~myString();

    myString & operator =(const char *cstr);
    myString & operator =(const __FlashStringHelper *str);
    myString & operator =(const myString &str);
    myString & operator =(myString &&str);
    

    bool operator ==(const myString &rhs);
    bool operator !=(const myString &rhs) { return !(*this == rhs); }

    const char * operator()() const;
    const char * operator()(const char *) const;
    const char * c_str() const;
    operator bool() const;
    operator String() const; 

    static const char * _nullString;// = "NULL";


protected:
    char *buffer {nullptr};
private:

};

class myStringf : public myString {
public:
    myStringf(const char *, ...);
    myStringf(const __FlashStringHelper *, ...);
private:

}; 

class myStringf_P : public myString {
public:
    myStringf_P(PGM_P, ... );
private:
    

};

struct settings_t {
    settings_t() { start_time = millis(); }

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
        bool usesyslog {false};
        IPAddress syslogIP;
        uint16_t syslogPort{514};
        uint8_t syslogProto{0}; 

    } GEN;

};

struct password_t {
    myString pass;
    myString salt;
    myString hash;
};

};
