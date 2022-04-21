/*
	27.7.2015 version 0.0: written in a day after frustration with the seemingly non-working cheap rotational encoders.
						   THEY DID WORK, and now can be used....
	28.7.2015 version 0.2: Rewrote the button class, so I understood my code better
	
	29.7.2015 version 0.3: Made an example program using an array of (two) buttons & rotational encoders: they all work independently and nicely.

	The reason I wrote this, is that the software I tried:
	- Used timing functions or assigned interrupts that killed my servo movement.
	- Some simple solutions did not fully respect the full quadrature transitions. This caused many strange readings.
	
	With the provided two classes you can read the rotational encoder and its button.
	Should work with multiple encoder, only tried it with two.

	The classes only use the timing function (millis() ) to time itself and debounce things.
	
	Two classes are defined:

	RotEncoder(int SW1Pin, int SW2Pin, int ClicksPerNotch, int EncoderLoopMillis, int AccelarationSteps)
	Where: 
	- SW1Pin: The digital pin for switch 1.
	- SW2Pin: The digital pin for switch 2.
	- ClicksPerNotch: Clicks that must be counted before an output click is registered. My encoder uses 4 transitions pet notch.
	- EncoderLoopMillis: call the routine as often as possible in the loop(), do not use "delays", only every configured interval time (in ms) processing is done.
	- AccelarationSteps: A multiplier for the extra accelration per notch to be applied (0 means never apply accelleration)
	
	If you do not like the direction of rotation: switch the pins or swap them in your sketch when instantiating a decoder.
	Good values were obtained with my cheap rotational encoders with values of 5 to 10 ms. 
	
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
	
	The Serial prints are commented out for a reason: WITH them the algorithm can be verifed, but the timing is not correct and must be modified...
	(or the result ignored).
	
	Have fun,
	
	Ruud Rademaker
	ruud at rademaker dot gmail dot comm
	
	Dübendorf, 27.6.2015
*/

#include "Arduino.h"
#include "RotEncoder.h"


//****************************************************** CONSTRUCTOR RotEncoder
RotEncoder::RotEncoder(int SW1Pin, int SW2Pin, int ClicksPerNotch, int EncoderLoopMillis, int AccelerationStep) :
	_SW1Pin(SW1Pin), 
	_SW2Pin(SW2Pin), 
	_ClicksPerNotch(ClicksPerNotch), 
	_EncoderLoopMillis(EncoderLoopMillis),
	
	_NextProcessingMillis(0),
	_EncAreWeStoppedCounter(0),
	_EncPreviousState(0),
	_EncMovement(0),
	_EncTicks(0),
	//_EncState(0),
	_AccelerationStep(AccelerationStep),
	_acceleration(0),
	_N_decel(0)
	//now(0)

{
	pinMode (_SW1Pin,INPUT); digitalWrite(_SW1Pin, HIGH); // This enables an INPUT port with active internal pullup resistor
	pinMode (_SW2Pin,INPUT); digitalWrite(_SW2Pin, HIGH); // This enables an INPUT port with active internal pullup resistor
}

//****************************************************** CONSTRUCTOR EncoderButton	
EncoderButton::EncoderButton(int ButtonPin, int DoubleClickTime, int ButtonLoopMillis, int HoldTime, EncoderButton::ButtonType BType, int NumberOfStates) : 
	_ButtonPin(ButtonPin), 
	_ButtonLoopMillis(ButtonLoopMillis),
	_DoubleClickTime(DoubleClickTime),
	_HoldTime(HoldTime),
	_BType(BType),
	_NumberOfStates(NumberOfStates),
	_State(0),
	_EncoderButtonLast(0),
	_NextProcessingMillis(0),
	PressedT1(0),
	PressedT2(0)
	
	//ButtonState(Open),
	//ButtonReturnState(Open),
	//now(0),
	//EncoderButtonNow(0)	

{
	pinMode (_ButtonPin,INPUT); digitalWrite(_ButtonPin, HIGH); // This enables an INPUT port with active internal pullup resistor
}

//****************************************************** CONSTRUCTOR EncoderButton
EncoderButton::Button EncoderButton::ReadButton(void) {
	
	
	now = millis();

	
	ButtonReturnState = ButtonState; //Save from previous loop

	if (now >= _NextProcessingMillis) {       // else return NotProcessed state and go back to the loop //
		_NextProcessingMillis = now + _ButtonLoopMillis;  // Save next check time....//

		// Read state and build a variable StateOldNew holding both states in binary form.
		EncoderButtonNow=(digitalRead(_ButtonPin)==LOW) ? 1 : 0;
		switch (2*_EncoderButtonLast + EncoderButtonNow) {
		case 0: 									//Open or Released when applicable, this state is active all the time the button is open.
			ButtonState = Open;
			ButtonReturnState = Open;
			if (_BType == DoubleClick) {
				if ((PressedT2 !=0) && (PressedT2 <= now + _DoubleClickTime)) {
					PressedT1 = 0;
					PressedT2 = 0;
					ButtonReturnState = DoubleClicked;
				}
			}
			
			if ((ButtonReturnState == Open) && (PressedT1 !=0) && ((_BType != DoubleClick) || (PressedT1 + _DoubleClickTime <= now))) {
				PressedT1 = PressedT2;
				PressedT2 = 0;
				ButtonReturnState = Clicked;
			}
			
			break;
		case 1: 									//State on "pressed" edge, this state only occur ONCE!!!
			PressedT2 = PressedT1;
			PressedT1 = now;
			ButtonState = Closed;
			ButtonReturnState = Pressed;
			break;
		case 2: 									//Released edge, this state only occurs ONCE!!!
			ButtonReturnState = Released;
			break;
		case 3: 									//Closed or Held, this state is active all the time the button is closed.
			ButtonState = Closed;
			if (now - PressedT1 >=  _HoldTime) {
				ButtonReturnState = Held; 
				ButtonState = Held;
			} else {
				ButtonReturnState = Closed;
			}
			
			break;																	 // if so issue a "click" or "double click when applicable;		
		}
		_EncoderButtonLast = EncoderButtonNow; // set the last value for the next loop,	
	}
	return ButtonReturnState; //ButtonState preserved from previous loop
}	

int EncoderButton::ReadLatch(void) {
	if (_BType == EncoderButton::Latch) {
		if (ReadButton() == EncoderButton::Clicked) {
			++_State %= _NumberOfStates;
		} else {
			_State = 0;
		}
	}
	return _State;
}

long RotEncoder::GetAccel(void) {
	return(_acceleration);
}
	
void RotEncoder::ReadEnc(long& EncValue) {
	

	#define __MovementStopped 0
	#define __MovementCCW     1
	#define __MovementCW      2
	
	// ----------------------------------------------------------------------------
	// Acceleration configuration
	//
	#define ENC_ACCEL_INC1       40 // number of ticks (before dividing by notches) when 1st acceleration kicks in
	#define ENC_ACCEL_INC2       90 // number of ticks (before dividing by notches) when 2nd acceleration kicks in	
	#define ENC_ACCEL_INC3      150 // number of ticks (before dividing by notches) when 3rd acceleration kicks in	
	#define ENC_ACCEL_INC4      250 // number of ticks (before dividing by notches) when 4th acceleration kicks in	
	#define ENC_ACCEL_ACC1        2 // number of EXTRA notches for 1st acceleration, to be multiplied by the AccelerationStep paramenter 
	#define ENC_ACCEL_ACC2        4 // number of EXTRA notches for 2nd acceleration, to be multiplied by the AccelerationStep paramenter
	#define ENC_ACCEL_ACC3       40 // number of EXTRA notches for 3rd acceleration, to be multiplied by the AccelerationStep paramenter
	#define ENC_ACCEL_ACC4       80 // number of EXTRA notches for 4th acceleration, to be multiplied by the AccelerationStep paramenter
	#define ENC_ACCEL_RESET     800 // time in ms that ACCELERATION IS RESET TO 0

		
	/////////////////////////////////////////////
	// Logical sequence, binary of the two switch (A and B) states can be:
	//	 A: 0 1 1 0 (msb)
	//   B: 0 0 1 1 (lsb) A moves from 0 to 1 first so::
	//      0 2 3 1 for rotating CW:
	// and
	//   A: 0 0 1 1
	//   B: 0 1 1 0 For rotating CCW (B moves from 0 to 1 first)
	//      0 1 3 2    
	// So:
	//  CW: 0 2 3 1
	// CCW: 0 1 3 2 
	// Now THIS is the KEY to decoding:
	// Define the NEXT sequence of switches: one array for rotating clockwise and one array for counterclockwise.
	const byte  Expected_cw_state[4] = {2,0,3,1}; // (from->to) 0->2 1->0 2->3 3->1, so: 2 0 3 1
	const byte Expected_ccw_state[4] = {1,3,0,2}; //            0->1 1->3 2->0 3->2, so: 1 3 0 2	
	// Behaviour	
	const int _ResetAreWeStopped = 4; // Number of intervals that NO transitions are detected forces the state that "WeAreStopped"

	//////////////////////////////////////////////////////////////////////////////
	now = millis();
	if ( now - _NextProcessingMillis >= _EncoderLoopMillis) {       // else wait for next loop (overflow save)//
		_NextProcessingMillis = now;

		_EncState = ((digitalRead(_SW1Pin) == LOW) ? 
							((digitalRead(_SW2Pin) == LOW) ? 0 : 1) : 
							((digitalRead(_SW2Pin) == LOW) ? 2 : 3));
					
		if (_EncState == _EncPreviousState) {
			_EncAreWeStoppedCounter = max(0,_EncAreWeStoppedCounter-1);
			
			if (_EncAreWeStoppedCounter==0) { 
				_EncMovement = __MovementStopped;
				_N_decel = max(0,_N_decel-_EncoderLoopMillis);
				if (_N_decel==0) {_acceleration=0;}
			}	

		} else {
			_EncAreWeStoppedCounter = _ResetAreWeStopped; //Reset the number of bouncing loops to skip the check whether we "Stopped rotating"
			_N_decel = ENC_ACCEL_RESET;

			if (_acceleration<0) {
				_acceleration--;
			} else {
				_acceleration++;
			}
			
			switch (_EncMovement) {
			case __MovementStopped:
				if      (_EncState ==  Expected_cw_state[_EncPreviousState]) {
					_EncMovement =  __MovementCW;
					_EncTicks++;
				} else if (_EncState == Expected_ccw_state[_EncPreviousState]) {
					_EncMovement = __MovementCCW;
					_EncTicks--;
				}
			break;
			case __MovementCW:
				if (_EncState ==  Expected_cw_state[_EncPreviousState]) {
					_EncTicks++;
					if (_acceleration<=0) {_acceleration=1;}
					if (_acceleration>ENC_ACCEL_INC1) {_EncTicks+=ENC_ACCEL_ACC1*_AccelerationStep;}
					if (_acceleration>ENC_ACCEL_INC2) {_EncTicks+=ENC_ACCEL_ACC2*_AccelerationStep;}
					if (_acceleration>ENC_ACCEL_INC3) {_EncTicks+=ENC_ACCEL_ACC3*_AccelerationStep;}
					if (_acceleration>ENC_ACCEL_INC4) {_EncTicks+=ENC_ACCEL_ACC4*_AccelerationStep;}
				}
			break;
			case __MovementCCW:
				if (_EncState == Expected_ccw_state[_EncPreviousState]) {
					_EncTicks--;
					if (_acceleration>=0) {_acceleration=-1;}
					if (-_acceleration>ENC_ACCEL_INC1) {_EncTicks-=ENC_ACCEL_ACC1*_AccelerationStep;}
					if (-_acceleration>ENC_ACCEL_INC2) {_EncTicks-=ENC_ACCEL_ACC2*_AccelerationStep;}
					if (-_acceleration>ENC_ACCEL_INC3) {_EncTicks-=ENC_ACCEL_ACC3*_AccelerationStep;}				}
					if (-_acceleration>ENC_ACCEL_INC4) {_EncTicks-=ENC_ACCEL_ACC4*_AccelerationStep;}
			break;
			}
			_EncPreviousState = _EncState;	
		}
	
		while ( _EncTicks >=  _ClicksPerNotch) 	{EncValue++; _EncTicks -= _ClicksPerNotch;}
		while ( _EncTicks <= -_ClicksPerNotch) 	{EncValue--; _EncTicks += _ClicksPerNotch;}
	}
}