# New ball for Cooper!

![best dog](kerp.jpg "best dog")

Cooper's glow-in-the-dark ball broke. The rubber ball is fine, so I'm making a blinky to go inside: an attiny45 microcontroller driving three LEDs, doing some color effects. One of them spells "Cooper" in morse code.

.

.

The circuit will be as shown below, where "shake switch" is just a loose wire basically. As the ball bounces around, it will open and close pretty randomly. But on the shelf at home, it will stay open or closed as it is. That's how we'll turn the ball on. (The ATTIny is programmed  to do its thing for a few minutes, then turn off the LEDs and enter sleep mode. An input pin voltage change will wake it up again, via pin change interrupt.)

All code is in [cooper.c](cooper.c). See comment at end for compiling and linking with gcc. I uploaded to the ATTiny using avrdude and a USBTiny programmer hooked up as shown:

![schematic](circuit.jpg)
![usbprog](attiny-programming-schematics.jpg)
