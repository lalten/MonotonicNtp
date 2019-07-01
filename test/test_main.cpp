#include <Arduino.h>
#include <WiFiUdp.h>
#include <unity.h>
#include "../lib/MonotonicNtp/monotonic_ntp.h"

#include <WiFiManager.h>
WiFiManager wifimanager;

WiFiUDP udp;
MonotonicNtp mntp(udp);

void test_begin() { TEST_ASSERT(mntp.begin()); }

void test_now() {
  uint64_t n = mntp.now();
  Serial.println(String("now: ") + (double)n / 1000.0);
  TEST_ASSERT(n > 1561934000);
}

void test_adjust() {
  uint32_t start_stamp = millis();
  bool ret = false;
  while (millis() - start_stamp < 2000) {
    ret |= mntp.adjust();
    if (ret) break;
    delay(100);
  }
  TEST_ASSERT(ret);
}

void test_monotonic() {
  mntp.adjust();
  uint64_t last_stamp = mntp.now();
  uint32_t start_stamp = millis();
  uint32_t adjust_stamp = millis();
  while (millis() - start_stamp < 60000) {
    if (millis() - adjust_stamp > 10000) {
      test_adjust();
      adjust_stamp = millis();
    }
    uint64_t this_stamp = mntp.now();
    TEST_ASSERT(this_stamp - last_stamp >= 0);
    last_stamp = this_stamp;
    yield();
  }
}

void setup() {
  Serial.begin(115200);
  wifimanager.autoConnect();

  UNITY_BEGIN();
  RUN_TEST(test_begin);
  RUN_TEST(test_now);
  RUN_TEST(test_adjust);
  RUN_TEST(test_monotonic);
  UNITY_END();  // stop unit testing
}

void loop() {
  mntp.loop();
  delay(100);
}
