/******************************************************************************
 * Copyright 2018 Google
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>


#define ADDRESS1 0x10
#define ADDRESS2 0x11
// 30-170
char apiKey[50];
// Set web server port number to 80
WiFiServer server(80);

//flag for saving data
bool shouldSaveConfig = false;



void send_to_dimmer(int ADDRESS, int outByte){
  Wire.beginTransmission(ADDRESS);    // transmit to device 1
  Wire.write(outByte);                      // sends ONE bytes
  Wire.endTransmission();
}






void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void saveConfig() {
  Serial.println("saving config");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["apiKey"] = apiKey;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }

  json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
}

void loadConfig() {
  Serial.println("mounted file system");

  if (SPIFFS.exists("/config.json")) {
    //file exists, reading and loading
    Serial.println("reading config file");
    File configFile = SPIFFS.open("/config.json", "r");

    if (configFile) {
      Serial.println("opened config file");
      size_t size = configFile.size();
      // Allocate a buffer to store contents of the file.
      std::unique_ptr<char[]> buf(new char[size]);

      configFile.readBytes(buf.get(), size);
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.parseObject(buf.get());
      json.printTo(Serial);
      if (json.success()) {
        Serial.println("\nparsed json");
        strcpy(apiKey, json["apiKey"]);
      } else {
        Serial.println("failed to load json config");
      }
    }
  }
}





#if defined(ARDUINO_SAMD_MKR1000) or defined(ESP32)
#define __SKIP_ESP8266__
#endif

#if defined(ESP8266)
#define __ESP8266_MQTT__
#endif

#ifdef __SKIP_ESP8266__



void setup(){
  Serial.begin(115200);
}

void loop(){
  Serial.println("Hello World");
}

#endif

#ifdef __ESP8266_MQTT__
#include <CloudIoTCore.h>
#include "esp8266_mqtt.h"
//#include "wifi.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif


bool altern=0;
int ld2410 = 14; 



void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Hello World");
  Wire.begin();



  Serial.println("mounting FS...");

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount FS");
    return;
  }

  loadConfig();

  WiFiManager wifiManager;
    
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // id/name, placeholder/prompt, default, length
  WiFiManagerParameter customApiKey("apiKey", "Device ID", apiKey, 50);
  wifiManager.addParameter(&customApiKey);

  // Uncomment and run it once, if you want to erase all the stored information
  //wifiManager.resetSettings();

  wifiManager.autoConnect("AutoConnectAP", "EquipaF123");
  strcpy(apiKey, customApiKey.getValue());

  if (shouldSaveConfig) {
    saveConfig();
  }

  Serial.println("Connected....");

  device_id = apiKey;
  Serial.println(device_id);
  Serial.println(apiKey);

  
  
  timeClient.begin();
  timeClient.setTimeOffset(0);
  setupCloudIoT(); // Creates globals for MQTT
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ld2410, INPUT);

  
  
}

static unsigned long lastMillis = 0;
static unsigned long lastMillis1 = 0;
int lastMillis2 = 0;

void loop()
{
  if (!mqtt->loop())
  {
    mqtt->mqttConnect();
  }

  delay(10); // <- fixes some issues with WiFi stability

  analog = analogRead(LDR);
  vADC = adconversion(analog) - 0.15;
  rLDR = getRLDR(vADC);
  int buttonState = digitalRead(ld2410);

  float light = (vADC/3.2) * 100;

  if (millis() - lastMillis2 > 200)
  {
    lastMillis2 = millis();

    /*
    Serial.print("Ref: ");
    Serial.print(lightRef);
    Serial.print("   light: ");
    Serial.print(light);
    Serial.print("PWM: ");
    Serial.println(pwm);
    Serial.print("Vadc: ");
    Serial.print(vADC);
    Serial.print(" | R LDR: ");
    Serial.println(rLDR);
    */
    Serial.print("Voltage:    ");
    Serial.println(vADC); 
    Serial.print("Light:    ");
    Serial.println(light);
    Serial.print("LD:    ");
    Serial.println(buttonState);
    if (values[0] == 0){
      lamp = lightController(light, lightRef, lamp);
      
      if (buttonState == 0 && values[2] == 1){lamp=0;}
      Serial.println(lamp);
      send_to_dimmer(ADDRESS1, lamp);
    }
  }
  // Request data
  if (millis() - lastMillis > 50)
  {
    lastMillis = millis();
  
    //Serial.println(request_data());
    mqtt->publishTelemetry(request_data());
    Serial.println(request_data());
    lightRef = values[1] * 10;
    if (values[0] == 1){
      lamp = lightRef;
      
      if (buttonState == 0 && values[2] == 1){lamp=0;}
      Serial.println(lamp);
      send_to_dimmer(ADDRESS1, lamp);
    }
  }
    
  //Send data
  if (millis() - lastMillis1 > 10000)
  {
    lastMillis1 = millis();

    Serial.println(send_data_ldr());
    mqtt->publishTelemetry(send_data_ldr());

    delay(10);
    Serial.println(send_data_mov());
    mqtt->publishTelemetry(send_data_mov());

    delay(10);
    Serial.println(send_data_lamp());
    mqtt->publishTelemetry(send_data_lamp());
  }


}

#endif
