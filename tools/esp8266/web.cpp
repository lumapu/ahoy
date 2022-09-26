//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#if defined(ESP32) && defined(F)
  #undef F
  #define F(sl) (sl)
#endif

#include "web.h"

#include "html/h/index_html.h"
#include "html/h/style_css.h"
#include "html/h/api_js.h"
#include "html/h/favicon_ico_gz.h"
#include "html/h/setup_html.h"
#include "html/h/visualization_html.h"
#include "html/h/update_html.h"
#include "html/h/serial_html.h"

const char* const pinArgNames[] = {"pinCs", "pinCe", "pinIrq"};

//-----------------------------------------------------------------------------
web::web(app *main, sysConfig_t *sysCfg, config_t *config, statistics_t *stat, char version[]) {
    mMain    = main;
    mSysCfg  = sysCfg;
    mConfig  = config;
    mStat    = stat;
    mVersion = version;
    mWeb     = new AsyncWebServer(80);
    mEvts    = new AsyncEventSource("/events");
    mApi     = new webApi(mWeb, main, sysCfg, config, stat, version);

    memset(mSerialBuf, 0, WEB_SERIAL_BUF_SIZE);
    mSerialBufFill     = 0;
    mWebSerialTicker   = 0;
    mWebSerialInterval = 1000; // [ms]
    mSerialAddTime     = true;
}


//-----------------------------------------------------------------------------
void web::setup(void) {
    DPRINTLN(DBG_VERBOSE, F("app::setup-begin"));
    mWeb->begin();
    DPRINTLN(DBG_VERBOSE, F("app::setup-on"));
    mWeb->on("/",               HTTP_GET,  std::bind(&web::onIndex,        this, std::placeholders::_1));
    mWeb->on("/style.css",      HTTP_GET,  std::bind(&web::onCss,          this, std::placeholders::_1));
    mWeb->on("/api.js",         HTTP_GET,  std::bind(&web::onApiJs,        this, std::placeholders::_1));
    mWeb->on("/favicon.ico",    HTTP_GET,  std::bind(&web::onFavicon,      this, std::placeholders::_1));
    mWeb->onNotFound (                     std::bind(&web::showNotFound,   this, std::placeholders::_1));
    mWeb->on("/reboot",         HTTP_ANY,  std::bind(&web::onReboot,       this, std::placeholders::_1));
    mWeb->on("/erase",          HTTP_ANY,  std::bind(&web::showErase,      this, std::placeholders::_1));
    mWeb->on("/factory",        HTTP_ANY,  std::bind(&web::showFactoryRst, this, std::placeholders::_1));

    mWeb->on("/setup",          HTTP_GET,  std::bind(&web::onSetup,        this, std::placeholders::_1));
    mWeb->on("/save",           HTTP_ANY,  std::bind(&web::showSave,       this, std::placeholders::_1));

    mWeb->on("/live",           HTTP_ANY,  std::bind(&web::onLive,         this, std::placeholders::_1));
    mWeb->on("/api1",           HTTP_POST, std::bind(&web::showWebApi,     this, std::placeholders::_1));


    mWeb->on("/update",         HTTP_GET,  std::bind(&web::onUpdate,       this, std::placeholders::_1));
    mWeb->on("/update",         HTTP_POST, std::bind(&web::showUpdate,     this, std::placeholders::_1),
                                           std::bind(&web::showUpdate2,    this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
    mWeb->on("/serial",         HTTP_GET,  std::bind(&web::onSerial,       this, std::placeholders::_1));


    mEvts->onConnect(std::bind(&web::onConnect, this, std::placeholders::_1));
    mWeb->addHandler(mEvts);

    mApi->setup();

    registerDebugCb(std::bind(&web::serialCb, this, std::placeholders::_1));
}


//-----------------------------------------------------------------------------
void web::loop(void) {
    mApi->loop();

    if(mMain->checkTicker(&mWebSerialTicker, mWebSerialInterval)) {
        if(mSerialBufFill > 0) {
            mEvts->send(mSerialBuf, "serial", millis());
            memset(mSerialBuf, 0, WEB_SERIAL_BUF_SIZE);
            mSerialBufFill = 0;
        }
    }
}


//-----------------------------------------------------------------------------
void web::onConnect(AsyncEventSourceClient *client) {
    DPRINTLN(DBG_VERBOSE, "onConnect");

    if(client->lastId())
        DPRINTLN(DBG_VERBOSE, "Client reconnected! Last message ID that it got is: " + String(client->lastId()));

    client->send("hello!", NULL, millis(), 1000);
}


//-----------------------------------------------------------------------------
void web::onIndex(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("onIndex"));

    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), index_html, index_html_len);
    response->addHeader(F("Content-Encoding"), "gzip");
    request->send(response);
}


//-----------------------------------------------------------------------------
void web::onCss(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/css"), style_css, style_css_len);
    response->addHeader(F("Content-Encoding"), "gzip");
    request->send(response);
}



//-----------------------------------------------------------------------------
void web::onApiJs(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("onApiJs"));

    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/javascript"), api_js, api_js_len);
    response->addHeader(F("Content-Encoding"), "gzip");
    request->send(response);
}


//-----------------------------------------------------------------------------
void web::onFavicon(AsyncWebServerRequest *request) {
    static const char favicon_type[] PROGMEM = "image/x-icon";
    AsyncWebServerResponse *response = request->beginResponse_P(200, favicon_type, favicon_ico_gz, favicon_ico_gz_len);
    response->addHeader(F("Content-Encoding"), "gzip");
    request->send(response);
}


//-----------------------------------------------------------------------------
void web::showNotFound(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("showNotFound - ") + request->url());
    String msg = F("File Not Found\n\nURL: ");
    msg += request->url();
    msg += F("\nMethod: ");
    msg += ( request->method() == HTTP_GET ) ? "GET" : "POST";
    msg += F("\nArguments: ");
    msg += request->args();
    msg += "\n";

    for(uint8_t i = 0; i < request->args(); i++ ) {
        msg += " " + request->argName(i) + ": " + request->arg(i) + "\n";
    }

    request->send(404, F("text/plain"), msg);
}


//-----------------------------------------------------------------------------
void web::onReboot(AsyncWebServerRequest *request) {
    request->send(200, F("text/html"), F("<!doctype html><html><head><title>Rebooting ...</title><meta http-equiv=\"refresh\" content=\"10; URL=/\"></head><body>rebooting ... auto reload after 10s</body></html>"));
    mMain->mShouldReboot = true;
}


//-----------------------------------------------------------------------------
void web::showErase(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("showErase"));
    mMain->eraseSettings();
    onReboot(request);
}


//-----------------------------------------------------------------------------
void web::showFactoryRst(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("showFactoryRst"));
    String content = "";
    int refresh = 3;
    if(request->args() > 0) {
        if(request->arg("reset").toInt() == 1) {
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
    request->send(200, F("text/html"), F("<!doctype html><html><head><title>Factory Reset</title><meta http-equiv=\"refresh\" content=\"") + String(refresh) + F("; URL=/\"></head><body>") + content + F("</body></html>"));
    if(refresh == 10) {
        delay(1000);
        ESP.restart();
    }
}


//-----------------------------------------------------------------------------
void web::onSetup(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("onSetup"));

    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), setup_html, setup_html_len);
    response->addHeader(F("Content-Encoding"), "gzip");
    request->send(response);
}


//-----------------------------------------------------------------------------
void web::showSave(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("showSave"));

    if(request->args() > 0) {
        char buf[20] = {0};

        // general
        if(request->arg("ssid") != "")
            request->arg("ssid").toCharArray(mSysCfg->stationSsid, SSID_LEN);
        if(request->arg("pwd") != "{PWD}")
            request->arg("pwd").toCharArray(mSysCfg->stationPwd, PWD_LEN);
        if(request->arg("device") != "")
            request->arg("device").toCharArray(mSysCfg->deviceName, DEVNAME_LEN);

        // inverter
        Inverter<> *iv;
        for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
            iv = mMain->mSys->getInverterByPos(i, false);
            // address
            request->arg("inv" + String(i) + "Addr").toCharArray(buf, 20);
            if(strlen(buf) == 0)
                memset(buf, 0, 20);
            iv->serial.u64 = mMain->Serial2u64(buf);

            // active power limit
            uint16_t actPwrLimit = request->arg("inv" + String(i) + "ActivePowerLimit").toInt();
            uint16_t actPwrLimitControl = request->arg("inv" + String(i) + "PowerLimitControl").toInt();
            if(NoPowerLimit != actPwrLimitControl) {
                if (actPwrLimit != 0xffff && actPwrLimit > 0){
                    iv->powerLimit[0] = actPwrLimit;
                    iv->powerLimit[1] = actPwrLimitControl;
                    iv->devControlCmd = ActivePowerContr;
                    iv->devControlRequest = true;
                    if ((iv->powerLimit[1] & 0x0001) == 0x0001)
                        DPRINTLN(DBG_INFO, F("Power limit for inverter ") + String(iv->id) + F(" set to ") + String(iv->powerLimit[0]) + F("%") );
                    else {
                        DPRINTLN(DBG_INFO, F("Power limit for inverter ") + String(iv->id) + F(" set to ") + String(iv->powerLimit[0]) + F("W") );
                        DPRINTLN(DBG_INFO, F("Power Limit Control Setting ") + String(iv->powerLimit[1]));
                    }
                }
                if (actPwrLimit == 0xffff) { // set to 100%
                    iv->powerLimit[0] = 100;
                    iv->powerLimit[1] = RelativPersistent;
                    iv->devControlCmd = ActivePowerContr;
                    iv->devControlRequest = true;
                    DPRINTLN(DBG_INFO, F("Power limit for inverter ") + String(iv->id) + F(" set to unlimted"));
                }
            }
                
            // name
            request->arg("inv" + String(i) + "Name").toCharArray(iv->name, MAX_NAME_LENGTH);

            // max channel power / name
            for(uint8_t j = 0; j < 4; j++) {
                iv->chMaxPwr[j] = request->arg("inv" + String(i) + "ModPwr" + String(j)).toInt() & 0xffff;
                request->arg("inv" + String(i) + "ModName" + String(j)).toCharArray(iv->chName[j], MAX_NAME_LENGTH);
            }
            iv->initialized = true;
        }
        if(request->arg("invInterval") != "")
            mConfig->sendInterval = request->arg("invInterval").toInt();
        if(request->arg("invRetry") != "")
            mConfig->maxRetransPerPyld = request->arg("invRetry").toInt();

        // pinout
        uint8_t pin;
        for(uint8_t i = 0; i < 3; i ++) {
            pin = request->arg(String(pinArgNames[i])).toInt();
            switch(i) {
                default: mConfig->pinCs  = pin; break;
                case 1:  mConfig->pinCe  = pin; break;
                case 2:  mConfig->pinIrq = pin; break;
            }
        }

        // nrf24 amplifier power
        mConfig->amplifierPower = request->arg("rf24Power").toInt() & 0x03;

        // ntp
        if(request->arg("ntpAddr") != "") {
            request->arg("ntpAddr").toCharArray(mConfig->ntpAddr, NTP_ADDR_LEN);
            mConfig->ntpPort = request->arg("ntpPort").toInt() & 0xffff;
        }

        // mqtt
        if(request->arg("mqttAddr") != "") {
            String addr = request->arg("mqttAddr");
            addr.trim();
            addr.toCharArray(mConfig->mqtt.broker, MQTT_ADDR_LEN);
            request->arg("mqttUser").toCharArray(mConfig->mqtt.user, MQTT_USER_LEN);
            if(request->arg("mqttPwd") != "{PWD}")
                request->arg("mqttPwd").toCharArray(mConfig->mqtt.pwd, MQTT_PWD_LEN);
            request->arg("mqttTopic").toCharArray(mConfig->mqtt.topic, MQTT_TOPIC_LEN);
            mConfig->mqtt.port = request->arg("mqttPort").toInt();
        }

        // serial console
        if(request->arg("serIntvl") != "") {
            mConfig->serialInterval = request->arg("serIntvl").toInt() & 0xffff;

            mConfig->serialDebug  = (request->arg("serDbg") == "on");
            mConfig->serialShowIv = (request->arg("serEn") == "on");
            // Needed to log TX buffers to serial console
            mMain->mSys->Radio.mSerialDebug = mConfig->serialDebug;
        }

        mMain->saveValues();

        if(request->arg("reboot") == "on")
            onReboot(request);
        else
            request->send(200, F("text/html"), F("<!doctype html><html><head><title>Setup saved</title><meta http-equiv=\"refresh\" content=\"0; URL=/setup\"></head><body>"
                "<p>saved</p></body></html>"));
    }
}


//-----------------------------------------------------------------------------
void web::onLive(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("onLive"));

    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), visualization_html, visualization_html_len);
    response->addHeader(F("Content-Encoding"), "gzip");
    request->send(response);
}


//-----------------------------------------------------------------------------
void web::showWebApi(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("web::showWebApi"));
    DPRINTLN(DBG_DEBUG, request->arg("plain"));
    const size_t capacity = 200; // Use arduinojson.org/assistant to compute the capacity.
    DynamicJsonDocument response(capacity);

    // Parse JSON object
    deserializeJson(response, request->arg("plain"));
    // ToDo: error handling for payload
    uint8_t iv_id = response["inverter"];
    uint8_t cmd = response["cmd"];
    Inverter<> *iv = mMain->mSys->getInverterByPos(iv_id);
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
        }
    }
    request->send(200, "text/json", "{success:true}");
}


//-----------------------------------------------------------------------------
void web::onUpdate(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("onUpdate"));


    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), update_html, update_html_len);
    response->addHeader(F("Content-Encoding"), "gzip");
    request->send(response);
}


//-----------------------------------------------------------------------------
void web::showUpdate(AsyncWebServerRequest *request) {
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
    mMain->mShouldReboot = reboot;
}


//-----------------------------------------------------------------------------
void web::showUpdate2(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
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


//-----------------------------------------------------------------------------
void web::onSerial(AsyncWebServerRequest *request) {
    DPRINTLN(DBG_VERBOSE, F("onSerial"));

    AsyncWebServerResponse *response = request->beginResponse_P(200, F("text/html"), serial_html, serial_html_len);
    response->addHeader(F("Content-Encoding"), "gzip");
    request->send(response);
}


//-----------------------------------------------------------------------------
void web::serialCb(String msg) {
    msg.replace("\r\n", "<rn>");
    if(mSerialAddTime) {
        if((9 + mSerialBufFill) <= WEB_SERIAL_BUF_SIZE) {
            strncpy(&mSerialBuf[mSerialBufFill], mMain->getTimeStr().c_str(), 9);
            mSerialBufFill += 9;
        }
        else {
            mSerialBufFill = 0;
            mEvts->send("webSerial, buffer overflow!", "serial", millis());
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
        mEvts->send("webSerial, buffer overflow!", "serial", millis());
    }

}
