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


#define ADDRESS1 0x10
#define ADDRESS2 0x11
// 30-170

int lamp;
int ldr=0;
bool mov=0;


void send_to_dimmer(int ADDRESS, int outByte){
  Wire.beginTransmission(ADDRESS);    // transmit to device 1
  Wire.write(outByte);                      // sends ONE bytes
  Wire.endTransmission();
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

#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif


bool altern=0;

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Hello World");
  Wire.begin();
  timeClient.begin();
  timeClient.setTimeOffset(0);
  setupCloudIoT(); // Creates globals for MQTT
  pinMode(LED_BUILTIN, OUTPUT);
}

static unsigned long lastMillis = 0;
static unsigned long lastMillis1 = 0;
void loop()
{
  if (!mqtt->loop())
  {
    mqtt->mqttConnect();
  }

  delay(10); // <- fixes some issues with WiFi stability

  // Request data
  if (millis() - lastMillis > 50)
  {
    lastMillis = millis();
  
    Serial.println(request_data());
    mqtt->publishTelemetry(request_data());
    lamp = values[1] * 10;
    Serial.println(lamp);
    send_to_dimmer(ADDRESS1, lamp);
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
