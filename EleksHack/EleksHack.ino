/*
 * Alterantive firmware for the EleksTubeIPS digital clock 
 * 
 * Instructions to build this sketch are found at
 * https://github.com/frankcohen/EleksTubeIPSHack/blob/main/README.md
 * 
 * Licensed under GPL v3
 * (c) Frank Cohen, All rights reserved. fcohen@votsh.com
 * Read the license in the license.txt file that comes with this code.
 * May 24, 2021
 * 
 */

#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#define FS_NO_GLOBALS
#include <FS.h>
#include "Hardware.h"
#include "ChipSelect.h"
#include "TFTs.h"
#include "time.h"
#include "Clock.h"

// Don't use SPIFFS, it's deprecated, use LittleFS instead
//https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html#spiffs-deprecation-warning
// From Arduino IDE Library Manager install LittleFS_esp32
#include <LITTLEFS.h> 

/* SSID & Password for the EleksTube to be a Web server */
const char* ssid = "EleksHack";  // Enter SSID here
const char* password = "thankyou";  //Enter Password here

IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

WebServer server(80);

const char* ntpServer = "pool.ntp.org";
long  gmtOffset_sec = 3600;
int   daylightOffset_sec = 3600;

File fsUploadFile;

#define SCREENWIDTH 135
#define SCREENHEIGHT 240

#include <TFT_eSPI.h> // Hardware-specific library
TFTs tfts;    // Display module driver

Clock uclock;
StoredConfig stored_config;

boolean playImages = true;
boolean playVideos = false;
boolean playClock = false;

#define BGCOLOR    0xAD75
#define GRIDCOLOR  0xA815
#define BGSHADOW   0x5285
#define GRIDSHADOW 0x600C
#define RED        0xF800
#define WHITE      0xFFFF

File root;

void updateClockDisplay(TFTs::show_t show) {
  tfts.setDigit(HOURS_TENS, uclock.getHoursTens(), show);
  tfts.setDigit(HOURS_ONES, uclock.getHoursOnes(), show);
  tfts.setDigit(MINUTES_TENS, uclock.getMinutesTens(), show);
  tfts.setDigit(MINUTES_ONES, uclock.getMinutesOnes(), show);
  tfts.setDigit(SECONDS_TENS, uclock.getSecondsTens(), show);
  tfts.setDigit(SECONDS_ONES, uclock.getSecondsOnes(), show);

  Serial.print( "time: " );
  Serial.print( uclock.getHoursTens() );
  Serial.print( uclock.getHoursOnes() );
  Serial.print( ":" );
  Serial.print( uclock.getMinutesTens() );
  Serial.print( uclock.getMinutesOnes() );
  Serial.print( ":" );
  Serial.print( uclock.getSecondsTens() );
  Serial.println( uclock.getSecondsOnes() );  
}

// Borrowed from https://github.com/zenmanenergy/ESP8266-Arduino-Examples/blob/master/helloWorld_urlencoded/urlencode.ino

String urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
      }
      yield();
    }
    return encodedString;   
}

unsigned char h2int(char c)
{
    if (c >= '0' && c <='9'){
        return((unsigned char)c - '0');
    }
    if (c >= 'a' && c <='f'){
        return((unsigned char)c - 'a' + 10);
    }
    if (c >= 'A' && c <='F'){
        return((unsigned char)c - 'A' + 10);
    }
    return(0);
}

String urldecode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    for (int i =0; i < str.length(); i++){
        c=str.charAt(i);
      if (c == '+'){
        encodedString+=' ';  
      }else if (c == '%') {
        i++;
        code0=str.charAt(i);
        i++;
        code1=str.charAt(i);
        c = (h2int(code0) << 4) | h2int(code1);
        encodedString+=c;
      } else{
        
        encodedString+=c;  
      }
      
      yield();
    }
    
   return encodedString;
}

static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do 
  {
    // feel free to do something here
  } while (millis() - start < ms);
}

void setup() {
  Serial.begin(115200);
  smartDelay(2000);

  Serial.println();
  Serial.println( "EleksTube IPS Alternative Firmware" );
  Serial.println();

  Serial.print( "Starting Wifi Access Point, connect to: " );
  Serial.print( ssid );
  Serial.print( ", using password: " );
  Serial.println( password );
  Serial.print( "Upon connection point your browser to:" );
  Serial.print( "192,168,1,1" );
  Serial.print( " to view the main menu." );
  Serial.println( "The gateway is at 192,168,1,1 and subnet mask is 255,255,255,0" );
  
  randomSeed(analogRead(0));

  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  if(!LITTLEFS.begin()){
      Serial.println("LITTLEFS/SPIFFS begin failed");
  }
  
  tfts.begin();
  tfts.fillScreen(TFT_BLACK);
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  tfts.setCursor(0, 0, 2);
  tfts.println("Eleks Hack");
  tfts.setTextColor(TFT_BLUE, TFT_BLACK );
  tfts.println("Starting as server");
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  tfts.println("Connect using WIFI");
  tfts.println("SSID: EleksHack");
  tfts.println("Password: thankyou");

  smartDelay(500);

  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.disconnect();

  smartDelay(500);

  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet);

  smartDelay(1000);

  server.onNotFound(handle_NotFound);
  server.on("/", HTTP_GET, handle_menu);
  server.on("/wifi", HTTP_GET, handle_connectwifi);
  server.on("/getpassword", HTTP_GET, handle_getpassword);
  server.on("/connect", HTTP_GET, handle_connect);
  server.on("/dir", HTTP_GET, handle_manage);
  server.on("/images", HTTP_GET, handle_images);
  server.on("/movies", HTTP_GET, handle_movies);
  server.on("/manage", HTTP_GET, handle_manage);
  server.on("/clock", HTTP_GET, handle_clock);
  server.on("/clockupdate", HTTP_POST, handle_clockupdate);
  server.on("/favicon.ico", HTTP_GET, handle_favicon);
  server.on("/upload", HTTP_POST, [](){ server.send(200, "text/plain", "");}, handle_upload);
  server.on("/uploadform", HTTP_GET, handle_uploadform);
  server.on("/success", HTTP_GET, handle_success);
  server.on("/delete", HTTP_GET, handle_delete);
  server.on("/format", HTTP_GET, handle_format);

  server.begin();
  Serial.println("Ready");
  tfts.setTextColor(TFT_BLUE, TFT_BLACK );
  tfts.println("Ready");
  
  tfts.beginJpg();
  
  Serial.println( "setup() done" );
}

void handle_favicon()
{
  server.send(404, "text/html", "No favicon here" );   
}

void handle_NotFound()
{
  Serial.println("handle_NotFound()");
  
  Serial.print("server.uri()=");
  Serial.println( server.uri() );
  
  String resp = "";
  resp +="<html><head><style>html { font-family: Bitter; color:blue; display: inline-block; margin: 0px auto; text-align: left; font-size:40px; background-color:powderblue;}</style>";
  resp +="<title>EleksHack Controller</title>\n";
  resp +="</head><body>";
  resp +="<h1>EleksHack Control</h1><br><br>";
  resp += "No handler for that URL";
  resp += server.uri();
  resp +="</body>\n";
  resp +="</html>\n";
  
  server.send(200, "text/html", resp ); 
}

void handle_format(){
  Serial.println("handle_connectwifi()");

  String resp = "";
  resp +="<html><head><style>html { font-family: Bitter; color:blue; display: inline-block; margin: 0px auto; text-align: left; font-size:40px; background-color:powderblue;}</style>";
  resp +="<title>EleksHack Controller</title>\n";
  resp +="</head><body>";
  resp +="<h1>Formatted SPIFFS file system</h1><br><br>";

  LITTLEFS.format();

  resp +="<p>Done</p>";

  resp +="</body>\n";
  resp +="</html>\n";
}


void handle_connectwifi() {
  Serial.println("handle_connectwifi()");

  String resp = "";
  resp +="<html><head><style>html { font-family: Bitter; color:blue; display: inline-block; margin: 0px auto; text-align: left; font-size:40px; background-color:powderblue;}</style>";
  resp +="<title>EleksHack Controller</title>\n";
  resp +="</head><body>";
  resp +="<h1>EleksHack Control</h1><br><br>";

  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) 
  {
    resp += "No networks found";
  } 
  else 
  {
    resp += "<p>Choose a network:</p>";
    for (int i = 0; i < n; ++i) 
    {
      resp += "<p><a href=\"/getpassword?ssid=";
      resp += urlencode( WiFi.SSID(i) );
      resp += "\">";
      resp += WiFi.SSID(i);
      resp += "</a></p>"; 
    }
  }
  resp +="</body>\n";
  resp +="</html>\n";

  Serial.println( resp );
  
  server.send(200, "text/html", resp); 
  
}

// Send form to sign-in

void handle_getpassword(){
  Serial.println("handle_choosenetwork()");

  String resp = "";
  resp +="<html><head><style>html { font-family: Bitter; color:blue; display: inline-block; margin: 0px auto; text-align: left; font-size:40px; background-color:powderblue;}</style>";
  resp +="<title>EleksHack Controller</title>\n";
  resp +="</head><body>";
  resp +="<h1>EleksHack Control</h1><br><br>";

  resp += "<form action='/connect'>";
  resp += "Network: " + server.arg("ssid") + "<br>";
  resp += "<input type=\"hidden\" name=\"ssid\" value=\"" + server.arg("ssid") + "\">";
  resp += "Password: <input name=\"pass\">";
  resp += "<input type=\"submit\" value=\"Submit\">";
  resp += "</form><br>";

  resp += "<p>NOTE: This form transmits passwords in <b>clear text</b>. There is no security. ";
  resp += "We recommend you use a Wifi station with no password. Or, do not use this control.</p>";

  resp +="</body>\n";
  resp +="</html>\n";  
  Serial.println( resp );
  server.send(200, "text/html", resp); 
}

// Clicked the sign-in form

void handle_connect(){
  Serial.println("handle_connect()");

  String resp = "";
  resp +="<html><head><style>html { font-family: Bitter; color:blue; display: inline-block; margin: 0px auto; text-align: left; font-size:40px; background-color:powderblue;}</style>";
  resp +="<title>EleksHack Controller</title>\n";
  resp +="</head><body>";
  resp +="<h1>EleksHack Control</h1><br><br>";

  unsigned int sslen = server.arg("ssid").length() + 1;
  char myssid[ sslen ];
  server.arg("ssid").toCharArray( myssid, sslen );

  unsigned int pwlen = server.arg("pass").length() + 1;
  char myspass[ pwlen ];  
  server.arg("pass").toCharArray( myspass, pwlen );

  Serial.println( "Connected to Wifi network: " );
  Serial.print( "ssid = " );
  Serial.print( myssid );
  Serial.print( " password = " );
  Serial.println( myspass );

  WiFi.begin( myssid, myspass );

  int timeout = millis();  
  while ( ( WiFi.status() != WL_CONNECTED ) && ( timeout + 5000 > millis() ) ) 
  {
    Serial.println("Connecting");
    smartDelay(1000);
  }

  if ( WiFi.status() == WL_CONNECTED )
  {
    resp += "Connected<br>";
  }
  else
  {
    resp += "Connection attempt timed out after 5 seconds.</br>";
  }

  if ( WiFi.status() == WL_NO_SSID_AVAIL )
  {
    resp += "No networks available<br>";
  }

  if ( WiFi.status() == WL_CONNECT_FAILED )
  {
    resp += "Connection failed<br>";
  }

  if ( WiFi.status() == WL_CONNECTION_LOST )
  {
    resp += "Connection lost<br>";
  }

  if ( WiFi.status() == WL_DISCONNECTED )
  {
    resp += "Disconnected from a network<br>";
  }

  resp +="</body>\n";
  resp +="</html>\n";  

  server.send(200, "text/html", resp); 
  Serial.println( resp );
}

void handle_menu()
{
  Serial.println( "Handle Menu" );

  String resp = "";
  resp +="<html><head><style>html { font-family: Bitter; color:blue; display: inline-block; margin: 0px auto; text-align: left; font-size:40px; background-color:powderblue;}</style>";
  resp +="<title>EleksHack Controller</title>\n";
  resp +="</head><body>";
  resp +="<h1>EleksHack Control</h1><br><br>";

  resp +="<p><a href=\"images\">Play images</a></p>";

  resp +="<p><a href=\"clock\">Play clock</a></p>";

  // Not yet, still neeed to figure out TFT_eSPI's DMA mode for MPEGs
  // resp +="<p><a href=\"/movies\">Play movies</a></p>";
  
  resp +="<p><a href=\"/wifi\">Connect to WiFi</a></p>";
  resp +="<p><a href=\"/manage\">Manage media</a></p>";

  resp +="<br><br><br><a href=\"https://github.com/SmittyHalibut/EleksTubeHAX\">A happy hacking project.</a>";

  resp +="</body>\n";
  resp +="</html>\n";  
  server.send(200, "text/html", resp); 
}

void handle_manage()
{
  String resp = "";
  resp +="<html><head><style>html { font-family: Bitter; color:blue; display: inline-block; margin: 0px auto; text-align: left; font-size:40px; background-color:powderblue;}</style>";
  resp +="<title>EleksHack Controller</title>\n";
  resp +="</head><body>";
  resp +="<h1>EleksHack Control</h1><br><br>";

  resp +="<p><a href=\"/uploadform\">Upload</a></p><br>";
  resp +="<p><a href=\"/format\">Format SPIFFs file system</a></p><br>";

  resp +="<br><br><p><a href=\"/\">Menu</a></p>";

  resp +="<br><br>totalBytes=";
  resp +=LITTLEFS.totalBytes();
  resp +=", usedBytes=";
  resp +=LITTLEFS.usedBytes();
  resp +=", freeBytes=";
  resp +=LITTLEFS.totalBytes() - LITTLEFS.usedBytes();
  resp +="<br><br>";

  String path = "/";
  File root = LITTLEFS.open(path);
  path = String();

  if(root.isDirectory()){
      File file = root.openNextFile();
      while(file){

        resp +="<p>";
        resp += (file.isDirectory()) ? "dir: " : "file: ";
        resp +=" ";
        resp += file.name();
        resp +=", ";
        resp += file.size();
        resp +=", <a href=\"/delete?file=";        
        resp += urlencode( file.name() );
        resp +="\">delete</a><\p>";
        file = root.openNextFile();
      }
  }

  resp +="<br><br><p><a href=\"/uploadform\">Upload</a></p>";

  resp +="<br><br><p><a href=\"/\">Menu</a></p>";

  resp +="</body>\n";
  resp +="</html>\n";  
  server.send(200, "text/html", resp); 
}

void handle_images()
{
  if ( server.hasArg("mode") )
  {
    if ( server.arg("mode") == "play" ) 
    {  
      playImages = true;
      playVideos = false;
      playClock = false;
    }
    if ( server.arg("mode") == "stop" )
    {
      playImages = false;
    }
  }
  
  String resp = "";
  resp +="<html><head><style>html { font-family: Bitter; color:blue; display: inline-block; margin: 0px auto; text-align: left; font-size:40px; background-color:powderblue;}</style>";
  resp +="<title>EleksHack Controller</title>\n";
  resp +="</head><body>";
  resp +="<h1>EleksHack Control</h1><br><br>";

  if ( playImages )
  {
    resp +="<p>Playing</p><br><br>";
  }
  else
  {
    resp +="<p>Stopped</p><br><br>";
  }

  resp += "<p><a href=\"/images?mode=play\">Play images</a></p>";
  resp += "<p><a href=\"/images?mode=stop\">Stop images</a></p>";

  resp +="<br><br><p><a href=\"/\">Menu</a></p>";

  resp +="</body>";
  resp +="</html>";  
  server.send(200, "text/html", resp); 
}

void handle_clock()
{  
  if ( server.hasArg("mode") )
  {
    if ( server.arg("mode") == "play" ) 
    {
      playClock = true;
      playImages = false;
      playVideos = false;
    }
    if ( server.arg("mode") == "stop" ) playClock = false;
  }

  uclock.begin(&stored_config.config.uclock);
  updateClockDisplay(TFTs::force);
  Serial.print("Saving config.");
  stored_config.save();
  Serial.println(" Done.");
    
  String resp = "";
  resp +="<html><head><style>html { font-family: Bitter; color:blue; display: inline-block; margin: 0px auto; text-align: left; font-size:40px; background-color:powderblue;}</style>";
  resp +="<title>EleksHack Controller</title>\n";
  resp +="</head><body>";
  resp +="<h1>EleksHack Control</h1><br><br>";

  if ( playClock )
  {
    resp +="<p>Playing</p><br><br>";
  }
  else
  {
    resp +="<p>Stopped</p><br><br>";
  }

  resp += "<p><a href=\"/clock?mode=play\">Play clock</a></p>";
  resp += "<p><a href=\"/clock?mode=stop\">Stop clock</a></p>";

  resp += "<br><br><p>Clock settings</p>";

  resp += "<form action=\"/clockupdate\" method=\"post\">";
  resp += "Timezone: <input size=\"10\" name=\"timezone\" value=\"-7\"><br>";
  resp += "Daylight savings adjustment: <input size=\"10\" name=\"daylight\" value=\"0" + server.arg("ssid") + "\">";
  resp += "<br>";
  resp += "<input type=\"submit\" value=\"Get NTP Time\">";
  resp += "</form><br>";

  resp += "<br><br>Notes:<br>";
  resp += "<p>UTC offset for your timezone in milliseconds. Refer the <a href=\"https://en.wikipedia.org/wiki/List_of_UTC_time_offsets\">list of UTC time offsets. For example, Pacific time is -7</a>.</p>";

  resp += "<p>If your country observes Daylight saving time set it to 3600. Otherwise, set it to 0.</p>";

  resp +="<br><br><p><a href=\"/\">Menu</a></p>";

  resp +="</body>";
  resp +="</html>";  
  server.send(200, "text/html", resp); 
}

void handle_clockupdate()
{ 
  String resp = "";
  resp +="<html><head><style>html { font-family: Bitter; color:blue; display: inline-block; margin: 0px auto; text-align: left; font-size:40px; background-color:powderblue;}</style>";
  resp +="<title>EleksHack Controller</title>\n";
  resp +="</head><body>";
  resp +="<h1>EleksHack Control</h1><br><br>";

  gmtOffset_sec = server.arg("timezone").toInt() * 60 * 60;
  Serial.print( "gmtOffset_sec = " );
  Serial.println( gmtOffset_sec );
  daylightOffset_sec = server.arg("daylight").toInt();
  Serial.print( "daylightOffset_sec = " );
  Serial.println( daylightOffset_sec );

  struct tm timeinfo;
  getLocalTime(&timeinfo);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  uclock.forceNTPUpdate();
  uclock.setTimeZoneOffset( gmtOffset_sec );
  uclock.adjustTimeZoneOffset( daylightOffset_sec );

  Serial.println( "TimeZone and Daylight offsets set" );
  resp +="<br><p>TimeZone and Daylight offsets set.</p>";

  resp +="<br><br><p><a href=\"/\">Menu</a></p>";

  resp +="</body>\n";
  resp +="</html>\n";  
  server.send(200, "text/html", resp); 
}

void handle_movies()
{
  if ( server.hasArg("mode") )
  {
    if ( server.arg("mode") == "play" ) playVideos = true;
    if ( server.arg("mode") == "stop" ) playVideos = false;
  }
  
  String resp = "";
  resp +="<html><head><style>html { font-family: Bitter; color:blue; display: inline-block; margin: 0px auto; text-align: left; font-size:40px; background-color:powderblue;}</style>";
  resp +="<title>EleksHack Controller</title>\n";
  resp +="</head><body>";
  resp +="<h1>EleksHack Control</h1><br><br>";

  if ( playVideos )
  {
    resp +="<p>Playing</p><br><br>";
  }
  else
  {
    resp +="<p>Stopped</p><br><br>";
  }

  resp += "<p><a href=\"/videos?mode=play\">Play videos</a></p>";
  resp += "<p><a href=\"/videos?mode=stop\">Stop videos</a></p>";

  resp +="<br><br><p><a href=\"/\">Menu</a></p>";

  resp +="</body>\n";
  resp +="</html>\n";  
  server.send(200, "text/html", resp); 
}

void handle_uploadform()
{
  String resp = "";
  resp +="<html><head><style>html { font-family: Bitter; color:blue; display: inline-block; margin: 0px auto; text-align: left; font-size:40px; background-color:powderblue;}</style>";
  resp +="<title>EleksHack Controller</title>\n";
  resp +="</head><body>";
  resp +="<h1>EleksHack Control</h1><br><br>";

  resp +="<p>Upload an image or movie</p>";
  
  resp +="<form action=\"upload\" method=\"post\" enctype=\"multipart/form-data\">";
  resp +="<input type=\"file\" name=\"name\">";
  resp +="<input class=\"button\" type=\"submit\" value=\"Upload\">";
  resp +="</form>";

  resp +="<br><br>totalBytes=";
  resp +=LITTLEFS.totalBytes();
  resp +=", usedBytes=";
  resp +=LITTLEFS.usedBytes();
  resp +=", freeBytes=";
  resp +=LITTLEFS.totalBytes() - LITTLEFS.usedBytes();
  
  resp +="<br><br><p><a href=\"/\">Menu</a></p>";

  resp +="</body>\n";
  resp +="</html>\n";  
  server.send(200, "text/html", resp); 
}

// Borrowed from https://tttapa.github.io/ESP8266/Chap12%20-%20Uploading%20to%20Server.html

String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void handle_upload()
{  
  HTTPUpload& upload = server.upload();

  if( upload.status == UPLOAD_FILE_START )
  {
    Serial.println( "UPLOAD_FILE_START" );
    Serial.print( "LITTLEFS totalBytes=" );
    Serial.print( LITTLEFS.totalBytes() );
    Serial.print( ", usedBytes=" );
    Serial.print( LITTLEFS.usedBytes() );
    Serial.print( ", free bytes=" );
    Serial.println( LITTLEFS.totalBytes() - LITTLEFS.usedBytes() );
    
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    
    Serial.print("handle_upload Name: "); 
    Serial.println(filename);
    
    fsUploadFile = LITTLEFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    filename = String();
  } 
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    Serial.println( "UPLOAD_FILE_WRITE" );
    
    // Write the received bytes to the file
    int stat = 0;
    if(fsUploadFile) { stat = fsUploadFile.write(upload.buf, upload.currentSize); };

    Serial.print( "wrote " );
    Serial.print( upload.currentSize );
    Serial.print( " bytes, stat = " );
    Serial.println( stat );
  } 
  else if (upload.status == UPLOAD_FILE_END)
  {
    Serial.println( "UPLOAD_FILE_END" );

    Serial.print( "upload.totalSize = " );
    Serial.println( upload.totalSize );

    if(fsUploadFile) 
    {                                    // If the file was successfully created
      fsUploadFile.close();
      server.sendHeader("Location","/success");      // Redirect the client to the success page
      server.send(303);
    }
    else 
    {
      server.send(500, "text/plain", "Upload failed");
    }
  }
}

void handle_success()
{
  String resp = "";
  resp +="<html><head><style>html { font-family: Bitter; color:blue; display: inline-block; margin: 0px auto; text-align: left; font-size:40px; background-color:powderblue;}</style>";
  resp +="<title>EleksHack Controller</title>\n";
  resp +="</head><body>";
  resp +="<h1>EleksHack Control</h1><br><br>";

  resp +="<p>Success</p><br><br>";

  resp +="<form action=\"upload\" method=\"post\" enctype=\"multipart/form-data\">";
  resp +="<input type=\"file\" name=\"name\">";
  resp +="<input class=\"button\" type=\"submit\" value=\"Upload\">";
  resp +="</form>";
  
  resp +="<br><br><p><a href=\"/\">Menu</a></p>";

  resp +="</body>\n";
  resp +="</html>\n";  
  server.send(200, "text/html", resp); 
}

void handle_delete()
{
  String resp = "";
  resp +="<html><head><style>html { font-family: Bitter; color:blue; display: inline-block; margin: 0px auto; text-align: left; font-size:40px; background-color:powderblue;}</style>";
  resp +="<title>EleksHack Controller</title>\n";
  resp +="</head><body>";
  resp +="<h1>EleksHack Control</h1><br><br>";

  resp +="<p>Deleting file ";
  resp += urldecode( server.arg("file") );
  resp +="</p>";

  if ( LITTLEFS.remove( urldecode( server.arg("file") ) ) )
  {
    resp +="<p>File deleted</p><br><br>";
    Serial.println("File deleted");
  } 
  else 
  {
    resp +="<p>Delete failed</p><br><br>";
    Serial.println("Delete failed");
  }
  
  resp +="<br><br><p><a href=\"/\">Menu</a></p>";

  resp +="</body>\n";
  resp +="</html>\n";  
  server.send(200, "text/html", resp); 
}

void loop() {
  server.handleClient();

  if ( playImages )
  {
    tfts.showNextJpg();
    smartDelay(2000);
  }

  if ( playVideos )
  {
    
  }

  if ( playClock )
  {
    uclock.loop();
    updateClockDisplay(TFTs::yes);  
    smartDelay(250);
  }

}
