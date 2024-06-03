//-----------------------------------------------------------------------------
// 2024 Ahoy, https://www.mikrocontroller.net/topic/525778
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
#include "ESPAsyncWebServer.h"
#include "html/h/api_js.h"
#include "html/h/colorBright_css.h"
#include "html/h/colorDark_css.h"
#include "html/h/favicon_ico.h"
#include "html/h/grid_info_json.h"
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
#include "html/h/wizard_html.h"
#include "html/h/history_html.h"

#define WEB_SERIAL_BUF_SIZE 2048

const char* const pinArgNames[] = {
    "pinCs", "pinCe", "pinIrq", "pinSclk", "pinMosi", "pinMiso", "pinLed0",
    "pinLed1", "pinLed2", "pinLedHighActive", "pinLedLum", "pinCmtSclk",
    "pinSdio", "pinCsb", "pinFcsb", "pinGpio3"
    #if defined (ETHERNET)
    , "ethCs", "ethSclk", "ethMiso", "ethMosi", "ethIrq", "ethRst"
    #endif
};

template <class HMSYSTEM>
class Web {
   public:
        Web(void) : mWeb(80), mEvts("/events") {}

        void setup(IApp *app, HMSYSTEM *sys, settings_t *config) {
            mApp     = app;
            mSys     = sys;
            mConfig  = config;

            DPRINTLN(DBG_VERBOSE, F("app::setup-on"));
            mWeb.on("/",               HTTP_GET,  std::bind(&Web::onIndex,        this, std::placeholders::_1, true));
            mWeb.on("/index",          HTTP_GET,  std::bind(&Web::onIndex,        this, std::placeholders::_1, false));
            mWeb.on("/login",          HTTP_ANY,  std::bind(&Web::onLogin,        this, std::placeholders::_1));
            mWeb.on("/logout",         HTTP_GET,  std::bind(&Web::onLogout,       this, std::placeholders::_1));
            mWeb.on("/colors.css",     HTTP_GET,  std::bind(&Web::onColor,        this, std::placeholders::_1));
            mWeb.on("/style.css",      HTTP_GET,  std::bind(&Web::onCss,          this, std::placeholders::_1));
            mWeb.on("/api.js",         HTTP_GET,  std::bind(&Web::onApiJs,        this, std::placeholders::_1));
            mWeb.on("/grid_info.json", HTTP_GET,  std::bind(&Web::onGridInfoJson, this, std::placeholders::_1));
            mWeb.on("/favicon.ico",    HTTP_GET,  std::bind(&Web::onFavicon,      this, std::placeholders::_1));
            mWeb.onNotFound (                     std::bind(&Web::showNotFound,   this, std::placeholders::_1));
            mWeb.on("/reboot",         HTTP_ANY,  std::bind(&Web::onReboot,       this, std::placeholders::_1));
            mWeb.on("/system",         HTTP_ANY,  std::bind(&Web::onSystem,       this, std::placeholders::_1));
            mWeb.on("/erase",          HTTP_ANY,  std::bind(&Web::showHtml,       this, std::placeholders::_1));
            mWeb.on("/erasetrue",      HTTP_ANY,  std::bind(&Web::showHtml,       this, std::placeholders::_1));
            mWeb.on("/factory",        HTTP_ANY,  std::bind(&Web::showHtml,       this, std::placeholders::_1));
            mWeb.on("/factorytrue",    HTTP_ANY,  std::bind(&Web::showHtml,       this, std::placeholders::_1));

            mWeb.on("/setup",          HTTP_GET,  std::bind(&Web::onSetup,        this, std::placeholders::_1));
            mWeb.on("/wizard",         HTTP_GET,  std::bind(&Web::onWizard,       this, std::placeholders::_1));
            mWeb.on("/generate_204",   HTTP_GET,  std::bind(&Web::onWizard,       this, std::placeholders::_1));   //Android captive portal
            mWeb.on("/save",           HTTP_POST, std::bind(&Web::showSave,       this, std::placeholders::_1));

            mWeb.on("/live",           HTTP_ANY,  std::bind(&Web::onLive,         this, std::placeholders::_1));
            mWeb.on("/history",        HTTP_ANY, std::bind(&Web::onHistory,       this, std::placeholders::_1));

        #ifdef ENABLE_PROMETHEUS_EP
            mWeb.on("/metrics",        HTTP_ANY,  std::bind(&Web::showMetrics,    this, std::placeholders::_1));
        #endif

            mWeb.on("/update",         HTTP_POST, std::bind(&Web::showUpdate,     this, std::placeholders::_1),
                                                  std::bind(&Web::showUpdate2,    this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
            mWeb.on("/update",         HTTP_GET,  std::bind(&Web::onUpdate,       this, std::placeholders::_1));
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
            if (mSerialClientConnnected) {
                if(nullptr == mSerialBuf)
                    return;

                if (mSerialBufFill > 0) {
                    mEvts.send(mSerialBuf, "serial", millis());
                    memset(mSerialBuf, 0, WEB_SERIAL_BUF_SIZE);
                    mSerialBufFill = 0;
                }
            } else if(nullptr != mSerialBuf) {
                delete[] mSerialBuf;
                mSerialBuf = nullptr;
            }
        }

        AsyncWebServer *getWebSrvPtr(void) {
            return &mWeb;
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
                    for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                        if((mConfig->inst.iv[i].serial.u64 != 0) && (mConfig->inst.iv[i].serial.u64 < 138999999999)) { // hexadecimal
                            mConfig->inst.iv[i].serial.u64 = ah::Serial2u64(String(mConfig->inst.iv[i].serial.u64).c_str());
                        }
                    }
                    mApp->saveSettings(true);
                }
                if (!mUploadFail)
                    DPRINTLN(DBG_INFO, F("upload finished!"));
            }
        }

        void serialCb(String msg) {
            if (!mSerialClientConnnected)
                return;

            if(nullptr == mSerialBuf)
                return;

            msg.replace("\r\n", "<rn>");
            if (mSerialAddTime) {
                if ((13 + mSerialBufFill) < WEB_SERIAL_BUF_SIZE) {
                    if (mApp->getTimestamp() > 0) {
                        strncpy(&mSerialBuf[mSerialBufFill], ah::getTimeStrMs(mApp->getTimestampMs() + mApp->getTimezoneOffset() * 1000).c_str(), 12);
                        mSerialBuf[mSerialBufFill+12] = ' ';
                        mSerialBufFill += 13;
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
            else if ((mConfig->sys.protectionMask & PROT_MASK_HISTORY) != PROT_MASK_HISTORY)
                request->redirect(F("/history"));
            else if ((mConfig->sys.protectionMask & PROT_MASK_SERIAL) != PROT_MASK_SERIAL)
                request->redirect(F("/serial"));
            else if ((mConfig->sys.protectionMask & PROT_MASK_SYSTEM) != PROT_MASK_SYSTEM)
                request->redirect(F("/system"));
            else
                request->redirect(F("/login"));
        }

        void checkProtection(AsyncWebServerRequest *request) {
            if(mApp->isProtected(request->client()->remoteIP().toString().c_str(), "", true)) {
                checkRedirect(request);
                return;
            }
        }

        void getPage(AsyncWebServerRequest *request, uint16_t mask, const uint8_t *zippedHtml, uint32_t len) {
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
            #if defined(ETHERNET)
            // workaround for AsyncWebServer_ESP32_W5500, because it can't distinguish
            // between HTTP_GET and HTTP_POST if both are registered
            if(request->method() == HTTP_GET)
                onUpdate(request);
            #endif

            bool reboot = (!Update.hasError());

            String html = F("<!doctype html><html><head><title>Update</title><meta http-equiv=\"refresh\" content=\"");
            #if defined(ETHERNET) && defined(CONFIG_IDF_TARGET_ESP32S3)
                html += F("5");
            #else
                html += F("20");
            #endif
            html += F("; URL=/\"></head><body>Update: ");
            if (reboot)
                html += "success";
            else
                html += "failed";
            html += F("<br/><br/>rebooting ...</body></html>");

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

            if(nullptr == mSerialBuf) {
                mSerialBuf = new char[WEB_SERIAL_BUF_SIZE];
                memset(mSerialBuf, 0, WEB_SERIAL_BUF_SIZE);
            }
            mSerialClientConnnected = true;

            if (client->lastId())
                DPRINTLN(DBG_VERBOSE, "Client reconnected! Last message ID that it got is: " + String(client->lastId()));

            client->send("hello!", NULL, millis(), 1000);
        }

        void onIndex(AsyncWebServerRequest *request, bool checkAp = true) {
            if(mApp->isApActive() && checkAp) {
                onWizard(request);
                return;
            }
            getPage(request, PROT_MASK_INDEX, index_html, index_html_len);
        }

        void onLogin(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("onLogin"));

            if (request->args() > 0) {
                if (String(request->arg("pwd")) == String(mConfig->sys.adminPwd)) {
                    mApp->unlock(request->client()->remoteIP().toString().c_str(), true);
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
            mApp->lock(true);

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

        void onGridInfoJson(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("onGridInfoJson"));

            AsyncWebServerResponse *response = request->beginResponse_P(200, F("application/json; charset=utf-8"), grid_info_json, grid_info_json_len);
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
            //DBGPRINTLN(request->url());
            request->redirect("/wizard");
        }

        void onReboot(AsyncWebServerRequest *request) {
            mApp->setRebootFlag();
            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html; charset=UTF-8"), system_html, system_html_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        void showHtml(AsyncWebServerRequest *request) {
            checkProtection(request);

            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html; charset=UTF-8"), system_html, system_html_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        void onSetup(AsyncWebServerRequest *request) {
            getPage(request, PROT_MASK_SETUP, setup_html, setup_html_len);
        }

        void onWizard(AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html; charset=UTF-8"), wizard_html, wizard_html_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            response->addHeader(F("content-type"), "text/html; charset=UTF-8");
            request->send(response);
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
            mConfig->sys.isHidden = (request->arg("hidd") == "on");
            #endif /* !defined(ETHERNET) */
            if (request->arg("ap_pwd") != "")
                request->arg("ap_pwd").toCharArray(mConfig->sys.apPwd, PWD_LEN);
            if (request->arg("device") != "")
                request->arg("device").toCharArray(mConfig->sys.deviceName, DEVNAME_LEN);
            mConfig->sys.darkMode = (request->arg("darkMode") == "on");
            mConfig->sys.schedReboot = (request->arg("schedReboot") == "on");
            mConfig->sys.region = (request->arg("region")).toInt();
            mConfig->sys.timezone = (request->arg("timezone")).toInt() - 12;

            if (request->arg("cstLnk") != "") {
                request->arg("cstLnk").toCharArray(mConfig->plugin.customLink, MAX_CUSTOM_LINK_LEN);
                request->arg("cstLnkTxt").toCharArray(mConfig->plugin.customLinkText, MAX_CUSTOM_LINK_TEXT_LEN);
            } else {
                mConfig->plugin.customLink[0] = '\0';
                mConfig->plugin.customLinkText[0] = '\0';
            }

            // protection
            if (request->arg("adminpwd") != "{PWD}") {
                request->arg("adminpwd").toCharArray(mConfig->sys.adminPwd, PWD_LEN);
                mApp->lock(false);
            }
            mConfig->sys.protectionMask = 0x0000;
            for (uint8_t i = 0; i < 7; i++) {
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
                mConfig->inst.sendInterval = request->arg("invInterval").toInt();
            mConfig->inst.rstValsAtMidNight = (request->arg("invRstMid") == "on");
            mConfig->inst.rstValsCommStop = (request->arg("invRstComStop") == "on");
            mConfig->inst.rstValsCommStart = (request->arg("invRstComStart") == "on");
            mConfig->inst.rstValsNotAvail = (request->arg("invRstNotAvail") == "on");
            mConfig->inst.startWithoutTime = (request->arg("strtWthtTm") == "on");
            mConfig->inst.readGrid = (request->arg("rdGrid") == "on");
            mConfig->inst.rstIncludeMaxVals = (request->arg("invRstMaxMid") == "on");


            // pinout
            #if defined(ETHERNET)
            for (uint8_t i = 0; i < 22; i++)
            #else
            for (uint8_t i = 0; i < 16; i++)
            #endif
            {
                uint8_t pin = request->arg(String(pinArgNames[i])).toInt();
                switch(i) {
                    case 0:  mConfig->nrf.pinCs    = ((pin != 0xff) ? pin : DEF_NRF_CS_PIN);  break;
                    case 1:  mConfig->nrf.pinCe    = ((pin != 0xff) ? pin : DEF_NRF_CE_PIN);  break;
                    case 2:  mConfig->nrf.pinIrq   = ((pin != 0xff) ? pin : DEF_NRF_IRQ_PIN); break;
                    case 3:  mConfig->nrf.pinSclk  = ((pin != 0xff) ? pin : DEF_NRF_SCLK_PIN); break;
                    case 4:  mConfig->nrf.pinMosi  = ((pin != 0xff) ? pin : DEF_NRF_MOSI_PIN); break;
                    case 5:  mConfig->nrf.pinMiso  = ((pin != 0xff) ? pin : DEF_NRF_MISO_PIN); break;
                    case 6:  mConfig->led.led[0]   = pin; break;
                    case 7:  mConfig->led.led[1]   = pin; break;
                    case 8:  mConfig->led.led[2]   = pin; break;
                    case 9:  mConfig->led.high_active = pin; break;  // this is not really a pin but a polarity, but handling it close to here makes sense
                    case 10:  mConfig->led.luminance = pin; break;  // this is not really a pin but a polarity, but handling it close to here makes sense
                    case 11: mConfig->cmt.pinSclk  = pin; break;
                    case 12: mConfig->cmt.pinSdio  = pin; break;
                    case 13: mConfig->cmt.pinCsb   = pin; break;
                    case 14: mConfig->cmt.pinFcsb  = pin; break;
                    case 15: mConfig->cmt.pinIrq   = pin; break;

                    #if defined(ETHERNET)
                    case 16: mConfig->sys.eth.pinCs   = pin; break;
                    case 17: mConfig->sys.eth.pinSclk = pin; break;
                    case 18: mConfig->sys.eth.pinMiso = pin; break;
                    case 19: mConfig->sys.eth.pinMosi = pin; break;
                    case 20: mConfig->sys.eth.pinIrq  = pin; break;
                    case 21: mConfig->sys.eth.pinRst  = pin; break;
                    #endif
                }
            }

            mConfig->nrf.enabled = (request->arg("nrfEnable") == "on");
            mConfig->cmt.enabled = (request->arg("cmtEnable") == "on");
            #if defined(ETHERNET)
            mConfig->sys.eth.enabled = (request->arg("ethEn") == "on");
            #endif

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
                mConfig->sun.offsetSecMorning = 0;
                mConfig->sun.offsetSecEvening = 0;
            } else {
                mConfig->sun.lat = request->arg("sunLat").toFloat();
                mConfig->sun.lon = request->arg("sunLon").toFloat();
                mConfig->sun.offsetSecMorning = request->arg("sunOffsSr").toInt() * 60;
                mConfig->sun.offsetSecEvening = request->arg("sunOffsSs").toInt() * 60;
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
            mConfig->mqtt.json = (request->arg("mqttJson") == "on");
            mConfig->mqtt.port = request->arg("mqttPort").toInt();
            mConfig->mqtt.interval = request->arg("mqttInterval").toInt();
            mConfig->mqtt.enableRetain = (request->arg("retain") == "on");

            // serial console
            mConfig->serial.debug = (request->arg("serDbg") == "on");
            mConfig->serial.privacyLog = (request->arg("priv") == "on");
            mConfig->serial.printWholeTrace = (request->arg("wholeTrace") == "on");
            mConfig->serial.showIv = (request->arg("serEn") == "on");
            mConfig->serial.log2mqtt = (request->arg("log2mqtt") == "on");

            // display
            mConfig->plugin.display.pwrSaveAtIvOffline = (request->arg("disp_pwr") == "on");
            mConfig->plugin.display.graph_size  = request->arg("disp_graph_size").toInt();
            mConfig->plugin.display.rot         = request->arg("disp_rot").toInt();
            mConfig->plugin.display.type        = request->arg("disp_typ").toInt();
            mConfig->plugin.display.contrast    = (mConfig->plugin.display.type  == DISP_TYPE_T0_NONE) ||   // contrast available only according optionsMap in setup.html, otherwise default value
                                                  (mConfig->plugin.display.type  == DISP_TYPE_T10_EPAPER) ? 140 : request->arg("disp_cont").toInt();
            mConfig->plugin.display.screenSaver = ((mConfig->plugin.display.type == DISP_TYPE_T1_SSD1306_128X64) // screensaver available only according optionsMap in setup.html, otherwise default value
                                                || (mConfig->plugin.display.type == DISP_TYPE_T2_SH1106_128X64)
                                                || (mConfig->plugin.display.type == DISP_TYPE_T4_SSD1306_128X32)
                                                || (mConfig->plugin.display.type == DISP_TYPE_T5_SSD1306_64X48)
                                                || (mConfig->plugin.display.type == DISP_TYPE_T6_SSD1309_128X64)) ? request->arg("disp_screensaver").toInt() : 0;
            mConfig->plugin.display.graph_ratio = ((mConfig->plugin.display.type == DISP_TYPE_T1_SSD1306_128X64) // display graph available only according optionsMap in setup.html, otherwise has to be 0
                                                || (mConfig->plugin.display.type == DISP_TYPE_T2_SH1106_128X64)
                                                || (mConfig->plugin.display.type == DISP_TYPE_T3_PCD8544_84X48)
                                                || (mConfig->plugin.display.type == DISP_TYPE_T6_SSD1309_128X64)) ? request->arg("disp_graph_ratio").toInt() : 0;

                                                                                           // available pins according pinMap in setup.html, otherwise default value
            mConfig->plugin.display.disp_data   = (mConfig->plugin.display.type == DISP_TYPE_T0_NONE) ? DEF_PIN_OFF : request->arg("disp_data").toInt();
            mConfig->plugin.display.disp_clk    = (mConfig->plugin.display.type == DISP_TYPE_T0_NONE) ? DEF_PIN_OFF : request->arg("disp_clk").toInt();
            mConfig->plugin.display.disp_cs     = (mConfig->plugin.display.type != DISP_TYPE_T3_PCD8544_84X48)
                                               && (mConfig->plugin.display.type != DISP_TYPE_T10_EPAPER) ? DEF_PIN_OFF : request->arg("disp_cs").toInt();
            mConfig->plugin.display.disp_dc     = (mConfig->plugin.display.type != DISP_TYPE_T3_PCD8544_84X48)
                                               && (mConfig->plugin.display.type != DISP_TYPE_T10_EPAPER) ? DEF_PIN_OFF : request->arg("disp_dc").toInt();
            mConfig->plugin.display.disp_reset  = (mConfig->plugin.display.type != DISP_TYPE_T10_EPAPER) ? DEF_PIN_OFF : request->arg("disp_rst").toInt();
            mConfig->plugin.display.disp_busy   = (mConfig->plugin.display.type != DISP_TYPE_T10_EPAPER) ? DEF_PIN_OFF : request->arg("disp_bsy").toInt();
            mConfig->plugin.display.pirPin      = (mConfig->plugin.display.screenSaver != DISP_TYPE_T2_SH1106_128X64) ? DEF_PIN_OFF : request->arg("pir_pin").toInt(); // pir pin only for motion screensaver
                                                                                                                                              // otherweise default value

            mApp->saveSettings((request->arg("reboot") == "on"));

            AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html; charset=UTF-8"), save_html, save_html_len);
            response->addHeader(F("Content-Encoding"), "gzip");
            request->send(response);
        }

        void onLive(AsyncWebServerRequest *request) {
            getPage(request, PROT_MASK_LIVE, visualization_html, visualization_html_len);
        }

        void onHistory(AsyncWebServerRequest *request) {
            getPage(request, PROT_MASK_HISTORY, history_html, history_html_len);
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
        // NOTE: Grouping for fields with channels and totals is currently not working
        // TODO: Handle grouping and sorting for independant from channel number
        // NOTE: Check packetsize for MAX_NUM_INVERTERS. Successfully Tested with 4 Inverters (each with 4 channels)
        const char* metricConstPrefix = "ahoy_solar_";
        const char* metricConstInverterFormat = " {inverter=\"%s\"} %d\n";
        typedef enum {
            metricsStateInverterInfo=0,           metricsStateInverterEnabled=1,        metricsStateInverterAvailable=2,
            metricsStateInverterProducing=3,      metricsStateInverterPowerLimitRead=4, metricsStateInverterPowerLimitAck=5,
            metricsStateInverterMaxPower=6,       metricsStateInverterRxSuccess=7,      metricsStateInverterRxFail=8,
            metricsStateInverterRxFailAnswer=9,   metricsStateInverterFrameCnt=10,      metricsStateInverterTxCnt=11,
            metricsStateInverterRetransmits=12,   metricsStateInverterIvRxCnt=13,       metricsStateInverterIvTxCnt=14,
            metricsStateInverterDtuRxCnt=15,      metricsStateInverterDtuTxCnt=16,
            metricStateRealtimeFieldId=metricsStateInverterDtuTxCnt+1, // ensure that this state follows the last per_inverter state
            metricStateRealtimeInverterId,
            metricsStateAlarmData,
            metricsStateStart,
            metricsStateEnd
        } MetricStep_t;
        MetricStep_t metricsStep = metricsStateInverterInfo;
        typedef struct {
            const char *topic;
            const char *type;
            const char *format;
            const std::function<uint64_t(Inverter<> *iv)> valueFunc;
        } InverterMetric_t;
        InverterMetric_t inverterMetrics[17] = {
            { "info",                 "gauge",   " {name=\"%s\",serial=\"%12llx\"} 1\n", [](Inverter<> *iv)-> uint64_t {return iv->config->serial.u64;} },
            { "is_enabled",           "gauge",   metricConstInverterFormat, [](Inverter<> *iv)-> uint64_t {return iv->config->enabled;} },
            { "is_available",         "gauge",   metricConstInverterFormat, [](Inverter<> *iv)-> uint64_t {return iv->isAvailable();} },
            { "is_producing",         "gauge",   metricConstInverterFormat, [](Inverter<> *iv)-> uint64_t {return iv->isProducing();} },
            { "power_limit_read",     "gauge",   metricConstInverterFormat, [](Inverter<> *iv)-> uint64_t {return iv->actPowerLimit;} },
            { "power_limit_ack",      "gauge",   metricConstInverterFormat, [](Inverter<> *iv)-> uint64_t {return (iv->powerLimitAck)?1:0;} },
            { "max_power",            "gauge",   metricConstInverterFormat, [](Inverter<> *iv)-> uint64_t {return iv->getMaxPower();} },
            { "radio_rx_success",     "counter" ,metricConstInverterFormat, [](Inverter<> *iv)-> uint64_t {return iv->radioStatistics.rxSuccess;} },
            { "radio_rx_fail",        "counter" ,metricConstInverterFormat, [](Inverter<> *iv)-> uint64_t {return iv->radioStatistics.rxFail;} },
            { "radio_rx_fail_answer", "counter" ,metricConstInverterFormat, [](Inverter<> *iv)-> uint64_t {return iv->radioStatistics.rxFailNoAnswer;} },
            { "radio_frame_cnt",      "counter" ,metricConstInverterFormat, [](Inverter<> *iv)-> uint64_t {return iv->radioStatistics.frmCnt;} },
            { "radio_tx_cnt",         "counter" ,metricConstInverterFormat, [](Inverter<> *iv)-> uint64_t {return iv->radioStatistics.txCnt;} },
            { "radio_retransmits",    "counter" ,metricConstInverterFormat, [](Inverter<> *iv)-> uint64_t {return iv->radioStatistics.retransmits;} },
            { "radio_iv_loss_cnt",    "counter" ,metricConstInverterFormat, [](Inverter<> *iv)-> uint64_t {return iv->radioStatistics.ivLoss;} },
            { "radio_iv_sent_cnt",    "counter" ,metricConstInverterFormat, [](Inverter<> *iv)-> uint64_t {return iv->radioStatistics.ivSent;} },
            { "radio_dtu_loss_cnt",   "counter" ,metricConstInverterFormat, [](Inverter<> *iv)-> uint64_t {return iv->radioStatistics.dtuLoss;} },
            { "radio_dtu_sent_cnt",   "counter" ,metricConstInverterFormat, [](Inverter<> *iv)-> uint64_t {return iv->radioStatistics.dtuSent;} }
        };

        void showMetrics(AsyncWebServerRequest *request) {
            DPRINTLN(DBG_VERBOSE, F("web::showMetrics"));
            metricsStep = metricsStateStart;
            AsyncWebServerResponse *response = request->beginChunkedResponse(F("text/plain"),
                                                                             [this](uint8_t *buffer, size_t maxLen, size_t filledLength) -> size_t
            {
                Inverter<> *iv;
                record_t<> *rec;
                String promUnit, promType;
                String metrics;
                char type[60], topic[100], val[25];
                size_t len = 0;
                int alarmChannelId;

                // Perform grouping on metrics according to format specification
                // Each step must return at least one character. Otherwise the processing of AsyncWebServerResponse stops.
                // So several "Info:" blocks are used to keep the transmission going
                switch (metricsStep) {
                    case metricsStateStart: // System Info  : fit to one packet
                        snprintf(type,sizeof(type),"# TYPE %sinfo gauge\n",metricConstPrefix);
                        snprintf(topic,sizeof(topic),"%sinfo{version=\"%s\",image=\"\",devicename=\"%s\"} 1\n",metricConstPrefix,
                            mApp->getVersion(), mConfig->sys.deviceName);
                        metrics = String(type) + String(topic);

                        snprintf(type,sizeof(type),"# TYPE %sfreeheap gauge\n",metricConstPrefix);
                        snprintf(topic,sizeof(topic),"%sfreeheap{devicename=\"%s\"} %u\n",metricConstPrefix,mConfig->sys.deviceName,ESP.getFreeHeap());
                        metrics += String(type) + String(topic);

                        snprintf(type,sizeof(type),"# TYPE %suptime counter\n",metricConstPrefix);
                        snprintf(topic,sizeof(topic),"%suptime{devicename=\"%s\"} %u\n",metricConstPrefix, mConfig->sys.deviceName, mApp->getUptime());
                        metrics += String(type) + String(topic);

                        snprintf(type,sizeof(type),"# TYPE %swifi_rssi_db gauge\n",metricConstPrefix);
                        snprintf(topic,sizeof(topic),"%swifi_rssi_db{devicename=\"%s\"} %d\n",metricConstPrefix, mConfig->sys.deviceName, WiFi.RSSI());
                        metrics += String(type) + String(topic);

                        len = snprintf(reinterpret_cast<char*>(buffer), maxLen,"%s",metrics.c_str());
                        // Next is Inverter information
                        metricsStep = metricsStateInverterInfo;
                        break;

                    // Information about all inverters configured : each metric for all inverters must fit to one network packet
                    case metricsStateInverterInfo:
                    case metricsStateInverterEnabled:
                    case metricsStateInverterAvailable:
                    case metricsStateInverterProducing:
                    case metricsStateInverterPowerLimitRead:
                    case metricsStateInverterPowerLimitAck:
                    case metricsStateInverterMaxPower:
                    case metricsStateInverterRxSuccess:
                    case metricsStateInverterRxFail:
                    case metricsStateInverterRxFailAnswer:
                    case metricsStateInverterFrameCnt:
                    case metricsStateInverterTxCnt:
                    case metricsStateInverterRetransmits:
                    case metricsStateInverterIvRxCnt:
                    case metricsStateInverterIvTxCnt:
                    case metricsStateInverterDtuRxCnt:
                    case metricsStateInverterDtuTxCnt:
                        metrics = "# TYPE ahoy_solar_inverter_" + String(inverterMetrics[metricsStep].topic) + " " + String(inverterMetrics[metricsStep].type) + "\n";
                        metrics += inverterMetric(topic, sizeof(topic),
                                        (String("ahoy_solar_inverter_") + inverterMetrics[metricsStep].topic +
                                            inverterMetrics[metricsStep].format).c_str(),
                                            inverterMetrics[metricsStep].valueFunc);
                        len = snprintf(reinterpret_cast<char*>(buffer), maxLen, "%s", metrics.c_str());
                        // ugly hack to increment the enum
                        metricsStep = static_cast<MetricStep_t>( static_cast<int>(metricsStep) + 1);
                        // Prepare  Realtime Field loop, which may be startet next
                        metricsFieldId = FLD_UDC;
                        break;


                    case metricStateRealtimeFieldId: // Iterate over all defined fields
                        if (metricsFieldId < FLD_LAST_ALARM_CODE) {
                            metrics = "# Info: processing realtime field #"+String(metricsFieldId)+"\n";
                            metricDeclared = false;
                            metricTotalDeclard = false;

                            metricsInverterId = 0;
                            metricsStep = metricStateRealtimeInverterId;
                        } else {
                            metrics = "# Info: all realtime fields processed\n";
                            metricsStep = metricsStateAlarmData;
                        }
                        len = snprintf(reinterpret_cast<char *>(buffer), maxLen, "%s", metrics.c_str());
                        break;

                  case metricStateRealtimeInverterId: // Iterate over all inverters for this field
                        metrics = "";
                        if (metricsInverterId < mSys->getNumInverters()) {
                            // process all channels of this inverter
                            iv = mSys->getInverterByPos(metricsInverterId);
                            if (NULL != iv) {
                                rec = iv->getRecordStruct(RealTimeRunData_Debug);
                                for (int metricsChannelId=0; metricsChannelId < rec->length;metricsChannelId++) {
                                    uint8_t channel = rec->assign[metricsChannelId].ch;

                                    // Try inverter channel (channel 0) or any channel with maxPwr > 0
                                    if (0 == channel || 0 != iv->config->chMaxPwr[channel-1]) {
                                        if (metricsFieldId == iv->getByteAssign(metricsChannelId, rec)->fieldId) {
                                            // This is the correct field to report
                                            std::tie(promUnit, promType) = convertToPromUnits(iv->getUnit(metricsChannelId, rec));
                                            // Declare metric only once
                                            if (channel != 0 && !metricDeclared) {
                                                snprintf(type, sizeof(type), "# TYPE %s%s%s %s\n",metricConstPrefix, iv->getFieldName(metricsChannelId, rec), promUnit.c_str(), promType.c_str());
                                                metrics += type;
                                                metricDeclared = true;
                                            }
                                            // report value
                                            if (0 == channel) {
                                                // Report a _total value if also channel values were reported. Otherwise report without _total
                                                char total[7] = {0};
                                                if (metricDeclared) {
                                                    // A declaration and value for channels have been delivered. So declare and deliver a _total metric
                                                    snprintf(total, sizeof(total), "_total");
                                                }
                                                if (!metricTotalDeclard) {
                                                    snprintf(type, sizeof(type), "# TYPE %s%s%s%s %s\n",metricConstPrefix, iv->getFieldName(metricsChannelId, rec), promUnit.c_str(), total, promType.c_str());
                                                    metrics += type;
                                                    metricTotalDeclard = true;
                                                }
                                                snprintf(topic, sizeof(topic), "%s%s%s%s{inverter=\"%s\"}",metricConstPrefix, iv->getFieldName(metricsChannelId, rec), promUnit.c_str(), total,iv->config->name);
                                            } else {
                                                // Report (non zero) channel value
                                                // Use a fallback channel name (ch0, ch1, ...)if non is given by user
                                                char chName[MAX_NAME_LENGTH];
                                                if (iv->config->chName[channel-1][0] != 0) {
                                                    strncpy(chName, iv->config->chName[channel-1], sizeof(chName));
                                                } else {
                                                    snprintf(chName,sizeof(chName),"ch%1d",channel);
                                                }
                                                snprintf(topic, sizeof(topic), "%s%s%s{inverter=\"%s\",channel=\"%s\"}",metricConstPrefix, iv->getFieldName(metricsChannelId, rec), promUnit.c_str(), iv->config->name,chName);
                                            }
                                            snprintf(val, sizeof(val), " %.3f\n", iv->getValue(metricsChannelId, rec));
                                            metrics += topic;
                                            metrics += val;
                                        }
                                    }
                                }
                                if (metrics.length() < 1) {
                                    metrics = "# Info: Field #"+String(metricsFieldId)+" (" + fields[metricsFieldId] +
                                                ") not available for inverter #"+String(metricsInverterId)+". Skipping remaining inverters\n";
                                    metricsFieldId++; // Process next field Id
                                    metricsStep = metricStateRealtimeFieldId;
                                }
                            } else {
                                metrics = "# Info: No data for field #"+String(metricsFieldId)+ " (" + fields[metricsFieldId] +
                                          ") of inverter #"+String(metricsInverterId)+". Skipping remaining inverters\n";
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
                        len = snprintf(reinterpret_cast<char *>(buffer), maxLen, "%s", metrics.c_str());
                        break;

                    case metricsStateAlarmData: // Alarm Info loop : fit to one packet
                        // Perform grouping on metrics according to Prometheus exposition format specification
                        snprintf(type, sizeof(type),"# TYPE %s%s gauge\n",metricConstPrefix,fields[FLD_LAST_ALARM_CODE]);
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
                                    snprintf(topic, sizeof(topic), "%s%s%s{inverter=\"%s\"}",metricConstPrefix, iv->getFieldName(alarmChannelId, rec), promUnit.c_str(), iv->config->name);
                                    snprintf(val, sizeof(val), " %.3f\n", iv->getValue(alarmChannelId, rec));
                                    metrics += topic;
                                    metrics += val;
                                }
                            }
                        }
                        len = snprintf(reinterpret_cast<char*>(buffer), maxLen, "%s", metrics.c_str());
                        metricsStep = metricsStateEnd;
                        break;

                    default: // end of transmission
                        DBGPRINT("E: Prometheus: Bad metricsStep=");
                        DBGPRINTLN(String(metricsStep));
                    case metricsStateEnd:
                        len = 0;
                        break;
                } // switch
                return len;
            });
            request->send(response);
        }


        // Traverse all inverters and collect the metric via valueFunc
        String inverterMetric(char *buffer, size_t len, const char *format, std::function<uint64_t(Inverter<> *iv)> valueFunc) {
            String metric = "";
            for (int id = 0; id < mSys->getNumInverters();id++) {
                Inverter<> *iv = mSys->getInverterByPos(id);
                if (NULL != iv) {
                    snprintf(buffer,len,format,iv->config->name, valueFunc(iv));
                    metric += String(buffer);
                }
            }
            return metric;
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

    private:
        int metricsInverterId = 0;
        uint8_t metricsFieldId = 0;
        bool metricDeclared = false, metricTotalDeclard = false;
#endif
    private:
        AsyncWebServer mWeb;
        AsyncEventSource mEvts;
        IApp *mApp = nullptr;
        HMSYSTEM *mSys = nullptr;

        settings_t *mConfig = nullptr;

        bool mSerialAddTime = true;
        char *mSerialBuf = nullptr;
        uint16_t mSerialBufFill = 0;
        bool mSerialClientConnnected = false;

        File mUploadFp;
        bool mUploadFail = false;
};

#endif /*__WEB_H__*/
