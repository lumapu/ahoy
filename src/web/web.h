//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __WEB_H__
#define __WEB_H__

#include "../utils/dbg.h"
#ifdef ESP32
    #include "AsyncTCP.h"
    #include "Update.h"
#else
    #include "ESPAsyncTCP.h"
#endif
#include "ESPAsyncWebServer.h"

#include "../appInterface.h"

#include "../hm/hmSystem.h"
#include "../utils/helper.h"

#include "html/h/index_html.h"
#include "html/h/login_html.h"
#include "html/h/style_css.h"
#include "html/h/api_js.h"
#include "html/h/favicon_ico.h"
#include "html/h/setup_html.h"
#include "html/h/visualization_html.h"
#include "html/h/update_html.h"
#include "html/h/serial_html.h"
#include "html/h/system_html.h"

#define WEB_SERIAL_BUF_SIZE 2048

const char* const pinArgNames[] = {"pinCs", "pinCe", "pinIrq", "pinLed0", "pinLed1"};

template<class HMSYSTEM>
class Web {
    public:
        Web(void) : mWeb(80), mEvts("/events") {
            mProtected     = true;
            mLogoutTimeout = 0;

            memset(mSerialBuf, 0, WEB_SERIAL_BUF_SIZE);
            mSerialBufFill     = 0;
            mSerialAddTime     = true;
            mSerialClientConnnected = false;
        }

        void setup(IApp *app, HMSYSTEM *sys, settings_t *config) {
            mApp     = app;
            mSys     = sys;
            mConfig  = config;

            DPRINTLN(DBG_VERBOSE, F("app::setup-on"));
            mWeb.on("/",               HTTP_GET,  std::bind(&Web::onIndex,        this, std::placeholders::_1));
            mWeb.on("/login",          HTTP_ANY,  std::bind(&Web::onLogin,        this, std::placeholders::_1));
            mWeb.on("/logout",         HTTP_GET,  std::bind(&Web::onLogout,       this, std::placeholders::_1));
            mWeb.on("/style.css",      HTTP_GET,  std::bind(&Web::onCss,          this, std::placeholders::_1));
            mWeb.on("/api.js",         HTTP_GET,  std::bind(&Web::onApiJs,        this, std::placeholders::_1));
            mWeb.on("/favicon.ico",    HTTP_GET,  std::bind(&Web::onFavicon,      this, std::placeholders::_1));
            mWeb.onNotFound (                     std::bind(&Web::showNotFound,   this, std::placeholders::_1));
            mWeb.on("/reboot",         HTTP_ANY,  std::bind(&Web::onReboot,       this, std::placeholders::_1));
            mWeb.on("/system",         HTTP_ANY,  std::bind(&Web::onSystem,       this, std::placeholders::_1));
            mWeb.on("/erase",          HTTP_ANY,  std::bind(&Web::showErase,      this, std::placeholders::_1));
            mWeb.on("/factory",        HTTP_ANY,  std::bind(&Web::showFactoryRst, this, std::placeholders::_1));

            mWeb.on("/setup",          HTTP_GET,  std::bind(&Web::onSetup,        this, std::placeholders::_1));
            mWeb.on("/save",           HTTP_ANY,  std::bind(&Web::showSave,       this, std::placeholders::_1));

            mWeb.on("/live",           HTTP_ANY,  std::bind(&Web::onLive,         this, std::placeholders::_1));
            //mWeb.on("/api1",           HTTP_POST, std::bind(&Web::showWebApi,     this, std::placeholders::_1));

        #ifdef ENABLE_JSON_EP
            mWeb.on("/json",           HTTP_ANY,  std::bind(&Web::showJson,       this, std::placeholders::_1));
        #endif
        #ifdef ENABLE_PROMETHEUS_EP
            mWeb.on("/metrics",        HTTP_ANY,  std::bind(&Web::showMetrics,    this, std::placeholders::_1));
        #endif

            mWeb.on("/update",         HTTP_GET,  std::bind(&Web::onUpdate,       this, std::placeholders::_1));
            mWeb.on("/update",         HTTP_POST, std::bind(&Web::showUpdate,     this, std::placeholders::_1),
                                                  std::bind(&Web::showUpdate2,    this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
            mWeb.on("/upload",         HTTP_POST, std::bind(&Web::onUpload,       this, std::placeholders::_1),
                                                  std::bind(&Web::onUpload2,      this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
            mWeb.on("/serial",         HTTP_GET,  std::bind(&Web::onSerial,       this, std::placeholders::_1));
            mWeb.on("/debug",          HTTP_GET,  std::bind(&Web::onDebug,        this, std::placeholders::_1));


            mEvts.onConnect(std::bind(&Web::onConnect, this, std::placeholders::_1));
            mWeb.addHandler(&mEvts);

            mWeb.begin();

            registerDebugCb(std::bind(&Web::serialCb, this, std::placeholders::_1)); // dbg.h

            mUploadFail = false;
        }

        void tickSecond() {
            if(0 != mLogoutTimeout) {
                mLogoutTimeout -= 1;
                if(0 == mLogoutTimeout) {
                    if(strlen(mConfig->sys.adminPwd) > 0)
                        mProtected = true;
                }

                DPRINTLN(DBG_DEBUG, "auto logout in " + String(mLogoutTimeout));
            }

            if(mSerialClientConnnected) {
                if(mSerialBufFill > 0) {
                    mEvts.send(mSerialBuf, "serial", millis());
                    memset(mSerialBuf, 0, WEB_SERIAL_BUF_SIZE);
                    mSerialBufFill = 0;
                }
            }
        }

        AsyncWebServer *getWebSrvPtr(void) {
            return &mWeb;
        }

        void setProtection(bool protect) {
            mProtected = protect;
        }

        bool getProtection() {
            return mProtected;
        }

        void showUpdate2(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            mApp->setOnUpdate();
            
            if(!index) {
                Serial.printf("Update Start: %s\n", filename.c_str());
        #ifndef ESP32
                Update.runAsync(true);
        #endif
                if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
                    Update.printError(Serial);
                }
            }
            if(!Update.hasError()) {
                if(Update.write(data, len) != len){
                    Update.printError(Serial);
                }
            }
            if(final) {
                if(Update.end(true)) {
                    Serial.printf("Update Success: %uB\n", index+len);
                } else {
                    Update.printError(Serial);
                }
            }
        }

        void onUpload2(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if(!index) {
                mUploadFail = false;
                mUploadFp = LittleFS.open("/tmp.json", "w");
                if(!mUploadFp) {
                    DPRINTLN(DBG_ERROR, F("can't open file!"));
                    mUploadFail = true;
                    mUploadFp.close();
                }
            }
            mUploadFp.write(data, len);
            if(final) {
                mUploadFp.close();
                File fp = LittleFS.open("/tmp.json", "r");
                if(!fp)
                    mUploadFail = true;
                else {
                    if(!mApp->readSettings("tmp.json")) {
                        mUploadFail = true;
                        DPRINTLN(DBG_ERROR, F("upload JSON error!"));
                    }
                    else
                        mApp->saveSettings();
                }
                DPRINTLN(DBG_INFO, F("upload finished!"));
            }
        }

        void serialCb(String msg) {
            if(!mSerialClientConnnected)
                return;

            msg.replace("\r\n", "<rn>");
            if(mSerialAddTime) {
                if((9 + mSerialBufFill) <= WEB_SERIAL_BUF_SIZE) {
                    if(mApp->getTimestamp() > 0) {
                        strncpy(&mSerialBuf[mSerialBufFill], mApp->getTimeStr(mApp->getTimezoneOffset()).c_str(), 9);
                        mSerialBufFill += 9;
                    }
                }
                else {
                    mSerialBufFill = 0;
                    mEvts.send("webSerial, buffer overflow!<rn>", "serial", millis());
                    return;
                }
                mSerialAddTime = false;
            }

            if(msg.endsWith("<rn>"))
                mSerialAddTime = true;

            uint16_t length = msg.length();
            if((length + mSerialBufFill) <= WEB_SERIAL_BUF_SIZE) {
                strncpy(&mSerialBuf[mSerialBufFill], msg.c_str(), length);
                mSerialBufFill += length;
            }
            else {
                mSerialBufFill = 0;
                mEvts.send("webSerial, buffer overflow!<rn>", "serial", millis());
            }
        }

    private:
        void onUpdate(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("onUpdate"));

            if(CHECK_MASK(mConfig->sys.protectionMask, PROT_MASK_UPDATE)) {
                if(mProtected) {
                    request->redirect("/login");
                    return;
                }
            }

            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), update_html, update_html_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        void showUpdate(AsyncWebServerRequest *request) {
            bool reboot = !Update.hasError();

            String html = F("<!doctype html><html><head><title>Update</title><meta http-equiv=\"refresh\" content=\"20; URL=/\"></head><body>Update: ");
            if(reboot)
                html += "success";
            else
                html += "failed";
            html += F("<br/><br/>rebooting ... auto reload after 20s</body></html>");

            AsyncWebServerResponse *response = request->beginResponse(200, F("text/html"), html);
            response->addHeader("Connection", "close");
            request->send(response);
            //if(reboot)
                mApp->setRebootFlag();
        }

        void onUpload(AsyncWebServerRequest *request) {
            bool reboot = !mUploadFail;

            String html = F("<!doctype html><html><head><title>Upload</title><meta http-equiv=\"refresh\" content=\"20; URL=/\"></head><body>Upload: ");
            if(reboot)
                html += "success";
            else
                html += "failed";
            html += F("<br/><br/>rebooting ... auto reload after 20s</body></html>");

            AsyncWebServerResponse *response = request->beginResponse(200, F("text/html"), html);
            response->addHeader("Connection", "close");
            request->send(response);
            //if(reboot)
                mApp->setRebootFlag();
        }

        void onConnect(AsyncEventSourceClient *client) {
            DPRINTLN(DBG_VERBOSE, "onConnect");

            mSerialClientConnnected = true;

            if(client->lastId())
                DPRINTLN(DBG_VERBOSE, "Client reconnected! Last message ID that it got is: " + String(client->lastId()));

            client->send("hello!", NULL, millis(), 1000);
        }

        void onIndex(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("onIndex"));

            if(CHECK_MASK(mConfig->sys.protectionMask, PROT_MASK_INDEX)) {
                if(mProtected) {
                    request->redirect("/login");
                    return;
                }
            }

            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), index_html, index_html_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        void onLogin(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("onLogin"));

            if(request->args() > 0) {
                if(String(request->arg("pwd")) == String(mConfig->sys.adminPwd)) {
                    mProtected = false;
                    request->redirect("/");
                }
            }

            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), login_html, login_html_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        void onLogout(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("onLogout"));

            if(mProtected) {
                request->redirect("/login");
                return;
            }

            mProtected = true;

            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), system_html, system_html_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        void onCss(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("onCss"));
            mLogoutTimeout = LOGOUT_TIMEOUT;
            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/css"), style_css, style_css_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        void onApiJs(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("onApiJs"));

            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/javascript"), api_js, api_js_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        void onFavicon(AsyncWebServerRequest *request) {
            static const char favicon_type[] PROGMEM = "image/x-icon";
            AsyncWebServerResponse *response = request->beginResponse_P(200, favicon_type, favicon_ico, favicon_ico_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        void showNotFound(AsyncWebServerRequest *request) {
            if(mProtected)
                request->redirect("/login");
            else
                request->redirect("/setup");
        }

        void onReboot(AsyncWebServerRequest *request) {
            mApp->setRebootFlag();
            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), system_html, system_html_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        void showErase(AsyncWebServerRequest *request) {
            if(mProtected) {
                request->redirect("/login");
                return;
            }

            DPRINTLN(DBG_VERBOSE, F("showErase"));
            mApp->eraseSettings(false);
            onReboot(request);
        }

        void showFactoryRst(AsyncWebServerRequest *request) {
            if(mProtected) {
                request->redirect("/login");
                return;
            }

            DPRINTLN(DBG_VERBOSE, F("showFactoryRst"));
            String content = "";
            int refresh = 3;
            if(request->args() > 0) {
                if(request->arg("reset").toInt() == 1) {
                    refresh = 10;
                    if(mApp->eraseSettings(true))
                        content = F("factory reset: success\n\nrebooting ... ");
                    else
                        content = F("factory reset: failed\n\nrebooting ... ");
                }
                else {
                    content = F("factory reset: aborted");
                    refresh = 3;
                }
            }
            else {
                content = F("<h1>Factory Reset</h1>"
                    "<p><a href=\"/factory?reset=1\">RESET</a><br/><br/><a href=\"/factory?reset=0\">CANCEL</a><br/></p>");
                refresh = 120;
            }
            request->send(200, F("text/html"), F("<!doctype html><html><head><title>Factory Reset</title><meta http-equiv=\"refresh\" content=\"") + String(refresh) + F("; URL=/\"></head><body>") + content + F("</body></html>"));
            if(refresh == 10) {
                delay(1000);
                ESP.restart();
            }
        }

        void onSetup(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("onSetup"));

            if(CHECK_MASK(mConfig->sys.protectionMask, PROT_MASK_SETUP)) {
                if(mProtected) {
                    request->redirect("/login");
                    return;
                }
            }

            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), setup_html, setup_html_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        void showSave(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("showSave"));

            if(mProtected) {
                request->redirect("/login");
                return;
            }

            if(request->args() == 0)
                return;

            char buf[20] = {0};

            // general
            if(request->arg("ssid") != "")
                request->arg("ssid").toCharArray(mConfig->sys.stationSsid, SSID_LEN);
            if(request->arg("pwd") != "{PWD}")
                request->arg("pwd").toCharArray(mConfig->sys.stationPwd, PWD_LEN);
            if(request->arg("device") != "")
                request->arg("device").toCharArray(mConfig->sys.deviceName, DEVNAME_LEN);

            // protection
            if(request->arg("adminpwd") != "{PWD}") {
                request->arg("adminpwd").toCharArray(mConfig->sys.adminPwd, PWD_LEN);
                mProtected = (strlen(mConfig->sys.adminPwd) > 0);
            }
            mConfig->sys.protectionMask = 0x0000;
            for(uint8_t i = 0; i < 6; i++) {
                if(request->arg("protMask" + String(i)) == "on")
                    mConfig->sys.protectionMask |= (1 << i);
            }

            // static ip
            request->arg("ipAddr").toCharArray(buf, 20);
            ah::ip2Arr(mConfig->sys.ip.ip, buf);
            request->arg("ipMask").toCharArray(buf, 20);
            ah::ip2Arr(mConfig->sys.ip.mask, buf);
            request->arg("ipDns1").toCharArray(buf, 20);
            ah::ip2Arr(mConfig->sys.ip.dns1, buf);
            request->arg("ipDns2").toCharArray(buf, 20);
            ah::ip2Arr(mConfig->sys.ip.dns2, buf);
            request->arg("ipGateway").toCharArray(buf, 20);
            ah::ip2Arr(mConfig->sys.ip.gateway, buf);


            // inverter
            Inverter<> *iv;
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
                iv = mSys->getInverterByPos(i, false);
                // enable communication
                iv->config->enabled = (request->arg("inv" + String(i) + "Enable") == "on");
                // address
                request->arg("inv" + String(i) + "Addr").toCharArray(buf, 20);
                if(strlen(buf) == 0)
                    memset(buf, 0, 20);
                iv->config->serial.u64 = ah::Serial2u64(buf);
                switch(iv->config->serial.b[4]) {
                    case 0x21: iv->type = INV_TYPE_1CH; iv->channels = 1; break;
                    case 0x41: iv->type = INV_TYPE_2CH; iv->channels = 2; break;
                    case 0x61: iv->type = INV_TYPE_4CH; iv->channels = 4; break;
                    default:  break;
                }
                iv->config->yieldCor = request->arg("inv" + String(i) + "YieldCor").toInt();

                // name
                request->arg("inv" + String(i) + "Name").toCharArray(iv->config->name, MAX_NAME_LENGTH);

                // max channel power / name
                for(uint8_t j = 0; j < 4; j++) {
                    iv->config->chMaxPwr[j] = request->arg("inv" + String(i) + "ModPwr" + String(j)).toInt() & 0xffff;
                    request->arg("inv" + String(i) + "ModName" + String(j)).toCharArray(iv->config->chName[j], MAX_NAME_LENGTH);
                }
                iv->initialized = true;
            }

            if(request->arg("invInterval") != "")
                mConfig->nrf.sendInterval = request->arg("invInterval").toInt();
            if(request->arg("invRetry") != "")
                mConfig->nrf.maxRetransPerPyld = request->arg("invRetry").toInt();

            // pinout
            uint8_t pin;
            for(uint8_t i = 0; i < 5; i ++) {
                pin = request->arg(String(pinArgNames[i])).toInt();
                switch(i) {
                    default: mConfig->nrf.pinCs    = ((pin != 0xff) ? pin : DEF_CS_PIN);  break;
                    case 1:  mConfig->nrf.pinCe    = ((pin != 0xff) ? pin : DEF_CE_PIN);  break;
                    case 2:  mConfig->nrf.pinIrq   = ((pin != 0xff) ? pin : DEF_IRQ_PIN); break;
                    case 3:  mConfig->led.led0 = pin; break;
                    case 4:  mConfig->led.led1 = pin; break;
                }
            }

            // nrf24 amplifier power
            mConfig->nrf.amplifierPower = request->arg("rf24Power").toInt() & 0x03;

            // ntp
            if(request->arg("ntpAddr") != "") {
                request->arg("ntpAddr").toCharArray(mConfig->ntp.addr, NTP_ADDR_LEN);
                mConfig->ntp.port = request->arg("ntpPort").toInt() & 0xffff;
            }

            // sun
            if(request->arg("sunLat") == "" || (request->arg("sunLon") == "")) {
                mConfig->sun.lat = 0.0;
                mConfig->sun.lon = 0.0;
                mConfig->sun.disNightCom = false;
                mConfig->sun.offsetSec = 0;
            } else {
                mConfig->sun.lat = request->arg("sunLat").toFloat();
                mConfig->sun.lon = request->arg("sunLon").toFloat();
                mConfig->sun.disNightCom = (request->arg("sunDisNightCom") == "on");
                mConfig->sun.offsetSec = request->arg("sunOffs").toInt() * 60;
            }

            // mqtt
            if(request->arg("mqttAddr") != "") {
                String addr = request->arg("mqttAddr");
                addr.trim();
                addr.toCharArray(mConfig->mqtt.broker, MQTT_ADDR_LEN);
            }
            else
                mConfig->mqtt.broker[0] = '\0';
            request->arg("mqttUser").toCharArray(mConfig->mqtt.user, MQTT_USER_LEN);
            if(request->arg("mqttPwd") != "{PWD}")
                request->arg("mqttPwd").toCharArray(mConfig->mqtt.pwd, MQTT_PWD_LEN);
            request->arg("mqttTopic").toCharArray(mConfig->mqtt.topic, MQTT_TOPIC_LEN);
            mConfig->mqtt.port = request->arg("mqttPort").toInt();
            mConfig->mqtt.interval = request->arg("mqttInterval").toInt();
            mConfig->mqtt.rstYieldMidNight = (request->arg("mqttRstMid") == "on");
            mConfig->mqtt.rstValsCommStop  = (request->arg("mqttRstComStop") == "on");
            mConfig->mqtt.rstValsNotAvail  = (request->arg("mqttRstNotAvail") == "on");

            // serial console
            if(request->arg("serIntvl") != "") {
                mConfig->serial.interval = request->arg("serIntvl").toInt() & 0xffff;

                mConfig->serial.debug  = (request->arg("serDbg") == "on");
                mConfig->serial.showIv = (request->arg("serEn") == "on");
                // Needed to log TX buffers to serial console
                mSys->Radio.mSerialDebug = mConfig->serial.debug;
            }

            // display
            mConfig->plugin.display.pwrSaveAtIvOffline  = (request->arg("dispPwr") == "on");
            mConfig->plugin.display.logoEn   = (request->arg("logoEn") == "on");
            mConfig->plugin.display.pxShift  = (request->arg("dispPxSh") == "on");
            mConfig->plugin.display.rot180   = (request->arg("disp180") == "on");
            mConfig->plugin.display.type     = request->arg("dispType").toInt();
            mConfig->plugin.display.contrast = request->arg("dispCont").toInt();
            mConfig->plugin.display.pin0     = request->arg("pinDisp0").toInt();
            mConfig->plugin.display.pin1     = request->arg("pinDisp1").toInt();


            mApp->saveSettings();

            if(request->arg("reboot") == "on")
                onReboot(request);
            else {
                AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), system_html, system_html_len);
                response->addHeader(F("Content-Encoding"), "gzip");
                request->send(response);
            }
        }

        void onLive(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("onLive"));

            if(CHECK_MASK(mConfig->sys.protectionMask, PROT_MASK_LIVE)) {
                if(mProtected) {
                    request->redirect("/login");
                    return;
                }
            }

            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), visualization_html, visualization_html_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        /*void showWebApi(AsyncWebServerRequest *request) {
            // TODO: remove
            DPRINTLN(DBG_VERBOSE, F("web::showWebApi"));
            DPRINTLN(DBG_DEBUG, request->arg("plain"));
            const size_t capacity = 200; // Use arduinojson.org/assistant to compute the capacity.
            DynamicJsonDocument response(capacity);

            // Parse JSON object
            deserializeJson(response, request->arg("plain"));
            // ToDo: error handling for payload
            uint8_t iv_id = response["inverter"];
            uint8_t cmd = response["cmd"];
            Inverter<> *iv = mSys->getInverterByPos(iv_id);
            if (NULL != iv) {
                if (response["tx_request"] == (uint8_t)TX_REQ_INFO) {
                    // if the AlarmData is requested set the Alarm Index to the requested one
                    if (cmd == AlarmData || cmd == AlarmUpdate) {
                        // set the AlarmMesIndex for the request from user input
                        iv->alarmMesIndex = response["payload"];
                    }
                    DPRINTLN(DBG_INFO, F("Will make tx-request 0x15 with subcmd ") + String(cmd) + F(" and payload ") + String((uint16_t) response["payload"]));
                    // process payload from web request corresponding to the cmd
                    iv->enqueCommand<InfoCommand>(cmd);
                }


                if (response["tx_request"] == (uint8_t)TX_REQ_DEVCONTROL) {
                    if (response["cmd"] == (uint8_t)ActivePowerContr) {
                        uint16_t webapiPayload = response["payload"];
                        uint16_t webapiPayload2 = response["payload2"];
                        if (webapiPayload > 0 && webapiPayload < 10000) {
                            iv->devControlCmd = ActivePowerContr;
                            iv->powerLimit[0] = webapiPayload;
                            if (webapiPayload2 > 0)
                                iv->powerLimit[1] = webapiPayload2; // dev option, no sanity check
                            else                                            // if not set, set it to 0x0000 default
                                iv->powerLimit[1] = AbsolutNonPersistent; // payload will be seted temporary in Watt absolut
                            if (iv->powerLimit[1] & 0x0001)
                                DPRINTLN(DBG_INFO, F("Power limit for inverter ") + String(iv->id) + F(" set to ") + String(iv->powerLimit[0]) + F("% via REST API"));
                            else
                                DPRINTLN(DBG_INFO, F("Power limit for inverter ") + String(iv->id) + F(" set to ") + String(iv->powerLimit[0]) + F("W via REST API"));
                            iv->devControlRequest = true; // queue it in the request loop
                        }
                    }
                    if (response["cmd"] == (uint8_t)TurnOff) {
                        iv->devControlCmd = TurnOff;
                        iv->devControlRequest = true; // queue it in the request loop
                    }
                    if (response["cmd"] == (uint8_t)TurnOn) {
                        iv->devControlCmd = TurnOn;
                        iv->devControlRequest = true; // queue it in the request loop
                    }
                    if (response["cmd"] == (uint8_t)CleanState_LockAndAlarm) {
                        iv->devControlCmd = CleanState_LockAndAlarm;
                        iv->devControlRequest = true; // queue it in the request loop
                    }
                    if (response["cmd"] == (uint8_t)Restart) {
                        iv->devControlCmd = Restart;
                        iv->devControlRequest = true; // queue it in the request loop
                    }
                }
            }
            request->send(200, "text/json", "{success:true}");
        }*/

        void onDebug(AsyncWebServerRequest *request) {
            mApp->getSchedulerNames();
            AsyncWebServerResponse *response = request->beginResponse(200, F("text/html"), "ok");
            request->send(response);
        }

        void onSerial(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("onSerial"));

            if(CHECK_MASK(mConfig->sys.protectionMask, PROT_MASK_SERIAL)) {
                if(mProtected) {
                    request->redirect("/login");
                    return;
                }
            }

            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), serial_html, serial_html_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        void onSystem(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("onSystem"));

            if(CHECK_MASK(mConfig->sys.protectionMask, PROT_MASK_SYSTEM)) {
                if(mProtected) {
                    request->redirect("/login");
                    return;
                }
            }

            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), system_html, system_html_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

#ifdef ENABLE_JSON_EP
        void showJson(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("web::showJson"));
            String modJson;
            Inverter<> *iv;
            record_t<> *rec;
                char topic[40], val[25];

            modJson = F("{\n");
            for(uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                iv = mSys->getInverterByPos(id);
                if(NULL == iv)
                    continue;

                rec = iv->getRecordStruct(RealTimeRunData_Debug);
                snprintf(topic, 30, "\"%s\": {\n", iv->config->name);
                modJson += String(topic);
                for(uint8_t i = 0; i < rec->length; i++) {
                    snprintf(topic, 40, "\t\"ch%d/%s\"", rec->assign[i].ch, iv->getFieldName(i, rec));
                    snprintf(val, 25, "[%.3f, \"%s\"]", iv->getValue(i, rec), iv->getUnit(i, rec));
                    modJson += String(topic) + ": " + String(val) + F(",\n");
                }
                modJson += F("\t\"last_msg\": \"") + ah::getDateTimeStr(rec->ts) + F("\"\n\t},\n");
            }
            modJson += F("\"json_ts\": \"") + String(ah::getDateTimeStr(mApp->getTimestamp())) + F("\"\n}\n");

            AsyncWebServerResponse *response = request->beginResponse(200, F("application/json"), modJson);
            request->send(response);
        }
#endif

#ifdef ENABLE_PROMETHEUS_EP
        enum {
            metricsStateStart, metricsStateInverter, metricStateChannel,metricsStateEnd
        } metricsStep;
        int metricsInverterId,metricsChannelId;

        void showMetrics(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("web::showMetrics"));

            metricsStep = metricsStateStart;
            AsyncWebServerResponse *response = request->beginChunkedResponse(F("text/plain"),
                                                                             [this](uint8_t *buffer, size_t maxLen, size_t filledLength) -> size_t
            {
                Inverter<> *iv;
                record_t<> *rec;
                statistics_t *stat;
                String metrics;
                char type[60], topic[100], val[25];
                size_t len = 0;

                switch (metricsStep) {
                    case metricsStateStart: // System Info & NRF Statistics : fit to one packet
                        snprintf(topic,sizeof(topic),"# TYPE ahoy_solar_info gauge\nahoy_solar_info{version=\"%s\",image=\"\",devicename=\"%s\"} 1\n",
                            mApp->getVersion(), mConfig->sys.deviceName);
                        metrics = topic;
                        // NRF Statistics
                        stat = mApp->getStatistics();
                        metrics += radioStatistic(F("rx_success"),     stat->rxSuccess);
                        metrics += radioStatistic(F("rx_fail"),        stat->rxFail);
                        metrics += radioStatistic(F("rx_fail_answer"), stat->rxFailNoAnser);
                        metrics += radioStatistic(F("frame_cnt"),      stat->frmCnt);
                        metrics += radioStatistic(F("tx_cnt"),         mSys->Radio.mSendCnt);

                        len = snprintf((char *)buffer,maxLen,"%s",metrics.c_str());
                        // Start Inverter loop
                        metricsInverterId = 0;
                        metricsStep = metricsStateInverter;
                        break;

                    case metricsStateInverter: // Inverter loop
                        if (metricsInverterId < mSys->getNumInverters()) {
                            iv = mSys->getInverterByPos(metricsInverterId);
                            if(NULL != iv) {
                                // Inverter info
                                len = snprintf((char *)buffer, maxLen, "ahoy_solar_inverter_info{name=\"%s\",serial=\"%12llx\",enabled=\"%d\"} 1\n",
                                    iv->config->name, iv->config->serial.u64,iv->config->enabled);
                                // Start Channel loop for this inverter
                                metricsChannelId = 0;
                                metricsStep = metricStateChannel;
                            }
                        } else {
                            metricsStep = metricsStateEnd;
                        }
                        break;

                    case metricStateChannel: // Channel loop
                        iv = mSys->getInverterByPos(metricsInverterId);
                        rec = iv->getRecordStruct(RealTimeRunData_Debug);
                        if (metricsChannelId < rec->length) {
                            uint8_t channel = rec->assign[metricsChannelId].ch;
                            String promUnit, promType;
                            std::tie(promUnit, promType) = convertToPromUnits(iv->getUnit(metricsChannelId, rec));
                            snprintf(type, sizeof(type), "# TYPE ahoy_solar_%s%s %s", iv->getFieldName(metricsChannelId, rec), promUnit.c_str(), promType.c_str());
                            if (0 == channel) {
                                snprintf(topic, sizeof(topic), "ahoy_solar_%s%s{inverter=\"%s\"}", iv->getFieldName(metricsChannelId, rec), promUnit.c_str(), iv->config->name);
                            } else {
                                snprintf(topic, sizeof(topic), "ahoy_solar_%s%s{inverter=\"%s\",channel=\"%s\"}", iv->getFieldName(metricsChannelId, rec), promUnit.c_str(), iv->config->name,iv->config->chName[channel-1]);
                            }
                            snprintf(val, sizeof(val), "%.3f", iv->getValue(metricsChannelId, rec));
                            len = snprintf((char*)buffer,maxLen,"%s\n%s %s\n",type,topic,val);

                            metricsChannelId++;
                        } else {
                            len = snprintf((char*)buffer,maxLen,"#\n"); // At least one char to send otherwise the transmission ends.

                            // All channels processed --> try next inverter
                            metricsInverterId++;
                            metricsStep = metricsStateInverter;
                        }
                        break;

                    case metricsStateEnd:
                    default: // end of transmission
                        len = 0;
                        break;
                }
                return len;
            });
            request->send(response);
        }

        String radioStatistic(String statistic, uint32_t value) {
            char type[60], topic[80], val[25];
            snprintf(type, sizeof(type), "# TYPE ahoy_solar_radio_%s gauge",statistic.c_str());
            snprintf(topic, sizeof(topic), "ahoy_solar_radio_%s",statistic.c_str());
            snprintf(val, sizeof(val), "%d", value);
            return ( String(type) + "\n" + String(topic) + " " + String(val) + "\n");
        }

        std::pair<String, String> convertToPromUnits(String shortUnit) {
            if(shortUnit == "A")    return {"_ampere", "gauge"};
            if(shortUnit == "V")    return {"_volt", "gauge"};
            if(shortUnit == "%")    return {"_ratio", "gauge"};
            if(shortUnit == "W")    return {"_watt", "gauge"};
            if(shortUnit == "Wh")   return {"_wattHours", "counter"};
            if(shortUnit == "kWh")  return {"_kilowattHours", "counter"};
            if(shortUnit == "°C")   return {"_celsius", "gauge"};
            if(shortUnit == "var")  return {"_var", "gauge"};
            if(shortUnit == "Hz")   return {"_hertz", "gauge"};
            return {"", "gauge"};
        }
#endif
        AsyncWebServer mWeb;
        AsyncEventSource mEvts;
        bool mProtected;
        uint32_t mLogoutTimeout;
        IApp *mApp;
        HMSYSTEM *mSys;

        settings_t *mConfig;

        bool mSerialAddTime;
        char mSerialBuf[WEB_SERIAL_BUF_SIZE];
        uint16_t mSerialBufFill;
        bool mSerialClientConnnected;

        File mUploadFp;
        bool mUploadFail;
};

#endif /*__WEB_H__*/
