#ifndef __WEB_API_H__
#define __WEB_API_H__

#include "../utils/dbg.h"
#ifdef ESP32
    #include "AsyncTCP.h"
#else
    #include "ESPAsyncTCP.h"
#endif
#include "ESPAsyncWebServer.h"
#include "AsyncJson.h"
#include "../app.h"


class app;

class webApi {
    public: 
        webApi(AsyncWebServer *srv, app *app, settings_t *config, statistics_t *stat, char version[]);

        void setup(void);
        void loop(void);

        uint32_t getTimezoneOffset() {
            return mTimezoneOffset;
        }

        void ctrlRequest(JsonObject obj);

    private:
        void onApi(AsyncWebServerRequest *request);
        void onApiPost(AsyncWebServerRequest *request);
        void onApiPostBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
        void getNotFound(JsonObject obj, String url);
        void onDwnldSetup(AsyncWebServerRequest *request);

        void getSysInfo(JsonObject obj);
        void getHtmlSystem(JsonObject obj);
        void getHtmlLogout(JsonObject obj);
        void getHtmlSave(JsonObject obj);
        void getReboot(JsonObject obj);
        void getStatistics(JsonObject obj);
        void getInverterList(JsonObject obj);
        void getMqtt(JsonObject obj);
        void getNtp(JsonObject obj);
        void getSun(JsonObject obj);
        void getPinout(JsonObject obj);
        void getRadio(JsonObject obj);
        void getSerial(JsonObject obj);
        void getStaticIp(JsonObject obj);

        void getMenu(JsonObject obj);
        void getIndex(JsonObject obj);
        void getSetup(JsonObject obj);
        void getNetworks(JsonObject obj);
        void getLive(JsonObject obj);
        void getRecord(JsonObject obj, record_t<> *rec);

        bool setCtrl(JsonObject jsonIn, JsonObject jsonOut);
        bool setSetup(JsonObject jsonIn, JsonObject jsonOut);

        AsyncWebServer *mSrv;
        app *mApp;

        settings_t *mConfig;
        statistics_t *mStat;
        char *mVersion;

        uint32_t mTimezoneOffset;
};

#endif /*__WEB_API_H__*/
