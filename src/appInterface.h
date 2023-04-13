//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __IAPP_H__
#define __IAPP_H__

#include "defines.h"
#include "hm/hmSystem.h"
#include "ESPAsyncWebServer.h"

// abstract interface to App. Make members of App accessible from child class
// like web or API without forward declaration
class IApp {
    public:
        virtual ~IApp() {}
        virtual bool saveSettings(bool stopFs) = 0;
        virtual bool readSettings(const char *path) = 0;
        virtual bool eraseSettings(bool eraseWifi) = 0;
        virtual bool getSavePending() = 0;
        virtual bool getLastSaveSucceed() = 0;
        virtual bool getShouldReboot() = 0;
        virtual void setOnUpdate() = 0;
        virtual void setRebootFlag() = 0;
        virtual const char *getVersion() = 0;
        virtual statistics_t *getStatistics() = 0;
        virtual void scanAvailNetworks() = 0;
        virtual void getAvailNetworks(JsonObject obj) = 0;

        virtual uint32_t getUptime() = 0;
        virtual uint32_t getTimestamp() = 0;
        virtual uint32_t getSunrise() = 0;
        virtual uint32_t getSunset() = 0;
        virtual void setTimestamp(uint32_t newTime) = 0;
        virtual String getTimeStr(uint32_t offset) = 0;
        virtual uint32_t getTimezoneOffset() = 0;
        virtual void getSchedulerInfo(uint8_t *max) = 0;
        virtual void getSchedulerNames() = 0;

        virtual bool getRebootRequestState() = 0;
        virtual bool getSettingsValid() = 0;
        virtual void setMqttDiscoveryFlag() = 0;
        virtual void setMqttPowerLimitAck(Inverter<> *iv) = 0;

        virtual void ivSendHighPrio(Inverter<> *iv) = 0;

        virtual bool getMqttIsConnected() = 0;
        virtual uint32_t getMqttRxCnt() = 0;
        virtual uint32_t getMqttTxCnt() = 0;

        virtual bool getProtection(AsyncWebServerRequest *request) = 0;
};

#endif /*__IAPP_H__*/
