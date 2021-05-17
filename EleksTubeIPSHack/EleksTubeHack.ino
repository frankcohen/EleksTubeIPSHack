#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <SPIFFS.h>

/* Put your SSID & Password */
const char* ssid = "EleksTubeIPSHack";  // Enter SSID here
const char* password = "thankyou";  //Enter Password here

IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

WebServer server(80);

#define FILESYSTEM SPIFFS

// Chip Select shift register
const uint8_t latchPin = 17;
const uint8_t clockPin = 16;
const uint8_t dataPin = 14;

static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do 
  {
    // feel free to do something here
  } while (millis() - start < ms);
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

void setup() {
  Serial.begin(115200);
  smartDelay(1000);

  Serial.println("");
  Serial.println("EleksTubeHack starting");

  FILESYSTEM.begin();
  
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  // Setup 74HC595 chip select. Enabled only the left display.
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  digitalWrite(latchPin, LOW);
  digitalWrite(dataPin, LOW);
  digitalWrite(clockPin, LOW);
  shiftOut(dataPin, clockPin, LSBFIRST, 0x00);  // 0x1F = Left most, 0x3E = Right most
  digitalWrite(latchPin, HIGH);

  smartDelay(500);

  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.disconnect();

  smartDelay(500);

  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet);

  smartDelay(1000);

  server.onNotFound(handle_NotFound);
  server.on("/", HTTP_GET, handle_OnConnect);
  server.on("/getpassword", HTTP_GET, handle_getpassword);
  server.on("/connect", HTTP_GET, handle_connect);
  server.on("/dir", HTTP_GET,   handle_FileList);
      
  server.begin();
  Serial.println("Setup complete");
}

void handle_NotFound()
{
  Serial.println("handle_NotFound()");
  
  String resp = "";
  resp +="<html><head>";
  resp +="<title>EleksTubeIPSHack Controller</title>\n";
  resp +="</head><body>";
  resp +="<h1>EleksTubeIPSHack Control</h1><br><br>";
  resp += "No handler for that URL";
  resp +="</body>\n";
  resp +="</html>\n";
    
  server.send(200, "text/html", resp ); 
}

void handle_OnConnect() {
  Serial.println("handle_OnConnect()");

  String resp = "";
  resp +="<html><head>";
  resp +="<title>EleksTubeIPSHack Controller</title>\n";
  resp +="</head><body>";
  resp +="<h1>EleksTubeIPSHack Control</h1><br><br>";

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
  resp +="<html><head>";
  resp +="<title>EleksTubeIPSHack Controller</title>\n";
  resp +="</head><body>";
  resp +="<h1>EleksTubeIPSHack Control</h1><br><br>";

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
  resp +="<html><head>";
  resp +="<title>EleksTubeIPSHack Controller</title>\n";
  resp +="</head><body>";
  resp +="<h1>EleksTubeIPSHack Control</h1><br><br>";

  unsigned int sslen = server.arg("ssid").length() + 1;
  char myssid[ sslen ];
  server.arg("ssid").toCharArray( myssid, sslen );

  unsigned int pwlen = server.arg("pass").length() + 1;
  char myspass[ pwlen ];  
  server.arg("pass").toCharArray( myspass, pwlen );

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

void handle_FileList() {

  String path = "/";

  File root = FILESYSTEM.open(path);
  path = String();

  String output = "[";
  if(root.isDirectory()){
      File file = root.openNextFile();
      while(file){
          if (output != "[") {
            output += ',';
          }
          output += "{\"type\":\"";
          output += (file.isDirectory()) ? "dir" : "file";
          output += "\",\"name\":\"";
          output += String(file.name()).substring(1);
          output += "\"}";
          file = root.openNextFile();
      }
  }
  output += "]";
  server.send(200, "text/json", output);
}




void loop() {
  server.handleClient();
  smartDelay(500);
}
