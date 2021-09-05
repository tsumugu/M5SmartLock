#include <M5StickC.h>
#include <WiFi.h>
#include <Wire.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <IOXhop_FirebaseStream.h>
#include <IOXhop_FirebaseESP32.h>

#define WIFI_SSID "[SSID here]"
#define WIFI_PASSWORD "[WIFI PW here]"
#define FIREBASE_DB_URL "xxxxxx.firebaseio.com"
#define FIREBASE_DB_AUTH "xxxxxx"
const String device_id = "[your unique device id here]";
#define FIREBASE_DB_DATA "/m5smartlock-status/[device_id here]"
Servo servo1;
const int32_t MIN_US = 500;
const int32_t MAX_US = 2400;
bool is_locking = false;
bool is_notmoved = true;

void open_door() {
  is_locking = false;
  is_notmoved = true;
  M5.Lcd.fillScreen(GREEN);
}
void lock_door() {
  is_locking = true;
  is_notmoved = true;
  M5.Lcd.fillScreen(RED);
}

void onStatusChanged(String status) {
  if (status == "LOCKED"){
    lock_door();
  } else {
    open_door();
  }
}

String GET_Request(const char* server) {
  HTTPClient http;    
  http.begin(server);
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();

  return payload;
}

void sendCommandToIoTCloud(String status) {
  String getURL = "http://[your server domain (xxxxxx.com) here]/m5smartlock/set-status/?deviceid="+device_id+"&reqstatus="+status;
  Serial.println(GET_Request(getURL.c_str()));
}

void setup() {
  // put your setup code here, to run once:
  M5.begin();
  Serial.begin(115200);
  Wire.begin(0,26);
  M5.Lcd.setRotation(3);
  M5.Lcd.setCursor(0,0,2);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int cnt=0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    cnt++;
    if(cnt%10==0) {
      M5.Lcd.println("WIFI Connection Failed");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    if(cnt>=30) {
      ESP.restart();
    }
  }
  M5.Lcd.println("WIFI Connected");
  delay(500);
  Firebase.begin(FIREBASE_DB_URL, FIREBASE_DB_AUTH);
  M5.Lcd.println("Firebase Connected");
  delay(500);
  Firebase.stream(FIREBASE_DB_DATA, [](FirebaseStream stream) {
    String eventType = stream.getEvent();
    eventType.toLowerCase();
    Serial.print("event: ");
    Serial.println(eventType);
    if (eventType == "put"){
      // JSONをParse
      String receiced_string_data = stream.getDataString();
      StaticJsonBuffer<200> jsonBuffer;
      char json[50];
      receiced_string_data.toCharArray(json, 50);
      JsonObject& root = jsonBuffer.parseObject(json);
      if (root.success()) {
          const char* lock_status = root["status"];
          String string_lock_status = String(lock_status);
          Serial.print("raw data: ");
          Serial.println(receiced_string_data);
          /*
          Serial.print("parsed data: ");
          Serial.println(lock_status);
          */
          // statusをcallbackに渡す
          onStatusChanged(string_lock_status);
      } else {
        Serial.println("JSON Parse Error Occured");
      }
    }
  });
}

void loop() {
  // put your main code here, to run repeatedly:
  M5.update();
  // 本体の前面ボタンが押されたときにfirebaseを書き換える。
  if (M5.BtnA.wasPressed()) {
    if (is_locking) {
      // {"status": "UNLOCKED"}
      sendCommandToIoTCloud("UNLOCKED");
    } else {
      // {"status": "LOCKED"}
      sendCommandToIoTCloud("LOCKED");
    }
  }
  if (is_notmoved) {
    if (is_locking) {
      servo1.setPeriodHertz(50);
      servo1.attach(26, MIN_US, MAX_US);
      delay(50);
      servo1.write(90);
      delay(500);
      servo1.detach();
      is_notmoved = false;
    } else {
      servo1.setPeriodHertz(50);
      servo1.attach(26, MIN_US, MAX_US);
      delay(50);
      servo1.write(0);
      delay(500);
      servo1.detach();
      is_notmoved = false;
    }
  }
  delay(1);
}
