#include "monotonic_ntp.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>

bool MonotonicNtp::begin(uint64_t update_period_us,
                         const char *ntp_server_name) {
  setUpdatePeriod(update_period_us);
  setNtpServer(ntp_server_name);

  Udp.begin(NTP_PORT);

  // Try to get a sync for at most two seconds, with two retries
  int retries = 3;
  while (!isSynced() && retries--) {
    uint32_t start_sync = millis();
    while (!isSynced() && millis() - start_sync < 2000) {
      if (!waiting_for_ntp_packet) {
        sendNTPpacket();
      } else {
        last_sync_systime = micros64();
        last_sync_ntptime = receiveNTPstamp();
        compfactor = 1.0;
        last_sync_comptime = last_sync_ntptime;
      }
    }
  }

  return isSynced();
}

void MonotonicNtp::loop() {
  if (micros64() - last_sync_systime > update_period_us) {
    adjust();
  }
}

uint64_t MonotonicNtp::now() { return nowAt(micros64()); }

uint64_t MonotonicNtp::nowAt(uint64_t systime) {
  uint64_t systime_elapsed = systime - last_sync_systime;
  return last_sync_comptime + compfactor * systime_elapsed;
}

bool MonotonicNtp::adjust() {
  if (!waiting_for_ntp_packet) {
    sendNTPpacket();
    return false;
  }
  uint64_t systime_now = micros64();
  uint64_t ntptime_now = receiveNTPstamp();
  uint64_t comptime_now = nowAt(ntpresponse_systime);
  if (!ntptime_now) {
    if (systime_now - ntprequest_systime > NTP_TIMEOUT) {
      waiting_for_ntp_packet = false;
    }
    return false;
  }
  waiting_for_ntp_packet = false;

  uint64_t systime_diff = systime_now - last_sync_systime;
  uint64_t ntptime_diff = ntptime_now - last_sync_ntptime;
  int64_t time_delta = comptime_now - ntptime_now;
  double new_compfactor = (double)(ntptime_diff - time_delta) / systime_diff;
  Serial.println(String((double)systime_now / 1e6, 6) +
                 ": compfactor = " + (double)ntptime_diff + "us / " +
                 (double)systime_diff + "us - " + (double)time_delta + "us / " +
                 (double)systime_diff + "us = " + String(new_compfactor, 6) +
                 ", " + String((new_compfactor - 1) * 24 * 60 * 60 * 1000, 0) +
                 " ms/day (delay " +
                 String((double)network_delay_us / 1000, 3) + "ms)");
  // reject implausible compfactors
  if (new_compfactor < 0.9 || new_compfactor > 1.1) {
    return false;
  }
  last_sync_comptime = nowAt(systime_now);
  compfactor = new_compfactor;
  last_sync_systime = systime_now;
  last_sync_ntptime = ntptime_now;
  last_systime_diff = systime_diff;
  last_ntptime_diff = ntptime_diff;
  last_comptime_delta = time_delta;

  return true;
}

void MonotonicNtp::setNtpServer(const char *server_name) {
  this->ntp_server_name =
      (char *)realloc(this->ntp_server_name, strlen(server_name));
  strcpy(this->ntp_server_name, server_name);
  setNtpServer();
}

void MonotonicNtp::setNtpServer() {
  WiFi.hostByName(ntp_server_name, ntp_server_address);
  Serial.println(String("ntp server: ") + ntp_server_name + ", " +
                 ntp_server_address.toString());
}

void MonotonicNtp::sendNTPpacket() {
  memset(&ntp_packet, 0, sizeof(ntp_packet_t));
  // Set the first byte's bits to 00,011,011 for li = 0 (don't process leap
  // indicator alarm condition), vn = 3 (NTP V3), and mode = 3 (client) (0x1B).
  // The rest will be left set to zero.
  *((uint8_t *)&ntp_packet + 0) = 0x1B;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(ntp_server_address, 123);  // NTP requests are to port 123
  Udp.write((uint8_t *)&ntp_packet, sizeof(ntp_packet_t));
  if (Udp.endPacket()) {
    ntprequest_systime = micros64();
    waiting_for_ntp_packet = true;
    while (Udp.parsePacket() > 0)
      ;  // discard any previously received packets
  }
}

uint64_t MonotonicNtp::receiveNTPstamp() {
  size_t size = Udp.parsePacket();
  if (size < sizeof(ntp_packet_t)) {
    // no packet available or this is not a NTP packet
    return 0;
  }
  ntpresponse_systime = micros64();

  // read packet into the buffer
  Udp.read((uint8_t *)&ntp_packet, sizeof(ntp_packet_t));

  // Received KoD packet
  if (ntp_packet.stratum == 0) {
    Serial.println(String("kiss code ") + ((char *)&ntp_packet.refId)[0] +
                   ((char *)&ntp_packet.refId)[1] +
                   ((char *)&ntp_packet.refId)[2] +
                   ((char *)&ntp_packet.refId)[3]);
    // Re-resolve NTP Server IP from pool
    setNtpServer();
    return 0;
  }

  // invalid time received
  if (ntp_packet.txTm_s == 0) {
    // Re-resolve NTP Server IP from pool
    setNtpServer();
    return 0;
  }

  // convert from network to machine endianness
  ntp_packet.txTm_s = ntohl(ntp_packet.txTm_s);
  // convert from seconds since 1900 to seconds since 1970
  ntp_packet.txTm_s -= NTP_UNIXEPOCH_OFFSET;

  // convert from network to machine endianness
  ntp_packet.txTm_f = ntohl(ntp_packet.txTm_f);
  // convert from fractional seconds to microseconds
  ntp_packet.txTm_f = ((uint64_t)ntp_packet.txTm_f * 1000 * 1000) >> 32;

  // add the time it took the packet from the server to us (assumed to be
  // symmetric)
  network_delay_us = (ntpresponse_systime - ntprequest_systime) / 2;
  ntp_packet.txTm_s += network_delay_us / 1000000;
  network_delay_us %= 1000000;
  ntp_packet.txTm_f += network_delay_us;

  // convert from seconds and microseconds to milliseconds and milliseconds
  uint64_t microseconds_since_1970 =
      ((uint64_t)ntp_packet.txTm_s * 1000 * 1000) + ntp_packet.txTm_f;

  return microseconds_since_1970;
}

MonotonicNtp::~MonotonicNtp() { free(ntp_server_name); }