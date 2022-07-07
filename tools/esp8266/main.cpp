//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#include "main.h"
#include "version.h"

#include "html/h/style_css.h"
#include "html/h/setup_html.h"


//-----------------------------------------------------------------------------
Main::Main(void) {
    mDns     = new DNSServer();
    mWeb     = new ESP8266WebServer(80);
    mUpdater = new ESP8266HTTPUpdateServer();
    mUdp     = new WiFiUDP();

    mApActive          = true;
    mWifiSettingsValid = false;
    mSettingsValid     = false;

    mLimit       = 10;
    mNextTryTs   = 0;
    mApLastTick  = 0;

    snprintf(mVersion, 12, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

    memset(&mDeviceName, 0, DEVNAME_LEN);

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
    bool startAp = mApActive;
    mLimit = timeout;

    mWeb->on("/setup",     std::bind(&Main::showSetup,      this));
    mWeb->on("/save",      std::bind(&Main::showSave,       this));
    mWeb->on("/uptime",    std::bind(&Main::showUptime,     this));
    mWeb->on("/time",      std::bind(&Main::showTime,       this));
    mWeb->on("/style.css", std::bind(&Main::showCss,        this));
    mWeb->on("/reboot",    std::bind(&Main::showReboot,     this));
    mWeb->on("/factory",   std::bind(&Main::showFactoryRst, this));
    mWeb->onNotFound (     std::bind(&Main::showNotFound,   this));

    startAp = getConfig();

#ifndef AP_ONLY
    if(false == startAp)
        startAp = setupStation(timeout);
#endif

    mUpdater->setup(mWeb);
    mApActive = startAp;
}


//-----------------------------------------------------------------------------
void Main::loop(void) {
    //DPRINTLN(DBG_VERBOSE, F("M"));
    if(mApActive) {
        mDns->processNextRequest();
#ifndef AP_ONLY
        if(checkTicker(&mNextTryTs, (WIFI_AP_ACTIVE_TIME * 1000))) {
            mApActive = setupStation(mLimit);
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
            if(!mApActive) {
                mTimestamp  = getNtpTime();
                DPRINTLN(DBG_INFO, "[NTP]: " + getDateTimeStr(mTimestamp));
            }
        }

        /*if(++mHeapStatCnt >= 10) {
            mHeapStatCnt = 0;
            stats();
        }*/
    }
}


//-----------------------------------------------------------------------------
bool Main::getConfig(void) {
    DPRINTLN(DBG_VERBOSE, F("Main::getConfig"));
    bool mApActive = false;

    mWifiSettingsValid = checkEEpCrc(ADDR_START, ADDR_WIFI_CRC, ADDR_WIFI_CRC);
    mSettingsValid = checkEEpCrc(ADDR_START_SETTINGS, ((ADDR_NEXT)-(ADDR_START_SETTINGS)), ADDR_SETTINGS_CRC);

    if(mWifiSettingsValid) {
        mEep->read(ADDR_SSID,    mStationSsid, SSID_LEN);
        mEep->read(ADDR_PWD,     mStationPwd, PWD_LEN);
        mEep->read(ADDR_DEVNAME, mDeviceName, DEVNAME_LEN);
    }

    if((!mWifiSettingsValid) || (mStationSsid[0] == 0xff)) {
        snprintf(mStationSsid, SSID_LEN,    "%s", FB_WIFI_SSID);
        snprintf(mStationPwd,  PWD_LEN,     "%s", FB_WIFI_PWD);
        snprintf(mDeviceName,  DEVNAME_LEN, "%s", DEF_DEVICE_NAME);
    }

    return mApActive;
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

    mWeb->onNotFound([&]() {
        showSetup();
    });
    mWeb->on("/", std::bind(&Main::showSetup, this));

    mWeb->begin();
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
    WiFi.begin(mStationSsid, mStationPwd);
    if(String(mDeviceName) != "")
        WiFi.hostname(mDeviceName);

    delay(2000);
    DPRINTLN(DBG_INFO, F("connect to network '") + String(mStationSsid) + F("' ..."));
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
void Main::showSetup(void) {
    DPRINTLN(DBG_VERBOSE, F("Main::showSetup"));
    String html = FPSTR(setup_html);
    html.replace(F("{SSID}"), mStationSsid);
    // PWD will be left at the default value (for protection)
    // -> the PWD will only be changed if it does not match the default "{PWD}"
    html.replace(F("{DEVICE}"), String(mDeviceName));
    html.replace(F("{VERSION}"), String(mVersion));
    if(mApActive)
        html.replace("{IP}", String(F("http://192.168.1.1")));
    else
        html.replace("{IP}", (F("http://") + String(WiFi.localIP().toString())));

    mWeb->send(200, F("text/html"), html);
}


//-----------------------------------------------------------------------------
void Main::showCss(void) {
    DPRINTLN(DBG_VERBOSE, F("Main::showCss"));
    mWeb->send(200, "text/css", FPSTR(style_css));
}


//-----------------------------------------------------------------------------
void Main::showSave(void) {
    DPRINTLN(DBG_VERBOSE, F("Main::showSave"));
    saveValues(true);
}


//-----------------------------------------------------------------------------
void Main::saveValues(bool webSend = true) {
    DPRINTLN(DBG_VERBOSE, F("Main::saveValues"));
    if(mWeb->args() > 0) {
        if(mWeb->arg("ssid") != "") {
            memset(mStationSsid, 0, SSID_LEN);
            mWeb->arg("ssid").toCharArray(mStationSsid, SSID_LEN);
            mEep->write(ADDR_SSID, mStationSsid, SSID_LEN);

            if(mWeb->arg("pwd") != "{PWD}") {
                memset(mStationPwd, 0, PWD_LEN);
                mWeb->arg("pwd").toCharArray(mStationPwd, PWD_LEN);
                mEep->write(ADDR_PWD, mStationPwd, PWD_LEN);
            }
        }

        memset(mDeviceName, 0, DEVNAME_LEN);
        mWeb->arg("device").toCharArray(mDeviceName, DEVNAME_LEN);
        mEep->write(ADDR_DEVNAME, mDeviceName, DEVNAME_LEN);
        mEep->commit();


        updateCrc();
        if(webSend) {
            if(mWeb->arg("reboot") == "on")
                showReboot();
            else // TODO: add device name as redirect in AP-mode
                mWeb->send(200, F("text/html"), F("<!doctype html><html><head><title>Setup saved</title><meta http-equiv=\"refresh\" content=\"0; URL=/setup\"></head><body>"
                             "<p>saved</p></body></html>"));
        }
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
void Main::showUptime(void) {
    //DPRINTLN(DBG_VERBOSE, F("Main::showUptime"));
    char time[20] = {0};

    int upTimeSc = uint32_t((mUptimeSecs) % 60);
    int upTimeMn = uint32_t((mUptimeSecs / (60)) % 60);
    int upTimeHr = uint32_t((mUptimeSecs / (60 * 60)) % 24);
    int upTimeDy = uint32_t((mUptimeSecs / (60 * 60 * 24)) % 365);

    snprintf(time, 20, "%d Tage, %02d:%02d:%02d", upTimeDy, upTimeHr, upTimeMn, upTimeSc);

    mWeb->send(200, "text/plain", String(time));
}


//-----------------------------------------------------------------------------
void Main::showTime(void) {
    //DPRINTLN(DBG_VERBOSE, F("Main::showTime"));
    mWeb->send(200, "text/plain", getDateTimeStr(mTimestamp));
}


//-----------------------------------------------------------------------------
void Main::showNotFound(void) {
    DPRINTLN(DBG_VERBOSE, F("Main::showNotFound - ") + mWeb->uri());
    String msg = F("File Not Found\n\nURI: ");
    msg += mWeb->uri();
    msg += F("\nMethod: ");
    msg += ( mWeb->method() == HTTP_GET ) ? "GET" : "POST";
    msg += F("\nArguments: ");
    msg += mWeb->args();
    msg += "\n";

    for(uint8_t i = 0; i < mWeb->args(); i++ ) {
        msg += " " + mWeb->argName(i) + ": " + mWeb->arg(i) + "\n";
    }

    mWeb->send(404, F("text/plain"), msg);
}


//-----------------------------------------------------------------------------
void Main::showReboot(void) {
    DPRINTLN(DBG_VERBOSE, F("Main::showReboot"));
    mWeb->send(200, F("text/html"), F("<!doctype html><html><head><title>Rebooting ...</title><meta http-equiv=\"refresh\" content=\"10; URL=/\"></head><body>rebooting ... auto reload after 10s</body></html>"));
    delay(1000);
    ESP.restart();
}



//-----------------------------------------------------------------------------
void Main::showFactoryRst(void) {
    DPRINTLN(DBG_VERBOSE, F("Main::showFactoryRst"));
    String content = "";
    int refresh = 3;
    if(mWeb->args() > 0) {
        if(mWeb->arg("reset").toInt() == 1) {
            eraseSettings(true);
            content = F("factory reset: success\n\nrebooting ... ");
            refresh = 10;
        }
        else {
            content = F("factory reset: aborted");
            refresh = 3;
        }
    }
    else {
        content = F("<h1>Factory Reset</h1>"
            "<p><a href=\"/factory?reset=1\">RESET</a><br/><br/><a href=\"/factory?reset=0\">CANCEL</a><br/></p>");
        refresh = 120;
    }
    mWeb->send(200, F("text/html"), F("<!doctype html><html><head><title>Factory Reset</title><meta http-equiv=\"refresh\" content=\"") + String(refresh) + F("; URL=/\"></head><body>") + content + F("</body></html>"));
    if(refresh == 10) {
        delay(1000);
        ESP.restart();
    }
}


//-----------------------------------------------------------------------------
time_t Main::getNtpTime(void) {
    //DPRINTLN(DBG_VERBOSE, F("Main::getNtpTime"));
    time_t date = 0;
    IPAddress timeServer;
    uint8_t buf[NTP_PACKET_SIZE];
    uint8_t retry = 0;

    WiFi.hostByName (TIMESERVER_NAME, timeServer);
    mUdp->begin(TIME_LOCAL_PORT);


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
String Main::getDateTimeStr(time_t t) {
    //DPRINTLN(DBG_VERBOSE, F("Main::getDateTimeStr"));
    char str[20] = {0};
    if(0 == t)
        sprintf(str, "n/a");
    else
        sprintf(str, "%04d-%02d-%02d %02d:%02d:%02d", year(t), month(t), day(t), hour(t), minute(t), second(t));
    return String(str);
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
