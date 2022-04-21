/*
	27.7.2015 version 0.0: written in a day after frustration with the seemingly non-working cheap rotational encoders.
	THEY DID WORK, and now can be used....

	The reason I wrote this, is that the software I tried:
	- Used timing functions or assigned interrupts that killed my servo movement.
	- Some simple solutions did not fully respect the full quadrature transitions. This caused many strange readings.
	
	With the provided two classes you can read the rotational encoder and its button.
	Should work with multiple encoder, only tried it with one.

	The classes only use the timing function (millis() ) to time itself and debounce things.
	
	Two classes are defined:

	RotEncoder(int SW1Pin, int SW2Pin, int ClicksPerNotch, int EncoderLoopMillis)
	Where: 
	- SW1Pin: The digital pin for switch 1.
	- SW2Pin: The digital pin for switch 2.
	- ClicksPerNotch: Clicks that must be counted before an output click is registered. My encoder uses 4 transitions pet notch.
	- EncoderLoopMillis: call the routine as often as possible in the loop(), do not use "delays", only every configured interval time (in ms) processing is done.
	
	If you do not like the direction of rotation: switch the pins or swap them in your sketch when instantiating a decoder.
	Good values were obtained with my cheap rotational encoders with values of 10 to 20 ms. 
	
	Method: RotEncoder.ReadEnc(long& aValue);
	This provided "Avalue" will be updated when the encoder is rotated, returns nothing.
	
		The algoritm is quite simple:
	Either you are stopped:
	- then determine whether you start rotating cw or ccw.
	Or you rotate cw:
	- then ONLY look for the next state to expect wrt. cw:
	Or you rotate ccw:
	- then ONLY look for the next state to expect wrt. ccw:
	
	This eliminates bouncing substantially: only ONE button at a time bounces, not both.
	Stopping is detected after a configured number of same states: this _EncoderDebounce is now set to 5 below (a constant)
	
	The Serial prints are commented out for a reason: WITH them the algorithm can be verified, but the timing is not correct and must be modified...
	(or the result ignored).
	
	Have fun,
	
	Ruud Rademaker
	ruud at rademaker dot gmail dot comm
	
	Dübendorf, 27.6.2015
*/

#ifndef RotEncoder_h
#define RotEncoder_h

#include "Arduino.h"

class RotEncoder
{
	public:
		RotEncoder(int SW1Pin, int SW2Pin, int ClicksPerNotch=4, int EncoderLoopMillis=5, int AccelerationStep=0);
		void ReadEnc(long& EncVal);
		long GetAccel(void);
		
	private:
		const int _SW1Pin;
		const int _SW2Pin;
		const int _ClicksPerNotch;
		const int _EncoderLoopMillis;
		
		// Own status vars
		unsigned long _NextProcessingMillis; 
		int _EncAreWeStoppedCounter;
		byte _EncPreviousState;
		byte _EncMovement;
		int _EncTicks;
		byte _EncState;
		int _AccelerationStep;
		int _acceleration;	
		long _N_decel;		
		unsigned long now;
};

class EncoderButton
{
	public:
		typedef enum Button_e {
			Open = 0,
			Closed,
			
			Pressed,
			Held,
			Released,
			
			Clicked,
			DoubleClicked,
		
		} Button;
		
		typedef enum ButtonType_e {
			Click = 0,
			DoubleClick,
			Latch,
			State
		} ButtonType;

		
	public:
		EncoderButton(int ButtonPin, int DoubleClickTime=300, int ButtonLoopMillis=10, int Holdtime=500, ButtonType BType = DoubleClick, int NumberOfStates=2 );
		EncoderButton::Button ReadButton(void);
		int ReadLatch(void);
	private:
		const int _ButtonPin;
		const int _ButtonLoopMillis;
		const int _DoubleClickTime;	
		const int _HoldTime;
		const ButtonType _BType;
		const int _NumberOfStates;
		
		int _State;

		byte _EncoderButtonLast;		
		unsigned long _NextProcessingMillis;
		
		EncoderButton::Button ButtonState;
		EncoderButton::Button ButtonReturnState;
		
		unsigned long now;		
		unsigned long PressedT1;
		unsigned long PressedT2;
		byte EncoderButtonNow;		

		
};

#endif

	