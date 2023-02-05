//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __APP_H__
#define __APP_H__


#include "utils/dbg.h"
#include <Arduino.h>
#include <RF24.h>
#include <RF24_config.h>
#include <ArduinoJson.h>

#include "appInterface.h"

#include "config/settings.h"
#include "defines.h"
#include "utils/crc.h"
#include "utils/scheduler.h"

#include "hm/hmSystem.h"
#include "hm/payload.h"
#include "wifi/ahoywifi.h"
#include "web/web.h"
#include "web/RestApi.h"

#include "publisher/pubMqtt.h"
#include "publisher/pubSerial.h"


// convert degrees and radians for sun calculation
#define SIN(x) (sin(radians(x)))
#define COS(x) (cos(radians(x)))
#define ASIN(x) (degrees(asin(x)))
#define ACOS(x) (degrees(acos(x)))

typedef HmSystem<MAX_NUM_INVERTERS> HmSystemType;
typedef Payload<HmSystemType> PayloadType;
typedef Web<HmSystemType> WebType;
typedef RestApi<HmSystemType> RestApiType;
typedef PubMqtt<HmSystemType> PubMqttType;
typedef PubSerial<HmSystemType> PubSerialType;

// PLUGINS
#include "plugins/MonochromeDisplay/MonochromeDisplay.h"
typedef MonochromeDisplay<HmSystemType> MonoDisplayType;


class app : public IApp, public ah::Scheduler {
    public:
        app();
        ~app() {}

        void setup(void);
        void loop(void);
        void loopStandard(void);
        void loopWifi(void);
        void onWifi(bool gotIp);
        void regularTickers(void);

        void handleIntr(void) {
            if(NULL != mSys)
                mSys->Radio.handleIntr();
        }

        uint32_t getUptime() {
            return Scheduler::getUptime();
        }

        uint32_t getTimestamp() {
            return Scheduler::getTimestamp();
        }

        bool saveSettings() {
            mShowRebootRequest = true;
            return mSettings.saveSettings();
        }

        bool readSettings(const char *path) {
            return mSettings.readSettings(path);
        }

        bool eraseSettings(bool eraseWifi = false) {
            return mSettings.eraseSettings(eraseWifi);
        }

        statistics_t *getStatistics() {
            return &mStat;
        }

        void scanAvailNetworks() {
            mWifi.scanAvailNetworks();
        }

        void getAvailNetworks(JsonObject obj) {
            mWifi.getAvailNetworks(obj);
        }

        void setOnUpdate() {
            onWifi(false);
        }

        void setRebootFlag() {
            once(std::bind(&app::tickReboot, this), 3, "rboot");
        }

        const char *getVersion() {
            return mVersion;
        }

        uint32_t getSunrise() {
            return mSunrise;
        }

        uint32_t getSunset() {
            return mSunset;
        }

        bool getSettingsValid() {
            return mSettings.getValid();
        }

        bool getRebootRequestState() {
            return mShowRebootRequest;
        }

        void setMqttDiscoveryFlag() {
            once(std::bind(&PubMqttType::sendDiscoveryConfig, &mMqtt), 1, "disCf");
        }

        void setMqttPowerLimitAck(Inverter<> *iv) {
            mMqtt.setPowerLimitAck(iv);
        }

        void ivSendHighPrio(Inverter<> *iv) {
            mPayload.ivSendHighPrio(iv);
        }

        bool getMqttIsConnected() {
            return mMqtt.isConnected();
        }

        uint32_t getMqttTxCnt() {
            return mMqtt.getTxCnt();
        }

        uint32_t getMqttRxCnt() {
            return mMqtt.getRxCnt();
        }

        bool getProtection() {
            return mWeb.getProtection();
        }

        uint8_t getIrqPin(void) {
            return mConfig->nrf.pinIrq;
        }

        String getTimeStr(uint32_t offset = 0) {
            char str[10];
            if(0 == mTimestamp)
                sprintf(str, "n/a");
            else
                sprintf(str, "%02d:%02d:%02d ", hour(mTimestamp + offset), minute(mTimestamp + offset), second(mTimestamp + offset));
            return String(str);
        }

        uint32_t getTimezoneOffset() {
            return mApi.getTimezoneOffset();
        }

        void getSchedulerInfo(uint8_t *max) {
            getStat(max);
        }

        void getSchedulerNames(void) {
            printSchedulers();
        }

        void setTimestamp(uint32_t newTime) {
            DPRINTLN(DBG_DEBUG, F("setTimestamp: ") + String(newTime));
            if(0 == newTime)
                mWifi.getNtpTime();
            else
                Scheduler::setTimestamp(newTime);
        }

        HmSystemType *mSys;

    private:
        typedef std::function<void()> innerLoopCb;

        void resetSystem(void);

        void payloadEventListener(uint8_t cmd) {
            #if !defined(AP_ONLY)
            mMqtt.payloadEventListener(cmd);
            #endif
            if(mConfig->plugin.display.type != 0)
                mMonoDisplay.payloadEventListener(cmd);
        }

        void mqttSubRxCb(JsonObject obj);

        void setupLed(void);
        void updateLed(void);

        void tickReboot(void) {
            DPRINTLN(DBG_INFO, F("Rebooting..."));
            onWifi(false);
            ah::Scheduler::resetTicker();
            WiFi.disconnect();
            ESP.restart();
        }

        void tickNtpUpdate(void);
        void tickCalcSunrise(void);
        void tickIVCommunication(void);
        void tickSun(void);
        void tickComm(void);
        void tickSend(void);
        void tickMidnight(void);
        /*void tickSerial(void) {
            if(Serial.available() == 0)
                return;

            uint8_t buf[80];
            uint8_t len = Serial.readBytes(buf, 80);
            DPRINTLN(DBG_INFO, "got serial data, len: " + String(len));
            for(uint8_t i = 0; i < len; i++) {
                if((0 != i) && (i % 8 == 0))
                    DBGPRINTLN("");
                DBGPRINT(String(buf[i], HEX) + " ");
            }
            DBGPRINTLN("");
        }*/

        innerLoopCb mInnerLoopCb;

        bool mShowRebootRequest;
        bool mIVCommunicationOn;

        ahoywifi mWifi;
        WebType mWeb;
        RestApiType mApi;
        PayloadType mPayload;
        PubSerialType mPubSerial;

        char mVersion[12];
        settings mSettings;
        settings_t *mConfig;

        uint8_t mSendLastIvId;

        statistics_t mStat;

        // mqtt
        PubMqttType mMqtt;
        bool mMqttReconnect;
        bool mMqttEnabled;

        // sun
        int32_t mCalculatedTimezoneOffset;
        uint32_t mSunrise, mSunset;

        // plugins
        MonoDisplayType mMonoDisplay;
};

#endif /*__APP_H__*/
