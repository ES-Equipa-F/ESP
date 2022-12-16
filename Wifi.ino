#include <ArduinoJson.h>
#include <FS.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>

// Set web server port number to 80
WiFiServer server(80);

//flag for saving data
bool shouldSaveConfig = false;

char apiKey[50];

//callback notifying us of the need to save config
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

void setup() {
  Serial.begin(115200);

  //clean FS, for testing
  //SPIFFS.format();
  
  //read configuration from FS json
  Serial.println("mounting FS...");

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount FS");
    return;
  }

  loadConfig();
  
  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  // id/name, placeholder/prompt, default, length
  WiFiManagerParameter customApiKey("apiKey", "API Key", apiKey, 50);
  wifiManager.addParameter(&customApiKey);
  
  // Uncomment and run it once, if you want to erase all the stored information
  //wifiManager.resetSettings();
  
  wifiManager.autoConnect("AutoConnectAP", "EquipaF123");
  strcpy(apiKey, customApiKey.getValue());

  if (shouldSaveConfig) {
    saveConfig();
  }
  
  Serial.println("Connected....");
  

}

void loop() {
  // put your main code here, to run repeatedly:

}
