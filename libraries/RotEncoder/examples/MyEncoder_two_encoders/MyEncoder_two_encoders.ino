#include <Wire.h>  // Comes with Arduino IDE, used for driving the LCD using 
#include <LiquidCrystal_I2C.h>
#include <RotEncoder.h>

LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address

// Rotational encoder parameters and settings
// Pins
RotEncoder    MyEncoders[2] = {    RotEncoder(5,6),    RotEncoder(8,9)};
EncoderButton MyButtons[2]  = { EncoderButton(4,400)  , EncoderButton(7,300)  };

// Some custom characters for the LCD (small c,h and o)
#define _custom_closed	0
#define _custom_held 	1
#define _custom_open 	2

uint8_t custom_closed[8] = {0x00,0x00,0x1C,0x10,0x10,0x1C,0x00,0x00};  		
uint8_t custom_held[8] = {0x00,0x00,0x14,0x1C,0x1C,0x14,0x00,0x00}; 
uint8_t custom_open[8] = {0x00,0x00,0x1C,0x14,0x14,0x1C,0x00,0x00};  		


void setup() {
  Serial.begin(9600);
	lcd.begin(16,2);   	// initialize the lcd for 16 chars 2 lines, turn on backlight
	lcd.clear();

	lcd.createChar(_custom_closed, custom_closed);
	lcd.createChar(_custom_held, custom_held);
	lcd.createChar(_custom_open, custom_open);
}

void loop() {
	static long EncLast[2] = {-1, -1};
	static long EncV[2] = {0, 0};
	static long KerenGedrukt[2] = {0, 0};
	static long KerenDubbelGedrukt[2] = {0, 0};
	static long KerenNeer[2] = {0, 0};
	static long KerenOmhoog[2] = {0, 0};	
	static boolean OpenOnce[2] = {false, false};

	byte Position = 0;
	
	for (byte i=0; (i<=1); i++) {
		MyEncoders[i].ReadEnc(EncV[i]);

	  
		if (EncV[i] != EncLast[i]) {
			EncLast[i] = EncV[i];
			lcd.setCursor(Position,0);
			lcd.print("   ");
			lcd.setCursor(Position,0);
			lcd.print(EncV[i]);		
		}

		switch (MyButtons[i].ReadButton()) {
		case EncoderButton::Open:
			if (OpenOnce[i]) {
				OpenOnce[i] = false;
				lcd.setCursor(Position+3,0);
				lcd.write(byte(_custom_open));
			}
			break;
		case EncoderButton::Pressed:
			OpenOnce[i] = true;
			KerenNeer[i]++;
			lcd.setCursor(Position,0);
			lcd.print("P  ");
			lcd.setCursor(Position+1,0);
			lcd.print(KerenNeer[i]);
			break;
		case EncoderButton::Held:
			lcd.setCursor(Position+3,0);
			lcd.write(byte(_custom_held));
			break;
		case EncoderButton::Closed:
			lcd.setCursor(Position+3,0);
			lcd.write(byte(_custom_closed));
			break;
		case EncoderButton::Released:
			KerenOmhoog[i]++;
			lcd.setCursor(Position+4,0);
			lcd.print("R");
			lcd.print(KerenOmhoog[i]);
			break;
		case EncoderButton::Clicked:
			KerenGedrukt[i]++;
			lcd.setCursor(Position,1);
			lcd.print("C");
			lcd.print(KerenGedrukt[i]);
			break;
		case EncoderButton::DoubleClicked:
			KerenDubbelGedrukt[i]++;
			lcd.setCursor(Position+4,1);
			lcd.print("D");
			lcd.print(KerenDubbelGedrukt[i]);
			break;
		}
		Position +=9;
	}
}



