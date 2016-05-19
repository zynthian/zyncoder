# zyncoder library

[![Join the chat at https://gitter.im/zynthian/zyncoder](https://badges.gitter.im/zynthian/zyncoder.svg)](https://gitter.im/zynthian/zyncoder?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
Rotary encoder &amp; switch library for RBPi GPIO (wiringPi). 

This library implements an interface for rotary encoders and switches connected to the RBPi GPIO.

It allows to read values from rotary encoders and switches and, optionaly, send it to MIDI/OSC controllers directly.
It can be used to create Embeded User Interfaces based in this kind of elements.

Also, implements an emulation layer that ease development and testing in desktop and laptop computers.
This emulation layer uses POSIX signals (SIGRTMIN-SIGRTMAX) as inputs for the virtual GPIO.

For compiling the library is required the next packages:

* alsalib (libasound.so)
* liblo (liblo.so)

Also, for compiling for real use (not emulation), is required:

* wiringPi (libwiringPi.so)

For building execute:
```
$ mkdir build
$ cd build
$ cmake ..
$ make
```
