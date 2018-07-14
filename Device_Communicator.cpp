// 
// 
// 

#include "Device_Communicator.h"

#include <algorithm>

void Device_Communicator::Init( const char* SSID, const char* password, const char* who_i_listen_to, const char* initial_message, unsigned int localUdpPort, Pin indicator_led_pin )
{
	local_udp_port = localUdpPort;
	Udp.begin( local_udp_port );
	//Udp.setTimeout( 100 );

	device_listener = who_i_listen_to;
	header_data = initial_message;
	indicator_led = indicator_led_pin;
	indicator_led.Set_To_Output();
	indicator_led.Set( HIGH ); // Turn light off until it's connected

	Attempt_Connect_To_Router( SSID, password );
}

void Device_Communicator::Update()
{
	if( !update_wait.Is_Ready() )
		return;
	
	if( !Check_Wifi_Status() )
		return;

	Check_For_New_Clients();

	Check_For_Disconnects();

	for( auto & c : active_clients )
	{
		// If client is saying something
		if( c.client.available() )
		{
			Read_Client_Data( c );
			c.timeout.Reset();
		}
	}
}

void Device_Communicator::Check_For_New_Clients()
{
	int packetSize = Udp.parsePacket();
	if( !packetSize )
		return;

	IPAddress incoming_ip = Udp.remoteIP();
	Serial.printf( "Received %d bytes from %s, port %d\n", packetSize, incoming_ip.toString().c_str(), Udp.remotePort() );
	char incomingPacket[ 255 ];
	int len = Udp.read( incomingPacket, 255 );
	if( len > 0 )
	{
		incomingPacket[ len ] = 0;
	}
	Serial.printf( "UDP packet contents: %s\n", incomingPacket );

	if( device_listener == incomingPacket )
		Serial.println( "Confirmed " + device_listener );
	else
		return;

	// Don't add duplicates
	bool already_connected = false;
	for( auto c_iterator = active_clients.begin(); c_iterator != active_clients.end(); ++c_iterator )
	{
		Connection & c = *c_iterator;
		if( c.ip == incoming_ip )
		{
			//Serial.println( c.ip.toString() + ": disconnected" );
			//c_iterator = active_clients.erase( c_iterator );
			already_connected = true;
		}
		//else
		//{
		//	++c_iterator;
		//}
	}
	if( already_connected )
	{
		Serial.println( "Avoiding adding duplicate: " + device_listener );
		return;
	}
	// Attempt to connect to ip that just pinged us
	active_clients.push_back( Connection{ incoming_ip } );
	Connection & c = active_clients.back();
	c.client.connect( incoming_ip, local_udp_port );
	if( c.client.connected() )
	{
		Serial.println( "Connected" );
		c.client.print( header_data );
		c.client.setTimeout( 10 ); // Only grab the data if it's ready already
	}
	else
	{
		c.client.stop();
		active_clients.pop_back();
		Serial.printf( "Failed to connect to %s\n", incoming_ip.toString().c_str() );
	}
}

void Device_Communicator::Check_For_Disconnects()
{
	// Find any connections to be removed
	auto to_be_removed =
		std::remove_if( active_clients.begin(), active_clients.end(),
		[]( Connection & c )
	{
		return !c.client.connected() || c.client.status() == CLOSED || c.timeout.Is_Ready();
	} );

	// Perform finishing functions on closing connections
	for( auto c = to_be_removed; c != active_clients.end(); ++c )
	{
		c->client.stop();
		Serial.println( c->ip.toString() + ": disconnected" );
	}

	// Finally delete the connection entries
	active_clients.erase( to_be_removed, active_clients.end() );
}

void Device_Communicator::Connect_Controller_Listener( std::function<void( const Connection & c, const String & command )> callback )
{
	controller_callbacks.push_back( callback );
}

void Device_Communicator::Read_Client_Data( Connection & c )
{
	c.partial_message = c.partial_message + c.client.readString();

	for( int end_of_line = c.partial_message.indexOf( '\n' );
		 end_of_line != -1;
		 end_of_line = c.partial_message.indexOf( '\n' ) )
	{ // wait for end of client's request, that is marked with an empty line
		String command = c.partial_message.substring( 0, end_of_line );
		c.partial_message = c.partial_message.substring( end_of_line + 1, c.partial_message.length() );

		for( auto callback : controller_callbacks )
			callback( c, command );
	}
}

void Device_Communicator::Send_Client_Data( const String & message ) const
{
	for( auto c : active_clients )
	{
		Send_Client_Data( c, message );
	}
}

void Device_Communicator::Send_Client_Data( Connection & c, const String & message ) const
{
	//Serial.print( "Sending to " + c.client.localIP().toString() + ": " + message );
	c.client.print( message );
}

bool Device_Communicator::Check_Wifi_Status()
{
	bool wifi_is_connected = false;
	switch( WiFi.status() )
	{
		case WL_CONNECTED:
			if( !wifi_was_connected )
			{
				indicator_led.Set( LOW ); // Turn light on to show it's connected
				Serial.println( "WiFi connected" );
				Serial.println( "IP address: " );
				Serial.println( WiFi.localIP() );
				wifi_was_connected = true;
			}
			wifi_is_connected = true;
		break;
		case WL_CONNECT_FAILED:
		case WL_CONNECTION_LOST:
		case WL_DISCONNECTED:
		case WL_NO_SSID_AVAIL:
		if( wifi_was_connected )
			{
				Serial.println( "WiFi disconnected" );
				wifi_was_connected = false;
			}
			if( disconnected_light_flash.Is_Ready() )
			{
				disconnected_light_flash.Reset();
				indicator_led.Toggle(); // Turn light on to show it's connected
			}
			wifi_is_connected = false;
		break;
		default:
		Serial.print( "Wifi status not dealt with: " );
		Serial.println( WiFi.status() );
		break;
	}

	return wifi_is_connected;
}

bool Device_Communicator::Attempt_Connect_To_Router( const char* router_ssid, const char* router_password )
{
	delay( 500 );
	// We start by connecting to a WiFi network
	WiFi.begin( router_ssid, router_password );

	Serial.println();
	Serial.print( "Attempting to connect to WiFi: " );
	Serial.println( router_ssid );
}


