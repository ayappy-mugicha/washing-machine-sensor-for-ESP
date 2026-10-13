#include <Arduino.h>
#include "config.h" // src/config.h
#include <Discord_Client.h> // lib/DiscordClient/Discord_Client.h
#include <VibrationSensor6050.h> // lib/VibrationSensor_MPU6050/VibrationSensor6050.h
#include <SleepMode.h> // lib/SleepMode/SleepMode.h
#include <sensorcheck.h> // lib/sensorcheck/sensorcheck.h
#include <NetworkSystem.h> // lib/NetworkSystem/NetworkSystem.h

const char* NTP_SERVER = "ntp.nict.jp"; // NTPサーバーのアドレス
const long UTC = 9 * 3600; // 日本時間
const int BATT_ADC_PIN = A3; // バッテリ残量を見るピン(あとで実装する)
bool snooze(const float angle);
float getBatteryVoltage(); // バッテリー残量
void sleepmode(int second); // espDeepsleep
void wifisetup(); // wifiのセットアップ
float offset(); // センサーの揺れの初期化
void waikeup(bool state); // 起床後の処理
void sendmessage(String message, bool status = true); // Discordにデータを送信する(Discord?_Client)

// インスタンス化
DiscordClient Discord(DISCORD_WEBHOOK); // Discordに送信するよう
NetworkSystem network(SECRET_SSID, SECRET_PASSWORD); // wifiに接続する
VibrationSensor6050 sensor; // センサーからのフィードバック
SensorCheck check; // センサーの存在を確認する
SleepMode sleepmodeobj(NTP_SERVER, UTC, DISCORD_WEBHOOK); // スリープモードの管理

//RTCメモリ保持変数
RTC_DATA_ATTR bool currentState = false; // 洗濯機が動いているかどうかの状態を保持する変数(動きなし:false、動いている:true)
RTC_DATA_ATTR int stopCount = 0; // 洗濯が止まったとみなす回数をカウントする変数(初期値は0、洗濯が止まっているとみなすたびに更新する) STOPLIMITと比較して洗濯が止まったとみなす
RTC_DATA_ATTR bool bootCount = false; // 起動回数をカウントする変数(初期値は0、起動後にtrueにして更新する)
RTC_DATA_ATTR float offsetIntencity = 0; // 洗濯の揺れのオフセット値(初期値は0、起動後にキャリブレーションして更新する)
RTC_DATA_ATTR unsigned long finishTime = 0; // 洗濯が終わったとみなす時間の初期化
RTC_DATA_ATTR bool waitingForSnooze = false; // スヌーズ待ちかどうか (スヌーズ機能オン:true, オフ:false)
// 初期値設定
const float THRESHOLD = 0.70; // THRESHOLD gread
float intencity = 0.0; // 洗濯の揺れの初期化
const int STOP_LIMIT = 3; // 洗濯が止まったとみなす回数
const float opendoor = 7.0; // 洗濯機のドアが開いているとみなす角度(要調整)
const int SELIAL_PORT = 115200; // シリアル通信の速度
int STERT_HOUR = 8; // 起床時間
int END_HOUR = 18; // 就寝時間

void setup() {
  waikeup(currentState); // 起床後の処理
  offsetIntencity = sensor.offset(); // センサーの揺れのオフセット値をキャリブレーション
  WasherStatus comined = sensor.getcombinedData(); // センサーからのデータを取得 インスタンス化
  intencity = abs(comined.intensity - offsetIntencity); // 洗濯の揺れの強さ

  // デバック用
  /* 
  String intencitystr = "intencity: " + String(intencity); 
  Discord.sendMessage(intencitystr.c_str());
  */
  if (currentState) // 洗濯が動いているときは、洗濯が止まったとみなす条件を確認する
  {
    if (intencity < THRESHOLD)
    { // set quitetime        
      stopCount++;
      if (stopCount >= STOP_LIMIT)
      { // end washing
        // String message = String(stopCount) + "回確認したけど、洗濯機は完全に止まってるみたいだね!!";
        stopCount = 0;
        currentState = false;
        waitingForSnooze = true;
        finishTime = millis();
        // Discord.sendMessage(message, true);
      }
      sleepmodeobj.gotosleep(300); // 10min
      // sleepmodeobj.gotosleep(2);
    }
    else
    {
      stopCount = 0;
      Serial.println("洗濯中");
      sleepmodeobj.gotosleep(300); // 5min
      // sleepmodeobj.gotosleep(2);
    }
  }
  else { // 洗濯が動いていないときは、洗濯が開始したとみなす条件を確認する
    if (intencity > THRESHOLD) {// 洗濯が開始したら
      stopCount = 0;
      currentState = true;
      // Discord.sendMessage("お洗濯を開始してるみたいだね!洗濯終わるまで見てるよ!!", true);
      sleepmodeobj.gotosleep(1200); // 20min
      // sleepmodeobj.gotosleep(2);
    }
    else
    {
      if (waitingForSnooze) { // スヌーズ待ちのときは、洗濯物を干したかどうかを確認する
        if(!snooze(comined.y)) {
          waitingForSnooze = false;
          sleepmodeobj.gotosleep(3600); // 30min
          // sleepmodeobj.gotosleep(2);
        }
      }
      // Discord.sendMessage("特に動いていません。電池の減りが気になるからスリープするね!",false);
      sleepmodeobj.gotosleep(300); // 5min
      // sleepmodeobj.gotosleep(2);
    }
  }
}

void waikeup(bool state) {
  float currentV = getApbFrequency();
  String statusMsg = "(電池電圧: " + String(currentV, 2) + "V)";
  Serial.begin(SELIAL_PORT);

  if (network.connect_wifi()) {
    if (!sleepmodeobj.updateLocalTime())
    {
      Discord.sendMessage("ごめんね。時刻がわからなかったよ", false);
    }else {
      if(!sleepmodeobj.deepsleep(STERT_HOUR, END_HOUR) && !state) {
        // sendmessage("深夜ではありません", false);
        Serial.println("深夜ではありません");
      }
      network.disconnect_wifi();
    }
  }

  sensor.setup();
  if (!sensor.sensor_exsis()) {
    Discord.sendMessage("センサーがつながってないよ!!!", false);
  }

  // 初回起動時にキャリブレーション
  if (!bootCount) {
    Discord.sendMessage("接続完了!お洗濯をしてるか見るね!", true);
    bootCount = true;
  }
  // Discord.sendMessage("接続完了!お洗濯をしてるか見るね!", true);
}

void loop (){}

float getBatteryVoltage()
{
  int raw = analogRead(BATT_ADC_PIN);
  return (raw * 3.3 / 4095.0) * 2.0;
}

bool snooze(const float angle)
{
  int snoozetime = 1800000; // 30m
  // int snoozetime = 100;
  Serial.printf("現在の角度: %.2f度\n", angle);
  if ( angle > opendoor ) {
    Discord.sendMessage("洗濯物を干してくれたんだね!!ありがとう!!",true);
    return false;
  }
  else {
    if(millis() - finishTime > snoozetime) {
      finishTime = millis();
      Discord.sendMessage("お洗濯は終わってるみたいだよ!洗濯物を干してね!",false);
      return true;
    }
  }
  return true;

}

// mv lib/DiscordClient/Discord_Client.h src/Discord_Client.h

// void sendmessage(String message, bool status) {
//   float currentV = getBatteryVoltage(); 
//   String statusMsg = "(電池電圧: " + String(currentV, 2) + "V)";
//   if (network.connect_wifi()){
//     String symbol = (status == true) ? "+":"-";
//     String payload = "## 洗濯機君からのお知らせ!! \\n";
//     payload += "```diff\\n";
//     payload += symbol + message + statusMsg + "\\n";
//     payload += "```";

//     Serial.println(payload);
//     Discord.sendMessage(payload);
//     network.disconnect_wifi();
//   } else {
//     Serial.println("Discordに送信できません。wifiに繋がっていません。再接続します");
//     // sendmessage(message,status);
//     network.connect_wifi();
//   }
//   return;
// }