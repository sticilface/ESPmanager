#include "ESPMAN.h"

int ESPMAN::JSONpackage::parse(char * data, int size)
{

    using namespace ESPMAN;

    if (!data) {
        return EMPTY_BUFFER;
    }

    if (_isArray) {
        _root = _jsonBuffer.parseArray(_data.get(), size);
    } else {
        _root = _jsonBuffer.parseObject(_data.get(), size);
    }

    if (!_root.success()) {
        return JSON_PARSE_ERROR;
    }

    return 0;

}

int ESPMAN::JSONpackage::parseSPIFS(const char * file, FS & fs)
{

    using namespace ESPMAN;

    File f = fs.open(file, "r");
    int totalBytes = f.size();

    if (!f) {
        return SPIFFS_FILE_OPEN_ERROR;
    }

    if (totalBytes > MAX_BUFFER_SIZE) {
        return FILE_TOO_LARGE;
    }

    _data = std::unique_ptr<char[]>(new char[totalBytes]);

    if (!_data) {
        return MALLOC_FAIL;
    }

    int position = 0;
    int bytesleft = totalBytes;

    while ((f.available() > -1) && (bytesleft > 0)) {

        // get available data size
        int sizeAvailable = f.available();
        if (sizeAvailable) {
            int readBytes = sizeAvailable;

            // read only the asked bytes
            if (readBytes > bytesleft) {
                readBytes = bytesleft;
            }

            // get new position in buffer
            char * buf = &_data.get()[position];
            // read data
            int bytesread = f.readBytes(_data.get(), readBytes);
            if (readBytes && bytesread == 0) { break; } //  this fixes a corrupt file that has size but can't be read.
            bytesleft -= bytesread;
            position += bytesread;

        }
        // time for network streams
        delay(0);
    }

    f.close();

    if (_isArray) {
        _root = _jsonBuffer.parseArray(_data.get(), totalBytes);
    } else {
        _root = _jsonBuffer.parseObject(_data.get(), totalBytes);
    }

    if (!_root.success()) {
        return JSON_PARSE_ERROR;
    }

    return 0;

}

void ESPMAN::JSONpackage::mergejson(JsonObject& dest, JsonObject& src)
{
    for (auto kvp : src) {
        dest[kvp.key] = kvp.value;
    }
}

bool ESPMAN::JSONpackage::save(const char * file)
{
    File f = SPIFFS.open(file, "w");

    if (!f) {
        return -1;
    }

    _root.prettyPrintTo(f);

    f.close();

    return 0;
}


ESPMAN::myString::myString(const char *cstr)
    : buffer(nullptr)
{
    //Serial.printf("%p created from const char *\n", this);
    if (cstr) {
        buffer = strdup(cstr);
    }
}

ESPMAN::myString::myString(const ESPMAN::myString &str)
    : buffer(nullptr)
{
    //Serial.printf("%p copied by &\n", &str);
    if (str.buffer) {
        buffer = strdup(str.buffer);
    }
}

ESPMAN::myString::myString(ESPMAN::myString &&str)
    : buffer(nullptr)
{
    //Serial.printf("%p copied by &&\n", &str);
    if (str.buffer) {
        buffer = str.buffer;
        str.buffer = nullptr;
    }
}

ESPMAN::myString::myString(const __FlashStringHelper *str)
    : buffer(nullptr)
{
    //Serial.printf("%p created from __FlashStringHelper \n", this);

    if (!str) {
        return;
    }

    PGM_P p = reinterpret_cast<PGM_P>(str);
    size_t len = strlen(p);

    if (len) {
        buffer = (char*)malloc(len + 1);
        strcpy_P(buffer, p);
    }

}

ESPMAN::myString::myString(String str)
    : buffer(nullptr)
{
    //Serial.printf("%p copied by &&\n", &str);
    size_t len = str.length();
    if (len) {
        buffer = (char*)malloc(len + 1);
        strncpy(buffer , str.c_str(), len + 1);
    }

}

ESPMAN::myString & ESPMAN::myString::operator =(const char *cstr)
{
    //Serial.printf("[ESPMAN::myString::operator =(const char *cstr)] cstr = %s\n", (cstr)? cstr : "null");

    if (buffer) { free(buffer); }
    if (cstr) {
        buffer = strdup(cstr);
    } else {
        buffer = nullptr;
    }
    return *this;
}

ESPMAN::myString & ESPMAN::myString::operator =(const __FlashStringHelper *str)
{
    //Serial.printf("[ESPMAN::myString::operator =(const char *cstr)] cstr = %s\n", (cstr)? cstr : "null");

    if (buffer) { free(buffer); }
    if (str) {

        PGM_P p = reinterpret_cast<PGM_P>(str);
        size_t len = strlen(p);

        if (len) {
            buffer = (char*)malloc(len + 1);
            strcpy_P(buffer, p);
        }

    } else {
        buffer = nullptr;
    }
    return *this;
}

ESPMAN::myString & ESPMAN::myString::operator =(const myString &rhs)
{
    //Serial.printf("%p created from =(const myString &rhs) \n", this);
    if (buffer) { free(buffer); }
    if (rhs.buffer) {
        buffer = strdup(rhs.buffer);
    } else {
        buffer = nullptr;
    }
    return *this;
}

ESPMAN::myString & ESPMAN::myString::operator =(myString &&rhs)
{
    //Serial.printf("%p created from =(const myString &&rhs) \n", this);
    if (buffer) { free(buffer); }
    if (rhs.buffer) {
        buffer = rhs.buffer;
        rhs.buffer = nullptr;
    } else {
        buffer = nullptr;
    }
    return *this;
}


ESPMAN::myString::operator bool() const
{
    return (buffer && strlen(buffer) > 0);
}

//  untested
bool ESPMAN::myString::operator ==(const myString &rhs)
{
    if (!rhs.buffer && !buffer) {
        return true;
    }

    if (rhs.buffer && buffer) {
        return strcmp(buffer, rhs.buffer) == 0;
    }

    return false;

}
//  untested
bool ESPMAN::myString::operator ==(char * cstr)
{
    if (!cstr && !buffer) {
        return true;
    }

    if (cstr && buffer) {
        return strcmp(buffer, cstr) == 0;
    }

    return false;

}

ESPMAN::myString::~myString()
{
    if (buffer) {
        //Serial.printf("%p freeing %s\n", this, buffer);
        free(buffer);
    }
}

ESPMAN::myStringf::myStringf(const char * format, ...)
    : myString()
{
    va_list arg;
    va_start(arg, format);
    size_t len = vsnprintf(nullptr, 0, format, arg);
    va_end(arg);

    if (len) {
        buffer = (char*)malloc(len + 1);
        if (buffer) {
            va_start(arg, format);
            vsnprintf(buffer, len + 1, format, arg);
            va_end(arg);
        }
    }
}

ESPMAN::myStringf_P::myStringf_P(PGM_P formatP, ...)
    : myString()
{
    va_list arg;
    va_start(arg, formatP);
    size_t len = vsnprintf_P(nullptr, 0, formatP, arg);
    va_end(arg);

    if (len) {
        buffer = (char*)malloc(len + 1);
        if (buffer) {
            va_start(arg, formatP);
            vsnprintf_P(buffer, len + 1, formatP, arg);
            va_end(arg);
        }
    }
}

