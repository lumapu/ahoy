#ifndef __WEB_API_H__
#define __WEB_API_H__

#include "dbg.h"
#include "ESPAsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include "AsyncJson.h"
#include "app.h"


class app;

class webApi {
    public: 
        webApi(AsyncWebServer *srv, app *app, sysConfig_t *sysCfg, config_t *config, statistics_t *stat, char version[]);

        void setup(void);
        void loop(void);

    private:
        void onApi(AsyncWebServerRequest *request);

        void getSystem(JsonObject obj);
        void getStatistics(JsonObject obj);
        void getInverterList(JsonObject obj);
        void getMqtt(JsonObject obj);
        void getNtp(JsonObject obj);
        void getPinout(JsonObject obj);
        void getRadio(JsonObject obj);
        void getSerial(JsonObject obj);

        void getNotFound(JsonObject obj, String url);
        void getIndex(JsonObject obj);
        void getSetup(JsonObject obj);

        AsyncWebServer *mSrv;
        app *mApp;

        config_t *mConfig;
        sysConfig_t *mSysCfg;
        statistics_t *mStat;
        char *mVersion;
};

#endif /*__WEB_API_H__*/
