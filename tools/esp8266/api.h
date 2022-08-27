#ifndef __API_H__
#define __API_H__

#include "dbg.h"
#include "ESPAsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include "app.h"


class app;

class api {
    public: 
        api(AsyncWebServer *srv, app *app, sysConfig_t *sysCfg, config_t *config, char version[]);

        void setup(void);
        void loop(void);

    private:
        void onSystem(AsyncWebServerRequest *request);
        void onInverterList(AsyncWebServerRequest *request);
        void onMqtt(AsyncWebServerRequest *request);

        AsyncWebServer *mSrv;
        app *mApp;

        config_t *mConfig;
        sysConfig_t *mSysCfg;
        char *mVersion;
};

#endif /*__API_H__*/
