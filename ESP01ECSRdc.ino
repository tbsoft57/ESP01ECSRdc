/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ESP01 vu du dessus (RST et CH_EN doivent être HIGH, io0 doit être HIGH au Boot sinon ESP passe en Mode Flachage)//
//                                                                                                                 //
//               io3-Rx   VCC                                                                                      //
//               io0      RST                                                                                      //
//               io2      CH_EN                                                                                    //
//               GND      io1-Tx                                                                                   //
//                                                                                                                 //
// DS18B20 debout vu de face avec meplat en 1er plan et arrondi à l'arrière                                        //
//                                                                                                                 //
//   GND  DQ  VDD                                                                                                  //
//                                                                                                                 //
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Type de carte Arduino IDE: GENERIC ESP8266 Module 1MB

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <OneWire.h> 
#include <DallasTemperature.h>Test

#define OTAName                    "ESP01ECSRdc"
#define WifiSSID                   "TBSOFT"
#define WifiPass                   "TB04011966"
#define IP                         55

#define io0                        0 // -> Doit être Pulled-Up par une resistance externe au Boot si elle est cablée sur autre chose (sinon Mode flash Boot)
#define io1                        1 // Tx OUTPUT 
#define io2                        2 // Correspond a OnBoardLed -> Doit être Pulled-Up par une resistance externe au Boot si elle est cablée sur autre chose (sinon Mode flash Boot)
#define io3                        3 // Rx INPUT 
#define tx                         1
#define rx                         3 

#define BILAME                     rx  // ROSE io1
#define ONE_WIRE_BUS               io0 // GRIS 
#define SSR                        io2 // BLANC
#define LIBRE                      tx  // BLEU io3

#define forceMaxInterval           7200000        // res à 1200W pendant 2h
#define wifiMaxInterval            60000          // si pas de reception alpha pendant 1min => on met alpha = 0
#define tempMaxInterval            1000           // lecture de temperatures toutes les secondes
#define ResWatts                   1200           // Ouissance de la resistance
#define pwmFreq                    20             // frequance correspondant à alpha = 1
#define ECSTempCorrector           1

float alpha                        = 0;           // tx de hachage du SSD
float ECStemp                      = 0;           // temperature ECS 
float watts                        = 0;           // puissance instantanée calculée avec Irms et Vrms

unsigned long wifiTimer            = millis();    // Timer pour le delais min quand plus de reception wifi
unsigned long tempTimer            = millis();    // Timer pour les delais de re-lecture des température
unsigned long forceTimer           = millis();    // Timer pour les delais du mode forcé

bool modeForce = false;

// Class instances
ESP8266WebServer server(80); OneWire oneWire(ONE_WIRE_BUS); DallasTemperature sensors(&oneWire, ONE_WIRE_BUS);

void loop() { server.handleClient(); ArduinoOTA.handle(); 
  if (modeForce) {
     if (millis() - forceTimer   > forceMaxInterval) { alpha = 0; watts = 0; analogWrite(SSR, 0); modeForce = false; }      
  } else {
    if (millis() - wifiTimer    > wifiMaxInterval)   { alpha = 0; watts = 0; analogWrite(SSR, 0); wifiTimer  = millis(); }   
  }
  if (millis() - tempTimer    > tempMaxInterval)               {     
    if (sensors.getTempCByIndex(0) > 0) ECStemp=sensors.getTempCByIndex(0)*ECSTempCorrector;                 
    sensors.requestTemperatures();   
    tempTimer=millis(); 
  }   
}
void root() { 
  if (!modeForce) {
    if (server.arg("alpha" )!="") { 
      alpha = server.arg("alpha").toFloat(); 
      watts = (digitalRead(BILAME)==LOW)? alpha * ResWatts:0; 
      analogWrite(SSR, alpha*255);   
    }
  }
  if (server.arg("modeForce" )!="") { 
    modeForce = (server.arg("modeForce" ) == "true")? true:false; 
    if (modeForce) { analogWrite(SSR, 255); forceTimer  = millis(); watts = (digitalRead(BILAME)==HIGH)? ResWatts:0; }  
    else           { analogWrite(SSR, 0);   forceTimer  = millis(); watts = 0; }
  }
  sendJsonResponse(); wifiTimer = millis(); 
}
void sendJsonResponse() { 
  setHeaders(); server.send(200, "application/json", "{ "
  "\"api\": \"ecsRdcStatus\", "
  "\"ECStemp\": " + String(ECStemp, 2) + ", "
  "\"alpha\": " + String(alpha, 6) + ", "
  "\"Res\": " + ((digitalRead(BILAME)==LOW)? "true":"false") + ", "  
  "\"modeForce\": " + ((modeForce)? "true":"false") + ", "    
  "\"watts\": " + String(watts) + " }"); 
}
void cors()             { setHeaders(); server.send(200, "text/plain", "" ); }
void handleNotFound()   { if (server.method() == HTTP_OPTIONS) { setHeaders(); server.send(204); } else server.send(404, "text/plain", ""); }
void redirectToRoot()   { server.sendHeader("Location", "/",true); server.send(302, "text/html",""); }
void setHeaders() {
  server.sendHeader("Access-Control-Max-Age", "10000");
  server.sendHeader("Access-Control-Allow-Methods", "GET,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");  
  server.sendHeader("Access-Control-Allow-Origin","*");      
}
void setup() {  
  pinMode(tx, FUNCTION_3); // -> transforme tx et rx en GPIO
  pinMode(rx, FUNCTION_3); // -> transforme tx et rx en GPIO
  
  pinMode(BILAME, INPUT); 
  pinMode(SSR,    OUTPUT); 

  WiFi.config(IPAddress(192, 168, 0, IP), IPAddress(192, 168, 0, 1), IPAddress(255, 255, 255, 0));
  WiFi.hostname(OTAName); WiFi.mode(WIFI_STA); WiFi.begin(WifiSSID, WifiPass);
  while (WiFi.status() != WL_CONNECTED) { delay(250); }

  ArduinoOTA.setHostname(OTAName); ArduinoOTA.begin();  

  server.on("/",               root); 
  server.on("/", HTTP_OPTIONS, cors);
  server.onNotFound(handleNotFound);
  server.begin(); 
  
  sensors.begin(); 
  sensors.setResolution(9); 
  sensors.setWaitForConversion(false);  
  sensors.requestTemperatures(); 
  
  analogWriteFreq(pwmFreq); analogWrite(SSR, 0);  
}

