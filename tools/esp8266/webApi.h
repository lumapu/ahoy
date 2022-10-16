#ifndef __WEB_API_H__
#define __WEB_API_H__

#include "dbg.h"
#ifdef ESP32
    #include "AsyncTCP.h"
#else
    #include "ESPAsyncTCP.h"
#endif
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
        void onApiPost(AsyncWebServerRequest *request);
        void onApiPostBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
        void getNotFound(JsonObject obj, String url);
        void onDwnldSetup(AsyncWebServerRequest *request);

        void getSystem(JsonObject obj);
        void getStatistics(JsonObject obj);
        void getInverterList(JsonObject obj);
        void getMqtt(JsonObject obj);
        void getNtp(JsonObject obj);
        void getPinout(JsonObject obj);
        void getRadio(JsonObject obj);
        void getSerial(JsonObject obj);


        void getIndex(JsonObject obj);
        void getSetup(JsonObject obj);
        void getLive(JsonObject obj);
        void getRecord(JsonObject obj, record_t<> *rec);

        bool setCtrl(DynamicJsonDocument jsonIn, JsonObject jsonOut);
        bool setSetup(DynamicJsonDocument jsonIn, JsonObject jsonOut);

        Inverter<> *getInverter(DynamicJsonDocument jsonIn, JsonObject jsonOut);

        double round3(double value) {
           return (int)(value * 1000 + 0.5) / 1000.0;
        }

        AsyncWebServer *mSrv;
        app *mApp;

        config_t *mConfig;
        sysConfig_t *mSysCfg;
        statistics_t *mStat;
        char *mVersion;
};

#endif /*__WEB_API_H__*/
