# zyncoder library

[Zynthian](http://zynthian.org) is an Open Synth Platform based in Raspberry Pi, Linux (Raspbian) and Free Software Synthesizers (mostly).

The [ZynthianOS SD-image](https://os.zynthian.org/zynthianos-last-stable.zip) includes all the software you need for building a ZynthianBox, including a good amount of sound libraries and presets. 

This repository contains the zyncoder library. A library for managing zynthian's input/output devices. It supports  switch buttons, rotary encoders (PEC11), infinite potentiometers (RV12), analog inputs (ADS1115) and outputs (MCP2748), TOF sensors, etc. 

For compiling the library is required the next packages:

* liblo (liblo.so)

For compiling for real use (not emulation), is required:

* wiringPi (libwiringPi.so)

For building execute:
```
$ ./build.sh
```

You can learn more about the Zynthian Project in any of our sites: 

+ [website](https://zynthian.org)
+ [wiki](https://wiki.zynthian.org)
+ [blog](https://blog.zynthian.org)
+ [forum](https://discourse.zynthian.org) => Join the conversation!!

You can buy official kits in the zynthian shop:

+ [shop](https://shop.zynthian.org)
