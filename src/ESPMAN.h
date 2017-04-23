/*! \file 
    \brief Extra helper classes for ESPManager.
    
    ESPMAN is a file+namespace for extra features of the ESPManager lib. 

   
*/

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <IPAddress.h>
#include <functional>
#include <FS.h>

/**
 *  @brief Contains tools for ESPmanager
 */
namespace ESPMAN
{
/** \public
 *  Error codes for ESPmanager. 
 */
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

/**
 *  \public
 *  Enum actions if no wifi found on boot 
 */
enum ap_boot_mode_t : int8_t {
    DISABLED = -1,
    NO_STA_BOOT = 0,
    NO_STA_BOOT_AP_5 = 5,
    NO_STA_BOOT_AP_10 = 10,
    NO_STA_BOOT_AP_30 = 30,
    NO_STA_BOOT_AP_60 = 60, 
};

/**
 *  \public
 *  Enum actions if wifi fails after boot
 */
enum no_sta_mode_t : int8_t {
    NO_STA_NOTHING = -2,  ///< Do nothing if there is no wifi connection on device boot. 
    NO_STA_REBOOT = -1,  ///< Reboot if there is no wifi on device boot, device waits for ::AP_START_DELAY
    NO_STA_START_AP = 0,
    NO_STA_START_AP_5 = 5,
    NO_STA_START_AP_10 = 10,
    NO_STA_START_AP_30 = 30,
    NO_STA_START_AP_60 = 60
};

/**
 *  @brief Collection of const progrem strings used in ESPmanager. 
 *  @addtogroup progmem strings
 *  @{
 */
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

 const char fstring_OK[] PROGMEM =  "OK";
/**
 *  @}
 */

// static const char * string_
// static const char * string_
// static const char * string_
// static const char * string_

static const int MAX_BUFFER_SIZE = 2048;  /**< @brief max permitted size for a buffer, generally used to receive network data */
static const int MAX_SSID_LENGTH = 32;  /**< @brief max permitted length of SSID */
static const int MAX_PASS_LENGTH = 64;  /**< @brief max permitted length of PASS */
static const int AP_START_DELAY = 2 * 60 * 1000; /**< @brief Fixed time constant before checking that there is no wifi before starting AP */
static const int SETTINGS_MEMORY_TIMEOUT = 1 * 60 * 1000; /**< @brief Time settings are kept in memory before being deleted.  Default is 1min */

/**
 * @brief A class to manage ArduinoJson objects. 
 * 
 * A class that manages the lifetime of a json object.  Either a JsonObject or JsonArray. 
 * You can also parse a SPIFFS file directly into the JSONpackage. 
 * Provides a merge function to merge two Json objects @todo template merge function. 
 */
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
    int parseStream(Stream & in); 
    int parse(char * data, int size);
    static void mergejson(JsonObject& dest, JsonObject& src);
    bool save(const char * file);
    operator bool() const; 

};

/**
 * @brief A class to manage Strings.
 * 
 * Not stricly speaking necessary, but this is an interface class to allow odd cool stuff with strings. 
 * Initialisable from lots of different types. 
 * move semantics so buffers can be moved without copy penalty. 
 * 
 */
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

/**
 *  @brief Derived from myString to allow printf type functionality. 
 *  
 *  @code
 *  myStringf abc("use of %s\n", "myString"); 
 *  @endcode
 */
class myStringf : public myString {
public:
    myStringf(const char *, ...);
    myStringf(const __FlashStringHelper *, ...);
private:

}; 

/**
 * @brief Derived from myString to allow use of printf but with buffers stored in progmem.  
 *  
 *  Probably better to just use F() macro. 
 *  
 */
class myStringf_P : public myString {
public:
    myStringf_P(PGM_P, ... );
private:
};


/**
 * @brief Master settings struct.  Contains all the settings. 
 * This struct is not kept in memory all the time.  It gets deleted and reloaded as required. 
 * 
 */
struct settings_t {
    settings_t() { start_time = millis(); }

    uint32_t start_time {0};
    bool configured {false};
    bool changed {false};

    /**
     * @brief Access point (AP) Settings struct
     * Parse this 
     */
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
    /**
     * @brief Station (STA) Settings struct
     */
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
    /**
     * @brief General settings struct, used for storing ESPmanager variables.
     */
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
/**
 * @private
 * Not in use. 
 */
struct password_t {
    myString pass;
    myString salt;
    myString hash;
};

};
