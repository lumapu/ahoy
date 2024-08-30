//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
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
#include "lang.h"
#include "AsyncJson.h"
#include "ESPAsyncWebServer.h"

#include "plugins/history.h"

#if defined(F) && defined(ESP32)
#undef F
#define F(sl) (sl)
#endif

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
            mRadioNrf = (NrfRadio<>*)mApp->getRadioObj(true);
            #if defined(ESP32)
            mRadioCmt = (CmtRadio<>*)mApp->getRadioObj(false);
            #endif
            mConfig   = config;
            #if defined(ENABLE_HISTORY_LOAD_DATA)
            mSrv->on("/api/addYDHist",
                             HTTP_POST, std::bind(&RestApi::onApiPost, this, std::placeholders::_1),
                                        std::bind(&RestApi::onApiPostYDHist,this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
            #endif
            mSrv->on("/api", HTTP_POST, std::bind(&RestApi::onApiPost,     this, std::placeholders::_1)).onBody(
                                        std::bind(&RestApi::onApiPostBody, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));
            mSrv->on("/api", HTTP_GET,  std::bind(&RestApi::onApi,         this, std::placeholders::_1));

            mSrv->on("/get_setup", HTTP_GET,  std::bind(&RestApi::onDwnldSetup, this, std::placeholders::_1));
            #if defined(ESP32)
            mSrv->on("/coredump", HTTP_GET,  std::bind(&RestApi::getCoreDump, this, std::placeholders::_1));
            #endif
        }

        uint32_t getTimezoneOffset(void) {
            return mTimezoneOffset;
        }

        void ctrlRequest(JsonObject obj) {
            DynamicJsonDocument json(128);
            JsonObject dummy = json.as<JsonObject>();
            if(obj[F("path")] == "ctrl")
                setCtrl(obj, dummy, "*");
            else if(obj[F("path")] == "setup")
                setSetup(obj, dummy, "*");
        }

    private:
        void onApi(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, String("onApi: ") + String((uint16_t)request->method())); // 1 == Get, 3 == POST

            mHeapFree = ESP.getFreeHeap();
            #ifndef ESP32
            mHeapFreeBlk = ESP.getMaxFreeBlockSize();
            mHeapFrag = ESP.getHeapFragmentation();
            #else
            mHeapFreeBlk = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
            if(mHeapFree > 0)
                mHeapFrag = 100 - ((mHeapFreeBlk * 100) / mHeapFree);
            else
                mHeapFrag = 0;
            #endif

            #if defined(ESP32)
            AsyncJsonResponse* response = new AsyncJsonResponse(false, 8000);
            #else
            AsyncJsonResponse* response = new AsyncJsonResponse(false, 6000);
            #endif
            JsonObject root = response->getRoot();

            String path = request->url().substring(5);
            if(path == "html/system")         getHtmlSystem(request, root);
            else if(path == "html/logout")    getHtmlLogout(request, root);
            else if(path == "html/reboot")    getHtmlReboot(request, root);
            else if(path == "html/save")      getHtmlSave(request, root);
            else if(path == "html/erase")     getHtmlErase(request, root);
            else if(path == "html/erasetrue") getHtmlEraseTrue(request, root);
            else if(path == "html/factory")     getHtmlFactory(request, root);
            else if(path == "html/factorytrue") getHtmlFactoryTrue(request, root);
            else if(path == "system")         getSysInfo(request, root);
            else if(path == "generic")        getGeneric(request, root);
            else if(path == "reboot")         getReboot(request, root);
            else if(path == "inverter/list")  getInverterList(root);
            else if(path == "index")          getIndex(request, root);
            else if(path == "setup")          getSetup(request, root);
            else if(path == "setup/networks") getNetworks(root);
            else if(path == "setup/getip")    getIp(root);
            else if(path == "live")           getLive(request,root);
            #if defined(ENABLE_HISTORY)
            else if (path == "powerHistory")  getPowerHistory(request, root, HistoryStorageType::POWER);
            else if (path == "powerHistoryDay")  getPowerHistory(request, root, HistoryStorageType::POWER_DAY);
            #endif /*ENABLE_HISTORY*/
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
                else if(path.substring(0, 14) == "inverter/grid/")
                    getGridProfile(root, request->url().substring(19).toInt());
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
            #if defined(ETHERNET)
            // workaround for AsyncWebServer_ESP32_W5500, because it can't distinguish
            // between HTTP_GET and HTTP_POST if both are registered
            if(request->method() == HTTP_GET)
                onApi(request);
            #endif
        }

        #if defined(ENABLE_HISTORY_LOAD_DATA)
        // VArt67: For debugging history graph. Loading data into graph
        void onApiPostYDHist(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, size_t final) {
            uint32_t total = request->contentLength();
            DPRINTLN(DBG_DEBUG, "[onApiPostYieldDHistory ] " + filename + " index:" + index + " len:" + len + " total:" + total + " final:" + final);

            if (0 == index) {
                if (NULL != mTmpBuf)
                    delete[] mTmpBuf;
                mTmpBuf = new uint8_t[total + 1];
                mTmpSize = total;
            }
            if (mTmpSize >= (len + index))
                memcpy(&mTmpBuf[index], data, len);

            if (!final)
                return;  // not last frame - nothing to do

            mTmpSize = len + index;  // correct the total size
            mTmpBuf[mTmpSize] = 0;

            #ifndef ESP32
            DynamicJsonDocument json(ESP.getMaxFreeBlockSize() - 512);  // need some memory on heap
            #else
            DynamicJsonDocument json(12000);  // does this work? I have no ESP32 :-(
            #endif
            DeserializationError err = deserializeJson(json, static_cast<const char *>(mTmpBuf, mTmpSize));
            json.shrinkToFit();
            JsonObject obj = json.as<JsonObject>();

            // Debugging
            // mTmpBuf[mTmpSize] = 0;
            // DPRINTLN(DBG_DEBUG, (const char *)mTmpBuf);

            if (!err && obj) {
                // insert data into yieldDayHistory object
                HistoryStorageType dataType;
                if (obj["maxDay"] > 0)  // this is power history data
                {
                    dataType = HistoryStorageType::POWER;
                    if (obj["refresh"] > 60)
                        dataType = HistoryStorageType::POWER_DAY;

                }
                else
                    dataType = HistoryStorageType::YIELD;

                size_t cnt = obj[F("value")].size();
                DPRINTLN(DBG_DEBUG, "ArraySize: " + String(cnt));

                for (uint16_t i = 0; i < cnt; i++) {
                    uint16_t val = obj[F("value")][i];
                    mApp->addValueToHistory((uint8_t)dataType, 0, val);
                    // DPRINT(DBG_VERBOSE, "value " + String(i) + ": " + String(val) + ", ");
                }
                uint32_t refresh = obj[F("refresh")];
                mApp->addValueToHistory((uint8_t)dataType, 1, refresh);
                if (dataType != HistoryStorageType::YIELD) {
                    uint32_t ts = obj[F("lastValueTs")];
                    mApp->addValueToHistory((uint8_t)dataType, 2, ts);
                }

            } else {
                switch (err.code()) {
                    case DeserializationError::Ok:
                        break;
                    case DeserializationError::IncompleteInput:
                        DPRINTLN(DBG_DEBUG, F("Incomplete input"));
                        break;
                    case DeserializationError::InvalidInput:
                        DPRINTLN(DBG_DEBUG, F("Invalid input"));
                        break;
                    case DeserializationError::NoMemory:
                        DPRINTLN(DBG_DEBUG, F("Not enough memory ") + String(json.capacity()) + " bytes");
                        break;
                    default:
                        DPRINTLN(DBG_DEBUG, F("Deserialization failed"));
                        break;
                }
            }

            request->send(204);  // Success with no page load
            delete[] mTmpBuf;
            mTmpBuf = NULL;
        }
        #endif

        void onApiPostBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            DPRINTLN(DBG_VERBOSE, "onApiPostBody");

            if(0 == index) {
                if(nullptr != mTmpBuf)
                    delete[] mTmpBuf;
                mTmpBuf = new uint8_t[total+1];
                mTmpSize = total;
            }
            if(mTmpSize >= (len + index))
                memcpy(&mTmpBuf[index], data, len);

            if((len + index) != total)
                return; // not last frame - nothing to do

            DynamicJsonDocument json(1000);

            AsyncJsonResponse* response = new AsyncJsonResponse(false, 200);
            JsonObject root = response->getRoot();
            DeserializationError err = deserializeJson(json, reinterpret_cast<const char*>(mTmpBuf), mTmpSize);
            if(!json.is<JsonObject>())
                root[F("error")] = F(DESER_FAILED);
            else {
                JsonObject obj = json.as<JsonObject>();

                root[F("success")] = (err) ? false : true;
                if(!err) {
                    String path = request->url().substring(5);
                    if(path == "ctrl")
                        root[F("success")] = setCtrl(obj, root, request->client()->remoteIP().toString().c_str());
                    else if(path == "setup")
                        root[F("success")] = setSetup(obj, root, request->client()->remoteIP().toString().c_str());
                    else {
                        root[F("success")] = false;
                        root[F("error")]   = F(PATH_NOT_FOUND) + path;
                    }
                } else {
                    switch (err.code()) {
                        case DeserializationError::Ok: break;
                        case DeserializationError::IncompleteInput: root[F("error")] = F(INCOMPLETE_INPUT); break;
                        case DeserializationError::InvalidInput:    root[F("error")] = F(INVALID_INPUT);    break;
                        case DeserializationError::NoMemory:        root[F("error")] = F(NOT_ENOUGH_MEM);   break;
                        default:                                    root[F("error")] = F(DESER_FAILED);     break;
                    }
                }
            }

            response->setLength();
            request->send(response);
            delete[] mTmpBuf;
            mTmpBuf = nullptr;
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
            ep[F("setup/networks")]   = url + F("setup/networks");
            ep[F("setup/getip")]      = url + F("setup/getip");
            ep[F("system")]           = url + F("system");
            ep[F("live")]             = url + F("live");
            #if defined(ENABLE_HISTORY)
            ep[F("powerHistory")]     = url + F("powerHistory");
            ep[F("powerHistoryDay")]  = url + F("powerHistoryDay");
            #endif
        }


        void onDwnldSetup(AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response;

            // save settings to have latest firmware changes in export
            mApp->saveSettings(false);

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
                i = 0;
                // convert all serial numbers to hexadecimal
                while (i != -1) {
                    i = tmp.indexOf("\"sn\":", i);
                    if(-1 != i) {
                        i+=5;
                        String sn = tmp.substring(i, tmp.indexOf("\"", i)-1);
                        tmp.replace(sn, String(sn) + ",\"note\":\"" + String(atoll(sn.c_str()),  HEX) + "\"");
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

        #if defined(ESP32)
        void getCoreDump(AsyncWebServerRequest *request) {
            const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, "coredump");
            if (partition != NULL) {
                size_t size = partition->size;

                AsyncWebServerResponse *response = request->beginResponse("application/octet-stream", size, [size, partition](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
                    if((index + maxLen) > size)
                        maxLen = size - index;

                    if (ESP_OK != esp_partition_read(partition, index, buffer, maxLen))
                        DPRINTLN(DBG_ERROR, F("can't read partition"));

                    return maxLen;
                });

                String filename = ah::getDateTimeStrFile(gTimezone.toLocal(mApp->getTimestamp()));
                filename += "_v" + String(mApp->getVersion());
                filename += "_" + String(ENV_NAME);

                response->addHeader("Content-Description", "File Transfer");
                response->addHeader("Content-Disposition", "attachment; filename=" + filename + "_coredump.bin");
                request->send(response);
            } else {
                AsyncWebServerResponse *response = request->beginResponse(200, F("application/json; charset=utf-8"), "{}");
                request->send(response);
            }
        }
        #endif

        void getGeneric(AsyncWebServerRequest *request, JsonObject obj) {
            mApp->resetLockTimeout();
            obj[F("wifi_rssi")]   = (WiFi.status() != WL_CONNECTED) ? 0 : WiFi.RSSI();
            obj[F("ts_uptime")]   = mApp->getUptime();
            obj[F("ts_now")]      = mApp->getTimestamp();
            obj[F("version")]     = String(mApp->getVersion());
            obj[F("modules")]     = String(mApp->getVersionModules());
            obj[F("build")]       = String(AUTO_GIT_HASH);
            obj[F("env")]         = String(ENV_NAME);
            obj[F("host")]        = mConfig->sys.deviceName;
            obj[F("menu_prot")]   = mApp->isProtected(request->client()->remoteIP().toString().c_str(), "", true);
            obj[F("menu_mask")]   = (uint16_t)(mConfig->sys.protectionMask );
            obj[F("menu_protEn")] = (bool) (mConfig->sys.adminPwd[0] != '\0');
            obj[F("cst_lnk")]     = String(mConfig->plugin.customLink);
            obj[F("cst_lnk_txt")] = String(mConfig->plugin.customLinkText);
            obj[F("region")]      = mConfig->sys.region;
            obj[F("timezone")]    = mConfig->sys.timezone;

        #if defined(ESP32)
            obj[F("esp_type")]    = F("ESP32");
        #else
            obj[F("esp_type")]    = F("ESP8266");
        #endif
        }

        void getSysInfo(AsyncWebServerRequest *request, JsonObject obj) {
            obj[F("device_name")]  = mConfig->sys.deviceName;
            obj[F("dark_mode")]    = (bool)mConfig->sys.darkMode;
            obj[F("sched_reboot")] = (bool)mConfig->sys.schedReboot;

            obj[F("pwd_set")]      = (strlen(mConfig->sys.adminPwd) > 0);
            obj[F("prot_mask")]    = mConfig->sys.protectionMask;

            getGeneric(request, obj.createNestedObject(F("generic")));
            getChipInfo(obj.createNestedObject(F("chip")));
            getRadioNrf(obj.createNestedObject(F("radioNrf")));
            getMqttInfo(obj.createNestedObject(F("mqtt")));
            getNetworkInfo(obj.createNestedObject(F("network")));
            getMemoryInfo(obj.createNestedObject(F("memory")));
            #if defined(ESP32)
            getRadioCmtInfo(obj.createNestedObject(F("radioCmt")));
            #endif

            /*uint8_t max;
            mApp->getSchedulerInfo(&max);
            obj[F("schMax")] = max;*/
        }

        void getHtmlSystem(AsyncWebServerRequest *request, JsonObject obj) {
            getSysInfo(request, obj.createNestedObject(F("system")));
            getGeneric(request, obj.createNestedObject(F("generic")));
            #if defined(ESP32)
            char tmp[300];
            snprintf(tmp, 300, "<a href=\"/factory\" class=\"btn\">%s</a><br/><br/><a href=\"/reboot\" class=\"btn\">%s</a><br/><br/><a href=\"/coredump\" class=\"btn\">%s</a>", FACTORY_RESET, BTN_REBOOT, BTN_COREDUMP);
            #else
            char tmp[200];
            snprintf(tmp, 200, "<a href=\"/factory\" class=\"btn\">%s</a><br/><br/><a href=\"/reboot\" class=\"btn\">%s</a>", FACTORY_RESET, BTN_REBOOT);
            #endif
            obj[F("html")] = String(tmp);
        }

        void getHtmlLogout(AsyncWebServerRequest *request, JsonObject obj) {
            getGeneric(request, obj.createNestedObject(F("generic")));
            obj[F("refresh")] = 3;
            obj[F("refresh_url")] = "/";
            obj[F("html")] = F("successfully logged out");
        }

        void getHtmlReboot(AsyncWebServerRequest *request, JsonObject obj) {
            getGeneric(request, obj.createNestedObject(F("generic")));
            #if defined(ETHERNET)
            obj[F("refresh")] = (mConfig->sys.eth.enabled) ? 5 : 20;
            #else
            obj[F("refresh")] = 20;
            #endif
            obj[F("refresh_url")] = "/";
            obj[F("html")] = F("rebooting ...");
        }

        void getHtmlSave(AsyncWebServerRequest *request, JsonObject obj) {
            getGeneric(request, obj.createNestedObject(F("generic")));
            obj[F("pending")] = (bool)mApp->getSavePending();
            obj[F("success")] = (bool)mApp->getLastSaveSucceed();
            obj[F("reboot")] = (bool)mApp->getShouldReboot();
            #if defined(ETHERNET)
            obj[F("reload")] = (mConfig->sys.eth.enabled) ? 5 : 20;
            #else
            obj[F("reload")] = 20;
            #endif
        }

        void getHtmlErase(AsyncWebServerRequest *request, JsonObject obj) {
            getGeneric(request, obj.createNestedObject(F("generic")));
            obj[F("html")] = F("Erase settings (not WiFi)? <a class=\"btn\" href=\"/erasetrue\">yes</a> <a class=\"btn\" href=\"/\">no</a>");
        }

        void getHtmlEraseTrue(AsyncWebServerRequest *request, JsonObject obj) {
            getGeneric(request, obj.createNestedObject(F("generic")));
            mApp->eraseSettings(false);
            mApp->setRebootFlag();
            obj[F("html")] = F("Erase settings: success");
            #if defined(ETHERNET) && defined(CONFIG_IDF_TARGET_ESP32S3)
            obj[F("reload")] = (mConfig->sys.eth.enabled) ? 5 : 20;
            #else
            obj[F("reload")] = 20;
            #endif
        }

        void getHtmlFactory(AsyncWebServerRequest *request, JsonObject obj) {
            getGeneric(request, obj.createNestedObject(F("generic")));
            char tmp[200];
            snprintf(tmp, 200, "%s <a class=\"btn\" href=\"/factorytrue\">%s</a> <a class=\"btn\" href=\"/\">%s</a>", FACTORY_RESET, BTN_YES, BTN_NO);
            obj[F("html")] = tmp;
        }

        void getHtmlFactoryTrue(AsyncWebServerRequest *request, JsonObject obj) {
            getGeneric(request, obj.createNestedObject(F("generic")));
            mApp->eraseSettings(true);
            mApp->setRebootFlag();
            obj[F("html")] = F("Factory reset: success");
            #if defined(ETHERNET)
            obj[F("reload")] = (mConfig->sys.eth.enabled) ? 5 : 20;
            #else
            obj[F("reload")] = 20;
            #endif
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
                obj[F("error")] = F(INV_NOT_FOUND);
                return;
            }
            obj[F("name")]           = String(iv->config->name);
            obj[F("rx_success")]     = iv->radioStatistics.rxSuccess;
            obj[F("rx_fail")]        = iv->radioStatistics.rxFail;
            obj[F("rx_fail_answer")] = iv->radioStatistics.rxFailNoAnswer;
            obj[F("frame_cnt")]      = iv->radioStatistics.frmCnt;
            obj[F("tx_cnt")]         = iv->radioStatistics.txCnt;
            obj[F("retransmits")]    = iv->radioStatistics.retransmits;
            obj[F("ivLoss")]         = iv->radioStatistics.ivLoss;
            obj[F("ivSent")]         = iv->radioStatistics.ivSent;
            obj[F("dtuLoss")]        = iv->radioStatistics.dtuLoss;
            obj[F("dtuSent")]        = iv->radioStatistics.dtuSent;
        }

        void getIvPowerLimitAck(JsonObject obj, uint8_t id) {
            Inverter<> *iv = mSys->getInverterByPos(id);
            if(NULL == iv) {
                obj[F("error")] = F(INV_NOT_FOUND);
                return;
            }
            obj["ack"] = (bool)iv->powerLimitAck;
        }

        void getInverterList(JsonObject obj) {
            JsonArray invArr = obj.createNestedArray(F("inverter"));

            Inverter<> *iv;
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
                iv = mSys->getInverterByPos(i);
                if(NULL == iv)
                    continue;

                JsonObject obj2 = invArr.createNestedObject();
                obj2[F("enabled")]  = (bool)iv->config->enabled;
                obj2[F("id")]       = i;
                obj2[F("name")]     = String(iv->config->name);
                obj2[F("serial")]   = String(iv->config->serial.u64, HEX);
                obj2[F("channels")] = iv->channels;
                obj2[F("freq")]     = iv->config->frequency;
                obj2[F("disnightcom")] = (bool)iv->config->disNightCom;
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
            obj[F("interval")]          = String(mConfig->inst.sendInterval);
            obj[F("max_num_inverters")] = MAX_NUM_INVERTERS;
            obj[F("rstMid")]            = (bool)mConfig->inst.rstValsAtMidNight;
            obj[F("rstNotAvail")]       = (bool)mConfig->inst.rstValsNotAvail;
            obj[F("rstComStop")]        = (bool)mConfig->inst.rstValsCommStop;
            obj[F("rstComStart")]       = (bool)mConfig->inst.rstValsCommStart;
            obj[F("strtWthtTm")]        = (bool)mConfig->inst.startWithoutTime;
            obj[F("rdGrid")]            = (bool)mConfig->inst.readGrid;
            obj[F("rstMaxMid")]         = (bool)mConfig->inst.rstIncludeMaxVals;
        }

        void getInverter(JsonObject obj, uint8_t id) {
            Inverter<> *iv = mSys->getInverterByPos(id);
            if(NULL == iv) {
                obj[F("error")] = F(INV_NOT_FOUND);
                return;
            }

            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
            obj[F("id")]               = id;
            obj[F("enabled")]          = (bool)iv->config->enabled;
            obj[F("name")]             = String(iv->config->name);
            obj[F("serial")]           = String(iv->config->serial.u64, HEX);
            obj[F("version")]          = String(iv->getFwVersion());
            obj[F("power_limit_read")] = ah::round1(iv->getChannelFieldValue(CH0, FLD_ACT_ACTIVE_PWR_LIMIT, iv->getRecordStruct(SystemConfigPara)));
            obj[F("power_limit_ack")]  = iv->powerLimitAck;
            obj[F("max_pwr")]          = iv->getMaxPower();
            obj[F("ts_last_success")]  = rec->ts;
            obj[F("generation")]       = iv->ivGen;
            obj[F("status")]           = (uint8_t)iv->getStatus();
            obj[F("alarm_cnt")]        = iv->alarmCnt;
            obj[F("rssi")]             = iv->rssi;
            obj[F("ts_max_ac_pwr")]    = iv->tsMaxAcPower;
            obj[F("ts_max_temp")]      = iv->tsMaxTemperature;

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

        void getGridProfile(JsonObject obj, uint8_t id) {
            Inverter<> *iv = mSys->getInverterByPos(id);
            if(NULL == iv) {
                return;
            }

            obj[F("name")] = String(iv->config->name);
            obj[F("grid")] = iv->getGridProfile();
        }

        void getIvAlarms(JsonObject obj, uint8_t id) {
            Inverter<> *iv = mSys->getInverterByPos(id);
            if(NULL == iv) {
                obj[F("error")] = F(INV_NOT_FOUND);
                return;
            }

            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);

            obj[F("iv_id")]   = id;
            obj[F("iv_name")] = String(iv->config->name);
            obj[F("cnt")]     = iv->alarmCnt;
            obj[F("last_id")] = iv->getChannelFieldValue(CH0, FLD_EVT, rec);

            JsonArray alarm = obj.createNestedArray(F("alarm"));

            // find oldest alarm
            uint8_t offset = 0;
            uint32_t oldestStart = 0xffffffff;
            for(uint8_t i = 0; i < Inverter<>::MaxAlarmNum; i++) {
                if((iv->lastAlarm[i].start != 0) && (iv->lastAlarm[i].start < oldestStart)) {
                    offset = i;
                    oldestStart = iv->lastAlarm[i].start;
                }
            }

            for(uint8_t i = 0; i < Inverter<>::MaxAlarmNum; i++) {
                uint8_t pos = (i + offset) % Inverter<>::MaxAlarmNum;
                alarm[pos][F("code")]  = iv->lastAlarm[pos].code;
                alarm[pos][F("str")]   = iv->getAlarmStr(iv->lastAlarm[pos].code);
                alarm[pos][F("start")] = iv->lastAlarm[pos].start;
                alarm[pos][F("end")]   = iv->lastAlarm[pos].end;
            }
        }

        void getIvVersion(JsonObject obj, uint8_t id) {
            Inverter<> *iv = mSys->getInverterByPos(id);
            if(NULL == iv) {
                obj[F("error")] = F(INV_NOT_FOUND);
                return;
            }

            record_t<> *rec = iv->getRecordStruct(InverterDevInform_Simple);

            obj[F("id")]         = id;
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
            obj[F("json")]       = (bool) mConfig->mqtt.json;
            obj[F("interval")]   = String(mConfig->mqtt.interval);
            obj[F("retain")]     = (bool)mConfig->mqtt.enableRetain;
        }

        void getNtp(JsonObject obj) {
            obj[F("addr")] = String(mConfig->ntp.addr);
            obj[F("port")] = String(mConfig->ntp.port);
            obj[F("interval")] = String(mConfig->ntp.interval);
        }

        void getSun(JsonObject obj) {
            obj[F("lat")] = mConfig->sun.lat ? String(mConfig->sun.lat, 5) : "";
            obj[F("lon")] = mConfig->sun.lat ? String(mConfig->sun.lon, 5) : "";
            obj[F("offsSr")] = mConfig->sun.offsetSecMorning;
            obj[F("offsSs")] = mConfig->sun.offsetSecEvening;
        }

        void getPinout(JsonObject obj) {
            obj[F("cs")]  = mConfig->nrf.pinCs;
            obj[F("ce")]  = mConfig->nrf.pinCe;
            obj[F("irq")] = mConfig->nrf.pinIrq;
            obj[F("sclk")] = mConfig->nrf.pinSclk;
            obj[F("mosi")] = mConfig->nrf.pinMosi;
            obj[F("miso")] = mConfig->nrf.pinMiso;
            obj[F("led0")] = mConfig->led.led[0];
            obj[F("led1")] = mConfig->led.led[1];
            obj[F("led2")] = mConfig->led.led[2];
            obj[F("led_high_active")] = mConfig->led.high_active;
            obj[F("led_lum")]         = mConfig->led.luminance;
        }

        #if defined(ESP32)
        void getRadioCmt(JsonObject obj) {
            obj[F("sclk")]  = mConfig->cmt.pinSclk;
            obj[F("sdio")]  = mConfig->cmt.pinSdio;
            obj[F("csb")]   = mConfig->cmt.pinCsb;
            obj[F("fcsb")]  = mConfig->cmt.pinFcsb;
            obj[F("gpio3")] = mConfig->cmt.pinIrq;
            obj[F("en")]    = (bool) mConfig->cmt.enabled;
            std::pair<uint16_t, uint16_t> range = mRadioCmt->getFreqRangeMhz();
            obj[F("freq_min")] = range.first;
            obj[F("freq_max")] = range.second;
        }

        void getRadioCmtInfo(JsonObject obj) {
            obj[F("en")] = (bool) mConfig->cmt.enabled;
            if(mConfig->cmt.enabled) {
                obj[F("isconnected")] = mRadioCmt->isChipConnected();
                obj[F("sn")]          = String(mRadioCmt->getDTUSn(), HEX);
                obj[F("irqOk")]       = mRadioCmt->mIrqOk;
            }
        }
        #endif

        #if defined(ETHERNET)
        void getEthernet(JsonObject obj) {
            obj[F("en")]           = mConfig->sys.eth.enabled;
            obj[F("cs")]           = mConfig->sys.eth.pinCs;
            obj[F("sclk")]         = mConfig->sys.eth.pinSclk;
            obj[F("miso")]         = mConfig->sys.eth.pinMiso;
            obj[F("mosi")]         = mConfig->sys.eth.pinMosi;
            obj[F("irq")]          = mConfig->sys.eth.pinIrq;
            obj[F("reset")]        = mConfig->sys.eth.pinRst;
        }
        #endif

        void getNetworkInfo(JsonObject obj) {
            #if defined(ETHERNET)
            bool isWired = mApp->isWiredConnection();
            if(!isWired)
                obj[F("wifi_channel")] = WiFi.channel();

            obj[F("wired")] = isWired;
            #else
                obj[F("wired")]        = false;
                obj[F("wifi_channel")] = WiFi.channel();
            #endif

            obj[F("ap_pwd")] = mConfig->sys.apPwd;
            obj[F("ssid")]   = mConfig->sys.stationSsid;
            obj[F("hidd")]   = mConfig->sys.isHidden;
            obj[F("mac")]    = mApp->getMac();
            obj[F("ip")]     = mApp->getIp();
        }

        void getChipInfo(JsonObject obj) {
            obj[F("cpu_freq")]     = ESP.getCpuFreqMHz();
            obj[F("sdk")]          = ESP.getSdkVersion();

            #if defined(ESP32)
                obj[F("temp_sensor_c")] = ah::readTemperature();
                obj[F("revision")] = ESP.getChipRevision();
                obj[F("model")]    = ESP.getChipModel();
                obj[F("cores")]    = ESP.getChipCores();

                switch (esp_reset_reason()) {
                    default:
                        [[fallthrough]];
                    case ESP_RST_UNKNOWN:
                        obj[F("reboot_reason")] = F("Unknown");
                        break;
                    case ESP_RST_POWERON:
                        obj[F("reboot_reason")] = F("Power on");
                        break;
                    case ESP_RST_EXT:
                        obj[F("reboot_reason")] = F("External");
                        break;
                    case ESP_RST_SW:
                        obj[F("reboot_reason")] = F("Software");
                        break;
                    case ESP_RST_PANIC:
                        obj[F("reboot_reason")] = F("Panic");
                        break;
                    case ESP_RST_INT_WDT:
                        obj[F("reboot_reason")] = F("Interrupt Watchdog");
                        break;
                    case ESP_RST_TASK_WDT:
                        obj[F("reboot_reason")] = F("Task Watchdog");
                        break;
                    case ESP_RST_WDT:
                        obj[F("reboot_reason")] = F("Watchdog");
                        break;
                    case ESP_RST_DEEPSLEEP:
                        obj[F("reboot_reason")] = F("Deepsleep");
                        break;
                    case ESP_RST_BROWNOUT:
                        obj[F("reboot_reason")] = F("Brownout");
                        break;
                    case ESP_RST_SDIO:
                        obj[F("reboot_reason")] = F("SDIO");
                        break;
                }
            #else
                obj[F("core_version")]  = ESP.getCoreVersion();
                obj[F("reboot_reason")] = ESP.getResetReason();
            #endif
        }

        void getMemoryInfo(JsonObject obj) {
            obj[F("heap_frag")]         = mHeapFrag;
            obj[F("heap_max_free_blk")] = mHeapFreeBlk;
            obj[F("heap_free")]         = mHeapFree;

            obj[F("par_size_app0")] = ESP.getFreeSketchSpace();
            obj[F("par_used_app0")] = ESP.getSketchSize();

            #if defined(ESP32)
                obj[F("heap_total")] = ESP.getHeapSize();

                const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, "coredump");
                if (partition != NULL)
                    obj[F("flash_size")] = partition->address + partition->size;

                obj[F("par_size_spiffs")] = LittleFS.totalBytes();
                obj[F("par_used_spiffs")] = LittleFS.usedBytes();
            #else
                obj[F("flash_size")] = ESP.getFlashChipRealSize();

                FSInfo info;
                LittleFS.info(info);
                obj[F("par_used_spiffs")] = info.usedBytes;
                obj[F("par_size_spiffs")] = info.totalBytes;
                obj[F("heap_total")] = 24*1014; // FIXME: don't know correct value
            #endif
        }

        void getRadioNrf(JsonObject obj) {
            obj[F("en")] = (bool) mConfig->nrf.enabled;
            if(mConfig->nrf.enabled) {
                obj[F("isconnected")] = mRadioNrf->isChipConnected();
                obj[F("dataRate")]    = mRadioNrf->getDataRate();
                obj[F("sn")]          = String(mRadioNrf->getDTUSn(), HEX);
                obj[F("irqOk")]       = mRadioNrf->mIrqOk;
            }
        }

        void getSerial(JsonObject obj) {
            obj[F("show_live_data")] = mConfig->serial.showIv;
            obj[F("debug")]          = mConfig->serial.debug;
            obj[F("priv")]           = mConfig->serial.privacyLog;
            obj[F("wholeTrace")]     = mConfig->serial.printWholeTrace;
            obj[F("log2mqtt")]       = mConfig->serial.log2mqtt;
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
            obj[F("disp_typ")]          = (uint8_t)mConfig->plugin.display.type;
            obj[F("disp_motion")]       = (boolean)mConfig->plugin.display.motion;
            obj[F("disp_pwr")]          = (bool)mConfig->plugin.display.pwrSaveAtIvOffline;
            obj[F("disp_screensaver")]  = (uint8_t)mConfig->plugin.display.screenSaver;
            obj[F("disp_rot")]          = (uint8_t)mConfig->plugin.display.rot;
            obj[F("disp_cont")]         = (uint8_t)mConfig->plugin.display.contrast;
            obj[F("disp_graph_ratio")]  = (uint8_t)mConfig->plugin.display.graph_ratio;
            obj[F("disp_graph_size")]   = (uint8_t)mConfig->plugin.display.graph_size;
            obj[F("disp_clk")]          = mConfig->plugin.display.disp_clk;
            obj[F("disp_data")]         = mConfig->plugin.display.disp_data;
            obj[F("disp_cs")]           = mConfig->plugin.display.disp_cs;
            obj[F("disp_dc")]           = mConfig->plugin.display.disp_dc;
            obj[F("disp_rst")]          = mConfig->plugin.display.disp_reset;
            obj[F("disp_bsy")]          = mConfig->plugin.display.disp_busy;
            obj[F("pir_pin")]           = mConfig->plugin.display.pirPin;
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
            obj[F("ts_now")]     = mApp->getTimestamp();
            obj[F("ts_sunrise")] = mApp->getSunrise();
            obj[F("ts_sunset")]  = mApp->getSunset();
            obj[F("ts_offsSr")]  = mConfig->sun.offsetSecMorning;
            obj[F("ts_offsSs")]  = mConfig->sun.offsetSecEvening;

            JsonArray inv = obj.createNestedArray(F("inverter"));
            Inverter<> *iv;
            bool disNightCom = false;
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
                iv = mSys->getInverterByPos(i);
                if(NULL == iv)
                    continue;

                record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
                JsonObject invObj = inv.createNestedObject();
                invObj[F("enabled")]         = (bool)iv->config->enabled;
                invObj[F("id")]              = i;
                invObj[F("name")]            = String(iv->config->name);
                invObj[F("cur_pwr")]         = ah::round3(iv->getChannelFieldValue(CH0, FLD_PAC, rec));
                invObj[F("is_avail")]        = iv->isAvailable();
                invObj[F("is_producing")]    = iv->isProducing();
                invObj[F("ts_last_success")] = iv->getLastTs(rec);
                if(iv->config->disNightCom)
                    disNightCom = true;
            }
            obj[F("disNightComm")] = disNightCom;

            JsonArray warn = obj.createNestedArray(F("warnings"));
            if(mApp->getRebootRequestState())
                warn.add(F(REBOOT_ESP_APPLY_CHANGES));
            if(0 == mApp->getTimestamp())
                warn.add(F(TIME_NOT_SET));
            #if !defined(ESP32)
            if(mApp->getWasInCh12to14())
                warn.add(F(WAS_IN_CH_12_TO_14));
            #endif
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
            #if defined(ETHERNET)
            getEthernet(obj.createNestedObject(F("eth")));
            #endif
            getRadioNrf(obj.createNestedObject(F("radioNrf")));
            getSerial(obj.createNestedObject(F("serial")));
            getStaticIp(obj.createNestedObject(F("static_ip")));
            getDisplay(obj.createNestedObject(F("display")));
        }

        void getNetworks(JsonObject obj) {
            obj[F("success")] = mApp->getAvailNetworks(obj);
            obj[F("ip")] = mApp->getIp();
        }

        void getIp(JsonObject obj) {
            obj[F("ip")] = mApp->getIp();
        }

        void getLive(AsyncWebServerRequest *request, JsonObject obj) {
            getGeneric(request, obj.createNestedObject(F("generic")));
            obj[F("refresh")] = mConfig->inst.sendInterval;
            obj[F("max_total_pwr")] = ah::round3(mApp->getTotalMaxPower());

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

        #if defined(ENABLE_HISTORY)
        void getPowerHistory(AsyncWebServerRequest *request, JsonObject obj, HistoryStorageType type) {
            getGeneric(request, obj.createNestedObject(F("generic")));
            obj[F("refresh")] = mApp->getHistoryPeriod(static_cast<uint8_t>(type));

            uint16_t max = 0;
            for (uint16_t fld = 0; fld < HISTORY_DATA_ARR_LENGTH; fld++) {
                uint16_t value = mApp->getHistoryValue(static_cast<uint8_t>(type), fld);
                obj[F("value")][fld] = value;
                if (value > max)
                    max = value;
            }
            obj[F("max")] = max;

            if(HistoryStorageType::POWER_DAY == type) {
                float yldDay = 0;
                for (uint8_t i = 0; i < mSys->getNumInverters(); i++) {
                    Inverter<> *iv = mSys->getInverterByPos(i);
                    if (iv == NULL)
                        continue;
                    record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
                    yldDay += iv->getChannelFieldValue(CH0, FLD_YD, rec);
                }
                obj[F("yld")] = ah::round3(yldDay / 1000.0);
            }

            obj[F("lastValueTs")] = mApp->getHistoryLastValueTs(static_cast<uint8_t>(type));
        }
        #endif /*ENABLE_HISTORY*/


        #if defined(ENABLE_HISTORY_YIELD_PER_DAY)
        void getYieldDayHistory(AsyncWebServerRequest *request, JsonObject obj) {
            obj[F("refresh")] = mApp->getHistoryPeriod((uint8_t)HistoryStorageType::YIELD);
            uint16_t max = 0;
            for (uint16_t fld = 0; fld < HISTORY_DATA_ARR_LENGTH; fld++) {
                uint16_t value = mApp->getHistoryValue((uint8_t)HistoryStorageType::YIELD, fld);
                obj[F("value")][fld] = value;
                if (value > max)
                    max = value;
            }
            obj[F("max")] = max;
        }
        #endif /*ENABLE_HISTORY_YIELD_PER_DAY*/

        bool setCtrl(JsonObject jsonIn, JsonObject jsonOut, const char *clientIP) {
            if(jsonIn.containsKey(F("auth"))) {
                if(String(jsonIn[F("auth")]) == String(mConfig->sys.adminPwd)) {
                    jsonOut[F("token")] = mApp->unlock(clientIP, false);
                    jsonIn[F("token")] = jsonOut[F("token")];
                } else {
                    jsonOut[F("error")] = F("ERR_AUTH");
                    return false;
                }
                if(!jsonIn.containsKey(F("cmd")))
                    return true;
            }

            if(isProtected(jsonIn, jsonOut, clientIP))
                return false;

            Inverter<> *iv = mSys->getInverterByPos(jsonIn[F("id")]);
            bool accepted = true;
            if(NULL == iv) {
                jsonOut[F("error")] = F("ERR_INDEX");
                return false;
            }
            jsonOut[F("id")] = jsonIn[F("id")];

            if(F("power") == jsonIn[F("cmd")])
                accepted = iv->setDevControlRequest((jsonIn[F("val")] == 1) ? TurnOn : TurnOff);
            else if(F("restart") == jsonIn[F("cmd")])
                accepted = iv->setDevControlRequest(Restart);
            else if(0 == strncmp("limit_", jsonIn[F("cmd")].as<const char*>(), 6)) {
                iv->powerLimit[0] = static_cast<uint16_t>(jsonIn["val"].as<float>() * 10.0);
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
            } else if(F("restart_ahoy") == jsonIn[F("cmd")]) {
                mApp->setRebootFlag();
            } else {
                jsonOut[F("error")] = F("ERR_UNKNOWN_CMD");
                return false;
            }

            if(!accepted) {
                jsonOut[F("error")] = F("ERR_LIMIT_NOT_ACCEPT");
                return false;
            }

            return true;
        }

        bool setSetup(JsonObject jsonIn, JsonObject jsonOut, const char *clientIP) {
            if(isProtected(jsonIn, jsonOut, clientIP))
                return false;

            if(F("set_time") == jsonIn[F("cmd")])
                mApp->setTimestamp(jsonIn[F("val")]);
            else if(F("sync_ntp") == jsonIn[F("cmd")])
                mApp->setTimestamp(0); // 0: update ntp flag
            else if(F("serial_utc_offset") == jsonIn[F("cmd")])
                mTimezoneOffset = jsonIn[F("val")];
            else if(F("discovery_cfg") == jsonIn[F("cmd")])
                mApp->setMqttDiscoveryFlag(); // for homeassistant
            else if(F("save_wifi") == jsonIn[F("cmd")]) {
                snprintf(mConfig->sys.stationSsid, SSID_LEN, "%s", jsonIn[F("ssid")].as<const char*>());
                snprintf(mConfig->sys.stationPwd, PWD_LEN, "%s", jsonIn[F("pwd")].as<const char*>());
                mApp->saveSettings(false); // without reboot
                mApp->setupStation();
            }
            #if defined(ETHERNET)
            else if(F("save_eth") == jsonIn[F("cmd")]) {
                mConfig->sys.eth.enabled = jsonIn[F("en")].as<bool>();
                mConfig->sys.eth.pinCs = jsonIn[F("cs")].as<uint8_t>();
                mConfig->sys.eth.pinSclk = jsonIn[F("sclk")].as<uint8_t>();
                mConfig->sys.eth.pinMiso = jsonIn[F("miso")].as<uint8_t>();
                mConfig->sys.eth.pinMosi = jsonIn[F("mosi")].as<uint8_t>();
                mConfig->sys.eth.pinIrq = jsonIn[F("irq")].as<uint8_t>();
                mConfig->sys.eth.pinRst = jsonIn[F("reset")].as<uint8_t>();
                mApp->saveSettings(true);
            }
            #endif
            else if(F("save_iv") == jsonIn[F("cmd")]) {
                Inverter<> *iv;

                for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                    iv = mSys->getInverterByPos(i, true);
                    if(nullptr != iv) {
                        if((i != jsonIn[F("id")]) && (iv->config->serial.u64 == jsonIn[F("ser")])) {
                            jsonOut[F("error")] = F("ERR_DUPLICATE_INVERTER");
                            return false;
                        }
                    }
                }

                iv = mSys->getInverterByPos(jsonIn[F("id")], false);
                iv->config->enabled = jsonIn[F("en")];
                iv->config->serial.u64 = jsonIn[F("ser")];
                snprintf(iv->config->name, MAX_NAME_LENGTH, "%s", jsonIn[F("name")].as<const char*>());

                for(uint8_t i = 0; i < 6; i++) {
                    iv->config->chMaxPwr[i] = jsonIn[F("ch")][i][F("pwr")];
                    iv->config->yieldCor[i] = jsonIn[F("ch")][i][F("yld")];
                    snprintf(iv->config->chName[i], MAX_NAME_LENGTH, "%s", jsonIn[F("ch")][i][F("name")].as<const char*>());
                }

                mApp->initInverter(jsonIn[F("id")]);
                iv->config->frequency   = jsonIn[F("freq")];
                iv->config->powerLevel  = jsonIn[F("pa")];
                iv->config->disNightCom = jsonIn[F("disnightcom")];
                mApp->saveSettings(false); // without reboot
            } else {
                jsonOut[F("error")] = F("ERR_UNKNOWN_CMD");
                return false;
            }

            return true;
        }

        bool isProtected(JsonObject jsonIn, JsonObject jsonOut, const char *clientIP) {
            if(mConfig->sys.adminPwd[0] != '\0') { // check if admin password is set
                if(strncmp("*", clientIP, 1) != 0) { // no call from MqTT
                    const char* token = nullptr;
                    if(jsonIn.containsKey(F("token")))
                        token = jsonIn["token"];

                    if(!mApp->isProtected(clientIP, token, false))
                        return false;

                    jsonOut[F("error")] = F("ERR_PROTECTED");
                    return true;
                }
            }

            return false;
        }

    private:
        constexpr static uint8_t acList[] = {FLD_UAC, FLD_IAC, FLD_PAC, FLD_F, FLD_PF, FLD_T, FLD_YT,
            FLD_YD, FLD_PDC, FLD_EFF, FLD_Q, FLD_MP, FLD_MT};
        constexpr static uint8_t acListHmt[] = {FLD_UAC_1N, FLD_IAC_1, FLD_PAC, FLD_F, FLD_PF, FLD_T,
            FLD_YT, FLD_YD, FLD_PDC, FLD_EFF, FLD_Q, FLD_MP, FLD_MT};
        constexpr static uint8_t dcList[] = {FLD_UDC, FLD_IDC, FLD_PDC, FLD_YD, FLD_YT, FLD_IRR, FLD_MP};

    private:
        IApp *mApp = nullptr;
        HMSYSTEM *mSys = nullptr;
        NrfRadio<> *mRadioNrf = nullptr;
        #if defined(ESP32)
        CmtRadio<> *mRadioCmt = nullptr;
        #endif
        AsyncWebServer *mSrv = nullptr;
        settings_t *mConfig = nullptr;

        uint32_t mTimezoneOffset = 0;
        uint32_t mHeapFree = 0, mHeapFreeBlk = 0;
        uint8_t mHeapFrag = 0;
        uint8_t *mTmpBuf = nullptr;
        uint32_t mTmpSize = 0;
};

#endif /*__WEB_API_H__*/
