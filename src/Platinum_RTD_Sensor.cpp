#include "Platinum_RTD_Sensor.h"

inline float Temp_To_Resistance_Ratio( float T )
{
	const float A = 3.81e-3;
	const float B = -6.02e-7;
	const float C = -6.0e-12;

	float T_2 = T * T;

	float resistance_ratio = 1 + A * T + T_2 * (B + C * (T_2 - 100 * T));

	return resistance_ratio;
}

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

inline float Derivative_Temp_To_Resistance_Ratio( float T )
{
	const float A = 3.81e-3;
	const float B = -6.02e-7;
	const float C = -6.0e-12;

	//float T_2 = T * T;

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
	//const double C = -6.0e-12;

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
