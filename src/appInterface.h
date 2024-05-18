//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __IAPP_H__
#define __IAPP_H__

#include "defines.h"
#include "ESPAsyncWebServer.h"
#include "utils/scheduler.h"

// abstract interface to App. Make members of App accessible from child class
// like web or API without forward declaration
class IApp {
    public:
        virtual ~IApp() {}
        virtual bool saveSettings(bool reboot) = 0;
        virtual void initInverter(uint8_t id) = 0;
        virtual bool readSettings(const char *path) = 0;
        virtual bool eraseSettings(bool eraseWifi) = 0;
        virtual bool getSavePending() = 0;
        virtual bool getLastSaveSucceed() = 0;
        virtual bool getShouldReboot() = 0;
        virtual void setRebootFlag() = 0;
        virtual const char *getVersion() = 0;
        virtual const char *getVersionModules() = 0;

        virtual void addOnce(ah::scdCb c, uint32_t timeout, const char *name) = 0;

        virtual bool getAvailNetworks(JsonObject obj) = 0;
        virtual void setupStation(void) = 0;
        virtual bool getWasInCh12to14(void) const = 0;
        virtual String getIp(void) = 0;
        virtual bool isApActive(void) = 0;

        virtual uint32_t getUptime() = 0;
        virtual uint32_t getTimestamp() = 0;
        virtual uint64_t getTimestampMs() = 0;
        virtual uint32_t getSunrise() = 0;
        virtual uint32_t getSunset() = 0;
        virtual void setTimestamp(uint32_t newTime) = 0;
        virtual uint32_t getTimezoneOffset() = 0;
        virtual void getSchedulerInfo(uint8_t *max) = 0;
        virtual void getSchedulerNames() = 0;

        virtual void triggerTickSend(uint8_t id) = 0;

        virtual bool getRebootRequestState() = 0;
        virtual bool getSettingsValid() = 0;
        virtual void setMqttDiscoveryFlag() = 0;
        virtual bool getMqttIsConnected() = 0;

        virtual bool getNrfEnabled() = 0;
        virtual bool getCmtEnabled() = 0;

        virtual uint32_t getMqttRxCnt() = 0;
        virtual uint32_t getMqttTxCnt() = 0;

        virtual void lock(bool fromWeb) = 0;
        virtual char *unlock(const char *clientIp, bool loginFromWeb) = 0;
        virtual void resetLockTimeout(void) = 0;
        virtual bool isProtected(const char *clientIp, const char *token, bool askedFromWeb) const = 0;

        virtual float getTotalMaxPower(void) = 0;
        virtual uint16_t getHistoryValue(uint8_t type, uint16_t i) = 0;
        virtual uint32_t getHistoryPeriod(uint8_t type) = 0;
        virtual uint16_t getHistoryMaxDay() = 0;
        virtual uint32_t getHistoryLastValueTs(uint8_t type) = 0;
        #if defined(ENABLE_HISTORY_LOAD_DATA)
        virtual void addValueToHistory(uint8_t historyType, uint8_t valueType, uint32_t value) = 0;
        #endif
        virtual void* getRadioObj(bool nrf) = 0;
};

#endif /*__IAPP_H__*/
