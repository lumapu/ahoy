#ifndef __WEB_API_H__
#define __WEB_API_H__

#include "dbg.h"
#include "ESPAsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include "app.h"


class app;

class webApi {
    public: 
        webApi(AsyncWebServer *srv, app *app, sysConfig_t *sysCfg, config_t *config, statistics_t *stat, char version[]);

        void setup(void);
        void loop(void);

    private:
        void onSystem(AsyncWebServerRequest *request);
        void onStatistics(AsyncWebServerRequest *request);
        void onInverterList(AsyncWebServerRequest *request);
        void onMqtt(AsyncWebServerRequest *request);
        void onNtp(AsyncWebServerRequest *request);
        void onPinout(AsyncWebServerRequest *request);
        void onRadio(AsyncWebServerRequest *request);
        void onSerial(AsyncWebServerRequest *request);

        AsyncWebServer *mSrv;
        app *mApp;

        config_t *mConfig;
        sysConfig_t *mSysCfg;
        statistics_t *mStat;
        char *mVersion;
};

#endif /*__WEB_API_H__*/
