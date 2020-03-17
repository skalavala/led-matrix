# led-matrix repo

MAX7219 LED Matrix - Digital LED Scrolling Text/Display

The wiring is defined in the config.h file as follows.

#define CLK_PIN                                 14                  // SCK    (D5 PIN)
#define DATA_PIN                                13                  // MOSI   (D7 PIN)
#define CS_PIN                                  16                  // CS     (D0 PIN)

Once the program is compiled and uploaded, you can't change the pin assignment.

ESP8266 Wiring:
================
Connect the CLK to D5
Connect Data to D7
Connect CS to D0
Connect VCC and Ground

Copy the libraries folder into "C:\Users\<<user>>\Documents\Arduino\" 
folder in order for the arduino.cc to pick up the files.

If you have multiple led matrix modules and you connected in serial, 
make sure you change 'MAX_DEVICES' value. 
The default is 4 as there are four 8x8 led blocks in each module.
