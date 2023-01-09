#include <WiFi.h>
#include <Ticker.h>
#include <WiFiUdp.h>

#define wifissid "ssid"
#define wifipw "password"

#define TIME_TO_SLEEP 3600  // seconds
#define NTP_SRV "pool.ntp.org"

RTC_DATA_ATTR uint32_t init_time = 0;

timeval sntpTime() {
  WiFiUDP udp;
  uint64_t sec = 0;
  uint64_t fractional = 0;
  uint8_t sendPacket[48] = {0};
  sendPacket[0] = 0x1B;
  udp.begin(123);
  udp.beginPacket(NTP_SRV, 123);
  udp.write(sendPacket, 48);
  udp.endPacket();
  for (int z=0; z<80; z++) {
    delay(250);
    if (udp.parsePacket()) { // we've got a packet
        byte recvPacket[48];
        if (udp.read(recvPacket, 48) == 48) {
          udp.stop();
          for (int x=0; x<4; x++) sec += recvPacket[40+x] << ((3-x)*8);
          sec -= 2208988800L;
          for (int x=0; x<4; x++) fractional += recvPacket[44+x] << ((3-x)*8);
          return (timeval){(long int)sec, (long int)fractional/1000L};
        }
    }
  }
  return (timeval){0, 0};
}

void initTime() {
  timeval epoch = sntpTime();
  settimeofday((const timeval*)&epoch, 0);
  init_time = time(NULL);
  Serial.printf("Time set at epoch %u\n", init_time);
}

void setup() {
  Serial.begin(115200);

#ifdef wifissid
  WiFi.begin(wifissid, wifipw);
#else
  WiFi.begin();
#endif  
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Failed to connect to wifi");
    return;
  }
  if (!init_time) initTime();
}

void loop() {
  timeval mytime = sntpTime();
  if (mytime.tv_sec == 0) {
    Serial.println("Unable to acquire NTP time");
    return;
  }
  int drift = mytime.tv_sec - time(NULL);
  uint32_t elapsed = time(NULL) - init_time;
  int ppm = elapsed ? drift * 1000000 / (int)elapsed : 0;
  Serial.printf("Drift is %d secs over %u elapsed (%d ppm)\n",
                 drift, elapsed, ppm);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * 1000000ULL);
  Serial.flush(); 
  esp_deep_sleep_start();
}
