/*
 * Alterantive firmware for the EleksTubeIPS digital clock 
 * 
 * Instructions to build this sketch are found at
 * https://github.com/frankcohen/EleksTubeIPSHack/blob/main/README.md
 * 
 * Licensed under GPL v3
 * (c) Frank Cohen, All rights reserved. fcohen@votsh.com
 * Read the license in the license.txt file that comes with this code.
 * May 30, 2021
 * 
 */

#include <WiFi.h>
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include <HTTPBodyParser.hpp>
#include <HTTPMultipartBodyParser.hpp>
#include <HTTPURLEncodedBodyParser.hpp>

#define FS_NO_GLOBALS
#include <FS.h>
#include "Hardware.h"
#include "ChipSelect.h"
#include "TFTs.h"
#include "time.h"
#include "Clock.h"

// The HTTPS Server comes in a separate namespace. For easier use, include it here.
using namespace httpsserver;

// Don't use SPIFFS, it's deprecated, use LittleFS instead
//https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html#spiffs-deprecation-warning
// From Arduino IDE Library Manager install LittleFS_esp32
#include <LITTLEFS.h> 

/* SSID & Password for the EleksTube to be a Web server */
const char* WIFI_SSID = "EleksHack";  // Enter SSID here
const char* WIFI_PSK = "thankyou";  //Enter Password here

IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

SSLCert * cert;
HTTPSServer * secureServer;

// Declare handler functions for the various URLs on the server
void handleRoot(HTTPRequest * req, HTTPResponse * res);
void handle404(HTTPRequest * req, HTTPResponse * res);
void handle_menu(HTTPRequest * req, HTTPResponse * res);
void handle_showslice(HTTPRequest * req, HTTPResponse * res);
void handle_connectwifi(HTTPRequest * req, HTTPResponse * res);
void handle_getpassword(HTTPRequest * req, HTTPResponse * res);
void handle_connect(HTTPRequest * req, HTTPResponse * res);
void handle_manage(HTTPRequest * req, HTTPResponse * res);
void handle_images(HTTPRequest * req, HTTPResponse * res);
void handle_movies(HTTPRequest * req, HTTPResponse * res);
void handle_manage(HTTPRequest * req, HTTPResponse * res);
void handle_clock(HTTPRequest * req, HTTPResponse * res);
void handle_clockupdate(HTTPRequest * req, HTTPResponse * res);
void handle_favicon(HTTPRequest * req, HTTPResponse * res);
void handle_upload(HTTPRequest * req, HTTPResponse * res);
void handle_uploadform(HTTPRequest * req, HTTPResponse * res);
void handle_success(HTTPRequest * req, HTTPResponse * res);
void handle_delete(HTTPRequest * req, HTTPResponse * res);
void handle_photobooth(HTTPRequest * req, HTTPResponse * res);
void handle_image(HTTPRequest * req, HTTPResponse * res);

void addResHeader(HTTPResponse * res);
void addResFooter(HTTPResponse * res);

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

boolean playImages = false;
boolean playVideos = false;
boolean playClock = false;

#define BGCOLOR    0xAD75
#define GRIDCOLOR  0xA815
#define BGSHADOW   0x5285
#define GRIDSHADOW 0x600C
#define RED        0xF800
#define WHITE      0xFFFF

#include "mbedtls/base64.h"

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
  smartDelay(500);

  Serial.println();
  Serial.println( "EleksTube IPS Alternative Firmware" );
  Serial.println();

  Serial.print( "Starting Wifi Access Point, connect to: " );
  Serial.print( WIFI_SSID );
  Serial.print( ", using password: " );
  Serial.println( WIFI_PSK );
  Serial.print( "Upon connection point your browser to:" );
  Serial.print( "192.168.1.1" );
  Serial.print( " to view the main menu." );
  Serial.println( "The gateway is at 192.168.1.1 and subnet mask is 255.255.255.0" );
  
  randomSeed(analogRead(0));

  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  char rx_byte = 0;

  if(!LITTLEFS.begin()){
      Serial.println("LITTLEFS/SPIFFS begin failed");
      Serial.println("Type Y and click Submit to format the SPIFFS");

      while (1)
      {
        while (Serial.available() > 0)
        {
          char rx_byte = Serial.read();
          if (rx_byte == 'Y')
          {
            Serial.println("Formatting..." );
            LITTLEFS.format();
            Serial.println("Formatting complete. Power cycle your clock.");
           }
        }
      }
  }
  
  tfts.begin();
  tfts.fillScreen(TFT_BLACK);
  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  tfts.setCursor(0, 0, 2);
  tfts.println("Eleks Hack");
  tfts.setTextColor(TFT_BLUE, TFT_BLACK );
  tfts.println("Creating self-signed");
  tfts.println("certificate");

  Serial.println("Creating a new self-signed certificate.");
  Serial.println("This may take up to a minute");

  // First, we create an empty certificate:
  cert = new SSLCert();

  // Now, we use the function createSelfSignedCert to create private key and certificate.
  // The function takes the following paramters:
  // - Key size: 1024 or 2048 bit should be fine here, 4096 on the ESP might be "paranoid mode"
  //   (in generel: shorter key = faster but less secure)
  // - Distinguished name: The name of the host as used in certificates.
  //   If you want to run your own DNS, the part after CN (Common Name) should match the DNS
  //   entry pointing to your ESP32. You can try to insert an IP there, but that's not really good style.
  // - Dates for certificate validity (optional, default is 2019-2029, both included)
  //   Format is YYYYMMDDhhmmss
  int createCertResult = createSelfSignedCert(
    *cert,
    KEYSIZE_2048,
    "CN=myesp32.local,O=FancyCompany,C=DE",
    "20190101000000",
    "20300101000000"
  );

  if (createCertResult != 0) {
    Serial.printf("Create certificate failed. Error Code = 0x%02X, check SSLCert.hpp for details", createCertResult);
    tfts.setTextColor(TFT_BLUE, TFT_BLACK );
    tfts.println("Certificate failed.");
    while(true) delay(500);
  }
  Serial.println("Created the certificate successfully");
  Serial.println("Connect using WIFI");
  Serial.println("SSID: EleksHack");
  Serial.println("Password: thankyou");
  Serial.println("then browse");
  Serial.println("https://92.168.1.1");
  Serial.println("for a menu");
  Serial.println("of commands");

  tfts.setTextColor(TFT_WHITE, TFT_BLACK);
  tfts.println("Connect using WIFI");
  tfts.println("SSID: EleksHack");
  tfts.println("Password: thankyou");
  tfts.println("then browse");
  tfts.println("https://92.168.1.1");
  tfts.println("for a menu");
  tfts.println("of commands");

  // TODO: Store certificate and the key in StoredConfig. This has the advantage that the certificate stays the same after a reboot
  // so your client still trusts your server, additionally you increase the speed-up of your application.
  // Some browsers like Firefox might even reject the second run for the same issuer name (the distinguished name defined above).
  //
  // Storing:
  //   For the key:
  //     cert->getPKLength() will return the length of the private key in bytes
  //     cert->getPKData() will return the actual private key (in DER-format, if that matters to you)
  //   For the certificate:
  //     cert->getCertLength() and ->getCertData() do the same for the actual certificate data.
  // Restoring:
  //   When your applications boots, check your non-volatile storage for an existing certificate, and if you find one
  //   use the parameterized SSLCert constructor to re-create the certificate and pass it to the HTTPSServer.
  //
  // A short reminder on key security: If you're working on something professional, be aware that the storage of the ESP32 is
  // not encrypted in any way. This means that if you just write it to the flash storage, it is easy to extract it if someone
  // gets a hand on your hardware. You should decide if that's a relevant risk for you and apply countermeasures like flash
  // encryption if neccessary

  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.disconnect();

  smartDelay(500);

  WiFi.softAP(WIFI_SSID, WIFI_PSK);
  WiFi.softAPConfig(local_ip, gateway, subnet);

  smartDelay(1000);

  // Use the new certificate to setup our server
  secureServer = new HTTPSServer(cert);

  // For every resource available on the server, we need to create a ResourceNode
  // The ResourceNode links URL and HTTP method to a handler function
  ResourceNode * nodeRoot    = new ResourceNode("/", "GET", &handle_menu);
  ResourceNode * nodeShowslice    = new ResourceNode("/showslice", "GET", &handle_showslice);
  ResourceNode * node404     = new ResourceNode("", "GET", &handle404);
  ResourceNode * nodeWifi    = new ResourceNode("/wifi", "GET", &handle_connectwifi);
  ResourceNode * nodePassword    = new ResourceNode("/getpassword", "GET", &handle_getpassword);
  ResourceNode * nodeConnect    = new ResourceNode("/connect", "GET", &handle_connect);
  ResourceNode * nodeDir    = new ResourceNode("/dir", "GET", &handle_manage);
  ResourceNode * nodeImages    = new ResourceNode("/images", "GET", &handle_images);
  ResourceNode * nodeMovies    = new ResourceNode("/movies", "GET", &handle_movies);
  ResourceNode * nodeManage    = new ResourceNode("/manage", "GET", &handle_manage);
  ResourceNode * nodeClock    = new ResourceNode("/clock", "GET", &handle_clock);
  ResourceNode * nodeClockUpdate    = new ResourceNode("/clockupdate", "POST", &handle_clockupdate);
  ResourceNode * nodeFavicon    = new ResourceNode("/favicon.ico", "GET", &handle_favicon);
  ResourceNode * nodeUpload    = new ResourceNode("/upload", "POST", &handle_upload);
  ResourceNode * nodeUploadform    = new ResourceNode("/uploadform", "GET", &handle_uploadform);
  ResourceNode * nodeSuccess    = new ResourceNode("/success", "GET", &handle_success);
  ResourceNode * nodeDelete    = new ResourceNode("/delete", "GET", &handle_delete);
  ResourceNode * nodePhotobooth    = new ResourceNode("/photobooth", "GET", &handle_photobooth);
  ResourceNode * nodeImage    = new ResourceNode("/image", "GET", &handle_image);

  // Add the root node to the server
  secureServer->registerNode(nodeRoot);
  secureServer->registerNode(nodeShowslice);
  secureServer->registerNode(node404);
  secureServer->registerNode(nodeWifi);
  secureServer->registerNode(nodePassword);
  secureServer->registerNode(nodeConnect);
  secureServer->registerNode(nodeDir);
  secureServer->registerNode(nodeImages);
  secureServer->registerNode(nodeMovies);
  secureServer->registerNode(nodeManage);
  secureServer->registerNode(nodeClock);
  secureServer->registerNode(nodeClockUpdate);
  secureServer->registerNode(nodeFavicon);
  secureServer->registerNode(nodeUpload);
  secureServer->registerNode(nodeUploadform);
  secureServer->registerNode(nodeSuccess);
  secureServer->registerNode(nodeDelete);
  secureServer->registerNode(nodePhotobooth);
  secureServer->registerNode(nodeImage);

  Serial.println("Starting server...");
  secureServer->start();
  Serial.println("Ready");
  tfts.setTextColor(TFT_BLUE, TFT_BLACK );
  tfts.println("Ready");
  
  tfts.beginJpg();
  
  uclock.begin(&stored_config.config.uclock);
  updateClockDisplay(TFTs::force);

  Serial.println( "setup() done" );
}

void handle_showslice(HTTPRequest * req, HTTPResponse * res)
{
  Serial.println( "Showslice" );
  
  addResHeader(res);
  res->println("Send slice to display");  
  addResFooter(res); 

  std::string filename;
  req->getParams()->getQueryParameter("filename", filename);
  char myfilename[ filename.length()+1 ];
  String mes = filename.c_str();
  mes.toCharArray( myfilename, mes.length()+1 );

  std::string display;
  req->getParams()->getQueryParameter("display", display);
  String mydnum = display.c_str();
  int displaynum = mydnum.toInt();

  tfts.showSlice( myfilename, displaynum 
  );
  
  res->setHeader("Location", "/success");
  res->setStatusCode(303);

  Serial.println( "Showslice done." );
}

void handle_favicon(HTTPRequest * req, HTTPResponse * res)
{
  res->setStatusCode(404);
  res->setStatusText("Not Found");

  res->setHeader("Content-Type", "text/html");
  res->setStatusCode(404);
  addResHeader(res);
  res->println("No favicon available");
  addResFooter(res); 
}

void addResHeader(HTTPResponse * res) 
{
  res->setHeader("Content-Type", "text/html");
  res->setStatusCode(200);

  res->println("<html><head><style>html { font-family: Bitter; color:blue; display: inline-block; margin: 0px auto; text-align: left; font-size:40px; background-color:powderblue;}</style>");
  res->println("<title>EleksHack Controller</title>\n");
  res->println("</head><body>");
}

void addResFooter(HTTPResponse * res) 
{
  res->println("</body>");
  res->println("</html>");  
}

void handle_NotFound(HTTPRequest * req, HTTPResponse * res) {
  Serial.println("handle_NotFound()");

  addResHeader(res);
  res->println("No handler for that URL");  
  addResFooter(res); 
}

void handle404(HTTPRequest * req, HTTPResponse * res) {
  // Discard request body, if we received any
  // We do this, as this is the default node and may also server POST/PUT requests
  req->discardRequestBody();

  // Set the response status
  res->setStatusCode(404);
  res->setStatusText("Not Found");

  // Set content type of the response
  res->setHeader("Content-Type", "text/html");

  // Write a tiny HTTP page
  res->println("<!DOCTYPE html>");
  res->println("<html>");
  res->println("<head><title>Not Found</title></head>");
  res->println("<body><h1>404 Not Found</h1><p>The requested resource was not found on this server.</p></body>");
  res->println("</html>");
}

void dumpTheOpenedFile( File file )
{
  const byte bytesPerRow = 16; // 4 as minimum

  char hexVal[9];
  byte buffer[bytesPerRow];

  Serial.print(F("File: "));
  Serial.println(file.name());
  Serial.println();

  // Writting header
  Serial.print(F("Offset    "));
  Serial.print(F("Hexadecimal ")); //added )

  // Pad with spaces depending on the amount of bytes per row (it should align with the text below)
  for (unsigned int spaces = (bytesPerRow - 4) * 3; spaces; spaces--)
  {
    Serial.write(' '); //put inside a {}
  }

  Serial.println(F(" ASCII"));


  file.seek(0);
  // Typing file's content
  while (file.available())
  {

    // Print offset
    sprintf(hexVal, "%08lX", file.position());

    Serial.print(hexVal);
    Serial.print(" "); // to leave a space between offset and first hex character
    byte amount = file.read(buffer, bytesPerRow);

    // Print hex values
    for (byte i = 0; i < amount; i++)
    {
      sprintf(hexVal, "%02X ", buffer[i]);
      Serial.print(hexVal);
    }

    // Fill with spaces in case we couldn't fill an entire row (due to reaching the end of the file)
    for (unsigned int spaces = (bytesPerRow - amount) * 3; spaces; spaces--)
      Serial.write(' ');

    // Print ASCII values
    for (byte i = 0; i < amount; i++)
    {
      // Printable characters appear as they are, non-printable appear as a dot/period (.)
      Serial.write(buffer[i] > 31 && buffer[i] != 127 ? buffer[i] : '.');
    }

    Serial.println(); // Next line
  } // End of while loop
  file.close();
  Serial.println("                  **File closed**");
} //dumpTheOpenedFile



void handle_image(HTTPRequest * req, HTTPResponse * res) {
   Serial.println("handle_image()");

   std::string filename;
   req->getParams()->getQueryParameter("filename", filename);
   char myfilename[ filename.length()+1 ];
   String mes = filename.c_str();
   mes.toCharArray( myfilename, mes.length()+1 );

   res->setHeader("Content-Type", "image/jpeg");
   res->setStatusCode(200);

   File myFile = LITTLEFS.open("/" + mes );
   if (myFile) 
   { 
      dumpTheOpenedFile( myFile );

/*
      // read from the file until there's nothing else in it:
      while (myFile.available()) 
      {
        char myc = (char) myFile.read();

        res->print( myc );
        
        byte myb = (byte) myc;
      
        Serial.print( myc );
        Serial.print( "=" );
        Serial.print( myb, HEX );
        Serial.print( ", " );
      }

*/
      // close the file:              
      myFile.close();
   }
   else
   {
      Serial.print( "File not found, " );
      Serial.print( "/" + mes );
   }
}

void handle_photobooth(HTTPRequest * req, HTTPResponse * res) {
   Serial.println("handle_photobooth()");

   res->setHeader("Content-Type", "text/html");
   res->setStatusCode(200);

   File myFile = LITTLEFS.open("/photobooth.html");
   if (myFile) 
   { 
      // read from the file until there's nothing else in it:
      while (myFile.available()) 
      {
        res->print( (char) myFile.read() );
      }
      // close the file:              
      myFile.close();      
   }
   else
   {
      addResHeader(res);
      res->println("<h1>EleksHack Control</h1><br><br>");
      res->println("photobooth.html not found in SPIFFS<br>");
      res->println("Use Manage media, then Upload <a href=\"https://github.com/frankcohen/EleksTubeIPSHack/tree/main/EleksHack\">photobooth.html</a> from the source code<br>");
      addResFooter(res);    
   }
}

void handle_format(HTTPRequest * req, HTTPResponse * res){
  Serial.println("handle_connectwifi()");

  addResHeader(res);
  res->println("<h1>Formatted SPIFFS file system</h1><br><br>");

  LITTLEFS.format();

  res->println("<p>Format complete</p>" );

  addResFooter(res);
}

void handle_connectwifi(HTTPRequest * req, HTTPResponse * res) {
  Serial.println("handle_connectwifi()");

  addResHeader(res);
  res->println("<h1>EleksHack Control</h1><br><br>");

  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) 
  {
    res->println("No networks found");
  } 
  else 
  {
    res->println("<p>Choose a network:</p>");
    for (int i = 0; i < n; ++i) 
    {
      res->println("<p><a href=\"/getpassword?ssid=" );
      res->println( urlencode( WiFi.SSID(i) ) );
      res->println( "\">" );
      res->println( WiFi.SSID(i) );
      res->println( "</a></p>" );
    }
  }
   
  addResFooter(res); 
}

// Send form to sign-in to the Wifi network

void handle_getpassword(HTTPRequest * req, HTTPResponse * res){
  Serial.println("handle_choosenetwork()");

  addResHeader(res);
  res->println("<h1>EleksHack Control</h1><br><br>" );

  std::string ssid;
  req->getParams()->getQueryParameter("ssid", ssid);
  
  res->println( "<form action='/connect'>" );
  res->println( "Network: " );
  res->print( ssid.c_str() );
  res->println( "<br>" );
  res->println( "<input type=\"hidden\" name=\"ssid\" value=\"" );
  res->print( ssid.c_str() );
  res->println( "\">" );
  res->println( "Password: <input name=\"pass\">" );
  res->println( "<input type=\"submit\" value=\"Submit\">" );
  res->println( "</form><br>" );

  res->println( "<p>NOTE: This form transmits passwords over self-signed SSL certificates. They do not provide all of the security properties that certificates signed by a CA aim to provide." );
  res->println( "We recommend you use a Wifi station with no password. Or, do not use this control.</p>" );

  addResFooter(res); 
}

// Clicked the sign-in form

void handle_connect(HTTPRequest * req, HTTPResponse * res){
  Serial.println("handle_connect()");

  addResHeader(res);
  res->println("<h1>EleksHack Control</h1><br><br>" );

  std::string ssid;
  req->getParams()->getQueryParameter("ssid", ssid);
  char myssid[ ssid.length()+1 ];
  String mes = ssid.c_str();
  mes.toCharArray( myssid, mes.length()+1 );

  std::string pass;
  req->getParams()->getQueryParameter("pass", pass);
  char myspass[ pass.length()+1 ];
  String me = pass.c_str();
  me.toCharArray( myspass, me.length()+1 );

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
    res->println( "Connected<br>" );
  }
  else
  {
    res->println( "Connection attempt timed out after 5 seconds.</br>"  );
  }

  if ( WiFi.status() == WL_NO_SSID_AVAIL )
  {
    res->println( "No networks available<br>" );
  }

  if ( WiFi.status() == WL_CONNECT_FAILED )
  {
    res->println( "Connection failed<br>" );
  }

  if ( WiFi.status() == WL_CONNECTION_LOST )
  {
    res->println( "Connection lost<br>" );
  }

  if ( WiFi.status() == WL_DISCONNECTED )
  {
    res->println( "Disconnected from a network<br>" );
  }

  addResFooter(res); 
}

void handle_menu(HTTPRequest * req, HTTPResponse * res)
{
  Serial.println( "Handle Menu" );

  addResHeader(res);
  res->println("<h1>EleksHack Control</h1><br><br>" );

  res->println("<p><a href=\"clock\">Play clock</a></p>" );

  res->println("<p><a href=\"images\">Play images</a></p>" );

  res->println("<p><a href=\"photobooth\">Photobooth, captures and plays sliced images</a></p>" );
  
  // Not yet, still neeed to figure out TFT_eSPI's DMA mode for MPEGs
  // probably not going to happen. EleksTube IPS has less than 4 Mbytes of memory.
  // instead, see the ReflectionsOS project. It has Gbytes of memory.
  // https://github.com/frankcohen/ReflectionsOS
  // res->println("<p><a href=\"/movies\">Play movies</a></p>" );
  
  res->println("<p><a href=\"/wifi\">Connect to WiFi</a></p>" );
  res->println("<p><a href=\"/manage\">Manage media</a></p>" );

  res->println("<br><br><br><a href=\"https://github.com/frankcohen/EleksTubeIPSHack\">A happy hacking project.</a>" );

  addResFooter(res); 
}

void handle_manage(HTTPRequest * req, HTTPResponse * res)
{
  Serial.println( "Handle Manage" );

  addResHeader(res);
  res->println("<h1>EleksHack Control</h1><br><br>" );

  res->println("<p><a href=\"/uploadform\">Upload</a></p><br>" );
  res->println("<p><a href=\"/format\">Format SPIFFs file system to LITTLEFS</a></p><br>" );

  res->println("<br><br><p><a href=\"/\">Menu</a></p>" );

  res->println("<br><br>totalBytes=" );
  res->println(LITTLEFS.totalBytes() );
  res->println(", usedBytes=" );
  res->println(LITTLEFS.usedBytes() );
  res->println(", freeBytes=" );
  res->println(LITTLEFS.totalBytes() - LITTLEFS.usedBytes() );
  res->println("<br><br>" );

  String path = "/";
  File root = LITTLEFS.open(path);
  path = String();

  if(root.isDirectory()){
      File file = root.openNextFile();
      while(file){

        res->println("<p>" );
        res->println( (file.isDirectory()) ? "dir: " : "file: " );
        res->println(" " );
        res->println( file.name() );
        res->println(", " );
        res->println( file.size() );
        res->println(", <a href=\"/delete?file=" );
        res->println( urlencode( file.name() ) );
        res->println("\">delete</a><\p>" );
        file = root.openNextFile();
      }
  }

  res->println("<br><br><p><a href=\"/uploadform\">Upload</a></p>" );

  res->println("<br><br><p><a href=\"/\">Menu</a></p>" );

  addResFooter(res); 
}

void handle_images(HTTPRequest * req, HTTPResponse * res)
{
  std::string mode;  
  bool hasMode = req->getParams()->getQueryParameter("mode", mode);
  if ( hasMode )
  {
    if ( mode == "play" ) 
    {  
      playImages = true;
      playVideos = false;
      playClock = false;
    }
    if ( mode == "stop" )
    {
      playImages = false;
    }
  }
  
  addResHeader(res);
  res->println("<h1>EleksHack Control</h1><br><br>" );

  if ( playImages )
  {
    res->println("<p>Playing</p><br><br>" );
  }
  else
  {
    res->println("<p>Stopped</p><br><br>" );
  }

  res->println( "<p><a href=\"/images?mode=play\">Play images</a></p>" );
  res->println( "<p><a href=\"/images?mode=stop\">Stop images</a></p>" );

  res->println("<br><br><p><a href=\"/\">Menu</a></p>" );

  addResFooter(res); 
}

void handle_clock(HTTPRequest * req, HTTPResponse * res)
{  
  std::string mode;
  bool hasMode = req->getParams()->getQueryParameter("mode", mode);

  if ( hasMode )
  {
    if ( mode == "play" ) 
    {
      playClock = true;
      playImages = false;
      playVideos = false;
    }
    if ( mode == "stop" ) playClock = false;
  }

  uclock.begin(&stored_config.config.uclock);
  updateClockDisplay(TFTs::force);
  Serial.print("Saving config.");
  stored_config.save();
  Serial.println(" Done.");
    
  addResHeader(res);
  res->println("<h1>EleksHack Control</h1><br><br>" );

  if ( playClock )
  {
    res->println("<p>Playing</p><br><br>" );
  }
  else
  {
    res->println("<p>Stopped</p><br><br>" );
  }

  res->println( "<p><a href=\"/clock?mode=play\">Play clock</a></p>" );
  res->println( "<p><a href=\"/clock?mode=stop\">Stop clock</a></p>" );

  res->println( "<br><br><p>Clock settings</p>" );

  res->println( "<form action=\"/clockupdate\" method=\"post\">" );
  res->println( "Timezone: <input size=\"10\" name=\"timezone\" value=\"-7\"><br>" );
  res->println( "Daylight savings adjustment: <input size=\"10\" name=\"daylight\" value=\"0\">" );
  res->println( "<br>" );
  res->println( "<input type=\"submit\" value=\"Get NTP Time\">" );
  res->println( "</form><br>" );

  res->println( "<br><br>Notes:<br>" );
  res->println( "<p>UTC offset for your timezone in milliseconds. Refer the <a href=\"https://en.wikipedia.org/wiki/List_of_UTC_time_offsets\">list of UTC time offsets. For example, Pacific time is -7</a>.</p>" );

  res->println( "<p>If your country observes Daylight saving time set it to 3600. Otherwise, set it to 0.</p>" );

  res->println("<br><br><p><a href=\"/\">Menu</a></p>" );

  addResFooter(res); 
}

void handle_clockupdate(HTTPRequest * req, HTTPResponse * res)
{ 
  addResHeader(res);
  res->println("<h1>EleksHack Control</h1><br><br>" );

  std::string timezone;
  bool hasTimezone = req->getParams()->getQueryParameter("timezone", timezone);
  String mytime = timezone.c_str();
  std::string daylight;
  bool hasDaylight = req->getParams()->getQueryParameter("daylight", daylight);
  String myday = daylight.c_str();
  
  gmtOffset_sec = mytime.toInt() * 60 * 60;
  Serial.print( "gmtOffset_sec = " );
  Serial.println( gmtOffset_sec );
  daylightOffset_sec = myday.toInt();
  Serial.print( "daylightOffset_sec = " );
  Serial.println( daylightOffset_sec );

  struct tm timeinfo;
  getLocalTime(&timeinfo);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  uclock.forceNTPUpdate();
  uclock.setTimeZoneOffset( gmtOffset_sec );
  uclock.adjustTimeZoneOffset( daylightOffset_sec );

  Serial.println( "TimeZone and Daylight offsets set" );
  res->println("<br><p>TimeZone and Daylight offsets set.</p>" );

  res->println("<br><br><p><a href=\"/\">Menu</a></p>" );

  addResFooter(res); 
}

void handle_movies(HTTPRequest * req, HTTPResponse * res)
{
  // Not yet :-(
}

void handle_uploadform(HTTPRequest * req, HTTPResponse * res)
{
  addResHeader(res);
  res->println("<h1>EleksHack Control</h1><br><br>" );

  res->println("<p>Upload an image or movie</p>" );
  
  res->println("<form action=\"upload\" method=\"post\" enctype=\"multipart/form-data\">"  );
  res->println("<input type=\"file\" name=\"name\">"  );
  res->println("<input class=\"button\" type=\"submit\" value=\"Upload\">"  );
  res->println("</form>"  );

  res->println("<br><br>totalBytes=" );
  res->println(LITTLEFS.totalBytes() );
  res->println(", usedBytes=" );
  res->println(LITTLEFS.usedBytes() );
  res->println(", freeBytes=" );
  res->println(LITTLEFS.totalBytes() - LITTLEFS.usedBytes() );
  
  res->println("<br><br><p><a href=\"/\">Menu</a></p>" );

  addResFooter(res); 
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

File file;

void handle_upload(HTTPRequest * req, HTTPResponse * res)
{ 
  Serial.println("Handle_upload");

  HTTPBodyParser *parser;
  parser = new HTTPMultipartBodyParser(req);
  bool didwrite = false;

  bool decode64 = false;
  std::string decval = req->getHeader( "Decode64" );
  String decstr = decval.c_str();
  if ( decstr == "true" )
  {
    decode64 = true;
    Serial.println( "Decode64 = true" );
  }
  
  while(parser->nextField()) {

    std::string name = parser->getFieldName();
    std::string filename = parser->getFieldFilename();
    std::string mimeType = parser->getFieldMimeType();
    Serial.printf("handleFormUpload: field name='%s', filename='%s', mimetype='%s'\n", name.c_str(), filename.c_str(), mimeType.c_str() );
    
    if ( ! (filename.rfind("/", 0) == 0) )
    {
      filename = "/" + filename;
    }
    
    Serial.print("handle_upload Name: "); 
    Serial.println(filename.c_str()  );
    
    fsUploadFile = LITTLEFS.open( filename.c_str(), "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)

    size_t fileLength = 0;
    didwrite = true;
    
    while (!parser->endOfField()) {
      byte buf[512];
      size_t readLength = parser->read(buf, 512);

      if ( decode64 && ( readLength>0 ) )
      {
        size_t outlen;
        unsigned char output[ 512 ];
        
        mbedtls_base64_decode(output, 512, &outlen, buf, readLength);

        fsUploadFile.write(output, outlen);
        fileLength += outlen;
      }
      else
      {
        fsUploadFile.write(buf, readLength);
        fileLength += readLength;
      }

    }
    
    fsUploadFile.close();
    res->printf("<p>Saved %d bytes to %s</p>", (int)fileLength, filename.c_str() );
  }

  if (!didwrite) {
    res->println("<p>Did not write any file contents</p>");
  }
  
  delete parser;
   
  Serial.print( "LITTLEFS totalBytes=" );
  Serial.print( LITTLEFS.totalBytes() );
  Serial.print( ", usedBytes=" );
  Serial.print( LITTLEFS.usedBytes() );
  Serial.print( ", free bytes=" );
  Serial.println( LITTLEFS.totalBytes() - LITTLEFS.usedBytes() );
    
  if(didwrite) 
  {                                    // If the file was successfully created
    res->setHeader("Location", "/success");
    res->setStatusCode(303);
  }
  else 
  {
    res->setStatusCode(500);
    res->setStatusText("Upload failed");
  }
}

void handle_success(HTTPRequest * req, HTTPResponse * res)
{
  Serial.println( "Handle_success" );

  addResHeader(res);
  res->println("<h1>EleksHack Control</h1><br><br>" );

  res->println("<p>Success</p><br><br>" );

  res->println("<form action=\"upload\" method=\"post\" enctype=\"multipart/form-data\">"  );
  res->println("<input type=\"file\" name=\"name\">"  );
  res->println("<input class=\"button\" type=\"submit\" value=\"Upload\">"  );
  res->println("</form>"  );
  
  res->println("<br><br><p><a href=\"/\">Menu</a></p>"  );

  addResFooter(res); 
}

void handle_delete(HTTPRequest * req, HTTPResponse * res)
{
  Serial.println( "Handle_delete");

  addResHeader(res);
  res->println("<h1>EleksHack Control</h1><br><br>" );

  std::string file;
  req->getParams()->getQueryParameter("file", file);

  res->println("<p>Deleting file " );
  res->println( urldecode( file.c_str() ) );
  res->println("</p>" );

  if ( LITTLEFS.remove( urldecode( file.c_str() ) ) )
  {
    res->println("<p>File deleted</p><br><br>" );
    Serial.println("File deleted");
  } 
  else 
  {
    res->println("<p>Delete failed</p><br><br>" );
    Serial.println("Delete failed");
  }
  
  res->println("<br><br><p><a href=\"/\">Menu</a></p>" );

  addResFooter(res); 
}

long timeForMore = millis();
long timeForMoreSlice = millis();

void loop() {
  secureServer->loop();

  if ( playImages )
  {
    if ( millis() > timeForMore + 2500 )
    {
      timeForMore = millis();
      tfts.showNextJpg();
    }
  }

  if ( playVideos )
  {
    
  }

  if ( playClock )
  {
    if ( millis() > timeForMore + 200 )
    {
      timeForMore = millis();
      uclock.loop();
      updateClockDisplay(TFTs::yes);  
    }
  }

}
