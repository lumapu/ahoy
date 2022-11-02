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

    mTimezoneOffset = 0;
}

//-----------------------------------------------------------------------------
void webApi::setup(void) {
    mSrv->on("/api", HTTP_GET,  std::bind(&webApi::onApi,         this, std::placeholders::_1));
    mSrv->on("/api", HTTP_POST, std::bind(&webApi::onApiPost,     this, std::placeholders::_1)).onBody(
                                std::bind(&webApi::onApiPostBody, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));

    mSrv->on("/get_setup", HTTP_GET,  std::bind(&webApi::onDwnldSetup,   this, std::placeholders::_1));
}

//-----------------------------------------------------------------------------
void webApi::loop(void) {
}


//-----------------------------------------------------------------------------
void webApi::onApi(AsyncWebServerRequest *request) {
    AsyncJsonResponse* response = new AsyncJsonResponse(false, 8192);
    JsonObject root = response->getRoot();

    Inverter<> *iv = mApp->mSys->getInverterByPos(0, false);
    String path = request->url().substring(5);
    if(path == "system")              getSystem(root);
    else if(path == "logout")         getLogout(root);
    else if(path == "save")           getSave(root);
    else if(path == "reboot")         getReboot(root);
    else if(path == "statistics")     getStatistics(root);
    else if(path == "inverter/list")  getInverterList(root);
    else if(path == "menu")           getMenu(root);
    else if(path == "index")          getIndex(root);
    else if(path == "setup")          getSetup(root);
    else if(path == "setup/networks") getNetworks(root);
    else if(path == "live")           getLive(root);
    else if(path == "record/info")    getRecord(root, iv->getRecordStruct(InverterDevInform_All));
    else if(path == "record/alarm")   getRecord(root, iv->getRecordStruct(AlarmData));
    else if(path == "record/config")  getRecord(root, iv->getRecordStruct(SystemConfigPara));
    else if(path == "record/live")    getRecord(root, iv->getRecordStruct(RealTimeRunData_Debug));
    else
        getNotFound(root, F("http://") + request->host() + F("/api/"));

    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Headers", "content-type");
    response->setLength();
    request->send(response);
}


//-----------------------------------------------------------------------------
void webApi::onApiPost(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, "onApiPost");
}


//-----------------------------------------------------------------------------
void webApi::onApiPostBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    DPRINTLN(DBG_VERBOSE, "onApiPostBody");
    DynamicJsonDocument json(200);
    AsyncJsonResponse* response = new AsyncJsonResponse(false, 200);
    JsonObject root = response->getRoot();

    DeserializationError err = deserializeJson(json, (const char *)data, len);
    JsonObject obj = json.as<JsonObject>();
    root[F("success")] = (err) ? false : true;
    if(!err) {
        String path = request->url().substring(5);
        if(path == "ctrl")
            root[F("success")] = setCtrl(obj, root);
        else if(path == "setup")
            root[F("success")] = setSetup(obj, root);
        else {
            root[F("success")] = false;
            root[F("error")]   = "Path not found: " + path;
        }
    }
    else {
        switch (err.code()) {
            case DeserializationError::Ok: break;
            case DeserializationError::InvalidInput: root[F("error")] = F("Invalid input");          break;
            case DeserializationError::NoMemory:     root[F("error")] = F("Not enough memory");      break;
            default:                                 root[F("error")] = F("Deserialization failed"); break;
        }
    }

    response->setLength();
    request->send(response);
}


//-----------------------------------------------------------------------------
void webApi::getNotFound(JsonObject obj, String url) {
    JsonObject ep = obj.createNestedObject("avail_endpoints");
    ep[F("system")]        = url + F("system");
    ep[F("statistics")]    = url + F("statistics");
    ep[F("inverter/list")] = url + F("inverter/list");
    ep[F("index")]         = url + F("index");
    ep[F("setup")]         = url + F("setup");
    ep[F("live")]          = url + F("live");
    ep[F("record/info")]   = url + F("record/info");
    ep[F("record/alarm")]  = url + F("record/alarm");
    ep[F("record/config")] = url + F("record/config");
    ep[F("record/live")]   = url + F("record/live");
}


//-----------------------------------------------------------------------------
void webApi::onDwnldSetup(AsyncWebServerRequest *request) {
    AsyncJsonResponse* response = new AsyncJsonResponse(false, 8192);
    JsonObject root = response->getRoot();

    getSetup(root);

    response->setLength();
    response->addHeader("Content-Type", "application/octet-stream");
    response->addHeader("Content-Description", "File Transfer");
    response->addHeader("Content-Disposition", "attachment; filename=ahoy_setup.json");
    request->send(response);
}


//-----------------------------------------------------------------------------
void webApi::getSysInfo(JsonObject obj) {
    obj[F("ssid")]        = mSysCfg->stationSsid;
    obj[F("device_name")] = mSysCfg->deviceName;
    obj[F("version")]     = String(mVersion);
    obj[F("build")]       = String(AUTO_GIT_HASH);
    obj[F("ts_uptime")]   = mApp->getUptime();
    obj[F("ts_now")]      = mApp->getTimestamp();
    obj[F("ts_sunrise")]  = mApp->getSunrise();
    obj[F("ts_sunset")]   = mApp->getSunset();
    obj[F("ts_sun_upd")]  = mApp->getLatestSunTimestamp();
    obj[F("wifi_rssi")]   = WiFi.RSSI();
    obj[F("disclaimer")]  = mConfig->disclaimer;
#if defined(ESP32)
    obj[F("esp_type")]    = F("ESP32");
#else
    obj[F("esp_type")]    = F("ESP8266");
#endif
}


//-----------------------------------------------------------------------------
void webApi::getSystem(JsonObject obj) {
    getMenu(obj.createNestedObject(F("menu")));
    getSysInfo(obj.createNestedObject(F("system")));
    obj[F("html")] = F("<a href=\"/factory\" class=\"btn\">Factory Reset</a><br/><br/><a href=\"/reboot\" class=\"btn\">Reboot</a>");
}


//-----------------------------------------------------------------------------
void webApi::getLogout(JsonObject obj) {
    getMenu(obj.createNestedObject(F("menu")));
    getSysInfo(obj.createNestedObject(F("system")));
    obj[F("refresh")] = 3;
    obj[F("refresh_url")] = "/";
    obj[F("html")] = F("succesfully logged out");
}


//-----------------------------------------------------------------------------
void webApi::getSave(JsonObject obj) {
    getMenu(obj.createNestedObject(F("menu")));
    getSysInfo(obj.createNestedObject(F("system")));
    obj[F("refresh")] = 2;
    obj[F("refresh_url")] = "/setup";
    obj[F("html")] = F("settings succesfully save");
}


//-----------------------------------------------------------------------------
void webApi::getReboot(JsonObject obj) {
    getMenu(obj.createNestedObject(F("menu")));
    getSysInfo(obj.createNestedObject(F("system")));
    obj[F("refresh")] = 10;
    obj[F("refresh_url")] = "/";
    obj[F("html")] = F("reboot. Autoreload after 10 seconds");
}


//-----------------------------------------------------------------------------
void webApi::getStatistics(JsonObject obj) {
    obj[F("rx_success")]     = mStat->rxSuccess;
    obj[F("rx_fail")]        = mStat->rxFail;
    obj[F("rx_fail_answer")] = mStat->rxFailNoAnser;
    obj[F("frame_cnt")]      = mStat->frmCnt;
    obj[F("tx_cnt")]         = mApp->mSys->Radio.mSendCnt;
}


//-----------------------------------------------------------------------------
void webApi::getInverterList(JsonObject obj) {
    JsonArray invArr = obj.createNestedArray(F("inverter"));

    Inverter<> *iv;
    for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
        iv = mApp->mSys->getInverterByPos(i);
        if(NULL != iv) {
            JsonObject obj2 = invArr.createNestedObject();
            obj2[F("id")]       = i;
            obj2[F("name")]     = String(iv->name);
            obj2[F("serial")]   = String(iv->serial.u64, HEX);
            obj2[F("channels")] = iv->channels;
            obj2[F("version")]  = String(iv->fwVersion);

            for(uint8_t j = 0; j < iv->channels; j ++) {
                obj2[F("ch_max_power")][j] = iv->chMaxPwr[j];
                obj2[F("ch_name")][j] = iv->chName[j];
            }
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
void webApi::getSun(JsonObject obj) {
    obj[F("lat")] = mConfig->sunLat ? String(mConfig->sunLat, 5) : "";
    obj[F("lon")] = mConfig->sunLat ? String(mConfig->sunLon, 5) : "";
    obj[F("disnightcom")] = mConfig->sunDisNightCom;
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
void webApi::getMenu(JsonObject obj) {
    obj["name"][0] = "Live";
    obj["link"][0] = "/live";
    obj["name"][1] = "Serial Console";
    obj["link"][1] = "/serial";
    obj["name"][2] = "Settings";
    obj["link"][2] = "/setup";
    obj["name"][3] = "-";
    obj["name"][4] = "REST API";
    obj["link"][4] = "/api";
    obj["trgt"][4] = "_blank";
    obj["name"][5] = "-";
    obj["name"][6] = "Update";
    obj["link"][6] = "/update";
    obj["name"][7] = "System";
    obj["link"][7] = "/system";
    if(strlen(mConfig->password) > 0) {
        obj["name"][8] = "-";
        obj["name"][9] = "Logout";
        obj["link"][9] = "/logout";
    }
}


//-----------------------------------------------------------------------------
void webApi::getIndex(JsonObject obj) {
    getMenu(obj.createNestedObject(F("menu")));
    getSysInfo(obj.createNestedObject(F("system")));
    getStatistics(obj.createNestedObject(F("statistics")));
    obj["refresh_interval"] = SEND_INTERVAL;

    JsonArray inv = obj.createNestedArray(F("inverter"));
    Inverter<> *iv;
    for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
        iv = mApp->mSys->getInverterByPos(i);
        if(NULL != iv) {
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
            JsonObject invObj = inv.createNestedObject();
            invObj[F("id")]              = i;
            invObj[F("name")]            = String(iv->name);
            invObj[F("version")]         = String(iv->fwVersion);
            invObj[F("is_avail")]        = iv->isAvailable(mApp->getTimestamp(), rec);
            invObj[F("is_producing")]    = iv->isProducing(mApp->getTimestamp(), rec);
            invObj[F("ts_last_success")] = iv->getLastTs(rec);
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
        info.add(F("MQTT is connected, ") + String(mApp->getMqttTxCnt()) + F(" packets sent"));
}


//-----------------------------------------------------------------------------
void webApi::getSetup(JsonObject obj) {
    getMenu(obj.createNestedObject(F("menu")));
    getSysInfo(obj.createNestedObject(F("system")));
    getInverterList(obj.createNestedObject(F("inverter")));
    getMqtt(obj.createNestedObject(F("mqtt")));
    getNtp(obj.createNestedObject(F("ntp")));
    getSun(obj.createNestedObject(F("sun")));
    getPinout(obj.createNestedObject(F("pinout")));
    getRadio(obj.createNestedObject(F("radio")));
    getSerial(obj.createNestedObject(F("serial")));
}


//-----------------------------------------------------------------------------
void webApi::getNetworks(JsonObject obj) {
    mApp->getAvailNetworks(obj);
}


//-----------------------------------------------------------------------------
void webApi::getLive(JsonObject obj) {
    getMenu(obj.createNestedObject(F("menu")));
    getSysInfo(obj.createNestedObject(F("system")));
    JsonArray invArr = obj.createNestedArray(F("inverter"));
    obj["refresh_interval"] = SEND_INTERVAL;

    uint8_t list[] = {FLD_UAC, FLD_IAC, FLD_PAC, FLD_F, FLD_PF, FLD_T, FLD_YT, FLD_YD, FLD_PDC, FLD_EFF, FLD_Q};

    Inverter<> *iv;
    uint8_t pos;
    for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
        iv = mApp->mSys->getInverterByPos(i);
        if(NULL != iv) {
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
            JsonObject obj2 = invArr.createNestedObject();
            obj2[F("name")]               = String(iv->name);
            obj2[F("channels")]           = iv->channels;
            obj2[F("power_limit_read")]   = round3(iv->actPowerLimit);
            obj2[F("last_alarm")]         = String(iv->lastAlarmMsg);
            obj2[F("ts_last_success")]    = rec->ts;

            JsonArray ch = obj2.createNestedArray("ch");
            JsonArray ch0 = ch.createNestedArray();
            obj2[F("ch_names")][0] = "AC";
            for (uint8_t fld = 0; fld < sizeof(list); fld++) {
                pos = (iv->getPosByChFld(CH0, list[fld], rec));
                ch0[fld] = (0xff != pos) ? round3(iv->getValue(pos, rec)) : 0.0;
                obj[F("ch0_fld_units")][fld] = (0xff != pos) ? String(iv->getUnit(pos, rec)) : notAvail;
                obj[F("ch0_fld_names")][fld] = (0xff != pos) ? String(iv->getFieldName(pos, rec)) : notAvail;
            }

            for(uint8_t j = 1; j <= iv->channels; j ++) {
                obj2[F("ch_names")][j] = String(iv->chName[j-1]);
                JsonArray cur = ch.createNestedArray();
                for (uint8_t k = 0; k < 6; k++) {
                    switch(k) {
                        default: pos = (iv->getPosByChFld(j, FLD_UDC, rec)); break;
                        case 1:  pos = (iv->getPosByChFld(j, FLD_IDC, rec)); break;
                        case 2:  pos = (iv->getPosByChFld(j, FLD_PDC, rec)); break;
                        case 3:  pos = (iv->getPosByChFld(j, FLD_YD, rec));  break;
                        case 4:  pos = (iv->getPosByChFld(j, FLD_YT, rec));  break;
                        case 5:  pos = (iv->getPosByChFld(j, FLD_IRR, rec)); break;
                    }
                    cur[k] = (0xff != pos) ? round3(iv->getValue(pos, rec)) : 0.0;
                    if(1 == j) {
                        obj[F("fld_units")][k] = (0xff != pos) ? String(iv->getUnit(pos, rec)) : notAvail;
                        obj[F("fld_names")][k] = (0xff != pos) ? String(iv->getFieldName(pos, rec)) : notAvail;
                    }
                }
            }
        }
    }
}


//-----------------------------------------------------------------------------
void webApi::getRecord(JsonObject obj, record_t<> *rec) {
    JsonArray invArr = obj.createNestedArray(F("inverter"));

    Inverter<> *iv;
    uint8_t pos;
    for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
        iv = mApp->mSys->getInverterByPos(i);
        if(NULL != iv) {
            JsonArray obj2 = invArr.createNestedArray();
            for(uint8_t j = 0; j < rec->length; j++) {
                byteAssign_t *assign = iv->getByteAssign(j, rec);
                pos = (iv->getPosByChFld(assign->ch, assign->fieldId, rec));
                obj2[j]["fld"]  = (0xff != pos) ? String(iv->getFieldName(pos, rec)) : notAvail;
                obj2[j]["unit"] = (0xff != pos) ? String(iv->getUnit(pos, rec)) : notAvail;
                obj2[j]["val"]  = (0xff != pos) ? String(iv->getValue(pos, rec)) : notAvail;
            }
        }
    }
}


//-----------------------------------------------------------------------------
bool webApi::setCtrl(JsonObject jsonIn, JsonObject jsonOut) {
    uint8_t cmd = jsonIn[F("cmd")];

    // Todo: num is the inverter number 0-3. For better display in DPRINTLN
    uint8_t num = jsonIn[F("inverter")];
    uint8_t tx_request = jsonIn[F("tx_request")];

    if(TX_REQ_DEVCONTROL == tx_request)
    {
        DPRINTLN(DBG_INFO, F("devcontrol [") + String(num) + F("], cmd: 0x") + String(cmd, HEX));

        Inverter<> *iv = getInverter(jsonIn, jsonOut);
        JsonArray payload = jsonIn[F("payload")].as<JsonArray>();

        if(NULL != iv)
        {
            switch (cmd)
            {
                case TurnOn:
                    iv->devControlCmd = TurnOn;
                    iv->devControlRequest = true;
                    break;
                case TurnOff:
                    iv->devControlCmd = TurnOff;
                    iv->devControlRequest = true;
                    break;
                case CleanState_LockAndAlarm:
                    iv->devControlCmd = CleanState_LockAndAlarm;
                    iv->devControlRequest = true;
                    break;
                case Restart:
                    iv->devControlCmd = Restart;
                    iv->devControlRequest = true;
                    break;
                case ActivePowerContr:
                    iv->devControlCmd = ActivePowerContr;
                    iv->devControlRequest = true;
                    iv->powerLimit[0] = payload[0];
                    iv->powerLimit[1] = payload[1];
                    break;
                default:
                    jsonOut["error"] = "unknown 'cmd' = " + String(cmd);
                    return false;
            }
        } else {
            return false;
        }
    }
    else {
        jsonOut[F("error")] = F("unknown 'tx_request'");
        return false;
    }

    return true;
}


//-----------------------------------------------------------------------------
bool webApi::setSetup(JsonObject jsonIn, JsonObject jsonOut) {
    if(F("scan_wifi") == jsonIn[F("cmd")])
        mApp->scanAvailNetworks();
    else if(F("set_time") == jsonIn[F("cmd")])
        mApp->setTimestamp(jsonIn[F("ts")]);
    else if(F("sync_ntp") == jsonIn[F("cmd")])
        mApp->setTimestamp(0); // 0: update ntp flag
    else if(F("serial_utc_offset") == jsonIn[F("cmd")])
        mTimezoneOffset = jsonIn[F("ts")];
    else if(F("discovery_cfg") == jsonIn[F("cmd")])
        mApp->mFlagSendDiscoveryConfig = true; // for homeassistant
    else {
        jsonOut[F("error")] = F("unknown cmd");
        return false;
    }

    return true;
}


//-----------------------------------------------------------------------------
Inverter<> *webApi::getInverter(JsonObject jsonIn, JsonObject jsonOut) {
    uint8_t id = jsonIn[F("inverter")];
    Inverter<> *iv = mApp->mSys->getInverterByPos(id);
    if(NULL == iv)
        jsonOut[F("error")] = F("inverter index to high: ") + String(id);
    return iv;
}
