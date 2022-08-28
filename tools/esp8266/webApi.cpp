//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#if defined(ESP32) && defined(F)
  #undef F
  #define F(sl) (sl)
#endif

#include "webApi.h"
#include "AsyncJson.h"

//-----------------------------------------------------------------------------
webApi::webApi(AsyncWebServer *srv, app *app, sysConfig_t *sysCfg, config_t *config, statistics_t *stat, char version[]) {
    mSrv = srv;
    mApp = app;
    mSysCfg  = sysCfg;
    mConfig  = config;
    mStat    = stat;
    mVersion = version;
}


//-----------------------------------------------------------------------------
void webApi::setup(void) {
    mSrv->on("/api/system",        HTTP_GET, std::bind(&webApi::onSystem,       this, std::placeholders::_1));
    mSrv->on("/api/statistics",    HTTP_GET, std::bind(&webApi::onStatistics,   this, std::placeholders::_1));
    mSrv->on("/api/inverter/list", HTTP_GET, std::bind(&webApi::onInverterList, this, std::placeholders::_1));
    mSrv->on("/api/mqtt",          HTTP_GET, std::bind(&webApi::onMqtt,         this, std::placeholders::_1));
    mSrv->on("/api/ntp",           HTTP_GET, std::bind(&webApi::onNtp,          this, std::placeholders::_1));
    mSrv->on("/api/pinout",        HTTP_GET, std::bind(&webApi::onPinout,       this, std::placeholders::_1));
    mSrv->on("/api/radio",         HTTP_GET, std::bind(&webApi::onRadio,        this, std::placeholders::_1));
    mSrv->on("/api/serial",        HTTP_GET, std::bind(&webApi::onSerial,       this, std::placeholders::_1));
}


//-----------------------------------------------------------------------------
void webApi::loop(void) {

}


//-----------------------------------------------------------------------------
void webApi::onSystem(AsyncWebServerRequest *request) {
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    root[F("ssid")]        = mSysCfg->stationSsid;
    root[F("device_name")] = mSysCfg->deviceName;
    root[F("version")]     = String(mVersion);
    root[F("build")]       = String(AUTO_GIT_HASH);
    root[F("ts_uptime")]   = mApp->getUptime();
    root[F("ts_now")]      = mApp->getTimestamp();

    response->setLength();
    //response->addHeader("Access-Control-Allow-Origin", "*");
    //response->addHeader("Access-Control-Allow-Headers", "content-type");
    request->send(response);
}


//-----------------------------------------------------------------------------
void webApi::onStatistics(AsyncWebServerRequest *request) {
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    root[F("rx_success")] = mStat->rxSuccess;
    root[F("rx_fail")]    = mStat->rxFail;
    root[F("frame_cnt")]  = mStat->frmCnt;
    root[F("tx_cnt")]     = mApp->mSys->Radio.mSendCnt;

    response->setLength();
    request->send(response);
}


//-----------------------------------------------------------------------------
void webApi::onInverterList(AsyncWebServerRequest *request) {
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();
    JsonArray invArr = root.createNestedArray("inverter");

    Inverter<> *iv;
    for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
        iv = mApp->mSys->getInverterByPos(i);
        if(NULL != iv) {
            JsonObject obj = invArr.createNestedObject();
            obj[F("id")]       = i;
            obj[F("name")]     = String(iv->name);
            obj[F("serial")]   = String(iv->serial.u64, HEX);
            obj[F("channels")] = iv->channels;
            obj[F("version")]  = String(iv->fwVersion);

            for(uint8_t j = 0; j < iv->channels; j ++) {
                obj[F("ch_max_power")][j] = iv->chMaxPwr[j];
                obj[F("ch_name")][j] = iv->chName[j];
            }

            obj[F("power_limit")]        = iv->powerLimit[0];
            obj[F("power_limit_option")] = iv->powerLimit[1];
        }
    }
    root[F("interval")]          = String(mConfig->sendInterval);
    root[F("retries")]           = String(mConfig->maxRetransPerPyld);
    root[F("max_num_inverters")] = MAX_NUM_INVERTERS;

    response->setLength();
    request->send(response);
}


//-----------------------------------------------------------------------------
void webApi::onMqtt(AsyncWebServerRequest *request) {
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    root[F("broker")] = String(mConfig->mqtt.broker);
    root[F("port")]   = String(mConfig->mqtt.port);
    root[F("user")]   = String(mConfig->mqtt.user);
    root[F("pwd")]    = (strlen(mConfig->mqtt.pwd) > 0) ? F("{PWD}") : String("");
    root[F("topic")]  = String(mConfig->mqtt.topic);

    response->setLength();
    request->send(response);
}


//-----------------------------------------------------------------------------
void webApi::onNtp(AsyncWebServerRequest *request) {
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    root[F("addr")] = String(mConfig->ntpAddr);
    root[F("port")] = String(mConfig->ntpPort);

    response->setLength();
    request->send(response);
}


//-----------------------------------------------------------------------------
void webApi::onPinout(AsyncWebServerRequest *request) {
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    root[F("cs")]  = mConfig->pinCs;
    root[F("ce")]  = mConfig->pinCe;
    root[F("irq")] = mConfig->pinIrq;

    response->setLength();
    request->send(response);
}


//-----------------------------------------------------------------------------
void webApi::onRadio(AsyncWebServerRequest *request) {
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    root[F("power_level")] = mConfig->amplifierPower;

    response->setLength();
    request->send(response);
}



//-----------------------------------------------------------------------------
void webApi::onSerial(AsyncWebServerRequest *request) {
    AsyncJsonResponse* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot();

    root[F("interval")] = (uint16_t)mConfig->serialInterval;
    root[F("show_live_data")] = mConfig->serialShowIv;
    root[F("debug")] = mConfig->serialDebug;

    response->setLength();
    request->send(response);
}
