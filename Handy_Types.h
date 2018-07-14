// Handy_Types.h

#ifndef _HANDY_TYPES_h
#define _HANDY_TYPES_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

template <typename T, typename Parameter>
class NumberType
{
public:
	explicit NumberType( T const& value ) : number( value ) {}
	explicit NumberType( T&& value ) : number( std::move( value ) ) {}
	//explicit NumberType( NumberType const& value ) : number( value.number ) {}
	//explicit NumberType( NumberType&& value ) : number( std::move( value.number ) ) {}
	//T& get() { return value_; }
	//T const& get() const { return value_; }

	static const NumberType None;

private:
	T number;
};

template <typename T, typename Parameter>
const NumberType<T, Parameter> NumberType<T, Parameter>::None{ ~0 };

//template <typename T, typename Parameter>
//class StringType
//{
//public:
//	explicit NamedType( T const& value ) : number( value ) {}
//	explicit NamedType( T&& value ) : number( std::move( value ) ) {}
//	//T& get() { return value_; }
//	//T const& get() const { return value_; }
//private:
//	T number;
//};

//using Port = NumberType<int, struct PortParameter>;

class Pin
{
public:
	explicit Pin( uint8_t const& pin_number ) : number( pin_number ) {}

	void Set_To_Output();
	void Set_To_Input();
	void Set_To_Input_Pullup();

	void Set( int high_or_low );
	void Toggle();

	static const Pin None;

private:
	int current_pin_value;
	uint8_t number;
};


class Time
{
public:
	//Time();
	//Time( const Time & t );
	//Time&& operator=( const Time & t );

	bool operator<(const Time & t);
	bool operator<=(const Time & t);
	bool operator>(const Time & t);
	bool operator>=(const Time & t);
	bool operator==(const Time & t);
	bool operator!=(const Time & t);

	Time operator+(const Time & t);
	Time operator-(const Time & t);

	static Time Seconds( unsigned long seconds );
	static Time Microseconds( unsigned long micros );
	static Time Milliseconds( unsigned long millis );

	static Time Seconds( int seconds );
	static Time Microseconds( int micros );
	static Time Milliseconds( int millis );

	static Time Seconds( float seconds );
	static Time Microseconds( float micros );
	static Time Milliseconds( float millis );

	static Time Now();

	long long microseconds{ 0 };
};



class Run_Periodically
{
public:
	Run_Periodically( const Time & period );
	bool Is_Ready();
	void Reset();

private:
	Time previously_active_time;
	Time period;
};


#endif

