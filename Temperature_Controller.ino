#include <PID_v1.h>
#include <Adafruit_MAX31856.h>

#include "OneEuroFilter.h"

// Use software SPI: CS, DI, DO, CLK
Adafruit_MAX31856 max = Adafruit_MAX31856( D3, D7, D6, D5 );
// use hardware SPI, just pass in the CS pin
//Adafruit_MAX31856 max = Adafruit_MAX31856(10);

double PID_Current_Temperature = 0;
double PID_Output = 0;
double PID_Set_Temperature = NAN;
//PID pid( &PID_Current_Temperature, &PID_Output, &previous_data.set_temp, 0.3, 0.0, 0.6, DIRECT );
PID pid( &PID_Current_Temperature, &PID_Output, &PID_Set_Temperature, 300, 10, 10, DIRECT );
bool Output_On = false;

void Check_Temp_Sensor_For_Error( Adafruit_MAX31856 & max )
{
	// Check and print any faults
	uint8_t fault = max.readFault();
	if( fault )
	{
		if( fault & MAX31856_FAULT_CJRANGE ) Serial.println( "Cold Junction Range Fault" );
		if( fault & MAX31856_FAULT_TCRANGE ) Serial.println( "Thermocouple Range Fault" );
		if( fault & MAX31856_FAULT_CJHIGH )  Serial.println( "Cold Junction High Fault" );
		if( fault & MAX31856_FAULT_CJLOW )   Serial.println( "Cold Junction Low Fault" );
		if( fault & MAX31856_FAULT_TCHIGH )  Serial.println( "Thermocouple High Fault" );
		if( fault & MAX31856_FAULT_TCLOW )   Serial.println( "Thermocouple Low Fault" );
		if( fault & MAX31856_FAULT_OVUV )    Serial.println( "Over/Under Voltage Fault" );
		if( fault & MAX31856_FAULT_OPEN )    Serial.println( "Thermocouple Open Fault" );
		analogWrite( D2, 0 ); // Shut off heater since we don't know the temperature
	}
}

void Run_Command( const String & command )
{
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

	max.begin();
	max.setThermocoupleType( MAX31856_TCTYPE_K );

	Serial.print( "Thermocouple type: " );
	switch( max.getThermocoupleType() )
	{
		case MAX31856_TCTYPE_B: Serial.println( "B Type" ); break;
		case MAX31856_TCTYPE_E: Serial.println( "E Type" ); break;
		case MAX31856_TCTYPE_J: Serial.println( "J Type" ); break;
		case MAX31856_TCTYPE_K: Serial.println( "K Type" ); break;
		case MAX31856_TCTYPE_N: Serial.println( "N Type" ); break;
		case MAX31856_TCTYPE_R: Serial.println( "R Type" ); break;
		case MAX31856_TCTYPE_S: Serial.println( "S Type" ); break;
		case MAX31856_TCTYPE_T: Serial.println( "T Type" ); break;
		case MAX31856_VMODE_G8: Serial.println( "Voltage x8 Gain mode" ); break;
		case MAX31856_VMODE_G32: Serial.println( "Voltage x8 Gain mode" ); break;
		default: Serial.println( "Unknown" ); break;
	}
}

void loop()
{
	String test;
	Serial.print( "Cold Junction Temp: " ); Serial.println( max.readCJTemperature() );

	float thermocouple_temp = max.readThermocoupleTemperature();
	//thermocouple_temp = 10;
	//float set_temp = 45;
	Serial.print( "Thermocouple Temp: " ); Serial.println( thermocouple_temp );
	PID_Current_Temperature = thermocouple_temp;

	//float weakener = 12.0;
	//float on_pct = max( (set_temp - thermocouple_temp) / 100, 0 );
	//int inverted = min( 1023, 1024 * (on_pct * weakener) );
	//Serial.print( "Driving amount: " ); Serial.println( inverted );
	//analogWrite( D2, inverted );

	Work_With_Serial_Connection();

	{ // Update PID Stuff
		if( !isnan( PID_Set_Temperature ) && pid.Compute() && Output_On )
		{
			analogWrite( D2, int( PID_Output ) );
			Serial.println( "PID Output: " + String( PID_Output ) + " Setpoint: " + String( PID_Set_Temperature ) + " Temp: " + String( PID_Current_Temperature ) );
		}
	}

	Check_Temp_Sensor_For_Error( max );
	delay( 1000 );
}