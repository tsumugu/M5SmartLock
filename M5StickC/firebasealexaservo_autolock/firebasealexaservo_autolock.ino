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
// GYRO_Y_AVR_NUM回連続でgyroYの値がGYRO_Y_TRESHOLDを超えた場合に開閉したとみなす
#define GYRO_Y_AVR_NUM 5
#define GYRO_Y_AVR_NUMM1 4 // GYRO_Y_AVR_NUMから1を引いた値
#define GYRO_Y_TRESHOLD 30
// 30秒後にオートロックする
#define AUTO_LOCK_WAIT_SEC 30

Servo servo1;
const int32_t MIN_US = 500;
const int32_t MAX_US = 2400;
bool is_locking = false;
bool is_notmoved = true;
float gyroX = 0;
float gyroY = 0;
float gyroZ = 0;
bool isSendCommand = false;
bool isTimerStarted = false;
unsigned long startMills = 0;
float gyroY_list[GYRO_Y_AVR_NUM];
int add_count = 0;

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


void add_gyroY_list(float list[GYRO_Y_AVR_NUM], float arg_gyroY) {
  // 全体をひとつずつずらす
  for (int i=0;i<GYRO_Y_AVR_NUM;i=i+1) {
    int new_sub = i-1;
    list[new_sub] = list[i];
  }
  // ひとつ空くのでそこに入れる
  list[GYRO_Y_AVR_NUMM1] = abs(arg_gyroY);
}

float calc_gyroY_avr(float list[GYRO_Y_AVR_NUM]) {
  float sum = 0.0;
  for (int i=0;i<GYRO_Y_AVR_NUM;i=i+1) {
    sum += list[i];
  }
  return sum/GYRO_Y_AVR_NUM;
}

void sendCommandToIoTCloud(String status) {
  String getURL = "http://[your server domain (xxxxxx.com) here]/m5smartlock/set-status/?deviceid="+device_id+"&reqstatus="+status;
  Serial.println(GET_Request(getURL.c_str()));
}

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

void setup() {
  // put your setup code here, to run once:
  M5.begin();
  Serial.begin(115200);
  Wire.begin(0,26);
  M5.MPU6886.Init();
  // 画面の明るさを7-12のうちの8に
  M5.Axp.ScreenBreath(8);
  M5.Lcd.setRotation(3);
  M5.Lcd.setCursor(0,0,2);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int cnt=0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //M5.Lcd.print(".");
    // tnx to https://msr-r.net/m5stickc-wifi-error/
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
  M5.Lcd.setRotation(2);
}

void loop() {
  M5.MPU6886.getGyroData(&gyroX,&gyroY,&gyroZ);
  M5.update();
  // 本体の前面ボタンが押されたときにfirebaseを書き換える。
  if (M5.BtnA.wasPressed()) {
    if (isTimerStarted && startMills!=0) {
      isSendCommand = false;
      isTimerStarted = false;
      startMills = 0;
    }
    if (is_locking) {
      // {"status": "UNLOCKED"}
      sendCommandToIoTCloud("UNLOCKED");
    } else {
      // {"status": "LOCKED"}
      sendCommandToIoTCloud("LOCKED");
    }
  }
  if (is_notmoved) {
    // delayを挟む必要がある tnx to https://ppdr.softether.net/smartlock-2
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
  // GYRO_Y_AVR_NUM個の値を保持できる配列にgyroYをどんどんpushしていく
  add_gyroY_list(gyroY_list, gyroY);
  if (add_count%GYRO_Y_AVR_NUM==0) {
    // その配列の平均を算出
    float avr = calc_gyroY_avr(gyroY_list);
    // GYRO_Y_TRESHOLDを超えていたとき
    if (avr>=GYRO_Y_TRESHOLD && !isSendCommand) {
      // 開いたとしてUNLOCK処理する
      open_door();
      //
      isSendCommand = true;
      // 閾値秒後に発火するイベントを設定
      if (!isTimerStarted && startMills==0) {
        M5.Lcd.setCursor(0,20,2);
        M5.Lcd.println("sec to auto lock");
        isTimerStarted = true;
        startMills = millis();
      }
    }
  }
  add_count++;
  if (isTimerStarted && startMills!=0) {
    unsigned long nowMills = millis();
    unsigned long diffMills = nowMills-startMills;
    int diffSec = diffMills/1000;
    int remainingSec = AUTO_LOCK_WAIT_SEC-diffSec;
    M5.Lcd.fillRect(0,0,240,19,0);
    M5.Lcd.drawNumber(remainingSec, 0, 0, 4);
    // 30秒が経過したら
    if (diffMills>=AUTO_LOCK_WAIT_SEC*1000) {
      isSendCommand = false;
      isTimerStarted = false;
      startMills = 0;
      // 自動的にロック
      lock_door();
      //
    }
  }
  delay(1);
}
