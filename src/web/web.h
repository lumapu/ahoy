//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
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
#include "../appInterface.h"
#include "../hm/hmSystem.h"
#include "../utils/helper.h"
#if defined(ETHERNET)
#include "AsyncWebServer_ESP32_W5500.h"
#else /* defined(ETHERNET) */
#include "ESPAsyncWebServer.h"
#endif /* defined(ETHERNET) */
#include "html/h/api_js.h"
#include "html/h/colorBright_css.h"
#include "html/h/colorDark_css.h"
#include "html/h/favicon_ico.h"
#include "html/h/index_html.h"
#include "html/h/login_html.h"
#include "html/h/serial_html.h"
#include "html/h/setup_html.h"
#include "html/h/style_css.h"
#include "html/h/system_html.h"
#include "html/h/save_html.h"
#include "html/h/update_html.h"
#include "html/h/visualization_html.h"
#include "html/h/about_html.h"

#define WEB_SERIAL_BUF_SIZE 2048

const char* const pinArgNames[] = {"pinCs", "pinCe", "pinIrq", "pinSclk", "pinMosi", "pinMiso", "pinLed0", "pinLed1", "pinLedHighActive", "pinCmtSclk", "pinSdio", "pinCsb", "pinFcsb", "pinGpio3"};

template <class HMSYSTEM>
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
            mWeb.on("/colors.css",     HTTP_GET,  std::bind(&Web::onColor,        this, std::placeholders::_1));
            mWeb.on("/style.css",      HTTP_GET,  std::bind(&Web::onCss,          this, std::placeholders::_1));
            mWeb.on("/api.js",         HTTP_GET,  std::bind(&Web::onApiJs,        this, std::placeholders::_1));
            mWeb.on("/favicon.ico",    HTTP_GET,  std::bind(&Web::onFavicon,      this, std::placeholders::_1));
            mWeb.onNotFound (                     std::bind(&Web::showNotFound,   this, std::placeholders::_1));
            mWeb.on("/reboot",         HTTP_ANY,  std::bind(&Web::onReboot,       this, std::placeholders::_1));
            mWeb.on("/system",         HTTP_ANY,  std::bind(&Web::onSystem,       this, std::placeholders::_1));
            mWeb.on("/erase",          HTTP_ANY,  std::bind(&Web::showErase,      this, std::placeholders::_1));
            mWeb.on("/factory",        HTTP_ANY,  std::bind(&Web::showFactoryRst, this, std::placeholders::_1));

            mWeb.on("/setup",          HTTP_GET,  std::bind(&Web::onSetup,        this, std::placeholders::_1));
            mWeb.on("/save",           HTTP_POST, std::bind(&Web::showSave,       this, std::placeholders::_1));

            mWeb.on("/live",           HTTP_ANY,  std::bind(&Web::onLive,         this, std::placeholders::_1));
            //mWeb.on("/api1",           HTTP_POST, std::bind(&Web::showWebApi,     this, std::placeholders::_1));

        #ifdef ENABLE_PROMETHEUS_EP
            mWeb.on("/metrics",        HTTP_ANY,  std::bind(&Web::showMetrics,    this, std::placeholders::_1));
        #endif

            mWeb.on("/update",         HTTP_GET,  std::bind(&Web::onUpdate,       this, std::placeholders::_1));
            mWeb.on("/update",         HTTP_POST, std::bind(&Web::showUpdate,     this, std::placeholders::_1),
                                                  std::bind(&Web::showUpdate2,    this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
            mWeb.on("/upload",         HTTP_POST, std::bind(&Web::onUpload,       this, std::placeholders::_1),
                                                  std::bind(&Web::onUpload2,      this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
            mWeb.on("/serial",         HTTP_GET,  std::bind(&Web::onSerial,       this, std::placeholders::_1));
            mWeb.on("/about",          HTTP_GET,  std::bind(&Web::onAbout,        this, std::placeholders::_1));
            mWeb.on("/debug",          HTTP_GET,  std::bind(&Web::onDebug,        this, std::placeholders::_1));


            mEvts.onConnect(std::bind(&Web::onConnect, this, std::placeholders::_1));
            mWeb.addHandler(&mEvts);

            mWeb.begin();

            registerDebugCb(std::bind(&Web::serialCb, this, std::placeholders::_1)); // dbg.h

            mUploadFail = false;
        }

        void tickSecond() {
            if (0 != mLogoutTimeout) {
                mLogoutTimeout -= 1;
                if (0 == mLogoutTimeout) {
                    if (strlen(mConfig->sys.adminPwd) > 0)
                        mProtected = true;
                }

                DPRINTLN(DBG_DEBUG, "auto logout in " + String(mLogoutTimeout));
            }

            if (mSerialClientConnnected) {
                if (mSerialBufFill > 0) {
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

        bool isProtected(AsyncWebServerRequest *request) {
            bool prot;
            prot = mProtected;
            if(!prot) {
                if(strlen(mConfig->sys.adminPwd) > 0) {
                    uint8_t ip[4];
                    ah::ip2Arr(ip, request->client()->remoteIP().toString().c_str());
                    for(uint8_t i = 0; i < 4; i++) {
                        if(mLoginIp[i] != ip[i])
                            prot = true;
                    }
                }
            }

            return prot;
        }

        void showUpdate2(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (!index) {
                Serial.printf("Update Start: %s\n", filename.c_str());
                #ifndef ESP32
                Update.runAsync(true);
                #endif
                if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
                    Update.printError(Serial);
                }
            }
            if (!Update.hasError()) {
                if (Update.write(data, len) != len)
                    Update.printError(Serial);
            }
            if (final) {
                if (Update.end(true))
                    Serial.printf("Update Success: %uB\n", index + len);
                else
                    Update.printError(Serial);
            }
        }

        void onUpload2(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (!index) {
                mUploadFail = false;
                mUploadFp = LittleFS.open("/tmp.json", "w");
                if (!mUploadFp) {
                    DPRINTLN(DBG_ERROR, F("can't open file!"));
                    mUploadFail = true;
                    mUploadFp.close();
                    return;
                }
            }
            mUploadFp.write(data, len);
            if (final) {
                mUploadFp.close();
                #if !defined(ETHERNET)
                char pwd[PWD_LEN];
                strncpy(pwd, mConfig->sys.stationPwd, PWD_LEN); // backup WiFi PWD
                #endif
                if (!mApp->readSettings("/tmp.json")) {
                    mUploadFail = true;
                    DPRINTLN(DBG_ERROR, F("upload JSON error!"));
                } else {
                    LittleFS.remove("/tmp.json");
                    #if !defined(ETHERNET)
                    strncpy(mConfig->sys.stationPwd, pwd, PWD_LEN); // restore WiFi PWD
                    #endif
                    mApp->saveSettings(true);
                }
                if (!mUploadFail)
                    DPRINTLN(DBG_INFO, F("upload finished!"));
            }
        }

        void serialCb(String msg) {
            if (!mSerialClientConnnected)
                return;

            msg.replace("\r\n", "<rn>");
            if (mSerialAddTime) {
                if ((9 + mSerialBufFill) < WEB_SERIAL_BUF_SIZE) {
                    if (mApp->getTimestamp() > 0) {
                        strncpy(&mSerialBuf[mSerialBufFill], mApp->getTimeStr(mApp->getTimezoneOffset()).c_str(), 9);
                        mSerialBufFill += 9;
                    }
                } else {
                    mSerialBufFill = 0;
                    mEvts.send("webSerial, buffer overflow!<rn>", "serial", millis());
                    return;
                }
                mSerialAddTime = false;
            }

            if (msg.endsWith("<rn>"))
                mSerialAddTime = true;

            uint16_t length = msg.length();
            if ((length + mSerialBufFill) < WEB_SERIAL_BUF_SIZE) {
                strncpy(&mSerialBuf[mSerialBufFill], msg.c_str(), length);
                mSerialBufFill += length;
            } else {
                mSerialBufFill = 0;
                mEvts.send("webSerial, buffer overflow!<rn>", "serial", millis());
            }
        }

    private:
        inline void checkRedirect(AsyncWebServerRequest *request) {
            if ((mConfig->sys.protectionMask & PROT_MASK_INDEX) != PROT_MASK_INDEX)
                request->redirect(F("/index"));
            else if ((mConfig->sys.protectionMask & PROT_MASK_LIVE) != PROT_MASK_LIVE)
                request->redirect(F("/live"));
            else if ((mConfig->sys.protectionMask & PROT_MASK_SERIAL) != PROT_MASK_SERIAL)
                request->redirect(F("/serial"));
            else if ((mConfig->sys.protectionMask & PROT_MASK_SYSTEM) != PROT_MASK_SYSTEM)
                request->redirect(F("/system"));
            else
                request->redirect(F("/login"));
        }

        void checkProtection(AsyncWebServerRequest *request) {
            if(isProtected(request)) {
                checkRedirect(request);
                return;
            }
        }

        void getPage(AsyncWebServerRequest *request, uint8_t mask, const uint8_t *zippedHtml, uint32_t len) {
            if (CHECK_MASK(mConfig->sys.protectionMask, mask))
                checkProtection(request);

            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html; charset=UTF-8"), zippedHtml, len);
            response->addHeader(F("Content-Encoding"), "gzip");
            response->addHeader(F("content-type"), "text/html; charset=UTF-8");
            if(request->hasParam("v"))
                response->addHeader(F("Cache-Control"), F("max-age=604800"));
            request->send(response);
        }

        void onUpdate(AsyncWebServerRequest *request) {
            getPage(request, PROT_MASK_UPDATE, update_html, update_html_len);
        }

        void showUpdate(AsyncWebServerRequest *request) {
            bool reboot = (!Update.hasError());

            String html = F("<!doctype html><html><head><title>Update</title><meta http-equiv=\"refresh\" content=\"20; URL=/\"></head><body>Update: ");
            if (reboot)
                html += "success";
            else
                html += "failed";
            html += F("<br/><br/>rebooting ... auto reload after 20s</body></html>");

            AsyncWebServerResponse *response = request->beginResponse(200, F("text/html; charset=UTF-8"), html);
            response->addHeader("Connection", "close");
            request->send(response);
            mApp->setRebootFlag();
        }

        void onUpload(AsyncWebServerRequest *request) {
            bool reboot = !mUploadFail;

            String html = F("<!doctype html><html><head><title>Upload</title><meta http-equiv=\"refresh\" content=\"20; URL=/\"></head><body>Upload: ");
            if (reboot)
                html += "success";
            else
                html += "failed";
            html += F("<br/><br/>rebooting ... auto reload after 20s</body></html>");

            AsyncWebServerResponse *response = request->beginResponse(200, F("text/html; charset=UTF-8"), html);
            response->addHeader("Connection", "close");
            request->send(response);
            mApp->setRebootFlag();
        }

        void onConnect(AsyncEventSourceClient *client) {
            DPRINTLN(DBG_VERBOSE, "onConnect");

            mSerialClientConnnected = true;

            if (client->lastId())
                DPRINTLN(DBG_VERBOSE, "Client reconnected! Last message ID that it got is: " + String(client->lastId()));

            client->send("hello!", NULL, millis(), 1000);
        }

        void onIndex(AsyncWebServerRequest *request) {
            getPage(request, PROT_MASK_INDEX, index_html, index_html_len);
        }

        void onLogin(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("onLogin"));

            if (request->args() > 0) {
                if (String(request->arg("pwd")) == String(mConfig->sys.adminPwd)) {
                    mProtected = false;
                    ah::ip2Arr(mLoginIp, request->client()->remoteIP().toString().c_str());
                    request->redirect("/");
                }
            }

            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html; charset=UTF-8"), login_html, login_html_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        void onLogout(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("onLogout"));

            checkProtection(request);

            mProtected = true;

            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html; charset=UTF-8"), system_html, system_html_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        void onColor(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("onColor"));
            AsyncWebServerResponse *response;
            if (mConfig->sys.darkMode)
                response = request->beginResponse_P(200, F("text/css"), colorDark_css, colorDark_css_len);
            else
                response = request->beginResponse_P(200, F("text/css"), colorBright_css, colorBright_css_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            if(request->hasParam("v")) {
                response->addHeader(F("Cache-Control"), F("max-age=604800"));
            }
            request->send(response);
        }

        void onCss(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("onCss"));
            mLogoutTimeout = LOGOUT_TIMEOUT;
            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/css"), style_css, style_css_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            if(request->hasParam("v")) {
                response->addHeader(F("Cache-Control"), F("max-age=604800"));
            }
            request->send(response);
        }

        void onApiJs(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("onApiJs"));

            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/javascript"), api_js, api_js_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            if(request->hasParam("v"))
                response->addHeader(F("Cache-Control"), F("max-age=604800"));
            request->send(response);
        }

        void onFavicon(AsyncWebServerRequest *request) {
            static const char favicon_type[] PROGMEM = "image/x-icon";
            AsyncWebServerResponse *response = request->beginResponse_P(200, favicon_type, favicon_ico, favicon_ico_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        void showNotFound(AsyncWebServerRequest *request) {
            checkProtection(request);
            request->redirect("/setup");
        }

        void onReboot(AsyncWebServerRequest *request) {
            mApp->setRebootFlag();
            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html; charset=UTF-8"), system_html, system_html_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        void showErase(AsyncWebServerRequest *request) {
            checkProtection(request);

            DPRINTLN(DBG_VERBOSE, F("showErase"));
            mApp->eraseSettings(false);
            onReboot(request);
        }

        void showFactoryRst(AsyncWebServerRequest *request) {
            checkProtection(request);

            DPRINTLN(DBG_VERBOSE, F("showFactoryRst"));
            String content = "";
            int refresh = 3;
            if (request->args() > 0) {
                if (request->arg("reset").toInt() == 1) {
                    refresh = 10;
                    if (mApp->eraseSettings(true))
                        content = F("factory reset: success\n\nrebooting ... ");
                    else
                        content = F("factory reset: failed\n\nrebooting ... ");
                } else {
                    content = F("factory reset: aborted");
                    refresh = 3;
                }
            } else {
                content = F("<h1>Factory Reset</h1>"
                    "<p><a href=\"/factory?reset=1\">RESET</a><br/><br/><a href=\"/factory?reset=0\">CANCEL</a><br/></p>");
                refresh = 120;
            }
            request->send(200, F("text/html; charset=UTF-8"), F("<!doctype html><html><head><title>Factory Reset</title><meta http-equiv=\"refresh\" content=\"") + String(refresh) + F("; URL=/\"></head><body>") + content + F("</body></html>"));
            if (refresh == 10)
                onReboot(request);
        }

        void onSetup(AsyncWebServerRequest *request) {
            getPage(request, PROT_MASK_SETUP, setup_html, setup_html_len);
        }

        void showSave(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("showSave"));

            checkProtection(request);

            if (request->args() == 0)
                return;

            char buf[20] = {0};

            // general
            #if !defined(ETHERNET)
            if (request->arg("ssid") != "")
                request->arg("ssid").toCharArray(mConfig->sys.stationSsid, SSID_LEN);
            if (request->arg("pwd") != "{PWD}")
                request->arg("pwd").toCharArray(mConfig->sys.stationPwd, PWD_LEN);
            if (request->arg("ap_pwd") != "")
                request->arg("ap_pwd").toCharArray(mConfig->sys.apPwd, PWD_LEN);
            mConfig->sys.isHidden = (request->arg("hidd") == "on");
            #endif /* !defined(ETHERNET) */
            if (request->arg("device") != "")
                request->arg("device").toCharArray(mConfig->sys.deviceName, DEVNAME_LEN);
            mConfig->sys.darkMode = (request->arg("darkMode") == "on");
            mConfig->sys.schedReboot = (request->arg("schedReboot") == "on");

            // protection
            if (request->arg("adminpwd") != "{PWD}") {
                request->arg("adminpwd").toCharArray(mConfig->sys.adminPwd, PWD_LEN);
                mProtected = (strlen(mConfig->sys.adminPwd) > 0);
            }
            mConfig->sys.protectionMask = 0x0000;
            for (uint8_t i = 0; i < 6; i++) {
                if (request->arg("protMask" + String(i)) == "on")
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

            if (request->arg("invInterval") != "")
                mConfig->nrf.sendInterval = request->arg("invInterval").toInt();
            mConfig->inst.rstYieldMidNight = (request->arg("invRstMid") == "on");
            mConfig->inst.rstValsCommStop = (request->arg("invRstComStop") == "on");
            mConfig->inst.rstValsNotAvail = (request->arg("invRstNotAvail") == "on");
            mConfig->inst.startWithoutTime = (request->arg("strtWthtTm") == "on");
            mConfig->inst.rstMaxValsMidNight = (request->arg("invRstMaxMid") == "on");
            mConfig->inst.yieldEffiency = (request->arg("yldEff")).toFloat();


            // pinout
            uint8_t pin;
            for (uint8_t i = 0; i < 14; i++) {
                pin = request->arg(String(pinArgNames[i])).toInt();
                switch(i) {
                    case 0:  mConfig->nrf.pinCs    = ((pin != 0xff) ? pin : DEF_NRF_CS_PIN);  break;
                    case 1:  mConfig->nrf.pinCe    = ((pin != 0xff) ? pin : DEF_NRF_CE_PIN);  break;
                    case 2:  mConfig->nrf.pinIrq   = ((pin != 0xff) ? pin : DEF_NRF_IRQ_PIN); break;
                    case 3:  mConfig->nrf.pinSclk  = ((pin != 0xff) ? pin : DEF_NRF_SCLK_PIN); break;
                    case 4:  mConfig->nrf.pinMosi  = ((pin != 0xff) ? pin : DEF_NRF_MOSI_PIN); break;
                    case 5:  mConfig->nrf.pinMiso  = ((pin != 0xff) ? pin : DEF_NRF_MISO_PIN); break;
                    case 6:  mConfig->led.led0 = pin; break;
                    case 7:  mConfig->led.led1 = pin; break;
                    case 8:  mConfig->led.led_high_active = pin; break;  // this is not really a pin but a polarity, but handling it close to here makes sense
                    case 9:  mConfig->cmt.pinSclk  = pin; break;
                    case 10: mConfig->cmt.pinSdio  = pin; break;
                    case 11: mConfig->cmt.pinCsb   = pin; break;
                    case 12: mConfig->cmt.pinFcsb  = pin; break;
                    case 13: mConfig->cmt.pinIrq   = pin; break;
                }
            }

            mConfig->nrf.enabled = (request->arg("nrfEnable") == "on");

            // cmt
            mConfig->cmt.enabled = (request->arg("cmtEnable") == "on");

            // ntp
            if (request->arg("ntpAddr") != "") {
                request->arg("ntpAddr").toCharArray(mConfig->ntp.addr, NTP_ADDR_LEN);
                mConfig->ntp.port = request->arg("ntpPort").toInt() & 0xffff;
                mConfig->ntp.interval = request->arg("ntpIntvl").toInt() & 0xffff;
            }

            // sun
            if (request->arg("sunLat") == "" || (request->arg("sunLon") == "")) {
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
            if (request->arg("mqttAddr") != "") {
                String addr = request->arg("mqttAddr");
                addr.trim();
                addr.toCharArray(mConfig->mqtt.broker, MQTT_ADDR_LEN);
            } else
                mConfig->mqtt.broker[0] = '\0';
            request->arg("mqttClientId").toCharArray(mConfig->mqtt.clientId, MQTT_CLIENTID_LEN);
            request->arg("mqttUser").toCharArray(mConfig->mqtt.user, MQTT_USER_LEN);
            if (request->arg("mqttPwd") != "{PWD}")
                request->arg("mqttPwd").toCharArray(mConfig->mqtt.pwd, MQTT_PWD_LEN);
            request->arg("mqttTopic").toCharArray(mConfig->mqtt.topic, MQTT_TOPIC_LEN);
            mConfig->mqtt.port = request->arg("mqttPort").toInt();
            mConfig->mqtt.interval = request->arg("mqttInterval").toInt();

            // serial console
            if (request->arg("serIntvl") != "") {
                mConfig->serial.interval = request->arg("serIntvl").toInt() & 0xffff;

                mConfig->serial.debug = (request->arg("serDbg") == "on");
                mConfig->serial.showIv = (request->arg("serEn") == "on");
                // Needed to log TX buffers to serial console
                // mSys->Radio.mSerialDebug = mConfig->serial.debug;
            }

            // display
            mConfig->plugin.display.pwrSaveAtIvOffline = (request->arg("disp_pwr") == "on");
            mConfig->plugin.display.pxShift    = (request->arg("disp_pxshift") == "on");
            mConfig->plugin.display.rot        = request->arg("disp_rot").toInt();
            mConfig->plugin.display.type       = request->arg("disp_typ").toInt();
            mConfig->plugin.display.contrast   = (mConfig->plugin.display.type == 0) ? 60 : request->arg("disp_cont").toInt();
            mConfig->plugin.display.disp_data  = (mConfig->plugin.display.type == 0) ? DEF_PIN_OFF : request->arg("disp_data").toInt();
            mConfig->plugin.display.disp_clk   = (mConfig->plugin.display.type == 0) ? DEF_PIN_OFF : request->arg("disp_clk").toInt();
            mConfig->plugin.display.disp_cs    = (mConfig->plugin.display.type < 3)  ? DEF_PIN_OFF : request->arg("disp_cs").toInt();
            mConfig->plugin.display.disp_reset = (mConfig->plugin.display.type < 3)  ? DEF_PIN_OFF : request->arg("disp_rst").toInt();
            mConfig->plugin.display.disp_dc    = (mConfig->plugin.display.type < 3)  ? DEF_PIN_OFF : request->arg("disp_dc").toInt();
            mConfig->plugin.display.disp_busy  = (mConfig->plugin.display.type < 10) ? DEF_PIN_OFF : request->arg("disp_bsy").toInt();

            mApp->saveSettings((request->arg("reboot") == "on"));

            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html; charset=UTF-8"), save_html, save_html_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        void onLive(AsyncWebServerRequest *request) {
            getPage(request, PROT_MASK_LIVE, visualization_html, visualization_html_len);
        }

        void onAbout(AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html; charset=UTF-8"), about_html, about_html_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            response->addHeader(F("content-type"), "text/html; charset=UTF-8");
            if(request->hasParam("v")) {
                response->addHeader(F("Cache-Control"), F("max-age=604800"));
            }

            request->send(response);
        }

        void onDebug(AsyncWebServerRequest *request) {
            mApp->getSchedulerNames();
            AsyncWebServerResponse *response = request->beginResponse(200, F("text/html; charset=UTF-8"), "ok");
            request->send(response);
        }

        void onSerial(AsyncWebServerRequest *request) {
            getPage(request, PROT_MASK_SERIAL, serial_html, serial_html_len);
        }

        void onSystem(AsyncWebServerRequest *request) {
            getPage(request, PROT_MASK_SYSTEM, system_html, system_html_len);
        }


#ifdef ENABLE_PROMETHEUS_EP
        // Note
        // Prometheus exposition format is defined here: https://github.com/prometheus/docs/blob/main/content/docs/instrumenting/exposition_formats.md
        // TODO: Check packetsize for MAX_NUM_INVERTERS. Successfully Tested with 4 Inverters (each with 4 channels)
        enum {
            metricsStateStart,
            metricsStateInverter1, metricsStateInverter2, metricsStateInverter3, metricsStateInverter4,
            metricStateRealtimeFieldId, metricStateRealtimeInverterId,
            metricsStateAlarmData,
            metricsStateEnd
        } metricsStep;
        int metricsInverterId;
        uint8_t metricsFieldId;
        bool metricDeclared;

        void showMetrics(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("web::showMetrics"));
            metricsStep = metricsStateStart;
            AsyncWebServerResponse *response = request->beginChunkedResponse(F("text/plain"),
                                                                             [this](uint8_t *buffer, size_t maxLen, size_t filledLength) -> size_t
            {
                Inverter<> *iv;
                record_t<> *rec;
                statistics_t *stat;
                String promUnit, promType;
                String metrics;
                char type[60], topic[100], val[25];
                size_t len = 0;
                int alarmChannelId;
                int metricsChannelId;

                // Perform grouping on metrics according to format specification
                // Each step must return at least one character. Otherwise the processing of AsyncWebServerResponse stops.
                // So several "Info:" blocks are used to keep the transmission going
                switch (metricsStep) {
                    case metricsStateStart: // System Info & NRF Statistics : fit to one packet
                        snprintf(type,sizeof(type),"# TYPE ahoy_solar_info gauge\n");
                        snprintf(topic,sizeof(topic),"ahoy_solar_info{version=\"%s\",image=\"\",devicename=\"%s\"} 1\n",
                            mApp->getVersion(), mConfig->sys.deviceName);
                        metrics = String(type) + String(topic);

                        snprintf(type,sizeof(type),"# TYPE ahoy_solar_freeheap gauge\n");
                        snprintf(topic,sizeof(topic),"ahoy_solar_freeheap{devicename=\"%s\"} %u\n",mConfig->sys.deviceName,ESP.getFreeHeap());
                        metrics += String(type) + String(topic);

                        snprintf(type,sizeof(type),"# TYPE ahoy_solar_uptime counter\n");
                        snprintf(topic,sizeof(topic),"ahoy_solar_uptime{devicename=\"%s\"} %u\n", mConfig->sys.deviceName, mApp->getUptime());
                        metrics += String(type) + String(topic);

                        snprintf(type,sizeof(type),"# TYPE ahoy_solar_wifi_rssi_db gauge\n");
                        snprintf(topic,sizeof(topic),"ahoy_solar_wifi_rssi_db{devicename=\"%s\"} %d\n", mConfig->sys.deviceName, WiFi.RSSI());
                        metrics += String(type) + String(topic);

                        // NRF Statistics
                        // @TODO 2023-10-01: the statistic data is now available per inverter
                        /*stat = mApp->getNrfStatistics();
                        metrics += radioStatistic(F("rx_success"),     stat->rxSuccess);
                        metrics += radioStatistic(F("rx_fail"),        stat->rxFail);
                        metrics += radioStatistic(F("rx_fail_answer"), stat->rxFailNoAnser);
                        metrics += radioStatistic(F("frame_cnt"),      stat->frmCnt);
                        metrics += radioStatistic(F("tx_cnt"),         stat->txCnt);
                        metrics += radioStatistic(F("retrans_cnt"),    stat->retransmits);*/

                        len = snprintf((char *)buffer,maxLen,"%s",metrics.c_str());
                        // Next is Inverter information
                        metricsInverterId = 0;
                        metricsStep = metricsStateInverter1;
                        break;

                    case metricsStateInverter1: // Information about all inverters configured : fit to one packet
                        metrics = "# TYPE ahoy_solar_inverter_info gauge\n";
                        metrics += inverterMetric(topic, sizeof(topic),"ahoy_solar_inverter_info{name=\"%s\",serial=\"%12llx\"} 1\n",
                                    [](Inverter<> *iv,IApp *mApp)-> uint64_t {return iv->config->serial.u64;});
                        len = snprintf((char *)buffer,maxLen,"%s",metrics.c_str());
                        metricsStep = metricsStateInverter2;
                        break;

                    case metricsStateInverter2: // Information about all inverters configured : fit to one packet
                        metrics += "# TYPE ahoy_solar_inverter_is_enabled gauge\n";
                        metrics += inverterMetric(topic, sizeof(topic),"ahoy_solar_inverter_is_enabled {inverter=\"%s\"} %d\n",
                                    [](Inverter<> *iv,IApp *mApp)-> uint64_t {return iv->config->enabled;});

                        len = snprintf((char *)buffer,maxLen,"%s",metrics.c_str());
                        metricsStep = metricsStateInverter3;
                        break;

                    case metricsStateInverter3: // Information about all inverters configured : fit to one packet
                        metrics += "# TYPE ahoy_solar_inverter_is_available gauge\n";
                        metrics += inverterMetric(topic, sizeof(topic),"ahoy_solar_inverter_is_available {inverter=\"%s\"} %d\n",
                                    [](Inverter<> *iv,IApp *mApp)-> uint64_t {return iv->isAvailable();});
                        len = snprintf((char *)buffer,maxLen,"%s",metrics.c_str());
                        metricsStep = metricsStateInverter4;
                        break;

                    case metricsStateInverter4: // Information about all inverters configured : fit to one packet
                        metrics += "# TYPE ahoy_solar_inverter_is_producing gauge\n";
                        metrics += inverterMetric(topic, sizeof(topic),"ahoy_solar_inverter_is_producing {inverter=\"%s\"} %d\n",
                                    [](Inverter<> *iv,IApp *mApp)-> uint64_t {return iv->isProducing();});
                       len = snprintf((char *)buffer,maxLen,"%s",metrics.c_str());
                        // Start Realtime Field loop
                        metricsFieldId = FLD_UDC;
                        metricsStep = metricStateRealtimeFieldId;
                        break;

                    case metricStateRealtimeFieldId: // Iterate over all defined fields
                        if (metricsFieldId < FLD_LAST_ALARM_CODE) {
                            metrics = "# Info: processing realtime field #"+String(metricsFieldId)+"\n";
                            metricDeclared = false;

                            metricsInverterId = 0;
                            metricsStep = metricStateRealtimeInverterId;
                        } else {
                            metrics = "# Info: all realtime fields processed\n";
                            metricsStep = metricsStateAlarmData;
                        }
                        len = snprintf((char *)buffer,maxLen,"%s",metrics.c_str());
                        break;

                  case metricStateRealtimeInverterId: // Iterate over all inverters for this field
                        metrics = "";
                        if (metricsInverterId < mSys->getNumInverters()) {
                            // process all channels of this inverter

                            iv = mSys->getInverterByPos(metricsInverterId);
                            if (NULL != iv) {
                                rec = iv->getRecordStruct(RealTimeRunData_Debug);
                                for (metricsChannelId=0; metricsChannelId < rec->length;metricsChannelId++) {
                                    uint8_t channel = rec->assign[metricsChannelId].ch;

                                    // Try inverter channel (channel 0) or any channel with maxPwr > 0
                                    if (0 == channel || 0 != iv->config->chMaxPwr[channel-1]) {
                                        if (metricsFieldId == iv->getByteAssign(metricsChannelId, rec)->fieldId) {
                                            // This is the correct field to report
                                            std::tie(promUnit, promType) = convertToPromUnits(iv->getUnit(metricsChannelId, rec));
                                            // Declare metric only once
                                            if (channel != 0 && !metricDeclared) {
                                                snprintf(type, sizeof(type), "# TYPE ahoy_solar_%s%s %s\n", iv->getFieldName(metricsChannelId, rec), promUnit.c_str(), promType.c_str());
                                                metrics += type;
                                                metricDeclared = true;
                                            }
                                            // report value
                                            if (0 == channel) {
                                                char total[7];
                                                total[0] = 0;
                                                if (metricDeclared) {
                                                    // A declaration and value for channels has been delivered. So declare and deliver a _total metric
                                                    strncpy(total,"_total",sizeof(total));
                                                }
                                                snprintf(type, sizeof(type), "# TYPE ahoy_solar_%s%s%s %s\n", iv->getFieldName(metricsChannelId, rec), promUnit.c_str(), total, promType.c_str());
                                                metrics += type;
                                                snprintf(topic, sizeof(topic), "ahoy_solar_%s%s%s{inverter=\"%s\"}", iv->getFieldName(metricsChannelId, rec), promUnit.c_str(), total,iv->config->name);
                                            } else {
                                                // Use a fallback channel name (ch0, ch1, ...)if non is given by user
                                                char chName[MAX_NAME_LENGTH];
                                                if (iv->config->chName[channel-1][0] != 0) {
                                                    strncpy(chName, iv->config->chName[channel-1], sizeof(chName));
                                                } else {
                                                    snprintf(chName,sizeof(chName),"ch%1d",channel);
                                                }
                                                snprintf(topic, sizeof(topic), "ahoy_solar_%s%s{inverter=\"%s\",channel=\"%s\"}", iv->getFieldName(metricsChannelId, rec), promUnit.c_str(), iv->config->name,chName);
                                            }
                                            snprintf(val, sizeof(val), " %.3f\n", iv->getValue(metricsChannelId, rec));
                                            metrics += topic;
                                            metrics += val;
                                        }
                                    }
                                }
                                if (metrics.length() < 1) {
                                    metrics = "# Info: Field #"+String(metricsFieldId)+" not available for inverter #"+String(metricsInverterId)+". Skipping remaining inverters\n";
                                    metricsFieldId++; // Process next field Id
                                    metricsStep = metricStateRealtimeFieldId;
                                }
                            } else {
                                metrics = "# Info: No data for field #"+String(metricsFieldId)+" of inverter #"+String(metricsInverterId)+". Skipping remaining inverters\n";
                                metricsFieldId++; // Process next field Id
                                metricsStep = metricStateRealtimeFieldId;
                            }
                            // Stay in this state and try next inverter
                            metricsInverterId++;
                        } else {
                            metrics = "# Info: All inverters for field #"+String(metricsFieldId)+" processed.\n";
                            metricsFieldId++; // Process next field Id
                            metricsStep = metricStateRealtimeFieldId;
                        }
                        len = snprintf((char *)buffer,maxLen,"%s",metrics.c_str());
                        break;

                    case metricsStateAlarmData: // Alarm Info loop : fit to one packet
                        // Perform grouping on metrics according to Prometheus exposition format specification
                        snprintf(type, sizeof(type),"# TYPE ahoy_solar_%s gauge\n",fields[FLD_LAST_ALARM_CODE]);
                        metrics = type;

                        for (metricsInverterId = 0; metricsInverterId < mSys->getNumInverters();metricsInverterId++) {
                            iv = mSys->getInverterByPos(metricsInverterId);
                            if (NULL != iv) {
                                rec = iv->getRecordStruct(AlarmData);
                                // simple hack : there is only one channel with alarm data
                                // TODO: find the right one channel with the alarm id
                                alarmChannelId = 0;
                                if (alarmChannelId < rec->length) {
                                    std::tie(promUnit, promType) = convertToPromUnits(iv->getUnit(alarmChannelId, rec));
                                    snprintf(topic, sizeof(topic), "ahoy_solar_%s%s{inverter=\"%s\"}", iv->getFieldName(alarmChannelId, rec), promUnit.c_str(), iv->config->name);
                                    snprintf(val, sizeof(val), " %.3f\n", iv->getValue(alarmChannelId, rec));
                                    metrics += topic;
                                    metrics += val;
                                }
                            }
                        }
                        len = snprintf((char*)buffer,maxLen,"%s",metrics.c_str());
                        metricsStep = metricsStateEnd;
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


        // Traverse all inverters and collect the metric via valueFunc
        String inverterMetric(char *buffer, size_t len, const char *format, std::function<uint64_t(Inverter<> *iv, IApp *mApp)> valueFunc) {
            Inverter<> *iv;
            String metric = "";
            for (int metricsInverterId = 0; metricsInverterId < mSys->getNumInverters();metricsInverterId++) {
                iv = mSys->getInverterByPos(metricsInverterId);
                if (NULL != iv) {
                    snprintf(buffer,len,format,iv->config->name, valueFunc(iv,mApp));
                    metric += String(buffer);
                }
            }
            return metric;
        }

        String radioStatistic(String statistic, uint32_t value) {
            char type[60], topic[80], val[25];
            snprintf(type, sizeof(type), "# TYPE ahoy_solar_radio_%s counter",statistic.c_str());
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
        uint8_t mLoginIp[4];
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
