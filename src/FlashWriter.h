/**
 * @file .
 * @brief Flashwriter class.
 *
 * This work is based on the updater class from ESP8266 Arduino. 
 * All the same copywrite stuff applied to this as that. 
 * 
 * GNU LESSER GENERAL PUBLIC LICENSE
 * 
 * https://github.com/esp8266/Arduino/blob/master/LICENSE
 */

#pragma once

#include <Arduino.h>
#include <Stream.h>
#include <MD5Builder.h>

/**
 * @brief Avoids LMAC errors when trying to write a network stream to a SPIFFS file. 
 *  
 * The problem was that writing to SPIFFS, when the partition was getting full, or full of old 
 * pages, required deleting pages to free space.  This was taking too much time, and the WiFi buffer
 * was filling up leading to LMAC errors.  This class fixes this problem, by writing to the Flash
 * area reserved for ESPUpdate class.  This flash is contiguous.  The problem is that this will lead
 * to uneven flash wear.  However, until defragging comes along it will have to do. 
 *
 * example:
 * 
 * @code{cpp}
 *  http.begin(url);
    File f = SPIFFS.open("/tempfile", "w+");
    int httpCode = http.GET();
    if (httpCode == 200) {
        int len = http.getSize();
        if (len) {
            FlashWriter writer;
            int byteswritten = 0;
            if (writer.begin(len)) {
                byteswritten = http.writeToStream(&writer);  
                if (byteswritten > 0 && byteswritten == len) {
                    byteswritten = writer.writeToStream(&f); 
                } 
            } 
        }
    }
 * @endcode
 * 
 * @todo maybe try normal write first, if that fails then use this class. 
 * 
 */
class FlashWriter : public Stream
{
  public:
    ~FlashWriter();

    bool begin(size_t size);
    bool isRunning() {
      return _size > 0;
    }
    bool isFinished() {
      return _currentAddress == (_startAddress + _size);
    }
    size_t size() {
      return _size;
    }
    size_t progress() {
      return _currentAddress - _startAddress;
    }
    size_t remaining() {
      return _size - (_currentAddress - _startAddress);
    }

    String getMD5() {
      _md5.calculate();
      return _md5.toString(); 
    }

    void timeout(uint32_t value) {  _timeout = value; } 

    virtual size_t write(const uint8_t *data, size_t len) override;
    virtual size_t write(uint8_t value) override { return write(&value, 1);  };
    virtual int available() override;
    virtual int read() override;
    virtual size_t readBytes(uint8_t *, size_t );
    virtual int peek() override { return 0; };
    virtual void flush() override {};

    int writeToStream(Stream * stream); 

  private:

    bool _writeBuffer();
    uint8_t *_buffer{nullptr};
    size_t _bufferLen{0};
    size_t _size{0};
    uint32_t _startAddress{0};
    uint32_t _currentAddress{0};
    uint32_t _currentReadAddress{0};
    uint32_t _timeout{20000}; 
    MD5Builder _md5;
};

