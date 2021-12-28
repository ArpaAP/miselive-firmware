#pragma GCC optimize ("-O2")
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "wpa2_enterprise.h"
#include <ArduinoJson.h>
#include <Wire.h>
#include <Scheduler.h>
#include "config.h"
#define VERSION "beta-0.1.0"
#define MEASURE_PIN 0
#define LED_POWER 4  // D2 (GPIO4)
#define SAMPLING_TIME 280
#define DELTA_TIME 40
#define SLEEP_TIME 9680

WiFiServer server(80);

float voMeasured = 0;
float calcVoltage = 0;
float dustDensity = 0;

class miseTask : public Task {
protected:
  void setup() {}

  void loop() {       
    digitalWrite(LED_POWER,LOW);                               // ledPower를 LOW로 설정합니다.
    delayMicroseconds(SAMPLING_TIME);                    // SAMPLING_TIME(280) 마이크로초만큼 지연합니다.
   
    voMeasured = analogRead(MEASURE_PIN);             //  MEASURE_PIN의 아날로그 값을 받아 voMeasured에 저장합니다.
   
    delayMicroseconds(DELTA_TIME);                          // DELTA_TIME(40) 마이크로초만큼 지연합니다.
    digitalWrite(LED_POWER,HIGH);                              // ledPower를 HIGH로 설정합니다.
    delayMicroseconds(SLEEP_TIME);                        // SLEEP_TIME(9680) 마이크로초만큼 지연합니다.
   
    calcVoltage = voMeasured * (5.0 / 1024.0);         // voMeasured의 값을 5.0/1024.0을 곱하여 calcVoltage에 저장합니다.
   
    int _d = (0.17 * calcVoltage - 0.1) * 1000;   // calcVoltage 값에 0.17을 곱하고 -0.1을 더합니다. (mg/m3)
                                                     // 값을 ug/m3 단위로 표현하기 위해 1000을 곱하여 dustDensity에 저장하여줍니다.
   
    if (_d > 0){                                      // dustDensity 30미만이면(잡음을 막기 위함)
      dustDensity = _d;
      Serial.print(dustDensity);                              // dustDensity을 시리얼 통신으로 출력합니다.
      Serial.println(" ug/m3");                               // " ug/m3"를 시리얼 통신으로 출력하고 줄을 바꿉니다.
    }
    delay(1000);                                                 // 1초동안 지연합니다.
  }
} mise_task;

class dataPostTask : public Task {
protected:
  void setup () {}

  void loop() {    
    WiFiClient client;
    HTTPClient http;
    http.begin(client, BACKEND_HOST);

    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> doc;
    doc["value"] = dustDensity;

    String jsonChar;
    serializeJson(doc, Serial);
    serializeJson(doc, jsonChar);
    
    int httpResponseCode = http.POST(jsonChar);

    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    http.end();
    
    delay(5000);
    yield();
  }
} data_post_task;

void setup(){
  Serial.begin(9600);          // 시리얼 통신을 사용하기 위해 보드레이트를 9600으로 설정합니다.
  pinMode(LED_POWER,OUTPUT);   // ledPower를 출력 단자로 설정합니다.

  Serial.println("Scanning WIFIs...");
  int numberOfNetworks = WiFi.scanNetworks();
 
  for(int i =0; i<numberOfNetworks; i++){
      Serial.print("Network name: ");
      Serial.println(WiFi.SSID(i));
      Serial.print("Signal strength: ");
      Serial.println(WiFi.RSSI(i));
      Serial.println("-----------------------");
  }
 
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(USE_EAP ? EAP_SSID : WIFI_SSID);

  if (USE_EAP)
  {
    Serial.println("USING EAP");
    wifi_set_opmode(STATION_MODE);
    
    // Configure SSID
    struct station_config wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    strcpy((char *)wifi_config.ssid, EAP_SSID);
    wifi_station_set_config(&wifi_config);
    
    // DO NOT use authentication using certificates
    wifi_station_clear_cert_key();
    wifi_station_clear_enterprise_ca_cert();
    
    // Authenticate using username/password
    wifi_station_set_wpa2_enterprise_auth(1);
    wifi_station_set_enterprise_identity((uint8 *)EAP_USERNAME, strlen(EAP_USERNAME));
    wifi_station_set_enterprise_username((uint8 *)EAP_USERNAME, strlen(EAP_USERNAME));
    wifi_station_set_enterprise_password((uint8 *)EAP_PASSWORD, strlen(EAP_PASSWORD));
    
    // Connect
    wifi_station_connect();
  } else {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    // Start the server    
    server.begin();
    Serial.println("Server started");
   
    // Print the IP address
    Serial.print("Use this URL to connect: ");
    Serial.print("http://");
    Serial.print(WiFi.localIP());
    Serial.println("/");
  }

  // Begin background tasks
  Scheduler.start(&mise_task);
  Scheduler.start(&data_post_task);
  Scheduler.begin();
}
 
void loop(){
  
}
