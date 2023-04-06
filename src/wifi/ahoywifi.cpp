//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
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
void ahoywifi::setup(settings_t *config, uint32_t *utcTimestamp, appWifiCb cb) {
    mConfig = config;
    mUtcTimestamp = utcTimestamp;
    mAppWifiCb = cb;

    mStaConn    = DISCONNECTED;
    mCnt        = 0;
    mScanActive = false;
    mScanCnt    = 0;

    #if defined(ESP8266)
    wifiConnectHandler = WiFi.onStationModeConnected(std::bind(&ahoywifi::onConnect, this, std::placeholders::_1));
    wifiGotIPHandler = WiFi.onStationModeGotIP(std::bind(&ahoywifi::onGotIP, this, std::placeholders::_1));
    wifiDisconnectHandler = WiFi.onStationModeDisconnected(std::bind(&ahoywifi::onDisconnect, this, std::placeholders::_1));
    #else
    WiFi.onEvent(std::bind(&ahoywifi::onWiFiEvent, this, std::placeholders::_1));
    #endif

    setupWifi(true);
}


//-----------------------------------------------------------------------------
void ahoywifi::setupWifi(bool startAP = false) {
    #if !defined(FB_WIFI_OVERRIDDEN)
        if(startAP) {
            setupAp();
            delay(1000);
        }
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
void ahoywifi::tickWifiLoop() {
    #if !defined(AP_ONLY)
    if(mStaConn != GOT_IP) {
        if (WiFi.softAPgetStationNum() > 0) { // do not reconnect if any AP connection exists
            if(mStaConn != IN_AP_MODE) {
                mStaConn = IN_AP_MODE;
                // first time switch to AP Mode
                if (mScanActive) {
                    WiFi.scanDelete();
                    mScanActive = false;
                }
                DBGPRINTLN(F("AP client connected"));
                welcome(mApIp.toString(), "");
                WiFi.mode(WIFI_AP);
                mDns.start(53, "*", mApIp);
                mAppWifiCb(true);
            }
            mDns.processNextRequest();
            return;
        }
        else if(mStaConn == IN_AP_MODE) {
            mCnt = 0;
            mDns.stop();
            WiFi.mode(WIFI_AP_STA);
            mStaConn = DISCONNECTED;
        }
        mCnt++;

        uint8_t timeout = (mStaConn == DISCONNECTED) ? 10 : 20; // seconds
        if (mStaConn == CONNECTED) // connected but no ip
            timeout = 20;

        if(!mScanActive && mBSSIDList.empty() && (mStaConn == DISCONNECTED)) { // start scanning APs with the given SSID
            DBGPRINT(F("scanning APs with SSID "));
            DBGPRINTLN(String(mConfig->sys.stationSsid));
            mScanCnt = 0;
            mScanActive = true;
            #if defined(ESP8266)
            WiFi.scanNetworks(true, false, 0U, (uint8_t *)mConfig->sys.stationSsid);
            #else
            WiFi.scanNetworks(true, false, false, 300U, 0U, mConfig->sys.stationSsid);
            #endif
            return;
        }
        DBGPRINT(F("reconnect in "));
        DBGPRINT(String(timeout-mCnt));
        DBGPRINTLN(F(" seconds"));
        if(mScanActive) {
            getBSSIDs();
            if((!mScanActive) && (!mBSSIDList.empty()))  // scan completed
                if ((mCnt % timeout) < timeout - 2)
                    mCnt = timeout - 2;
        }
        if((mCnt % timeout) == 0) { // try to reconnect after x sec without connection
            mStaConn = CONNECTING;
            WiFi.disconnect();

            if(mBSSIDList.size() > 0) { // get first BSSID in list
                DBGPRINT(F("try to connect to AP with BSSID:"));
                uint8_t bssid[6];
                for (int j = 0; j < 6; j++) {
                    bssid[j] = mBSSIDList.front();
                    mBSSIDList.pop_front();
                    DBGPRINT(" "  + String(bssid[j], HEX));
                }
                DBGPRINTLN("");
                WiFi.begin(mConfig->sys.stationSsid, mConfig->sys.stationPwd, 0, &bssid[0]);
            }
            else
                mStaConn = DISCONNECTED;

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
    DBGPRINT(String(VERSION_MAJOR));
    DBGPRINT(F("."));
    DBGPRINT(String(VERSION_MINOR));
    DBGPRINT(F("."));
    DBGPRINTLN(String(VERSION_PATCH));
    DBGPRINT(F("Github Hash: "));
    DBGPRINTLN(String(AUTO_GIT_HASH));

    DBGPRINT(F("\n---------\nAP MODE\nSSID: "));
    DBGPRINTLN(WIFI_AP_SSID);
    DBGPRINT(F("PWD: "));
    DBGPRINTLN(WIFI_AP_PWD);
    DBGPRINT(F("IP Address: http://"));
    DBGPRINTLN(mApIp.toString());
    DBGPRINTLN(F("---------\n"));

    if(String(mConfig->sys.deviceName) != "")
        WiFi.hostname(mConfig->sys.deviceName);

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(mApIp, mApIp, IPAddress(255, 255, 255, 0));
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PWD);
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
    mBSSIDList.clear();
    if(String(mConfig->sys.deviceName) != "")
        WiFi.hostname(mConfig->sys.deviceName);
    WiFi.mode(WIFI_AP_STA);


    DBGPRINT(F("connect to network '"));
    DBGPRINT(mConfig->sys.stationSsid);
    DBGPRINTLN(F("' ..."));
}


//-----------------------------------------------------------------------------
bool ahoywifi::getNtpTime(void) {
    if(GOT_IP != mStaConn)
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
void ahoywifi::sortRSSI(int *sort, int n) {
    for (int i = 0; i < n; i++)
        sort[i] = i;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (WiFi.RSSI(sort[j]) > WiFi.RSSI(sort[i]))
                std::swap(sort[i], sort[j]);
}

//-----------------------------------------------------------------------------
void ahoywifi::scanAvailNetworks(void) {
    if(!mScanActive) {
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
    if (n < 0)
        return;
    if(n > 0) {
        int sort[n];
        sortRSSI(&sort[0], n);
        for (int i = 0; i < n; ++i) {
            nets[i]["ssid"] = WiFi.SSID(sort[i]);
            nets[i]["rssi"] = WiFi.RSSI(sort[i]);
        }
    }
    mScanActive = false;
    WiFi.scanDelete();
    if(mStaConn == IN_AP_MODE)
        WiFi.mode(WIFI_AP);
}

//-----------------------------------------------------------------------------
void ahoywifi::getBSSIDs() {
    int n = WiFi.scanComplete();
    if (n < 0) {
        if (++mScanCnt < 20)
            return;
    }
    if(n > 0) {
        mBSSIDList.clear();
        int sort[n];
        sortRSSI(&sort[0], n);
        for (int i = 0; i < n; i++) {
            DBGPRINT("BSSID " + String(i) + ":");
            uint8_t *bssid = WiFi.BSSID(sort[i]);
            for (int j = 0; j < 6; j++){
                DBGPRINT(" " + String(bssid[j], HEX));
                mBSSIDList.push_back(bssid[j]);
            }
            DBGPRINTLN("");
        }
    }
    mScanActive = false;
    WiFi.scanDelete();
}

//-----------------------------------------------------------------------------
void ahoywifi::connectionEvent(WiFiStatus_t status) {
    DPRINTLN(DBG_INFO, "connectionEvent");

    switch(status) {
        case CONNECTED:
            if(mStaConn != CONNECTED) {
                mStaConn = CONNECTED;
                DBGPRINTLN(F("\n[WiFi] Connected"));
            }
            break;

        case GOT_IP:
            mStaConn = GOT_IP;
            if (mScanActive) {  // maybe another scan has started
                 WiFi.scanDelete();
                 mScanActive = false;
            }
            welcome(WiFi.localIP().toString(), F(" (Station)"));
            WiFi.softAPdisconnect();
            WiFi.mode(WIFI_STA);
            DBGPRINTLN(F("[WiFi] AP disabled"));
            delay(100);
            mAppWifiCb(true);
            break;

        case DISCONNECTED:
            if(mStaConn != CONNECTING) {
                mStaConn = DISCONNECTED;
                mCnt       = 5;     // try to reconnect in 5 sec
                setupWifi();        // reconnect with AP / Station setup
                mAppWifiCb(false);
                DPRINTLN(DBG_INFO, "[WiFi] Connection Lost");
            }
            break;

        default:
            break;
    }
}


//-----------------------------------------------------------------------------
#if defined(ESP8266)
    //-------------------------------------------------------------------------
    void ahoywifi::onConnect(const WiFiEventStationModeConnected& event) {
        connectionEvent(CONNECTED);
    }

    //-------------------------------------------------------------------------
    void ahoywifi::onGotIP(const WiFiEventStationModeGotIP& event) {
        connectionEvent(GOT_IP);
    }

    //-------------------------------------------------------------------------
    void ahoywifi::onDisconnect(const WiFiEventStationModeDisconnected& event) {
        connectionEvent(DISCONNECTED);
    }

#else
    //-------------------------------------------------------------------------
    void ahoywifi::onWiFiEvent(WiFiEvent_t event) {
        DBGPRINT(F("Wifi event: "));
        DBGPRINTLN(String(event));

        switch(event) {
            case SYSTEM_EVENT_STA_CONNECTED:
                connectionEvent(CONNECTED);
                break;

            case SYSTEM_EVENT_STA_GOT_IP:
                connectionEvent(GOT_IP);
                break;

            case SYSTEM_EVENT_STA_DISCONNECTED:
                connectionEvent(DISCONNECTED);
                break;

            default:
                break;
        }
    }
#endif


//-----------------------------------------------------------------------------
void ahoywifi::welcome(String ip, String mode) {
    DBGPRINTLN(F("\n\n--------------------------------"));
    DBGPRINTLN(F("Welcome to AHOY!"));
    DBGPRINT(F("\npoint your browser to http://"));
    DBGPRINT(ip);
    DBGPRINTLN(mode);
    DBGPRINTLN(F("to configure your device"));
    DBGPRINTLN(F("--------------------------------\n"));
}
