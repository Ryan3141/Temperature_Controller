#include <Arduino.h>

// int latchPin = 27;
// int clockPin = 22;
// int dataPin = 25;

// Note that I accidentally inverted all of the signals on the pcb which is taken into account in the code
// Also 100 microseconds delay was chosen due to the optocouplers being slow (Requiring at least 50 microseconds to switch)

class Shift_Register
{
public:
	Shift_Register( int latchPin,  int clockPin,  int dataPin ) :
		latchPin( latchPin ),
		clockPin( clockPin ),
		dataPin( dataPin )
	{
		pinMode(latchPin, OUTPUT);
		pinMode(dataPin, OUTPUT);
		pinMode(clockPin, OUTPUT);
	}

	void Write_Bits( uint8_t bits, uint8_t bitOrder = MSBFIRST );
private:
	int latchPin;
	int clockPin;
	int dataPin;
};

