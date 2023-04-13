//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __APP_H__
#define __APP_H__

#include <Arduino.h>
#include <ArduinoJson.h>
#include <RF24.h>
#include <RF24_config.h>

#include "appInterface.h"
#include "config/settings.h"
#include "defines.h"
#include "hm/hmPayload.h"
#include "hm/hmSystem.h"
#include "hm/miPayload.h"
#include "publisher/pubMqtt.h"
#include "publisher/pubSerial.h"
#include "utils/crc.h"
#include "utils/dbg.h"
#include "utils/scheduler.h"
#include "web/RestApi.h"
#include "web/web.h"
#include "wifi/ahoywifi.h"

// convert degrees and radians for sun calculation
#define SIN(x) (sin(radians(x)))
#define COS(x) (cos(radians(x)))
#define ASIN(x) (degrees(asin(x)))
#define ACOS(x) (degrees(acos(x)))

typedef HmSystem<MAX_NUM_INVERTERS> HmSystemType;
typedef HmPayload<HmSystemType> PayloadType;
typedef MiPayload<HmSystemType> MiPayloadType;
typedef Web<HmSystemType> WebType;
typedef RestApi<HmSystemType> RestApiType;
typedef PubMqtt<HmSystemType> PubMqttType;
typedef PubSerial<HmSystemType> PubSerialType;

// PLUGINS
#include "plugins/Display/Display.h"
typedef Display<HmSystemType> DisplayType;

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
            mSys.Radio.handleIntr();
        }

        uint32_t getUptime() {
            return Scheduler::getUptime();
        }

        uint32_t getTimestamp() {
            return Scheduler::getTimestamp();
        }

        bool saveSettings(bool reboot) {
            mShowRebootRequest = true; // only message on index, no reboot
            mSavePending = true;
            mSaveReboot = reboot;
            once(std::bind(&app::tickSave, this), 3, "save");
            return true;
        }

        bool readSettings(const char *path) {
            return mSettings.readSettings(path);
        }

        bool eraseSettings(bool eraseWifi = false) {
            return mSettings.eraseSettings(eraseWifi);
        }

        bool getSavePending() {
            return mSavePending;
        }

        bool getLastSaveSucceed() {
            return mSettings.getLastSaveSucceed();
        }

        bool getShouldReboot() {
            return mSaveReboot;
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
            if(mIVCommunicationOn) { // only send commands if communcation is enabled
                if (iv->ivGen == IV_HM)
                    mPayload.ivSendHighPrio(iv);
                else if (iv->ivGen == IV_MI)
                    mMiPayload.ivSendHighPrio(iv);
            }
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

        bool getProtection(AsyncWebServerRequest *request) {
            return mWeb.isProtected(request);
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
            DPRINT(DBG_DEBUG, F("setTimestamp: "));
            DBGPRINTLN(String(newTime));
            if(0 == newTime)
                mWifi.getNtpTime();
            else
                Scheduler::setTimestamp(newTime);
        }

        HmSystemType mSys;

    private:
        typedef std::function<void()> innerLoopCb;

        void resetSystem(void);

        void payloadEventListener(uint8_t cmd) {
            #if !defined(AP_ONLY)
            if (mMqttEnabled)
                mMqtt.payloadEventListener(cmd);
            #endif
            if(mConfig->plugin.display.type != 0)
               mDisplay.payloadEventListener(cmd);
        }

        void mqttSubRxCb(JsonObject obj);

        void setupLed();
        void updateLed();

        void tickReboot(void) {
            DPRINTLN(DBG_INFO, F("Rebooting..."));
            onWifi(false);
            ah::Scheduler::resetTicker();
            WiFi.disconnect();
            delay(200);
            ESP.restart();
        }

        void tickSave(void) {
            if(!mSettings.saveSettings())
                mSaveReboot = false;
            mSavePending = false;

            if(mSaveReboot)
                setRebootFlag();
        }

        void tickNtpUpdate(void);
        void tickCalcSunrise(void);
        void tickIVCommunication(void);
        void tickSun(void);
        void tickComm(void);
        void tickSend(void);
        void tickMinute(void);
        void tickZeroValues(void);
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
        MiPayloadType mMiPayload;
        PubSerialType mPubSerial;

        char mVersion[12];
        settings mSettings;
        settings_t *mConfig;
        bool mSavePending;
        bool mSaveReboot;

        uint8_t mSendLastIvId;
        bool mSendFirst;

        statistics_t mStat;

        // mqtt
        PubMqttType mMqtt;
        bool mMqttReconnect;
        bool mMqttEnabled;

        // sun
        int32_t mCalculatedTimezoneOffset;
        uint32_t mSunrise, mSunset;

        // plugins
        DisplayType mDisplay;
};

#endif /*__APP_H__*/
