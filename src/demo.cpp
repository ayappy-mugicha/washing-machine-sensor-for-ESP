#include <Arduino.h>
#include "config.h"
#include <Discord_Client.h>
#include <VibrationSensor6050.h>
#include <NetworkSystem.h>
#include <DemoVibrationJudge.h>

const int SERIAL_PORT = 115200;
const int WINDOW_SAMPLES = 20;
const int WINDOW_INTERVAL_MS = 50;

DiscordClient Discord(DISCORD_WEBHOOK);
NetworkSystem network(SECRET_SSID, SECRET_PASSWORD);
VibrationSensor6050 sensor;

float offsetIntensity = 0;
DemoVibrationState judgeState;

void setup() {
  Serial.begin(SERIAL_PORT);
  judgeState.phase = DEMO_IDLE;
  judgeState.quietCount = 0;

  network.connect_wifi();
  sensor.setup();
  if (!sensor.sensor_exsis()) {
    Discord.sendMessage("センサーがつながってないよ!!!", false, false);
  }
  offsetIntensity = sensor.offset();
  Discord.sendMessage("接続完了! 文化祭デモで振動を見るね!", true, false);
}

void loop() {
  VibrationWindow window = sensor.sampleWindow(offsetIntensity, WINDOW_SAMPLES, WINDOW_INTERVAL_MS);
  const DemoVibrationConfig cfg = demoVibrationDefaultConfig();
  DemoVibrationResult result = DemoVibrationJudge::step(
      judgeState, window.rms, window.ok, cfg);
  judgeState = result.state;

  if (result.updateOffset && window.ok) {
    offsetIntensity = window.meanRaw;
  }

  if (result.action == DEMO_ACTION_START) {
    Discord.sendMessage("振動を検知したよ!", true, false);
  } else if (result.action == DEMO_ACTION_STOP) {
    Discord.sendMessage("振動が止まったよ!", false, false);
  }

  Serial.printf("phase=%u rms=%.3f offset=%.3f action=%u\n",
                (unsigned)judgeState.phase, window.rms, offsetIntensity,
                (unsigned)result.action);
}
