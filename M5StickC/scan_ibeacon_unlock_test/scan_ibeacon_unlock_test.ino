#include <M5Atom.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define WIFI_SSID "[SSID here]"
#define WIFI_PASSWORD "[WIFI PW here]"
const String device_id = "[your unique device id here]";

// 20台までとする
#define BLE_DEVICE_NUM_LIMIT 20
// 許可するUUIDの個数
#define ALLOW_USER_LIST_COUNT 3
// 許可するUUID
String allowUuidsList[] = {"XXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX", "XXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX", "XXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"};
String uuidsList[BLE_DEVICE_NUM_LIMIT][2];
String beforeUuidsList[BLE_DEVICE_NUM_LIMIT][2];
int listPointer = 0;
int beforeUuidListPointer = 0;
bool isCallbacked = false;
bool isFirstCall = false;

class IBeaconAdvertised: public BLEAdvertisedDeviceCallbacks {
  public:
    // BLE検出時のコールバック
    void onResult(BLEAdvertisedDevice device) {
      if (!isIBeacon(device)) {
        return;
      }
      // 距離については無視してよき？
      String uuid = getUuid(device).c_str();
      bool isContainUUID = false;
      for (int i=0;i<BLE_DEVICE_NUM_LIMIT;i++) {
        if (uuidsList[i][0] == uuid) {
          isContainUUID = true;
          break;
        }
      }
      if (!isContainUUID) {
        uuidsList[listPointer][0] = uuid;
        uuidsList[listPointer][1] = millis();
        listPointer++;
      }
      isCallbacked = true;
    }

  private:
    // iBeaconパケット判定
    bool isIBeacon(BLEAdvertisedDevice device) {
      if (device.getManufacturerData().length() < 25) {
        return false;
      }
      if (getCompanyId(device) != 0x004C) {
        return false;
      }
      if (getIBeaconHeader(device) != 0x1502) {
        return false;
      }
      return true;
    }

    // CompanyId取得
    unsigned short getCompanyId(BLEAdvertisedDevice device) {
      const unsigned short* pCompanyId = (const unsigned short*)&device
                                         .getManufacturerData()
                                         .c_str()[0];
      return *pCompanyId;
    }

    // iBeacon Header取得
    unsigned short getIBeaconHeader(BLEAdvertisedDevice device) {
      const unsigned short* pHeader = (const unsigned short*)&device
                                      .getManufacturerData()
                                      .c_str()[2];
      return *pHeader;
    }

    // UUID取得
    String getUuid(BLEAdvertisedDevice device) {
      const char* pUuid = &device.getManufacturerData().c_str()[4];
      char uuid[64] = {0};
      sprintf(
        uuid,
        "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        pUuid[0], pUuid[1], pUuid[2], pUuid[3], pUuid[4], pUuid[5], pUuid[6], pUuid[7],
        pUuid[8], pUuid[9], pUuid[10], pUuid[11], pUuid[12], pUuid[13], pUuid[14], pUuid[15]
      );
      return String(uuid);
    }

    // TxPower取得
    signed char getTxPower(BLEAdvertisedDevice device) {
      const signed char* pTxPower = (const signed char*)&device
                                    .getManufacturerData()
                                    .c_str()[24];
      return *pTxPower;
    }

    // iBeaconの情報をシリアル出力
    void printIBeacon(BLEAdvertisedDevice device) {
      Serial.printf("addr:%s rssi:%d uuid:%s power:%d\r\n",
                    device.getAddress().toString().c_str(),
                    device.getRSSI(),
                    getUuid(device).c_str(),
                    *(signed char*)&device.getManufacturerData().c_str()[24]);
    }
};

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
  String getURL = "http://[your server domain (xxxxxx.com) here]/set-status/?deviceid="+device_id+"&reqstatus="+status;
  Serial.println(GET_Request(getURL.c_str()));
}

void unlock() {
  Serial.println("unlock");
  sendCommandToIoTCloud("UNLOCKED");
}

void setup() {
  M5.begin(true, false, true);
  delay(100);
  Serial.begin(115200);
  delay(100);
  BLEDevice::init("");
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int cnt=0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    cnt++;
    if(cnt%10==0) {
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    if(cnt>=30) {
      ESP.restart();
    }
  }
  delay(100);
  M5.dis.drawpix(0, 0xa5ff00);
}

void loop() {
  Serial.println("--- start ---");
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new IBeaconAdvertised(), true);
  scan->setActiveScan(true);
  // 追いつかないことがあるので慎重に。
  scan->start(2);
  // BLEデバイスを認識していたら
  if (isCallbacked) {
    // uuidsListを取り出し
    for (int i=0;i<listPointer;i++) {
      String uuid = uuidsList[i][0];
      if (uuid=="") {
        break;
      }
      String timestamp = uuidsList[i][1];
      Serial.print(uuid);
      Serial.print(" : ");
      Serial.print(timestamp);
      Serial.print(" : ");
      bool isExistUUID = false;
      // String beforeTimestamp = "";
      // 前回のuuidsListを取り出し
      for (int j=0;j<beforeUuidListPointer;j++) {
        if (beforeUuidsList[j][0] == uuid) {
          // beforeTimestamp = beforeUuidsList[j][1];
          Serial.print(beforeUuidsList[j][1]);
          isExistUUID = true;
          break;
        }
      }
      if (isFirstCall) {
        if (!isExistUUID) {
          for (int k=0;k<ALLOW_USER_LIST_COUNT;k++) {
            if (allowUuidsList[k] == uuid) {
              unlock();
              break;
            }
          }
          break;
        }
      }
      Serial.println("");
    }
    // beforeUuidListにコピー
    for (int k=0;k<listPointer;k++) {
      beforeUuidsList[k][0] = uuidsList[k][0];
      beforeUuidsList[k][1] = uuidsList[k][1];
    }
    beforeUuidListPointer = listPointer;
    // uuidslistを初期化
    memset(uuidsList, 0, sizeof(uuidsList));
    listPointer = 0;
    isFirstCall = true;
    isCallbacked = false;
  } else {
    // listPointerが0個のとき、callbackもされず、beforeUuidsListが書き換わらないので全消し
    if (listPointer==0) {
      memset(beforeUuidsList, 0, sizeof(beforeUuidsList));
      beforeUuidListPointer = 0;
    }
  }
  Serial.println("--- complete ---");
}
