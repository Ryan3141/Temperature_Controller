#include "Shift_Register.h"

void myshiftOut(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val)
{
	uint8_t i = 0;

	for( uint8_t i = 0; i < 8; i++ )
	{
		if( bitOrder == LSBFIRST )
			digitalWrite( dataPin, !(val & (1 << i)) );
		else
			digitalWrite( dataPin, !(val & (1 << (7 - i))) );
		delayMicroseconds( 100 );
		digitalWrite(clockPin, LOW);
		delayMicroseconds( 100 );
		digitalWrite(clockPin, HIGH);
	}
}

void Shift_Register::Write_Bits( uint8_t bits, uint8_t bitOrder )
{
	delayMicroseconds( 100 );
	digitalWrite( latchPin, HIGH );
	myshiftOut( dataPin, clockPin, bitOrder, bits );
	delayMicroseconds( 100 );
	digitalWrite( latchPin, LOW );
	delayMicroseconds( 100 );
}

