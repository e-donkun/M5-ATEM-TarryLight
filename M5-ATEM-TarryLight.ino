/*
  M5-ATEM-TarryLight
  Copyright (C) 2020 Jun SUZUKI

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <M5StickC.h>
#include <WiFi.h>
#include <SkaarhojPgmspace.h>
#include <ATEMbase.h>
#include <ATEMstd.h>

#include "arduino_secrets.h"


// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

// IP address of the ATEM switcher.
static const IPAddress kSwitcherIp(192, 168, 24, 210);

// Camera inputs to cycle through with the front button. ATEM Mini and
// Mini Pro have four; raise this for a switcher with more inputs.
static const uint8_t kCameraCount = 4;

// Verbosity of the ATEM library on the serial port. 0x80 is verbose.
static const uint8_t kAtemSerialOutput = 0;

// Timings, in milliseconds.
static const uint32_t kWiFiConnectTimeout = 15000;
static const uint32_t kWiFiRetryInterval = 10000;
static const uint32_t kInfoRefreshInterval = 500;
static const uint32_t kLongPressTime = 2000;

// Demo mode walks through the tally states so the light can be shown off
// without a switcher. This is how long each state stays on screen.
static const uint32_t kDemoStepInterval = 2000;

// Colours in RGB565, defined here so the sketch does not depend on the colour
// macros of whichever display library version happens to be installed.
// http://www.barth-dev.de/online/rgb565-color-picker/
static const uint16_t kColorProgram = 0xF800;     // 255   0   0
static const uint16_t kColorPreview = 0x07E0;     //   0 255   0
static const uint16_t kColorIdle = 0xFFFF;        // 255 255 255
static const uint16_t kColorTallyLabel = 0x0000;  //   0   0   0
static const uint16_t kColorIdleLabel = 0x0020;   //   8   8   8
static const uint16_t kColorTextBg = 0x0000;
static const uint16_t kColorText = 0xFFFF;

// The internal red LED of the M5StickC is active low.
static const uint8_t kLedPin = 10;
static const uint8_t kLedOn = LOW;
static const uint8_t kLedOff = HIGH;

// The tally label is drawn in portrait, text screens in landscape.
static const uint8_t kRotationTally = 0;
static const uint8_t kRotationText = 3;  // BtnB is on top.

static const char *const kWiFiSsid = SECRET_SSID;
static const char *const kWiFiPassword = SECRET_PASS;


// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

enum TallyState : uint8_t {
  kTallyIdle,
  kTallyPreview,
  kTallyProgram,
};

// What is currently on the screen. kScreenNone forces the next update to
// redraw, which is also how the very first frame gets drawn.
enum Screen : uint8_t {
  kScreenNone,
  kScreenTally,
  kScreenNoWiFi,
  kScreenNoAtem,
  kScreenInfo,
};

ATEMstd AtemSwitcher;

// The states demo mode cycles through, in order.
static const TallyState kDemoSequence[] = {kTallyIdle, kTallyPreview, kTallyProgram};
static const uint8_t kDemoSequenceLength =
    sizeof(kDemoSequence) / sizeof(kDemoSequence[0]);

static uint8_t cameraNumber = 1;
static Screen currentScreen = kScreenNone;
static TallyState currentTally = kTallyIdle;
static bool infoMode = false;
static bool demoMode = false;

static uint32_t lastWiFiAttempt = 0;
static uint32_t lastInfoRefresh = 0;
static uint32_t demoSteppedAt = 0;
static uint8_t demoStep = 0;
static uint32_t btnAPressedAt = 0;
static bool btnALongPressDone = false;
static uint32_t btnBPressedAt = 0;
static bool btnBLongPressDone = false;


// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

static void drawTally(TallyState state) {
  uint16_t background = kColorIdle;
  uint16_t label = kColorIdleLabel;
  uint8_t led = kLedOff;

  if (state == kTallyProgram) {
    background = kColorProgram;
    label = kColorTallyLabel;
    led = kLedOn;
  } else if (state == kTallyPreview) {
    background = kColorPreview;
    label = kColorTallyLabel;
  }

  digitalWrite(kLedPin, led);
  M5.Lcd.setRotation(kRotationTally);
  M5.Lcd.fillScreen(background);
  M5.Lcd.setTextColor(label, background);
  M5.Lcd.drawString(String(cameraNumber), 15, 40, 8);

  // The colours are the real ones, so say when they are being faked.
  if (demoMode) {
    M5.Lcd.drawString("DEMO", 18, 135, 2);
  }
}

static void drawTextScreen(const char *title, const String &detail) {
  digitalWrite(kLedPin, kLedOff);
  M5.Lcd.setRotation(kRotationText);
  M5.Lcd.fillScreen(kColorTextBg);
  M5.Lcd.setTextColor(kColorText, kColorTextBg);
  M5.Lcd.setCursor(1, 1);
  M5.Lcd.println(title);
  M5.Lcd.println(detail);
  M5.Lcd.printf("Cam %d", cameraNumber);
}

static uint8_t batteryPercent() {
  // Rough linear estimate over the usable range of the cell. The AXP192
  // reports the battery voltage in units of 1.1 mV.
  const float volts = M5.Axp.GetVbatData() * 1.1f / 1000.0f;
  float percent = (volts - 3.2f) / (4.15f - 3.2f) * 100.0f;

  if (percent > 100.0f) {
    percent = 100.0f;
  } else if (percent < 0.0f) {
    percent = 0.0f;
  }
  return (uint8_t)percent;
}


// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------

static TallyState readTally() {
  // Program wins over preview: on a cut both flags can be set for the same
  // input, and in that case the camera is on air.
  if (AtemSwitcher.getProgramTally(cameraNumber)) {
    return kTallyProgram;
  }
  if (AtemSwitcher.getPreviewTally(cameraNumber)) {
    return kTallyPreview;
  }
  return kTallyIdle;
}

static void showTally(TallyState state) {
  if (currentScreen == kScreenTally && currentTally == state) {
    return;
  }
  currentScreen = kScreenTally;
  currentTally = state;
  drawTally(state);
}

static TallyState demoTally() {
  const uint32_t now = millis();
  if (now - demoSteppedAt >= kDemoStepInterval) {
    demoSteppedAt = now;
    demoStep = (demoStep + 1) % kDemoSequenceLength;
  }
  return kDemoSequence[demoStep];
}

static void setDemoMode(bool enabled) {
  demoMode = enabled;
  demoStep = 0;
  demoSteppedAt = millis();
  infoMode = false;
  currentScreen = kScreenNone;
}

static void showTextScreen(Screen screen, const char *title, const String &detail) {
  if (currentScreen == screen) {
    return;
  }
  currentScreen = screen;
  drawTextScreen(title, detail);
}

static void showInfo() {
  const uint32_t now = millis();

  if (currentScreen != kScreenInfo) {
    currentScreen = kScreenInfo;
    digitalWrite(kLedPin, kLedOff);
    M5.Lcd.setRotation(kRotationText);
    M5.Lcd.fillScreen(kColorTextBg);
    lastInfoRefresh = now - kInfoRefreshInterval;  // draw right away
  }
  if (now - lastInfoRefresh < kInfoRefreshInterval) {
    return;
  }
  lastInfoRefresh = now;

  // The fields are padded so that a shorter value overwrites a longer one.
  M5.Lcd.setTextColor(kColorText, kColorTextBg);
  M5.Lcd.setCursor(1, 1);
  const char *link = demoMode ? "DEMO"
                              : (AtemSwitcher.isConnected() ? "ATEM up" : "ATEM down");
  M5.Lcd.printf("Cam %d   %-9s\n", cameraNumber, link);
  M5.Lcd.printf("IP  %-15s\n", WiFi.localIP().toString().c_str());
  M5.Lcd.printf("MAC %s\n", WiFi.macAddress().c_str());
  M5.Lcd.printf("Bat %3d%%\n", batteryPercent());
}


// ---------------------------------------------------------------------------
// Connections
// ---------------------------------------------------------------------------

static bool connectWiFi() {
  M5.Lcd.setRotation(kRotationText);
  M5.Lcd.fillScreen(kColorTextBg);
  M5.Lcd.setTextColor(kColorText, kColorTextBg);
  M5.Lcd.setCursor(1, 1);
  M5.Lcd.println("Connecting to");
  M5.Lcd.println(kWiFiSsid);
  Serial.printf("Connecting to %s\n", kWiFiSsid);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // modem sleep adds latency to the tally updates
  WiFi.begin(kWiFiSsid, kWiFiPassword);

  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - started < kWiFiConnectTimeout) {
    delay(250);
    M5.Lcd.print(".");
    Serial.print(".");
  }
  Serial.println();

  lastWiFiAttempt = millis();
  currentScreen = kScreenNone;
  return WiFi.status() == WL_CONNECTED;
}

static void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  const uint32_t now = millis();
  if (now - lastWiFiAttempt < kWiFiRetryInterval) {
    return;
  }
  lastWiFiAttempt = now;

  Serial.println("WiFi lost, retrying");
  WiFi.disconnect();
  WiFi.begin(kWiFiSsid, kWiFiPassword);
}

static void reconnect() {
  drawTextScreen("Reconnecting", kWiFiSsid);
  currentScreen = kScreenNone;

  WiFi.disconnect();
  connectWiFi();
  AtemSwitcher.connect();
}


// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

static void handleButtons() {
  // BtnA: short press selects the next camera, holding it starts or stops
  // demo mode. As with BtnB the short press is acted on when released, so
  // that a hold does not also step the camera.
  if (M5.BtnA.wasPressed()) {
    btnAPressedAt = millis();
    btnALongPressDone = false;
  }
  if (M5.BtnA.isPressed() && !btnALongPressDone &&
      millis() - btnAPressedAt >= kLongPressTime) {
    btnALongPressDone = true;
    setDemoMode(!demoMode);
  }
  if (M5.BtnA.wasReleased() && !btnALongPressDone) {
    cameraNumber = (cameraNumber % kCameraCount) + 1;
    currentScreen = kScreenNone;  // redraw with the new number
  }

  // BtnB: short press toggles the info screen, holding it reconnects. The
  // press is acted on when released so that a hold does not do both.
  if (M5.BtnB.wasPressed()) {
    btnBPressedAt = millis();
    btnBLongPressDone = false;
  }
  if (M5.BtnB.isPressed() && !btnBLongPressDone &&
      millis() - btnBPressedAt >= kLongPressTime) {
    btnBLongPressDone = true;
    reconnect();
  }
  if (M5.BtnB.wasReleased() && !btnBLongPressDone) {
    infoMode = !infoMode;
    currentScreen = kScreenNone;
  }
}

static void updateDisplay() {
  if (infoMode) {
    showInfo();
  } else if (demoMode) {
    // Deliberately ahead of the connection checks: demo mode is meant to work
    // with no network and no switcher around.
    showTally(demoTally());
  } else if (WiFi.status() != WL_CONNECTED) {
    // Not the same as "camera not selected", so it gets its own screen
    // instead of the white idle tally.
    showTextScreen(kScreenNoWiFi, "No WiFi", WiFi.macAddress());
  } else if (!AtemSwitcher.isConnected()) {
    showTextScreen(kScreenNoAtem, "No ATEM", kSwitcherIp.toString());
  } else {
    showTally(readTally());
  }
}


// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  M5.begin();
  delay(10);

  pinMode(kLedPin, OUTPUT);
  digitalWrite(kLedPin, kLedOff);

  connectWiFi();

  AtemSwitcher.begin(kSwitcherIp);
  AtemSwitcher.serialOutput(kAtemSerialOutput);
  AtemSwitcher.connect();
}

void loop() {
  M5.update();
  handleButtons();
  maintainWiFi();

  // runLoop() answers the switcher, keeps the session alive and reconnects on
  // its own once the link has been quiet for five seconds, so it wants to be
  // called as often as possible.
  if (WiFi.status() == WL_CONNECTED) {
    AtemSwitcher.runLoop();
  }

  updateDisplay();
  delay(1);
}
