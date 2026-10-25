#include <Arduino.h>
#include "config.h" // src/config.h
#include <Discord_Client.h> // lib/DiscordClient/Discord_Client.h
#include <VibrationSensor6050.h> // lib/VibrationSensor_MPU6050/VibrationSensor6050.h
#include <SleepMode.h> // lib/SleepMode/SleepMode.h
#include <sensorcheck.h> // lib/sensorcheck/sensorcheck.h
#include <NetworkSystem.h> // lib/NetworkSystem/NetworkSystem.h
#include <WasherJudge.h>
#include <CyclePolicy.h>

const char* NTP_SERVER = "ntp.nict.jp"; // NTPサーバーのアドレス
const long UTC = 9 * 3600; // 日本時間
#if defined(A3)
const int BATT_ADC_PIN = A3; // バッテリ残量を見るピン(あとで実装する)
#else
const int BATT_ADC_PIN = D3;
#endif
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

  const WasherJudgeConfig cfg = washerJudgeDefaultConfig();
  VibrationWindow window = sensor.sampleWindow(offsetIntencity, WINDOW_SAMPLES, WINDOW_INTERVAL_MS,
                                               cfg.openDoorDeg);
  WindowStats stats;
  stats.ok = window.ok;
  stats.rms = window.rms;
  stats.peak = window.peak;
  stats.tiltDeg = window.tiltDeg;
  stats.sampleCount = window.successCount;
  stats.openCount = window.openCount;

  const WasherJudgeState previous = loadJudgeState();
  WasherJudgeResult result = WasherJudge::step(previous, stats, monotonicSec, cfg);

  if (result.updateOffset && window.ok) {
    offsetIntencity = window.meanRaw;
    offsetValid = true;
  }

  bool sent = true;
  if (result.action == WASHER_ACTION_THANK_HUNG) {
    sent = Discord.sendMessage("洗濯物を干してくれたんだね!!ありがとう!!", true);
  } else if (result.action == WASHER_ACTION_REMIND_HANG) {
    sent = Discord.sendMessage("お洗濯は終わってるみたいだよ!洗濯物を干してね!", false);
  }

  const NotifyCommit committed = commitNotify(
      result.state, result.sleepSec, previous, result.action, sent, cfg.sleepIdleSec);
  persistJudgeState(committed.state);

  Serial.printf("phase=%u rms=%.3f tilt=%.1f door=%d action=%u offset=%.3f sleep=%lu\n",
                (unsigned)washerPhase, window.rms, window.tiltDeg,
                washerDoorOpen(stats) ? 1 : 0, (unsigned)result.action,
                offsetIntencity, (unsigned long)committed.sleepSec);

  monotonicSec += committed.sleepSec;
  sleepmodeobj.gotosleep((int)committed.sleepSec);
}

void waikeup(uint8_t phase) {
  Serial.begin(SELIAL_PORT);

  if (network.connect_wifi()) {
    if (!sleepmodeobj.updateLocalTime())
    {
      Discord.sendMessage("ごめんね。時刻がわからなかったよ", false);
      network.disconnect_wifi();
    }else {
      int hour = 0;
      if (sleepmodeobj.localHour(hour) &&
          shouldNightSleep(phase, hour, STERT_HOUR, END_HOUR)) {
        sleepmodeobj.deepsleep(STERT_HOUR, END_HOUR, &monotonicSec);
      } else if (phase == WASHER_IDLE) {
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
