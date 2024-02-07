
#ifndef __SYSLOG_H__
#define __SYSLOG_H__

#ifdef ESP8266
    #include <ESP8266WiFi.h>
#elif defined(ESP32)
    #include <WiFi.h>
#endif
#include <WiFiUdp.h>
#include "../config/config.h"
#include "../config/settings.h"

#ifdef ENABLE_SYSLOG

#define SYSLOG_BUF_SIZE 255

#define PRI_EMERGENCY 0
#define PRI_ALERT     1
#define PRI_CRITICAL  2
#define PRI_ERROR     3
#define PRI_WARNING   4
#define PRI_NOTICE    5
#define PRI_INFO      6
#define PRI_DEBUG     7

#define FAC_USER   1
#define FAC_LOCAL0 16
#define FAC_LOCAL1 17
#define FAC_LOCAL2 18
#define FAC_LOCAL3 19
#define FAC_LOCAL4 20
#define FAC_LOCAL5 21
#define FAC_LOCAL6 22
#define FAC_LOCAL7 23

class DbgSyslog {
    public:
        DbgSyslog();
        void setup (settings_t *config);
        void syslogCb(String msg);
        void log(const char *hostname, uint8_t facility, uint8_t severity, char* msg);

    private:
        WiFiUDP mSyslogUdp;
        IPAddress mSyslogIP;
        settings_t *mConfig = nullptr;
        char mSyslogBuffer[SYSLOG_BUF_SIZE+1];
        uint16_t mSyslogBufFill = 0;
        int mSyslogSeverity = PRI_NOTICE;
};

#endif /*ENABLE_SYSLOG*/

#endif /*__SYSLOG_H__*/
