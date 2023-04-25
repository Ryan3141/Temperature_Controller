#include <Arduino.h>

#include <array>
#include <SPI.h>
#include <Adafruit_MAX31855.h>
#include <PID_v1.h>

#include "Platinum_RTD_Sensor.h"
#include "Handy_Types.h"
#include "Shift_Register.h"
#include "Device_Communicator.h"
// Compiling on windows
#if defined(_WIN32) || defined(_WIN64)
#include "C:\Tools\Login_Info\Microcontrollers.h"
#else // Compiling on linux
#include "/home/ryan/Tools/Login_Info/Microcontrollers.h"
#endif

const char* who_i_listen_to = "Temperature Controller";
const unsigned int port_to_use = 6543;

const int shortcircuit_detection_pin = 35;
const int battery1_low_detection_pin = 34;
const int battery2_low_detection_pin = 26;
const int heater_pin = 33;

const int transimpedance_latch_pin = 4;
const int transimpedance_clock_pin = 17;
const int transimpedance_data_pin  = 16;
const int mux_latch_pin = 27;
const int mux_clock_pin = 22;
const int mux_data_pin  = 25;

// Flipped around plugs
const std::array<int, 16> mux_plus_pads = { 2, 1, 3, 6, 5, 8, 10, 9, 19, 20, 18, 15, 16, 13, 11, 12 };
const std::array<int, 16> mux_minus_pads = { 1, 4, 3, 6, 8, 7, 10, 9, 20, 17, 18, 15, 13, 14, 11, 12 };
// Not sure what this is
//const std::array<int, 16> mux_plus_pads = { 1, 2, 4, 5, 6, 7, 9, 10, 20, 19, 17, 16, 15, 14, 12, 11 };
//const std::array<int, 16> mux_minus_pads = { 2, 3, 4, 5, 7, 8, 9, 10, 20, 19, 18, 17, 15, 14, 13, 12 };
// Right way around plugs
//const std::array<int, 16> mux_plus_pads = { 1, 2, 4, 5, 6, 7, 9, 10, 11, 12, 14, 15, 16, 17, 19, 20 };
//const std::array<int, 16> mux_minus_pads = { 2, 3, 4, 5, 7, 8, 9, 10, 12, 13, 14, 15, 17, 18, 19, 20 };
// Not correct
//const std::array<int, 16> mux_plus_pads = { 2, 1, 3, 6, 5, 8, 10, 9, 12, 11, 13, 16, 15, 18, 20, 19 };
//const std::array<int, 16> mux_minus_pads = { 1, 4, 3, 6, 8, 7, 10, 9, 11, 14, 13, 16, 18, 17, 20, 19 };

// Use software SPI: CS, DI, DO, CLK
Adafruit_MAX31865 max31865 = Adafruit_MAX31865( 5, 23, 19, 18 );

Device_Communicator wifi_devices;

// use hardware SPI, just pass in the CS pin CS, DI, DO, CLK
Adafruit_MAX31855 thermocouple_sensor = Adafruit_MAX31855( 18, 13, 19 );

// Setup timer and attach timer to a led pin
// use first channel of 16 channels (started from zero)
const int LEDC_CHANNEL_0 = 0;
// use 13 bit precission for LEDC timer
const int LEDC_TIMER_13_BIT = 13;
// use 5000 Hz as a LEDC base frequency
const int LEDC_BASE_FREQ = 60;

double PID_Current_Temperature = 0;
double PID_Output = 0;
double PID_Set_Temperature = NAN;
//PID pid( &PID_Current_Temperature, &PID_Output, &previous_data.set_temp, 0.3, 0.0, 0.6, DIRECT );
PID pid( &PID_Current_Temperature, &PID_Output, &PID_Set_Temperature, 300, 80, 40, DIRECT );


void Send_Message( const String & message, Device_Communicator & devices = wifi_devices );

void Change_Transimpedance_Switcher( bool only_do_iv, int gain = 0 )
{
	static Shift_Register signaler( transimpedance_latch_pin, transimpedance_clock_pin, transimpedance_data_pin );

	auto select_mux = []( uint8_t selection ){ return selection << 1; };
	const uint8_t enable = 0x1;

	if( only_do_iv )
		signaler.Write_Bits( enable | select_mux( 4 ) );
	else
	{
		if( gain == 100 )
			signaler.Write_Bits( enable | select_mux( 0 ) );
		else if( gain == 1000 )
			signaler.Write_Bits( enable | select_mux( 1 ) );
		else if( gain == 10000 )
			signaler.Write_Bits( enable | select_mux( 2 ) );
		else if( gain == 100000 )
			signaler.Write_Bits( enable | select_mux( 3 ) );
		else
		{
			Serial.println( "INVALID GAIN REQUESTED" );
			return;
		}
	}
}

void Set_Mux_Value( int i_plus, int i_minus, bool only_reflash_current = false )
{
	static Shift_Register signaler( mux_latch_pin, mux_clock_pin, mux_data_pin );
	static uint8_t merged_bitstream = 0;
	if( !only_reflash_current )
	{
		merged_bitstream = ( (i_plus << 4) & 0xF0 ) |
	                       ( i_minus & 0x0F );
	}

	signaler.Write_Bits( merged_bitstream, LSBFIRST );
}

void Recheck_Mux_Pins()
{
	static bool battery1_is_low = true;
	static bool battery2_is_low = true;
	if( battery1_is_low || battery2_is_low )
	{
		battery1_is_low = (digitalRead( battery1_low_detection_pin ) == LOW);
		battery2_is_low = (digitalRead( battery2_low_detection_pin ) == LOW);
		if( !battery1_is_low && !battery2_is_low )
			Set_Mux_Value( 0, 0, true );
	}
	else
	{
		battery1_is_low = (digitalRead( battery1_low_detection_pin ) == LOW);
		battery2_is_low = (digitalRead( battery2_low_detection_pin ) == LOW);
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

// Arduino like analogWrite
// value has to be between 0 and valueMax
void analogWrite( uint8_t channel, uint32_t value, uint32_t valueMax = 8191 )
{
	// calculate duty, 8191 from 2 ^ 13 - 1
	uint32_t duty = (8191 / valueMax) * min( value, valueMax );

	// write duty to LEDC
	ledcWrite( channel, duty );
}

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
	if( command.length() == 0 || command == "PING;" )
		return;
	//Serial.print( command + "\n" );
	String l_command = command;
	l_command.toLowerCase();
	if( l_command.startsWith( "set temp " ) )
	{
		String data = l_command.substring( 9 );
		PID_Set_Temperature = data.toFloat();
		Send_Message( "Temperature setpoint changed to " + String( PID_Set_Temperature ) + "\n" );
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
	else if( l_command.startsWith( "set ti gain " ) )
	{
		String data1 = l_command.substring( 12 );
		int gain = data1.toInt();
		Change_Transimpedance_Switcher( false, gain );
		Send_Message( "Transimpedance gain set to " + String( gain ) + "\n" );
	}
	else if( l_command.startsWith( "set ti off" ) )
	{
		Change_Transimpedance_Switcher( true );
		Send_Message( "Transimpedance mode off\n" );
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

	// Serial.println( "Stuff to read" );
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

void setup()
{
	Serial.begin( 115200 );
	delay( 4000 );
	Set_Mux_Value( 0, 0 );

	pinMode( shortcircuit_detection_pin, INPUT );
	pinMode( battery1_low_detection_pin, INPUT );
	pinMode( battery2_low_detection_pin, INPUT );


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
	const float Reference_Resistor = 4300.0;
	//const float Reference_Resistor = 4235.0;
	//const float Reference_Resistor = 3271.0;
	static Run_Periodically sample_temperature_checker( Time::Milliseconds( 500 ) );
	if( sample_temperature_checker.Is_Ready() ) // All temp sensors set to same resolution
	{
		if( false ) // Debug
		{
			static double debug_temp = 0;
			Send_Message( "Temperature = " + String( debug_temp ) + "\n" );
			debug_temp += ((rand() % 1024) - 512) / 10240.;
			PID_Current_Temperature = debug_temp;
		}
		else if( Check_Temp_Sensor_For_Error( max31865 ) )
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

		// Read thermocouple
		{
			double internal_temperature = thermocouple_sensor.readInternal();
			Send_Message( "Cold Junction Temperature = " + String( internal_temperature ) + "\n" );
			double c = thermocouple_sensor.readCelsius();
			if( isnan( c ) )
				Send_Message( "Something wrong with thermocouple!\n" );
			else
				Send_Message( "Thermocouple Temperature = " + String( c ) + "\n" );
		}
	}

	static Run_Periodically status_timer( Time::Milliseconds( 2000 ) );
	if( status_timer.Is_Ready() )
	{
		int shortcircuit = digitalRead( shortcircuit_detection_pin );
		int batter1_low = digitalRead( battery1_low_detection_pin );
		int batter2_low = digitalRead( battery2_low_detection_pin );
		Send_Message( "shortcircuit = " + String( shortcircuit ) + "\n" +
					  "batter1_low = " + String( batter1_low ) + "\n" +
					  "batter2_low = " + String( batter2_low ) + "\n" );
	}

	static Run_Periodically mux_reflasher( Time::Milliseconds( 500 ) );
	if( mux_reflasher.Is_Ready() )
		Recheck_Mux_Pins();

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

	delay( 10 );
}
