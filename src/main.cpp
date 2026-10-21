#include <Arduino.h>
#include "config.h" // src/config.h
#include <Discord_Client.h> // lib/DiscordClient/Discord_Client.h
#include <VibrationSensor6050.h> // lib/VibrationSensor_MPU6050/VibrationSensor6050.h
#include <SleepMode.h> // lib/SleepMode/SleepMode.h
#include <sensorcheck.h> // lib/sensorcheck/sensorcheck.h
#include <NetworkSystem.h> // lib/NetworkSystem/NetworkSystem.h
#include <WasherJudge.h>

const char* NTP_SERVER = "ntp.nict.jp"; // NTPサーバーのアドレス
const long UTC = 9 * 3600; // 日本時間
const int BATT_ADC_PIN = A3; // バッテリ残量を見るピン(あとで実装する)
float getBatteryVoltage(); // バッテリー残量
void waikeup(uint8_t phase); // 起床後の処理

// インスタンス化
DiscordClient Discord(DISCORD_WEBHOOK); // Discordに送信するよう
NetworkSystem network(SECRET_SSID, SECRET_PASSWORD); // wifiに接続する
VibrationSensor6050 sensor; // センサーからのフィードバック
SensorCheck check; // センサーの存在を確認する
SleepMode sleepmodeobj(NTP_SERVER, UTC, DISCORD_WEBHOOK); // スリープモードの管理

//RTCメモリ保持変数
RTC_DATA_ATTR uint8_t washerPhase = WASHER_IDLE;
RTC_DATA_ATTR int startCount = 0;
RTC_DATA_ATTR uint32_t washStartSec = 0;
RTC_DATA_ATTR uint32_t quietSinceSec = 0;
RTC_DATA_ATTR uint32_t finishTimeSec = 0;
RTC_DATA_ATTR uint32_t monotonicSec = 0;
RTC_DATA_ATTR bool bootCount = false;
RTC_DATA_ATTR float offsetIntencity = 0;
RTC_DATA_ATTR bool offsetValid = false;

const float opendoor = 7.0; // 洗濯機のドアが開いているとみなす角度(要調整)
const int SELIAL_PORT = 115200; // シリアル通信の速度
const int WINDOW_SAMPLES = 60;
const int WINDOW_INTERVAL_MS = 50;
int STERT_HOUR = 8; // 起床時間
int END_HOUR = 18; // 就寝時間

static void persistJudgeState(const WasherJudgeState& state) {
  washerPhase = state.phase;
  startCount = state.startCount;
  washStartSec = state.washStartSec;
  quietSinceSec = state.quietSinceSec;
  finishTimeSec = state.finishTimeSec;
}

static WasherJudgeState loadJudgeState() {
  WasherJudgeState state;
  state.phase = washerPhase;
  state.startCount = startCount;
  state.washStartSec = washStartSec;
  state.quietSinceSec = quietSinceSec;
  state.finishTimeSec = finishTimeSec;
  return state;
}

void setup() {
  waikeup(washerPhase);

  if (!offsetValid) {
    offsetIntencity = sensor.offset();
    offsetValid = true;
  }

  VibrationWindow window = sensor.sampleWindow(offsetIntencity, WINDOW_SAMPLES, WINDOW_INTERVAL_MS);
  WindowStats stats;
  stats.ok = window.ok;
  stats.rms = window.rms;
  stats.peak = window.peak;
  stats.lastY = window.lastY;

  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  const bool doorOpen = window.ok && window.lastY > opendoor;
  WasherJudgeResult result = WasherJudge::step(loadJudgeState(), stats, monotonicSec, doorOpen, cfg);

  if (result.updateOffset && window.ok) {
    offsetIntencity = window.meanRaw;
    offsetValid = true;
  }

  persistJudgeState(result.state);

  if (result.action == WASHER_ACTION_THANK_HUNG) {
    Discord.sendMessage("洗濯物を干してくれたんだね!!ありがとう!!", true);
  } else if (result.action == WASHER_ACTION_REMIND_HANG) {
    Discord.sendMessage("お洗濯は終わってるみたいだよ!洗濯物を干してね!", false);
  }

  Serial.printf("phase=%u rms=%.3f offset=%.3f sleep=%u\n",
                (unsigned)washerPhase, window.rms, offsetIntencity, result.sleepSec);

  monotonicSec += result.sleepSec;
  sleepmodeobj.gotosleep((int)result.sleepSec);
}

void waikeup(uint8_t phase) {
  Serial.begin(SELIAL_PORT);

  if (network.connect_wifi()) {
    if (!sleepmodeobj.updateLocalTime())
    {
      Discord.sendMessage("ごめんね。時刻がわからなかったよ", false);
    }else {
      if(!sleepmodeobj.deepsleep(STERT_HOUR, END_HOUR, &monotonicSec) && phase == WASHER_IDLE) {
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
}

void loop (){}

float getBatteryVoltage()
{
  int raw = analogRead(BATT_ADC_PIN);
  return (raw * 3.3 / 4095.0) * 2.0;
}
