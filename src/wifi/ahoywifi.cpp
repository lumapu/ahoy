//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#if !defined(ETHERNET)
#if defined(ESP32) && defined(F)
  #undef F
  #define F(sl) (sl)
#endif
#include "ahoywifi.h"

#if defined(ESP32)
#include <ESPmDNS.h>
#else
#include <ESP8266mDNS.h>
#endif

// NTP CONFIG
#define NTP_PACKET_SIZE     48

//-----------------------------------------------------------------------------
ahoywifi::ahoywifi() : mApIp(192, 168, 4, 1) {}


/**
 * TODO: ESP32 has native strongest AP support!
 *       WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
         WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
*/

//-----------------------------------------------------------------------------
void ahoywifi::setup(settings_t *config, uint32_t *utcTimestamp, appWifiCb cb) {
    mConfig = config;
    mUtcTimestamp = utcTimestamp;
    mAppWifiCb = cb;

    mGotDisconnect = false;
    mStaConn = DISCONNECTED;
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
        #if defined(FB_WIFI_OVERRIDDEN)
            snprintf(mConfig->sys.stationSsid, SSID_LEN, "%s", FB_WIFI_SSID);
            snprintf(mConfig->sys.stationPwd, PWD_LEN, "%s", FB_WIFI_PWD);
            setupStation();
        #else
            if(mConfig->valid) {
                if(strncmp(mConfig->sys.stationSsid, FB_WIFI_SSID, 14) != 0)
                    setupStation();
            }
        #endif
    #endif
}


void ahoywifi::tickWifiLoop() {
    static const uint8_t TIMEOUT = 20;
    static const uint8_t SCAN_TIMEOUT = 10;
    #if !defined(AP_ONLY)

    mCnt++;

    switch (mStaConn) {
        case IN_STA_MODE:
            // Nothing to do
            if (mGotDisconnect) {
                mStaConn = RESET;
            }
            #if !defined(ESP32)
            MDNS.update();
            #endif
            return;
        case IN_AP_MODE:
            if (WiFi.softAPgetStationNum() == 0) {
                mCnt = 0;
                mDns.stop();
                WiFi.mode(WIFI_AP_STA);
                mStaConn = DISCONNECTED;
            } else {
                mDns.processNextRequest();
                return;
            }
            break;
        case DISCONNECTED:
            if (WiFi.softAPgetStationNum() > 0) {
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
                mDns.processNextRequest();
                return;
            } else if (!mScanActive) {
                DBGPRINT(F("scanning APs with SSID "));
                DBGPRINTLN(String(mConfig->sys.stationSsid));
                mScanCnt = 0;
                mCnt = 0;
                mScanActive = true;
#if defined(ESP8266)
                WiFi.scanNetworks(true, true, 0U, ([this]() {
                    if (mConfig->sys.isHidden)
                        return (uint8_t*)NULL;
                    return (uint8_t*)(mConfig->sys.stationSsid);
                    })());
#else
                WiFi.scanNetworks(true, true, false, 300U, 0U, ([this]() {
                    if (mConfig->sys.isHidden)
                        return (char*)NULL;
                    return (mConfig->sys.stationSsid);
                    })());
#endif
                return;
            } else if(getBSSIDs()) {
                // Scan ready
                mStaConn = SCAN_READY;
            } else {
                // In case of a timeout, what do we do?
                // For now we start scanning again as the original code did.
                // Would be better to into PA mode

                if (isTimeout(SCAN_TIMEOUT)) {
                    WiFi.scanDelete();
                    mScanActive = false;
                }
            }
            break;
        case SCAN_READY:
                mStaConn = CONNECTING;
                mCnt = 0;
                DBGPRINT(F("try to connect to AP with BSSID:"));
                uint8_t bssid[6];
                for (int j = 0; j < 6; j++) {
                    bssid[j] = mBSSIDList.front();
                    mBSSIDList.pop_front();
                    DBGPRINT(" "  + String(bssid[j], HEX));
                }
                DBGPRINTLN("");
                mGotDisconnect = false;
                WiFi.begin(mConfig->sys.stationSsid, mConfig->sys.stationPwd, 0, &bssid[0]);

                break;
        case CONNECTING:
            if (isTimeout(TIMEOUT)) {
                WiFi.disconnect();
                mStaConn = mBSSIDList.empty() ? DISCONNECTED : SCAN_READY;
            }
            break;
        case CONNECTED:
            // Connection but no IP yet
            if (isTimeout(TIMEOUT) || mGotDisconnect) {
                mStaConn = RESET;
            }
            break;
        case GOT_IP:
            welcome(WiFi.localIP().toString(), F(" (Station)"));
            WiFi.softAPdisconnect();
            WiFi.mode(WIFI_STA);
            DBGPRINTLN(F("[WiFi] AP disabled"));
            delay(100);
            mAppWifiCb(true);
            mGotDisconnect = false;
            mStaConn = IN_STA_MODE;

            if (!MDNS.begin(mConfig->sys.deviceName)) {
                DPRINTLN(DBG_ERROR, F("Error setting up MDNS responder!"));
            } else {
                DBGPRINT(F("[WiFi] mDNS established: "));
                DBGPRINT(mConfig->sys.deviceName);
                DBGPRINTLN(F(".local"));
            }

            break;
        case RESET:
            mGotDisconnect = false;
            mStaConn = DISCONNECTED;
            mCnt = 5;     // try to reconnect in 5 sec
            setupWifi();        // reconnect with AP / Station setup
            mAppWifiCb(false);
            DPRINTLN(DBG_INFO, "[WiFi] Connection Lost");
            break;
        default:
            DBGPRINTLN(F("Unhandled status"));
            break;
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
    DBGPRINTLN(mConfig->sys.apPwd);
    DBGPRINT(F("IP Address: http://"));
    DBGPRINTLN(mApIp.toString());
    DBGPRINTLN(F("---------\n"));

    if(String(mConfig->sys.deviceName) != "")
        WiFi.hostname(mConfig->sys.deviceName);

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(mApIp, mApIp, IPAddress(255, 255, 255, 0));
    WiFi.softAP(WIFI_AP_SSID, mConfig->sys.apPwd);
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
    if(IN_STA_MODE != mStaConn)
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
bool ahoywifi::getAvailNetworks(JsonObject obj) {
    JsonArray nets = obj.createNestedArray("networks");

    int n = WiFi.scanComplete();
    if (n < 0)
        return false;
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

    return true;
}

//-----------------------------------------------------------------------------
bool ahoywifi::getBSSIDs() {
    bool result = false;
    int n = WiFi.scanComplete();
    if (n < 0) {
        if (++mScanCnt < 20)
            return false;
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
        result = true;
    }
    mScanActive = false;
    WiFi.scanDelete();
    return result;
}

//-----------------------------------------------------------------------------
void ahoywifi::connectionEvent(WiFiStatus_t status) {
    DPRINTLN(DBG_INFO, "connectionEvent");

    switch(status) {
        case CONNECTED:
            if(mStaConn != CONNECTED) {
                mStaConn = CONNECTED;
                mGotDisconnect = false;
                DBGPRINTLN(F("\n[WiFi] Connected"));
            }
            break;

        case GOT_IP:
            mStaConn = GOT_IP;
            break;

        case DISCONNECTED:
            mGotDisconnect = true;
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

#endif /* !defined(ETHERNET) */
