//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#if defined(ESP32) && defined(F)
  #undef F
  #define F(sl) (sl)
#endif

#include "webApi.h"

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
    mSrv->on("/api", HTTP_GET, std::bind(&webApi::onApi, this, std::placeholders::_1));
}


//-----------------------------------------------------------------------------
void webApi::loop(void) {
}


//-----------------------------------------------------------------------------
void webApi::onApi(AsyncWebServerRequest *request) {
    AsyncJsonResponse* response = new AsyncJsonResponse(false, 2048);
    JsonObject root = response->getRoot();

    String path = request->url().substring(5);
    if(path == "system")             getSystem(root);
    else if(path == "statistics")    getStatistics(root);
    else if(path == "inverter/list") getInverterList(root);
    else if(path == "mqtt")          getMqtt(root);
    else if(path == "ntp")           getNtp(root);
    else if(path == "pinout")        getPinout(root);
    else if(path == "radio")         getRadio(root);
    else if(path == "serial")        getSerial(root);
    else if(path == "index")         getIndex(root);
    else if(path == "setup")         getSetup(root);
    else
        getNotFound(root, F("http://") + request->host() + F("/api/"));

    response->setLength();
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Headers", "content-type");
    request->send(response);
}


//-----------------------------------------------------------------------------
void webApi::getSystem(JsonObject obj) {
    obj[F("ssid")]        = mSysCfg->stationSsid;
    obj[F("device_name")] = mSysCfg->deviceName;
    obj[F("version")]     = String(mVersion);
    obj[F("build")]       = String(AUTO_GIT_HASH);
    obj[F("ts_uptime")]   = mApp->getUptime();
    obj[F("ts_now")]      = mApp->getTimestamp();
}


//-----------------------------------------------------------------------------
void webApi::getStatistics(JsonObject obj) {
    obj[F("rx_success")] = mStat->rxSuccess;
    obj[F("rx_fail")]    = mStat->rxFail;
    obj[F("frame_cnt")]  = mStat->frmCnt;
    obj[F("tx_cnt")]     = mApp->mSys->Radio.mSendCnt;
}


//-----------------------------------------------------------------------------
void webApi::getInverterList(JsonObject obj) {
    JsonArray invArr = obj.createNestedArray(F("inverter"));

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
    obj[F("interval")]          = String(mConfig->sendInterval);
    obj[F("retries")]           = String(mConfig->maxRetransPerPyld);
    obj[F("max_num_inverters")] = MAX_NUM_INVERTERS;
}


//-----------------------------------------------------------------------------
void webApi::getMqtt(JsonObject obj) {
    obj[F("broker")] = String(mConfig->mqtt.broker);
    obj[F("port")]   = String(mConfig->mqtt.port);
    obj[F("user")]   = String(mConfig->mqtt.user);
    obj[F("pwd")]    = (strlen(mConfig->mqtt.pwd) > 0) ? F("{PWD}") : String("");
    obj[F("topic")]  = String(mConfig->mqtt.topic);
}


//-----------------------------------------------------------------------------
void webApi::getNtp(JsonObject obj) {
    obj[F("addr")] = String(mConfig->ntpAddr);
    obj[F("port")] = String(mConfig->ntpPort);
}


//-----------------------------------------------------------------------------
void webApi::getPinout(JsonObject obj) {
    obj[F("cs")]  = mConfig->pinCs;
    obj[F("ce")]  = mConfig->pinCe;
    obj[F("irq")] = mConfig->pinIrq;
}


//-----------------------------------------------------------------------------
void webApi::getRadio(JsonObject obj) {
    obj[F("power_level")] = mConfig->amplifierPower;
}


//-----------------------------------------------------------------------------
void webApi::getSerial(JsonObject obj) {
    obj[F("interval")]       = (uint16_t)mConfig->serialInterval;
    obj[F("show_live_data")] = mConfig->serialShowIv;
    obj[F("debug")]          = mConfig->serialDebug;
}


//-----------------------------------------------------------------------------
void webApi::getNotFound(JsonObject obj, String url) {
    JsonObject ep = obj.createNestedObject("avail_endpoints");
    ep[F("system")]        = url + F("system");
    ep[F("statistics")]    = url + F("statistics");
    ep[F("inverter/list")] = url + F("inverter/list");
    ep[F("mqtt")]          = url + F("mqtt");
    ep[F("ntp")]           = url + F("ntp");
    ep[F("pinout")]        = url + F("pinout");
    ep[F("radio")]         = url + F("radio");
    ep[F("serial")]        = url + F("serial");
    ep[F("index")]         = url + F("index");
    ep[F("setup")]         = url + F("setup");
}


//-----------------------------------------------------------------------------
void webApi::getIndex(JsonObject obj) {
    getSystem(obj.createNestedObject(F("system")));
    getStatistics(obj.createNestedObject(F("statistics")));
    obj["refresh_interval"] = SEND_INTERVAL;

    JsonArray inv = obj.createNestedArray(F("inverter"));
    Inverter<> *iv;
    for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
        iv = mApp->mSys->getInverterByPos(i);
        if(NULL != iv) {
            JsonObject invObj = inv.createNestedObject();
            invObj[F("id")]              = i;
            invObj[F("name")]            = String(iv->name);
            invObj[F("version")]         = String(iv->fwVersion);
            invObj[F("is_avail")]        = iv->isAvailable(mApp->getTimestamp());
            invObj[F("is_producing")]    = iv->isProducing(mApp->getTimestamp());
            invObj[F("ts_last_success")] = iv->getLastTs();
        }
    }

    JsonArray warn = obj.createNestedArray(F("warnings"));
    if(!mApp->mSys->Radio.isChipConnected())
        warn.add(F("your NRF24 module can't be reached, check the wiring and pinout"));
    if(!mApp->mqttIsConnected())
        warn.add(F("MQTT is not connected"));

    JsonArray info = obj.createNestedArray(F("infos"));
    if(mApp->getRebootRequestState())
        info.add(F("reboot your ESP to apply all your configuration changes!"));
    if(!mApp->getSettingsValid())
        info.add(F("your settings are invalid"));
    if(mApp->mqttIsConnected())
        info.add(F("MQTT is connected"));
}


//-----------------------------------------------------------------------------
void webApi::getSetup(JsonObject obj) {
    getSystem(obj.createNestedObject(F("system")));
    getInverterList(obj.createNestedObject(F("inverter")));
    getMqtt(obj.createNestedObject(F("mqtt")));
    getNtp(obj.createNestedObject(F("ntp")));
    getPinout(obj.createNestedObject(F("pinout")));
    getRadio(obj.createNestedObject(F("radio")));
    getSerial(obj.createNestedObject(F("serial")));
}
