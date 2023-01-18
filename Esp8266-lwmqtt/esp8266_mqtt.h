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
// This file contains static methods for API requests using Wifi / MQTT

#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FS.h>


// You need to set certificates to All SSL cyphers and you may need to
// increase memory settings in Arduino/cores/esp8266/StackThunk.cpp:
//   https://github.com/esp8266/Arduino/issues/6811
#include "WiFiClientSecureBearSSL.h"
#include <time.h>

#include <MQTT.h>

#include <CloudIoTCore.h>
#include <CloudIoTCoreMqtt.h>
#include "ciotc_config.h" // Wifi configuration here

#define vIN 5
#define vAREF 3.3
#define R 4000  // Voltage divider resistor value

int lamp;
int ldr=0;
bool mov=0;
int LDR = 0;
int analog;
float vADC, rLDR;
int lightRef = 40;




int values[3] = {};

float getRLDR(float vADC) {
  float LDR = (R * (vIN - vADC)) / vADC;             // Voltage divider to get the resistance of the LDR
  return LDR;
}

float adconversion(int analog) {

  float ADC = float(analog) * (vAREF / float(1023));  // Converts analog to voltage
  //int lux = ???;                          // Converts resitance to lumen
  return ADC;
}

int lightController(int light, int lightref, int spwm){
  float kp=0.2;
  int diff = lightref - light;
  Serial.print("Lightdiff:    ");
  Serial.println(diff); 
  spwm = spwm + int(diff * kp);
  spwm = max(min(spwm,100),0);

  return spwm;
}

// !!REPLACEME!!
// The MQTT callback function for commands and configuration updates
// Place your message handler code here.
void messageReceivedAdvanced(MQTTClient *client, char topic[], char bytes[], int length)
{
  if (length > 0){
    char ree[]= "";
    strcpy(ree, bytes);
    char delimiters[] = ",";

    char *ptr;
    int index = 0;

    ptr = strtok(ree, delimiters);

    while (ptr != NULL) {
      values[index] = atoi(ptr);
      ptr = strtok(NULL, delimiters);
      index = index + 1;
    }
    
    Serial.printf("incoming: %s - %s\n", topic, bytes);
    
  } else {
    Serial.printf("0\n"); // Success but no message
  }
}
///////////////////////////////

// Initialize WiFi and MQTT for this board
static MQTTClient *mqttClient;
static BearSSL::WiFiClientSecure netClient;
static BearSSL::X509List certList;
static CloudIoTCoreDevice device(project_id, location, registry_id, device_id);
CloudIoTCoreMqtt *mqtt;

// Initialize NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");


///////////////////////////////
// Helpers specific to this board
///////////////////////////////
String send_data_ldr()
{
  //send data ldr
  String value = "'value': " + String(ldr);
  String sensor_id = "'sensor_id': '" + String(device_id) + "_ldr'";
  return "{'Action':'send', 'table': 'sensor_value', " + value + ", " + sensor_id + "}";
}

String send_data_mov()
{
  //send data ld2410
  String value = "'value': " + String(mov);
  String sensor_id = "'sensor_id': '" + String(device_id) + "_movimento'";
  return "{'Action':'send', 'table': 'sensor_value', " + value + ", " + sensor_id + "}";
}

String send_data_lamp()
{
  int len = strlen(device_id);
  Serial.println(device_id[len-1]);

  //send data lampada
  String value = "'value': " + String(lamp);
  String light_id = "'light_id': " + String(device_id[len-1]);
  Serial.println(light_id);
  return "{'Action':'send', 'table': 'brightness_value', " + value + ", " + light_id + "}";
}

String request_data()
{
  
  return "{'Action':'request','Device':'" + String(device_id) + "'}";
}





String getJwt()
{
  // Disable software watchdog as these operations can take a while.
  ESP.wdtDisable();
  time_t iat = time(nullptr);
  Serial.println("Refreshing JWT");
  String jwt = device.createJWT(iat, jwt_exp_secs);
  ESP.wdtEnable(0);
  return jwt;
}

static void readDerCert(const char *filename) {
  File ca = SPIFFS.open(filename, "r");
  if (ca)
  {
    size_t size = ca.size();
    uint8_t cert[size];
    ca.read(cert, size);
    certList.append(cert, size);
    ca.close();

    Serial.print("Success to open ca file ");
  }
  else
  {
    Serial.print("Failed to open ca file ");
  }
  Serial.println(filename);
}

static void setupCertAndPrivateKey()
{
  // Set CA cert on wifi client
  // If using a static (pem) cert, uncomment in ciotc_config.h:
  certList.append(primary_ca);
  certList.append(backup_ca);
  netClient.setTrustAnchors(&certList);

  device.setPrivateKey(private_key);
  return;

  // If using the (preferred) method with the cert and private key in /data (SPIFFS)
  // To get the private key run
  // openssl ec -in <private-key.pem> -outform DER -out private-key.der

  if (!SPIFFS.begin())
  {
    Serial.println("Failed to mount file system");
    return;
  }

  readDerCert("/gtsltsr.crt"); // primary_ca.pem
  readDerCert("/GSR4.crt"); // backup_ca.pem
  netClient.setTrustAnchors(&certList);


  File f = SPIFFS.open("/private-key.der", "r");
  if (f) {
    size_t size = f.size();
    uint8_t data[size];
    f.read(data, size);
    f.close();

    BearSSL::PrivateKey pk(data, size);
    device.setPrivateKey(pk.getEC()->x);

    Serial.println("Success to open private-key.der");
  } else {
    Serial.println("Failed to open private-key.der");
  }

  SPIFFS.end();
}

static void setupWifi()
{
  /*WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
  }*/

  configTime(0, 0, ntp_primary, ntp_secondary);
  Serial.println("Waiting on time sync...");
  while (time(nullptr) < 1510644967)
  {
    delay(10);
  }
}

///////////////////////////////
// Orchestrates various methods from preceeding code.
///////////////////////////////
bool publishTelemetry(String data)
{
  return mqtt->publishTelemetry(data);
}

bool publishTelemetry(const char *data, int length)
{
  return mqtt->publishTelemetry(data, length);
}

bool publishTelemetry(String subfolder, String data)
{
  return mqtt->publishTelemetry(subfolder, data);
}

bool publishTelemetry(String subfolder, const char *data, int length)
{
  return mqtt->publishTelemetry(subfolder, data, length);
}

// TODO: fix globals
void setupCloudIoT()
{
  // ESP8266 WiFi setup
  setupWifi();

  // ESP8266 WiFi secure initialization and device private key
  setupCertAndPrivateKey();

  mqttClient = new MQTTClient(512);
  mqttClient->setOptions(180, true, 1000); // keepAlive, cleanSession, timeout
  mqtt = new CloudIoTCoreMqtt(mqttClient, &netClient, &device);
  mqtt->setUseLts(true);
  mqtt->startMQTTAdvanced(); // Opens connection using advanced callback
}
