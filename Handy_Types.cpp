// 
// 
// 

#include "Handy_Types.h"


const Pin Pin::None{ ~0 };

void Pin::Set_To_Output()
{
	pinMode( this->number, OUTPUT );
}

void Pin::Set_To_Input()
{
	pinMode( this->number, INPUT );
}

void Pin::Set_To_Input_Pullup()
{
	pinMode( this->number, INPUT_PULLUP );
}

void Pin::Set( int high_or_low )
{
	current_pin_value = high_or_low;
	digitalWrite( this->number, high_or_low );
}

void Pin::Toggle()
{
	current_pin_value = !current_pin_value;
	digitalWrite( this->number, current_pin_value );
}

//Time::Time( const Time & t )
//{
//
//}
//
//Time Time::operator=( const Time & t )
//{
//
//}

bool Time::operator<(const Time & t)
{
	return this->microseconds < t.microseconds;
}

bool Time::operator<=(const Time & t)
{
	return this->microseconds <= t.microseconds;
}

bool Time::operator>( const Time & t )
{
	return this->microseconds > t.microseconds;
}

bool Time::operator>=(const Time & t)
{
	return this->microseconds >= t.microseconds;
}

bool Time::operator==(const Time & t)
{
	return this->microseconds == t.microseconds;
}

bool Time::operator!=(const Time & t)
{
	return this->microseconds != t.microseconds;
}

Time Time::operator+(const Time & t)
{
	Time sum;
	sum.microseconds = this->microseconds + t.microseconds;
	return sum;
}

Time Time::operator-(const Time & t)
{
	Time difference;
	difference.microseconds = this->microseconds - t.microseconds;
	return difference;
}

Time Time::Microseconds( unsigned long micros )
{
	Time t;
	t.microseconds = micros;
	return t;
}

Time Time::Milliseconds( unsigned long millis )
{
	Time t;
	t.microseconds = millis * 1000;
	return t;
}

Time Time::Seconds( unsigned long seconds )
{
	Time t;
	t.microseconds = seconds * 1000000;
	return t;
}

Time Time::Microseconds( int micros )
{
	Time t;
	t.microseconds = micros;
	return t;
}

Time Time::Milliseconds( int millis )
{
	Time t;
	t.microseconds = millis * 1000;
	return t;
}

Time Time::Seconds( int seconds )
{
	Time t;
	t.microseconds = seconds * 1000000;
	return t;
}

Time Time::Microseconds( float micros )
{
	Time t;
	t.microseconds = micros;
	return t;
}

Time Time::Milliseconds( float millis )
{
	Time t;
	t.microseconds = millis * 1000;
	return t;
}

Time Time::Seconds( float seconds )
{
	Time t;
	t.microseconds = seconds * 1000000;
	return t;
}

Time Time::Now()
{
	Time now;
	now.microseconds = millis() * 1000 + micros();
	return now;
}


Run_Periodically::Run_Periodically( const Time & period ) :
	period( period ),
	previously_active_time( Time::Now() )
{
}

bool Run_Periodically::Is_Ready()
{
	Time current_time( Time::Now() );
	if( current_time - previously_active_time < period )
		return false;

	previously_active_time = current_time;
	return true;
}

void Run_Periodically::Reset()
{
	previously_active_time = Time::Now();
}
