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
ahoywifi::ahoywifi() : mApIp(192, 168, 4, 1) {}


//-----------------------------------------------------------------------------
void ahoywifi::setup(settings_t *config, uint32_t *utcTimestamp) {
    mConfig = config;
    mUtcTimestamp = utcTimestamp;

    mCnt        = 0;
    mConnected  = false;
    mReconnect  = false;
    mScanActive = false;

    #if defined(ESP8266)
    wifiConnectHandler = WiFi.onStationModeConnected(std::bind(&ahoywifi::onConnect, this, std::placeholders::_1));
    wifiGotIPHandler = WiFi.onStationModeGotIP(std::bind(&ahoywifi::onGotIP, this, std::placeholders::_1));
    wifiDisconnectHandler = WiFi.onStationModeDisconnected(std::bind(&ahoywifi::onDisconnect, this, std::placeholders::_1));
    #else
    WiFi.onEvent(std::bind(&ahoywifi::onWiFiEvent, this, std::placeholders::_1));
    #endif

    setupWifi();
}


//-----------------------------------------------------------------------------
void ahoywifi::setupWifi(void) {
    #if !defined(FB_WIFI_OVERRIDDEN)
        setupAp();
        delay(1000);
    #endif
    #if !defined(AP_ONLY)
        if(mConfig->valid) {
            #if !defined(FB_WIFI_OVERRIDDEN)
                if(strncmp(mConfig->sys.stationSsid, FB_WIFI_SSID, 14) != 0)
                    setupStation();
            #else
                setupStation();
            #endif
        }
    #endif
}


//-----------------------------------------------------------------------------
void ahoywifi::loop() {
    #if !defined(AP_ONLY)
    if(mReconnect) {
        delay(100);
        if (WiFi.softAPgetStationNum() > 0) { // do not reconnect if any AP connection exists
            mDns.processNextRequest();
            if((WIFI_AP_STA == WiFi.getMode()) && !mScanActive) {
                DBGPRINTLN(F("AP client connected"));
                welcome(mApIp.toString());
                WiFi.mode(WIFI_AP);
            }
            return;
        }
        else if(WIFI_AP == WiFi.getMode()) {
            mCnt = 0;
            WiFi.mode(WIFI_AP_STA);
        }
        mCnt++;

        if ((mCnt % 10) == 0) {
            DBGPRINT(F("reconnect in "));
            DBGPRINT(String((100-mCnt)/10));
            DBGPRINTLN(F(" seconds"));
        }
        if((mCnt % 100) == 0) { // try to reconnect after 10 sec without connection
            WiFi.reconnect();
            mCnt = 0;
        }
    }
    #endif
}


//-----------------------------------------------------------------------------
void ahoywifi::setupAp(void) {
    DPRINTLN(DBG_VERBOSE, F("wifi::setupAp"));

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
    WiFi.softAPConfig(mApIp, mApIp, IPAddress(255, 255, 255, 0));
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PWD);

    mDns.start(53, "*", mApIp);
}


//-----------------------------------------------------------------------------
void ahoywifi::setupStation(void) {
    DPRINTLN(DBG_VERBOSE, F("wifi::setupStation"));
    if(mConfig->sys.ip.ip[0] != 0) {
        IPAddress ip(mConfig->sys.ip.ip);
        IPAddress mask(mConfig->sys.ip.mask);
        IPAddress dns1(mConfig->sys.ip.dns1);
        IPAddress dns2(mConfig->sys.ip.dns2);
        IPAddress gateway(mConfig->sys.ip.gateway);
        if(!WiFi.config(ip, gateway, mask, dns1, dns2))
            DPRINTLN(DBG_ERROR, F("failed to set static IP!"));
    }
    mReconnect = (WiFi.begin(mConfig->sys.stationSsid, mConfig->sys.stationPwd) != WL_CONNECTED);
    if(String(mConfig->sys.deviceName) != "")
        WiFi.hostname(mConfig->sys.deviceName);

    DBGPRINT(F("connect to network '"));
    DBGPRINT(mConfig->sys.stationSsid);
    DBGPRINTLN(F("' ..."));
}


//-----------------------------------------------------------------------------
bool ahoywifi::getNtpTime(void) {
    if(!mConnected)
        return false;

    IPAddress timeServer;
    uint8_t buf[NTP_PACKET_SIZE];
    uint8_t retry = 0;

    if (WiFi.hostByName(mConfig->ntp.addr, timeServer) != 1)
        return false;

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
void ahoywifi::scanAvailNetworks(void) {
    if(-2 == WiFi.scanComplete()) {
        mScanActive = true;
        if(WIFI_AP == WiFi.getMode())
            WiFi.mode(WIFI_AP_STA);
        WiFi.scanNetworks(true);
    }
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
        mScanActive = false;
        WiFi.scanDelete();
    }
}


//-----------------------------------------------------------------------------
void ahoywifi::connectionEvent(bool connected) {
    if (connected) {
        if(!mConnected) {
            mConnected = true;
            mReconnect = false;
            DBGPRINTLN(F("\n[WiFi] Connected"));
            WiFi.mode(WIFI_STA);
            DBGPRINTLN(F("[WiFi] AP disabled"));
            mDns.stop();
        }
    } else {
        if(mConnected) {
            mConnected = false;
            mReconnect = true;
            mCnt       = 50;    // try to reconnect in 5 sec
            setupWifi();        // reconnect with AP / Station setup
            DPRINTLN(DBG_INFO, "[WiFi] Connection Lost");
        }
    }
}


//-----------------------------------------------------------------------------
#if defined(ESP8266)
    //-------------------------------------------------------------------------
    void ahoywifi::onConnect(const WiFiEventStationModeConnected& event) {
        connectionEvent(true);
    }

    //-------------------------------------------------------------------------
    void ahoywifi::onGotIP(const WiFiEventStationModeGotIP& event) {
        welcome(WiFi.localIP().toString() + F(" (Station)"));
    }

    //-------------------------------------------------------------------------
    void ahoywifi::onDisconnect(const WiFiEventStationModeDisconnected& event) {
        connectionEvent(false);
    }

#else
    //-------------------------------------------------------------------------
    void ahoywifi::onWiFiEvent(WiFiEvent_t event) {
        DBGPRINT(F("Wifi event: "));
        DBGPRINTLN(String(event));

        switch(event) {
            case SYSTEM_EVENT_STA_CONNECTED:
                connectionEvent(true);
                break;

            case SYSTEM_EVENT_STA_GOT_IP:
                welcome(WiFi.localIP().toString() + F(" (Station)"));
                break;

            case SYSTEM_EVENT_STA_DISCONNECTED:
                connectionEvent(false);
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
