# DoctorWhoDalek

Make a Dalek for the Williams Doctor Who pinball move, like Williams intended to... And more: animate 165 RGB LEDS.

See it move here: https://www.youtube.com/watch?v=ovQI1cAGlCM

The first series of the Williams dr Who pinball (from 1989) had a motor to move the Dalek in its topper. For budget reason this was removed after a fey models, but.... The firmware STILL supports it.

This project makes the Dalek move again, using a small servo, an ESP8266 D1 mini (Wemos) and this code. You find all information to hook it up in the "hardware" files. The .lm files contain the stripboard PCB's that I designed (using "Lochmaster"). You only need to look into the ESP D1 mini .LM file.

Next I went a step further, and also added 125 RGB LEDS (25 colums of 5 LEDS) in the foot and 39 RGB leds in the head. The sketch allows you to program animations (stored in directories of the D1 mini's flash drive), played out at 100 frames per second. You can also conveniently program these animations from the Dalex.xslm (Excel) file. It uses an MQTT interface to directly send patterns from the excel into your Dalek for previewing. You can also set speed and movementtypes through MQTT and the Excel file.

![image](https://user-images.githubusercontent.com/5008440/164726425-9bf18d93-d381-49f8-bf57-d78eacf48afd.png)

Leds, Ring, Nose/eyes all can be programmed with "standby" patterns, as well as "active" patterns. The Leds in the base animate as and when the head is operated, nose/eyes and the Leds in the ring are animated when the pinball lights up the flasher light in the head (which is gone now). This flasher is pinball solenoid14. the servo is operated by the motor solenoid (solenoid25).

Here you see it interact with the test screen of the pinball machine itself and wiggle the head and animate the LEDs. In reality the LEDS look much better!
https://www.youtube.com/watch?v=p3V4AYly9XI

In essence the LEDS form 4 simple matrices, all interface with one Holtek HT16K33 led driver chip. Each led driver can operate 128 LEDS, one RGB needs 3 LEDS, so I needed 4 drivers for my 164 RGB Leds. The flashmemory of the D1 mini contains the text files that have the LED patterns to animate. It supports running text as well! The wiring of the 4 LED drivers looks intimidating, but is not complex, just a lot of wires! ![image](https://user-images.githubusercontent.com/5008440/164727341-96ae8ff1-6484-4c18-be27-35e26721301a.png)

All these 164 LEDs are animated with only 4 wires using standard I2C so,: +5V, GND, SCL and SDA. So the head needs these 4 wires, as well as 3 wires for a micro servo. These go to the ESP8266 D1 mini (wemos). I also put 3 step down convertors on that board for 5V for the ESP, 6V for the servo and 5.5V for the LEDS.

The interface between pinball and ESP D1 mini has 6 wires: 12V, GND, row1 (J208-1), column8 (J206-9), solenoid25 (J122-1) and solenoid14 (J128-2).




