# ElekstubeIPSHack
Hacking the Elekstube IPS ESP32 TFT based clock

[EleksTube IPS](https://www.banggood.com/Pseudo-glow-Tube-Programmable-Display-IPS-Screen-RGB-Clock-Desktop-Creative-Ornaments-Digital-Clock-Colorful-LED-Picture-Display-p-1789259.html?rmmds=myorder&cur_warehouse=CN) is an ESP32-based digital clock. It appears to be the first in a wave of new 32-bit network-enabled multiple display products.

![Photo of the EleksTube IPS sitting on a desktop displaying Nixie Tube images](docs/images/EleksTubeIPS.jpg)

[EleksMaker](https://eleksmaker.com/) designed it to appear to have a [Nixie Tube display](https://en.wikipedia.org/wiki/Nixie_tube). Of course, it's all an ingenious fake! An ESP32 processor drives 6 TFT LCD displays. USB C connectors provide power and communication to a FTDI microcontroller programmer using a pre-programmed bootloader. The environment is familiar to users of Arduino microcontroller boards, C/C++ languages, Arduino IDE, and the ESP32 processor.

The board features 3 push buttons for set-up configuration, a power button, a real-time clock with battery backup, a power control chip to dim the displays, and 6 screens. The screens are IPS 1.14 Inch 8PIN Spi Hd Tft Lcd-screen with ST7789 driver IC. They display 135 by 240 pixels and are supported by the TFT_eSPI library.

All of this feels like an Arduino playground. I love it right out of the box! EleksTube IPS comes pre-programmed with its own firmware. At the time of writing this EleksMaker had not published the source-code to their firmware. The firmware implements its own protocol for a Windows-based application to upload new images for the clock function. [Check here for details](http://wiki.eleksmaker.cn/doku.php?id=ips).

I'd also like to build some new experiences with it:

- Getting the time from a Network Time Protocol (NTP) server on the Internet
- Uploading images and video to the clock from a Web browser
- Configuration of time, Wifi access, start-up animation from a Web browser

I started a discussion [Hacking the EleksTube IPS Clock - anyone tried it?](https://www.reddit.com/r/arduino/comments/mq5td9/hacking_the_elekstube_ips_clock_anyone_tried_it/) on Reddit. [@SmittyHalibut](https://www.reddit.com/user/SmittyHalibut/), [@RedNax67](@https://www.reddit.com/user/RedNax67/), and others worked to build an open-source firmware. @SmittyHalibut's firmware is [here](https://github.com/SmittyHalibut/EleksTubeHAX). It uses the ESP32 SPIFFS filesystem to store images. And it implments class libraries for buttons, backlights, RTC, and displays.

Installation

Builds on Arduino IDE 1.8.13 on MacOS 11.3.1. Choose Tools -> Board -> ESP Arduino (in sketchbook) -> ESP Dev Module. Set Tools -> Upload speed to 115200.

More details on how this works on the way!

Next steps

Implement HTTPS to protect Wifi connection passwords
https://esp32.com/viewtopic.php?t=19452

Let the ESP32 select Wifi connections based on signal strength and availability
https://diyprojects.io/esp32-how-to-connect-local-wifi-network-arduino-code/#.YKxQz5NKiog


License
This work is distributed for free under a GPL version 3 open-source license.

Contributers
[@FrankCohen](https://www.reddit.com/user/frankcohen), [@SmittyHalibut](https://www.reddit.com/user/SmittyHalibut/), [@RedNax67](@https://www.reddit.com/user/RedNax67/)

Please feel free to jump-in here. Make a contribution, make a fork, make a comment, open a bug report. The water is warm and all are welcome.
