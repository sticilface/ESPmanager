/*! \file 
    \brief to come
    
    This is to test the documentation.
*/
#include "ESPMAN.h"


// ESPMAN::JSONpackage::JSONpackage(bool isArray, size_t default_size) :
//     _jsonDoc( new DynamicJsonDocument(default_size))
// {



    

//     // if (isArray) {
//     //     _isArray = true;
//     //     //_root = _jsonBuffer.createArray();
//     // } else {
//     //     _isArray = false;
//     //     //_root = _jsonBuffer.createObject();
//     // }
// }


// int ESPMAN::JSONpackage::parse(char * data, int size)
// {




//     using namespace ESPMAN;

//     if (!data) {
//         return EMPTY_BUFFER;
//     }

//     // if (_isArray) {
//     //     //_root = _jsonBuffer.parseArray(_data.get(), size);
//     // } else {
//     //     //_root = _jsonBuffer.parseObject(_data.get(), size);
//     // }

//     // if (!_root.success()) {
//     //     return JSON_PARSE_ERROR;
//     // }

//     return 0;

// }

// int ESPMAN::JSONpackage::parseSPIFS(const char * file, FS & fs)
// {

//     using namespace ESPMAN;

//     File f = fs.open(file, "r");
//     int totalBytes = f.size();

//     if (!f) {
//         return SPIFFS_FILE_OPEN_ERROR;
//     }

//     if (totalBytes > MAX_BUFFER_SIZE) {
//         return FILE_TOO_LARGE;
//     }

//     // if (_isArray) {
//     //     _root = _jsonBuffer.parseArray(f);
//     // } else {
//     //     _root = _jsonBuffer.parseObject(f);
//     // }

//     return parseStream(f); 

// }

// int ESPMAN::JSONpackage::parseStream(Stream & in)
// {

//     if (_jsonDoc) {
//         _error = deserializeJson(*_jsonDoc, in); 

//         if (_error != DeserializationError::Ok) {
//             return 1; 
//         }
//     } else {
//         return 1; 
//     }

//     return 0; 

// }

// void ESPMAN::JSONpackage::mergejson(JsonObject& dest, JsonObject& src)
// {
//     for (auto kvp : src) {
// /*  MUST FIX THIS FUNCTION */
//         //dest[kvp.key] = kvp.value;
//     }
// }

// bool ESPMAN::JSONpackage::save(const char * file)
// {
//     File f = SPIFFS.open(file, "w");

//     if (!f) {
//         return -1;
//     }

//     serializeJsonPretty(_root, f); 
//     f.close();

//     return 0;
// }

// /*MUST FIX THIS */
// ESPMAN::JSONpackage::operator bool() const {
//     return 0; //_root.success(); 
// }


ESPMAN::myString::myString(const char *cstr)
    : buffer(nullptr)
{
    //Serial.printf("%p created from const char * :%s\n", this, cstr);
    if (cstr) {
        buffer = strdup(cstr);
    }
}

ESPMAN::myString::myString(nullptr_t ptr): buffer(nullptr) { }

ESPMAN::myString::myString(const ESPMAN::myString &str)
    : buffer(nullptr)
{
    //Serial.printf("%p copied by & from %p = %s\n", this , &str, str.c_str() );
    if (str.buffer) {
        buffer = strdup(str.buffer);
    }
}

ESPMAN::myString::myString(ESPMAN::myString &&str)
    : buffer(nullptr)
{
    //Serial.printf("%p copied by && from %p = %s\n", this , &str, str.c_str() );
    if (str.buffer) {
        buffer = str.buffer;
        str.buffer = nullptr;
    }
}

ESPMAN::myString::myString(const __FlashStringHelper *str)
    : buffer(nullptr)
{

    if (!str) {
        return;
    }

    PGM_P p = reinterpret_cast<PGM_P>(str);
    size_t len = strlen_P(p);

    if (len) {
        buffer = (char*)malloc(len + 1);
        strcpy_P(buffer, p);
        //Serial.printf("%p created from __FlashStringHelper = %s\n", this, buffer);
    }


}

ESPMAN::myString::myString(String str)
    : buffer(nullptr)
{
    //Serial.printf("%p copied by plain copy from %p\n", this, &str);
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

    if (buffer) { free(buffer); }
    if (str) {

        PGM_P p = reinterpret_cast<PGM_P>(str);
        size_t len = strlen_P(p);

        if (len) {
            buffer = (char*)malloc(len + 1);
            strcpy_P(buffer, p);
            //Serial.printf("%p[ESPMAN::myString::operator =const __FlashStringHelper *str] cstr = %s\n", this , buffer);

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

const char * ESPMAN::myString::operator()() const
{
    if (buffer) {
        return static_cast<const char *>(buffer);
    } else {
        return _nullString;
    }

};

const char * ESPMAN::myString::operator()(const char *) const
{
    if (buffer) {
        return static_cast<const char *>(buffer);
    } else {
        return _nullString;
    }
};

const char * ESPMAN::myString::c_str() const
{
    //if (buffer) {
        return static_cast<const char *>(buffer);
    // } else {
    //     return _nullString;
    // }
};

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
// bool ESPMAN::myString::operator ==(char * cstr)
// {
//     if (!cstr && !buffer) {
//         return true;
//     }

//     if (cstr && buffer) {
//         return strcmp(buffer, cstr) == 0;
//     }

//     return false;

// }

ESPMAN::myString::operator String() const
{
    return String(buffer);
}

const char * ESPMAN::myString::_nullString = "null";

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

ESPMAN::myStringf::myStringf(const __FlashStringHelper * format, ...)
    : myString()
{
    PGM_P formatP = reinterpret_cast<PGM_P>(format);
    if (!formatP) {
        return; 
    }
    va_list arg;
    va_start(arg, format);
    size_t len = vsnprintf_P(nullptr, 0, formatP, arg);
    va_end(arg);

    if (len) {
        buffer = (char*)malloc(len + 1);
        if (buffer) {
            va_start(arg, format);
            vsnprintf_P(buffer, len + 1, formatP, arg);
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

