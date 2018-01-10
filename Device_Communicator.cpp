// 
// 
// 

#include "Device_Communicator.h"



Device_Communicator::Device_Communicator()
{
	Connect_Controller_Listener( []( const Connection & c, const String & command ) {} );
}

void Device_Communicator::Init( const char* SSID, const char* password, const char* who_i_listen_to, const char* initial_message, unsigned int localUdpPort )
{
	local_udp_port = localUdpPort;
	Udp.begin( local_udp_port );
	//Udp.setTimeout( 100 );

	Connect_To_Router( SSID, password );

	device_listener = who_i_listen_to;
	header_data = initial_message;
}

void Device_Communicator::Update()
{
	unsigned long current_time = millis();
	if( current_time - previous_reading_time < update_wait_time )
		return;
	previous_reading_time = current_time;
	
	//if( current_time - previous_reading_time > update_wait_time )
	{
		Check_For_New_Clients();
		previous_reading_time = current_time;
	}

	for( auto c_iterator = active_clients.begin(); c_iterator != active_clients.end(); )
	{
		//Serial.println( "debug " + String(debug++) );
		Connection & c = *c_iterator;
		if( !c.client.connected() )
		{
			Serial.println( c.ip.toString() + ": disconnected" );
			c_iterator = active_clients.erase( c_iterator );

			continue;
		}
		else
		{
			++c_iterator;
		}

		// If client is saying something
		if( c.client.available() )
			Read_Client_Data( c );
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
	for( Connection & c : active_clients )
	{
		if( c.ip == incoming_ip )
			already_connected = true;
	}
	if( already_connected )
		return;

	// Attempt to connect to ip that just pinged us
	active_clients.push_back( { incoming_ip, WiFiClient() } );
	Connection & c = active_clients.back();
	c.client.connect( incoming_ip, local_udp_port );
	if( c.client.connected() )
	{
		Serial.println( "Connected" );
		c.client.print( header_data );
		c.client.setTimeout( 10 ); // Only grab the data if it's ready already
	}
	else
		Serial.printf( "Failed to connect to %s\n", incoming_ip.toString().c_str() );
}

void Device_Communicator::Connect_Controller_Listener( std::function<void( const Connection & c, const String & command )> callback )
{
	controller_callbacks.push_back( callback );
}

void Device_Communicator::Read_Client_Data( Connection & c )
{
	c.partial_message = c.partial_message + c.client.readString();
	int end_of_line = c.partial_message.indexOf( '\n' );
	if( end_of_line != -1 ) // wait for end of client's request, that is marked with an empty line
	{
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

void Device_Communicator::Connect_To_Router( const char* router_ssid, const char* router_password )
{
	delay( 10 );

	// We start by connecting to a WiFi network
	WiFi.begin( router_ssid, router_password );

	Serial.println();
	Serial.println();
	Serial.print( "Waiting for WiFi" );

	while( WiFi.status() != WL_CONNECTED )
	{
		Serial.print( "." );
		delay( 500 );
	}

	Serial.println( "" );
	Serial.println( "WiFi connected" );
	Serial.println( "IP address: " );
	Serial.println( WiFi.localIP() );
	//server.begin();
	//Serial.println( "Server started" );

	delay( 500 );
}


