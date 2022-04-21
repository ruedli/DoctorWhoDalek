/*
	25.02.2017 version 0.0: written to support my drivers for 128 LEDS
	
	The leds are assumed to be in a configuration using RGB Leds, arranged in a matrix of 8 columns with 5 RGB LEDS.
	My "Dalek" on top of the Doctor WHO pinball machine, uses now three of these matrixec _AND_ I chenged from TM1640 drivers to HT16K33 (i2c) chips.
	
	This library separates the memory and buffer management from the chip communication. 
	It supports both communication modes, TM1640 and HT16K33.
	
	TM1640 mode can be activated using a "#define" named "__TM1640". If this define does NOT exist, HT16K33 mode is assumend.
	NOTE: CURRENTLY THIS IS NOT IMPLEMENTED, ONLY HT16K33 !!!!
	
	Have fun,
	
	Ruud Rademaker
	ruud at rademaker dot gmail dot comm
	
	Dübendorf, 25.02.2017
*/

#ifndef DalekDriver_h
#define DalekDriver_h

#include "Arduino.h"

#define HT16K33_BLINK_CMD 0x80
#define HT16K33_BLINK_DISPLAYON 0x01
#define HT16K33_BLINK_OFF 0
#define HT16K33_BLINK_2HZ  1
#define HT16K33_BLINK_1HZ  2
#define HT16K33_BLINK_HALFHZ  3

#define HT16K33_CMD_BRIGHTNESS 0x0E

class DalekDriver
{
	public:
		DalekDriver(uint8_t HexAddress=0x70);
		void InitDriver(void);
		void ClearBuffer(void);
		void ShowLeds(void);
		void setBrightness(uint8_t bright = 15);
		void blinkRate(uint8_t b);
		void SetRing(uint8_t Led, uint8_t Color);
		void SetEyeL(byte color, byte mask);
		void SetEyeR(byte color, byte mask);
		void SetNose(byte color, byte mask);
		void SetLed(byte row, byte col, byte color);
		void SetLedExtra(byte row, byte color);
		void readKeys(void);
		void SetLedColumn(byte col, int columncolors);
		
	private:
		uint8_t _Buffer[16];    //This has room for the 128 bits: 16x8=128 bits...
		uint8_t _HexAddress;    //This holds the I2C address for the driver
		byte _KeyBuffer[6];		//For holding the input from keys.
		bool IsBlue(byte color);
		bool IsGreen(byte color);
		bool IsRed(byte color);
};

#endif

