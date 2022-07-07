/*
	25.02.2017 version 0.0: written to support my drivers for 128 LEDS
	
	The leds are assumed to be in a configuration using RGB Leds, arranged in a matrix of 8 columns with 5 RGB LEDS.
	My "Dalek" on top of the Doctor WHO pinball machine, uses now three of these matrices _AND_ I changed from TM1640 drivers to HT16K33 (i2c) chips.
	
	This library separates the memory and buffer management from the chip communication. 
	It supports both communication modes, TM1640 and HT16K33.
	
	TM1640 mode can be activated using a "#define" named "__TM1640". If this define does NOT exist, HT16K33 mode is assumend.
	NOTE: CURRENTLY THIS IS NOT IMPLEMENTED, ONLY HT16K33 !!!!
	
	Have fun,
	
	Ruud Rademaker
	ruud at rademaker dot gmail dot comm
	
	Dübendorf, 25.02.2017
*/
#include "Arduino.h"
#include <Wire.h>  // Comes with Arduino IDE, used for driving the LED drivers 

#include "DalekDriver.h"

/* 	You can define how your DALEK is built, perhaps you skipped the first row of LEDS or 
	perhaps you swapped the RGB pins, so that leds display BGR instead of RGB.
	uncomment the defines below accordingly
*/
//#define REGULAR_LEDS

//#define SWAP_LEDS_RGB
//#define SWAP_EXTRA_RGB
//#define SWAP_LEFT_RGB
//#define SWAP_NOSE_RGB
//#define SWAP_RIGHT_RGB

// END OF DEFINES FOR LED SOLDERING issues.

//****************************************************** CONSTRUCTOR L
DalekDriver::DalekDriver(byte HexAddress) :
	_HexAddress(HexAddress)
{
	DalekDriver::ClearBuffer();
}

void DalekDriver::InitDriver(void) {
	Wire.beginTransmission(_HexAddress);
	Wire.write(0x21);  // turn on oscillator
	Wire.endTransmission();
	DalekDriver::blinkRate(HT16K33_BLINK_OFF);
	DalekDriver::setBrightness(15); // max brightness
	DalekDriver::ClearBuffer();		// switch all LEDS off.
	DalekDriver::ShowLeds();		// Send tothe HT16K33
}

void DalekDriver::ClearBuffer(void)
{
	for (byte i = 0;i<16;i++) {
		_Buffer[i]=0;
	}
}

void DalekDriver::ShowLeds(void)
{
	Wire.beginTransmission(_HexAddress);
	Wire.write((uint8_t)0x00); // start at address $00

	for (uint8_t i=0; i<16; i++) {
	Wire.write(_Buffer[i]);      
	}
	Wire.endTransmission(); 
	//readKeys();
}

void DalekDriver::setBrightness(uint8_t bright) {
  if (bright > 15) bright = 15;
  Wire.beginTransmission(_HexAddress);
  Wire.write(0xE0 | bright);
  Wire.endTransmission();
  //readKeys();    
}

void DalekDriver::blinkRate(uint8_t b) {
  Wire.beginTransmission(_HexAddress);
  if (b > 3) b = 0; // turn off if not sure
  Wire.write(HT16K33_BLINK_CMD | HT16K33_BLINK_DISPLAYON | (b << 1)); 
  Wire.endTransmission();
}

/*
LED buffer looks like this:

/* Color has for the column the following vaules:
	bit 0: blue
	bit 1: green
	bit 2: red		
	The order is (blue, green, red)1  (blue, green, red)2 etc. : 15 in total for 5 in a column (0..14).
	
	 bit   row  color 

	+-----------------+
	|  0 row 0  BLUE  | TOP ROW!!!
	|  1 row 0  GREEN | 
	|  2 row 0  RED   | 
	+-----------------+
	|  3 row 1  BLUE  | 
	|  4 row 1  GREEN | 
	|  5 row 1  RED   | 
	+-----------------+
	|  6 row 2  BLUE  | 
	|  7 row 2  GREEN |      3x8=24 columns, 8 in each driver, every time one word of 15 bits, 
	|  8 row 2  RED   |  the 3x8 bits left (each bit 16 of a word) are used for column25.
	+-----------------+
	|  9 row 3  BLUE  | 
	| 10 row 3  GREEN | 
	| 11 row 3  RED   | 
	+-----------------+
	| 12 row 4  BLUE  | 
	| 13 row 4  GREEN | 
	| 14 row 4  RED   | BOTTOM ROW
	+-----------------+
	
Bit 15 is specially arranged to fit 2 additional RGB leds for the last column:	

==These are the colors for the BUFFER leds, NOT FOR COL 24, but only COL 0..23!!!!!!
	  RedPart(byte color) {  RedPart = (color & 4) >> 2; }
	GreenPart(byte color) {GreenPart = (color & 2) >> 1; }
	 BluePart(byte color) { BluePart = (color & 1);}

	The COL24 RGB LEDS are stored in the last three bytes of Buffer4:
    Digits(3, 16) = RedSet(AllLEDS(1, MaxCol)) + 2 * GreenSet(AllLEDS(1, MaxCol)) + 4 * BlueSet(AllLEDS(1, MaxCol)) + 8 * RedSet(AllLEDS(2, MaxCol)) + 16 * GreenSet(AllLEDS(2, MaxCol)) + 32 * BlueSet(AllLEDS(2, MaxCol))
    Digits(2, 16) = RedSet(AllLEDS(3, MaxCol)) + 2 * GreenSet(AllLEDS(3, MaxCol)) + 4 * BlueSet(AllLEDS(3, MaxCol)) + 8 * RedSet(AllLEDS(4, MaxCol)) + 16 * GreenSet(AllLEDS(4, MaxCol)) + 32 * BlueSet(AllLEDS(4, MaxCol))
    Digits(1, 16) = RedSet(AllLEDS(5, MaxCol)) + 2 * GreenSet(AllLEDS(5, MaxCol)) + 4 * BlueSet(AllLEDS(5, MaxCol))

	byte 15: 6 bits (van LSB-> bit 6): row1(red), row1(green), row1(blue)    row2(red), row2(green), row2(blue)
	byte 14: 6 bits (van LSB-> bit 6): row3(red), row3(green), row3(blue)    row4(red), row4(green), row4(blue)
	byte 13: 3 bits (van LSB-> bit 3): row5(red), row5(green), row5(blue)
	
	Buffer1
	+---------+---------+---------+---------+---------+---------+---------+---------+
	| 15    7 | 15    6 | 15    5 | 15    4 | 15    3 | 15    2 | 15    1 | 15    0 |
	| L eye   | R eye   | Row 1   | Row 1   |  Row 1  |  Row 0  | Row 0   | Row 0   |
	| BLUE    | GREEN   | BLUE    | GREEN   |  RED    |  BLUE   | GREEN   | RED     |
	| -or- NU | -or- NU |         |         |         |         |         |         |
	+---------+---------+---------+---------+---------+---------+---------+---------+

	Buffer2
	+---------+---------+---------+---------+---------+---------+---------+---------+
	| 15    7 | 15    6 | 15    5 | 15    4 | 15    3 | 15    2 | 15    1 | 15    0 |
	| L eye   | R eye   | Row 3   | Row 3   | Row 3   | Row 2   | Row 2   | Row 2   |
	| RED     | BLUE    | BLUE    | GREEN   | RED     | BLUE    | GREEN   | RED     |
	| -or- NU | -or- NU |         |         |         |         |         |         |
	+---------+---------+---------+---------+---------+---------+---------+---------+

	Buffer3
	+---------+---------+---------+---------+---------+---------+---------+---------+
	| 15    7 | 15    6 | 15    5 | 15    4 | 15    3 | 15    2 | 15    1 | 15    0 |
	| R eye   | R eye   | nose    | nose    | nose    | Row 4   | Row 4   | Row 4   |
	| GREEN   | RED     | BLUE    | GREEN   | RED     | BLUE    | GREEN   | RED     |
	| -or- NU | -or- NU | -or- NU | -or- NU | -or- NU |         |         |         |
	+---------+---------+---------+---------+---------+---------+---------+---------+
		
Summary for the whole buffer 1,(2 and 3), including row 24 for the 3 buffers:

In a 16 byte buffer depicted below as 2 columns of 8 bytes each, counting like this:

		+--------+--------+
		|   0    |  1     |
		+--------+--------+
		|   2    |  3     |
		+--------+--------+
		|   4    |  5     |
		+--------+--------+
		|   6    |  7     |
		+--------+--------+
		|   8    |  9     |
		+--------+--------+
		|  10    | 11     |
		+--------+--------+
		|  12    | 13     |
		+--------+--------+
		|  14    | 15     |
		+--------+--------+

		Per bit the mapping for the LED buffers looks like this:
		2 buffers with
		8*5 + 2 = 42 RGB LEDS = 126 LEDS
		1 buffer with
		8*5 + 2 = 42 RGB LEDS = 123 LEDS
		
		Over the 3 buffers, 9 LEDS are not used.

  
	        Row 0          Row 1          Row 2          Row 3          Row 4          
       |+0 0|+0 1|+0 2|+0 3|+0 4|+0 5|+0 6|+0 7|+1 0|+1 1|+1 2|+1 3|+1 4|+1 5|+1 6|+1 7|
       +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
col  0 | B    G    R  | B    G    R  | B    G  - R  | B    G    R  | B    G    R  |c 24|
     8 |              |              |              |              |              |r0 R|
    16 +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+ 2  +
col  1 | B    G    R  | B    G    R  | B    G  - R  | B    G    R  | B    G    R  | 4 G|
     9 |              |              |              |              |              |    |
    17 +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+    +
col  2 | B    G    R  | B    G    R  | B    G  - R  | B    G    R  | B    G    R  |   B|
    10 |              |              |              |              |              |    |
    18 +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
col  3 | B    G    R  | B    G    R  | B    G  - R  | B    G    R  | B    G    R  |c 24|
    11 |              |              |              |              |              |r1 R|
    19 +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+ 3  +
col  4 | B    G    R  | B    G    R  | B    G  - R  | B    G    R  | B    G    R  |nu G|
    12 |              |              |              |              |              |    |
    20 +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+    +
col  5 | B    G    R  | B    G    R  | B    G  - R  | B    G    R  | B    G    R  |   B|
    13 |              |              |              |              |              |    |
    21 +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
col  6 | B    G    R  | B    G    R  | B    G  - R  | B    G    R  | B    G    R  |  NU|
    14 |              |              |              |              |              |    |
    22 +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+    +
col  7 | B    G    R  | B    G    R  | B    G  - R  | B    G    R  | B    G    R  |    |
    15 |              |              |              |              |              |    | 
    23 +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+

	Per bit the mapping for the Ring / eyes / nose look like this:

	24+3x5=39 RGB LEDS (117 LEDS) over these bytes (one LED per bit) in the head divided like this:
	(9+2=11 LEDS not used)

  |+0 0|+0 1|+0 2 0  3|+0 4|+0 5|+0 6|+0 7|+1 0|+1 1|+1 2|+1 3|+1 4|+1 5|+1 6|+1 7|
  +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
0 | RING 0       | RING 8       | RING 16 -    | R EYE 0      | L EYE 0      |L  B|
  | B    G    R  | B    G    R  | B    G  - R  | B    G    R  | B    G    R  |EYE |
  +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+4   +
2 | RING 1       | RING 9       | RING 17 -    | R EYE 1      | L EYE 1      |   G|
  | B    G    R  | B    G    R  | B    G  - R  | B    G    R  | B    G    R  |    |
  +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+    +
4 | RING 2       | RING 10      | RING 18 -    | R EYE 2      | L EYE 2      |   R|
  | B    G    R  | B    G    R  | B   G   - R  | B    G    R  | B    G    R  |    |
  +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
6 | RING 3       | RING 11      | RING 19 -    | R EYE 3      | L EYE 3      |    |
  | B    G    R  | B    G    R  | B   G   - R  | B    G    R  | B    G    R  | NU |
  +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
8 | RING 4       | RING 12      | RING 20 -    | R EYE 4      | NOSE 0       |   B|
  | B    G    R  | B    G    R  | B   G   - R  | B    G    R  | B    G    R  |NOSE|
  +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+4   +
10| RING 5       | RING 13      | RING 21 -    |              | NOSE 1       |   G|
  | B    G    R  | B    G    R  | B   G   - R  |              | B    G    R  |    |
  +----+----+----+----+----+----+----+----+----+    +    +    +----+----+----+    +
12| RING 6       | RING 14      | RING 22 -    |              | NOSE 2       |   R|
  | B    G    R  | B    G    R  | B   G   - R  |      NU      | B    G    R  |    |
  +----+----+----+----+----+----+----+----+----+    +    +    +----+----+----+----+
14| RING 7       | RING 15      | RING 23 -    |              | NOSE 3       |    |
  | B    G    R  | B    G    R  | B   G   - R  |              | B    G    R  | NU |
  +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+


*/

void DalekDriver::SetRing(uint8_t Led, uint8_t color) {
	switch (Led>>3) {
	case 0:
		//Serial.print("LED ");Serial.print(Led);Serial.print(" ");Serial.print(Led<<1);Serial.print(" ");Serial.println(Led>>3);
		_Buffer[Led<<1] &=    0b11111000;
		_Buffer[Led<<1] |=    color;
		break;
	case 1:
		//Serial.print("LED ");Serial.print(Led);Serial.print(" ");Serial.print((Led-8)<<1);Serial.print(" ");Serial.print(Led>>3);
		//Serial.print(" COL ");Serial.print(color);Serial.print("->");;Serial.println(color<<3);
		_Buffer[(Led-8)<<1] &=  0b11000111;
		_Buffer[(Led-8)<<1] |=  (color<<3);
		break;
	case 2:
		//Serial.print("LED ");Serial.print(Led);Serial.print(" ");Serial.print((Led-16)<<1);Serial.print("-");Serial.print(((Led-16)<<1)+1);Serial.print(" ");Serial.print(Led>>3);
		//Serial.print(" COL ");Serial.print(color);Serial.print("->");;Serial.println((color&0b11)<<6);
		_Buffer[(Led-16)<<1] &= 0b00111111;
		_Buffer[(Led-16)<<1] |= ((color&0b11)<<6);
		_Buffer[(((Led-16)<<1)+1)] &= 0b11111110;
		if (IsRed(color)) _Buffer[(((Led-16)<<1)+1)] |=0b1;
		break;
	}
}

void DalekDriver::SetEyeR(byte SetColor, byte mask) {
#ifdef SWAP_LEFT_RGB
	uint8_t color = SwapRGB(SetColor);
#else
	uint8_t color = SetColor;
#endif	
	
  for (byte i=0; i<=4; i++) {
	//Serial.print("R i   :");Serial.println(i);
    if (mask & (1<<i)) {
		//Serial.print("R mask: ");Serial.println(mask);
		//Serial.print("R Pntr: ");Serial.println((i<<1)+1);
		//Serial.print("R Colr: ");Serial.println(color<<1);
		_Buffer[(i<<1)+1] &= 0b11110001;
		_Buffer[(i<<1)+1] |= (color << 1);
	}
  }
}

void DalekDriver::SetEyeL(byte SetColor, byte mask) {
#ifdef SWAP_RIGHT_RGB
	uint8_t color = SwapRGB(SetColor);
#else
	uint8_t color = SetColor;
#endif	
	
  for (byte i=0; i<=4; i++) {
	//Serial.print("\nL i   :");Serial.println(i);
    if (mask & (1<<i)) {
		//Serial.print("L mask: ");Serial.println(mask);
		switch (i) {
		case 4:
			if (IsBlue(color)) {
				_Buffer[1] |= 0b10000000;
			} else {
				_Buffer[1] &= 0b01111111;
			}
			if (IsGreen(color)) {
				_Buffer[3] |= 0b10000000;
			} else {
				_Buffer[3] &= 0b01111111;
			}
			if (IsRed(color)) {
				_Buffer[5] |= 0b10000000;
			} else {
				_Buffer[5] &= 0b01111111;
			}
			break;
		default:
			_Buffer[(i<<1)+1] &= 0b10001111;
			_Buffer[(i<<1)+1] |= (color << 4);
			//Serial.print("L Pntr: ");Serial.println((i<<1)-1);
			//Serial.print("L Colr: ");Serial.print(color);Serial.print(" ");Serial.println(color<<4);
		}
	}
  }
}

void DalekDriver::SetNose(uint8_t SetColor, uint8_t mask) {

#ifdef SWAP_NOSE_RGB
	uint8_t color = SwapRGB(SetColor);
#else
	uint8_t color = SetColor;
#endif
	
  for (uint8_t i=0; i<=4; i++) {
	//Serial.print("\nN i   :");Serial.println(i);
    if (mask & (1<<i)) {
		//Serial.print("N mask: ");Serial.println(mask);
		switch (i) {
		case 4:
			if (IsBlue(color)) {
				_Buffer[9] |= 0b10000000;
			} else {
				_Buffer[9] &= 0b01111111;
			}
			if (IsGreen(color)) {
				_Buffer[11] |= 0b10000000;
			} else {
				_Buffer[11] &= 0b01111111;
			}
			if (IsRed(color)) {
				_Buffer[13] |= 0b10000000;
			} else {
				_Buffer[13] &= 0b01111111;
			}
			break;
		default:
			_Buffer[(i<<1)+9] &= 0b10001111;
			_Buffer[(i<<1)+9] |= (color << 4);
			//Serial.print("N Pntr: ");Serial.println((i<<1)-1);
			//Serial.print("N Colr: ");Serial.print(color);Serial.print(" ");Serial.println(color<<4);
		}
	}
  }
}

/* Dalekdriver is a bit strange, as I shifted the hardware 1 LED (so skipped 1) and the 
   skipped first real LED became wired to last outpus of the HT16K33
   
   If you want this, set no define.
   
   If you want the "regular" order, so top LED A0..A2, next A3..A5 etc last LED A12..A14, then set define REGULAR_LEDS
*/

void DalekDriver::SetLed(byte row, byte col, byte SetColor) {

#ifdef SWAP_LEDS_RGB
	byte color = SwapRGB(SetColor);
#else
	byte color = SetColor;
#endif

#ifdef REGULAR_LEDS
	/*
	This is preparing for V7 of the hardware, in case I do not start at the second row and swap row1 and 5.
	*/
	switch (row) {
	case 0:
		_Buffer[2*col] &= 0b11111000;		// & is the logical AND, so bits wit
		_Buffer[2*col] |= color;
		break;
	case 1:
		_Buffer[2*col] &= 0b11000111;
		_Buffer[2*col] |= color<<3;
		break;
	case 2:
		_Buffer[2*col] &= 0b00111111;
		_Buffer[2*col] |= (color&0b11) <<6;
		if (IsRed(color)) _Buffer[2*col+1] |= 0b1; else _Buffer[2*col+1] &= 0b11111110;
		break;
	case 3:
		_Buffer[2*col+1] &= 0b11110001;
		_Buffer[2*col+1] |= color<<1;
		break;
	case 4:
		_Buffer[2*col+1] &= 0b10001111;
		_Buffer[2*col+1] |= color<<4;
		break;
	}
#else
	switch (row) {
	case 0:
		_Buffer[2*col+1] &= 0b10001111;
		_Buffer[2*col+1] |= color<<4;
		break;
	case 1:
		_Buffer[2*col] &= 0b11111000;
		_Buffer[2*col] |= color;
		break;
	case 2:
		_Buffer[2*col] &= 0b11000111;
		_Buffer[2*col] |= color<<3;
		break;
	case 3:
		_Buffer[2*col] &= 0b00111111;
		_Buffer[2*col] |= (color&0b11) <<6;
		if (IsRed(color)) _Buffer[2*col+1] |= 0b1; else _Buffer[2*col+1] &= 0b11111110;
		break;
	case 4:
		_Buffer[2*col+1] &= 0b11110001;
		_Buffer[2*col+1] |= color<<1;
		break;
	}
#endif
}

void DalekDriver::SetLedColumn(byte col, int Mycolumncolors) {
	// RGB swapping for experts: MASK R G and B and shift R and B...
	int columncolors = Mycolumncolors;
	
	#ifdef SWAP_LEDS_RGB
	columncolors = ((columncolors & 0b0001001001001001) << 2 ) | (columncolors & 0b0010010010010010) | ((columncolors &0b0100100100100100) >> 2 );
	#endif

	/* Need to shift things a bit, as default soldering skipped the first (top) row of LEDS
	*/
	#ifdef REGULAR_LEDS
	_Buffer[2*col]   =  columncolors & 0x00FF;
	_Buffer[2*col+1] = (columncolors & 0xFF00)>>8;
	#else
	_Buffer[2*col] =   ((columncolors & 0b0000011111111000) >> 3);
	_Buffer[2*col+1] = ((columncolors & 0b0111100000000000) >> 11) | ((columncolors & 0b0000000000000111) << 4);
	#endif
}

void DalekDriver::SetLedExtra(byte row, byte SetColor) {

#ifdef SWAP_EXTRA_RGB
	byte color = SwapRGB(SetColor);
#else
	byte color = SetColor;
#endif


	if (row>1) row=1;
	if (IsBlue(color)) {
		_Buffer[5+6*row] |= 0b10000000;				
	} else {
		_Buffer[5+6*row] &= 0b01111111;				
	}
	if (IsGreen(color)) {
		_Buffer[7+6*row] |= 0b10000000;				
	} else {
		_Buffer[7+6*row] &= 0b01111111;				
	}
	if (IsRed(color)) {
		_Buffer[9+6*row] |= 0b10000000;				
	} else {
		_Buffer[9+6*row] &= 0b01111111;				
	}
}

bool DalekDriver::IsRed(byte color) {
	return (color & 0b100);
	/*
	switch (color) {
	case 4: case 5: case 6: case 7:
		return true;
	default:
		return false;
	}
	*/
}

bool DalekDriver::IsGreen(byte color) {
	return (color & 0b10);
	/*
	switch (color) {
	case 2: case 3: case 6: case 7:
		return true;
	default:
		return false;
	}
	*/
}

bool DalekDriver::IsBlue(byte color) {
	return (color & 0b1);
	/*
	switch (color) {
	case 1: case 3: case 5: case 7:
		return true;
	default:
		return false;
	}
	*/
}

void DalekDriver::readKeys() {
  //Set reading address for the device
  Wire.beginTransmission(_HexAddress); // transmit to device HT16K33
  Wire.write(byte(0x60));      // sets register pointer INT register (0x60)
  Wire.endTransmission();      // stop transmitting
  
  Wire.requestFrom((int)_HexAddress, 1);    // request 6 bytes from slave device #2
  
  while(Wire.available())    					// slave may send less than requested
  { 
    byte flag = Wire.read();    				// receive the interupt flag: no hardware interupt, as ALL row are needed
	if (flag!=0) {								// We received at least one key...
	  Wire.beginTransmission(_HexAddress); 		// transmit to device HT16K33
	  Wire.write(byte(0x40));      				// sets register pointer KEY scan buffer (0x40)
	  Wire.endTransmission();      				// stop transmitting
	  Wire.requestFrom((int)_HexAddress, 6);    // request 6 bytes: reading the whole buffer (which also resets the INT flag)
 
	  uint8_t i=0;
	  while(Wire.available())    				// slave may send less than requested
	  { 
		_KeyBuffer[i] = Wire.read();    			// receive a byte as character
		if (_KeyBuffer[i]!=0) {
			Serial.print(_HexAddress);
			Serial.print(" key ");
			Serial.print(i);
			Serial.print(": ");
			Serial.println(_KeyBuffer[i]);
		}
		i++;
	  }
	}
  }
}

uint8_t DalekDriver::SwapRGB(uint8_t color) {
/*	This routine swaps from RGB to BGR
	Use it when colors appear swapped, so e.g. when LEDS are soldered in with the wrong pin order.
	000 -> 000 (no swap necessary)
	001 -> 100 (1 -> 4) 
	010 -> 010 (no swap necessary)
	011 -> 110 (3 -> 6)
	100 -> 001 (4 -> 1)
	101 -> 101 (no swap necessary)
	110 -> 011 (6 -> 3)
	111 -> 111 (no swap necessary)
*/	
	return (((color&0b001) << 2) | (color&0b010) | ((color&0b100) >> 2));
/*
	switch (color) {
	case 1:
		return 4;
	case 3:
		return 6;
	case 4:
		return 1;
	case 6:
		return 3;
	default:
		return color;
	}
*/
}

