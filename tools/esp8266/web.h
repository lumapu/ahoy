//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __WEB_H__
#define __WEB_H__

#include "dbg.h"
#ifdef ESP8266
    #include <ESP8266WebServer.h>
    #include <ESP8266HTTPUpdateServer.h>
#elif defined(ESP32)
    #include <WebServer.h>
    #include <HTTPUpdateServer.h>
#endif

#include "app.h"

class app;

class web {
    public:
        web(app *main, sysConfig_t *sysCfg, config_t *config, char version[]);
        ~web() {}

        void setup(void);
        void loop(void);

        void showIndex(void);
        void showCss(void);
        void showFavicon(void);
        void showNotFound(void);
        void showUptime(void);
        void showReboot(void);
        void showErase();
        void showFactoryRst(void);
        void showSetup(void);
        void showSave(void);

        void showStatistics(void);
        void showVisualization(void);
        void showLiveData(void);
        void showJson(void);
        void showWebApi(void);

    private:
        #ifdef ESP8266
            ESP8266WebServer *mWeb;
            ESP8266HTTPUpdateServer *mUpdater;
        #elif defined(ESP32)
            WebServer *mWeb;
            HTTPUpdateServer *mUpdater;
        #endif

        config_t *mConfig;
        sysConfig_t *mSysCfg;
        char *mVersion;
        app *mMain;
};

#endif /*__WEB_H__*/
