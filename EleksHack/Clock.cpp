#include "Clock.h"

void Clock::begin(StoredConfig::Config::Clock *config_) {
  config = config_;

  if (config->is_valid != StoredConfig::valid) {
    // Config is invalid, probably a new device never had its config written.
    // Load some reasonable defaults.
    Serial.println("Loaded Clock config is invalid, using default.  This is normal on first boot.");
    setTwelveHour(true);
    setTimeZoneOffset(0);
    config->is_valid = StoredConfig::valid;
  }
  
  ntpTimeClient.begin();
  ntpTimeClient.update();
  Serial.println(ntpTimeClient.getFormattedTime());
  setSyncProvider(&Clock::syncProvider);
}

void Clock::loop() {
  if (timeStatus() == timeNotSet) {
    time_valid = false;
  }
  else {
    loop_time = now();
    local_time = loop_time + config->time_zone_offset;
    time_valid = true;
  }
}

void Clock::forceNTPUpdate()
{
  time_t ntp_now, rtc_now;
  rtc_now = RTC.get();

  Serial.print("Getting NTP.");
  ntpTimeClient.forceUpdate();
  Serial.print(".");
  ntp_now = ntpTimeClient.getEpochTime();
  Serial.println(" Done.");

  // Sync the RTC to NTP
  RTC.set(ntp_now);  
}

// Static methods used for sync provider to TimeLib library.
time_t Clock::syncProvider() {
  Serial.println("syncProvider()");
  time_t ntp_now, rtc_now;
  rtc_now = RTC.get();

  if (millis() - millis_last_ntp > refresh_ntp_every_ms || millis_last_ntp == 0) {
    // It's time to get a new NTP sync
    Serial.print("Getting NTP.");
    ntpTimeClient.forceUpdate();
    Serial.print(".");
    ntp_now = ntpTimeClient.getEpochTime();
    Serial.println(" Done.");

    // Sync the RTC to NTP if needed.
    if (ntp_now != rtc_now) {
      RTC.set(ntp_now);
      Serial.println("NTP, RTC, Diff: ");
      Serial.println(ntp_now);
      Serial.println(rtc_now);
      Serial.println(ntp_now-rtc_now);
    }
    millis_last_ntp = millis();
    return ntp_now;    
  }
  Serial.println("Using RTC time.");
  return rtc_now;
}

uint32_t Clock::millis_last_ntp = 0;
WiFiUDP Clock::ntpUDP;
NTPClient Clock::ntpTimeClient(ntpUDP);
