Development notes

Soft Access Point (AP) - to select the SSDI and control the IPS

ssid is EleksTubeIPSHack
password is thankyou

Shows a network scan


Station Mode (STA)- to download shows and media to the IPS



ESP32 is access point for OnePlus phone, shows list of networks, connect
ESP32 is STA to download media


To get the form data I used:
https://techtutorialsx.com/2017/12/01/esp32-arduino-asynchronous-http-webserver/


<p><a href="/choosenetwork?ssid=frankone">frankone</a></p>

choosenetwork sends page with a form

<form action="/get">
    Network: frankone<br>
    <input type="hidden" name="ssid" value="frankone">
    <input type="password" name="pass">
    <input type="submit" value="Submit">
</form><br>


Build requires:
https://github.com/me-no-dev/AsyncTCP
https://github.com/me-no-dev/ESPAsyncWebServer


Play images
Play videos

Make precompiled bin available from repository (no compiling needed)
Publish to repository
Announce to world







For later
https://github.com/fhessel/esp32_https_server
Save the network and password into SPIFFS to auto-connect after a reset
