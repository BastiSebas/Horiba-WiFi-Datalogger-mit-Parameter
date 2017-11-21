#include <FS.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>
//define your default values here, if there are different values in config.json, they are overwritten.
char MEASUREMENT_NAME[34] = "YOUR_MEASUREMENT_NAME";
const char* AutoConnectAPName = "AutoConnectAP";
const char* AutoConnectAPPW = "password";

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  // Serial.println("Should save config");
  shouldSaveConfig = true;
}

#define OLED_RESET LED_BUILTIN  // Bei ESP8266 ResetPin anders belegt!!
Adafruit_SSD1306 display(OLED_RESET);
String Daten;

// Server Config
const char* InfluxDB_Server_IP = "130.149.67.141";
const int InfluxDB_Server_Port = 8086;
WiFiClient client;

String buildstring (double &NO_s, double &NO2_s, double &NOx_s){
  String content = String(MEASUREMENT_NAME) + ",host=esp8266 NO_horiba=" + String(NO_s, 4)
  + ",NO2_horiba=" + String(NO2_s, 4) + "," + "NOx_horiba=" + String(NOx_s, 4);
  return content;
}

void Upload(String Uploadstring){
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(Uploadstring);
  display.display();
  //send to InfluxDB
  // Verbindungstest mit dem InfluxDB Server connect() liefert bool false / true als return
  int Verbindungsstatus = client.connect(InfluxDB_Server_IP, InfluxDB_Server_Port);

  // Kann keine Verbindung hergestellt werden -> Fehlerausgabe incl. Fehlernummer
  if(Verbindungsstatus <= 0) {
    display.print("Keine Verbindung zum InfluxDB Server, Error #");
    display.println(Verbindungsstatus);
    display.display();
    return;
    }
  client.println("POST /write?db=MESSCONTAINER HTTP/1.1");
  client.println("User-Agent: esp8266/0.1");
  client.println("Host: localhost:8086");
  client.println("Accept: */*");
  client.print("Content-Length: " + String(Uploadstring.length()) + "\r\n");
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.println(); /*HTTP Header und Body müssen durch Leerzeile getrennt werden*/
  client.println(Uploadstring); // Übermittlung des eigentlichen Data-Strings
  client.flush(); //
  delay(100); //wait for server to process data

  // Antwort des Servers wird gelesen, ausgegeben und anschließend die Verbindung geschlossen
  display.println("Antwort des Servers");
  while(client.available()) { // Empfange Antwort
    display.print((char)client.read());
    }

  display.println();
  display.display();
  client.stop();
}

void SendRequest() {
  Serial.write(02);
  Serial.write(68);
  Serial.write(65);
  Serial.write(03);
  Serial.write(48);
  Serial.write(52);
}

void Displayvalues(double &NO_d, double &NO2_d, double &NOx_d){
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("NO: ");
  display.print(NO_d, 2);
  display.println("ppb");
  display.print("NO2: ");
  display.print(NO2_d, 2);
  display.println("ppb");
  display.print("NOx: ");
  display.print(NOx_d, 2);
  display.println("ppb");
  display.display();
  delay(1000);
}

void ReadData(){
  char buffer[100];
  buffer[0]=0;
  //numBytes = Serial.available();
  int numBytes = 99;
  Serial.readBytes(buffer, numBytes);
  buffer[99] = '\0';
  display.println(buffer);
  display.display();
  delay(1000);

  char NO_b[6];
  char NO_p[4];
  memcpy(NO_b, &buffer[10], 5 );
  NO_b[5] = '\0';
  memcpy(NO_p, &buffer[15], 3 );
  NO_p[4] = '\0';


  char NO2_b[6];
  char NO2_p[4];
  memcpy(NO2_b, &buffer[40], 5 );
  NO2_b[5] = '\0';
  memcpy(NO2_p, &buffer[45], 3 );
  NO2_p[3] = '\0';

  char NOx_b[6];
  char NOx_p[4];
  memcpy(NOx_b, &buffer[70], 5 );
  NOx_b[5] = '\0';
  memcpy(NOx_p, &buffer[75], 3 );
  NOx_p[3] = '\0';

  char verify[5];
  memcpy(verify, &buffer[1], 4 );
  NO_b[5] = '\0';
  String verifystring;
  verifystring.concat(verify);

  display.clearDisplay();
  display.setCursor(0, 0);

  display.print("NO_b: ");
  display.println(NO_b);
  display.print("NO_p: ");
  display.println(NO_p);
  display.print("NO2_b: ");
  display.println(NO2_b);
  display.print("NO2_p: ");
  display.println(NO2_p);
  display.print("NOx_b: ");
  display.println(NOx_b);
  display.print("NOx_p: ");
  display.println(NOx_p);
  display.display();
  delay(1000);


  double NO = atoi(NO_b) * pow(10, atoi(NO_p));
  double NO2 = atoi(NO2_b) * pow(10, atoi(NO2_p));
  double NOx = atoi(NOx_b) * pow(10, atoi(NOx_p));
  Displayvalues(NO, NO2, NOx);

  String DATA=buildstring(NO, NO2, NOx);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.display();
  delay(1000);

  if (verifystring.startsWith("MD03")){
    Upload(DATA);
  }
  else {
    while(Serial.available()){
      Serial.read();
      display.println("Data corrupt");
      display.display();
    }
  }
}

void setup()   {

Serial.begin(9600);
display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
display.setTextSize(1);
display.setTextColor(WHITE);
display.clearDisplay();
display.println("Starte Konfiguration");
display.print("SSID: ");
display.println(AutoConnectAPName);
display.print("Passwort: ");
display.println(AutoConnectAPPW);
display.display();
// Serial.println("mounting FS...");

if (SPIFFS.begin()) {
  // Serial.println("mounted file system");
  if (SPIFFS.exists("/config.json")) {
    //file exists, reading and loading
    // Serial.println("reading config file");
    File configFile = SPIFFS.open("/config.json", "r");
    if (configFile) {
      // Serial.println("opened config file");
      size_t size = configFile.size();
      // Allocate a buffer to store contents of the file.
      std::unique_ptr<char[]> buf(new char[size]);

      configFile.readBytes(buf.get(), size);
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.parseObject(buf.get());
      json.printTo(Serial);
      if (json.success()) {
        Serial.println("\nparsed json");

        strcpy(MEASUREMENT_NAME, json["MEASUREMENT_NAME"]);

      } else {
        // Serial.println("failed to load json config");
      }
    }
  }
} else {
  // Serial.println("failed to mount FS");
}
//end read


// The extra parameters to be configured (can be either global or just in the setup)
// After connecting, parameter.getValue() will get you the configured value
// id/name placeholder/prompt default length
WiFiManagerParameter custom_MEASUREMENT_NAME("Measurement", "MEASUREMENT_NAME", MEASUREMENT_NAME, 32);

//WiFiManager
//Local intialization. Once its business is done, there is no need to keep it around
WiFiManager wifiManager;

//set config save notify callback
wifiManager.setSaveConfigCallback(saveConfigCallback);

//set static ip

//add all your parameters here
wifiManager.addParameter(&custom_MEASUREMENT_NAME);

//sets timeout until configuration portal gets turned off
//useful to make it all retry or go to sleep
//in seconds
//wifiManager.setTimeout(120);

//fetches ssid and pass and tries to connect
//if it does not connect it starts an access point with the specified name
//here  "AutoConnectAP"
//and goes into a blocking loop awaiting configuration
if (!wifiManager.autoConnect(AutoConnectAPName, AutoConnectAPPW)) {
  // Serial.println("failed to connect and hit timeout");
  delay(3000);
  //reset and try again, or maybe put it to deep sleep
  ESP.reset();
  delay(5000);
}

//if you get here you have connected to the WiFi
display.print("Verbindung hergestellt");
display.display();
delay(1000);

//read updated parameters
strcpy(MEASUREMENT_NAME, custom_MEASUREMENT_NAME.getValue());

//save the custom parameters to FS
if (shouldSaveConfig) {
  Serial.println("saving config");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["MEASUREMENT_NAME"] = MEASUREMENT_NAME;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }

  json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
  //end save
}
display.clearDisplay();
display.setCursor(0, 0);
display.println("local ip");
display.println(WiFi.localIP());
display.println("Name der Messung: ");
display.println(MEASUREMENT_NAME);
Serial.println("local ip");
Serial.println(WiFi.localIP());
display.display();
delay(5000);
// text display tests

}

void loop() {
    display.setCursor(0, 0);
    display.display();
    SendRequest();
    delay(1000);

    if (Serial.available() > 0) {
      ReadData();
      delay(1000);
      }

    else {
      display.println("No Data");
      display.display();
      delay(1000);
    }
    display.clearDisplay();
}
