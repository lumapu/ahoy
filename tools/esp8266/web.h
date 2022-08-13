//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __WEB_H__
#define __WEB_H__

#include "dbg.h"
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

#include "app.h"

#include "html/h/index_html.h"
#include "html/h/style_css.h"
#include "favicon.h"
#include "html/h/setup_html.h"


class web {
    public:
        web(app *main) {
            mMain = main;
            mWeb  = main->mWeb;
            //mWeb = new ESP8266WebServer(80);
            mUpdater = new ESP8266HTTPUpdateServer();
            mUpdater->setup(mWeb);
        }

        void setup(void) {
            mWeb->on("/",               std::bind(&web::showIndex,      this));
            mWeb->on("/style.css",      std::bind(&web::showCss,        this));
            mWeb->on("/favicon.ico",    std::bind(&web::showFavicon,    this));
            mWeb->onNotFound (          std::bind(&web::showNotFound,   this));
            mWeb->on("/uptime",         std::bind(&web::showUptime,     this));
            mWeb->on("/reboot",         std::bind(&web::showReboot,     this));
            mWeb->on("/erase",          std::bind(&web::showErase,      this));
            mWeb->on("/factory",        std::bind(&web::showFactoryRst, this));

            mWeb->on("/setup",          std::bind(&web::showSetup,      this));
            mWeb->on("/save",           std::bind(&web::showSave,       this));
        }

        void showIndex(void) {
            DPRINTLN(DBG_VERBOSE, F("showIndex"));
            String html = FPSTR(index_html);
            html.replace(F("{DEVICE}"), mMain->sysConfig.deviceName);
            html.replace(F("{VERSION}"), mMain->version);
            html.replace(F("{TS}"), String(mMain->config.sendInterval) + " ");
            html.replace(F("{JS_TS}"), String(mMain->config.sendInterval * 1000));
            html.replace(F("{BUILD}"), String(AUTO_GIT_HASH));
            mWeb->send(200, "text/html", html);
        }

        void showCss(void) {
            mWeb->send(200, "text/css", FPSTR(style_css));
        }

        void showFavicon(void) {
            static const char favicon_type[] PROGMEM = "image/x-icon";
            static const char favicon_content[] PROGMEM = FAVICON_PANEL_16;
            mWeb->send_P(200, favicon_type, favicon_content, sizeof(favicon_content));
        }

        void showNotFound(void) {
            DPRINTLN(DBG_VERBOSE, F("showNotFound - ") + mWeb->uri());
            String msg = F("File Not Found\n\nURI: ");
            msg += mWeb->uri();
            mWeb->send(404, F("text/plain"), msg);
        }

        void showUptime(void) {
            char time[21] = {0};
            uint32_t uptime = mMain->getUptime();

            uint32_t upTimeSc = uint32_t((uptime) % 60);
            uint32_t upTimeMn = uint32_t((uptime / (60)) % 60);
            uint32_t upTimeHr = uint32_t((uptime / (60 * 60)) % 24);
            uint32_t upTimeDy = uint32_t((uptime / (60 * 60 * 24)) % 365);

            snprintf(time, 20, "%d Days, %02d:%02d:%02d;", upTimeDy, upTimeHr, upTimeMn, upTimeSc);

            mWeb->send(200, "text/plain", String(time) + mMain->getDateTimeStr(mMain->getTimestamp()));
        }

        void showReboot(void) {
            mWeb->send(200, F("text/html"), F("<!doctype html><html><head><title>Rebooting ...</title><meta http-equiv=\"refresh\" content=\"10; URL=/\"></head><body>rebooting ... auto reload after 10s</body></html>"));
            delay(1000);
            ESP.restart();
        }

        void showErase() {
            DPRINTLN(DBG_VERBOSE, F("showErase"));
            mMain->eraseSettings();
            showReboot();
        }

        void showFactoryRst(void) {
            DPRINTLN(DBG_VERBOSE, F("showFactoryRst"));
            String content = "";
            int refresh = 3;
            if(mWeb->args() > 0) {
                if(mWeb->arg("reset").toInt() == 1) {
                    mMain->eraseSettings(true);
                    content = F("factory reset: success\n\nrebooting ... ");
                    refresh = 10;
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
            mWeb->send(200, F("text/html"), F("<!doctype html><html><head><title>Factory Reset</title><meta http-equiv=\"refresh\" content=\"") + String(refresh) + F("; URL=/\"></head><body>") + content + F("</body></html>"));
            if(refresh == 10) {
                delay(1000);
                ESP.restart();
            }
        }

        void showSetup(void) {
            DPRINTLN(DBG_VERBOSE, F("showSetup"));
            String html = FPSTR(setup_html);
            html.replace(F("{SSID}"), mMain->sysConfig.stationSsid);
            // PWD will be left at the default value (for protection)
            // -> the PWD will only be changed if it does not match the default "{PWD}"
            html.replace(F("{DEVICE}"), String(mMain->sysConfig.deviceName));
            html.replace(F("{VERSION}"), String(mMain->version));
            if(mMain->sysConfig.apActive)
                html.replace("{IP}", String(F("http://192.168.1.1")));
            else
                html.replace("{IP}", (F("http://") + String(WiFi.localIP().toString())));

            mWeb->send(200, F("text/html"), html);
        }

        void showSave(void) {
            DPRINTLN(DBG_VERBOSE, F("showSave"));

            if(mWeb->args() > 0) {
                uint32_t saveMask = 0;
                char buf[20] = {0};

                // general
                if(mWeb->arg("ssid") != "") {
                    mWeb->arg("ssid").toCharArray(mMain->sysConfig.stationSsid, SSID_LEN);
                    saveMask |= SAVE_SSID;
                }
                if(mWeb->arg("pwd") != "{PWD}") {
                    mWeb->arg("pwd").toCharArray(mMain->sysConfig.stationPwd, PWD_LEN);
                    saveMask |= SAVE_PWD;
                }
                if(mWeb->arg("device") != "") {
                    mWeb->arg("device").toCharArray(mMain->sysConfig.deviceName, DEVNAME_LEN);
                    saveMask |= SAVE_DEVICE_NAME;
                }

                // inverter
                Inverter<> *iv;
                for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
                    iv = mMain->mSys->getInverterByPos(i, false);
                    // address
                    mWeb->arg("inv" + String(i) + "Addr").toCharArray(buf, 20);
                    if(strlen(buf) == 0)
                        memset(buf, 0, 20);
                    else
                        saveMask |= SAVE_INVERTERS;
                    iv->serial.u64 = mMain->Serial2u64(buf);

                    // active power limit
                    uint16_t actPwrLimit = mWeb->arg("inv" + String(i) + "ActivePowerLimit").toInt();
                    if (actPwrLimit != 0xffff && actPwrLimit > 0)
                        iv->powerLimit[0] = actPwrLimit;

                    // name
                    mWeb->arg("inv" + String(i) + "Name").toCharArray(iv->name, MAX_NAME_LENGTH);

                    // max channel power / name
                    for(uint8_t j = 0; j < 4; j++) {
                        iv->chMaxPwr[j] = mWeb->arg("inv" + String(i) + "ModPwr" + String(j)).toInt() & 0xffff;
                        mWeb->arg("inv" + String(i) + "ModName" + String(j)).toCharArray(iv->chName[j], MAX_NAME_LENGTH);
                    }
                }
                if(mWeb->arg("invInterval") != "") {
                    mMain->config.sendInterval = mWeb->arg("invInterval").toInt();
                    saveMask |= SAVE_INV_SEND_INTERVAL;
                }
                if(mWeb->arg("invRetry") != "") {
                    mMain->config.sendInterval = mWeb->arg("invRetry").toInt();
                    saveMask |= SAVE_INV_RETRY;
                }

                // pinout
                uint8_t pin;
                for(uint8_t i = 0; i < 3; i ++) {
                    pin = mWeb->arg(String(pinArgNames[i])).toInt();
                    switch(i) {
                        default: mMain->mSys->Radio.pinCs  = pin; break;
                        case 1:  mMain->mSys->Radio.pinCe  = pin; break;
                        case 2:  mMain->mSys->Radio.pinIrq = pin; break;
                    }
                }
                saveMask |= SAVE_PINOUT;

                // nrf24 amplifier power
                mMain->mSys->Radio.AmplifierPower = mWeb->arg("rf24Power").toInt() & 0x03;
                saveMask |= SAVE_RF24;

                // ntp
                if(mWeb->arg("ntpAddr") != "") {
                    mWeb->arg("ntpAddr").toCharArray(mMain->config.ntpAddr, NTP_ADDR_LEN);
                    mMain->config.ntpPort = mWeb->arg("ntpPort").toInt() & 0xffff;
                    saveMask |= SAVE_NTP;
                }

                // mqtt
                if(mWeb->arg("mqttAddr") != "") {
                    mWeb->arg("mqttAddr").toCharArray(mMain->config.mqtt.broker, MQTT_ADDR_LEN);
                    mWeb->arg("mqttUser").toCharArray(mMain->config.mqtt.user, MQTT_USER_LEN);
                    mWeb->arg("mqttPwd").toCharArray(mMain->config.mqtt.pwd, MQTT_PWD_LEN);
                    mWeb->arg("mqttTopic").toCharArray(mMain->config.mqtt.topic, MQTT_TOPIC_LEN);
                    mMain->config.mqtt.port = mWeb->arg("mqttPort").toInt();
                    saveMask |= SAVE_MQTT;
                }

                // serial console
                if(mWeb->arg("serIntvl") != "") {
                    mMain->config.serialInterval = mWeb->arg("serIntvl").toInt() & 0xffff;

                    mMain->config.serialDebug  = (mWeb->arg("serEn") == "on");
                    mMain->config.serialShowIv = (mWeb->arg("serDbg") == "on");
                    saveMask |= SAVE_SERIAL;
                }


                mMain->saveValues(saveMask);

                if(mWeb->arg("reboot") == "on")
                    showReboot();
                else
                    mWeb->send(200, F("text/html"), F("<!doctype html><html><head><title>Setup saved</title><meta http-equiv=\"refresh\" content=\"0; URL=/setup\"></head><body>"
                        "<p>saved</p></body></html>"));
            }
        }




    private:
        ESP8266WebServer *mWeb;
        ESP8266HTTPUpdateServer *mUpdater;
        app *mMain;

};

#endif /*__WEB_H__*/
