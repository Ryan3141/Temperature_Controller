// Device_Communicator.h

#ifndef _DEVICE_COMMUNICATOR_h
#define _DEVICE_COMMUNICATOR_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

#include <functional>
#include <vector>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

struct Connection
{
	IPAddress ip;
	WiFiClient client;
	String partial_message;
};

class Device_Communicator
{
public:
	Device_Communicator();
	void Init( const char* SSID, const char* password, const char* who_i_listen_to, const char* initial_message, unsigned int localUdpPort );
	void Update();

	void Connect_Controller_Listener( std::function<void( const Connection & c, const String & command )> callback );
	void Send_Client_Data( const String & message ) const;
	void Send_Client_Data( Connection & c, const String & message ) const;

private:
	void Check_For_New_Clients();
	void Connect_To_Router( const char* router_ssid, const char* router_password );
	void Read_Client_Data( Connection & c );

	WiFiUDP Udp;
	std::vector<Connection> active_clients;
	unsigned int local_udp_port;
	String device_listener;
	String header_data;
	std::vector< std::function<void( const Connection & c, const String & command )> > controller_callbacks;
	unsigned long update_wait_time{ 100 }; // Wait between updates in milliseconds
	unsigned long previous_reading_time{ millis() };
};

#endif

