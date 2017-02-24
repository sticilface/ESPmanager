/*

SysLog implemention for ESPManager

Credit due to https://github.com/arcao/Syslog for the implementation.


*/
#pragma once

#include "ESPMAN.h"
#include <IPAddress.h>
#include <WiFiUdp.h>
//#include <bitset>

using namespace ESPMAN;

//typedef std::bitset<32> myBit; 

#define LOG_PRIMASK 0x07  /* mask to extract priority part (internal) */
/* extract priority */
#define LOG_PRI(p)  ((p) & LOG_PRIMASK)
#define LOG_MAKEPRI(fac, pri) (((fac) << 3) | (pri))
#define LOG_NFACILITIES 24  /* current number of facilities */
#define LOG_FACMASK 0x03f8  /* mask to extract facility part */
                            /* facility of pri */
#define LOG_FAC(p)  (((p) & LOG_FACMASK) >> 3)
#define LOG_MASK(pri)  (1 << (pri))	/* mask for one priority */
#define LOG_UPTO(pri)  ((1 << ((pri)+1)) - 1)	/* all priorities through pri */


enum logLevel_t : uint32_t {
	LOG_EMERG    = 0, /* system is unusable */
	LOG_ALERT    = 1, /* action must be taken immediately */
	LOG_CRIT     = 2, /* critical conditions */
	LOG_ERR      = 3, /* error conditions */
	LOG_WARNING  = 4, /* warning conditions */
	LOG_NOTICE   = 5, /* normal but significant condition */
	LOG_INFO     = 6, /* informational */
	LOG_DEBUG    = 7  /* debug-level messages */
};

enum protocol_t : uint8_t {
	SYSLOG_PROTO_IETF = 0,   // RFC 5424
	SYSLOG_PROTO_BSD  = 1   // RFC 3164
};

/* facility codes */
enum facility_t : uint32_t {
	LOG_KERN     = (0 << 3),  /* kernel messages */
	LOG_USER     = (1 << 3),  /* random user-level messages */
	LOG_MAIL     = (2 << 3),  /* mail system */
	LOG_DAEMON   = (3 << 3),  /* system daemons */
	LOG_AUTH     = (4 << 3),  /* security/authorization messages */
	LOG_SYSLOG   = (5 << 3),  /* messages generated internally by syslogd */
	LOG_LPR      = (6 << 3),  /* line printer subsystem */
	LOG_NEWS     = (7 << 3),  /* network news subsystem */
	LOG_UUCP     = (8 << 3),  /* UUCP subsystem */
	LOG_CRON     = (9 << 3),  /* clock daemon */
	LOG_AUTHPRIV = (10 << 3), /* security/authorization messages (private) */
	LOG_FTP      = (11 << 3)  /* ftp daemon */
};




class SysLog
{
public:
	SysLog();
	SysLog(IPAddress ip, uint16_t port, protocol_t protocol = SYSLOG_PROTO_IETF);
	~SysLog() {}

	void setServer(IPAddress ip, uint16_t port);
	void setMask(uint8_t mask) { _mask = mask; }
	void setPriority(uint16_t priority) { _priority = priority; } 
	void setDeviceName(String name) { _deviceName = name; }
	void setAppName(String & name) { _appName = name; }
	void setProtocol(protocol_t protocol) { _protocol = protocol; }

	bool log(myString msg); 
	bool log(uint16_t pri, myString msg); 
	bool log(myString appName, myString msg);
	bool log(uint16_t pri, myString appName, myString msg);

private:
	bool _send(uint16_t pri, const char * deviceName, const char * appName, const char * msg);
	IPAddress _addr;
	uint16_t _port;
	protocol_t _protocol;
	uint8_t _mask; 
	uint16_t _priority; 
	String _appName;
	String _deviceName; 
	bool _initialised; 
	WiFiUDP _client; 

};