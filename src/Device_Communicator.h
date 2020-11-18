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
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include <WiFiUdp.h>

#include "Handy_Types.h"

struct Connection
{
	Connection() {}
	Connection( IPAddress ip ) : ip( ip ) {}

	IPAddress ip;
	WiFiClient client;
	String partial_message{ "" };
	Run_Periodically timeout{ Time::Seconds( 100 ) };
};

class Device_Communicator
{
public:
	void Init( const char* SSID, const char* password, const char* who_i_listen_to, const char* initial_message, unsigned int localUdpPort, Pin indicator_led_pin );
	void Update();

	void Connect_Controller_Listener( std::function<void( const Connection & c, const String & command )> callback );
	void Send_Client_Data( const String & message );
	void Send_Client_Data( Connection & c, const String & message );

	std::vector<Connection> active_clients;
private:
	bool Check_Wifi_Status();
	void Check_For_New_Clients();
	void Check_For_Disconnects();
	void Attempt_Connect_To_Router( const char* router_ssid, const char* router_password );
	void Read_Client_Data( Connection & c );

	WiFiUDP Udp;
	unsigned int local_udp_port;
	String device_listener;
	String header_data;
	std::vector< std::function<void( const Connection & c, const String & command )> > controller_callbacks;
	Run_Periodically update_wait{ Time::Milliseconds( 1000 ) }; // Wait between updates in milliseconds
	Run_Periodically disconnected_light_flash{ Time::Milliseconds( 400 ) }; // Wait between updates in milliseconds
	Pin indicator_led{ Pin::None };
	bool wifi_was_connected{ false };
	int maximum_buffer_size{ 4096 };
};

#endif

