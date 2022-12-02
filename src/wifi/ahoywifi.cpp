//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#if defined(ESP32) && defined(F)
  #undef F
  #define F(sl) (sl)
#endif
#include "ahoywifi.h"
#include "../utils/ahoyTimer.h"


// NTP CONFIG
#define NTP_PACKET_SIZE     48


//-----------------------------------------------------------------------------
ahoywifi::ahoywifi(settings_t *config) {
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
    //wifiConnectHandler = WiFi.onStationModeGotIP(std::bind(&ahoywifi::onConnect, this, std::placeholders::_1));
    //wifiDisconnectHandler = WiFi.onStationModeDisconnected(std::bind(&ahoywifi::onDisconnect, this, std::placeholders::_1));

    #ifdef FB_WIFI_OVERRIDDEN
        mStationWifiIsDef = false;
    #else
        mStationWifiIsDef = (strncmp(mConfig->sys.stationSsid, FB_WIFI_SSID, 14) == 0);
    #endif
    mWifiStationTimeout = timeout;
    #ifndef AP_ONLY
        if(false == mApActive)
            mApActive = (mStationWifiIsDef) ? true : setupStation(mWifiStationTimeout);
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
            DBGPRINTLN(F("192.168.4.1"));
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
        if(ah::checkTicker(&mNextTryTs, (WIFI_AP_ACTIVE_TIME * 1000))) {
            mApActive = (mStationWifiIsDef) ? true : setupStation(mWifiStationTimeout);
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
                mApLastTick = millis();
                uint8_t cnt = WiFi.softAPgetStationNum();
                if(cnt > 0) {
                    DPRINTLN(DBG_INFO, String(cnt) + F(" client connected (no timeout)"));
                    mNextTryTs = (millis() + (WIFI_AP_ACTIVE_TIME * 1000));
                }
                else {
                    DBGPRINT(F("AP will be closed in "));
                    DBGPRINT(String((mNextTryTs - mApLastTick) / 1000));
                    DBGPRINTLN(F(" seconds"));
                }
            }
        }
#endif
    }

    if((WiFi.status() != WL_CONNECTED) && wifiWasEstablished) {
        if(!mApActive) {
            DPRINTLN(DBG_INFO, "[WiFi]: Connection Lost");
            mApActive = (mStationWifiIsDef) ? true : setupStation(mWifiStationTimeout);
        }
    }

    return mApActive;
}


//-----------------------------------------------------------------------------
void ahoywifi::setupAp(const char *ssid, const char *pwd) {
    DPRINTLN(DBG_VERBOSE, F("app::setupAp"));
    IPAddress apIp(192, 168, 4, 1);

    DBGPRINTLN(F("\n---------\nAhoy Info:"));
    DBGPRINT(F("Version: "));
    DBGPRINTLN(String(VERSION_MAJOR) + F(".") + String(VERSION_MINOR) + F(".") + String(VERSION_PATCH));
    DBGPRINT(F("Github Hash: "));
    DBGPRINTLN(String(AUTO_GIT_HASH));

    DBGPRINT(F("\n---------\nAP MODE\nSSID: "));
    DBGPRINTLN(ssid);
    DBGPRINT(F("PWD: "));
    DBGPRINTLN(pwd);
    DBGPRINT(F("\nActive for: "));
    DBGPRINT(String(WIFI_AP_ACTIVE_TIME));
    DBGPRINTLN(F(" seconds"));

    DBGPRINTLN("\nIp Address: " + apIp[0] + apIp[1] + apIp[2] + apIp[3]);
    DBGPRINTLN(F("\n---------\n"));

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
    if(mConfig->sys.ip.ip[0] != 0) {
        IPAddress ip(mConfig->sys.ip.ip);
        IPAddress mask(mConfig->sys.ip.mask);
        IPAddress dns1(mConfig->sys.ip.dns1);
        IPAddress dns2(mConfig->sys.ip.dns2);
        IPAddress gateway(mConfig->sys.ip.gateway);
        if(!WiFi.config(ip, gateway, mask, dns1, dns2))
            DPRINTLN(DBG_ERROR, F("failed to set static IP!"));
    }
    WiFi.begin(mConfig->sys.stationSsid, mConfig->sys.stationPwd);
    if(String(mConfig->sys.deviceName) != "")
        WiFi.hostname(mConfig->sys.deviceName);

    delay(2000);
    DBGPRINT(F("connect to network '"));
    DBGPRINT(mConfig->sys.stationSsid);
    DBGPRINTLN(F("' ..."));
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        if(cnt % 40 == 0)
            DBGPRINTLN(".");
        else
            DBGPRINT(".");

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

    WiFi.hostByName(mConfig->ntp.addr, timeServer);
    mUdp->begin(mConfig->ntp.port);

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
void ahoywifi::scanAvailNetworks(void) {
    int n = WiFi.scanComplete();
    if(n == -2)
        WiFi.scanNetworks(true);
}


//-----------------------------------------------------------------------------
void ahoywifi::getAvailNetworks(JsonObject obj) {
    JsonArray nets = obj.createNestedArray("networks");

    int n = WiFi.scanComplete();
    if(n > 0) {
        int sort[n];
        for (int i = 0; i < n; i++)
            sort[i] = i;
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++)
                if (WiFi.RSSI(sort[j]) > WiFi.RSSI(sort[i]))
                    std::swap(sort[i], sort[j]);
        for (int i = 0; i < n; ++i) {
            nets[i]["ssid"]   = WiFi.SSID(sort[i]);
            nets[i]["rssi"]   = WiFi.RSSI(sort[i]);
        }
        WiFi.scanDelete();
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


//-----------------------------------------------------------------------------
/*void ahoywifi::onConnect(const WiFiEventStationModeGotIP& event) {
    Serial.println("Connected to Wi-Fi.");
}


//-----------------------------------------------------------------------------
void ahoywifi::onDisconnect(const WiFiEventStationModeDisconnected& event) {
    Serial.println("Disconnected from Wi-Fi.");
}*/
