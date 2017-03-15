/*
    UDP finder and storer of found devices.
    Port set in Melvanimate.h

    Sends ping every 5min unless pin recieved , should linit the traffic..
    Adds IP address and name to json array,
    First attempt at using std::smart pointers and lists


    ToDo
    1)  impletement send and receive timeout and ping type... PING and PONG methods.
    2)  Remove stale ones that have been slient for x time....
    3) test changing of hostname
    4) test for memory leaks
    5) test for number of records..
    6) write a tester that emulates many devices... to test
    7)  check for nullptr / crash when STA fails... as it does cause it





*/

#ifndef ESP_DEVICE_FINDER_H
#define ESP_DEVICE_FINDER_H

#include <functional>

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <IPAddress.h>
#include <list>
//#include <ArduinoJson.h>

//#define DebugUDP Serial
//#define UDP_TEST_SENDER //  this sends lots of pretend

#if defined(DebugUDP)
//#define DebugUDPf(...) DebugUDP.printf(__VA_ARGS__)
#define DebugUDPf(_1, ...) DebugUDP.printf_P( PSTR(_1), ##__VA_ARGS__) //  this saves around 5K RAM...

#else
#define DebugUDPf(...) {}
#endif



class UdpContext;

struct UDP_item {
  UDP_item(IPAddress ip, const char * ID) : IP(ip), name(new char[strlen(ID) + 1 ]) {
    strcpy( name.get() , ID );
    DebugUDPf("[UDP_item::UDP_item] %s (%u.%u.%u.%u)\n", name.get(), IP[0], IP[1], IP[2], IP[3]);
    lastseen = millis();
  };
  ~UDP_item() {
    DebugUDPf("[UDP_item::~UDP_item] %s (%u.%u.%u.%u)\n", name.get(), IP[0], IP[1], IP[2], IP[3]);
  }
  IPAddress IP;
  std::unique_ptr<char[]> name;
  uint32_t lastseen;
};

typedef std::list<  std::unique_ptr<UDP_item>  > UDPList;


class ESPdeviceFinder {
public:
  ESPdeviceFinder() {}
  void begin(const char * host, uint16_t port);
  void loop();
  void setHost(const char * host);


  uint8_t count();
  //void addJson(JsonArray & root);
 // UDPList getList() { return devices; }
  const char * getName(uint8_t i); 
  IPAddress getIP(uint8_t i); 


private:
  
  enum UDP_REQUEST_TYPE : uint8_t { PING = 0, PONG };
  UDPList devices;
  void _restart();
  bool _listen();
  //uint32_t _getOurIp();
  void _update();
  void _parsePacket();
  void _sendRequest(UDP_REQUEST_TYPE method);
  void _addToList(IPAddress IP, std::unique_ptr<char[]>(ID));

  uint16_t _port{0};
  const char * _host{nullptr};
  uint32_t _lastmessage{0};
//  UdpContext* _conn{nullptr};
  bool _waiting4ping{false};
  uint32_t _checkTimeOut{0};
  uint32_t _sendPong{0};
  bool _state{false};
  WiFiUDP _udp;

   WiFiEventHandler _disconnectedHandler;
   WiFiEventHandler _gotIPHandler;

  #ifdef UDP_TEST_SENDER
      void _test_sender();
  #endif



};

#endif


