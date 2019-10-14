#include <Adafruit_FeatherOLED.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>

#include "monotonic_ntp.h"

Adafruit_FeatherOLED oled = Adafruit_FeatherOLED();
WiFiManager wifimanager;
WiFiUDP udp;
MonotonicNtp mntp(udp);

void setup() {
  Serial.begin(115200);

  oled.init();
  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.print("Connecting to WiFi...");
  oled.display();

  wifimanager.autoConnect();

  mntp.begin(12 * 60 * 60 * 1000 * 1000ULL); // 24 hours
  // mntp.begin(60 * 60 * 1000 * 1000);  // 1 hour
  // mntp.begin(60 * 1000 * 1000); // 1 min
}

void loop() {
  mntp.loop();

  oled.clearDisplay();
  oled.setCursor(0, 0);

  oled.print("now: ");
  oled.println((double)mntp.now() / 1000000, 3);

  oled.print("tdiff ");
  oled.println((double)mntp.getLastComptimeDelta() / 1000, 3);

  oled.print("sync:");
  oled.println((double)(mntp.getLastComptimeSync() +mntp.getUpdatePeriod())/ 1000000, 3);

  // oled.print("ntp diff: ");
  // oled.println((double)mntp.getLastNtptimeDiff(), 0);

  // oled.print("sys diff: ");
  // oled.println((double)mntp.getLastSystimeDiff(), 0);

  // oled.print("net delay ms:");
  // oled.println((double)mntp.getLastNetworkDelay() / 1000, 3);

  // oled.print("cf:  ");
  // oled.println(mntp.getCompensationFactor(), 9);

  oled.print("ms/day:  ");
  oled.println((mntp.getCompensationFactor() - 1.0) * 24 * 60 * 60 * 1000, 3);

  oled.display();
}