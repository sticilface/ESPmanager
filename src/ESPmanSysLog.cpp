/*! \file 
    \brief to come
    
    This is to test the documentation.
*/

#include "ESPmanSysLog.h"

SysLog::SysLog()
	: _addr(INADDR_NONE)
	, _port(0)
	, _protocol(SYSLOG_PROTO_IETF)
	, _mask(0xff)
	, _priority(LOG_KERN)
	, _appName("ESPManager")
	, _deviceName(String())
	, _initialised(false)
{
	char tmpName[15];
	sprintf_P(tmpName, PSTR("esp8266-%06x"), ESP.getChipId());
	_deviceName = tmpName; 
}

SysLog::SysLog(IPAddress ip, uint16_t port, protocol_t protocol)
	: _addr(ip)
	, _port(port)
	, _protocol(protocol)
	, _mask(0xff)
	, _priority(LOG_KERN)
	, _appName("ESPManager")
	, _deviceName(String())
	, _initialised(false)
{
	char tmpName[15];
	sprintf_P(tmpName, PSTR("esp8266-%06x"), ESP.getChipId());
	_deviceName = tmpName; 
}

void SysLog::setServer(IPAddress ip, uint16_t port)
{
	_addr = ip;
	_port = port;
}

bool SysLog::log(myString msg)
{
	return _send(_priority, _deviceName.c_str(), _appName.c_str(), msg.c_str() ); 
}

bool SysLog::log(uint16_t pri, myString msg)
{
	return _send(pri, _deviceName.c_str() , _appName.c_str(), msg.c_str() ); 
}

bool SysLog::log(myString appName, myString msg)
{
	return _send(_priority, _deviceName.c_str() , appName.c_str() , msg.c_str() ); 
}

bool SysLog::log(uint16_t pri, myString appName, myString msg)
{
	return _send(pri, _deviceName.c_str() , appName.c_str() , msg.c_str() ); 
}

bool SysLog::_send(uint16_t pri, const char * deviceName,  const char * appName, const char * msg)
{
	
	// return if no server... 
	if (_addr == INADDR_NONE || _port == 0) {
		return false;
	}

	//  return if no msg... 
	if (msg == nullptr ) {
		return false;
	}

	// Check priority against _mask values.
	if ((LOG_MASK(LOG_PRI(pri)) & _mask) == 0) {
		return false;
	}

	// Set default facility if none specified.
	if ((pri & LOG_FACMASK) == 0) {
		pri = LOG_MAKEPRI(LOG_FAC(_priority), pri);
	}

	if (_client.beginPacket( _addr, _port)) {

		_client.print('<');
		_client.print(pri);
		if (_protocol == SYSLOG_PROTO_IETF) {
			_client.print(F(">1 - "));
		} else if (_protocol == SYSLOG_PROTO_BSD) {
			_client.print(F(">"));
		}
		_client.print(deviceName);
		_client.print(' ');
		_client.print(appName);
		if (_protocol == SYSLOG_PROTO_IETF) {
			_client.print(F(" - - - \xEF\xBB\xBF"));
		} else if (_protocol == SYSLOG_PROTO_BSD) {
			_client.print(F("[0]: "));
		}
		_client.print(msg);
		return _client.endPacket();
		
	}

	return false;

}









