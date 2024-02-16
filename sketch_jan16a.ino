#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <DHT11.h>
#include <ArduinoJson.h>

//Home
// const char *ssid =  "TP-LINK_696C";
// const char *pass =  "46199881";
//Garage
const char *ssid =  "YOTA-4689";
const char *pass =  "76543210";

WiFiClient client;

#define DHT_PIN D1
DHT11 dht11(DHT_PIN);
float temperature;
//PROD
String host_url="http://193.124.113.20:8010";
//DEV
// String host_url="http://10.0.0.101:6565";

String info_url="/miner/info";
 
const char* minerIP1 = "10.0.0.101"; 
const char* minerIP2 = "10.0.0.102"; 
const char* minerIP3 = "10.0.0.103"; 

const int minerPORT = 4028;   

const char* minerIP = minerIP1;

#define INTERVAL 600000

struct MinerInfo {
  float tempMin;
  float tempMax;
  float tempAvg;
  float envTemp;
  float currentTemp;
  float power;
  float hashRate;
};

void wifiConnect() {
  Serial.println("Connecting to ");
  Serial.println(ssid); 
 
  WiFi.begin(ssid, pass); 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected"); 
}

void setup() 
{
       Serial.begin(9600);
       delay(10);
               
       wifiConnect();
}

void getTemperature() {
  float newTemperature = dht11.readTemperature();
  if (newTemperature != DHT11::ERROR_CHECKSUM && newTemperature != DHT11::ERROR_TIMEOUT)
  {
      Serial.print("Temperature: ");
      Serial.print(newTemperature);
      Serial.println(" °C");
      temperature = newTemperature;
  }
  else
  {
      Serial.println(DHT11::getErrorString(newTemperature));
  }
}
 
void sendHttpInfo(MinerInfo *info) {
  Serial.printf("INFO: %d\n", info);
  if (info == NULL) {
    return;
  }
        WiFiClient client;
        HTTPClient http;

        // Specify the target URL
        String url = host_url + info_url;
        http.begin(client, url);

        http.addHeader("Content-Type", "application/json");

        // Buffer to hold the formatted string
        char buffer[120];

        // Formatting using sprintf
        sprintf(buffer, "{\"chipsTemp\":\"%s\",\"power\":%.2f,\"performance\":%.2f,\"roomTemp\":%.2f,\"outerTemp\":%.2f}", 
          String(info->currentTemp), info->power, info->hashRate / 1000000, info->envTemp, temperature);
        
        Serial.printf("Sending: %s\n", buffer);
        
        // Send the GET request and check for errors
        int httpCode = http.PUT(buffer);
        if (httpCode > 0) {
            // HTTP response received
            Serial.printf("[HTTP] PUT request response code: %d\n", httpCode);

            // Read the response body
            String payload = http.getString();
            Serial.println("Response: " + payload);
        } else {
            Serial.printf("[HTTP] PUT request failed, error: %s\n", http.errorToString(httpCode).c_str());
        }

        // Close the connection
        http.end();
}

int parseMinerInfo(const String& json, MinerInfo *minerInfo) {
  StaticJsonDocument<1300> doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return -1;
  }

  minerInfo->tempMin = doc["SUMMARY"][0]["Chip Temp Min"];
  minerInfo->tempMax = doc["SUMMARY"][0]["Chip Temp Max"];
  minerInfo->tempAvg = doc["SUMMARY"][0]["Chip Temp Avg"];
  minerInfo->power = doc["SUMMARY"][0]["Power"];
  minerInfo->currentTemp = doc["SUMMARY"][0]["Temperature"];
  minerInfo->hashRate = doc["SUMMARY"][0]["HS RT"];
  minerInfo->envTemp = doc["SUMMARY"][0]["Env Temp"];

  return 0;
}

int getMinerInfo(MinerInfo *minerInfo) {
  Serial.printf("Connecting to %s\n", minerIP);
  // Create a TCP client
  WiFiClient client;

  // Connect to the server
  if (client.connect(minerIP, minerPORT)) {
      Serial.println("Connected to server");

      // Send the message
      client.write("{\"cmd\":\"summary\"}");

      // Read and print the server's response
      String response = "";
      Serial.printf("Client available: %d\n", client.available());
      char buf[1000];
      client.peekBytes(buf, 1000);
      Serial.printf("Resp: %s\n", buf);
      while (client.available()) {
          char c = client.read();
          response += c;
      }
      Serial.println("Server response: " + response);

      // Close the connection
      client.stop();
      Serial.println("Message sent");
      int result = parseMinerInfo(response, minerInfo);
      if (result == 0) {
        Serial.printf("Env Temp: %.2f, minerTemp: %.2f\n", minerInfo->envTemp, minerInfo->currentTemp);
        return 1;
      }
  } else {
    Serial.println("Connection to server failed");
    if (minerIP == minerIP3) {
      minerIP = minerIP1;
      return 0;
    } else {
      if (minerIP == minerIP1) {
        minerIP = minerIP2;
      } else if (minerIP == minerIP2) {
        minerIP = minerIP3;
      }
      return getMinerInfo(minerInfo);
    }
  }
  return 0;
}

void loop() 
{  
  getTemperature();   
  MinerInfo info;
  int result = getMinerInfo(&info); 
  Serial.printf("Result: %d\n", result);
  if (WiFi.status() == WL_CONNECTED) {
    if (result > 0) {
      sendHttpInfo(&info);
    } else {
      Serial.printf("Miner info failed\n");  
    }
  } else {
    Serial.printf("Wifi is not connected\n");
    wifiConnect();
  }
  delay(INTERVAL);
}

