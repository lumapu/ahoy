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

enum {WIFI_NOT_FOUND = 0, WIFI_FOUND, WIFI_NOT_COMPLETE};


//-----------------------------------------------------------------------------
ahoywifi::ahoywifi() : mApIp(192, 168, 4, 1), mApMask(255, 255, 255, 0) {
    mDnsActive = false;
    mClientCnt = 0;
    mLoopCnt   = 250;
    mExtScan   = false;
}


//-----------------------------------------------------------------------------
void ahoywifi::setup(settings_t *config, uint32_t *utcTimestamp) {
    mCnt       = 0;
    mConnected = false;
    mReconnect = false;
    mConfig = config;
    mUtcTimestamp = utcTimestamp;

    if(String(mConfig->sys.deviceName) != "")
        WiFi.hostname(mConfig->sys.deviceName);

    #if !defined(FB_WIFI_OVERRIDDEN)
        setupAp();
    #endif

    #if defined(ESP8266)
    wifiConnectHandler = WiFi.onStationModeGotIP(std::bind(&ahoywifi::onConnect, this, std::placeholders::_1));
    wifiDisconnectHandler = WiFi.onStationModeDisconnected(std::bind(&ahoywifi::onDisconnect, this, std::placeholders::_1));
    #else
    WiFi.onEvent(std::bind(&ahoywifi::onWiFiEvent, this, std::placeholders::_1));
    #endif
}


//-----------------------------------------------------------------------------
void ahoywifi::loop() {
    #if !defined(AP_ONLY)
    if(mReconnect) {
        delay(100);
        mCnt++;
        if((mCnt % 50) == 0)
            WiFi.disconnect();
        else if((mCnt % 60) == 0) {
            DPRINTLN(DBG_INFO, F("[WiFi] reconnect"));
            WiFi.begin(mConfig->sys.stationSsid, mConfig->sys.stationPwd);
            mCnt = 0;
        }
    }
    yield();
    if(mDnsActive) {
        mDns.processNextRequest();
        uint8_t cnt = WiFi.softAPgetStationNum();
        if(cnt != mClientCnt) {
            mClientCnt = cnt;
            DPRINTLN(DBG_INFO, String(cnt) + F(" client(s) connected"));
        }

        if(!mExtScan && (mLoopCnt == 240)) {
            if(scanStationNetwork()) {
                setupStation();
                mLoopCnt = 0;
            }
        }

        if(0 != mLoopCnt) {
            if(++mLoopCnt > 250) {
                mLoopCnt = 1;
                if(!mExtScan)
                    scanAvailNetworks(false);
            }
            delay(25);
        }
    }
    #endif
}


//-----------------------------------------------------------------------------
void ahoywifi::setupAp(void) {
    DPRINTLN(DBG_INFO, F("wifi::setupAp"));

    DBGPRINTLN(F("\n---------\nAhoyDTU Info:"));
    DBGPRINT(F("Version: "));
    DBGPRINTLN(String(VERSION_MAJOR) + F(".") + String(VERSION_MINOR) + F(".") + String(VERSION_PATCH));
    DBGPRINT(F("Github Hash: "));
    DBGPRINTLN(String(AUTO_GIT_HASH));

    DBGPRINT(F("\n---------\nAP MODE\nSSID: "));
    DBGPRINTLN(WIFI_AP_SSID);
    DBGPRINT(F("PWD: "));
    DBGPRINTLN(WIFI_AP_PWD);
    DBGPRINTLN("IP Address: http://" + mApIp.toString());
    DBGPRINTLN(F("---------\n"));

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(mApIp, mApIp, mApMask);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PWD);

    mDns.setErrorReplyCode(DNSReplyCode::NoError);
    mDns.start(53, "*", WiFi.softAPIP());
    mDnsActive = true;
}


//-----------------------------------------------------------------------------
void ahoywifi::setupStation(void) {
    DPRINTLN(DBG_INFO, F("wifi::setupStation"));
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

    DBGPRINT(F("connect to network '"));
    DBGPRINT(mConfig->sys.stationSsid);
}


//-----------------------------------------------------------------------------
bool ahoywifi::scanStationNetwork(void) {
    bool found = false;
    int n = WiFi.scanComplete();
    if(n > 0) {
        for (int i = 0; i < n; i++) {
            DPRINTLN(DBG_INFO, "found network: " + WiFi.SSID(i));
            if(String(mConfig->sys.stationSsid) == WiFi.SSID(i)) {
                found = true;
                break;
            }
        }
        WiFi.scanDelete();
    }
    return found;
}


//-----------------------------------------------------------------------------
bool ahoywifi::getNtpTime(void) {
    //DPRINTLN(DBG_VERBOSE, F("wifi::getNtpTime"));
    if(!mConnected)
        return false;

    IPAddress timeServer;
    uint8_t buf[NTP_PACKET_SIZE];
    uint8_t retry = 0;

    WiFi.hostByName(mConfig->ntp.addr, timeServer);
    mUdp.begin(mConfig->ntp.port);

    sendNTPpacket(timeServer);

    while(retry++ < 5) {
        int wait = 150;
        while(--wait) {
            if(NTP_PACKET_SIZE <= mUdp.parsePacket()) {
                uint64_t secsSince1900;
                mUdp.read(buf, NTP_PACKET_SIZE);
                secsSince1900  = (buf[40] << 24);
                secsSince1900 |= (buf[41] << 16);
                secsSince1900 |= (buf[42] <<  8);
                secsSince1900 |= (buf[43]      );

                *mUtcTimestamp = secsSince1900 - 2208988800UL; // UTC time
                DPRINTLN(DBG_INFO, "[NTP]: " + ah::getDateTimeStr(*mUtcTimestamp) + " UTC");
                return true;
            } else
                delay(10);
        }
    }

    DPRINTLN(DBG_INFO, F("[NTP]: getNtpTime failed"));
    return false;
}


//-----------------------------------------------------------------------------
void ahoywifi::scanAvailNetworks(bool externalCall) {
    if(externalCall)
        mExtScan = true;
    if(-2 == WiFi.scanComplete())
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
    mExtScan = false;
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

    mUdp.beginPacket(address, 123); // NTP request, port 123
    mUdp.write(buf, NTP_PACKET_SIZE);
    mUdp.endPacket();
}


//-----------------------------------------------------------------------------
#if defined(ESP8266)
    void ahoywifi::onConnect(const WiFiEventStationModeGotIP& event) {
        if(!mConnected) {
            mConnected = true;
            mReconnect = false;
            DBGPRINTLN(F("\n[WiFi] Connected"));
            WiFi.mode(WIFI_STA);
            DBGPRINTLN(F("[WiFi] AP disabled"));
            mDnsActive = false;
            mDns.stop();

            welcome(WiFi.localIP().toString() + F(" (Station)"));
        }
    }


    //-------------------------------------------------------------------------
    void ahoywifi::onDisconnect(const WiFiEventStationModeDisconnected& event) {
        if(mConnected) {
            mConnected = false;
            mReconnect = true;
            mCnt       = 0;
            DPRINTLN(DBG_INFO, "[WiFi] Connection Lost");
        }
    }

#else
    //-------------------------------------------------------------------------
    void ahoywifi::onWiFiEvent(WiFiEvent_t event) {
        switch(event) {
            case SYSTEM_EVENT_STA_GOT_IP:
                if(!mConnected) {
                    delay(1000);
                    mConnected = true;
                    DBGPRINTLN(F("\n[WiFi] Connected"));
                    welcome(WiFi.localIP().toString() + F(" (Station)"));
                    WiFi.mode(WIFI_STA);
                    WiFi.begin();
                    DBGPRINTLN(F("[WiFi] AP disabled"));
                    mDnsActive = false;
                    mDns.stop();
                    mReconnect = false;
                }
                break;

            case SYSTEM_EVENT_STA_DISCONNECTED:
                if(mConnected) {
                    mConnected = false;
                    mReconnect = true;
                    mCnt       = 0;
                    DPRINTLN(DBG_INFO, "[WiFi] Connection Lost");
                }
                break;

            default:
                break;
        }
    }
#endif


//-----------------------------------------------------------------------------
void ahoywifi::welcome(String msg) {
    DBGPRINTLN(F("\n\n--------------------------------"));
    DBGPRINTLN(F("Welcome to AHOY!"));
    DBGPRINT(F("\npoint your browser to http://"));
    DBGPRINTLN(msg);
    DBGPRINTLN(F("to configure your device"));
    DBGPRINTLN(F("--------------------------------\n"));
}
