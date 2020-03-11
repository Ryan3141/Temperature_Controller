#include <array>
#include <SPI.h>
#include <Adafruit_MAX31855.h>
#include <PID_v1.h>
#include <Adafruit_MAX31865.h>

//#include "OneEuroFilter.h"
#include "Device_Communicator.h"

const char* ssid = "";
const char* password = "";
const char* who_i_listen_to = "Temperature Controller";
const unsigned int port_to_use = 6543;

//const int D0 = 26;
//const int D1 = 22;
//const int D2 = 21;
//const int D3 = 17;
//const int D4 = 16;
//const int D5 = 18;
//const int D6 = 19;
//const int D7 = 23;
//const int D8 = 5;

const int heater_pin = 33;
const std::array<int, 4> v_plus_pins = { 32, 4, 17, 16 };
const std::array<int, 4> v_minus_pins = { 27, 22, 25, 21 };
//const std::array<int, 4> v_minus_pins = { 21, 25, 22, 27 };

// Flipped around plugs
//const std::array<int, 16> mux_plus_pads = { 2, 1, 3, 6, 5, 8, 10, 9, 19, 20, 18, 15, 16, 13, 11, 12 };
//const std::array<int, 16> mux_minus_pads = { 1, 4, 3, 6, 8, 7, 10, 9, 20, 17, 18, 15, 13, 14, 11, 12 };
//const std::array<int, 16> mux_plus_pads = { 1, 2, 4, 5, 6, 7, 9, 10, 20, 19, 17, 16, 15, 14, 12, 11 };
//const std::array<int, 16> mux_minus_pads = { 2, 3, 4, 5, 7, 8, 9, 10, 20, 19, 18, 17, 15, 14, 13, 12 };
//const std::array<int, 16> mux_plus_pads = { 1, 2, 4, 5, 6, 7, 9, 10, 11, 12, 14, 15, 16, 17, 19, 20 };
//const std::array<int, 16> mux_minus_pads = { 2, 3, 4, 5, 7, 8, 9, 10, 12, 13, 14, 15, 17, 18, 19, 20 };
const std::array<int, 16> mux_plus_pads = { 1, 2, 4, 5, 6, 7, 9, 10, 11, 12, 14, 15, 16, 17, 19, 20 };
const std::array<int, 16> mux_minus_pads = { 2, 3, 4, 5, 7, 8, 9, 10, 12, 13, 14, 15, 17, 18, 19, 20 };

void Find_Slowdown()
{
	static unsigned long previous_reading_time = millis();
	unsigned long current_time = millis();
	if( current_time - previous_reading_time >= 5000 )
	{
		if( current_time - previous_reading_time >= 10000 )
		{
			Serial.println( "----------------------------------------------------" );
		}
		Serial.println( current_time );
		previous_reading_time = current_time;
	}
}
std::tuple<int, int, bool> Get_Mux_Values_For_Pads( int pad1, int pad2 )
{
	auto pad_plus_i = std::find( mux_plus_pads.begin(), mux_plus_pads.end(), pad1 );
	auto pad_minus_i = std::find( mux_minus_pads.begin(), mux_minus_pads.end(), pad2 );
	int mux_plus_value = std::distance( mux_plus_pads.begin(), pad_plus_i );
	int mux_minus_value = std::distance( mux_minus_pads.begin(), pad_minus_i );
	bool is_reversed = false;

	if( pad_plus_i == mux_plus_pads.end() || pad_minus_i == mux_minus_pads.end() ) // Can't make the connection one way, try swapping
	{
		is_reversed = true;
		pad_plus_i = std::find( mux_plus_pads.begin(), mux_plus_pads.end(), pad2 );
		pad_minus_i = std::find( mux_minus_pads.begin(), mux_minus_pads.end(), pad1 );

		if( pad_plus_i == mux_plus_pads.end() || pad_minus_i == mux_minus_pads.end() )
		{
			std::tuple<int, int, bool> error_return_values { -1, -1, false };
			return error_return_values; // Unable to make connection
		}
		mux_plus_value = std::distance( mux_plus_pads.begin(), pad_plus_i );
		mux_minus_value = std::distance( mux_minus_pads.begin(), pad_minus_i );
	}

	std::tuple<int, int, bool> return_values { mux_plus_value, mux_minus_value, is_reversed };
	return return_values;
}

void Initialize_Mux_Pins()
{
	for( int pin : v_plus_pins )
	{
		pinMode( pin, OUTPUT );
		digitalWrite( pin, LOW );
	}
	for( int pin : v_minus_pins )
	{
		pinMode( pin, OUTPUT );
		digitalWrite( pin, LOW );
	}
}

void Set_Mux_Value( int i_plus, int i_minus )
{
	//Serial.println( "Changing PINS: " + String( i_plus ) + " " + String( i_minus ) );
	for( int pin_i = 0; pin_i < v_plus_pins.size(); pin_i++ )
	{
		int pin_plus = v_plus_pins[ pin_i ];
		int pin_minus = v_minus_pins[ pin_i ];
		//     Serial.println( i & (1 << pin_i) );
		if( i_plus & (1 << pin_i) )
			digitalWrite( pin_plus, HIGH );
		else
			digitalWrite( pin_plus, LOW );
		if( i_minus & (1 << pin_i) )
			digitalWrite( pin_minus, HIGH );
		else
			digitalWrite( pin_minus, LOW );
	}
}

// Arduino like analogWrite
// value has to be between 0 and valueMax
void analogWrite( uint8_t channel, uint32_t value, uint32_t valueMax = 8191 )
{
	// calculate duty, 8191 from 2 ^ 13 - 1
	uint32_t duty = (8191 / valueMax) * min( value, valueMax );

	// write duty to LEDC
	ledcWrite( channel, duty );
}


// Use software SPI: CS, DI, DO, CLK
Adafruit_MAX31865 max31865 = Adafruit_MAX31865( 5, 23, 19, 18 );
//Adafruit_MAX31865 max31865 = Adafruit_MAX31865( 5, D7, D6, D5 );

Device_Communicator wifi_devices;

// use hardware SPI, just pass in the CS pin CS, DI, DO, CLK
//Adafruit_MAX31856 thermocouple_sensor = Adafruit_MAX31855( 13, 23, 19, 18 );
//Adafruit_MAX31856 sensor = Adafruit_MAX31856( D0, D7, D6, D5 );
Adafruit_MAX31855 thermocouple_sensor = Adafruit_MAX31855( 18, 13, 19 );

// Setup timer and attach timer to a led pin
// use first channel of 16 channels (started from zero)
const int LEDC_CHANNEL_0 = 0;
// use 13 bit precission for LEDC timer
const int LEDC_TIMER_13_BIT = 13;
// use 5000 Hz as a LEDC base frequency
const int LEDC_BASE_FREQ = 5000;

double PID_Current_Temperature = 0;
double PID_Output = 0;
double PID_Set_Temperature = NAN;
//PID pid( &PID_Current_Temperature, &PID_Output, &previous_data.set_temp, 0.3, 0.0, 0.6, DIRECT );
PID pid( &PID_Current_Temperature, &PID_Output, &PID_Set_Temperature, 300, 80, 40, DIRECT );
void Send_Message( const String & message, Device_Communicator & devices = wifi_devices );


bool Check_Temp_Sensor_For_Error( Adafruit_MAX31865 & max31865 )
{
	// Check and print any faults
	uint8_t fault = max31865.readFault();
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
		max31865.clearFault();

		return false;
	}

	return true;
}

void Run_Command( const String & command )
{
	if( command == "PING;" )
		return;
	//Serial.print( command + "\n" );
	String l_command = command;
	l_command.toLowerCase();
	if( l_command.startsWith( "set temp " ) )
	{
		String data = l_command.substring( 9 );
		//Serial.println( "set temp" + data );
		//digitalWrite( D4, data.toInt() != 0 );
		PID_Set_Temperature = data.toFloat();
		Send_Message( "Temperature setpoint changed to " + String( PID_Set_Temperature ) + "\n" );
		//pid.SetSetpoint( previous_data.set_temp );
	}
	else if( l_command.startsWith( "set pid " ) )
	{
		String data1 = l_command.substring( 8 );
		String data2 = data1.substring( data1.indexOf( ' ' ) + 1 );
		String data3 = data2.substring( data2.indexOf( ' ' ) + 1 );

		pid.SetTunings( data1.toFloat(), data2.toFloat(), data3.toFloat() );

		Send_Message( "PID coefficients changed to " + String( pid.GetKp() ) + " " + String( pid.GetKi() ) + " " + String( pid.GetKd() ) + "\n" );
	}
	else if( l_command.startsWith( "get pid " ) )
	{
		Send_Message( "PID coefficients = " + String( pid.GetKp() ) + " " + String( pid.GetKi() ) + " " + String( pid.GetKd() ) + "\n" );
	}
	else if( l_command.startsWith( "turn off" ) )
	{
		pid.SetMode( MANUAL );
		PID_Output = 0;
		analogWrite( LEDC_CHANNEL_0, 0 );
		Send_Message( "Turning output off\n" );
		Send_Message( "PID Output: 0\n" );
	}
	else if( l_command.startsWith( "turn on" ) )
	{
		pid.SetMode( AUTOMATIC );
		Send_Message( "Turning output on\n" );
	}
	else if( l_command.startsWith( "set pads " ) )
	{
		String data1 = l_command.substring( 9 );
		String data2 = data1.substring( data1.indexOf( ' ' ) + 1 );
		int pad1 = data1.toInt();
		int pad2 = data2.toInt();
		std::tuple<int, int, bool> get_values = Get_Mux_Values_For_Pads( pad1, pad2 );
		//Serial.println( "Debug: " + String( data1.toInt() ) + " " + String( data2.toInt() ) );
		Serial.println( "Debug: " + String( std::get<0>( get_values ) ) + " " + String( std::get<1>( get_values ) ) );
		if( std::get<0>( get_values ) == -1 )
			Send_Message( "Unable to connect pads\n" );
		else
		{
			Set_Mux_Value( std::get<0>( get_values ), std::get<1>( get_values ) );
			//Send_Message( "Pads connected " + String( get_values[ 0 ] ) + " " + String( get_values[ 1 ] ) + "\n" );
			bool is_reversed = std::get<2>( get_values );
			Send_Message( "Pads connected " + String( pad1 ) + " " + String( pad2 ) + (is_reversed ? " reversed\n" : "\n") );
		}
	}
}

void Work_With_Serial_Connection()
{
	static bool serial_is_connected = false;
	if( !Serial.available() )
	{
		serial_is_connected = false;
		return;
	}

	if( serial_is_connected == false )
	{
		Serial.println( who_i_listen_to );
	}
	serial_is_connected = true;

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
		delay(0);
	}
}

#if 1
void setup()
{
	Serial.begin( 115200 );
	delay( 4000 );
	Initialize_Mux_Pins();

	{ // PID initialization
		pid.SetOutputLimits( 0, 8191 );
		pid.SetMode( AUTOMATIC );
//		pinMode( heater_pin, OUTPUT );
//		analogWrite( heater_pin, 0 );

		ledcSetup( LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT );
		ledcAttachPin( heater_pin, LEDC_CHANNEL_0 );
		analogWrite( LEDC_CHANNEL_0, 0 );
	}

	bool success = max31865.begin( MAX31865_2WIRE );
	if( success )
		Serial.println( "RTD began successfully" );
	//sensor.begin();
	//sensor.setThermocoupleType( MAX31856_TCTYPE_K );
	wifi_devices.Connect_Controller_Listener( []( const Connection & c, const String & command ) { Run_Command( command ); } );
	wifi_devices.Init( ssid, password, who_i_listen_to, "", port_to_use, Pin(2) );
}

void Send_Message( const String & message, Device_Communicator & devices )
{
	Serial.print( message );
	//Serial.flush();
	devices.Send_Client_Data( message );
}

void loop()
{
	Find_Slowdown();

	const float Reference_Resistor = 4300.0;
	//const float Reference_Resistor = 4235.0;
	//const float Reference_Resistor = 3271.0;
	static unsigned long previous_reading_time = millis();
	unsigned long current_time = millis();
	if( current_time - previous_reading_time >= 500 ) // All temp sensors set to same resolution
	{
		//Serial.print( "A" );
		//Send_Message( "Temperature = 0\n" );
		if( 0 )
		{
			static double debug_temp = 0;
			Send_Message( "Temperature = " + String( debug_temp ) + "\n" );
			debug_temp += ((rand() % 1024) - 512) / 1024.;
			PID_Current_Temperature = debug_temp;
		}
		if( Check_Temp_Sensor_For_Error( max31865 ) )
		{
			//uint16_t rtd = max31865.readRTD();
			//static uint16_t rtd = 4600;
			//rtd += rand() % 20 - 9;
			uint16_t rtd = max31865.readRTD();
			Send_Message( "RTD value: " + String( rtd ) + "\n" );
			double ratio = rtd / 32768.;
			Send_Message( "Ratio = " + String( ratio, 8 ) + "\n" );
			double resistance_now = ratio * Reference_Resistor;
			Send_Message( "Resistance = " + String( resistance_now, 8 ) + "\n" );
			//float temperature = max31865.temperature( 1000, Reference_Resistor );
			if( resistance_now > 3000.0 || resistance_now < 100.0 )
			{
				Send_Message( "Error With Sensor!\n" );
			}
			else
			{
				double resistance_at_zero_C = 1000.;
				double ratio_of_zero_C_resistance = resistance_now / resistance_at_zero_C;
				float temperature = Newtons_Method( ratio_of_zero_C_resistance );
				//float temperature = Newtons_Method( ratio * Reference_Resistor / 1000 );
				//float temperature = sensor.readThermocoupleTemperature();
				Send_Message( "Temperature = " + String( temperature ) + "\n" );

				PID_Current_Temperature = temperature;
			}
		}
		else
		{
			//Serial.println( "Skipped due to error" );
		}
		
		{
			//Serial.print( "Before: " );
			//Serial.println( millis() );
			double internal_temperature = thermocouple_sensor.readInternal();
			//Serial.print( "Middle: " );
			//Serial.println( millis() );
			Send_Message( "Cold Junction Temperature = " + String( internal_temperature ) + "\n" );
			//Serial.print( "After1: " );
			//Serial.println( millis() );
			double c = thermocouple_sensor.readCelsius();
			//Serial.print( "After2: " );
			//Serial.println( millis() );
			if( isnan( c ) )
				Send_Message( "Something wrong with thermocouple!\n" );
			else
				Send_Message( "Thermocouple Temperature = " + String( c ) + "\n" );
			//Serial.print( "After3: " );
			//Serial.println( millis() );
		}
		previous_reading_time = current_time;
	}

	wifi_devices.Update();
 // delay(0);
	//Work_With_Serial_Connection();

	{ // Update PID Stuff
		if( !isnan( PID_Set_Temperature ) && pid.Compute() )
		{
			analogWrite( LEDC_CHANNEL_0, int( PID_Output ) );
			//if( !Serial.available() )
			Send_Message( "PID Output: " + String( PID_Output * (100.0 / 8191) ) + " Setpoint: " + String( PID_Set_Temperature ) + " Temp: " + String( PID_Current_Temperature ) + "\n" );
		}
	}

	//std::array<int, 2> get_values = Get_Mux_Values_For_Pads( 4, 5 );
	//Set_Mux_Value( get_values[ 0 ], get_values[ 1 ] );

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
		pinMode( heater_pin, OUTPUT );
		analogWrite( heater_pin, 0 );
	}

	for( uint16_t R = 1; R < 32768; R++ )
		Serial.print( String( Adafruit_MAX31865_temperature( R, 1000, 4300 ) ) + "\t" );
	Serial.println();

	max31865.begin( MAX31865_2WIRE );  // set to 2WIRE or 4WIRE as necessary
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
		uint16_t rtd = max31865.readRTD();

		Serial.print( "RTD value: " ); Serial.println( rtd );
		float ratio = rtd;
		ratio /= 32768;
		Serial.print( "Ratio = " ); Serial.println( ratio, 8 );
		Serial.print( "Resistance = " ); Serial.println( Reference_Resistor * ratio, 8 );
		float temperature = Translate_Temperature( rtd, 1000, Reference_Resistor );
		Serial.print( "Temperature = " ); Serial.println( temperature );

		PID_Current_Temperature = temperature;
		//PID_Current_Temperature = 0;
		previous_reading_time = current_time;
	}
	delay(0);


	Work_With_Serial_Connection();

	{ // Update PID Stuff
		if( !isnan( PID_Set_Temperature ) && pid.Compute() && Output_On )
		{
			analogWrite( heater_pin, int( PID_Output ) );
			//if( !Serial.available() )
			Serial.println( "PID Output: " + String( PID_Output ) + " Setpoint: " + String( PID_Set_Temperature ) + " Temp: " + String( PID_Current_Temperature ) );
		}
	}

	Check_Temp_Sensor_For_Error( max31865 );
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
