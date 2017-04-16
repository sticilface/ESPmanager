/*! \file 
    \brief to come
    
    This is to test the documentation.
*/

#pragma once

/*


      This work is based on the updater class from ESP8266 Arduino. 
      All the same copywrite stuff applied to this as that. 

      GNU LESSER GENERAL PUBLIC LICENSE

      https://github.com/esp8266/Arduino/blob/master/LICENSE



*/ 

#include <Arduino.h>
#include <Stream.h>
#include <MD5Builder.h>


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
    virtual size_t write(uint8_t value) override { write(&value, 1);  };
    virtual int available() override;
    virtual int read() override;
    
    virtual size_t readBytes(uint8_t *, size_t );

    virtual int peek() override {};
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

