


#pragma once

#include <ESPAsyncWebServer.h>
#include <ESP8266WiFi.h>

// #include <Arduino.h>
// #include "stddef.h"

class AsyncStaticPassThroughWebHandler: public AsyncWebHandler {
  private:
    String _getPath(AsyncWebServerRequest *request); 
  protected:
    FS _fs;
    String _uri;
    String _path;
    String _forwardUri;
    String _cache_header;
    bool _isFile;
  public:
    AsyncStaticPassThroughWebHandler(FS& fs, const char* path, const char* uri, const char* forwardUri ,const char* cache_header)
      : _fs(fs), _uri(uri), _path(path), _forwardUri(forwardUri),_cache_header(cache_header){

      _isFile = _fs.exists(path) || _fs.exists((String(path)+".gz").c_str());
      if (_uri != "/" && _uri.endsWith("/")) {
        _uri = _uri.substring(0, _uri.length() - 1); 
        DEBUGF("[AsyncStaticPassThroughWebHandler] _uri / removed\n"); 
      }
      if (_path != "/" && _path.endsWith("/")) {
        _path = _path.substring(0, _path.length() - 1); 
        DEBUGF("[AsyncStaticPassThroughWebHandler] _path / removed\n"); 
      }
    }
    bool canHandle(AsyncWebServerRequest *request);
    void handleRequest(AsyncWebServerRequest *request);
    
};