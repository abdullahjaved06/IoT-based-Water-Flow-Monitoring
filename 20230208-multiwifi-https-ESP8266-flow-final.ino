
#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

// could not get json to post
//#include <ArduinoJson.h>

#include <ESP8266HTTPClient.h>

#include <WiFiClientSecureBearSSL.h>
// Fingerprint for demo URL, expires on June 2, 2021, needs to be updated well before this date
//const uint8_t fingerprint[20] = {0x40, 0xaf, 0x00, 0x6b, 0xec, 0x90, 0x22, 0x41, 0x8e, 0xa3, 0xad, 0xfa, 0x1a, 0xe8, 0x25, 0x41, 0x1d, 0x1a, 0x54, 0xb3};
const char fingerprint[] PROGMEM = "46 B0 E8 09 1A 49 75 E4 D4 61 1B B7 A3 DA 7D FA E6 7B 92 B8";


#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif



//String api_user = "svuser";
//String api_pass = "svuser:34545655";


String serverName = "https://svuser:34545655@api1.stablevitals.com/?vital_api=1";



String doc;
// we need a few wifi details - so that we have SV-EXT
const char* ssidOne = "StableVitals.ext"; //replace this with your WiFi network name
const char* passwordOne = "1q@W3e4r"; //replace this with your WiFi network password

const char* ssidTwo = "StableVitals.com"; //replace this with your WiFi network name
const char* passwordTwo = "1234567890"; //replace this with your WiFi network password

const char* ssidThree = "ST3000"; //replace this with your WiFi network name
const char* passwordThree = "1234567890"; //replace this with your WiFi network password


const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

const int calVal_eepromAdress = 0;
unsigned long t = 0;

byte sensorInterrupt = 0;  // 0 = digital pin 2
byte sensorPin       = 14;

// The hall-effect flow sensor outputs approximately 4.5 pulses per second per
// litre/minute of flow.
// 20230327 - the original code called for 4.5 the testing we did witha pi showed 7.5 was a better number
//float calibrationFactor = 4.5;
float calibrationFactor = 7.5;

// hard  coded below
float volumeOvershoot = .88;

volatile byte pulseCount;  

int readingCount = 0;
const int readingsToTake = 5;

float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;

unsigned long oldTime;

/*
Insterrupt Service Routine
 */ 
 // ICACHE_RAM_ATTR
ICACHE_RAM_ATTR void pulseCounter() {
  // Increment the pulse counter
  pulseCount++;
}



ESP8266WiFiMulti WiFiMulti;

void setup() {

  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  delay(10);
  
  resetDocument();
  
  Serial.println();
  Serial.println("Starting...");
  Serial.print(" macid : ");
  Serial.println(WiFi.macAddress());
  Serial.println();
  Serial.println();
  Serial.println();

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(ssidOne, passwordOne);
  WiFiMulti.addAP(ssidTwo, passwordTwo);
  WiFiMulti.addAP(ssidThree, passwordThree);

  Serial.print(" macid : ");
  Serial.println(WiFi.macAddress());
  
  if ((WiFiMulti.run() == WL_CONNECTED)) {

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();
    Serial.println("setup wifi init");
    Serial.print("Connected! IP address: ");
    Serial.print(WiFi.localIP());
    Serial.print(" macid : ");
    Serial.println(WiFi.macAddress());
  }

     pinMode(sensorPin, INPUT);
  digitalWrite(sensorPin, HIGH);

  pulseCount        = 0;
  flowRate          = 0.0;
  flowMilliLitres   = 0;
  totalMilliLitres  = 0;
  oldTime           = 0;

  // The Hall-effect sensor is connected to pin 2 which uses interrupt 0.
  // Configured to trigger on a FALLING state change (transition from HIGH
  // state to LOW state)
 // attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
     Serial.print("sensor pin "); 
     Serial.println(sensorPin); 
     Serial.print("sensor pin interupt "); 
     Serial.println(digitalPinToInterrupt(sensorPin)); 
     
   attachInterrupt(digitalPinToInterrupt(sensorPin), pulseCounter, FALLING);

    
}


void resetDocument() {
  doc = "device_id=" + String(WiFi.macAddress());
  doc += "&type=flow_litres";
  doc += "&SSID=" + WiFi.SSID();
  // doc += "&localIP=" + WiFi.localIP().toString();

}

void loop() {

  if (readingCount > readingsToTake && (WiFiMulti.run() == WL_CONNECTED)) {

    struct tm timeinfo;
    Serial.println("good reading count . ");
    Serial.print("Connected! IP address: ");
    Serial.print(WiFi.localIP());
    Serial.print(" SSID: ");
    Serial.println(WiFi.SSID());
   // Serial.print(" MAC ID: ");
   // Serial.println(String(WiFi.macAddress()))
   
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);

    client->setFingerprint(fingerprint);
    // Or, if you happy to ignore the SSL certificate, then use the following line instead:

    Serial.print("RUNNING INSECURE\n");
    client->setInsecure();

    HTTPClient https;

    Serial.print("[HTTPS] begin...\n");
    
    if(!getLocalTime(&timeinfo)) {
      Serial.println("No time available (yet)");
    }

   
    if (https.begin(*client, serverName.c_str())) {  // HTTPS

      Serial.print("[HTTPS] x POST...\n");


      https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    //  https.setAuthorization(api_user, api_pass);
      // Data to send with HTTP POST
      // requestBody = "api_key=2345&field1=" + String(random(50));

      int httpCode = https.POST(doc);

      Serial.println(doc);

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTPS]  POST... code: %d\n", httpCode);

        // TODO we need to validate we did a good job
        Serial.println("TODO we need to validate we did a good job");
        
        readingCount = 0;
        resetDocument();
        Serial.println("PAYLOAD");
        String payload = https.getString();
        Serial.println(payload);
        Serial.println("DONE payload");

        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          //Serial.println(payload);
        }
      } else {
        Serial.printf("[HTTPS] POST ... failed, error: %s\n", https.errorToString(httpCode).c_str());
      }

      https.end();
    } else {
      Serial.printf("[HTTPS] Unable to connect\n");
    }
  }

//  Serial.print(" do the sensor work...");

     // Only process counters once per second
   if((millis() - oldTime) > 1000) { 
    // Disable the interrupt while calculating flow rate and sending the value to
    // the host
    detachInterrupt(sensorPin);
        
    // Because this loop may not complete in exactly 1 second intervals we calculate
    // the number of milliseconds that have passed since the last execution and use
    // that to scale the output. We also apply the calibrationFactor to scale the output
    // based on the number of pulses per second per units of measure (litres/minute in
    // this case) coming from the sensor.
    flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;
    
    // Note the time this processing pass was executed. Note that because we've
    // disabled interrupts the millis() function won't actually be incrementing right
    // at this point, but it will still return the value it was set to just before
    // interrupts went away.
    oldTime = millis();
    
    // Divide the flow rate in litres/minute by 60 to determine how many litres have
    // passed through the sensor in this 1 second interval, then multiply by 1000 to
    // convert to millilitres.
  //  flowMilliLitres = (flowRate / 60) * 1000;
//    flowMilliLitres = (flowRate / 60) * volumeOvershoot;
    flowMilliLitres = (flowRate / 60) * 1000;
    
    // Add the millilitres passed in this second to the cumulative total
    totalMilliLitres += flowMilliLitres;
      
    unsigned int frac;
    if(flowMilliLitres > 0) {
        // Print the flow rate for this second in litres / minute
    Serial.print("ZZZ Flow rate: ");
    Serial.print(int(flowRate));  // Print the integer part of the variable
    Serial.print(".");             // Print the decimal point
    // Determine the fractional part. The 10 multiplier gives us 1 decimal place.
    frac = (flowRate - int(flowRate)) * 10;
    Serial.print(frac, DEC) ;      // Print the fractional part of the variable
    Serial.print("L/min");
    // Print the number of litres flowed in this second
    Serial.print("  Current Liquid Flowing: ");             // Output separator
    Serial.print(flowMilliLitres);
    Serial.print("mL/Sec");

   readingCount++;
  time_t now;
  time(&now);
  Serial.print("time stamp Xyz - ");
    Serial.println(now);
      doc += "&reading[]=" + (String)(totalMilliLitres * volumeOvershoot);
      doc += "&pulses[]=" + (String)pulseCount;  
  doc += "&rDate[]=" + (String)now;
    // Print the cumulative total of litres flowed since starting
    Serial.print("  Output Liquid Quantity: ");             // Output separator
    Serial.print(totalMilliLitres);
    Serial.println("mL"); 

    }
  
    // Reset the pulse counter so we can start incrementing again
    pulseCount = 0;
    
    // Enable the interrupt again now that we've finished sending output
   // attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
   attachInterrupt(digitalPinToInterrupt(sensorPin), pulseCounter, FALLING);

  }
  
  
  
         

    
}

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}


bool ntp
(struct tm * info) {
    uint32_t ms = 5000;
    uint32_t start = millis();
    time_t now;
    while((millis()-start) <= ms) {
        time(&now);
        localtime_r(&now, info);
        if(info->tm_year > (2016 - 1900)){
            return true;
        }
        delay(10);
    }
    return false;
} 
