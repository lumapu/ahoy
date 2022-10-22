//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#if defined(ESP32) && defined(F)
  #undef F
  #define F(sl) (sl)
#endif
#include "ahoywifi.h"


// NTP CONFIG
#define NTP_PACKET_SIZE     48


//-----------------------------------------------------------------------------
ahoywifi::ahoywifi(app *main, sysConfig_t *sysCfg, config_t *config) {
    mMain    = main;
    mSysCfg  = sysCfg;
    mConfig  = config;

    mDns = new DNSServer();
    mUdp = new WiFiUDP();

    mWifiStationTimeout = 10;
    wifiWasEstablished  = false;
    mNextTryTs          = 0;
    mApLastTick         = 0;
    mApActive           = false;
}


//-----------------------------------------------------------------------------
void ahoywifi::setup(uint32_t timeout, bool settingValid) {
    mWifiStationTimeout = timeout;
    #ifndef AP_ONLY
        if(false == mApActive)
            mApActive = setupStation(mWifiStationTimeout);
    #endif

    if(!settingValid) {
        DPRINTLN(DBG_WARN, F("your settings are not valid! check [IP]/setup"));
        mApActive = true;
        mApLastTick = millis();
        mNextTryTs = (millis() + (WIFI_AP_ACTIVE_TIME * 1000));
        setupAp(WIFI_AP_SSID, WIFI_AP_PWD);
    }
    else {
        DPRINTLN(DBG_INFO, F("\n\n----------------------------------------"));
        DPRINTLN(DBG_INFO, F("Welcome to AHOY!"));
        DPRINT(DBG_INFO, F("\npoint your browser to http://"));
        if(mApActive)
            DBGPRINTLN(F("192.168.1.1"));
        else
            DBGPRINTLN(WiFi.localIP().toString());
        DPRINTLN(DBG_INFO, F("to configure your device"));
        DPRINTLN(DBG_INFO, F("----------------------------------------\n"));
    }
}


//-----------------------------------------------------------------------------
bool ahoywifi::loop(void) {
    if(mApActive) {
        mDns->processNextRequest();
#ifndef AP_ONLY
        if(mMain->checkTicker(&mNextTryTs, (WIFI_AP_ACTIVE_TIME * 1000))) {
            mApActive = setupStation(mWifiStationTimeout);
            if(mApActive) {
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
                    DPRINTLN(DBG_INFO, String(cnt) + F(" client connected, resetting AP timeout"));
                    mNextTryTs = (millis() + (WIFI_AP_ACTIVE_TIME * 1000));
                }
                mApLastTick = millis();
                DPRINTLN(DBG_INFO, F("AP will be closed in ") + String((mNextTryTs - mApLastTick) / 1000) + F(" seconds"));
            }
        }
#endif
    }

    if((WiFi.status() != WL_CONNECTED) && wifiWasEstablished) {
        if(!mApActive) {
            DPRINTLN(DBG_INFO, "[WiFi]: Connection Lost");
            mApActive = setupStation(mWifiStationTimeout);
        }
    }

    return mApActive;
}


//-----------------------------------------------------------------------------
void ahoywifi::setupAp(const char *ssid, const char *pwd) {
    DPRINTLN(DBG_VERBOSE, F("app::setupAp"));
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

    mDns->start(53, "*", apIp);
}


//-----------------------------------------------------------------------------
bool ahoywifi::setupStation(uint32_t timeout) {
    DPRINTLN(DBG_VERBOSE, F("app::setupStation"));
    int32_t cnt;
    bool startAp = false;

    if(timeout >= 3)
        cnt = (timeout - 3) / 2 * 10;
    else {
        timeout = 1;
        cnt = 1;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(mSysCfg->stationSsid, mSysCfg->stationPwd);
    if(String(mSysCfg->deviceName) != "")
        WiFi.hostname(mSysCfg->deviceName);

    delay(2000);
    DPRINTLN(DBG_INFO, F("connect to network '") + String(mSysCfg->stationSsid) + F("' ..."));
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        if(cnt % 40 == 0)
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

    if(false == startAp)
        wifiWasEstablished = true;

    delay(1000);
    return startAp;
}


//-----------------------------------------------------------------------------
bool ahoywifi::getApActive(void) {
    return mApActive;
}

//-----------------------------------------------------------------------------
time_t ahoywifi::getNtpTime(void) {
    //DPRINTLN(DBG_VERBOSE, F("wifi::getNtpTime"));
    time_t date = 0;
    IPAddress timeServer;
    uint8_t buf[NTP_PACKET_SIZE];
    uint8_t retry = 0;

    WiFi.hostByName(mConfig->ntpAddr, timeServer);
    mUdp->begin(mConfig->ntpPort);

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
                break;
            }
            else
                delay(10);
        }
    }

    return date;
}


//-----------------------------------------------------------------------------
void ahoywifi::getAvailNetworks(JsonObject obj) {
    JsonArray nets = obj.createNestedArray("networks");

    int n = WiFi.scanComplete();
    if(n == -2) {
        WiFi.scanNetworks(true);
    } else if(n) {
        for (int i = 0; i < n; ++i) {
            nets[i]["ssid"]   = WiFi.SSID(i);
            nets[i]["rssi"]   = WiFi.RSSI(i);
            // TODO: does github workflow use another version of this library?
            // ahoywifi.cpp:223:38: error: 'class WiFiClass' has no member named 'isHidden'
            //nets[i]["hidden"] = WiFi.isHidden(i) ? true : false;
        }
        WiFi.scanDelete();
        if(WiFi.scanComplete() == -2)
            WiFi.scanNetworks(true);
    }
}


//-----------------------------------------------------------------------------
void ahoywifi::sendNTPpacket(IPAddress& address) {
    //DPRINTLN(DBG_VERBOSE, F("wifi::sendNTPpacket"));
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
