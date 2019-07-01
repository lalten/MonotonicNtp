#pragma once

#include <IPAddress.h>
#include <Udp.h>
#include <stdint.h>

class MonotonicNtp {
 public:
  MonotonicNtp(UDP &udp) : Udp(udp) {}
  bool begin(uint32_t update_period_ms = 12 * 60 * 60 * 1000,
             const char *ntp_server_name = "pool.ntp.org");

  uint64_t now();
  void loop();
  bool adjust();

  void setUpdatePeriod(uint64_t update_period_us) {
    this->update_period_us = update_period_us;
  }
  uint64_t getUpdatePeriod() { return update_period_us; }
  void setNtpServer(const char *server_name);
  const char *getNtpServer() { return this->ntp_server_name; }
  bool isSynced() { return last_sync_ntptime != 0; }

  uint64_t getLastNtptimeDiff() { return last_ntptime_diff; }
  uint64_t getLastSystimeDiff() { return last_systime_diff; }
  uint64_t getLastComptimeSync() { return last_sync_comptime; }
  uint64_t getLastNetworkDelay() { return network_delay_us; }
  int64_t getLastComptimeDelta() { return last_comptime_delta; }
  double getCompensationFactor() { return compfactor; }

  ~MonotonicNtp();

 private:
  MonotonicNtp();
  uint64_t nowAt(uint64_t systime);
  void setNtpServer();

  uint64_t update_period_us;
  uint64_t last_sync_ntptime = 0;
  uint64_t last_sync_comptime = 0;
  uint64_t last_sync_systime = 0;
  uint64_t ntprequest_systime = 0;
  uint64_t ntpresponse_systime = 0;

  double compfactor = 1.0;

  // for debugging
  uint64_t last_ntptime_diff = 0;
  uint64_t last_systime_diff = 0;
  uint32_t network_delay_us = 0;
  int64_t last_comptime_delta = 0;

  bool waiting_for_ntp_packet = false;

  UDP &Udp;

  typedef struct {
    uint8_t
        li_vn_mode;   // Eight bits. li, vn, and mode.
                      // li.   Two bits.   Leap indicator.
                      // vn.   Three bits. Version number of the protocol.
                      // mode. Three bits. Client will pick mode 3 for client.
    uint8_t stratum;  // Eight bits. Stratum level of the local clock.
    uint8_t poll;  // Eight bits. Maximum interval between successive messages.
    uint8_t precision;        // Eight bits. Precision of the local clock.
    uint32_t rootDelay;       // 32 bits. Total round trip delay time.
    uint32_t rootDispersion;  // 32 bits. Max error aloud from primary clock
                              // source.
    uint32_t refId;           // 32 bits. Reference clock identifier.
    uint32_t refTm_s;         // 32 bits. Reference time-stamp seconds.
    uint32_t refTm_f;   // 32 bits. Reference time-stamp fraction of a second.
    uint32_t origTm_s;  // 32 bits. Originate time-stamp seconds.
    uint32_t origTm_f;  // 32 bits. Originate time-stamp fraction of a second.
    uint32_t rxTm_s;    // 32 bits. Received time-stamp seconds.
    uint32_t rxTm_f;    // 32 bits. Received time-stamp fraction of a second.
    uint32_t txTm_s;    // 32 bits and the most important field the client cares
                        // about. Transmit time-stamp seconds.
    uint32_t txTm_f;    // 32 bits. Transmit time-stamp fraction of a second.
  } ntp_packet_t;       // Total: 384 bits or 48 bytes.

  ntp_packet_t ntp_packet;
  static const uint32_t NTP_TIMEOUT = 1500000;  // microseconds
  static const uint16_t NTP_PORT = 1337;
  static const uint32_t NTP_UNIXEPOCH_OFFSET =
      2208988800;  // time difference between 1.1.1900 and 1.1.1970 in seconds
  char *ntp_server_name;
  IPAddress ntp_server_address;

  void sendNTPpacket();
  uint64_t receiveNTPstamp();
};