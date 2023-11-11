//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __WEB_API_H__
#define __WEB_API_H__

#include "../utils/dbg.h"
#ifdef ESP32
#include "AsyncTCP.h"
#else
#include "ESPAsyncTCP.h"
#endif
#include "../appInterface.h"
#include "../hm/hmSystem.h"
#include "../utils/helper.h"
#include "AsyncJson.h"
#if defined(ETHERNET)
#include "AsyncWebServer_ESP32_W5500.h"
#else
#include "ESPAsyncWebServer.h"
#endif

#if defined(F) && defined(ESP32)
#undef F
#define F(sl) (sl)
#endif

const uint8_t acList[] = {FLD_UAC, FLD_IAC, FLD_PAC, FLD_F, FLD_PF, FLD_T, FLD_YT, FLD_YD, FLD_PDC, FLD_EFF, FLD_Q, FLD_MP};
const uint8_t acListHmt[] = {FLD_UAC_1N, FLD_IAC_1, FLD_PAC, FLD_F, FLD_PF, FLD_T, FLD_YT, FLD_YD, FLD_PDC, FLD_EFF, FLD_Q, FLD_MP};
const uint8_t dcList[] = {FLD_UDC, FLD_IDC, FLD_PDC, FLD_YD, FLD_YT, FLD_IRR, FLD_MP};

template<class HMSYSTEM>
class RestApi {
    public:
        RestApi() {
            mTimezoneOffset = 0;
            mHeapFree       = 0;
            mHeapFreeBlk    = 0;
            mHeapFrag       = 0;
        }

        void setup(IApp *app, HMSYSTEM *sys, AsyncWebServer *srv, settings_t *config) {
            mApp      = app;
            mSrv      = srv;
            mSys      = sys;
            mRadioNrf = (HmRadio<>*)mApp->getRadioObj(true);
            #if defined(ESP32)
            mRadioCmt = (CmtRadio<esp32_3wSpi>*)mApp->getRadioObj(false);
            #endif
            mConfig   = config;
            mSrv->on("/api", HTTP_GET,  std::bind(&RestApi::onApi,         this, std::placeholders::_1));
            mSrv->on("/api", HTTP_POST, std::bind(&RestApi::onApiPost,     this, std::placeholders::_1)).onBody(
                                        std::bind(&RestApi::onApiPostBody, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));

            mSrv->on("/get_setup", HTTP_GET,  std::bind(&RestApi::onDwnldSetup, this, std::placeholders::_1));
        }

        uint32_t getTimezoneOffset(void) {
            return mTimezoneOffset;
        }

        void ctrlRequest(JsonObject obj) {
            DynamicJsonDocument json(128);
            JsonObject dummy = json.as<JsonObject>();
            if(obj[F("path")] == "ctrl")
                setCtrl(obj, dummy);
            else if(obj[F("path")] == "setup")
                setSetup(obj, dummy);
        }

    private:
        void onApi(AsyncWebServerRequest *request) {
            mHeapFree = ESP.getFreeHeap();
            #ifndef ESP32
            mHeapFreeBlk = ESP.getMaxFreeBlockSize();
            mHeapFrag = ESP.getHeapFragmentation();
            #endif

            AsyncJsonResponse* response = new AsyncJsonResponse(false, 6000);
            JsonObject root = response->getRoot();

            String path = request->url().substring(5);
            if(path == "html/system")         getHtmlSystem(request, root);
            else if(path == "html/logout")    getHtmlLogout(request, root);
            else if(path == "html/reboot")    getHtmlReboot(request, root);
            else if(path == "html/save")      getHtmlSave(request, root);
            else if(path == "system")         getSysInfo(request, root);
            else if(path == "generic")        getGeneric(request, root);
            else if(path == "reboot")         getReboot(request, root);
            else if(path == "inverter/list")  getInverterList(root);
            else if(path == "index")          getIndex(request, root);
            else if(path == "setup")          getSetup(request, root);
            #if !defined(ETHERNET)
            else if(path == "setup/networks") getNetworks(root);
            #endif /* !defined(ETHERNET) */
            else if(path == "live")           getLive(request,root);
            else {
                if(path.substring(0, 12) == "inverter/id/")
                    getInverter(root, request->url().substring(17).toInt());
                else if(path.substring(0, 15) == "inverter/alarm/")
                    getIvAlarms(root, request->url().substring(20).toInt());
                else if(path.substring(0, 17) == "inverter/version/")
                    getIvVersion(root, request->url().substring(22).toInt());
                else if(path.substring(0, 19) == "inverter/radiostat/")
                    getIvStatistis(root, request->url().substring(24).toInt());
                else if(path.substring(0, 16) == "inverter/pwrack/")
                    getIvPowerLimitAck(root, request->url().substring(21).toInt());
                else
                    getNotFound(root, F("http://") + request->host() + F("/api/"));
            }

            //DPRINTLN(DBG_INFO, "API mem usage: " + String(root.memoryUsage()));
            response->addHeader("Access-Control-Allow-Origin", "*");
            response->addHeader("Access-Control-Allow-Headers", "content-type");
            response->setLength();
            request->send(response);
        }

        void onApiPost(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, "onApiPost");
        }

        void onApiPostBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            DPRINTLN(DBG_VERBOSE, "onApiPostBody");
            DynamicJsonDocument json(800);
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

        void getNotFound(JsonObject obj, String url) {
            JsonObject ep = obj.createNestedObject("avail_endpoints");
            ep[F("inverter/list")]    = url + F("inverter/list");
            ep[F("inverter/id/0")]    = url + F("inverter/id/0");
            ep[F("inverter/alarm/0")] = url + F("inverter/alarm/0");
            ep[F("inverter/version/0")] = url + F("inverter/version/0");
            ep[F("generic")]          = url + F("generic");
            ep[F("index")]            = url + F("index");
            ep[F("setup")]            = url + F("setup");
            ep[F("system")]           = url + F("system");
            ep[F("live")]             = url + F("live");
        }


        void onDwnldSetup(AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response;

            File fp = LittleFS.open("/settings.json", "r");
            if(!fp) {
                DPRINTLN(DBG_ERROR, F("failed to load settings"));
                response = request->beginResponse(200, F("application/json; charset=utf-8"), "{}");
            }
            else {
                String tmp = fp.readString();
                int i = 0;
                // remove all passwords
                while (i != -1) {
                    i = tmp.indexOf("\"pwd\":", i);
                    if(-1 != i) {
                        i+=7;
                        tmp.remove(i, tmp.indexOf("\"", i)-i);
                    }
                }
                response = request->beginResponse(200, F("application/json; charset=utf-8"), tmp);
            }

            String filename = ah::getDateTimeStrFile(gTimezone.toLocal(mApp->getTimestamp()));
            filename += "_v" + String(mApp->getVersion());

            response->addHeader("Content-Type", "application/octet-stream");
            response->addHeader("Content-Description", "File Transfer");
            response->addHeader("Content-Disposition", "attachment; filename=" + filename + "_ahoy_setup.json");
            request->send(response);
            fp.close();
        }

        void getGeneric(AsyncWebServerRequest *request, JsonObject obj) {
            obj[F("wifi_rssi")]   = (WiFi.status() != WL_CONNECTED) ? 0 : WiFi.RSSI();
            obj[F("ts_uptime")]   = mApp->getUptime();
            obj[F("ts_now")]      = mApp->getTimestamp();
            obj[F("version")]     = String(mApp->getVersion());
            obj[F("build")]       = String(AUTO_GIT_HASH);
            obj[F("menu_prot")]   = mApp->getProtection(request);
            obj[F("menu_mask")]   = (uint16_t)(mConfig->sys.protectionMask );
            obj[F("menu_protEn")] = (bool) (strlen(mConfig->sys.adminPwd) > 0);

        #if defined(ESP32)
            obj[F("esp_type")]    = F("ESP32");
        #else
            obj[F("esp_type")]    = F("ESP8266");
        #endif
        }

        void getSysInfo(AsyncWebServerRequest *request, JsonObject obj) {
            #if !defined(ETHERNET)
            obj[F("ssid")]         = mConfig->sys.stationSsid;
            obj[F("ap_pwd")]       = mConfig->sys.apPwd;
            obj[F("hidd")]         = mConfig->sys.isHidden;
            #endif /* !defined(ETHERNET) */
            obj[F("device_name")]  = mConfig->sys.deviceName;
            obj[F("dark_mode")]    = (bool)mConfig->sys.darkMode;
            obj[F("sched_reboot")] = (bool)mConfig->sys.schedReboot;

            obj[F("mac")]          = WiFi.macAddress();
            obj[F("hostname")]     = mConfig->sys.deviceName;
            obj[F("pwd_set")]      = (strlen(mConfig->sys.adminPwd) > 0);
            obj[F("prot_mask")]    = mConfig->sys.protectionMask;

            obj[F("sdk")]          = ESP.getSdkVersion();
            obj[F("cpu_freq")]     = ESP.getCpuFreqMHz();
            obj[F("heap_free")]    = mHeapFree;
            obj[F("sketch_total")] = ESP.getFreeSketchSpace();
            obj[F("sketch_used")]  = ESP.getSketchSize() / 1024; // in kb
            getGeneric(request, obj);

            getRadioNrf(obj.createNestedObject(F("radioNrf")));
            #if defined(ESP32)
            getRadioCmtInfo(obj.createNestedObject(F("radioCmt")));
            #endif
            getMqttInfo(obj.createNestedObject(F("mqtt")));

        #if defined(ESP32)
            obj[F("chip_revision")] = ESP.getChipRevision();
            obj[F("chip_model")]    = ESP.getChipModel();
            obj[F("chip_cores")]    = ESP.getChipCores();
            obj[F("heap_total")]    = ESP.getHeapSize();
            //obj[F("core_version")]  = F("n/a");
            //obj[F("flash_size")]    = F("n/a");
            //obj[F("heap_frag")]     = F("n/a");
            //obj[F("max_free_blk")]  = F("n/a");
            //obj[F("reboot_reason")] = F("n/a");
        #else
            //obj[F("heap_total")]    = F("n/a");
            //obj[F("chip_revision")] = F("n/a");
            //obj[F("chip_model")]    = F("n/a");
            //obj[F("chip_cores")]    = F("n/a");
            obj[F("heap_frag")]     = mHeapFrag;
            obj[F("max_free_blk")]  = mHeapFreeBlk;
            obj[F("core_version")]  = ESP.getCoreVersion();
            obj[F("flash_size")]    = ESP.getFlashChipRealSize() / 1024; // in kb
            obj[F("reboot_reason")] = ESP.getResetReason();
        #endif
            //obj[F("littlefs_total")] = LittleFS.totalBytes();
            //obj[F("littlefs_used")] = LittleFS.usedBytes();

            uint8_t max;
            mApp->getSchedulerInfo(&max);
            obj[F("schMax")] = max;
        }

        void getHtmlSystem(AsyncWebServerRequest *request, JsonObject obj) {
            getSysInfo(request, obj.createNestedObject(F("system")));
            getGeneric(request, obj.createNestedObject(F("generic")));
            obj[F("html")] = F("<a href=\"/factory\" class=\"btn\">AhoyFactory Reset</a><br/><br/><a href=\"/reboot\" class=\"btn\">Reboot</a>");
        }

        void getHtmlLogout(AsyncWebServerRequest *request, JsonObject obj) {
            getGeneric(request, obj.createNestedObject(F("generic")));
            obj[F("refresh")] = 3;
            obj[F("refresh_url")] = "/";
            obj[F("html")] = F("successfully logged out");
        }

        void getHtmlReboot(AsyncWebServerRequest *request, JsonObject obj) {
            getGeneric(request, obj.createNestedObject(F("generic")));
            obj[F("refresh")] = 20;
            obj[F("refresh_url")] = "/";
            obj[F("html")] = F("rebooting ...");
        }

        void getHtmlSave(AsyncWebServerRequest *request, JsonObject obj) {
            getGeneric(request, obj.createNestedObject(F("generic")));
            obj["pending"] = (bool)mApp->getSavePending();
            obj["success"] = (bool)mApp->getLastSaveSucceed();
            obj["reboot"] = (bool)mApp->getShouldReboot();
        }

        void getReboot(AsyncWebServerRequest *request, JsonObject obj) {
            getGeneric(request, obj.createNestedObject(F("generic")));
            obj[F("refresh")] = 10;
            obj[F("refresh_url")] = "/";
            obj[F("html")] = F("reboot. Autoreload after 10 seconds");
        }

        void getIvStatistis(JsonObject obj, uint8_t id) {
            Inverter<> *iv = mSys->getInverterByPos(id);
            if(NULL == iv) {
                obj[F("error")] = F("inverter not found!");
                return;
            }
            obj[F("name")]           = String(iv->config->name);
            obj[F("rx_success")]     = iv->radioStatistics.rxSuccess;
            obj[F("rx_fail")]        = iv->radioStatistics.rxFail;
            obj[F("rx_fail_answer")] = iv->radioStatistics.rxFailNoAnser;
            obj[F("frame_cnt")]      = iv->radioStatistics.frmCnt;
            obj[F("tx_cnt")]         = iv->radioStatistics.txCnt;
            obj[F("retransmits")]    = iv->radioStatistics.retransmits;
        }

        void getIvPowerLimitAck(JsonObject obj, uint8_t id) {
            Inverter<> *iv = mSys->getInverterByPos(id);
            if(NULL == iv) {
                obj[F("error")] = F("inverter not found!");
                return;
            }
            obj["ack"] = (bool)iv->powerLimitAck;
        }

        void getInverterList(JsonObject obj) {
            JsonArray invArr = obj.createNestedArray(F("inverter"));

            Inverter<> *iv;
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
                iv = mSys->getInverterByPos(i);
                if(NULL != iv) {
                    JsonObject obj2 = invArr.createNestedObject();
                    obj2[F("enabled")]  = (bool)iv->config->enabled;
                    obj2[F("id")]       = i;
                    obj2[F("name")]     = String(iv->config->name);
                    obj2[F("serial")]   = String(iv->config->serial.u64, HEX);
                    obj2[F("channels")] = iv->channels;
                    obj2[F("freq")]     = iv->config->frequency;
                    if(0xff == iv->config->powerLevel) {
                        if((IV_HMT == iv->ivGen) || (IV_HMS == iv->ivGen))
                            obj2[F("pa")] = 30; // 20dBm
                        else
                            obj2[F("pa")] = 1; // low
                    } else
                        obj2[F("pa")] = iv->config->powerLevel;

                    for(uint8_t j = 0; j < iv->channels; j ++) {
                        obj2[F("ch_yield_cor")][j] = (double)iv->config->yieldCor[j];
                        obj2[F("ch_name")][j]      = iv->config->chName[j];
                        obj2[F("ch_max_pwr")][j]   = iv->config->chMaxPwr[j];
                    }
                }
            }
            obj[F("interval")]          = String(mConfig->nrf.sendInterval);
            obj[F("max_num_inverters")] = MAX_NUM_INVERTERS;
            obj[F("rstMid")]            = (bool)mConfig->inst.rstYieldMidNight;
            obj[F("rstNotAvail")]       = (bool)mConfig->inst.rstValsNotAvail;
            obj[F("rstComStop")]        = (bool)mConfig->inst.rstValsCommStop;
            obj[F("strtWthtTm")]        = (bool)mConfig->inst.startWithoutTime;
            obj[F("rstMaxMid")]         = (bool)mConfig->inst.rstMaxValsMidNight;
            obj[F("yldEff")]            = mConfig->inst.yieldEffiency;
        }

        void getInverter(JsonObject obj, uint8_t id) {
            Inverter<> *iv = mSys->getInverterByPos(id);
            if(NULL == iv) {
                obj[F("error")] = F("inverter not found!");
                return;
            }

            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
            obj[F("id")]               = id;
            obj[F("enabled")]          = (bool)iv->config->enabled;
            obj[F("name")]             = String(iv->config->name);
            obj[F("serial")]           = String(iv->config->serial.u64, HEX);
            obj[F("version")]          = String(iv->getFwVersion());
            obj[F("power_limit_read")] = ah::round3(iv->actPowerLimit);
            obj[F("power_limit_ack")]  = iv->powerLimitAck;
            obj[F("max_pwr")]          = iv->getMaxPower();
            obj[F("ts_last_success")]  = rec->ts;
            obj[F("generation")]       = iv->ivGen;
            obj[F("status")]           = (uint8_t)iv->status;
            obj[F("alarm_cnt")]        = iv->alarmCnt;
            obj[F("rssi")]             = iv->rssi;

            JsonArray ch = obj.createNestedArray("ch");

            // AC
            uint8_t pos;
            obj[F("ch_name")][0] = "AC";
            JsonArray ch0 = ch.createNestedArray();
            if(IV_HMT == iv->ivGen) {
                for (uint8_t fld = 0; fld < sizeof(acListHmt); fld++) {
                    pos = (iv->getPosByChFld(CH0, acListHmt[fld], rec));
                    ch0[fld] = (0xff != pos) ? ah::round3(iv->getValue(pos, rec)) : 0.0;
                }
            } else  {
                for (uint8_t fld = 0; fld < sizeof(acList); fld++) {
                    pos = (iv->getPosByChFld(CH0, acList[fld], rec));
                    ch0[fld] = (0xff != pos) ? ah::round3(iv->getValue(pos, rec)) : 0.0;
                }
            }

            // DC
            for(uint8_t j = 0; j < iv->channels; j ++) {
                obj[F("ch_name")][j+1] = iv->config->chName[j];
                obj[F("ch_max_pwr")][j+1] = iv->config->chMaxPwr[j];
                JsonArray cur = ch.createNestedArray();
                for (uint8_t fld = 0; fld < sizeof(dcList); fld++) {
                    pos = (iv->getPosByChFld((j+1), dcList[fld], rec));
                    cur[fld] = (0xff != pos) ? ah::round3(iv->getValue(pos, rec)) : 0.0;
                }
            }
        }

        void getIvAlarms(JsonObject obj, uint8_t id) {
            Inverter<> *iv = mSys->getInverterByPos(id);
            if(NULL == iv) {
                obj[F("error")] = F("inverter not found!");
                return;
            }

            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);

            obj[F("iv_id")]   = id;
            obj[F("iv_name")] = String(iv->config->name);
            obj[F("cnt")]     = iv->alarmCnt;
            obj[F("last_id")] = iv->getChannelFieldValue(CH0, FLD_EVT, rec);

            JsonArray alarm = obj.createNestedArray(F("alarm"));
            for(uint8_t i = 0; i < 10; i++) {
                alarm[i][F("code")]  = iv->lastAlarm[i].code;
                alarm[i][F("str")]   = iv->getAlarmStr(iv->lastAlarm[i].code);
                alarm[i][F("start")] = iv->lastAlarm[i].start;
                alarm[i][F("end")]   = iv->lastAlarm[i].end;
            }
        }

        void getIvVersion(JsonObject obj, uint8_t id) {
            Inverter<> *iv = mSys->getInverterByPos(id);
            if(NULL == iv) {
                obj[F("error")] = F("inverter not found!");
                return;
            }

            record_t<> *rec = iv->getRecordStruct(InverterDevInform_Simple);

            obj[F("name")]       = String(iv->config->name);
            obj[F("serial")]     = String(iv->config->serial.u64, HEX);
            obj[F("generation")] = iv->ivGen;
            obj[F("max_pwr")]    = iv->getMaxPower();
            obj[F("part_num")]   = iv->getChannelFieldValueInt(CH0, FLD_PART_NUM, rec);
            obj[F("hw_ver")]     = iv->getChannelFieldValueInt(CH0, FLD_HW_VERSION, rec);
            obj[F("prod_cw")]    = ((iv->config->serial.b[3] & 0x0f) * 10 + (((iv->config->serial.b[2] >> 4) & 0x0f)));
            obj[F("prod_year")]  = ((iv->config->serial.b[3] >> 4) & 0x0f) + 2014;


            rec = iv->getRecordStruct(InverterDevInform_All);
            char buf[10];
            uint16_t val;

            val = iv->getChannelFieldValueInt(CH0, FLD_FW_BUILD_MONTH_DAY, rec);
            snprintf(buf, 10, "-%02d-%02d", (val / 100), (val % 100));
            obj[F("fw_date")]    = String(iv->getChannelFieldValueInt(CH0, FLD_FW_BUILD_YEAR, rec)) + String(buf);
            val = iv->getChannelFieldValueInt(CH0, FLD_FW_BUILD_HOUR_MINUTE, rec);
            snprintf(buf, 10, "%02d:%02d", (val / 100), (val % 100));
            obj[F("fw_time")]    = String(buf);
            val = iv->getChannelFieldValueInt(CH0, FLD_FW_VERSION, rec);
            snprintf(buf, 10, "%d.%02d.%02d", (val / 10000), ((val % 10000) / 100), (val % 100));
            obj[F("fw_ver")]     = String(buf);
            obj[F("boot_ver")]   = iv->getChannelFieldValueInt(CH0, FLD_BOOTLOADER_VER, rec);
        }

        void getMqtt(JsonObject obj) {
            obj[F("broker")]     = String(mConfig->mqtt.broker);
            obj[F("clientId")]   = String(mConfig->mqtt.clientId);
            obj[F("port")]       = String(mConfig->mqtt.port);
            obj[F("user")]       = String(mConfig->mqtt.user);
            obj[F("pwd")]        = (strlen(mConfig->mqtt.pwd) > 0) ? F("{PWD}") : String("");
            obj[F("topic")]      = String(mConfig->mqtt.topic);
            obj[F("interval")]   = String(mConfig->mqtt.interval);
        }

        void getNtp(JsonObject obj) {
            obj[F("addr")] = String(mConfig->ntp.addr);
            obj[F("port")] = String(mConfig->ntp.port);
            obj[F("interval")] = String(mConfig->ntp.interval);
        }

        void getSun(JsonObject obj) {
            obj[F("lat")] = mConfig->sun.lat ? String(mConfig->sun.lat, 5) : "";
            obj[F("lon")] = mConfig->sun.lat ? String(mConfig->sun.lon, 5) : "";
            obj[F("disnightcom")] = mConfig->sun.disNightCom;
            obj[F("offs")] = mConfig->sun.offsetSec;
        }

        void getPinout(JsonObject obj) {
            obj[F("cs")]  = mConfig->nrf.pinCs;
            obj[F("ce")]  = mConfig->nrf.pinCe;
            obj[F("irq")] = mConfig->nrf.pinIrq;
            obj[F("sclk")] = mConfig->nrf.pinSclk;
            obj[F("mosi")] = mConfig->nrf.pinMosi;
            obj[F("miso")] = mConfig->nrf.pinMiso;
            obj[F("led0")] = mConfig->led.led0;
            obj[F("led1")] = mConfig->led.led1;
            obj[F("led_high_active")] = mConfig->led.led_high_active;
        }

        #if defined(ESP32)
        void getRadioCmt(JsonObject obj) {
            obj[F("sclk")]  = mConfig->cmt.pinSclk;
            obj[F("sdio")]  = mConfig->cmt.pinSdio;
            obj[F("csb")]   = mConfig->cmt.pinCsb;
            obj[F("fcsb")]  = mConfig->cmt.pinFcsb;
            obj[F("gpio3")] = mConfig->cmt.pinIrq;
            obj[F("en")]    = (bool) mConfig->cmt.enabled;
        }

        void getRadioCmtInfo(JsonObject obj) {
            obj[F("en")] = (bool) mConfig->cmt.enabled;
            obj[F("isconnected")] = mRadioCmt->isConnected();
        }
        #endif

        void getRadioNrf(JsonObject obj) {
            obj[F("en")]          = (bool) mConfig->nrf.enabled;
            obj[F("isconnected")] = mRadioNrf->isChipConnected();
            obj[F("dataRate")]    = mRadioNrf->getDataRate();
            //obj[F("isPVariant")]  = mRadioNrf->isPVariant();
        }

        void getSerial(JsonObject obj) {
            obj[F("interval")]       = (uint16_t)mConfig->serial.interval;
            obj[F("show_live_data")] = mConfig->serial.showIv;
            obj[F("debug")]          = mConfig->serial.debug;
        }

        void getStaticIp(JsonObject obj) {
            char buf[16];
            ah::ip2Char(mConfig->sys.ip.ip, buf);      obj[F("ip")]      = String(buf);
            ah::ip2Char(mConfig->sys.ip.mask, buf);    obj[F("mask")]    = String(buf);
            ah::ip2Char(mConfig->sys.ip.dns1, buf);    obj[F("dns1")]    = String(buf);
            ah::ip2Char(mConfig->sys.ip.dns2, buf);    obj[F("dns2")]    = String(buf);
            ah::ip2Char(mConfig->sys.ip.gateway, buf); obj[F("gateway")] = String(buf);
        }

        void getDisplay(JsonObject obj) {
            obj[F("disp_typ")]     = (uint8_t)mConfig->plugin.display.type;
            obj[F("disp_pwr")]     = (bool)mConfig->plugin.display.pwrSaveAtIvOffline;
            obj[F("disp_pxshift")] = (bool)mConfig->plugin.display.pxShift;
            obj[F("disp_rot")]     = (uint8_t)mConfig->plugin.display.rot;
            obj[F("disp_cont")]    = (uint8_t)mConfig->plugin.display.contrast;
            obj[F("disp_clk")]     = (mConfig->plugin.display.type == 0) ? DEF_PIN_OFF : mConfig->plugin.display.disp_clk;
            obj[F("disp_data")]    = (mConfig->plugin.display.type == 0) ? DEF_PIN_OFF : mConfig->plugin.display.disp_data;
            obj[F("disp_cs")]      = (mConfig->plugin.display.type < 3)  ? DEF_PIN_OFF : mConfig->plugin.display.disp_cs;
            obj[F("disp_dc")]      = (mConfig->plugin.display.type < 3)  ? DEF_PIN_OFF : mConfig->plugin.display.disp_dc;
            obj[F("disp_rst")]     = (mConfig->plugin.display.type < 3)  ? DEF_PIN_OFF : mConfig->plugin.display.disp_reset;
            obj[F("disp_bsy")]     = (mConfig->plugin.display.type < 10) ? DEF_PIN_OFF : mConfig->plugin.display.disp_busy;
        }

        void getMqttInfo(JsonObject obj) {
            obj[F("enabled")]   = (mConfig->mqtt.broker[0] != '\0');
            obj[F("connected")] = mApp->getMqttIsConnected();
            obj[F("tx_cnt")]    = mApp->getMqttTxCnt();
            obj[F("rx_cnt")]    = mApp->getMqttRxCnt();
            obj[F("interval")]  = mConfig->mqtt.interval;
        }

        void getIndex(AsyncWebServerRequest *request, JsonObject obj) {
            getGeneric(request, obj.createNestedObject(F("generic")));
            obj[F("ts_now")]       = mApp->getTimestamp();
            obj[F("ts_sunrise")]   = mApp->getSunrise();
            obj[F("ts_sunset")]    = mApp->getSunset();
            obj[F("ts_offset")]    = mConfig->sun.offsetSec;
            obj[F("disNightComm")] = mConfig->sun.disNightCom;

            JsonArray inv = obj.createNestedArray(F("inverter"));
            Inverter<> *iv;
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
                iv = mSys->getInverterByPos(i);
                if(NULL != iv) {
                    record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
                    JsonObject invObj = inv.createNestedObject();
                    invObj[F("enabled")]         = (bool)iv->config->enabled;
                    invObj[F("id")]              = i;
                    invObj[F("name")]            = String(iv->config->name);
                    invObj[F("cur_pwr")]         = ah::round3(iv->getChannelFieldValue(CH0, FLD_PAC, rec));
                    invObj[F("is_avail")]        = iv->isAvailable();
                    invObj[F("is_producing")]    = iv->isProducing();
                    invObj[F("ts_last_success")] = iv->getLastTs(rec);
                }
            }

            JsonArray warn = obj.createNestedArray(F("warnings"));
            if(!mRadioNrf->isChipConnected() && mConfig->nrf.enabled)
                warn.add(F("your NRF24 module can't be reached, check the wiring, pinout and enable"));
            else if(!mRadioNrf->isPVariant() && mConfig->nrf.enabled)
                warn.add(F("your NRF24 module isn't a plus version(+), maybe incompatible"));
            if(!mApp->getSettingsValid())
                warn.add(F("your settings are invalid"));
            if(mApp->getRebootRequestState())
                warn.add(F("reboot your ESP to apply all your configuration changes"));
            if(0 == mApp->getTimestamp())
                warn.add(F("time not set. No communication to inverter possible"));
        }

        void getSetup(AsyncWebServerRequest *request, JsonObject obj) {
            getGeneric(request, obj.createNestedObject(F("generic")));
            getSysInfo(request, obj.createNestedObject(F("system")));
            //getInverterList(obj.createNestedObject(F("inverter")));
            getMqtt(obj.createNestedObject(F("mqtt")));
            getNtp(obj.createNestedObject(F("ntp")));
            getSun(obj.createNestedObject(F("sun")));
            getPinout(obj.createNestedObject(F("pinout")));
            #if defined(ESP32)
            getRadioCmt(obj.createNestedObject(F("radioCmt")));
            #endif
            getRadioNrf(obj.createNestedObject(F("radioNrf")));
            getSerial(obj.createNestedObject(F("serial")));
            getStaticIp(obj.createNestedObject(F("static_ip")));
            getDisplay(obj.createNestedObject(F("display")));
        }

        #if !defined(ETHERNET)
        void getNetworks(JsonObject obj) {
            mApp->getAvailNetworks(obj);
        }
        #endif /* !defined(ETHERNET) */

        void getLive(AsyncWebServerRequest *request, JsonObject obj) {
            getGeneric(request, obj.createNestedObject(F("generic")));
            obj[F("refresh")] = mConfig->nrf.sendInterval;

            for (uint8_t fld = 0; fld < sizeof(acList); fld++) {
                obj[F("ch0_fld_units")][fld] = String(units[fieldUnits[acList[fld]]]);
                obj[F("ch0_fld_names")][fld] = String(fields[acList[fld]]);
            }
            for (uint8_t fld = 0; fld < sizeof(dcList); fld++) {
                obj[F("fld_units")][fld] = String(units[fieldUnits[dcList[fld]]]);
                obj[F("fld_names")][fld] = String(fields[dcList[fld]]);
            }

            Inverter<> *iv;
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
                iv = mSys->getInverterByPos(i);
                bool parse = false;
                if(NULL != iv)
                    parse = iv->config->enabled;
                obj[F("iv")][i] = parse;
            }
        }

        bool setCtrl(JsonObject jsonIn, JsonObject jsonOut) {
            Inverter<> *iv = mSys->getInverterByPos(jsonIn[F("id")]);
            bool accepted = true;
            if(NULL == iv) {
                jsonOut[F("error")] = F("inverter index invalid: ") + jsonIn[F("id")].as<String>();
                return false;
            }
            jsonOut[F("id")] = jsonIn[F("id")];

            if(F("power") == jsonIn[F("cmd")])
                accepted = iv->setDevControlRequest((jsonIn[F("val")] == 1) ? TurnOn : TurnOff);
            else if(F("restart") == jsonIn[F("cmd")])
                accepted = iv->setDevControlRequest(Restart);
            else if(0 == strncmp("limit_", jsonIn[F("cmd")].as<const char*>(), 6)) {
                iv->powerLimit[0] = jsonIn["val"];
                if(F("limit_persistent_relative") == jsonIn[F("cmd")])
                    iv->powerLimit[1] = RelativPersistent;
                else if(F("limit_persistent_absolute") == jsonIn[F("cmd")])
                    iv->powerLimit[1] = AbsolutPersistent;
                else if(F("limit_nonpersistent_relative") == jsonIn[F("cmd")])
                    iv->powerLimit[1] = RelativNonPersistent;
                else if(F("limit_nonpersistent_absolute") == jsonIn[F("cmd")])
                    iv->powerLimit[1] = AbsolutNonPersistent;

                accepted = iv->setDevControlRequest(ActivePowerContr);
            } else if(F("dev") == jsonIn[F("cmd")]) {
                DPRINTLN(DBG_INFO, F("dev cmd"));
                iv->setDevCommand(jsonIn[F("val")].as<int>());
            } else {
                jsonOut[F("error")] = F("unknown cmd: '") + jsonIn["cmd"].as<String>() + "'";
                return false;
            }

            if(!accepted) {
                jsonOut[F("error")] = F("inverter does not accept dev control request at this moment");
                return false;
            }

            return true;
        }

        bool setSetup(JsonObject jsonIn, JsonObject jsonOut) {
            #if !defined(ETHERNET)
            if(F("scan_wifi") == jsonIn[F("cmd")])
                mApp->scanAvailNetworks();
            else
            #endif /* !defined(ETHERNET) */
            if(F("set_time") == jsonIn[F("cmd")])
                mApp->setTimestamp(jsonIn[F("val")]);
            else if(F("sync_ntp") == jsonIn[F("cmd")])
                mApp->setTimestamp(0); // 0: update ntp flag
            else if(F("serial_utc_offset") == jsonIn[F("cmd")])
                mTimezoneOffset = jsonIn[F("val")];
            else if(F("discovery_cfg") == jsonIn[F("cmd")])
                mApp->setMqttDiscoveryFlag(); // for homeassistant
            else if(F("save_iv") == jsonIn[F("cmd")]) {
                Inverter<> *iv = mSys->getInverterByPos(jsonIn[F("id")], false);
                iv->config->enabled = jsonIn[F("en")];
                iv->config->serial.u64 = jsonIn[F("ser")];
                snprintf(iv->config->name, MAX_NAME_LENGTH, "%s", jsonIn[F("name")].as<const char*>());

                for(uint8_t i = 0; i < 6; i++) {
                    iv->config->chMaxPwr[i] = jsonIn[F("ch")][i][F("pwr")];
                    iv->config->yieldCor[i] = jsonIn[F("ch")][i][F("yld")];
                    snprintf(iv->config->chName[i], MAX_NAME_LENGTH, "%s", jsonIn[F("ch")][i][F("name")].as<const char*>());
                }

                switch(iv->config->serial.b[4]) {
                    case 0x24:
                    case 0x22:
                    case 0x21: iv->type = INV_TYPE_1CH; iv->channels = 1; break;

                    case 0x44:
                    case 0x42:
                    case 0x41: iv->type = INV_TYPE_2CH; iv->channels = 2; break;

                    case 0x64:
                    case 0x62:
                    case 0x61: iv->type = INV_TYPE_4CH; iv->channels = 4; break;
                    default:  break;
                }
                iv->config->frequency = jsonIn[F("freq")];
                iv->config->powerLevel = jsonIn[F("pa")];
                mApp->saveSettings(false); // without reboot
            } else {
                jsonOut[F("error")] = F("unknown cmd");
                return false;
            }

            return true;
        }

        IApp *mApp;
        HMSYSTEM *mSys;
        HmRadio<> *mRadioNrf;
        #if defined(ESP32)
        CmtRadio<esp32_3wSpi> *mRadioCmt;
        #endif
        AsyncWebServer *mSrv;
        settings_t *mConfig;

        uint32_t mTimezoneOffset;
        uint32_t mHeapFree, mHeapFreeBlk;
        uint8_t mHeapFrag;
};

#endif /*__WEB_API_H__*/
