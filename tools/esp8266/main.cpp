//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#include "main.h"
#include "version.h"

//-----------------------------------------------------------------------------
Main::Main(void) {
    mDns     = new DNSServer();
    mWeb     = new ESP8266WebServer(80);
    mUdp     = new WiFiUDP();

    memset(&config, 0, sizeof(config_t));

    config.apActive          = true;
    mWifiSettingsValid = false;
    mSettingsValid     = false;

    mLimit       = 10;
    mNextTryTs   = 0;
    mApLastTick  = 0;

    // default config
    snprintf(config.version, 12, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    config.apActive = false;
    config.sendInterval = SEND_INTERVAL;


    mEep = new eep();
    Serial.begin(115200);
    DPRINTLN(DBG_VERBOSE, F("Main::Main"));

    mUptimeSecs     = 0;
    mUptimeTicker   = 0xffffffff;
    mUptimeInterval = 1000;

#ifdef AP_ONLY
    mTimestamp = 1;
#else
    mTimestamp = 0;
#endif

    mHeapStatCnt = 0;
}


//-----------------------------------------------------------------------------
void Main::setup(uint32_t timeout) {
    DPRINTLN(DBG_VERBOSE, F("Main::setup"));
    bool startAp = config.apActive;
    mLimit = timeout;


    startAp = getConfig();

#ifndef AP_ONLY
    if(false == startAp)
        startAp = setupStation(timeout);
#endif

    config.apActive = startAp;
    mStActive = !startAp;
}


//-----------------------------------------------------------------------------
void Main::loop(void) {
    //DPRINTLN(DBG_VERBOSE, F("M"));
    if(config.apActive) {
        mDns->processNextRequest();
#ifndef AP_ONLY
        if(checkTicker(&mNextTryTs, (WIFI_AP_ACTIVE_TIME * 1000))) {
            config.apActive = setupStation(mLimit);
            if(config.apActive) {
                if(strlen(WIFI_AP_PWD) < 8)
                    DPRINTLN(DBG_ERROR, F("password must be at least 8 characters long"));
                mApLastTick = millis();
                mNextTryTs = (millis() + (WIFI_AP_ACTIVE_TIME * 1000));
                setupAp(WIFI_AP_SSID, WIFI_AP_PWD);
            }
        }
        else {
            if(millis() - mApLastTick > 10000) {
                uint8_t cnt = WiFi.softAPgetStationNum();
                if(cnt > 0) {
                    DPRINTLN(DBG_INFO, String(cnt) + F(" clients connected, resetting AP timeout"));
                    mNextTryTs = (millis() + (WIFI_AP_ACTIVE_TIME * 1000));
                }
                mApLastTick = millis();
                DPRINTLN(DBG_INFO, F("AP will be closed in ") + String((mNextTryTs - mApLastTick) / 1000) + F(" seconds"));
            }
        }
#endif
    }
    mWeb->handleClient();

    if(checkTicker(&mUptimeTicker, mUptimeInterval)) {
        mUptimeSecs++;
        if(0 != mTimestamp)
            mTimestamp++;
        else {
            if(!config.apActive) {
                mTimestamp  = getNtpTime();
                DPRINTLN(DBG_INFO, "[NTP]: " + getDateTimeStr(mTimestamp));
            }
        }

        /*if(++mHeapStatCnt >= 10) {
            mHeapStatCnt = 0;
            stats();
        }*/
    }
    if (WiFi.status() != WL_CONNECTED) {
        DPRINTLN(DBG_INFO, "[WiFi]: Connection Lost");
        mStActive = false;
    }
}


//-----------------------------------------------------------------------------
bool Main::getConfig(void) {
    DPRINTLN(DBG_VERBOSE, F("Main::getConfig"));
    config.apActive = false;

    mWifiSettingsValid = checkEEpCrc(ADDR_START, ADDR_WIFI_CRC, ADDR_WIFI_CRC);
    mSettingsValid = checkEEpCrc(ADDR_START_SETTINGS, ((ADDR_NEXT)-(ADDR_START_SETTINGS)), ADDR_SETTINGS_CRC);

    if(mWifiSettingsValid) {
        mEep->read(ADDR_SSID,    config.stationSsid, SSID_LEN);
        mEep->read(ADDR_PWD,     config.stationPwd, PWD_LEN);
        mEep->read(ADDR_DEVNAME, config.deviceName, DEVNAME_LEN);
    }

    if((!mWifiSettingsValid) || (config.stationSsid[0] == 0xff)) {
        snprintf(config.stationSsid, SSID_LEN,    "%s", FB_WIFI_SSID);
        snprintf(config.stationPwd,  PWD_LEN,     "%s", FB_WIFI_PWD);
        snprintf(config.deviceName,  DEVNAME_LEN, "%s", DEF_DEVICE_NAME);
    }

    return config.apActive;
}


//-----------------------------------------------------------------------------
void Main::setupAp(const char *ssid, const char *pwd) {
    DPRINTLN(DBG_VERBOSE, F("Main::setupAp"));
    IPAddress apIp(192, 168, 1, 1);

    DPRINTLN(DBG_INFO, F("\n---------\nAP MODE\nSSID: ")
        + String(ssid) + F("\nPWD: ")
        + String(pwd) + F("\nActive for: ")
        + String(WIFI_AP_ACTIVE_TIME) + F(" seconds")
        + F("\n---------\n"));
    DPRINTLN(DBG_DEBUG, String(mNextTryTs));

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0));
    WiFi.softAP(ssid, pwd);

    mDns->start(mDnsPort, "*", apIp);

    /*mWeb->onNotFound([&]() {
        showSetup();
    });
    mWeb->on("/", std::bind(&Main::showSetup, this));

    mWeb->begin();*/
}


//-----------------------------------------------------------------------------
bool Main::setupStation(uint32_t timeout) {
    DPRINTLN(DBG_VERBOSE, F("Main::setupStation"));
    int32_t cnt;
    bool startAp = false;

    if(timeout >= 3)
        cnt = (timeout - 3) / 2 * 10;
    else {
        timeout = 1;
        cnt = 1;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(config.stationSsid, config.stationPwd);
    if(String(config.deviceName) != "")
        WiFi.hostname(config.deviceName);

    delay(2000);
    DPRINTLN(DBG_INFO, F("connect to network '") + String(config.stationSsid) + F("' ..."));
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        if(cnt % 100 == 0)
            Serial.println(".");
        else
            Serial.print(".");

        if(timeout > 0) { // limit == 0 -> no limit
            if(--cnt <= 0) {
                if(WiFi.status() != WL_CONNECTED) {
                    startAp = true;
                    WiFi.disconnect();
                }
                delay(100);
                break;
            }
        }
    }
    Serial.println(".");

    if(false == startAp) {
        mWeb->begin();
    }

    delay(1000);

    return startAp;
}



//-----------------------------------------------------------------------------
void Main::saveValues(uint32_t saveMask = 0) {
    DPRINTLN(DBG_VERBOSE, F("Main::saveValues"));

    if(CHK_MSK(saveMask, SAVE_SSID))
        mEep->write(ADDR_SSID, config.stationSsid, SSID_LEN);
    if(CHK_MSK(saveMask, SAVE_PWD))
        mEep->write(ADDR_PWD, config.stationPwd, SSID_LEN);
    if(CHK_MSK(saveMask, SAVE_DEVICE_NAME))
        mEep->write(ADDR_DEVNAME, config.deviceName, DEVNAME_LEN);

    if(saveMask > 0) {
        updateCrc();
        mEep->commit();
    }
}


//-----------------------------------------------------------------------------
void Main::updateCrc(void) {
    DPRINTLN(DBG_VERBOSE, F("Main::updateCrc"));
    uint16_t crc;
    crc = buildEEpCrc(ADDR_START, ADDR_WIFI_CRC);
    //Serial.println("new CRC: " + String(crc, HEX));
    mEep->write(ADDR_WIFI_CRC, crc);
    mEep->commit();
}


//-----------------------------------------------------------------------------
time_t Main::getNtpTime(void) {
    //DPRINTLN(DBG_VERBOSE, F("Main::getNtpTime"));
    time_t date = 0;
    IPAddress timeServer;
    uint8_t buf[NTP_PACKET_SIZE];
    uint8_t retry = 0;

    WiFi.hostByName(NTP_SERVER_NAME, timeServer);
    mUdp->begin(NTP_LOCAL_PORT);


    sendNTPpacket(timeServer);

    while(retry++ < 5) {
        int wait = 150;
        while(--wait) {
            if(NTP_PACKET_SIZE <= mUdp->parsePacket()) {
                uint64_t secsSince1900;
                mUdp->read(buf, NTP_PACKET_SIZE);
                secsSince1900  = (buf[40] << 24);
                secsSince1900 |= (buf[41] << 16);
                secsSince1900 |= (buf[42] <<  8);
                secsSince1900 |= (buf[43]      );

                date = secsSince1900 - 2208988800UL; // UTC time
                date += (TIMEZONE + offsetDayLightSaving(date)) * 3600;
                break;
            }
            else
                delay(10);
        }
    }

    return date;
}


//-----------------------------------------------------------------------------
void Main::sendNTPpacket(IPAddress& address) {
    //DPRINTLN(DBG_VERBOSE, F("Main::sendNTPpacket"));
    uint8_t buf[NTP_PACKET_SIZE] = {0};

    buf[0] = B11100011; // LI, Version, Mode
    buf[1] = 0;         // Stratum
    buf[2] = 6;         // Max Interval between messages in seconds
    buf[3] = 0xEC;      // Clock Precision
    // bytes 4 - 11 are for Root Delay and Dispersion and were set to 0 by memset
    buf[12] = 49;       // four-byte reference ID identifying
    buf[13] = 0x4E;
    buf[14] = 49;
    buf[15] = 52;

    mUdp->beginPacket(address, 123); // NTP request, port 123
    mUdp->write(buf, NTP_PACKET_SIZE);
    mUdp->endPacket();
}


//-----------------------------------------------------------------------------
// calculates the daylight saving time for middle Europe. Input: Unixtime in UTC
// from: https://forum.arduino.cc/index.php?topic=172044.msg1278536#msg1278536
time_t Main::offsetDayLightSaving (uint32_t local_t) {
    //DPRINTLN(DBG_VERBOSE, F("Main::offsetDayLightSaving"));
    int m = month (local_t);
    if(m < 3 || m > 10) return 0; // no DSL in Jan, Feb, Nov, Dez
    if(m > 3 && m < 10) return 1; // DSL in Apr, May, Jun, Jul, Aug, Sep
    int y = year (local_t);
    int h = hour (local_t);
    int hToday = (h + 24 * day(local_t));
    if((m == 3  && hToday >= (1 + TIMEZONE + 24 * (31 - (5 * y /4 + 4) % 7)))
        || (m == 10 && hToday <  (1 + TIMEZONE + 24 * (31 - (5 * y /4 + 1) % 7))) )
        return 1;
    else
        return 0;
}
