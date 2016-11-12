#include "ESPMAN.h"

int ESPMAN::JSONpackage::parse(char * data, int size) {

        using namespace ESPMAN;

        if (!data) {
                return EMPTY_BUFFER;
        }

        if (_isArray) {
                _root = _jsonBuffer.parseArray(_data.get(),size);
        } else {
                _root = _jsonBuffer.parseObject(_data.get(),size);
        }

        if (!_root.success()) {
                return JSON_PARSE_ERROR;
        }

        return 0;

}

int ESPMAN::JSONpackage::parseSPIFS(const char * file, FS & fs) {

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
                _root = _jsonBuffer.parseArray(_data.get(),totalBytes);
        } else {
                _root = _jsonBuffer.parseObject(_data.get(),totalBytes);
        }

        if (!_root.success()) {
                return JSON_PARSE_ERROR;
        }

        return 0;

}

void ESPMAN::JSONpackage::mergejson(JsonObject& dest, JsonObject& src) {
   for (auto kvp : src) {
     dest[kvp.key] = kvp.value;
   }
}

bool ESPMAN::JSONpackage::save(const char * file) {
  File f = SPIFFS.open(file, "w");

  if (!f) {
          return -1;
  }

  _root.prettyPrintTo(f);

  f.close();

  return 0;
}


ESPMAN::myString::myString(const char *cstr)
{
        if (cstr) {
                buffer = strdup(cstr);
        }
}

ESPMAN::myString::myString(const ESPMAN::myString &str)
{
        //Serial.print("[ESPMAN::myString::myString(const ESPMAN::myString &str)]\n");

        if (buffer) {
                free(buffer);
                buffer = nullptr;
        }

        if (str.buffer) {
                buffer = strdup(str.buffer);
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

ESPMAN::myString & ESPMAN::myString::operator =(const myString &rhs)
{

        //Serial.printf("[ESPMAN::myString::operator =(const myString &str)] rhs = %s\n", (rhs.buffer)? rhs.buffer : "null" );

        if (buffer) { free(buffer); }
        if (rhs.buffer) {
                buffer = strdup(rhs.buffer);
        } else {
                buffer = nullptr;
        }
        return *this;
}

  ESPMAN::myString::operator bool() const {
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

ESPMAN::myString::~myString() {
        if (buffer) {
                //Serial.printf("[ESPMAN::myString::~myString()] %s\n", buffer);
                free(buffer);
        }
}
