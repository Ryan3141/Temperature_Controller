#include <PID_v1.h>
#include <Adafruit_MAX31865.h>

#include "OneEuroFilter.h"
#include "Device_Communicator.h"

// Use software SPI: CS, DI, DO, CLK
Adafruit_MAX31865 max = Adafruit_MAX31865( D3, D7, D6, D5 );

Device_Communicator wifi_devices;

// use hardware SPI, just pass in the CS pin
//Adafruit_MAX31856 max = Adafruit_MAX31856(10);

double PID_Current_Temperature = 0;
double PID_Output = 0;
double PID_Set_Temperature = NAN;
//PID pid( &PID_Current_Temperature, &PID_Output, &previous_data.set_temp, 0.3, 0.0, 0.6, DIRECT );
PID pid( &PID_Current_Temperature, &PID_Output, &PID_Set_Temperature, 300, 80, 40, DIRECT );
bool Output_On = false;

void Check_Temp_Sensor_For_Error( Adafruit_MAX31865 & max )
{
	// Check and print any faults
	uint8_t fault = max.readFault();
	if( fault )
	{
		Serial.print( "Fault 0x" ); Serial.println( fault, HEX );
		if( fault & MAX31865_FAULT_HIGHTHRESH )
		{
			Serial.println( "RTD High Threshold" );
		}
		if( fault & MAX31865_FAULT_LOWTHRESH )
		{
			Serial.println( "RTD Low Threshold" );
		}
		if( fault & MAX31865_FAULT_REFINLOW )
		{
			Serial.println( "REFIN- > 0.85 x Bias" );
		}
		if( fault & MAX31865_FAULT_REFINHIGH )
		{
			Serial.println( "REFIN- < 0.85 x Bias - FORCE- open" );
		}
		if( fault & MAX31865_FAULT_RTDINLOW )
		{
			Serial.println( "RTDIN- < 0.85 x Bias - FORCE- open" );
		}
		if( fault & MAX31865_FAULT_OVUV )
		{
			Serial.println( "Under/Over voltage" );
		}
		max.clearFault();
	}
}

void Run_Command( const String & command )
{
	if( command == "PING" )
		return;
	Serial.print( command + "\n" );
	String l_command = command;
	l_command.toLowerCase();
	if( l_command.startsWith( "set temp " ) )
	{
		String data = l_command.substring( 9 );
		Serial.println( "set temp" + data );
		//digitalWrite( D4, data.toInt() != 0 );
		PID_Set_Temperature = data.toFloat();
		Serial.println( "Temperature setpoint changed to " + String( PID_Set_Temperature ) );
		//pid.SetSetpoint( previous_data.set_temp );
	}
	else if( l_command.startsWith( "set pid " ) )
	{
		String data1 = l_command.substring( 8 );
		String data2 = data1.substring( data1.indexOf( ' ' ) + 1 );
		String data3 = data2.substring( data2.indexOf( ' ' ) + 1 );

		pid.SetTunings( data1.toFloat(), data2.toFloat(), data3.toFloat() );

		Serial.println( "PID coefficients changed to " + String( pid.GetKp() ) + " " + String( pid.GetKi() ) + " " + String( pid.GetKd() ) );
	}
	else if( l_command.startsWith( "turn off" ) )
	{
		Output_On = false;
		analogWrite( D2, 0 );
		Serial.println( "Turning output off" );
	}
	else if( l_command.startsWith( "turn on" ) )
	{
		Output_On = true;
		Serial.println( "Turning output on" );
	}
}

void Work_With_Serial_Connection()
{
	if( !Serial.available() )
		return;

	Serial.println( "Stuff to read" );
	static String computer_serial_partial_message;
	String & partial_message = computer_serial_partial_message;
	partial_message = partial_message + Serial.readString();

	int end_of_line = partial_message.indexOf( ';' );
	while( end_of_line != -1 ) // wait for end of client's request, that is marked with an empty line
	{
		String command = partial_message.substring( 0, end_of_line );
		partial_message = partial_message.substring( end_of_line + 1, partial_message.length() );
		while( partial_message.length() > 0 && (partial_message[ 0 ] == '\r' || partial_message[ 0 ] == '\n' || partial_message[ 0 ] == ' ') )
			partial_message = partial_message.substring( 1 );
		//partial_message.trim();
		//Serial.print( "\"" + partial_message + "\"" );

		Run_Command( command );

		end_of_line = partial_message.indexOf( ';' );
	}
}

#if 1
void setup()
{
	Serial.begin( 115200 );
	Serial.println( "MAX31856 thermocouple test" );

	{ // PID initialization
		pid.SetOutputLimits( 0, 1023 );
		pid.SetMode( AUTOMATIC );
		pinMode( D2, OUTPUT );
		analogWrite( D2, 0 );
	}

	max.begin( MAX31865_2WIRE );

	const char* ssid = "Micro-Physics-Lab";
	const char* password = "dangerouslyirrelevant";
	const char* who_i_listen_to = "Temperature Controller";
	const unsigned int port_to_use = 6543;
	wifi_devices.Init( ssid, password, who_i_listen_to, "", port_to_use );
	wifi_devices.Connect_Controller_Listener( []( const Connection & c, const String & command ) { Run_Command( command ); } );
	pinMode( D4, OUTPUT );
	digitalWrite( D4, LOW );
}

void Send_Message( const String & message, const Device_Communicator & devices = wifi_devices )
{
	Serial.print( message );
	devices.Send_Client_Data( message );
}

void loop()
{
	const float Reference_Resistor = 4300.0;
	static unsigned long previous_reading_time = millis();
	unsigned long current_time = millis();
	if( current_time - previous_reading_time >= 500 ) // All temp sensors set to same resolution
	{
		//static uint16_t rtd = 4600;
		//rtd += rand() % 20 - 9;
		uint16_t rtd = max.readRTD();
		Send_Message( "RTD value: " + String( rtd ) + "\n" );
		float ratio = rtd;
		ratio /= 32768;
		Send_Message( "Ratio = " + String( ratio, 8 ) + "\n" );
		Send_Message( "Resistance = " + String( Reference_Resistor * ratio, 8 ) + "\n" );
		float temperature = max.temperature( 1000, Reference_Resistor );
		//float temperature = Newtons_Method( ratio * Reference_Resistor / 1000 );
		Send_Message( "Temperature = " + String( temperature ) + "\n" );

		PID_Current_Temperature = temperature;
		previous_reading_time = current_time;
	}

	wifi_devices.Update();
	Work_With_Serial_Connection();

	{ // Update PID Stuff
		if( !isnan( PID_Set_Temperature ) && pid.Compute() && Output_On )
		{
			analogWrite( D2, int( PID_Output ) );
			//if( !Serial.available() )
			Send_Message( "PID Output: " + String( PID_Output ) + " Setpoint: " + String( PID_Set_Temperature ) + " Temp: " + String( PID_Current_Temperature ) + "\n" );
		}
	}

	Check_Temp_Sensor_For_Error( max );
	delay( 10 );
}

#else
void setup()
{
	Serial.begin( 115200 );
	Serial.println( "MAX31856 thermocouple test" );

	{ // PID initialization
		pid.SetOutputLimits( 0, 1023 );
		pid.SetMode( AUTOMATIC );
		pinMode( D2, OUTPUT );
		analogWrite( D2, 0 );
	}

	for( uint16_t R = 1; R < 32768; R++ )
		Serial.print( String( Adafruit_MAX31865_temperature( R, 1000, 4300 ) ) + "\t" );
	Serial.println();

	max.begin( MAX31865_2WIRE );  // set to 2WIRE or 4WIRE as necessary
}

void loop()
{
	const float Reference_Resistor = 4300.0;
	static unsigned long previous_reading_time = millis();
	unsigned long current_time = millis();

	if( current_time - previous_reading_time >= 500 ) // All temp sensors set to same resolution
	{
		static uint16_t rtd = 4600;
		rtd += rand() % 20 - 9;
		//uint16_t rtd = max.readRTD();

		//Serial.print( "RTD value: " ); Serial.println( rtd );
		//float ratio = rtd;
		//ratio /= 32768;
		//Serial.print( "Ratio = " ); Serial.println( ratio, 8 );
		//Serial.print( "Resistance = " ); Serial.println( Reference_Resistor * ratio, 8 );
		//float temperature = Translate_Temperature( rtd, 1000, Reference_Resistor );
		Serial.print( "Temperature = " ); Serial.println( temperature );

		//PID_Current_Temperature = temperature;
		PID_Current_Temperature = 0;
		previous_reading_time = current_time;
	}


	Work_With_Serial_Connection();

	{ // Update PID Stuff
		if( !isnan( PID_Set_Temperature ) && pid.Compute() && Output_On )
		{
			analogWrite( D2, int( PID_Output ) );
			//if( !Serial.available() )
			Serial.println( "PID Output: " + String( PID_Output ) + " Setpoint: " + String( PID_Set_Temperature ) + " Temp: " + String( PID_Current_Temperature ) );
		}
	}

	Check_Temp_Sensor_For_Error( max );
	delay( 10 );
}
#endif

////////////////////////////////
float Translate_Temperature( uint16_t digital_reading, float RTDnominal, float refResistor )
{
	// http://www.analog.com/media/en/technical-documentation/application-notes/AN709_0.pdf

	float resistance_ratio = refResistor * (digital_reading / 32768.0) / RTDnominal;

	// Binary search
	float left = -270.0, right = 270;
	while( right - left > 0.1 )
	{
		float center = (right + left) / 2;
		if( resistance_ratio < Temp_To_Resistance_Ratio( center ) )
			right = center;
		else
			left = center;
	}
	float center = (right + left) / 2;

	return center;
}

inline float Temp_To_Resistance_Ratio( float T )
{
	const float A = 3.81e-3;
	const float B = -6.02e-7;
	const float C = -6.0e-12;

	float T_2 = T * T;

	float resistance_ratio = 1 + A * T + T_2 * (B + C * (T_2 - 100 * T));

	return resistance_ratio;
}

inline float Derivative_Temp_To_Resistance_Ratio( float T )
{
	const float A = 3.81e-3;
	const float B = -6.02e-7;
	const float C = -6.0e-12;

	float T_2 = T * T;

	float derivative_resistance_ratio = A + T * (2 * B + C * (4 * T - 3 * 100));

	return derivative_resistance_ratio;
}

float Newtons_Method( float resistance_ratio )
{
	float x_i = 1.0;
	float x_i_old;
	int i = 0;
	while( true )
	{
		i += 1;
		x_i_old = x_i;
		x_i = x_i - (Temp_To_Resistance_Ratio( x_i ) - resistance_ratio) / Derivative_Temp_To_Resistance_Ratio( x_i );
		if( abs( x_i - x_i_old ) < 1e-2 )
		{
			return x_i;
		}
	}
}

float Binary_Search( float resistance_ratio )
{
	// Binary search
	float left = -270.0, right = 270;
	while( right - left > 0.01 )
	{
		float center = (right + left) / 2;
		if( resistance_ratio < Temp_To_Resistance_Ratio( center ) )
			right = center;
		else
			left = center;
	}
	float center = (right + left) / 2;

	return center;
}

float Adafruit_MAX31865_temperature( uint16_t binary_value, float RTDnominal, float refResistor )
{
	// http://www.analog.com/media/en/technical-documentation/application-notes/AN709_0.pdf
	const double A = 3.81e-3;
	const double B = -6.02e-7;
	const double C = -6.0e-12;

	float Z1, Z2, Z3, Z4, Rt, temp;

	Rt = binary_value;
	Rt /= 32768;
	Rt *= refResistor;

	//Serial.print("Resistance: "); Serial.println(Rt, 8);

	Z1 = -A;
	Z2 = A * A - (4 * B);
	Z3 = (4 * B) / RTDnominal;
	Z4 = 2 * B;

	temp = Z2 + (Z3 * Rt);
	temp = (sqrt( temp ) + Z1) / Z4;

	if( temp >= 0 ) return temp;

	// ugh.
	float rpoly = Rt;

	temp = -242.02;
	temp += 2.2228 * rpoly;
	rpoly *= Rt;  // square
	temp += 2.5859e-3 * rpoly;
	rpoly *= Rt;  // ^3
	temp -= 4.8260e-6 * rpoly;
	rpoly *= Rt;  // ^4
	temp -= 2.8183e-8 * rpoly;
	rpoly *= Rt;  // ^5
	temp += 1.5243e-10 * rpoly;

	return temp;
}

float Adafruit_MAX31865_temperature( float Rt )
{
	// http://www.analog.com/media/en/technical-documentation/application-notes/AN709_0.pdf

	float Z1, Z2, Z3, Z4, temp;

	//Serial.print("Resistance: "); Serial.println(Rt, 8);

	Z1 = -RTD_A;
	Z2 = RTD_A * RTD_A - (4 * RTD_B);
	Z3 = (4 * RTD_B);
	Z4 = 2 * RTD_B;

	temp = Z2 + (Z3 * Rt);
	temp = (sqrt( temp ) + Z1) / Z4;

	if( temp >= 0 ) return temp;

	// ugh.
	float rpoly = Rt;

	temp = -242.02;
	temp += 2.2228 * rpoly;
	rpoly *= Rt;  // square
	temp += 2.5859e-3 * rpoly;
	rpoly *= Rt;  // ^3
	temp -= 4.8260e-6 * rpoly;
	rpoly *= Rt;  // ^4
	temp -= 2.8183e-8 * rpoly;
	rpoly *= Rt;  // ^5
	temp += 1.5243e-10 * rpoly;

	return temp;
}

//#endif