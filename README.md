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



Rough docs:

I needed two things to happen: deliver a Web interface so we wouldn't have to use the physical buttons on the clock, and let the clock connect to a NTP server over the Internet to get the time. ESP32 supports both, at the same time!

Upon start-up the clock is a Wifi access point with the SSID of EleksHack and the password thankyou. Point your browser to 192.168.1.1 to view the main menu. On your recommendation I added these details to the serial monitor output as it starts. And it prints the connected Wifi addresses to the serial monitor when the station mode connects. ESP32 has a cool feature to be both access point and station concurrently!

If you did nothing more at this point the clock will run on the value stored in the Real Time Clock chip. To see the clock, click the Play Clock link in the menu in your browser.

To get the time from the NTP server, create a Wifi connection from the clock to your nearby Wifi access point. Click Connect To Wifi from the menu. Your browser shows a scan of available Wifi networks. Click on one, enter a password, and connect. Once connected the NTP server time updates the clock time and the time stored in the RTC.

NOTE: This form transmits passwords in clear text. There is no security. I recommend you use a Wifi station with no password. Or, do not use this control. There are plenty of ways to secure the password transmission from snooping, you're welcome to implement one.

Also, there is a bug in my code when using the StoredConfig library. So the unit does not save the passwords, Wifi connections, or play mode between hard resets. I would be grateful for you to fix that.

The clock has 2 shows: Play Clock, Play Images. The clock show is the normal EleksTube IPS clock display. The image show displays a random JPG formatted image on a random display (of the 6 displays).

Play Clock requires the digits of the clock to be stored in BMP format. For example, /1.bmp is the image file for the number 1. Use the Menu command Manage Media and the Upload function to store a BMP formatted file to the internal (SPIFFS) file system. Manage Media also shows the files in the file system, and the delete link removes the file. Nixie tube looking BMP files are in the /data directory of the source repository.

EleksTube IPS ships with SPIFFS formatted using the old and deprecated SPIFFS library. My code uses the replacement for SPIFFS library... called LittleFS. In the Manage Media page use the Format SPIFFs File System before uploading images.

Play Images is something I personally wanted. I use it to randomly show pictures of my children. Both my children are in love and life is good! Click Play Images from the main menu, then click Play. It picks a JPG image from the file system approximately every 2 seconds.

Lastly, I haven't seen a time-out when using the Web UX. Would you please let me know more. Glad to help fix whatever I broke.



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
