
// ============================================================================
// 文化祭デモ用 demo.cpp 改修案（MQTT版）
// ----------------------------------------------------------------------------
// 既存の src/demo.cpp をベースに、以下の3点だけを足したものです。
//   1. 判定イベント（振動開始／停止＝洗濯終了）を MQTT で PC へ送る
//   2. 蓋の開閉判定を有効にする（本番 WasherJudge と同じ物差しを流用）
//   3. 蓋が開いている間は振動判定を止める（★重要・理由は下記）
//
// ★なぜ 3 が必要か:
//   蓋を開けると重力の向きが変わり、intensity は約 19.6 まで跳ね上がります。
//   開始しきい値（RMS 1.7）をはるかに超えるため、そのまま判定にかけると
//   「蓋を開けただけ」で振動開始と誤判定し、さらにアイドル時の
//   オフセット更新で基準値まで壊れます。本番ファームが終了判定の前に
//   蓋チェックを挟んでいるのと同じ理由です。
//
// 蓋判定の物差し（本番と同一）:
//   sampleWindow() が返す openCount（傾き30°以上のサンプル数）が
//   成功サンプル数の過半数なら「開」。window.tiltDeg は中央値。
//   ※ sampleWindow / VibrationWindow は既存のものをそのまま使う。
//
// 使い方:
//   - platformio.ini の [env:demo]（コメントアウト中）を有効化し、
//     lib_deps に PubSubClient を足す:
//         lib_deps =
//             electroniccats/MPU6050
//             knolleary/PubSubClient
//   - src/config.h に下の「config.h への追加分」を足す
//   - このファイルを src/demo.cpp と差し替える（差分確認は Git で）
//
// MQTT トピック設計:
//   sentakuki/demo/wash_state  (retained) "idle" / "running" / "finished"
//   sentakuki/demo/door_state  (retained) "open" / "closed"
//   sentakuki/demo/event       (通常)     "running_started" / "wash_finished"
//                                         / "door_opened" / "door_closed"
//   状態トピックを retained にしておくことで、アプリを後から起動しても
//   現在の状態がすぐ表示される。サイクルのリセットは ESP32 側で行う:
//   finished のあと蓋が「開→閉」したら wash_state を idle に戻す。
//
// Discord について:
//   会場ネットが不確実なため、既定では無効（DEMO_USE_DISCORD 0）。
//   MQTT と併用したい場合は 1 に変える。ただし HTTPS 送信のタイムアウトが
//   1秒窓のループを引っ張る場合があるので、本番当日は 0 を推奨。
// ============================================================================

#include <Arduino.h>
#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <Discord_Client.h>
#include <VibrationSensor6050.h>
#include <NetworkSystem.h>
#include <DemoVibrationJudge.h>

#define DEMO_USE_DISCORD 0

// ---- config.h への追加分（例） -------------------------------------------
// static constexpr const char* MQTT_BROKER_IP = "192.168.137.1";  // WindowsホットスポットのPC
// static constexpr int         MQTT_BROKER_PORT = 1883;
// static constexpr const char* MQTT_CLIENT_ID = "sentakuki-esp32";
// ----------------------------------------------------------------------------

const int SERIAL_PORT = 115200;
const int WINDOW_SAMPLES = 20;
const int WINDOW_INTERVAL_MS = 50;

const char* TOPIC_WASH_STATE = "sentakuki/demo/wash_state";
const char* TOPIC_DOOR_STATE = "sentakuki/demo/door_state";
const char* TOPIC_EVENT      = "sentakuki/demo/event";

DiscordClient Discord(DISCORD_WEBHOOK);
NetworkSystem network(SECRET_SSID, SECRET_PASSWORD);
VibrationSensor6050 sensor;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

float offsetIntensity = 0;
DemoVibrationState judgeState;

// 蓋判定の状態（2窓連続で一致したら確定＝チャタリング防止）
bool doorOpenConfirmed = false;   // 確定した蓋状態
bool doorCandidate = false;       // 候補（前窓の生判定）
bool washFinished = false;        // finished を表示中か（リセット判定用）
unsigned long lastMqttRetryMs = 0;

bool rawDoorOpen(const VibrationWindow& window) {
  // 本番 washerDoorOpen() と同じ: openCount が成功数の過半数
  return window.ok && (window.openCount * 2) > window.successCount;
}

void mqttPublish(const char* topic, const char* payload, bool retained) {
  if (mqtt.connected()) {
    mqtt.publish(topic, payload, retained);
  }
  Serial.printf("mqtt> %s = %s%s\n", topic, payload, retained ? " (retained)" : "");
}

void mqttEnsureConnected() {
  if (mqtt.connected()) {
    return;
  }
  const unsigned long now = millis();
  if (now - lastMqttRetryMs < 5000) {
    return;  // 再接続は最大5秒に1回（1秒窓の判定を止めない）
  }
  lastMqttRetryMs = now;
  mqtt.setServer(MQTT_BROKER_IP, MQTT_BROKER_PORT);
  if (mqtt.connect(MQTT_CLIENT_ID)) {
    Serial.println("mqtt connected");
    // 接続直後に自分の知っている状態を流し直す
    mqttPublish(TOPIC_DOOR_STATE, doorOpenConfirmed ? "open" : "closed", true);
    mqttPublish(TOPIC_WASH_STATE,
                judgeState.phase == DEMO_VIBRATING ? "running"
                : washFinished                     ? "finished"
                                                   : "idle",
                true);
  } else {
    Serial.printf("mqtt connect failed rc=%d\n", mqtt.state());
  }
}

void setup() {
  Serial.begin(SERIAL_PORT);
  judgeState.phase = DEMO_IDLE;
  judgeState.quietCount = 0;

  network.connect_wifi();
  sensor.setup();
  if (!sensor.sensor_exsis()) {
#if DEMO_USE_DISCORD
    Discord.sendMessage("センサーがつながってないよ!!!", false, false);
#endif
    Serial.println("sensor not found");
  }
  offsetIntensity = sensor.offset();

  mqttEnsureConnected();
  mqttPublish(TOPIC_WASH_STATE, "idle", true);
  mqttPublish(TOPIC_DOOR_STATE, "closed", true);
  mqttPublish(TOPIC_EVENT, "demo_ready", false);

#if DEMO_USE_DISCORD
  Discord.sendMessage("接続完了! 文化祭デモで振動を見るね!", true, false);
#endif
}

void loop() {
  mqttEnsureConnected();
  mqtt.loop();

  VibrationWindow window = sensor.sampleWindow(offsetIntensity, WINDOW_SAMPLES, WINDOW_INTERVAL_MS);

  // ---- 蓋判定（振動していない時だけ有効） --------------------------------
  // 振動中は加速度が荒れて傾きが信用できないため、本番同様に判定しない。
  if (window.ok && judgeState.phase == DEMO_IDLE) {
    const bool raw = rawDoorOpen(window);
    if (raw == doorCandidate && raw != doorOpenConfirmed) {
      // 2窓連続で同じ生判定 → 確定
      doorOpenConfirmed = raw;
      if (doorOpenConfirmed) {
        mqttPublish(TOPIC_DOOR_STATE, "open", true);
        mqttPublish(TOPIC_EVENT, "door_opened", false);
      } else {
        mqttPublish(TOPIC_DOOR_STATE, "closed", true);
        mqttPublish(TOPIC_EVENT, "door_closed", false);
        if (washFinished) {
          // 終了後に蓋が開→閉 = 洗濯物を取り出した → 次の来場者へリセット
          washFinished = false;
          mqttPublish(TOPIC_WASH_STATE, "idle", true);
        }
      }
    }
    doorCandidate = raw;
  }

  // ---- 蓋が開いている間は振動判定をしない（誤判定とオフセット破壊の防止）
  if (doorOpenConfirmed) {
    Serial.printf("door=open (judge paused) tilt=%.1f open=%d/%d\n",
                  window.tiltDeg, window.openCount, window.successCount);
    return;
  }

  // ---- 振動判定（既存 DemoVibrationJudge をそのまま使用） ----------------
  const DemoVibrationConfig cfg = demoVibrationDefaultConfig();
  DemoVibrationResult result = DemoVibrationJudge::step(
      judgeState, window.rms, window.ok, cfg);
  judgeState = result.state;

  if (result.updateOffset && window.ok) {
    offsetIntensity = window.meanRaw;
  }

  if (result.action == DEMO_ACTION_START) {
    mqttPublish(TOPIC_WASH_STATE, "running", true);
    mqttPublish(TOPIC_EVENT, "running_started", false);
#if DEMO_USE_DISCORD
    Discord.sendMessage("振動を検知したよ!", true, false);
#endif
  } else if (result.action == DEMO_ACTION_STOP) {
    washFinished = true;
    mqttPublish(TOPIC_WASH_STATE, "finished", true);
    mqttPublish(TOPIC_EVENT, "wash_finished", false);
#if DEMO_USE_DISCORD
    Discord.sendMessage("振動が止まったよ!", false, false);
#endif
  }

  Serial.printf("phase=%u rms=%.3f offset=%.3f tilt=%.1f door=%d action=%u\n",
                (unsigned)judgeState.phase, window.rms, offsetIntensity,
                window.tiltDeg, doorOpenConfirmed ? 1 : 0,
                (unsigned)result.action);
}

