// Set platform
#define ESP8286
//#define Arduino

#include <ESP8266WiFi.h>
#include <WiFiClient.h>

// include MDNS (for WiFimanager)
#ifdef ESP8286
#include <ESP8266mDNS.h>
#endif

//Not needed! although the OTA example sketch contained them
#ifndef platformio_build
#include <WiFiUdp.h>
#endif

// Set these defines to match your environment, copy initial file from OctoPlugout.config.h.RELEASE ========
#include "DalekConfig.h"

#ifdef ESP8286
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ArduinoOTA.h>

// Includes for MQTT
#include <PubSubClient.h>
#endif

#include <Metro.h>
#include <Wire.h>  // Comes with Arduino IDE, used for driving the LCD using 
//#include <LiquidCrystal_I2C.h>
//#include <SoftwareSerial.h>

#ifdef Arduino
#include <EEPROM.h>
#elif defined ESP8286
#include <EEPROM.h>
#endif

#include <Servo.h>

#ifdef __TM1640
#include <TM1638.h> // required because the way arduino deals with libraries
#include <TM1640.h>
#endif

#ifdef __HT16K33
// include the wire libray for i2c
#include <Wire.h>  // Comes with Arduino IDE
// include my driver library:
#include <DalekDriver.h>    // A library for managing a driver for 128 LEDS; 
							// it supports a configuration of 8 columns with 5 RGB LEDS in them. (so 8x5x3=120 LEDS)
							// The remaining 8 LEDS can also be set. 
#endif

#define NOCATCH_UP
#include <Metro.h>

#ifdef Arduino
// SPI library for SD card, + SD card library
#include <SPI.h>
#include <SD.h>
//Define the used pins:
/*
 The circuit for the SD card reader:
  * SD card attached to SPI bus as follows:
 ** MOSI - pin 11 on Arduino Uno/Duemilanove/Diecimila
 ** MISO - pin 12 on Arduino Uno/Duemilanove/Diecimila
 ** CLK  - pin 13 on Arduino Uno/Duemilanove/Diecimila
 ** CS   - depends on your SD card shield or module, it is pin 10 in this scetch
*/
#define __chipSelect 10
#endif
#ifdef ESP8266
//improves read speed for LittleFS
#define CONFIG_LITTLEFS_CACHE_SIZE 512	

#include <FS.h>
#include <LittleFS.h>
#endif


#ifdef __TM1640
#define __Clock_1 4
#define __Data__1 5
#define __Clock_2 6
#define __Data__2 7
#define __Clock_3 8
#define __Data__3 9
// Each TM1640 is connected using a distinct Data and a clock pin:
LedDriver module1(__Data__1, __Clock_1);
LedDriver module2(__Data__2, __Clock_2);	
LedDriver module3(__Data__3, __Clock_3);
#endif 

#ifdef __HT16K33
// Leftmost LEDS
#define __DriverAddress1 0x70
// Center LEDS 
#define __DriverAddress2 0x71 
// Righthand LEDS 
#define __DriverAddress3 0x72
// 15 LEDS for nose(5RGB), eyes(2x5RGB) and a ring with 24 RGB LEDS on the head. 
#define __DriverAddress4 0x73

DalekDriver DisplayDrivers[3] = { DalekDriver(__DriverAddress1), DalekDriver(__DriverAddress2), DalekDriver(__DriverAddress3)};
DalekDriver HeadDriver(__DriverAddress4);

#endif 

//Loop timing using metro for the head.
Metro _BLINKprocessing = Metro(10000);	//Used for blinking the head LEDS
Metro _FLASHprocessing = Metro(10000);	//Used for flashing the head LEDS
Metro _STROBEprocessing = Metro(10000); //Used for strobing the head LEDS 

boolean EscHit = false;

boolean HighResMode = false;

//Normalmode display on lowres LEDS
#define __NormalMode 0 
//Display compressed, show left: 12 blanks right
#define __LeftMode 1 
//Display compressed, show centered: 6 blanks left and right
#define __CenterMode 2 
//Display compressed, show right: 12 blanks left
#define __RightMode 3 
int ModeDisplay = __NormalMode;

struct TimingType {
    unsigned long PatternTime[2];
	byte pointerFile[2];
	byte numberFiles[2];
    unsigned long Sol14ExtendTime;
    unsigned long Sol25ExtendTime;
	uint8_t AnimationIntensity;
	uint8_t MinIntensity;
	uint8_t MaxIntensity;
	Metro ActiveProcessing;
	Metro TimeProcessing;
	File dataFile;
	boolean HasFiles[2];
	boolean StreamIsActive;
	boolean GetNextPattern;
	boolean PatternOK;
	byte StreamSource;
	boolean WeJustBecameActive;
	boolean HowLongToRemainActive;
	boolean SwapActiveSleeping;
	const char *initS[2];
};

	// For the bug that with "short times", the second loop (activeprocessing check) is fired, just after activating.
	// To fix that we slightly delay processing till later:
	//
	// Integrated in TimingType
	//static boolean WeJustBecameActive[3] = {false, false, false};
	//static boolean HowLongToRemainActive[3] ={0,0,0}; //To be filled in when getting active.


// These properties are only relevant for the Head, so they are not contained in the TimingType structure
unsigned HP_BlinkTime[2] = {500, 500};
unsigned HP_FlashTime[2] = {200, 200};
unsigned HP_StrobeTime[2] = {100, 100};

boolean Blink_on=false;
boolean Strobe_on=false;
boolean Flash_on=false;

#define N_L_eye 1
#define N_R_eye 2
#define N_Nose  3

// Animation parameters and constants
#define __No  			0
#define __Yes 			1
#define __Continuous    2
#define __OFF 			3
#define __ON  			4
#define __BLINK		    8
#define __FLASH		   12
#define __STROBE	   20
#define __STROBEBLINK  24
#define __STROBEFLASH  28
#define __ApplySTROBE  (__STROBE - __ON)

#define __stream_LED 0
#define __stream_HEAD 1
#define __stream_RING 2

#define __stream_Active 0
#define __stream_Sleeping 1

#define __streamFrom_SD 0
#define __streamFrom_MQTT_SD 1
#define __streamFrom_MQTT_Wait 2
#define __streamFrom_MQTT_Loop 3
#define __streamFrom_Serial 4

//Headanimation, stringbased
TimingType HP_TimeTiming[3]  = {{	.Sol14ExtendTime = 0, 
									.Sol25ExtendTime = 5000,
									.AnimationIntensity = 7,
									.MinIntensity = 0,
									.MaxIntensity = 7,
									.StreamIsActive = false,
									.GetNextPattern = false,									
									.PatternOK = false,
									.StreamSource = __streamFrom_SD,
									.WeJustBecameActive = false,
									.HowLongToRemainActive = 0,
									.SwapActiveSleeping = false,
									.initS = {"Y 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,130,0,0,68,0,0,40,0,0,16,0,0,1Z", "Y 0Z"}},
								{	.Sol14ExtendTime = 5,
									.Sol25ExtendTime = 0,
									.AnimationIntensity = 7,
									.MinIntensity = 0,
									.MaxIntensity = 7,
									.StreamIsActive = false,
									.GetNextPattern = false,									
									.PatternOK = false,
									.StreamSource = __streamFrom_SD,
									.WeJustBecameActive = false,
									.HowLongToRemainActive = 0,
									.SwapActiveSleeping = false,
									.initS = {"C444", "C000"}},
								{	.Sol14ExtendTime = 5,
									.Sol25ExtendTime = 0,
									.AnimationIntensity = 7,
									.MinIntensity = 0,
									.MaxIntensity = 7,
									.StreamIsActive = false,
									.GetNextPattern = false,									
									.PatternOK = false,
									.StreamSource = __streamFrom_SD,
									.WeJustBecameActive = false,
									.HowLongToRemainActive = 0,
									.SwapActiveSleeping = false,
									.initS = {"A00000000004Z", "A0Z"}}
								};
														
#define __update_display_needed 2 // bit 1: bit 0 indicates the stream: active (0) -or- sleeping (1)
#define __update_display_not_needed 0

boolean ForceOut = false;   // escape out when entering redirection of the SD stream to serial...
//byte ShowSerialInput = __stream_NoStream; // determine whether to loop, or to show a fixed pattern as typed in...

byte L_eye = 4;
byte R_eye = 4;
byte Nose = 4;

byte R_eye_pattern[2] = {__ON,__ON};
byte L_eye_pattern[2] = {__ON,__ON};
byte Nose_pattern[2] = {__ON,__ON};

byte R_eye_nextcol[2] = {1,1};
byte L_eye_nextcol[2] = {1,1};
byte Nose_nextcol[2] = {1,1};

boolean ShowFilenames=false;	  // This shows the filenames on serial log. use command A -> T to toggle.

////////////////////////////////////

#define __RR_PROJECT 10      // project stored in EEPROM(100), so that parameters are not read from another project.
#define __RR_VERSION 4       // version for this project that the parameters can be interpreted for, to avoid incompatibilities.
#define __RR_SW_Version "4.0"// The software (WIP)

#define __ShapeSine 0
#define __ShapeSaw 1
#define __ShapeBlock1 2
#define __ShapeBlock2 3
#define __ShapeRandom 4

#define __MoveByPinball 0   			// Who (listening to the Solenoid25 signal
#define __MoveAutonomous 1  			// Sweeping moving all by itself continuously
#define __MoveFixedLeft 2				// Fixed in left position (adjustable)
#define __MoveFixedCenter 3 			// Fixed in center position (adjustable)
#define __MoveFixedRight 4				// Fixed in right position (adjustable)
#define __MoveByPinball_stop_nocenter 5 // Who (listening to the Solenoid25 signal, stop immedieately when solenois signal disappears
#define __MoveByPinball_stop_center 6	// Who (listening to the Solenoid25 signal, stop immedieately when solenois signal disappears, center slowly

#define __DelayTimeForCentering 60000

//#define DEBUG
#ifdef DEBUG
#define DEBUG_OUT(a,b) Serial.print(a); Serial.print(F(":\t")); Serial.println(b);
#else
#define DEBUG_OUT(a,b) // a b
#endif

/* Doctor WHO, moving Dalek
 by Ruud Rademaker
 
 The first 10 Bally Doctor WHO Pinball machines (prototypes) had a moving Dalek on the top of
 the cabinet which was removed lateron for production cost reasons.
 
 Solutions exist to implement a moving Dalek, using relatively expensive motors and a gear (for left-right-left movement)
 together with an opto switch to detect the mid position.
 
 This project realizes a cheap and easy to install Arduino based solution, accomplished from:
 
 Further, since version 3
 - The TM1638/TM1640 is now obsolete, instead an LCD display is used
 - As input options there are now:
   o A 5 button keyboard, basical up/down left/right and enter.
   o a rotary switch (software in place, hardware is bouncing too much... for now it is disabled.
  
 INPUTS
 - PIN   6: the solenoid drive #25 - to know whether movement is desired during gameplay.
 - PIN   x: the solenoid drive #14 - to know when to flash the LEDS in the head (new in v 4.0)
 - PIN  A0: 5 buttons keyboard, with resistors to distinct 5 keys using one analog input. This uses the arrangement as off the robo LCD shield
 - PIN 7 : Rotary switch button
 - PIN 8 : Rotary switch A
 - PIN 9 : Rotary switch B
 
 OUTPUTS
 - PIN 5 opto switch output for Switch 81 in order to signal the mid position
  (The pinball machine detects that the Dalek is present and in the middle in this way).
 - PIN 4 A servo signal to sweep the head
 
 PARAMETERS
 All parameters are saved to EEPROM upon command.
 The device can be controlled and programmed using the Serial IO.

As of version 4.0, what used to be in two separate arduino's is now combined in one ESP32 D1 mini:

Animated RGB Leds: Designed for the DALEK of a dr WHO pinball 
This sketch supports animation of RBG LEDS using TM1640 modules.

One TM1640 or HT16K33 chip can drive 128 LEDS
This implies: maximum of 42 RGB leds (126 of the 128) and still 2 LEDS "left" 
              -or- (like I use them) 40 RGB LEDS (120 of the 128) and 8 LEDS left. 
			  
A TM1640  driver uses COMMON   ANODE LEDS for column 1-24 and common CATHODE for column 25
A HT16K33 driver uses COMMON CATHODE LEDS for column 1-24 and common   ANODE for column 25

The LEDS can either be driven in a LOWRES mode: using only 2 drivers
Driver 1: 8x5 = 40 RGB LEDS
Driver 2: 5x5 = 25 RGB LEDS

Alternatively, HIHGRES mode you can hook up 3 driver chips and control:
TM1640 1: (Column  1- 8) 8x5 = 40 RGB LEDS (120 of the 128)
TM1640 2: (Column  9-16) 8x5 = 40 RGB LEDS (120 of the 128)
TM1640 3: (Column 17-24) 8x5 = 40 RGB LEDS (120 of the 128)

Highres is easier to solder, as the LEDS are closer together, and the wires can easily be soldered together.
I do recommend however putting the LEDS on PCB's: per driver one board with 2 rows 
(interfacing to the driver and other boards), and tow PCB's with 3 rows each. This makes 8 rows in total.
I based it on stripboard that connects all the non-common wires of the LEDS. 
The common wires are soldered mid-air, above the board: 5 common wires of the 5 RGB LEDS in each column,
giving you a matrix of EACH driver (there are 3 in total) with 8x15 LEDs, so 8x5 RGB leds.  

The remaining 3 times 8 LEDS = 24 LEDS are used as follows:
Column 25: 1x5 = 5 RGB LEDS, uses therefore 15 of the remaining 24 LEDS of the three drivers.
This column 25 is built using 
- COMMON CATHODE for the TM1640 -or-
- COMMON   ANODE for the HT16K33

The LAST 9 remaining LEDS are will be used (together with optocouplers to couple leds
    from different drivers to one common cathode LED) for a RGB LEDs in the 
- NOSE, 
- RIGHT EYE
- LEFT EYE

-or-

- A 4th driver HT16K33 totally dedicated to the HEAD leds

See DalekDriver.cpp for layout of the LEDS over the buffers.

All rights reserved,
have fun 

Ruud Rademaker

firstname.lastname(at)gmail.com

The animations are stored in text files on an SD card, or in the LittleFS file system.
Text files must be like: SEQnnnn.TXT where n is a digit [0-9]. So: SEQ0000.TXT, SEQ0001.TXT, SEQ0002.TXT etc...
Version 4.0 uses 6 directories (ending in H or R: while the head is not moving (H for holding) or when the head is moving (R for Running)) to store files:

LedH and LedR: for the 125 Dalek body RGB leds
HeadH and HeadR: for the 3x5 nose, left eye and right eye LEDS.
RingH and RingR: for the 24 RGB leds in the head (ring shaped).

There are two modes in which these are animated:
- Animation
- SingleCommand

In animation mode, files are read from SD / LittleFS files, or 
when no files are available an in program default per stream is used.

In singleCommand mode, the LEDS follow the command provided over serial or MQTT.

When in animation mode, enter ESC or @ then the mode you want to be providing commands for:
L for leds in the base
H for the eyes/nose (left/nose/right)
R for the ring of leds in the head
A resume animation mode.

-or- provide an MQTT topic with the message (see MQTT overview of topics/commands)

When in SingleCommand mode, hit ESC or @ to obtain the selection menu, from which you can enter 
another SingleCommand mode, or resume animation,

-or- provide MQTT topic "animate" with optional message to play a file.

The following commands are supported in textfiles AND on the USB input.
I used putty with serial and/or a Bluetooth card attached to the pro for this purpose, 
so you can play with it from your PC (for both arduino as D1 mini), or MQTT for the ESP32 D1 mini.

All patterns for the base and the ring set the LEDS to a specified color.
For the eyes/nose blinking/flashing/strobing patterns can be set.

This works as follows:
The pattern time determines how long the pattern is displayed. During this LEDS can blink / flash / strobe.

With blink time you specify its on and off time. Blink=500 means 0.5s on and 0.5 seconds off.
When you specify flashtime=0, these (blink) times apply.
When flashtime>0, the leds will be on only for the flash. So:
blink=500, flash = 50 means: 0.05s on, and 0.095s off.
blink=500, flash = 950 means: 0.95s on, and 0.05s off.
blink=500, flash = 500 means: 0.5s on, and 0.5s off. This is the same as blinking.

When you specify strobe time=0, these times apply.
When strobe time>0, LEDS will quickly switch on and off during the "flash" or "blink" time.
So with strobing during blinking (flash time ignored): 
blink=500, flash=400, strobe=50: 0.05s on, 0.05s off, 0.05s on, 0.05s off, 0.05s on, 0.05s off, 0.05s on, 0.05s on, 0.05s off, 0.55s off
blink=500, flash=150, strobe=100:0.1s  on, 0.1s  off, 0.1s  on, 0.1s  off, 0.1s  on, 0.5s off

So strobing during flashing: 
blink=500, flash=400, strobe=50: 0.05s on, 0.05s off, 0.05s on, 0.05s off, 0.05s on, 0.05s off, 0.05s on, 0.65s off,
blink=500, flash=150, strobe=50: 0.05s on, 0.05s off, 0.05s on, 0.85s off

Even while the times apply for all nose/eyes LEDS, you can apply blinking/flashing/strobing for each eye/nose individually.
Use the Lsss command, described below.

All timing and mode commands do NOT count as patterns. The C head command (for color, described below) does.

After each pattern is displayed for "pattern time ms", the next pattern is displayed and random colors are applied, where specified.
 
================================== Commands in LED mode (for the LEDs at the Dalek base)

D <description>
  Just a free style description, so you know what is in the file 
  (may be used in menues added at some later point in time)
  Will be reported as state in MQTT.
  
T <time>[,timeExtend14,timeExtend25]
  Set the time in ms between two animated patterns.
  TimeExtend14 > 0, react to trigger of solenoid14 and extend "active" mode for TimeExtend14 ms.
  TimeExtend25 > 0, react to trigger of solenoid14 and extend "active" mode for TimeExtend25 ms.

S <short description>
  Just a free style SHORT description, so you know what is in the file 
  (may be used in menues added at some later point in time)
 
I [0:7]  
  Set the intensity of the display 
  (0, 1, 2, 3, 4 show a visible difference in intensity, above this differences are hardly noticeable)
  EXAMPLE: I 3

B [word], [word], ... <maximum of 13 words> Z
  Set a lowres pattern, each word represents a 15 bit number with 5 LEDS in a column. Per LED 3 bits are used for Blue, Green, Red respectively. so:

        Bit:    15 14 13 12 11 10  9  8  7  6  5  4  3  2  1
			+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
			|  |R5|G5|B5|R4|G4|B4|R3|G3|B3|R2|G2|B2|R1|G1|B1|
			+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
	Led 1 is the top LED.
C [word], [word], ... <maximum of 25 words> Z
  Set a highres pattern, each word represents a 15 bit number with 5 LEDS in a column. Per LED 3 bits are used for Blue, Green, Red respectively. so:

        Bit:    15 14 13 12 11 10  9  8  7  6  5  4  3  2  1
			+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
			|  |R5|G5|B5|R4|G4|B4|R3|G3|B3|R2|G2|B2|R1|G1|B1|
			+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
	Led 1 is the top LED.

	Note that column 25 (the last word) is driven by all three LED drivers:
	2 RGB's for driver one and two, and 1 RGB from the third driver, making the 25th column with its 5 RGBs.
	
STILL supported...(based having TM1640 command in mind, but I made them work in my firmware also for the HT16K33) 
X [byte],[byte],[byte],[byte],[byte],... <maximum of 32 bytes> Z
  Set a lowres pattern across 2 TM1640 modules (note, on a HIghRes DALEK, the pattern is still correctly displayed using the 3 LED drivers.)
  You may terminate the bytes in the pattern by an early "Z", the pattern is padded with "0"-s  
  EXAMPLE: X 21,127,5,8 Z
  EXAMPLE: X Z

STILL supported... 
Y [byte],[byte],[byte],[byte],[byte],... <maximum of 48 bytes> Z
  Set a highres pattern across 3 TM1640 modules
  You may terminate the bytes in the pattern by an early "Z", the pattern is padded with "0"-s  
  EXAMPLE: Y 21,127,5,8 Z
  EXAMPLE: Y Z

L OBSOLETE and removed for v4.0 Move (rotate) the pattern displayed one step to the left.  
  
R OBSOLETE and removed for v4.0 Move (rotate) the pattern displayed one step to the right.
  
M [0:3]
  Set the displaymode for highres DALEKS
  M 0: Displays lowres commands on the HighRes Dalek as they appear on on LOWRES dalek (not as simple as you think)
  M 1: Displays lowres commands on the HighRes Dalek on the right hand side of the Dalek
  M 2: Displays lowres commands on the HighRes Dalek on the centered side on the Dalek
  M 3: Displays lowres commands on the HighRes Dalek on the left hand side of the Dalek
  
A
  Start animation again after you have entered commands using USB serial input or bluetooth.

================================== Commands in RING mode (for the ring around the Dalek head)
Ttime[,timeExtend14,timeExtend25]
  Set the pattern time in ms between two animated patterns.
  TimeExtend14 > 0, react to trigger of solenoid14 and extend "active" mode for TimeExtend14 ms.
  TimeExtend25 > 0, react to trigger of solenoid14 and extend "active" mode for TimeExtend25 ms.

Adddddddddddddddddddddddd
  Set the color of each LED of the ring.         

  d is a digit [0-7] determining the color of each of the 24 LEDS (counterclockwise seen from the top).
  There are 24 digits, can optionally be terminated by <cr><lf> or a z.

================================== Commands in HEAD mode (for the Nose and eyes of the Dalek head)

Cdddbbbsss
 Sets the color of left-eye, nose and right-eye.
 This defines a pattern, that is displayed, as well as blinking/flashing and strobing mode.
	[0..7] set a color
	d is any of:
	A      Random (Any) color
	G      Random (as a Group)
	U      Random (Unique color)
	L      Follow Left eye (take color what is was previously)
	R      Follow Right eye (take color what is was previously)
	N      Follow Nose (take color what is was previously)

	b is any of:
	B      Blink
	F      Flash
	O	   On the whole period
	-	   Keep the previous definition that was set.
	
	s is any of:
	S	   Strobe the LED while it blinks, flashes or is on
	N	   No strobing
	-	   The previous strobing mode defined for this LED.
	
	Missing "s" or "b" are interpreted as "-"
 
Lmmm
 OBSOLETE: use the optional "bbb" and "sss" with the "C" command instead. 
 Sets the mode of working of left-eye, nose and right-eye.
 This does not count as a pattern, it determines how subsequent Cddd patterns are played.
	m is any of:
	o On         O On Strobing
	b Blink      B Blink Strobing
	f Flash      F Flash Strobing

Ttime[,timeExtend14,timeExtend25]
  Set the pattern time in ms between two animated patterns.
  TimeExtend14 > 0, react to trigger of solenoid14 and extend "active" mode for TimeExtend14 ms.
  TimeExtend25 > 0, react to trigger of solenoid14 and extend "active" mode for TimeExtend25 ms.

Btime
 Set the blink time
 
Ftime
 Set the flashing time
 
Stime
 Set the strobing time
 
========================================================================================================

Library for TM1640.

Copyright (C) 2011 Ricardo Batista <rjbatista at gmail dot com>

This program is free software: you can redistribute it and/or modify
it under the terms of the version 3 GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 The following hardware is needed:
 - 1 ESP32 D1 mini (recommended) or:
 - 2 Nano Arduino or:
 - 2 arduino Pro mini

 - Servo (5 USD)
 - 5V regulator (in order not to power the Servo from the Arduino board) (1 USD)
 - 3 Optocouplers (to operate Switch 81 & react to solenoid25 / solenoid14) (0.25 USD)
 - Resistor 330E, 0.25W (to drive the optocoupler) (0.1 USD)
 - Diode IN4148 (to complete switch 81 in the matrix), or use an LED (0.1 USD)
 - Jumpers, wires (p.m.)
 
 Total hardware cost below 30 USD
 
 The following connections towards the Pinball machine are needed:
 - Ground
 - +12V
 - Row 1 of the Switch matrix
 - Column 8 of the Switch matrix
 - Solenoid #25 of the flash/driver.
 - (optional) Solenoid #14 of the flash/driver (this is the Lamp from the original Dalek head Flasher).
 
 The LEDS were originally programmed on another Arduino. Now, for the ESP32 D1 Mini, the code is integrated.
 If you want LEDS in the base, you need additional hardware (use HT16K33's, TM1640's will not work as of version 4.0):
 - 3 HT16K33 drivers. These uses COMMON CATHODE LEDS for column 1-24 and common ANODE leds for column 25, so:
 - 120 RGB leds , 5mm diffuse common CATHODE (these make columns 1-24, 5 LEDS each)
 -   5 RGB leds , 5mm diffuse common   ANODE (these make columns 25, also 5 LEDS)
 
 If you additionaly like LEDS in the head:
 - 1 HT16K33 driver for the head
 - 24 RGB LEDS for the ring on the Dalek head.
 - 15 RGB LEDS: 5 in the nose, and 5 in each eye.
 
 Allthough the drivers can lightup the 5 LEDS in nose and each eye individually, the animation software does not utilize this.
 Nose and eyes each have "just" one of the 7 colors.
 
 Additionally you need stripboard, dupont headers, wire and solder. 
 
 I used wire wrapping on the dupont headers and the RGB wires. 
 Works very reliable, better that soldering, these joints tend to break when soldered mid-air.
 
  CHANGELOG
 - 1.3 Swapped pins to accomodate PCB
 
 - 1.4 Added logic for the TM1638
 
 - 1.5  Added better logic for detecting the middle and moving / starting correctly after stopping, beingUp and GoingUP.
 
 - 1.9 Better LEDs usage: showing the movement.

 - 2.0 Released: Also change behaviour of long press - (store settings) and long press + (recover settings)
 
 - 3.0 New version, took out the LEDS again, LCD using I2C
 
 - 3.1 Fixed keyboard issues

 - 3.2 Added my own class for rotational encoders.

 - 4.0 Make it run on ESP32 D1 mini.
       Remove LCD
	   Remove Rotencoder
	   Remove Buttons
	   Add 25x5 + 24 + 3x5 RGB LED animation, 492 LEDS using 4 HT16K33 (capacity 128 LEDS each)
	    - 375: 25 columns with 5 RGB LEDS
		-  72: A ring with 24 RGB LEDS
		-  45: Nose, left eye, right eye 5 RGB LEDS each
	   Add MQTT interface
	   Store settings in EEPROM
	   Store patterns on flashdrive of the D1 mini.
	   Add OTA updates
	   Add screen to configure wifi settings.
	   
 - 4.1 Remove the old code for LCD and menu
 
 - 4.2 Added hardware recommendations
 */


//WiFiManager & parameters, Global intialization. 
WiFiManager wm;

unsigned int  timeout   = 120; // seconds to run for
unsigned long  startTimePortal = millis();
bool portalRunning      = false;
bool startAP            = false; // start AP and webserver if true, else start only webserver
bool requestConfig		= false;

WiFiManagerParameter wp_RPM;
WiFiManagerParameter wp_Interval;
WiFiManagerParameter wp_Shape;
WiFiManagerParameter wp_InvertedServo;
WiFiManagerParameter wp_Left;
WiFiManagerParameter wp_Right;
WiFiManagerParameter wp_Center;

#ifdef def_mqtt_server			
WiFiManagerParameter mqtt_server;
WiFiManagerParameter mqtt_topic;
WiFiManagerParameter mqtt_user;
WiFiManagerParameter mqtt_pass;
WiFiManagerParameter mqtt_port;
#endif

#ifndef MinMQTTconnectInterval
#define MinMQTTconnectInterval 60000
#endif
boolean ReportNfiles = true;

// Use wifi socket for mqtt ONLY, or calls (like API calls) will disconnect the MQTT client.

#ifdef def_mqtt_server
WiFiClient MQTT_client; 

PubSubClient MQTTclient(MQTT_client);

unsigned long TimeMQTTReported;
boolean MQTT_interpreting=false;
boolean MQTT_fileIsOpen = false;

#define MAX_TOPIC_LEN	(60)
char Topic[MAX_TOPIC_LEN];
#define MSG_BUFFER_SIZE	(230)
char msg[MSG_BUFFER_SIZE];

bool mqtt_active = true;	//By setting the mqtt_server to "" (or "none") you can skip mqtt functionality.
#endif

// Set the length in bytes
#define L_salt			sizeof(salt)
#define L_project		1
#define L_version		1

#define L_left			sizeof(_Left)    // Left position of Servo EEPROM(6,7)
#define L_center		sizeof(_Center)    // Left position of Servo EEPROM(8,9)
#define L_right			sizeof(_Right)    // Left position of Servo EEPROM(10,11)

#define L_rpm			sizeof(_RPM)
#define L_interval		sizeof(_Interval)
#define L_shape			sizeof(_Shape)
#define L_inverted		sizeof(_InvertedServo)
#define L_movement		sizeof(_MovementMode)

#define L_mqtt_server	40 + 1
#define L_mqtt_topic	52 + 1
#define L_mqtt_user		16 + 1
#define L_mqtt_pass		16 + 1
#define L_mqtt_port		2

#define S_salt			1

#define S_left			S_salt + L_salt
#define S_center		S_left + L_left
#define S_right			S_center + L_center

#define S_rpm			S_right			+	L_right
#define S_interval		S_rpm			+	L_rpm
#define S_shape			S_interval		+	L_interval
#define S_inverted		S_shape			+	L_shape
#define S_movement		S_inverted		+	L_inverted

#ifdef def_mqtt_server
#define S_mqtt_server	S_movement		+ L_movement
#define S_mqtt_topic	S_mqtt_server	+ L_mqtt_server
#define S_mqtt_user		S_mqtt_topic 	+ L_mqtt_topic
#define S_mqtt_pass		S_mqtt_user  	+ L_mqtt_user
#define S_mqtt_port		S_mqtt_pass  	+ L_mqtt_pass
#define L_EEPROM		S_mqtt_port + L_mqtt_port - 1
#else
#define L_EEPROM		S_movement		+ L_movement - 1
#endif

// SALT is save to eeprom, if it is wrong, it assumed to be corrupted and reinitialized
// By making SALTs different, EEPROM is reinitialized when mqtt_server mode is different, porject or version changed, or the constant is changed.
#define SALT L_EEPROM + __RR_PROJECT + __RR_VERSION + SALT_INIT
	
unsigned short _Left;            // Left position of Servo EEPROM(5,6)
unsigned short _Center;          // Left position of Servo EEPROM(7,8)
unsigned short _Right;           // Left position of Servo EEPROM(9,10)
boolean _FirstLoop = true;

char s_mqtt_server[L_mqtt_server];
char s_mqtt_topic[L_mqtt_topic];
char s_mqtt_user[L_mqtt_user];
char s_mqtt_pass[L_mqtt_pass];
unsigned short i_mqtt_port;
unsigned long LastMQTTconnectattempt = 0;

// Global variables for mode, speed, position etc.
unsigned char _RPM;              // Rotations per minute for the Servo from EEPROM(1)
unsigned char _Interval;         // Interval time in milisecods for setting the servo position from EEPROM(2)
unsigned char _Shape;            /* Type of movement from EEPROM(3):
 0: __ShapeSine
 1: __ShapeSaw
 2: __ShapeBlock1
 3: __ShapeBlock2
 4: __ShapeRandom
 */
boolean _InvertedServo = false;
 
byte _ShapeRandom;     // The actual randomly (or fixed) chosen shape;
byte OldShapeRandom = -1;
const long _RandomInterval = 10000;

// INPUT ====================================================================================


#ifdef Arduino
#define _Solenoid25Pin 6    /* the solenoid drive #25 - to know 
                               whether movement is desired during gameplay. */
#endif

#ifdef ESP8286
#define _Solenoid25Pin 12   /* D6=GPIO12 connector J122-1
							   the solenoid drive #25 - This is the undocumented  output for the Williams Dalek motor
                               whether movement is desired during gameplay. */
							   
#define _Solenoid14Pin 13	/* D7=GPIO13 connector J128-2
							   This is reading the Flasher signal of Solenoid #14 */

#endif

// These were all the INPUT pins.............................................................

// OUTPUT ===================================================================================
#ifdef Arduino
#define _ServoPin 4			//This the the servo that makes the head move (Arduino pin 4)
#define _CenterLEDPin 5		/*This is the output that drives the optocoupler, which is 
							attached to row/col opto switch 81 in the pinball */
#endif

#ifdef ESP8286
#define _ServoPin 2			//This the the servo that makes the head move (ESP 32 D4=GPIO2)
#define _CenterLEDPin 15	//This is the output that drives the optocoupler, which is 
							//attached to row/col opto switch 81 in the pinball (D8=GPIO15)						
#define _SDAPin 4			//D2	GPIO4	OK	OK	often used as SDA (I2C)
#define _SCLPin 5 			//D1	GPIO5	OK	OK	often used as SCL (I2C)

#endif
// These were all the OUTPUT pins............................................................

const byte esc = 27;

byte _MovementMode;     /* Type of movement of the Dalek (in EEPROM)
 0: __MoveByPinball 		Who listening to the Solenoid25 signal, speed control (This is the DEFAULT, startup mode)
 1: __MoveAutonomous 	    Sweeping moving all by itself continuously
 2: __MoveFixedLeft			Fixed in left position (adjustable)
 3: __MoveFixedCenter		Fixed in center position (adjustable)
 4: __MoveFixedRight		Fixed in right position (adjustable)
 5: __MoveByPinball_stop_nocenter
 6: __MoveByPinball_stop_center
 */

double _Step;            // Each interval add _Step to x, for high RMP (moving fast) Step is larger.

//float _y=0;             // Calculated (before calibration) servo position  ([-1, 1])

// Loop for metro
const byte _EncoderLoopMillis = 5;
Metro _ServoLoop = Metro(10);								//Loop for setting the servo
Metro _ProcessingLoop = Metro(500);

// A servo......
Servo _Servo;

long _DalekStatus;

byte _Command = '0'; //(no command)

// const float _CenterMargin = 0.05;
boolean _WeAreCENTERED = false;
boolean _WeAreRight = true; // true means we are upwards:
boolean OldWeAreLeft = false;
boolean OldWeAreRight = false;

// Y: 0 ->  1   -> 0 and 
// X: 0+marge -> 0.25 -> 0.5-marge
//
// Otherwise False

boolean _WeAreLeft = false; // true means we downwards:
// Y: 0 ->  -1  -> 0 and 
// X: 0.5+marge -> 0.75 -> 1-marge
//
// Otherwise False

boolean _WeAreGoingRight = true; // False means we come from "down"so need to start at:
boolean OldWeAreGoingRight = false;
// TRUE
// Y: 0 ->  1
// X: 0 -> 0.25 

// FALSE
// Y: 1 ->  -1
// X: 0.25 -> 0.75

// TRUE
// Y: -1 ->  0
// X: 0.75 -> 1 

// Determining this seems complex, however: add 0.25 to X and map X to [0, 1) it becomes:
// TRUE
// Y: 0 ->  1
// X: 0 -> 0.5 

// FALSE
// Y: 1 ->  -1
// X: 0.5 -> 1

const float _XMarge = 0.025; // Is used to determine whether we REALLY passed the center. When X passes within this margin, the _WeAreCENTERED boolean is TRUE
// Outside this margin EITHER _WeAreRight or _WeAreLeft = true.

const float Pi = 3.14159265358979323846264338;
const float Pi5 = Pi/2;
const float Pi2 = 2*Pi;

// Oneliner functions...

float CalcSweepStep(byte Interval, byte RPM) {
	return (float)Interval * (float)RPM / 60000.0;
}

float MyMap(float MyValue, float MyInterval) {
	float x;
	x = MyValue;
	while (x > MyInterval) {
		x -= MyInterval;
	}
	while (x < 0) {
		x += MyInterval;
	}
	return x;
}

boolean Sign(float x) {
	if (x<0) {
	return false;
	} else {
		return true;
	}
}

void EEPROMputs(int strEEPROM, const char *save) {
	byte i = 0;
	while (byte(save[i])!=0) {
		EEPROM.put(strEEPROM + i,(byte)(save[i]));
		i++;
	}
	EEPROM.put(strEEPROM + i,(byte)(0));
}
void EEPROMputIP(int strEEPROM, IPAddress ipsave, byte size) {
	for (byte i=0;i<size;i++) EEPROM.put(strEEPROM + i,ipsave[i]);
}

void ReadSettings() {

#ifdef Arduino
	if ((EEPROM.read(100) == __RR_PROJECT) && (EEPROM.read(101) == __RR_VERSION)) {

	// Proper version, read the variables
		_RPM = EEPROM.read(1);
		_Interval = EEPROM.read(2);
		_Shape = EEPROM.read(3) ;

		_Left  = constrain( EEPROM.read(5) * 256 + EEPROM.read(4), 500, 1499);
		_Center  = constrain(EEPROM.read(7) * 256 + EEPROM.read(6),1000,2000);
		_Right  = constrain(EEPROM.read(9) * 256 + EEPROM.read(8),1501,2500);
		_InvertedServo = EEPROM.read(10);
		_MovementMode = __MoveByPinball_stop_center;	//Not stored in this version		
	} else {
		
		_RPM = 30;
		_Interval = 25;
		_Shape = 0;
		_InvertedServo = false;
		_Left = 1000;  // min =544, typical remote control: 1000;
		_Right = 2000; // max =2400, typical remote control: 2000;
		_Center = 1500;
		_MovementMode = __MoveByPinball_stop_center;
		WriteSettings();
	}
	
#else
	
	short salt;
	EEPROM.begin(L_EEPROM);
	EEPROM.get(0, salt);
	
	#ifdef DEBUG
	Serial.print(F("Comparing "));
	Serial.print(salt);
	Serial.print(" with ");
	Serial.println(SALT);
	#endif
	
	if (salt == SALT) {
		#ifdef DEBUG
		Serial.println(F("Reading EEPROM"));
		#endif
		
		EEPROM.get(S_rpm,_RPM);
		EEPROM.get(S_interval,_Interval);
		EEPROM.get(S_shape, _Shape);
		EEPROM.get(S_inverted, _InvertedServo);
		EEPROM.get(S_left, _Left);
		EEPROM.get(S_right, _Right);
		EEPROM.get(S_center, _Center);
		EEPROM.get(S_movement, _MovementMode);
		
		#ifdef def_mqtt_server
		EEPROM.get(S_mqtt_server, s_mqtt_server);
		EEPROM.get(S_mqtt_topic, s_mqtt_topic);
		EEPROM.get(S_mqtt_user, s_mqtt_user);
		EEPROM.get(S_mqtt_pass, s_mqtt_pass);
		EEPROM.get(S_mqtt_port, i_mqtt_port);

		if (((byte)s_mqtt_server[0]==0) or (strcmp(s_mqtt_server,"none")==0)) {
			mqtt_active = false;
		} else {
			mqtt_active = true;
		}
		
		#else
		mqtt_active = false;
		#endif
		
		#ifdef DEBUG
		Serial.println(F("Retrieved from EEPROM:"));
		Serial.print(F("S_rpm "));
		Serial.println(_RPM);
		Serial.print(F("S_interval "));
		Serial.println(_Interval);
		Serial.print(F("S_shape "));
		Serial.println(_Shape);
		Serial.print(F("S_left "));
		Serial.println(_Left);
		Serial.print(F("S_right "));
		Serial.println(_Right);
		Serial.print(F("S_center "));
		Serial.println(_Center);
		
		#ifdef def_mqtt_server
		if (mqtt_active) {
			Serial.print(F("M_server = "));
			Serial.println(s_mqtt_server);
			Serial.print(F("M_topic  = "));
			Serial.println(s_mqtt_topic);
			Serial.print(F("M_user   = "));
			Serial.println(s_mqtt_user);
			Serial.print(F("M_pass   = "));
			Serial.println(s_mqtt_pass);
			Serial.print(F("M_port   = "));
			Serial.println(i_mqtt_port);
		}
		#endif
		#endif
		
		EEPROM.end();
	} else {
		
		EEPROM.end();
		#ifdef DEBUG
		Serial.println(F("Initializing EEPROM from defaults"));
		#endif
		
		_RPM = 30;
		_Interval = 25;
		_Shape = 0;
		_InvertedServo = false;
		_Left = 1000;  // min =544, typical remote control: 1000;
		_Right = 2000; // max =2400, typical remote control: 2000;
		_Center = 1500;
		_MovementMode = __MoveByPinball_stop_center;

		#ifdef def_mqtt_server		
		strcpy (s_mqtt_server, def_mqtt_server);
		strcpy (s_mqtt_topic, def_mqtt_topic);
		strcpy (s_mqtt_user, def_mqtt_user);
		strcpy (s_mqtt_pass, def_mqtt_pass);
		i_mqtt_port = def_mqtt_port;
		#endif
		
		#ifdef def_mqtt_server
		if (((byte)s_mqtt_server[0]==0) or (strcmp(s_mqtt_server,"none")==0)) {
			mqtt_active = false;
		} else {
			mqtt_active = true;
		}	
		#endif	
		
		WriteSettings();
	}	 
#endif
}

void WriteSettings() {
#ifdef Arduino
	EEPROM.write(100,__RR_PROJECT);
	EEPROM.write(101,__RR_VERSION);

	EEPROM.write(1, _RPM);
	EEPROM.write(2, _Interval);
	EEPROM.write(3, _Shape ) ;
	EEPROM.write(4, _Left%256);
	EEPROM.write(5, _Left/256);
	EEPROM.write(6, _Center%256);
	EEPROM.write(7, _Center/256);
	EEPROM.write(8, _Right%256);
	EEPROM.write(9, _Right/256);
	EEPROM.write(10, _InvertedServo);
#else
	short salt = SALT;
	EEPROM.begin(L_EEPROM);
	EEPROM.put(0, salt);
	EEPROM.put(S_center, _Center);
	EEPROM.put(S_right, _Right);
	EEPROM.put(S_left, _Left);
	EEPROM.put(S_inverted, _InvertedServo);
	EEPROM.put(S_shape, _Shape);
	EEPROM.put(S_interval,_Interval);
	EEPROM.put(S_rpm,_RPM);
	EEPROM.put(S_movement,_MovementMode);	
	#ifdef def_mqtt_server		  
	EEPROMputs(S_mqtt_server, s_mqtt_server);
	EEPROMputs(S_mqtt_topic, s_mqtt_topic);
	EEPROMputs(S_mqtt_user, s_mqtt_user);
	EEPROMputs(S_mqtt_pass, s_mqtt_pass);
	EEPROM.put(S_mqtt_port, i_mqtt_port);
	#endif

	EEPROM.commit();
	EEPROM.end();

	#ifdef def_mqtt_server
	if (((byte)s_mqtt_server[0]==0) or (strcmp(s_mqtt_server,"none")==0)) {
		mqtt_active = false;
	} else {
		mqtt_active = true;
	}
	#endif

	#ifdef DEBUG
	Serial.print("write _RPM: ");
	Serial.println(_RPM);
	Serial.print("write _Right: ");
	Serial.println(_Right);
	Serial.print("write _Left: ");
	Serial.println(_Left);
	#endif

	setupAfterSettingsChange();

#endif
}

void PrintDesc(uint16_t i) {
	#ifdef Arduino
	File myfile = SD.open(Getfilename_desc(i), FILE_READ);
	#else
	File myfile = LittleFS.open(Getfilename_desc(i), "r");
	#endif	

	if (myfile.available()) {
		while (myfile.available()) {
			Serial.print((char)myfile.read());
		}
	} else {
		Serial.print(Getfilename_desc(i));
		Serial.println(F(" is not available"));
	}
	myfile.close();
}

String Getfilename_desc(short num) {
	char filename[16];
	int n = sprintf(filename,"/text/S%04i.TXT", num);
	return filename;
}

void PrintDescN(uint16_t msgnum) {Serial.println(F(""));PrintDesc(msgnum);}

//boolean ShowOutput(byte LEDorHEADorRING) {return ((LEDorHEADorRING==ShowSerialInput) || (LEDorHEADorRING==__stream_ForceSerial) );}
boolean ShowOutput(byte LEDorHEADorRING) { return (HP_TimeTiming[LEDorHEADorRING].StreamSource==__streamFrom_Serial);}


String getParam(String name){
  //read parameter from server, for customhmtl input
  String value;
  if(wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback(){
	#ifdef DEBUG
	Serial.println(F("[CALLBACK] saveParamCallback fired"));
	#endif

	_RPM = getParam("D_RPM").toInt();
	_Interval = getParam("D_Interval").toInt();
	_Shape = getParam("D_Shape").toInt();
	_InvertedServo = getParam("D_Inverted").toInt();
	_Left = getParam("D_Left").toInt();
	_Right = getParam("D_Right").toInt();
	_Center = getParam("D_Center").toInt();
	
	#ifdef def_mqtt_server
	strcpy(s_mqtt_server,getParam("M_server").c_str());
	strcpy(s_mqtt_topic,getParam("M_topic").c_str());
	strcpy(s_mqtt_user,getParam("M_user").c_str());
	strcpy(s_mqtt_pass,getParam("M_pass").c_str());
	i_mqtt_port = getParam("M_port").toInt();
	#endif

	WriteSettings();
	
	delay(2000);
	//restart
	ESP.restart();
	while(true) delay(10);	//Do not continue
	
}

void setup_handle_autoconnect() {

//		wm.startConfigPortal("DoctorWhoDalek","pass1234"); // no password protected ap
//BUG?? In WiFimanager: configportal crashed the ESP on saving the param screen, 
//      so for now erase wifi and autoconnect if you want the portal.
	wm.setCountry(WiFiCountry); 
	
	// set Hostname
	wm.setHostname("DoctorWhoDalek");

	// show password publicly in form
	//wm.setShowPassword(true);

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result
	
    // res = wm.autoConnect(); // auto generated AP name from chipid
    // res = wm.autoConnect("AutoConnectAP"); // anonymous ap

	//Serial.println(F("going to autoconnect"));
	sprintf(msg, "%d", _RPM);
	new (&wp_RPM) WiFiManagerParameter("D_RPM", "RPM", msg, MSG_BUFFER_SIZE);
	sprintf(msg, "%d", _Interval);
	new (&wp_Interval) WiFiManagerParameter("D_Interval", "Interval", msg, MSG_BUFFER_SIZE);
	sprintf(msg, "%d", _Shape);
	new (&wp_Shape) WiFiManagerParameter("D_Shape", "Shape", msg, MSG_BUFFER_SIZE);
	sprintf(msg, "%d", _InvertedServo);
	new (&wp_InvertedServo) WiFiManagerParameter("D_Inverted", "Inverted", msg, MSG_BUFFER_SIZE);

	sprintf(msg, "%d", _Left);
	new (&wp_Left) WiFiManagerParameter("D_Left", "Left", msg, MSG_BUFFER_SIZE);
	sprintf(msg, "%d", _Right);
	new (&wp_Right) WiFiManagerParameter("D_Right", "Right", msg, MSG_BUFFER_SIZE);
	sprintf(msg, "%d", _Center);
	new (&wp_Center) WiFiManagerParameter("D_Center", "Center", msg, MSG_BUFFER_SIZE);

	#ifdef def_mqtt_server			
	new (&mqtt_server) WiFiManagerParameter("M_server", "mqtt server", s_mqtt_server, L_mqtt_server-1);
	new (&mqtt_topic) WiFiManagerParameter("M_topic", "mqtt topic", s_mqtt_topic, L_mqtt_topic-1);
	new (&mqtt_user) WiFiManagerParameter ("M_user", "mqtt user", s_mqtt_user, L_mqtt_user-1);
	new (&mqtt_pass) WiFiManagerParameter ("M_pass", "mqtt password", s_mqtt_pass, L_mqtt_pass-1);
	sprintf(msg, "%d", i_mqtt_port);
	new (&mqtt_port) WiFiManagerParameter ("M_port", "mqtt port", msg, MSG_BUFFER_SIZE);
	#endif

	wm.addParameter(&wp_RPM);	
	wm.addParameter(&wp_Interval);
	wm.addParameter(&wp_Shape);
	wm.addParameter(&wp_InvertedServo);
	wm.addParameter(&wp_Left);
	wm.addParameter(&wp_Right);
	wm.addParameter(&wp_Center);

	#ifdef def_mqtt_server		
	wm.addParameter(&mqtt_server);
	wm.addParameter(&mqtt_port);
	wm.addParameter(&mqtt_user);
	wm.addParameter(&mqtt_pass);
	wm.addParameter(&mqtt_topic);
	#endif
	
    //wm.setConfigPortalBlocking(true);
    wm.setSaveParamsCallback(saveParamCallback);
	
//	std::vector<const char *> menu = {"wifi","info","sep","restart","exit"};
	std::vector<const char *> menu = {"wifi","info","param","sep","restart","exit"};
//	std::vector<const char *> menu = {"info","param","sep","restart","exit"};
	wm.setMenu(menu);
	
	// set dark theme
	wm.setClass("invert");	

	wm.setConfigPortalTimeout(180); // auto close configportal after n seconds
	//wm.setCaptivePortalEnable(false); // disable captive portal redirection
	wm.setTimeout(60); // Wait in config portal, before trying the original wifi again.
	//wm.setCaptivePortalEnable(false); // disable captive portal redirection
	wm.setAPClientCheck(true); // avoid timeout if client connected to softap
	
	wm.autoConnect("DoctorWhoDalek");
}

#ifdef def_mqtt_server
bool PublishMQTT(const char* topicRoot, const char* topic, String msg, bool retained = false) {
	char Topic[60];
	strcpy(Topic,topicRoot);
	strcat(Topic,topic);
	return MQTTclient.publish(Topic,msg.c_str(), retained);
}

bool SubscribeMQTT(const char* topicRoot, const char* topic) {
	char Topic[60];
	strcpy(Topic,topicRoot);
	strcat(Topic,topic);
	return MQTTclient.subscribe(Topic);
}

void DeleteMQTTstream(String Schar) {
	fileDelete("/mqtt/" + Schar + "w.txt");
	fileDelete("/mqtt/" + Schar + "a.txt");
}

void PlayoutFromMQTT(byte Stream, byte StreamType, String MessageReceived = "") {
	if ((HP_TimeTiming[Stream].StreamSource == __streamFrom_SD) or 
	    (HP_TimeTiming[Stream].StreamSource == __streamFrom_MQTT_SD) or 
	    (HP_TimeTiming[Stream].StreamSource ==__streamFrom_MQTT_Loop)) HP_TimeTiming[Stream].dataFile.close();
	
	if (StreamType == __stream_Sleeping) {
		HP_TimeTiming[Stream].StreamIsActive = false;
		writeMQTTFile(LittleFS,Stream, MessageReceived.c_str());		
		HP_TimeTiming[Stream].StreamSource = __streamFrom_MQTT_SD;
	} else {
		//HP_TimeTiming[Stream].StreamIsActive = true;
		HP_TimeTiming[Stream].StreamIsActive = false;
		HP_TimeTiming[Stream].StreamSource = __streamFrom_MQTT_Loop;
	}
	HP_TimeTiming[Stream].PatternOK = false;
	HP_TimeTiming[Stream].GetNextPattern = true;
	OpenSDFile(Stream, __stream_Sleeping);	//MQTT commands only apply for sleeping mode.
}

void MQTTcallback(char* topic, byte* payload, unsigned int length) {
	if (mqtt_active) {
		#ifdef DEBUG
		Serial.print(F("Message arrived ["));
		Serial.print(topic);
		Serial.print("] [");
		#endif
		
		String MessageReceived = "";
		for (unsigned int i = 0; i < length; i++) {
			#ifdef DEBUG
			Serial.print((char)payload[i]);
			#endif
			MessageReceived += (char)payload[i];
		}

		#ifdef DEBUG
		Serial.println("]");
		Serial.print("Topic extracted: [");
		#endif
		String TopicReceived = "";
		String aCharacter;
		for (unsigned int  i = 0; topic[i]!=0; i++) {
			aCharacter = (char)topic[i];
			if (aCharacter == "/") TopicReceived = ""; else TopicReceived += aCharacter;
		}
		

		TopicReceived.toUpperCase();
		if (TopicReceived!="HEADPATTERN") MessageReceived.toUpperCase();
		
		#ifdef DEBUG
			Serial.print(TopicReceived);
			Serial.println("]");
		#endif

		bool DoWriteSettings = true;
		if (TopicReceived == "RPM") {
			_RPM = MessageReceived.toInt();
			_FirstLoop=true;
		} else if (TopicReceived == "LEFT") {
			_Left = MessageReceived.toInt();
		} else if (TopicReceived == "RIGHT") {
			_Right = MessageReceived.toInt();
		} else if (TopicReceived == "CENTER") {
			_Center = MessageReceived.toInt();
		} else if (TopicReceived == "SHAPE") {
			_Shape = MessageReceived.toInt();
		} else if (TopicReceived == "INTERVAL") {
			_Interval = MessageReceived.toInt();
		} else if (TopicReceived == "MOVEMENT") {
			_MovementMode = MessageReceived.toInt();
			_FirstLoop=true;
		} else if (TopicReceived == "INVERTEDSERVO") {
			_InvertedServo = MessageReceived.toInt();
		} else {
			DoWriteSettings=false;
			if (TopicReceived == "LEDPATTERN") {
				ResetPlayout(__stream_HEAD);
				ResetPlayout(__stream_RING);		
				PlayoutFromMQTT(__stream_LED, __stream_Sleeping, MessageReceived);
			} else if (TopicReceived == "RINGPATTERN") {
				ResetPlayout(__stream_LED);
				ResetPlayout(__stream_HEAD);
				PlayoutFromMQTT(__stream_RING, __stream_Sleeping, MessageReceived);
			} else if (TopicReceived == "HEADPATTERN") {
				ResetPlayout(__stream_LED);
				ResetPlayout(__stream_RING);
				PlayoutFromMQTT(__stream_HEAD, __stream_Sleeping, MessageReceived);
			} else if (TopicReceived == "PLAYMQTT") {
				if (MessageReceived == "RESET") {
					ResetPlayout(__stream_LED);
					ResetPlayout(__stream_HEAD);
					ResetPlayout(__stream_RING);
				} else if (MessageReceived == "LED") {
					PlayoutFromMQTT(__stream_LED,__stream_Active);
				} else if (MessageReceived == "HEAD") {
					PlayoutFromMQTT(__stream_HEAD,__stream_Active);
				} else if (MessageReceived == "RING") {
					PlayoutFromMQTT(__stream_RING,__stream_Active);
				}
			} else if (TopicReceived == "ACTIVE") {
				if (MessageReceived == "ALL") {
					HP_TimeTiming[__stream_LED].SwapActiveSleeping = true;
					HP_TimeTiming[__stream_HEAD].SwapActiveSleeping = true;
					HP_TimeTiming[__stream_RING].SwapActiveSleeping = true;
					ResetPlayout(__stream_LED);
					ResetPlayout(__stream_HEAD);
					ResetPlayout(__stream_RING);	
				} else if (MessageReceived == "LED") {
					HP_TimeTiming[__stream_LED].SwapActiveSleeping = true;
					ResetPlayout(__stream_LED);
				} else if (MessageReceived == "HEAD") {
					HP_TimeTiming[__stream_HEAD].SwapActiveSleeping = true;
					ResetPlayout(__stream_HEAD);
				} else if (MessageReceived == "RING") {
					HP_TimeTiming[__stream_RING].SwapActiveSleeping = true;
					ResetPlayout(__stream_RING);
				} 
			} else if (TopicReceived == "SLEEP") {	
				if (MessageReceived == "ALL") {
					HP_TimeTiming[__stream_LED].SwapActiveSleeping = false;
					HP_TimeTiming[__stream_HEAD].SwapActiveSleeping = false;
					HP_TimeTiming[__stream_RING].SwapActiveSleeping = false;
					ResetPlayout(__stream_LED);
					ResetPlayout(__stream_HEAD);
					ResetPlayout(__stream_RING);						
				} else if (MessageReceived == "LED") {
					HP_TimeTiming[__stream_LED].SwapActiveSleeping = false;
					ResetPlayout(__stream_LED);
				} else if (MessageReceived == "HEAD") {
					HP_TimeTiming[__stream_HEAD].SwapActiveSleeping = false;
					ResetPlayout(__stream_HEAD);
				} else if (MessageReceived == "RING") {
					HP_TimeTiming[__stream_RING].SwapActiveSleeping = false;
					ResetPlayout(__stream_RING);
				}
			} else if (TopicReceived == "ERASE") {	
				if (MessageReceived == "ALL") {
					DeleteMQTTstream("L");
					DeleteMQTTstream("H");
					DeleteMQTTstream("R");
				} else if (MessageReceived == "LED") {
					DeleteMQTTstream("L");
				} else if (MessageReceived == "HEAD") {
					DeleteMQTTstream("H");
				} else if (MessageReceived == "RING") {
					DeleteMQTTstream("R");			
				}
			} else if (TopicReceived == "SHOWFILENAMES") {	
				if (MessageReceived == "ON") {
					ShowFilenames = true;
				} else if (MessageReceived == "TRUE") {
					ShowFilenames = true;
				} else {
					ShowFilenames = false;
				}
			} else {
				#ifdef DEBUG
				Serial.print(F("Topic not recognized, "));
				Serial.print(F("message: ["));
				Serial.print(MessageReceived.c_str());
				Serial.println("]");
				#endif
			}
		}
		if (DoWriteSettings) WriteSettings();
	}

}

void ResetPlayout(byte Stream) {
	switch (HP_TimeTiming[Stream].StreamSource) {
	case __streamFrom_MQTT_SD:
	case __streamFrom_MQTT_Loop:
	case __streamFrom_SD:
		HP_TimeTiming[Stream].dataFile.close();
	}

	HP_TimeTiming[Stream].StreamSource = __streamFrom_SD;
	HP_TimeTiming[Stream].GetNextPattern = true;
	OpenFirstSleepingSDFile(Stream);
}

bool reconnect_mqtt() {
	//If we get here mqtt is NOT connected! If mqtt is configured and active it tries to connect, but only once per minute.
	#ifdef def_mqtt_server
	if (!mqtt_active) {
		return false;
	} else {		//mqtt configured, active and a connection is desired, but not there.

		unsigned long Now = millis(); 
		if (Now-(unsigned long)MinMQTTconnectInterval< LastMQTTconnectattempt){ //Do not try to reconnect too often
			return false;	
		} else {
			LastMQTTconnectattempt = Now;
			// Try (just once) to reconnected if necessary
			if (MQTTclient.connected()) {
				#ifdef DEBUG
				//Serial.print(F("reconnect_mqtt: MQTT CONNECTED *****************"));
				#endif
				return true;
			} else {

				#ifdef DEBUG
				Serial.println(F("Attempting MQTT connection..."));
				#endif
				// The config might have changed....			
				MQTTclient.setServer(s_mqtt_server, i_mqtt_port);


				// Create a random MQTTclient ID
				String clientId = "DOCTORWHODALEK-";
				clientId += String(random(0xffff), HEX);
				WiFi.mode(WIFI_STA);
				WiFi.hostname(op_hostname);

				delay(100);
				// Attempt to connect
				if (MQTTclient.connect(clientId.c_str(),s_mqtt_user,s_mqtt_pass)) {
					#ifdef DEBUG
					Serial.print(clientId);
					Serial.print(F(" connected to MQTT server "));
					Serial.println(s_mqtt_server);
					#endif
					
					// Resubscribe			
					if (SubscribeMQTT(s_mqtt_topic,(char *)topic_Head)) {
						#ifdef DEBUG
						Serial.print (F("Subscribed "));
						Serial.print (s_mqtt_topic);
						Serial.println (topic_Head);
						#endif
					} else {
						#ifdef DEBUG
						//Serial.println (F("Subscribe FAILED"));
						#endif
					}

					return true;

				} else {
					#ifdef DEBUG
					Serial.print(F("failed, rc="));
					Serial.println(MQTTclient.state());
					Serial.println(s_mqtt_server);
					//Serial.println(s_mqtt_user);
					//Serial.println(s_mqtt_pass);
					//Serial.println(s_mqtt_topic);
					Serial.println(" try again in next loop");
					#endif

					return false;
				}
			}
		}
	}
	#endif
	return false;
}
#endif
	
void ServoReAttach() {
	if (_Servo.attached()) {
		_Servo.detach();
		_Servo.attach(_ServoPin,_Left,_Right);
	}
}

float saw(float x)
{
	if (x < 0.25) {
		return 4*x;
	} else if (x<0.75) {
		return 2-4*x;
	} else {
		return 4*(x-1);
	}
}

float block(float x) {
	if (x < 0.125) {
		return 0;
	} else if (x<0.375) {
		return 1;
	} else if (x<0.625) {
		return 0;
	} else if (x<0.875) {
		return -1;
	} else {
		return 0;
	}
}  

float block2(float x) {
	if (x < 0.0625) {
		return 0;
	} else if (x<0.1875) {
		return 0.5;
	} else if (x<0.3125) {
		return 1;
	} else if (x<0.4375) {
		return 0.5;
	} else if (x<0.5625) {
		return 0;
	} else if (x<0.6875) {
		return -0.5;
	} else if (x<0.8125) {
		return -1;
	} else if (x<0.9375) {
		return -0.5;
	} else {
		return 0;
	}
}

int CalcPos(float x)
{
	int positie=99;

	float        y;

	switch (_ShapeRandom)	{
	case 1: // Saw
		y = saw(x);
		break;
	case 2: // Block
		y = block(x);
		break;
	case 3: // Block2
		y = block2(x);
		break;
	default: // Sin
		y = sin(Pi2 * x);
	}
	// Send out a calibrated signal between _Left, _Center and _Right.

	if (y<0) {
		positie = map(10000*y, -10000, 0, _Left , _Center);
	} else {
		positie = map(10000*y, 0, 10000, _Center , _Right);
	}

	return positie;

}

void OpenSDFile(byte LEDorHEADorRING, byte ActiveOrSleeping) {
	DEBUG_OUT("Open file", Getfilename(LEDorHEADorRING, ActiveOrSleeping))
	#ifdef Arduino
	HP_TimeTiming[LEDorHEADorRING].dataFile = SD.open(Getfilename(LEDorHEADorRING, ActiveOrSleeping));
	#else
	HP_TimeTiming[LEDorHEADorRING].dataFile = LittleFS.open(Getfilename(LEDorHEADorRING, ActiveOrSleeping),"r");
	#endif
	//ShowFilenames_opened(LEDorHEADorRING,ActiveOrSleeping,"----");
}

void ShowFilenames_opened(byte LEDorHEADorRING, byte ACTIVEorSLEEPING,String desc) {
	if (ShowFilenames) {
		sprintf(msg, "%-6s %-8s (%d)\t%s",(ACTIVEorSLEEPING==__stream_Active ? "Active" : "Sleep "),HP_TimeTiming[LEDorHEADorRING].dataFile.name(),HP_TimeTiming[LEDorHEADorRING].dataFile.size(),desc.c_str());
		switch (LEDorHEADorRING) {
		case __stream_HEAD:
			if (ShowFilenames) Serial.print (F("\r\nHEAD\t"));//PrintDescN(40); //N
			PublishMQTT(def_mqtt_topic, "/StreamHEAD", msg);
			break;
		case __stream_RING:
			if (ShowFilenames) Serial.print (F("\r\nRING\t"));// PrintDescN(44); //N
			PublishMQTT(def_mqtt_topic, "/StreamRING", msg);
			break;
		case __stream_LED:
			if (ShowFilenames) Serial.print (F("\r\nLED\t"));//  PrintDescN(44); //N
			PublishMQTT(def_mqtt_topic, "/StreamLEDs", msg);
		}
		Serial.println(msg);
	}
}

boolean fileExists(byte LEDorHEADorRING, byte ActiveOrSleeping, byte n) {
	//int fnum = (streamType * 2 + streamActive) * 1000 + n;
	
	#ifdef DEBUG
	Serial.printf("Tested filename with Streamtype=%u, streamActive=%u, n=%u",LEDorHEADorRING,ActiveOrSleeping,n);
	Serial.println(Getfilename_bynum(LEDorHEADorRING, ActiveOrSleeping,n));
	#endif
	
	#ifdef Arduino
	return SD.exists(Getfilename_bynum(LEDorHEADorRING, ActiveOrSleeping,n));
	#else
	return LittleFS.exists(Getfilename_bynum(LEDorHEADorRING, ActiveOrSleeping,n));
	#endif		
}

void fileDelete(String FileName) {
	#ifdef Arduino
	if (SD.exists(FileName)) SD.remove(FileName);
	#else
	if (LittleFS.exists(FileName)) LittleFS.remove(FileName);
	#endif		
}

void setup()
{
	// LED Stuff ==========================================================================================================
	
	Wire.begin(); //Starting this one without an address, This scetch uses it as master.
	//Wire.onReceive(receiveEvent); // register event
	DisplayDrivers[0].InitDriver();
	DisplayDrivers[1].InitDriver();
	DisplayDrivers[2].InitDriver();
	HeadDriver.InitDriver();
	
	// initialize serial:
	Serial.begin(115200);
	while (!Serial) {}
	delay(1000);
	Serial.flush();

	#ifdef DEBUG
	Serial.println("IN SETUP");
	#endif
	
    // see if the card is present and can be initialized:
	#ifdef Arduino
	pinMode(__chipSelect, OUTPUT);
	if (!SD.begin(__chipSelect)) {
		Serial.println(F("Card failed or not present"));
		// don't do anything more:
		return;
	} else {
		PrintDescN(1);	  
	}	
	#endif

	#ifdef ESP8266
	if (!LittleFS.begin()) {
		Serial.println(F("LittleFS mount failed"));
		// don't do anything more:
		return;
	} else {
		PrintDescN(1);
		FSInfo fs_info;
		LittleFS.info(fs_info);
		Serial.printf("\n\rFilesystem open, used: %u, available %u, %u files can be opened\n\r",fs_info.usedBytes,fs_info.totalBytes,fs_info.maxOpenFiles);
	}	
	#endif
	
	// Figure out whether there are files on disk.
	// For the LEDS, the Head and the Ring:
	for (int streamType : {__stream_LED, __stream_HEAD, __stream_RING} ) {
		for (int stream : {__stream_Active, __stream_Sleeping}) { 
			HP_TimeTiming[streamType].numberFiles[stream] = 0;
			HP_TimeTiming[streamType].pointerFile[stream] = -1;
			HP_TimeTiming[streamType].HasFiles[stream] = false;
			//DEBUG_OUT("testing file",Getfilename_bynum(streamType,stream,0))
			for (int i=0; fileExists(streamType,stream,i); i++) {
				//DEBUG_OUT("testing file",Getfilename_bynum(streamType,stream,i+1))
				HP_TimeTiming[streamType].numberFiles[stream] = i+1;
				HP_TimeTiming[streamType].pointerFile[stream] = 0;
				HP_TimeTiming[streamType].HasFiles[stream] = true;
			}
		}
	}

	DEBUG_OUT("Active  LED files", HP_TimeTiming[__stream_LED].numberFiles[__stream_Active])
	DEBUG_OUT("Sleep   LED files", HP_TimeTiming[__stream_LED].numberFiles[__stream_Sleeping])
	DEBUG_OUT("Active HEAD files", HP_TimeTiming[__stream_HEAD].numberFiles[__stream_Active])
	DEBUG_OUT("Sleep  HEAD files", HP_TimeTiming[__stream_HEAD].numberFiles[__stream_Sleeping])
	DEBUG_OUT("Active RING files", HP_TimeTiming[__stream_RING].numberFiles[__stream_Active])
	DEBUG_OUT("Sleep  RING files", HP_TimeTiming[__stream_RING].numberFiles[__stream_Sleeping])
	
	ReportNfiles = true;
	// Show how many files per category.
	
	// Open the files of the "sleeping" stream: open and set pointer to the FIRST inactive file.
	for (byte streamType : {__stream_LED, __stream_HEAD, __stream_RING} ) {
		OpenFirstSleepingSDFile(streamType);
	}
	

	
	// DALEK stuff ================================================================================================================
	
	#ifdef def_mqtt_server
	//Set the mqtt callback
	MQTTclient.setCallback(MQTTcallback);
	#endif

	/* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
	 would try to act as both a client and an access-point and could cause
	 network-issues with your other WiFi-devices on your WiFi-network. */
	WiFi.mode(WIFI_STA);
	
	//Set the hostname
	ArduinoOTA.setHostname(op_hostname);
	#ifdef OTApass
	//ArduinoOTA.setPassword(OTApass);
	#endif

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
	#ifdef ESP8266
	LittleFS.end();
	#endif
	
    Serial.println("Start updating " + type);


  });

  ArduinoOTA.onEnd([]() {
	Serial.println("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\n\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
	Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println(F("Auth Failed"));
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println(F("Begin Failed"));
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println(F("Connect Failed"));
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println(F("Receive Failed"));
    } else if (error == OTA_END_ERROR) {
      Serial.println(F("End Failed"));
    }
  });
  
  ArduinoOTA.begin();

	#ifdef DEBUG
	wm.setDebugOutput(true);
	Serial.print(F("Ready: version "));
	Serial.println(__RR_SW_Version);
	#else
	wm.setDebugOutput(false);
	#endif
	
	setup_handle_autoconnect();
  
  	

	// Read settings from EEPROM (or initialize them if setting are not correct)
	ReadSettings();

	_MovementMode = __MoveByPinball; // Set it to listening to the pinball machine...
	setupAfterSettingsChange();
	
	// Initialize the output pin for the centered servo:
	pinMode(_CenterLEDPin, OUTPUT);
	pinMode(_SCLPin, OUTPUT);
	pinMode(_SDAPin, OUTPUT);	
	pinMode(_Solenoid25Pin, INPUT);
	pinMode(_Solenoid14Pin, INPUT);
	
	//digitalWrite(_Solenoid25Pin, HIGH); 	// This enables the internal pullup resistor, 
	/*										 so that by connecting it to GND, you detect a LOW. 
											 Does not work on ESP32 D1 mini, use reel resistors */
											 
	digitalWrite(_SDAPin, LOW);	// Switch test LED off
	digitalWrite(_SCLPin, LOW); // Switch test LED off
}

void OpenFirstSleepingSDFile(byte LEDorHEADorRING) {
	// Open the files of the "sleeping" stream: open and set pointer to the FIRST inactive file.
	HP_TimeTiming[LEDorHEADorRING].StreamIsActive = false;
	
	//Swap whether active or normal stream is taken. 
	byte ACTIVEorSLEEPING = HP_TimeTiming[LEDorHEADorRING].SwapActiveSleeping ? __stream_Active : __stream_Sleeping;

	if (HP_TimeTiming[LEDorHEADorRING].HasFiles[ACTIVEorSLEEPING]) {
		HP_TimeTiming[LEDorHEADorRING].pointerFile[ACTIVEorSLEEPING] = 0;
		OpenSDFile(LEDorHEADorRING, ACTIVEorSLEEPING);
	}
	
	HP_TimeTiming[LEDorHEADorRING].StreamSource = __streamFrom_SD;

	// Kick off the timers, so that the streams are initialized after 10ms and then load the first pattern.
	// The pointers point to the NEXT filenumber, so init will load file "seqx000.txt" and set the pointer from 0 to 1
	HP_TimeTiming[LEDorHEADorRING].ActiveProcessing.interval(10);
	HP_TimeTiming[LEDorHEADorRING].ActiveProcessing.reset();	
}

void setupAfterSettingsChange() {
	// Initialize the Servo update time
	_ServoLoop.interval(_Interval);
	_ServoLoop.reset();

	_Servo.attach(_ServoPin,_Left,_Right);  // attaches the servo on pin _ServoPin to the servo object
	_Step = CalcSweepStep(_Interval, _RPM);

	//_TimeRandomShape = (unsigned long)millis();
}

void doWiFiManager(){
  // is auto timeout portal running
  if(portalRunning){
    wm.process();
    if((millis()-startTimePortal) > (timeout*1000)){
      Serial.println("portaltimeout");
      portalRunning = false;
      if(startAP){
        wm.stopConfigPortal();
      }  
      else{
        wm.stopWebPortal();
      } 
   }
  }

  // is configuration portal requested?
  if(requestConfig && (!portalRunning)) {
	if(startAP){
		#ifdef debug_
		Serial.println(F("Button Pressed, Starting Config Portal"));
		#endif
		wm.setConfigPortalBlocking(false);
		wm.startConfigPortal("SetupOctoplugout");
	} else {
		#ifdef debug_
		Serial.println(F("Button Pressed, Starting Web Portal"));
		#endif
		wm.startWebPortal();
	}  
	portalRunning = true;
	startTimePortal = millis();
	}
}

#ifdef DEBUG
	bool mqtt_last_connected = false;
#endif

void loop() 
{
	static float x=0;             	// Where we are relative to the interval ([0, 1])
	static int   Spos=1500;       	// variable to store the servo position
#ifdef DEBUG
	static int   Opos=1500;       	// variable to store the servo position
	static int   OOpos=1500;       	// variable to store the servo position
	static int   Count=0;       	// variable to store the servo position
#endif
	static byte  ProcessingPhase=0;	// In order not to do EVERYTHING at once in the loops: each loop does "one task" in sequence.
	static int  LastKeyPressed=0;   // Used for de-bouncing the keys...
	boolean Solenoid25;		// Used for capturing the hardware pin status, it is FALSE when the solenoid is activated.
	boolean Solenoid14;		// Used for capturing the hardware pin status, it is FALSE when the solenoid is activated.
	static boolean OldSolenoid25 = false;
	static boolean OldSolenoid14 = false;
	static unsigned long LoopTime = millis();
	static boolean MeasureLoop = false;
	
	static unsigned long _TimeRandomShape;   // Here we detect whether it is time to select a new movement mode...
	static unsigned long _TimeLastMovement;	 // Detecting whether it is time to move to the center pos
	static boolean _NeedToCenter = false;	 // Ensures that after a minut we fully return to the center.				
	
	unsigned long Now = millis();
	
	#ifdef ESP8286
	MDNS.update();
	
	doWiFiManager();

	ArduinoOTA.handle();	
	#ifdef def_mqtt_server
	bool mqtt_connected = false;
	
	if (mqtt_active) {
		if (MQTTclient.connected()) {
			mqtt_connected = true;
		} else {
			#ifdef DEBUG
			Serial.println(F("Attempt to reconnect MQTTn in loop"));
			#endif
			mqtt_connected = reconnect_mqtt();
		}

		if (mqtt_connected) {
			if (!MQTTclient.loop()) {
				#ifdef DEBUG
				Serial.print(F("Calling MQTT Loop FAILED"));
				#endif
			}
		}
		
		#ifdef DEBUG
		if (mqtt_connected != mqtt_last_connected) {
			if (mqtt_connected) {
				Serial.println(F("In loop: mqtt is CONNECTED"));
			} else {
				Serial.print(F("In loop: mqtt is DISCONNECTED"));
			}
			mqtt_last_connected = mqtt_connected;
		}
		#endif
		
		if (ReportNfiles) {
			sprintf(msg,"%d active and %d sleeping LED streams found", HP_TimeTiming[__stream_LED].numberFiles[__stream_Active], HP_TimeTiming[__stream_LED].numberFiles[__stream_Sleeping]);
			PublishMQTT(def_mqtt_topic, "/NledStr", msg);

			sprintf(msg,"%d active and %d sleeping HEAD streams found", HP_TimeTiming[__stream_HEAD].numberFiles[__stream_Active], HP_TimeTiming[__stream_HEAD].numberFiles[__stream_Sleeping]);
			PublishMQTT(def_mqtt_topic, "/NheadStr", msg);

			sprintf(msg,"%d active and %d sleeping RING streams found", HP_TimeTiming[__stream_RING].numberFiles[__stream_Active], HP_TimeTiming[__stream_RING].numberFiles[__stream_Sleeping]);
			PublishMQTT(def_mqtt_topic, "/NringStr", msg);
			
			ReportNfiles = false;
		}
	}
	#endif

	#endif

	Solenoid25 = (digitalRead(_Solenoid25Pin) == LOW);
	Solenoid14 = (digitalRead(_Solenoid14Pin) == HIGH);
	
	#ifdef DEBUG
	if (Solenoid25!=OldSolenoid25) {
		DEBUG_OUT("Solenoid 25", Solenoid25 ? "pressed" : "released");
		OldSolenoid25=Solenoid25;
	}
	if (Solenoid14!=OldSolenoid14) {
		DEBUG_OUT("Solenoid 14", Solenoid14 ? "released": "pressed");
		OldSolenoid14=Solenoid14;
	}
	#endif
	
	if (_ServoLoop.check() == 1) {
		// Move the servo
		if (_InvertedServo) {
			_Servo.writeMicroseconds(3000 - Spos);
		} else {
			_Servo.writeMicroseconds(Spos);
		}

		// Handle all buttons

		// Calculate where we are with respect to the wave form on a scale of (0 to 1)

		//if (( (digitalRead(_Solenoid25Pin) == HIGH) && !((module.getButtons() & B10000000) == B10000000)) && _WeAreCENTERED && (_MovementMode == 0)) {
		// when Solenoid25 is true, we should move, 
		// when solenoid25 is false: DO NOT MOVE
		if (_MovementMode == __MoveFixedLeft) {
			x = 0.25;
			Spos = _Left;
		} else if (_MovementMode == __MoveFixedCenter) {
			x = 0.5;
			Spos = _Center;
		} else if (_MovementMode == __MoveFixedRight) {
			x = 0.75;
			Spos = _Right;			
		} else if (!Solenoid25 && (_MovementMode != __MoveAutonomous)) {
			// Here we should STOP moving, but under some exceptions it will move BACK to the center
			//
			// NOTE the actual Dr Who pinball machine firmware activates the solenoid signal to trigger 
			// movement towards the center, when stopped and not centered!
			if ( (_MovementMode == __MoveByPinball_stop_center) && !_NeedToCenter ) {
				// Do nothing, but move to the center after a minute.
				if ((millis() - _TimeLastMovement) > __DelayTimeForCentering) {
					_NeedToCenter = true;
				}
			} else if ((_MovementMode == __MoveByPinball) || _NeedToCenter) {
				// Here we just center....
				if (_WeAreCENTERED) {
					// Here we should STOP moving, but move BACK to the center, because we are close to the center
					if (_WeAreGoingRight) {
					// Target moving towards x = 0;
						if (x < 0.5) {
						  x = max(0.0, x - _Step);
						} else { 
						x = min(1.0, x +_Step);
						}
					} else {
						// Target moving towards x=0.5;
						if (x < 0.5) {
							x = min(0.5, x + _Step);
						} else {
							x = max(0.5 , x - _Step);
						}
					}
				} else {
					// OR Continue moving the normal way towards the center...
					x += _Step;
					// Make sure we stay in the [0;1] interval.
					x = MyMap(x, 1.0);			
					// Calculate Servo position for the next time			
				}
				// We moved towards the center... BACK or forward, once centered it stops
				Spos = CalcPos(x);
			}
		} else {										// Need to move
			x += _Step;
			// Make sure we stay in the [0;1] interval.
			x = MyMap(x, 1.0);			
			
			// Calculate Servo position for the next time
			Spos = CalcPos(x);
			_NeedToCenter = false;
			_TimeLastMovement = millis();
		} // end if ((digitalRead

		_WeAreRight = (x > _XMarge) && (x < 0.5 - _XMarge);
		_WeAreLeft = (x > 0.5 + _XMarge) && (x < 1 - _XMarge);
		_WeAreCENTERED = (!_WeAreRight) && (!_WeAreLeft);
		_WeAreGoingRight = (MyMap(x+0.25, 1.0) < 0.5);

		// Display the "Centered" LED.
		if (_WeAreCENTERED) {
			digitalWrite(_CenterLEDPin, LOW);
			if (_WeAreGoingRight) {
				if (MeasureLoop) {
					MeasureLoop = false;
					unsigned long Tlong = Now - LoopTime;
					LoopTime = Now;
					if (_FirstLoop) {
						_FirstLoop = false;
					} else {
						sprintf(msg, "T %.2f s, RPM %.1f", double(Tlong) / 1000.0, 60000.0 / double(Tlong));
						PublishMQTT(def_mqtt_topic, topic_Cycle, msg);
					}
				}
			} else {
				MeasureLoop = true;
			}
		} else {
			digitalWrite(_CenterLEDPin, HIGH);
		}

#ifdef noDEBUGfornow		
		if (Spos != OOpos) {
			Count +=1;
			Serial.print(_Time);
			Serial.print(" ");
			Serial.print(_Step);
			Serial.print(" ");
			Serial.print(Count);
			Serial.print(" ");
			Serial.println(x);
			OOpos = Opos;
			Opos = Spos;
		} else {
			Count=0;
		}
#endif
	}

	
	if (_ProcessingLoop.check() == 1) {
		DEBUG_OUT("Processing loop entered","")
		// Is it indeed already time to change the "shape of movement" AND are we supposed to change it???
		if (_Shape ==__ShapeRandom) {
			if ((millis() - _TimeRandomShape) >= _RandomInterval) {  //if so, change it and wait for the next moment to change it.
			_TimeRandomShape = millis();
			_ShapeRandom = random(0,__ShapeRandom);
			}
		} else {
			_ShapeRandom = _Shape;
		}
		//update speed calc string
		_Step = CalcSweepStep(_Interval, _RPM);
	}
	// Now dealing with the LED's
	
	// Set this define, to work with a test pattern, in stead of patterns read from SD card / LittleFS.
	//#define TestLEDPattern
	#ifdef TestLEDPattern
	
	static byte TestRow = 4;
	static byte TestColumn = 24;
	static byte TestColor = 0;
	static long TestLoopTime = Now;
	static boolean ToggleMask = true;
	
	if ((Now - TestLoopTime) > 50) {
		TestLoopTime = Now;
		//MySetLed(TestRow, TestColumn, 0);
		
		TestColumn +=1;
		if (TestColumn > 24) {
			TestColumn = 0;
			TestRow+=1;
			if (TestRow>4) {
				ToggleMask = !ToggleMask;
				TestRow=0;
				TestColor+=1;
				DisplayDrivers[0].ClearBuffer();
				DisplayDrivers[1].ClearBuffer();
				DisplayDrivers[2].ClearBuffer();
				HeadDriver.ClearBuffer();		
				if (TestColor>7) {
					TestColor=1;
				}
			}
		}
		
		MySetLed(TestRow, TestColumn, TestColor);

		byte mask = (ToggleMask ? 0b11111 : (1 << (TestRow)));
		if (TestColumn >11) HeadDriver.SetEyeL(TestColor,mask); else HeadDriver.SetEyeL(0,0b11111);
		if (TestColumn >11) HeadDriver.SetEyeR(TestColor,mask); else HeadDriver.SetEyeR(0,0b11111);
		if (TestColumn >11) HeadDriver.SetNose(TestColor,mask); else HeadDriver.SetNose(0,0b11111);
		if (TestRow==1 or TestRow==3) HeadDriver.SetRing(TestColumn,0); else HeadDriver.SetRing(TestColumn,TestColor);
		
		DisplayDrivers[0].ShowLeds();
		DisplayDrivers[1].ShowLeds();
		DisplayDrivers[2].ShowLeds();
		HeadDriver.ShowLeds();
	}

	#else
	
	// Now the three display timers (for LED, head and ring) are checked and processed.
	boolean UpdateDisplayNeeded=false;
	// When he "activity timer" expires, the stream should be forced to sleeping.
	// When it is "Active", the activity is set to "Sleeping", when it is sleeping, it stays that way.

	for (byte streamType : {__stream_LED, __stream_HEAD, __stream_RING} ) {
		#ifdef DEBUG
		PrintStreamState(streamType,"entering loop");
		DEBUG_OUT("Stream source",HP_TimeTiming[streamType].StreamSource)
		#endif

		// These are used to select what is played out in sleeping mode: sleeping or active files (when swapped)
		byte SwappedSleepingStream = HP_TimeTiming[streamType].SwapActiveSleeping ?  __stream_Active   : __stream_Sleeping;
		byte SwappedActiveStream   = HP_TimeTiming[streamType].SwapActiveSleeping ?  __stream_Sleeping : __stream_Active;
	
		// Check whether Solenoid activated things need activating or extending.
		unsigned long ActiveExtend = 0;
		if (!Solenoid14) ActiveExtend += HP_TimeTiming[streamType].Sol14ExtendTime;
		if (Solenoid25) ActiveExtend += HP_TimeTiming[streamType].Sol25ExtendTime;

		// BUG, indien hij al actief is, NIET de file openen en sluiten, dat doet hij nu wel.
		// Dit komt, omdat de timer "ActiveProcessing" in de VOLGENDE check meteen al afgaat... 
		// bij grotere active extend tijden treedt het niet op
		// Verandering: zet de timer LANG.
		// Als de solenoid signalen verdijnen EN hij actief is, DAN 1x timer resetten naar de korte tijd.
		
		if (ActiveExtend>0) {
			DEBUG_OUT("Extend active time by ",ActiveExtend)
			// Whatever the state, reset the timer.
			// Set it long for now, but remember the time we want..
			HP_TimeTiming[streamType].WeJustBecameActive = true;
			HP_TimeTiming[streamType].HowLongToRemainActive = ActiveExtend;
			HP_TimeTiming[streamType].ActiveProcessing.interval(5000); //5 seconds should be more than enough
			HP_TimeTiming[streamType].ActiveProcessing.reset();
			
			//Depending on state (when it is sleeping) do something, so it gets activated.
			if (!HP_TimeTiming[streamType].StreamIsActive) {
				// Stream was not active (sleeping), activate it and redirect input.
				DEBUG_OUT("stream activated",streamType)
				HP_TimeTiming[streamType].StreamIsActive = true;
				
				if (HP_TimeTiming[streamType].HasFiles[SwappedSleepingStream]) {
					//close the open files
					DEBUG_OUT("close data stream (sleeping)",streamType)
					HP_TimeTiming[streamType].dataFile.close();
				}
				//Open the active stream
				// Does the active stream have files?
				if (HP_TimeTiming[streamType].HasFiles[SwappedActiveStream]) {
					// Open a new dataFile
					DEBUG_OUT("open data stream (active)",streamType);
					OpenSDFile(streamType, SwappedActiveStream);		
				}
				// Force reading a pattern, independent of the pattern timer: we became active.
				HP_TimeTiming[streamType].GetNextPattern = true;
				
				// Reset the timer (TimeProcessing) for getting (reading) new patterns
				HP_TimeTiming[streamType].TimeProcessing.interval(HP_TimeTiming[streamType].PatternTime[SwappedActiveStream]);
				HP_TimeTiming[streamType].TimeProcessing.reset();
	
			}
		} else {
			if (HP_TimeTiming[streamType].WeJustBecameActive) {
				HP_TimeTiming[streamType].WeJustBecameActive = false;
				HP_TimeTiming[streamType].ActiveProcessing.interval(HP_TimeTiming[streamType].HowLongToRemainActive);
				HP_TimeTiming[streamType].ActiveProcessing.reset();
			}
		}
		
		// Check whether this timing is active or not, when it is sleeping, there is nothing to do.
		if (HP_TimeTiming[streamType].StreamIsActive) {
			// Check whether the timing for activity is expired
			if (HP_TimeTiming[streamType].ActiveProcessing.check() == 1) {
				DEBUG_OUT("ActiveProcessing.check() active->sleep",streamType);
				// Make the stream sleep..
				HP_TimeTiming[streamType].StreamIsActive = false;	
				
				// If it was active, it must be set to sleeping.
				// Does the active stream have files?
				if (HP_TimeTiming[streamType].HasFiles[SwappedActiveStream]) {
					//close the open file
					HP_TimeTiming[streamType].dataFile.close();		
				}
				// Open the sleeping stream
				// Does the sleeping stream have files?
				if (HP_TimeTiming[streamType].HasFiles[SwappedSleepingStream]) {
					// Open a new dataFile
					DEBUG_OUT("open data stream (sleeping)",streamType);
					OpenSDFile(streamType, SwappedSleepingStream);		
				}
				
				// Reset the timer loop for activity (ActiveProcessing) set it really long, because we are sleeping it doesn't matter.
				// It will never be checked anyhow...
				HP_TimeTiming[streamType].ActiveProcessing.interval(4000000000);
				HP_TimeTiming[streamType].ActiveProcessing.reset();
				
				// Reset the timer (TimeProcessing) for getting (reading) new patterns
				HP_TimeTiming[streamType].TimeProcessing.interval(HP_TimeTiming[streamType].PatternTime[__stream_Sleeping]);
				HP_TimeTiming[streamType].TimeProcessing.interval(ActiveExtend);
				HP_TimeTiming[streamType].TimeProcessing.reset();
				
				// Force reading a pattern, independent of the pattern timer: we became sleeping
				HP_TimeTiming[streamType].GetNextPattern = true;
			}	
		}

		// At this point, the correct streams are open and when the 
		// "pattern timers" kick in, it is time to display the next pattern.			
		// Check whether the timing for PATTERN is expired
		if (HP_TimeTiming[streamType].TimeProcessing.check()==1 ) {
			HP_TimeTiming[streamType].GetNextPattern = true;
		}
	
		// get new patterns
		for (byte streamType : {__stream_LED, __stream_HEAD, __stream_RING} ) {
			switch (HP_TimeTiming[streamType].StreamSource) {
			case __streamFrom_Serial:
				DEBUG_OUT("New pattern (redirected) for stream", streamType)		
				//ReadNewPattern(byte LEDorHEADorRING)
				if (ReadNewPattern(streamType)) {
					UpdateDisplayNeeded=true;
				}
				break;
			case __streamFrom_SD:
			case __streamFrom_MQTT_SD:
			case __streamFrom_MQTT_Loop:
				if (HP_TimeTiming[streamType].GetNextPattern) {
					DEBUG_OUT("Search new pattern (from file) for stream", streamType)		
					if (ReadNewPattern(streamType)) {
						UpdateDisplayNeeded=true;
						HP_TimeTiming[streamType].GetNextPattern = false;  //Ensure you wait until you read again..
					}
				}
				break;
			}		
		}
	}
	// Finally there is a check whether the display needs updating...
	

	if (_BLINKprocessing.check()==1) {
		UpdateDisplayNeeded=true;
		Blink_on=!Blink_on;
		// Ensure flash / strobe are in sync
		if (Blink_on) {
			Flash_on=true;
			_FLASHprocessing.reset();
			_STROBEprocessing.reset();
		}
	}
	
	if (_FLASHprocessing.check()==1) {
		UpdateDisplayNeeded=true;
		Flash_on=false;
	}
		
	if (_STROBEprocessing.check()==1) {
		UpdateDisplayNeeded=true;
		Strobe_on=!Strobe_on;
	}
	
	if (EscHit) {
		EscHit = false;
		ProcessESC();
	}
	
	if (UpdateDisplayNeeded) SetDisplayNow();
	#endif
}

void ResetFlashTimer(byte ACTIVEorSLEEPING) {
	_FLASHprocessing.interval(HP_FlashTime[ACTIVEorSLEEPING]);
	_FLASHprocessing.reset();
	Flash_on=true;
}

void ResetHeadTimers(byte ACTIVEorSLEEPING) {
	_BLINKprocessing.interval(HP_BlinkTime[ACTIVEorSLEEPING]);
	_BLINKprocessing.reset();

	ResetFlashTimer(ACTIVEorSLEEPING);

	_STROBEprocessing.interval(HP_StrobeTime[ACTIVEorSLEEPING]);
	_STROBEprocessing.reset();
	Blink_on=true;
	Strobe_on=true;
}

unsigned long ParseInt(byte LEDorHEADorRING) {
	int inChar;
	String Avalue = "0";
	boolean SeenDigits=false;
	while (true) {
		inChar = GetCharSync(LEDorHEADorRING);		
		switch (inChar) {
		case 0:
			break;
		case '0': 
		case '1': 
		case '2': 
		case '3': 
		case '4': 
		case '5': 
		case '6': 
		case '7': 
		case '8': 
		case '9': 
			// add it to the value string:
			Avalue += (char)inChar;
			SeenDigits=true;
			if(ShowOutput(LEDorHEADorRING)) Serial.print((char)inChar);
			break;
		case 13: 
		case 10:
		case ' ':
		case ',':
		default:
			if (SeenDigits) {
				if (ShowOutput(LEDorHEADorRING)) Serial.print(F("\r\n\n"));
				return Avalue.toInt();
			} else break;
		}
	}
	return 0;
}

byte ParseIntensity(byte LEDorHEADorRING) {
	/* This reads 1 -or- to 3 integers, for 
	- intensity (of active stream)
	- minimum intensity of this stream
	- maximum intensity of this stream
	
	For compatibility reasons, provide just one number > 0 to only set the intensity, but:
	send a zero (0), FOLLOWED by three numbers to set all three.
	*/
	
	unsigned long TestReturn = ParseInt(LEDorHEADorRING);
	
	if (TestReturn>0) {
		// This is the "normal" (old behaviour), only one number.
		DEBUG_OUT("Stream intensity read: ",TestReturn)
		return (Constrain(TestReturn,HP_TimeTiming[LEDorHEADorRING].MinIntensity,HP_TimeTiming[LEDorHEADorRING].MaxIntensity));
	} else {
		// You provided a 0, now read three numbers
		TestReturn = ParseInt(LEDorHEADorRING);
		HP_TimeTiming[LEDorHEADorRING].MinIntensity = ParseInt(LEDorHEADorRING);
		HP_TimeTiming[LEDorHEADorRING].MaxIntensity = ParseInt(LEDorHEADorRING);
		if (HP_TimeTiming[LEDorHEADorRING].MinIntensity > HP_TimeTiming[LEDorHEADorRING].MaxIntensity) {
			//Swap them
			byte temp = HP_TimeTiming[LEDorHEADorRING].MinIntensity;
			HP_TimeTiming[LEDorHEADorRING].MinIntensity = HP_TimeTiming[LEDorHEADorRING].MaxIntensity;
			HP_TimeTiming[LEDorHEADorRING].MaxIntensity = temp;
		}

		DEBUG_OUT("Intensity: ", TestReturn)
		DEBUG_OUT("Min Intensity: ", HP_TimeTiming[LEDorHEADorRING].MinIntensity)
		DEBUG_OUT("Max Intensity: ", HP_TimeTiming[LEDorHEADorRING].MaxIntensity)
	} 
	return (Constrain(TestReturn,HP_TimeTiming[LEDorHEADorRING].MinIntensity,HP_TimeTiming[LEDorHEADorRING].MaxIntensity));
}


byte Constrain(byte x, byte Min, byte Max) {
	byte temp = x<Max ? x : Max;
	return temp>Min ? temp : Min;
}


TimingType ParseTiming(TimingType HP_Time,byte LEDorHEADorRING) {

	/* This read 1 -or- to 3 integers, for 
	- patterntime (of active stream)
	- activation time when Solenoid14 is pressed
	- activation time when solenoid25 is pressed.
	
	For compatibility reasons, provide just one number > 0 to only set the PatternTime, but:
	send a zero (0), FOLLOWED by three numbers to set all three.
	*/
	
	unsigned long TestReturn = ParseInt(LEDorHEADorRING);
	
	if (TestReturn>0) {
		// This is the "normal" (old behaviour), only one number.
		HP_Time.PatternTime[HP_Time.StreamIsActive ? __stream_Active : __stream_Sleeping] = TestReturn;
		DEBUG_OUT("Default time read: ",TestReturn)
	} else {
		// You provided a 0, now read three numbers
		HP_Time.PatternTime[HP_Time.StreamIsActive ? __stream_Active : __stream_Sleeping] = ParseInt(LEDorHEADorRING);
		HP_Time.Sol14ExtendTime = ParseInt(LEDorHEADorRING);
		HP_Time.Sol25ExtendTime = ParseInt(LEDorHEADorRING);
		
		DEBUG_OUT("Extended time  read: ",HP_Time.PatternTime[HP_Time.StreamIsActive ? __stream_Active : __stream_Sleeping])
		DEBUG_OUT("Extended Sol14 read: ",HP_Time.Sol14ExtendTime)
		DEBUG_OUT("Extended Sol25 read: ",HP_Time.Sol25ExtendTime)
	} 
	return HP_Time;
}


void SetDisplayNow(){
	/*
	Serial.println(DisplayDrivers[0].ShowLeds());
	Serial.println(DisplayDrivers[1].ShowLeds());
	Serial.println(DisplayDrivers[2].ShowLeds());
	*/
	DisplayDrivers[0].ShowLeds();
	DisplayDrivers[1].ShowLeds();
	DisplayDrivers[2].ShowLeds();

	
	//This must be done differently: make the HEAD routines change the driver LEDS values, then here they can be _just_ displayed using showleds()
	
	byte ACTIVEorSLEEPING = HP_TimeTiming[__stream_HEAD].StreamIsActive ? __stream_Active: __stream_Sleeping;
	byte tempcolor = CalcColor(Nose_pattern[ACTIVEorSLEEPING],Nose);
	HeadDriver.SetNose(tempcolor,0b11111);
	
	tempcolor = CalcColor(R_eye_pattern[ACTIVEorSLEEPING],R_eye);
	HeadDriver.SetEyeR(tempcolor,0b11111);
	
	tempcolor = CalcColor(L_eye_pattern[ACTIVEorSLEEPING],L_eye);
	HeadDriver.SetEyeL(tempcolor,0b11111);
	
	HeadDriver.ShowLeds();
}

byte CalcColor(byte pattern, byte color) {
	switch (pattern) {
	case __ON:
		return color;
	case __FLASH:
		return (Flash_on ? color : 0);
	case __BLINK:
		return (Blink_on ? color : 0);
	case __STROBE:
		return (Strobe_on ? color : 0);
	case __STROBEBLINK:
		return ((Blink_on && Strobe_on) ? color : 0);
	case __STROBEFLASH:
		return ((Flash_on && Strobe_on) ? color : 0);
	default:
		return 0;
	}
}

boolean ReadNewPattern(byte LEDorHEADorRING) {
	String ReadString;
	boolean FoundPattern = false;
	boolean ParseOLD = false;
	
	byte RandomGroup;
	byte temp_L_eye;
	byte temp_Nose;
	byte temp_R_eye;

	char inChar;
	//int  iChar;

	short CharPos=0;
	
	while (!FoundPattern) {

		inChar = toupper(GetChar(LEDorHEADorRING, CharPos));
		//iChar = inChar;
		if (inChar==0) {
			/*
			if (ShowSerialInput==__stream_HEAD) {
				RecolorHead(ACTIVEorSLEEPING);
			}
			//Serial.println("Skip & out");
			*/
			DEBUG_OUT ("no pattern found, zero char returned",(int)inChar)
			return false;
		}
		
		byte ACTIVEorSLEEPING = HP_TimeTiming[LEDorHEADorRING].StreamIsActive ? __stream_Active : __stream_Sleeping;
		CharPos++;
		//DEBUG_OUT ("CharPosition -------------anything......",CharPos)
		
		if (CharPos==1) {						// Position 1 is only valid for a command
			//first the common interpretations
			DEBUG_OUT ("Char Position 1--------GEN",(int)inChar)
			switch (inChar) {
			case 0: case 13:
				CharPos--;
				break;
			/*
			case 'A':
				InterpretAnimationIntervals(LEDorHEADorRING);
				CharPos=0;	
				break;
			*/	
			case 'D':// Read the description until the end of the line
				ReadString = ParseString(LEDorHEADorRING);
				ShowFilenames_opened(LEDorHEADorRING, ACTIVEorSLEEPING, ReadString);
				//if (ShowFilenames) {PrintTab();Serial.print(ReadString);}
				CharPos=0;
				break;		
			case 'I': // Read the intensity until the end of the line
				uint8_t MyIntensity;
				MyIntensity=ParseIntensity(LEDorHEADorRING);
				if (ShowOutput(LEDorHEADorRING)) { PrintDesc(31);}
				
				HP_TimeTiming[LEDorHEADorRING].AnimationIntensity = MyIntensity;
				switch (LEDorHEADorRING) {
				case __stream_LED:
					SetIntensityLED(MyIntensity);
					break;
				case __stream_HEAD:
					SetIntensityHEAD(MyIntensity);
					break;
				case __stream_RING:
					SetIntensityRING(MyIntensity);
					break;
				}		
				////////////////// _LEDprocessing.reset(); //Is this necesary??????
				CharPos=0;
				break;
			case 'T': // Read timing (one or three values)
				HP_TimeTiming[LEDorHEADorRING] = ParseTiming(HP_TimeTiming[LEDorHEADorRING],LEDorHEADorRING);
				// maybe reset the timers here...
				// Not doing the one for active processing, as that one is triggerd externally.
				
				// Reset the timer (TimeProcessing) for getting (reading) new patterns
				HP_TimeTiming[LEDorHEADorRING].TimeProcessing.interval(HP_TimeTiming[LEDorHEADorRING].PatternTime[HP_TimeTiming[LEDorHEADorRING].StreamIsActive ? __stream_Active : __stream_Sleeping]);
				HP_TimeTiming[LEDorHEADorRING].TimeProcessing.reset();
				CharPos=0;
				PrintColorStatus(LEDorHEADorRING);	
				break;
			default: //Now  the specific patterns...
				switch (LEDorHEADorRING) {
				case __stream_RING: //The new ring patterns...
					DEBUG_OUT("RING processing",inChar)
					switch (inChar) {						
					case 'R'://Read all LED colors: upto 24
						if (ShowOutput(LEDorHEADorRING)) PrintDesc(34);						
						ParseRING();
						FoundPattern=true;
						break;
					}
					break;
				case __stream_LED: //All the ledmatrix patterns
					DEBUG_OUT("LED processing",inChar)
					ParseOLD = false; //Will be set to TRUE when parsing X or Y sequences...
					switch (inChar) {
					case 'S':// Read the SHORT description until the end of the line
						ReadString = ParseString(LEDorHEADorRING);
						CharPos=0;
						break;
					/*
					case 'L':
						ShiftLeft();
						InvisibleBufferDirty=true;
						SetDisplayNow();
						break;
					case 'R':
						ShiftRight();
						//InvisibleBufferDirty=true;
						SetDisplayNow();
						break;
					*/
					case 'M':
						if (ShowOutput(LEDorHEADorRING)) PrintDesc(32);
						ModeDisplay = ParseInt(LEDorHEADorRING);				
						switch (ModeDisplay) {  //This sets the display mode (for lowres), 0 is normal, 2 is compressed and centered. 1 shifted to the left, 3 shifted to the right 
						case 0: //Normalmode display on lowres LEDS
						case 1: //Display compressed, show left: 12 blanks right
						case 2: //Display compressed, show centered: 6 blanks left and right
						case 3: //Display compressed, show right: 12 blanks left
							break;
						default:
							ModeDisplay=0; //Force to a known mode: 0 if something else...
							break;
						}
						break;

					case 'X':
						ParseOLD=true;
					case 'B':
						HighResMode=false;
						if (ShowOutput(LEDorHEADorRING)) {PrintDesc(33);}						
						ParseLEDS(ParseOLD);
						FoundPattern=true;
						break;
					case 'Y':
						ParseOLD=true;
					case 'C':
						if (ShowOutput(LEDorHEADorRING)) {PrintDesc(34);}	
						HighResMode=true;
						ParseLEDS(ParseOLD);
						FoundPattern=true;
					}
					break;
				case __stream_HEAD: //All the head patterns 
					DEBUG_OUT("LED processing",inChar)
					byte ACTIVEorSLEEPING = HP_TimeTiming[__stream_HEAD].StreamIsActive ? __stream_Active : __stream_Sleeping;
					switch (inChar) {				
					case 'B':// Read the BLINK timing until the end of the line
						if (ShowOutput(LEDorHEADorRING)) PrintDesc(12);
						HP_BlinkTime[ACTIVEorSLEEPING]=ParseInt(LEDorHEADorRING);
						CharPos=0;
						ResetHeadTimers(ACTIVEorSLEEPING);
						break;
					case 'F':// Read the FLASH timing until the end of the line
						if (ShowOutput(LEDorHEADorRING)) PrintDesc(11);
						HP_FlashTime[ACTIVEorSLEEPING]=ParseInt(LEDorHEADorRING);
						CharPos=0;
						ResetHeadTimers(ACTIVEorSLEEPING);
						PrintColorStatus(LEDorHEADorRING);	
						break;
					case 'S':// Read the STROBE timing until the end of the line
						if (ShowOutput(LEDorHEADorRING)) PrintDesc(13);
						HP_StrobeTime[ACTIVEorSLEEPING]=ParseInt(LEDorHEADorRING);
						//DEBUG_OUT("ACTIVEorSLEEPING",ACTIVEorSLEEPING)
						//DEBUG_OUT("HP_StrobeTime",HP_StrobeTime[ACTIVEorSLEEPING])
						CharPos=0;
						ResetHeadTimers(ACTIVEorSLEEPING);
						break;
					/*
					case 'L'://Determine how the lights work:\n\ro On\n\rb Blink\n\rf Flash\n\rO Strobe\n\rB Blinkstrobing\n\rF Flashstrobing\n\n\r
						if (ShowOutput(LEDorHEADorRING)) {PrintColorStatus(LEDorHEADorRING);PrintDesc(25);} 
						
						L_eye_pattern[ACTIVEorSLEEPING] = GetHeadPattern(GetCharSync(LEDorHEADorRING));
						Nose_pattern[ACTIVEorSLEEPING] = GetHeadPattern(GetCharSync(LEDorHEADorRING));
						R_eye_pattern[ACTIVEorSLEEPING] = GetHeadPattern(GetCharSync(LEDorHEADorRING));
						
						PrintColorStatus(LEDorHEADorRING);	
			
						CharPos=0;
						
						break;
					*/
					case 'C'://Determine the color of the lights: [0..7], Random, Random as group, (previous) L_eye, nose, r_eye
						if (ShowOutput(LEDorHEADorRING)) {PrintColorStatus(LEDorHEADorRING);PrintDesc(27);}

						
						boolean continueProcessing = true; 	// Set to FALSE when eol is reached in the following routines, then defaults will be applied.
						
						L_eye_nextcol[ACTIVEorSLEEPING] = GetCharColor(LEDorHEADorRING, L_eye_nextcol[ACTIVEorSLEEPING], continueProcessing);
						Nose_nextcol[ACTIVEorSLEEPING] = GetCharColor(LEDorHEADorRING, Nose_nextcol[ACTIVEorSLEEPING], continueProcessing);
						R_eye_nextcol[ACTIVEorSLEEPING] = GetCharColor(LEDorHEADorRING, R_eye_nextcol[ACTIVEorSLEEPING] , continueProcessing);
						
						// Get ON, Flash and Blink instructions from next 3 optional character.
						byte L_eye_temppat = GetCharPattern(LEDorHEADorRING, L_eye_pattern[ACTIVEorSLEEPING],continueProcessing);
						byte Nose_temppat = GetCharPattern(LEDorHEADorRING, Nose_pattern[ACTIVEorSLEEPING],continueProcessing);
						byte R_eye_temppat = GetCharPattern(LEDorHEADorRING, R_eye_pattern[ACTIVEorSLEEPING],continueProcessing);

						// Get strobe in normal instructions from the next 3 optional characters.
						L_eye_pattern[ACTIVEorSLEEPING] = GetCharStrobing(LEDorHEADorRING, L_eye_temppat,L_eye_pattern[ACTIVEorSLEEPING],continueProcessing);
						Nose_pattern[ACTIVEorSLEEPING] = GetCharStrobing(LEDorHEADorRING, Nose_temppat,Nose_pattern[ACTIVEorSLEEPING],continueProcessing);
						R_eye_pattern[ACTIVEorSLEEPING] = GetCharStrobing(LEDorHEADorRING, R_eye_temppat,R_eye_pattern[ACTIVEorSLEEPING],continueProcessing);
				
						RecolorHead(ACTIVEorSLEEPING);		// makes unique random values really unique and apply color SPEC to get the real color.
						
						PrintColorStatus(LEDorHEADorRING);						
						
						ResetHeadTimers(ACTIVEorSLEEPING);

						FoundPattern=true;
						break;
					}
				}			
			}
		}
		
		switch (inChar) {
		//DEBUG_OUT ("CharPosition---IGNORING IGNORING IGNORING",CharPos)
		case 10: case 13:// CR en LF
			CharPos=0;
			inChar=0;
			//FoundPattern = ForceOut;
			//ForceOut=false;
			break;
		default:
			inChar=0; 							// Ignore the line if not parsing something...
		}	
	}
	
	if (FoundPattern) HP_TimeTiming[LEDorHEADorRING].PatternOK = true;
	return FoundPattern;
}

void PrintStreamState(byte LEDorHEADorRING,char* desc) {
	DEBUG_OUT(desc,LEDorHEADorRING);
	switch (LEDorHEADorRING) {
	case __stream_LED:
		Serial.print("LED stream (");
		break;
	case __stream_HEAD:
		Serial.print("HEAD stream (");
		break;
	case __stream_RING:
		Serial.print("RING stream (");
		break;
	}
	
	Serial.println(HP_TimeTiming[LEDorHEADorRING].StreamIsActive ? "active)": "sleeping)");
}


void PrintColorStatus (byte LEDorHEADorRING) {
	#ifdef DEBUG
	if (true) {
	#else
	if (ShowOutput(LEDorHEADorRING)) {
	#endif
		byte ACTIVEorSLEEPING = HP_TimeTiming[LEDorHEADorRING].StreamIsActive ? __stream_Active: __stream_Sleeping;
		PrintDesc(30);Serial.println(HP_TimeTiming[LEDorHEADorRING].PatternTime[ACTIVEorSLEEPING]);
		PrintDesc(12);Serial.println(HP_BlinkTime[ACTIVEorSLEEPING]);
		PrintDesc(11);Serial.println(HP_FlashTime[ACTIVEorSLEEPING]);
		PrintDesc(13);Serial.println(HP_StrobeTime[ACTIVEorSLEEPING]);
		
		PrintDescN(6);PrintPattern(L_eye_pattern[ACTIVEorSLEEPING]);PrintColor(L_eye_nextcol[ACTIVEorSLEEPING]);
		PrintDescN(7);PrintTab();PrintPattern(Nose_pattern[ACTIVEorSLEEPING]);PrintColor(Nose_nextcol[ACTIVEorSLEEPING]);
		PrintDescN(8);PrintPattern(R_eye_pattern[ACTIVEorSLEEPING]);PrintColor(R_eye_nextcol[ACTIVEorSLEEPING]);PrintCR();			
	}
}

byte GetHeadPattern(char inChar) {
	DEBUG_OUT("Char HeadPattern **********",inChar)
	switch (inChar) {
	case 'b':
		return __BLINK;
	case 'f':
		return __FLASH;
	case 'B':
		return __STROBEBLINK;
	case 'F':
		return __STROBEFLASH;
	case 'O':
		return __STROBE;
	default:
		return __ON;
	}
}

void PrintPattern(char inChar) {
	PrintTab();
	switch (inChar) {
	case __STROBE:
		PrintDesc(13);
		break;
	case __BLINK:
		PrintDesc(12);
		break;
	case __FLASH:
		PrintDesc(11);
		break;
	case __STROBEBLINK:
		PrintDesc(14);
		break;
	case __STROBEFLASH:
		PrintDesc(15);
		break;
	case __ON:
		PrintDesc(29);
	}
}

byte GetHeadColor(char inChar,byte& RandomGroup, byte CurrentColor) {
	byte NewColor;
	switch (inChar) {
	case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
		return inChar - 48;
	case 'A': case 'U':
		do NewColor = random(1,8); while (NewColor==CurrentColor);
		return NewColor;
	case 'G':
		if (RandomGroup==255){
			do RandomGroup=random(1,8); while (RandomGroup==CurrentColor);
		}
		return RandomGroup;
	case 'L':
		return L_eye;
	case 'N':
		return Nose;
	case 'R':
		return R_eye;
	default:
		return 0;
	}
}

void PrintColor(char inChar) {
	PrintTab();
	switch (inChar) {
	case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
		PrintDesc(inChar - 32);
		break;
	case 'A':
		PrintDesc(9);
		break;
	case 'U':
		PrintDesc(39);
		break;
	case 'G':
		PrintDesc(10);
		break;
	case 'L':
		PrintDesc(6);
		break;
	case 'N':
		PrintDesc(7);
		break;
	case 'R':
		PrintDesc(8);
		break;
	}
}

void RecolorHead(byte ACTIVEorSLEEPING){
	byte RandomGroup=255;

	byte temp_L_eye;
	byte temp_R_eye;
	byte temp_Nose;
	
	temp_L_eye = GetHeadColor(L_eye_nextcol[ACTIVEorSLEEPING],RandomGroup,L_eye);
	temp_Nose = GetHeadColor(Nose_nextcol[ACTIVEorSLEEPING],RandomGroup,Nose);	
	temp_R_eye = GetHeadColor(R_eye_nextcol[ACTIVEorSLEEPING],RandomGroup,R_eye);

	//Now... RECHECK unique...
	while ((L_eye_nextcol[ACTIVEorSLEEPING]=='U') && ((temp_R_eye==temp_L_eye) || (temp_L_eye==temp_Nose) )) temp_L_eye = GetHeadColor(L_eye_nextcol[ACTIVEorSLEEPING],RandomGroup,L_eye);
	while (( Nose_nextcol[ACTIVEorSLEEPING]=='U') && (( temp_Nose==temp_L_eye) || (temp_R_eye==temp_Nose) ))  temp_Nose = GetHeadColor( Nose_nextcol[ACTIVEorSLEEPING],RandomGroup, Nose);		
	while ((R_eye_nextcol[ACTIVEorSLEEPING]=='U') && ((temp_R_eye==temp_L_eye) || (temp_R_eye==temp_Nose) )) temp_R_eye = GetHeadColor(R_eye_nextcol[ACTIVEorSLEEPING],RandomGroup,R_eye);
	
	
	L_eye = temp_L_eye;
	R_eye = temp_R_eye;
	Nose = temp_Nose;	
}

void SetIntensityLED(int MyIntensity) {
	DisplayDrivers[0].setBrightness(MyIntensity<<1);
	DisplayDrivers[1].setBrightness(MyIntensity<<1);
	DisplayDrivers[2].setBrightness(MyIntensity<<1);
}

void SetIntensityHEAD(int MyIntensity) {
	HeadDriver.setBrightness(MyIntensity<<1);
}

void SetIntensityRING(int MyIntensity) {
	HeadDriver.setBrightness(MyIntensity<<1);
}
		
byte RedBit(byte color){
	return (color & 4) >> 2;
}			
byte BlueBit(byte color){
	return (color & 1);
}
byte GreenBit(byte color){
	return (color & 2) >> 1;
}

void ParseRING() {
		
	short CharPos = 0;
	int inChar;
	
	//Clear all leds
	for (uint8_t p=0;p<25;p++) {
		HeadDriver.SetRing(p,0);
	}
	
	byte pointer=0;
	while (pointer<24) {
		inChar = GetChar(__stream_RING, CharPos);		
		switch (toupper(inChar)) {
		case 0:
			break;
		case 'Z': case 10: case 13:
			if(ShowOutput(__stream_RING)) Serial.print(F("Z\r\n\n"));
			pointer=24;
			break;
		case '0':
		case '1': 
		case '2': 
		case '3': 
		case '4': 
		case '5': 
		case '6': 
		case '7':
			HeadDriver.SetRing(pointer++,inChar-48);
			break;
		case 'A':	//Start of led definition, number 0-7 to follow.
			///////////////////// _RINGprocessing.reset();
			pointer=0;
			break;
		default:
			break;
		}
	}
}

void ParseLEDS(boolean ParseOLD) {
	boolean SawSeparator=true;
	String Avalue = "0";
	int pointer = 0;

	
	byte HiByte;
	byte LoByte;
	byte FromByte;
	boolean FoundPattern=false;
	
	byte BlueByte=0;
	byte GreenByte=0;
	byte RedByte;
	
	short CharPos = 0;
	byte bank = 0;
	byte parseRow = 0;
	
	
	int inChar;
	
	//Clear all buffers
	DisplayDrivers[0].ClearBuffer();
	DisplayDrivers[1].ClearBuffer();
	DisplayDrivers[2].ClearBuffer();
	
	/*Erase non visible elements from buffer 1 and 5
	if (InvisibleBufferDirty) {
		ClearInvisibleBuffers;
		InvisibleBufferDirty=false;
	}
	*/
	
	while (!FoundPattern) {
	//while (pointer<=47) {
		inChar = GetChar(__stream_LED, CharPos);		
		switch (toupper(inChar)) {
		case 0:
			break;
		case 'Z':
			if(ShowOutput(__stream_LED)) Serial.print(F("Z\r\n\n"));
			FoundPattern = true;
		case ',': case ' ': case 10: case 13:
			if (!SawSeparator) {
			 /* 
				OLD PARSING: X/Y
				For LOW RES:
				32 numbers are possible for a lowres string
				They are as follows:
					 0-14 (15 bytes) go into buffer 2: these are 8 columns of 5 common anode RGB LEds, so 120  LEDS
					15-29 (15 bytes) go into buffer 3: these are 5 columns of 5 common anode RGB LEds, so 75 LEDS: 
					THESE buffer 2 15 bytes use ONLY 5 bits, so 5x15 = 75 LEDS are used and 45 LEDS (3 bytes) not used of Buffer3.
			 
				For HIGH RES:
				48 numbers (bytes) are possible for a highres string
				They are as follows:
					 0-14 (15 bytes) go into buffer 2: these are 8 columns of 5 common anode RGB LEds, so 120  LEDS
					15-29 (15 bytes) go into buffer 3: these are 8 columns of 5 common anode RGB LEds, so 120 LEDS
					30-44 (15 bytes) go into buffer 4: these are 8 columns of 5 common anode RGB LEds, so 120 LEDS
				what remains are 3 bytes (one byte from each module: bit "16"), or 24 LEDS. 
				These are used for:
				- one column 25 (5 RBG common kathode leds, so 15 LEDS)
				- 3 lights composed of single Red, Green and Blue LEDS each for (common anode) Nose, Left eye and Right eye, so 9 individual LEDS
				
				From a high res perspective, filling the command to send to the TM1640 buffer would make sense 16 bytes for each TM1640
				to be compatible with the low resolution, which uses only 15 bytes, I use three sequences of 15 bytes each, so 45 bytes.
				Then I send the remaining 3 bytes, as a highRES version of the hardware uses these bits for the last row (15 leds) and nose and eyes (9 leds)
				Row and nose/eyes together they use the last 3 bytes (byte 46, 47 and 48), which are provided in the string AFTER the first 3x15=45 bytes...

				NEW PARSING TM1640:
				For LOW RES:
					13 numbers are possible for a highres string, one per column
					Each number is only for the 15 bits (so 15 LEDS or 5 RGB leds):
				
				They are as follows:
					 0- 7 (8 digits) go into driver1: these are 8 columns of 5 RGB LEds, so 120 LEDS
					 8-12 (5 digits) go into driver2: these are 5 columns of 5 RGB LEds, so  75 LEDS
			
				For HIGH RES:
				25 numbers are possible for a highres string
				Each number is only for the 15 bits: one number is added for column25, with special routines.
				
				They are as follows:
					 0- 7 (8 digits) go into driver1: these are 8 columns of 5 RGB LEds, so 120 LEDS
					 8-15 (8 digits) go into driver2: these are 8 columns of 5 RGB LEds, so 120 LEDS
					16-23 (8 digits) go into driver3: these are 8 columns of 5 RGB LEds, so 120 LEDS
					   24 (one digit): is for column25 (6, 6 and 3) and NOSE/EYES (3+3+3) going to all three drivers.
					
				From a high res perspective, filling the command to send to the TM1640 buffer would make sense 16 bytes for each TM1640
				to be compatible with the low resolution, which uses only 13 ints, I use three sequences of 15 bytes each, so 45 bytes.

				*/

				if (ParseOLD) {
					//Here parsing the old fashioned way... X and Y commands...
					//Must wait till we have the three bytes that make all colors for 8 RGB LEDS......
					BlueByte = GreenByte;
					GreenByte = RedByte;
					RedByte = Avalue.toInt();
					//DEBUG_OUT("Pointer ",pointer)					
					if (((pointer+1) % 3) == 0) {
	
						if (bank < 3) { //Normal LEDS for 3x8 columns
							for (int j=0;j<8;j++) {
								MySetLed(parseRow,LowResColumnToHighRes(bank*8+j),GetColorOLD(j,BlueByte,GreenByte,RedByte));
								//DisplayDrivers[bank].SetLed(parseRow,j,GetColorOLD(j,BlueByte,GreenByte,RedByte));
							}							
						} else {		//Column 25 in three bytes...
							MySetLed(0,24,GetRowColorSwapRedBlue(0,RedByte));
							MySetLed(1,24,GetRowColorSwapRedBlue(1,RedByte));
							MySetLed(2,24,GetRowColorSwapRedBlue(0,GreenByte));
							MySetLed(3,24,GetRowColorSwapRedBlue(1,GreenByte));
							MySetLed(4,24,GetRowColorSwapRedBlue(0,BlueByte));
							
							/*
							DisplayDrivers[0].SetLedExtra(0,GetRowColorSwapRedBlue(0,RedByte));   //Sets LED 1
							DisplayDrivers[0].SetLedExtra(1,GetRowColorSwapRedBlue(1,RedByte));   //Sets LED 2
							DisplayDrivers[1].SetLedExtra(0,GetRowColorSwapRedBlue(0,GreenByte)); //Sets LED 3
							DisplayDrivers[1].SetLedExtra(1,GetRowColorSwapRedBlue(1,GreenByte)); //Sets LED 4
							DisplayDrivers[2].SetLedExtra(1,GetRowColorSwapRedBlue(0,BlueByte));  //Sets LED 5
							*/
						}
						
						parseRow++;
						if (parseRow==5) {
							bank++;
							parseRow = 0;							
						}
					}
				} else { //Parsing new:
					int col = LowResColumnToHighRes(pointer);
					//Serial.print(F("\n\rpointer, bank, col, col2: "));Serial.print(pointer);Serial.print(F("\t"));
					int bank = col >> 3;
					col = col - (bank<<3);
					//Serial.print(bank);Serial.print(F("\t"));Serial.println(col);
					int columncolors = Avalue.toInt();
					if (bank<3) {
						DisplayDrivers[bank].SetLedColumn(col,columncolors);
					} else {
						DisplayDrivers[0].SetLedExtra(0,GetRowColor(0,columncolors));
						DisplayDrivers[0].SetLedExtra(1,GetRowColor(1,columncolors));
						DisplayDrivers[1].SetLedExtra(0,GetRowColor(2,columncolors));
						DisplayDrivers[1].SetLedExtra(1,GetRowColor(3,columncolors));
						DisplayDrivers[2].SetLedExtra(1,GetRowColor(4,columncolors));
					}
				}

				pointer++;
				Avalue="0";

				if (pointer > (ParseOLD ? (HighResMode ? 47 : 29) : (HighResMode ? 24 : 12))) FoundPattern=true;
				//DEBUG_OUT("FoundPattern     ",FoundPattern)
			}
			if(ShowOutput(__stream_LED)) Serial.print(F(" "));
			SawSeparator=true;
			break;
		case '0': 
		case '1': 
		case '2': 
		case '3': 
		case '4': 
		case '5': 
		case '6': 
		case '7': 
		case '8': 
		case '9': 
			// add it to the value string:
			if(ShowOutput(__stream_LED)) Serial.print((char)inChar);
			Avalue += (char)inChar;
			SawSeparator=false;
			break;
		case 'A':
			// ************************************** .reset();
			break;
		case 'B': case 'C':
			pointer=0;
			Avalue="0";
			SawSeparator=true;
			break;
		default:
			break;
		}
	}
}

byte LowResColumnToHighRes(byte col) {
	if (HighResMode) {
		return col;
	} else {
		switch (ModeDisplay) {  //This sets the display mode (for lowres), 0 is normal, 2 is compressed and centered. 1 shifted to the left, 3 shifted to the right 
		case 0: //Normalmode display on lowres LEDS
			return col*2;
		case 1: //Display compressed, show left: 12 blanks right
			return col;
		case 2: //Display compressed, show centered: 6 blanks left and right
			return col+6;
		case 3: //Display compressed, show right: 12 blanks left
			return col+12;
		}
	}
}

byte GetColorOLD(byte j,byte BlueByte,byte GreenByte,byte RedByte) {
	return ((BlueByte >> j) & 0b1) | (((GreenByte >> j) & 0b1) << 1) | (((RedByte >> j) & 0b1) << 2); 
}

byte GetRowColor(byte row, uint16_t colors) {
	return (colors>>3*row) & 0b111;
}
byte GetRowColorSwapRedBlue(byte row, uint16_t colors) {
	byte color = (colors>>3*row);
	return (color & 0b010) | ((color & 0b100) >> 2) | ((color & 0b001) << 2) ;
}

void MySetLed(byte row, byte colIn, byte color) {
	uint8_t bank = colIn>>3;
	uint8_t col = colIn - 8 * bank;
	
	if (colIn>24) return;
	
	if (bank==3) {
		bank=row>>1;
		if (row>1) row-=2;
		if (row>1) row=1;
		DisplayDrivers[bank].SetLedExtra(row,color);
	} else {
		DisplayDrivers[bank].SetLed(row,col,color);
	}
}

String ParseString(byte LEDorHEADorRING) {
	boolean FoundPattern = false;
	int inChar;
	String Avalue="";
	while (!FoundPattern) {
		inChar = GetCharSync(LEDorHEADorRING);
		switch (toupper((char)inChar)) {
		case 0:
			break;
		case 13: 
		case 10:
			FoundPattern = true;
			break;			
		default:
			// add it to the value string:
			Avalue += (char)inChar;
			break;
		}
	}

	return Avalue;

}

String Getfilename(byte LEDorHEADorRING, byte ActiveOrSleeping) {
	if (HP_TimeTiming[LEDorHEADorRING].StreamSource==__streamFrom_SD) {
		return Getfilename_bynum(LEDorHEADorRING, ActiveOrSleeping, HP_TimeTiming[LEDorHEADorRING].pointerFile[ActiveOrSleeping]);		
	} else {
		String FileType = (HP_TimeTiming[LEDorHEADorRING].StreamSource==__streamFrom_MQTT_Loop) ? "a" : "w";
		switch (LEDorHEADorRING) {
		case __stream_LED:
			return "/mqtt/L" + FileType + ".txt";
		case __stream_HEAD:
			return "/mqtt/H" + FileType + ".txt";
		case __stream_RING:
			return "/mqtt/R" + FileType + ".txt";
		}

	}
}

String Getfilename_bynum(byte LEDorHEADorRING, byte ActiveOrSleeping, short num) {
	char filename[20];
	short StreamNumber;
	
	StreamNumber = 10*LEDorHEADorRING + ActiveOrSleeping;
		
	switch (StreamNumber) {
	case 0:
		sprintf(filename,"ledsAct/s%03i.txt",num);
		break;
	case 1:
		sprintf(filename,"ledsSlp/s%03i.txt",num);
		break;
	case 10:
		sprintf(filename,"headAct/s%03i.txt",num);
		break;
	case 11:
		sprintf(filename,"headSlp/s%03i.txt",num);
		break;
	case 20:
		sprintf(filename,"ringAct/s%03i.txt",num);
		break;
	case 21:
		sprintf(filename,"ringSlp/s%03i.txt",num);
		break;
	default:
		sprintf(filename,"s%1i_s%1i.txt",LEDorHEADorRING,ActiveOrSleeping);
		break;
	}
	return filename;
}

void FilterESC(char &CharBuf) {
	//if (CharBuf==13) UpdateAnimationMode();
	if ((CharBuf==27) || (CharBuf=='@')) EscHit=true; //==Force till a certain stream when hitting ESC or @, otherwise IGNORE it
}

void ProcessESC(void) {
	byte CharBuf;
	PrintDescN(4);
	while (true) {
		CharBuf=toupper(Serial.read()); //Next character after the esc determines what to do....
		switch (CharBuf) {
		case 10: case 13:
			PrintDescN(4); //PrintDescN(5); //N
			CharBuf=0;
			break;
		case 'L':
			HP_TimeTiming[__stream_LED].StreamSource = __streamFrom_Serial;
			HP_TimeTiming[__stream_HEAD].StreamSource = __streamFrom_SD;
			HP_TimeTiming[__stream_RING].StreamSource = __streamFrom_SD;
			CharBuf=0;
			PrintDescN(36); //N
			ShowFilenames=false;
			return;
		case 'H':
			HP_TimeTiming[__stream_LED].StreamSource = __streamFrom_SD;
			HP_TimeTiming[__stream_HEAD].StreamSource = __streamFrom_Serial;
			HP_TimeTiming[__stream_RING].StreamSource = __streamFrom_SD;
			CharBuf=0;
			PrintDescN(37); //N
			ShowFilenames=false;
			return;
		case 'R':
			HP_TimeTiming[__stream_LED].StreamSource = __streamFrom_SD;
			HP_TimeTiming[__stream_HEAD].StreamSource = __streamFrom_SD;
			HP_TimeTiming[__stream_RING].StreamSource = __streamFrom_Serial;
			CharBuf=0;
			PrintDescN(45); //N
			ShowFilenames=false;
			return;				
		/*
		case 'A':
			InterpretAnimationIntervals(__stream_ForceSerial);
			CharBuf=0; //To prevent from asking the user twice
			return;
			break;
		*/
		case 'T':
			ShowFilenames=!ShowFilenames;
			CharBuf=0;
			PrintDesc(43); PrintDesc((ShowFilenames ? 29 : 16 )); //N
			return;
			break;
		/*	
		case 'P': // Write (to SD)
			WriteFileToSD();
			return;
			break;
		case 'G': // Get (from SD)
			ReadFileFromSD();
			return;
			break;
		*/
		case -1:
			break;
		case 'N':
			//DEBUG_OUT("No more serial input due to..C",(char)CharBuf)
			HP_TimeTiming[__stream_LED].StreamSource = __streamFrom_SD;
			HP_TimeTiming[__stream_HEAD].StreamSource = __streamFrom_SD;
			HP_TimeTiming[__stream_RING].StreamSource = __streamFrom_SD;
			//ForceOut=true;
			CharBuf=0;
			PrintDescN(38); //N
			ShowFilenames=true;
			return;
		}
	}
}

char GetCharSync(byte LEDorHEADorRING) {
	char c=0;
	short CharPos=0;
	
	while (c == 0) {
		c=GetChar(LEDorHEADorRING, CharPos);
		//if (c==0) delay(100);
	}
	return c;
}

char GetCharSerial(void) {
	char c;
	while ((c=(char)Serial.read()) == -1) {}
	return c;
}

byte GetCharColor(byte LEDorHEADorRING, byte DefaultValue, boolean &continueProcessing) {
	//This function returns a color PATTEN, not a real color!
	
	byte inChar = 0;

	if (!continueProcessing) inChar = '-';
	
	while (inChar == 0) {
		inChar = GetCharSync(LEDorHEADorRING);		
	}
	
	switch (toupper(inChar)) {
		case '0': 
		case '1': 
		case '2': 
		case '3': 
		case '4': 
		case '5': 
		case '6': 
		case '7': 
		case 'A': 
		case 'U': 
		case 'G': 
		case 'L': 
		case 'N': 
		case 'R':
			return inChar;
		case 10:
		case 13:
			continueProcessing = false; //NO break!
		default:
			return DefaultValue;
	}
	
	// Should never come here.....
}

byte GetCharPattern(byte LEDorHEADorRING, byte DefaultValue, boolean &continueProcessing) {
	//DefaultValue is a pattern constant, not a character.
	//This function also return a pattern constant.
	
	//If DefaultValue has strobing characteristics, these are removed, to be re-applied in GetCharStrobing

	byte inChar = 0;

	if (!continueProcessing) inChar = '-';
	
	while (inChar == 0) {
		inChar = GetCharSync(LEDorHEADorRING);		
	}
	
	switch (toupper(inChar)) {
		case 'O': 
		case 'B': 
		case 'F': 
			return GetHeadPattern(tolower(inChar)); // Remove any strobing characteristics.
		case 10:
		case 13:
			continueProcessing = false; //NO break!
		default:
			return ((DefaultValue<__STROBE) ? DefaultValue : DefaultValue - __ApplySTROBE); // Remove strobing characteristics.
	}
	
	// Should never come here.....
}

byte GetCharStrobing(byte LEDorHEADorRING, byte InputValue, byte DefaultPattern, boolean &continueProcessing) {
	//InputValue is a pattern constant, not a character.	
	//DefaultPattern is a pattern constant, not a character.
	//This function also return a pattern constant.

	//From DefaultPattern, ONLY its Strobing characteristic is used. Inputvalue allready contains the correct value (without strobing characteristic).

	byte inChar = 0;
	
	if (!continueProcessing) inChar = '-';
	
	while (inChar == 0) {
		inChar = GetCharSync(LEDorHEADorRING);		
	}
	
	switch (toupper(inChar)) {
		case 'S': 
		case '1':
			return (InputValue + __ApplySTROBE);
		case 'N':
		case '0':
			return (InputValue);		
		case 10:
		case 13:
			continueProcessing = false;	//NO break!
		default:
			return ((DefaultPattern<__STROBE) ? InputValue : InputValue + __ApplySTROBE); // The condition means the original pattern was not flashing.
	}
	// Should never come here.....
}

char GetChar(byte LEDorHEADorRING, short &CharPos) {
	char CharBuf;
	boolean  GotSerialCharacter=false;
	
	switch (HP_TimeTiming[LEDorHEADorRING].StreamSource) {
	case __streamFrom_Serial:

		//If it is forced to serial, LEDorHEADorRING are irrelevant. After this if clause LEDorHEADorRINGorFORCED equals LEDorHEADorRING..
		//if ((ShowSerialInput==LEDorHEADorRING) || (LEDorHEADorRING==__stream_ForceSerial)) {
		if (ShowOutput(LEDorHEADorRING)) {
			if (!Serial.available()) return 0; //let the HIGHER level skip this input...
			CharBuf=(char)Serial.read();
			FilterESC(CharBuf);
			return CharBuf;
		}
		break;
	case __streamFrom_SD:
	case __streamFrom_MQTT_SD:
	case __streamFrom_MQTT_Loop:
		if (Serial.available()) {
			CharBuf=(char)Serial.read();//Serial.print(CharBuf);
			FilterESC(CharBuf);
			return 0;
		} else {
			return CharFromSD(LEDorHEADorRING, CharPos);
		}
		break;
	case __streamFrom_MQTT_Wait:
		return 0;
		break;
	}
}

char CharFromSD(byte LEDorHEADorRING, short &CharPos) {
	char CharBuf;
	
	#ifdef Arduino
	#define EOF -1
	#else
	#define EOF 255
	#endif

	switch (HP_TimeTiming[LEDorHEADorRING].StreamSource) {
	case __streamFrom_Serial:			// Should never hit this, we are reading from SD!
		return 0;
		break;
	case __streamFrom_SD:
	case __streamFrom_MQTT_SD:
	case __streamFrom_MQTT_Loop:
		CharBuf=HP_TimeTiming[LEDorHEADorRING].dataFile.read();
		/*
#define __streamFrom_SD 0
#define __streamFrom_MQTT_SD 1
#define __streamFrom_MQTT_Wait 2
#define __streamFrom_MQTT_Loop 3
#define __streamFrom_Serial 4		
		*/
		if (CharBuf == EOF) { // eof, so...close and reopen the next one
			HP_TimeTiming[LEDorHEADorRING].dataFile.close();
			
			// Execute a single MQTT commandfile only once.
			if (HP_TimeTiming[LEDorHEADorRING].StreamSource==__streamFrom_MQTT_SD) {
				HP_TimeTiming[LEDorHEADorRING].StreamSource = __streamFrom_MQTT_Wait;
				return 0;
			}
			
			// If commandfile is a looping file, only loop when there are patterns in it.
			if (HP_TimeTiming[LEDorHEADorRING].StreamSource == __streamFrom_MQTT_Loop) {

				//When the loop has no patterns, do not continue to read from the same file
				if (!HP_TimeTiming[LEDorHEADorRING].PatternOK) {
					HP_TimeTiming[LEDorHEADorRING].StreamSource = __streamFrom_MQTT_Wait;
					return 0;
				}
			}
			break;
		}
		return CharBuf;
		break; 							// Never executed.
	case __streamFrom_MQTT_Wait:
		return 0;
		break;							// Never executed.
	}

	// We only get here, when reading from SD and when EOF is reached.
	#ifdef DEBUG
	Serial.println("EOF 1, increment to next file");
	#endif
	
	if (HP_TimeTiming[LEDorHEADorRING].StreamSource==__streamFrom_MQTT_Loop) {
		OpenSDFile(LEDorHEADorRING,__stream_Active);
	}
	
	// increment pointer, or reloop: read from the file when entering, prepare the next file to read.
	// "normal" processing
	// INVERTMODE FOR LOOKING AT CORRECT FILES, use the XOR function to invert the boolean.
	byte ACTIVEorSLEEPING = (HP_TimeTiming[LEDorHEADorRING].SwapActiveSleeping xor HP_TimeTiming[LEDorHEADorRING].StreamIsActive) ? __stream_Active : __stream_Sleeping;
	if (HP_TimeTiming[LEDorHEADorRING].pointerFile[ACTIVEorSLEEPING] < (HP_TimeTiming[LEDorHEADorRING].numberFiles[ACTIVEorSLEEPING]-1)) {
		HP_TimeTiming[LEDorHEADorRING].pointerFile[ACTIVEorSLEEPING] += 1;
	} else {
		HP_TimeTiming[LEDorHEADorRING].pointerFile[ACTIVEorSLEEPING] = 0;
	}

	OpenSDFile(LEDorHEADorRING, ACTIVEorSLEEPING);
		
	CharBuf=HP_TimeTiming[LEDorHEADorRING].dataFile.read();
	if (CharBuf == EOF) { //STIL closed, set filenum to 0 and retry
		#ifdef DEBUG
		Serial.println("EOF 2, this should NOT happen");
		#endif	
		//stop reading from SD..
		HP_TimeTiming[LEDorHEADorRING].HasFiles[ACTIVEorSLEEPING] = false;
	}
	CharPos = 0;

	//when opening a new file, process these commands
	HP_TimeTiming[LEDorHEADorRING].AnimationIntensity=Constrain(7,HP_TimeTiming[LEDorHEADorRING].MinIntensity,HP_TimeTiming[LEDorHEADorRING].MaxIntensity);
	HP_TimeTiming[LEDorHEADorRING].PatternTime[ACTIVEorSLEEPING]=1000;
	
	switch (LEDorHEADorRING) {
	case __stream_HEAD:
		HP_BlinkTime[ACTIVEorSLEEPING]=500;
		HP_FlashTime[ACTIVEorSLEEPING]=20;
		HP_StrobeTime[ACTIVEorSLEEPING]=10;
		ResetHeadTimers(ACTIVEorSLEEPING);
		SetIntensityHEAD(HP_TimeTiming[__stream_HEAD].AnimationIntensity);
		break;
	case __stream_RING:
		SetIntensityRING(HP_TimeTiming[__stream_RING].AnimationIntensity);
		break;
	case __stream_LED:
		SetIntensityLED(HP_TimeTiming[__stream_LED].AnimationIntensity);
	}
	
	HP_TimeTiming[LEDorHEADorRING].TimeProcessing.interval(HP_TimeTiming[LEDorHEADorRING].PatternTime[ACTIVEorSLEEPING]);
	HP_TimeTiming[LEDorHEADorRING].TimeProcessing.reset();		


	//Set default timing and intensity after changing the file

	//DEBUG_OUT("read2 file Charbuf",CharBuf)
	//Serial.println("EOF DONE, increment to next file");

	return CharBuf;
}

void PrintTab(void) {
	Serial.print(F("\t"));
}

void PrintCR(void) {
	Serial.print(F("\n\r"));
}

void writeMQTTFile(fs::FS &fs, byte Stream, const char * message){
    String path; // "/mqtt/f1.txt"
	
	switch (Stream) {
	case __stream_LED:
		path = "/mqtt/L";
		break;
	case __stream_HEAD:
		path = "/mqtt/H";
		break;
	case __stream_RING:
		path = "/mqtt/R";
		break;
	}
	
	#ifdef DEBUG
	Serial.printf("Writing file: %s\r\n", path);
	#endif
	
	// Writing /mqtt/Xa.txt in append mode and /mqtt/Xw.txt just one line.
	for (String MyMode : {"a","w"}) {	
		File file = fs.open(path + MyMode + ".txt", MyMode.c_str());
		if(!file){
			PublishMQTT(def_mqtt_topic, "/ERROR", "MQTT - failed to open file for writing");
			return;
		}
		if(file.println(message)){
		} else {
			PublishMQTT(def_mqtt_topic, "/ERROR", "MQTT - write file failed");
		}
		file.close();
	}
}
