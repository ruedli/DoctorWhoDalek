# DoctorWhoDalek

Make a Dalek for the Williams Doctor Who pinball move, like Williams intended to... And more: animate 165 RGB LEDS.

The first series of the Williams dr Who pinball (from 1989) had a motor to move the Dalek in its topper. For budget reason this was removed after a fey models, but.... The firmware STILL supports it.

This project makes the Dalek move again, using a small servo, an ESP8266 D1 mini (Wemos) and this code. You find all information to hook it up in the "hardware" files. The .lm files contain the stripboard PCB's that I designed (using "Lochmaster"). You only need to look into the ESP D1 mini files.

Next I went a step further, and also added 125 RGB LEDS in the foot and 39 RGB leds in the head. The sketch allows you to program animations (stored in directories of the D1 mini's flash drive), played out at 100 frame per second. You can also conveniently program these animations from the Dalex.xslm file. It uses an MQTT interface to directly send patterns from the excel into your Dalek for previewing.

Leds, Ring, Nose/eyes all can be programmed with "standby" patterns, as well as "active" patterns. The Leds in the base animate as and when the head is operated, nos/eyes and the Leds in the ring are animated when the pinball lights up the flasher light in the head (which is gone now). This flasher is pinball solenoid14. the servo is operated by the motor solenois (solenoid25).


