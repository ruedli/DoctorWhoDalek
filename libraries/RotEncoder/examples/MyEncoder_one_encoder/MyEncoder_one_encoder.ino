#include <Wire.h>  // Comes with Arduino IDE, used for driving the LCD using 
#include <LiquidCrystal_I2C.h>
#include <RotEncoder.h>

LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address



// Rotational encoder parameters and settings
// Pins
RotEncoder MijnEncoder(5,6 , 4 , 5);
EncoderButton MijnKnop(4,20);

#define _custom_closed	0
#define _custom_held 	1
#define _custom_open 	2

uint8_t custom_closed[8] = {0x00,0x00,0x1C,0x10,0x10,0x1C,0x00,0x00};  		//a character "25"
uint8_t custom_held[8] = {0x00,0x00,0x14,0x1C,0x1C,0x14,0x00,0x00};  		//a character "25"
uint8_t custom_open[8] = {0x00,0x00,0x1C,0x14,0x14,0x1C,0x00,0x00};  		//a character "25"



void setup() {
  Serial.begin(9600);


	lcd.begin(16,2);   	// initialize the lcd for 16 chars 2 lines, turn on backlight
    //displayAccelerationStatus();
	lcd.clear();
	

	lcd.createChar(_custom_closed, custom_closed);
	lcd.createChar(_custom_held, custom_held);
	lcd.createChar(_custom_open, custom_open);

}

void loop() {
	static long EncLast = -1;
	static long EncV = 0;
	static long KerenGedrukt = 0;
	static long KerenDubbelGedrukt = 0;
	static long KerenNeer = 0;
	static long KerenOmhoog = 0;	
	static boolean OpenOnce;	

	MijnEncoder.ReadEnc(EncV);
  
	if (EncV != EncLast) {

		EncLast = EncV;
		
		lcd.setCursor(9,0);
		lcd.print("   ");
		lcd.setCursor(9,0);
		lcd.print(EncV);		
	}

    switch (MijnKnop.ReadButton()) {
	case EncoderButton::Open:
		if (OpenOnce) {
			OpenOnce = false;
			lcd.setCursor(12,0);
			lcd.write(byte(_custom_open));
		}
		break;
	case EncoderButton::Pressed:
		OpenOnce = true;
		KerenNeer++;
		lcd.setCursor(9,0);
		lcd.print("P  ");
		lcd.setCursor(10,0);
		lcd.print(KerenNeer);
		break;
    case EncoderButton::Held:
		lcd.setCursor(12,0);
		lcd.write(byte(_custom_held));
		break;
    case EncoderButton::Closed:
		lcd.setCursor(11,0);
		lcd.setCursor(12,0);
		lcd.write(byte(_custom_closed));
		break;
	case EncoderButton::Released:
		KerenOmhoog++;
		lcd.setCursor(13,0);
		lcd.print("R");
		lcd.print(KerenOmhoog);
		break;
    case EncoderButton::Clicked:
		KerenGedrukt++;
		lcd.setCursor(9,1);
		lcd.print("C");
		lcd.print(KerenGedrukt);
		break;
    case EncoderButton::DoubleClicked:
		KerenDubbelGedrukt++;
		lcd.setCursor(13,1);
		lcd.print("D");
		lcd.print(KerenDubbelGedrukt);
        break;
    }	

	
	
}



