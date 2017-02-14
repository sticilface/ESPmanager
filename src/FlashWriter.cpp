#include "FlashWriter.h"
#include <flash_utils.h>

extern "C" uint32_t _SPIFFS_start;

FlashWriter::~FlashWriter()
{

  if (_buffer) {
    delete _buffer;
  }

  //Serial.println("FlashWriter ~");
}

bool FlashWriter::begin(size_t size)
{
  uint32_t currentSketchSize = (ESP.getSketchSize() + FLASH_SECTOR_SIZE - 1) & (~(FLASH_SECTOR_SIZE - 1));
  //address of the end of the space available for sketch and update
  uint32_t updateEndAddress = (uint32_t)&_SPIFFS_start - 0x40200000;
  //size of the update rounded to a sector
  uint32_t roundedSize = (size + FLASH_SECTOR_SIZE - 1) & (~(FLASH_SECTOR_SIZE - 1));
  //address where we will start writing the update
  uint32_t updateStartAddress = updateEndAddress - roundedSize;

//  Serial.printf("[begin] roundedSize:       0x%08X (%d)\n", roundedSize, roundedSize);
//  Serial.printf("[begin] updateEndAddress:  0x%08X (%d)\n", updateEndAddress, updateEndAddress);
//  Serial.printf("[begin] currentSketchSize: 0x%08X (%d)\n", currentSketchSize, currentSketchSize);

  if (updateStartAddress < currentSketchSize) {
    return false;
  }
  _startAddress = updateStartAddress;
  _currentAddress = _startAddress;
  _currentReadAddress = _startAddress;
  _size = size;
  _buffer = new uint8_t[FLASH_SECTOR_SIZE];

  if (!_buffer) {
    return false;
  }

  _md5.begin();

  return true;
}

bool FlashWriter::_writeBuffer() {

  yield();
  bool result = ESP.flashEraseSector(_currentAddress / FLASH_SECTOR_SIZE);
  yield();
  if (result) {
    result = ESP.flashWrite(_currentAddress, (uint32_t*) _buffer, _bufferLen);
    //Serial.printf("[%s] %u written to flash\n" , __func__ ,_bufferLen );
  }
  yield();

  if (!result) {
    _currentAddress = (_startAddress + _size);

    return false;
  }
  _md5.add(_buffer, _bufferLen);
  _currentAddress += _bufferLen;
  _bufferLen = 0;
  return true;
}



size_t FlashWriter::write(const uint8_t *data, size_t len = 1) {
  if (len > remaining()) {
    return 0;
  }

  size_t left = len;

  while ((_bufferLen + left) > FLASH_SECTOR_SIZE) {
    size_t toBuff = FLASH_SECTOR_SIZE - _bufferLen;
    memcpy(_buffer + _bufferLen, data + (len - left), toBuff);
    _bufferLen += toBuff;
    if (!_writeBuffer()) {
      return len - left;
    }
    left -= toBuff;
    yield();
  }
  //lets see whats left
  memcpy(_buffer + _bufferLen, data + (len - left), left);
  _bufferLen += left;
  if (_bufferLen == remaining()) {
    //we are at the end of the update, so should write what's left to flash
    if (!_writeBuffer()) {
      return len - left;
    }
  }
  return len;
}

int FlashWriter::read() {
  uint8_t data = 0; 
  readBytes(&data,1);
  return data; 
}


size_t FlashWriter::readBytes(uint8_t * dst, size_t size) {

  uint32_t addr = _currentReadAddress;
  uint32_t alignedBegin = (addr + 3) & (~3);
  uint32_t alignedEnd = (addr + size) & (~3);
  
  if (alignedEnd < alignedBegin) {
    alignedEnd = alignedBegin;
  }

  if (addr < alignedBegin) {
    uint32_t nb = alignedBegin - addr;
    uint32_t tmp;
    if (!ESP.flashRead(alignedBegin - 4, &tmp, 4)) {
      //Serial.printf("[%s: %d] addr=%x size=%u ab=%x ae=%x\r\n", __PRETTY_FUNCTION__  , __LINE__, addr, size, alignedBegin, alignedEnd);
      return 0;
    }
    memcpy(dst, &tmp + 4 - nb, nb);
  }

  if (alignedEnd != alignedBegin) {
    if (!ESP.flashRead(alignedBegin, (uint32_t*) (dst + alignedBegin - addr), alignedEnd - alignedBegin)) {
      //Serial.printf("[%s: %d] addr=%x size=%u ab=%x ae=%x\r\n", __PRETTY_FUNCTION__  , __LINE__, addr, size, alignedBegin, alignedEnd);
      return 0;
    }
  }

  if (addr + size > alignedEnd) {
    uint32_t nb = addr + size - alignedEnd;
    uint32_t tmp;
    if (!ESP.flashRead(alignedEnd, &tmp, 4)) {
      //Serial.printf("[%s: %d] addr=%x size=%x ab=%x ae=%x\r\n", __PRETTY_FUNCTION__  , __LINE__, addr, size, alignedBegin, alignedEnd);
      return 0;
    }
    memcpy(dst + size - nb, &tmp, nb);
  }

  _currentReadAddress += size; 
  return size; 
}

int FlashWriter::available()
{
  return _size - (_currentReadAddress - _startAddress) ; 
}

int FlashWriter::writeToStream(Stream * stream)
{
    uint32_t ret = 0;
    uint32_t start_time = millis(); 

    if (!stream) {
      return 0; 
    }

    size_t left = available(); 

    //Serial.printf("[%s: %d] _size = %u, left = %u\n", __PRETTY_FUNCTION__  , __LINE__, _size, left);

    while(left) {
      uint32_t fallbackAddr = _currentReadAddress; //  if there is a failed write then this allows it to restart next loop. 
      
      if (left > FLASH_SECTOR_SIZE) {
        _bufferLen = FLASH_SECTOR_SIZE; 
        readBytes(_buffer, FLASH_SECTOR_SIZE); 
        
      } else {
        _bufferLen = left; 
        readBytes(_buffer, left); 
      }

      uint32_t write_time = millis(); 
      int val = stream->write(_buffer, _bufferLen); 

      if (val > 0) {
        ret += val; 
        //Serial.printf("[%s: %d] %i bytes written in %u ms (%u%%)\n", __PRETTY_FUNCTION__  , __LINE__, val, millis() - write_time, ret / (_size / 100)  );
        start_time = millis();  //  reset the timer if the last block was written correctly... 
        
      } else {
        _currentReadAddress = fallbackAddr; //  try to read and write that block again... 
        //Serial.printf("[%s: %d] Write Error (time =%u ms) \n", __PRETTY_FUNCTION__  , __LINE__, millis() - write_time );

      }
      
      left = available(); 

      if (millis() - start_time > _timeout) {
        //Serial.printf("[%s: %d] Write TimeOut \n", __PRETTY_FUNCTION__  , __LINE__ );
        return 0; 
      }
      
      yield();
  
    }

    return ret; 
}

