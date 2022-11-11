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
#include "favicon.h"
#include "html/h/setup_html.h"
#include "html/h/visualization_html.h"


const uint16_t pwrLimitOptionValues[] {
    NoPowerLimit,
    AbsolutNonPersistent,
    AbsolutPersistent,
    RelativNonPersistent,
    RelativPersistent
};

const char* const pwrLimitOptions[] {
    "no power limit",
    "absolute in Watt non persistent",
    "absolute in Watt persistent",
    "relativ in percent non persistent",
    "relativ in percent persistent"
};

//-----------------------------------------------------------------------------
web::web(app *main, sysConfig_t *sysCfg, config_t *config, char version[]) {
    mMain    = main;
    mSysCfg  = sysCfg;
    mConfig  = config;
    mVersion = version;
    #ifdef ESP8266
        mWeb     = new ESP8266WebServer(80);
        mUpdater = new ESP8266HTTPUpdateServer();
    #elif defined(ESP32)
        mWeb     = new WebServer(80);
        mUpdater = new HTTPUpdateServer();
    #endif
    mUpdater->setup(mWeb);
}


//-----------------------------------------------------------------------------
void web::setup(void) {
    DPRINTLN(DBG_VERBOSE, F("app::setup-begin"));
    mWeb->begin();
        DPRINTLN(DBG_VERBOSE, F("app::setup-on"));
    mWeb->on("/",               std::bind(&web::showIndex,         this));
    mWeb->on("/style.css",      std::bind(&web::showCss,           this));
    mWeb->on("/favicon.ico",    std::bind(&web::showFavicon,       this));
    mWeb->onNotFound (          std::bind(&web::showNotFound,      this));
    mWeb->on("/uptime",         std::bind(&web::showUptime,        this));
    mWeb->on("/reboot",         std::bind(&web::showReboot,        this));
    mWeb->on("/erase",          std::bind(&web::showErase,         this));
    mWeb->on("/factory",        std::bind(&web::showFactoryRst,    this));

    mWeb->on("/setup",          std::bind(&web::showSetup,         this));
    mWeb->on("/save",           std::bind(&web::showSave,          this));

    mWeb->on("/cmdstat",        std::bind(&web::showStatistics,    this));
    mWeb->on("/visualization",  std::bind(&web::showVisualization, this));
    mWeb->on("/livedata",       std::bind(&web::showLiveData,      this));

#ifdef ENABLE_JSON_EP
    mWeb->on("/json",           std::bind(&web::showJson,          this));
#endif
#ifdef ENABLE_PROMETHEUS_EP
    mWeb->on("/metrics",        std::bind(&web::showMetrics,       this));
#endif

    mWeb->on("/api", HTTP_POST, std::bind(&web::showWebApi,        this));
}


//-----------------------------------------------------------------------------
void web::loop(void) {
    mWeb->handleClient();
}


//-----------------------------------------------------------------------------
void web::showIndex(void) {
    DPRINTLN(DBG_VERBOSE, F("showIndex"));
    String html = FPSTR(index_html);
    html.replace(F("{DEVICE}"), mSysCfg->deviceName);
    html.replace(F("{VERSION}"), mVersion);
    html.replace(F("{TS}"), String(mConfig->sendInterval) + " ");
    html.replace(F("{JS_TS}"), String(mConfig->sendInterval * 1000));
    html.replace(F("{BUILD}"), String(AUTO_GIT_HASH));
    mWeb->send(200, "text/html", html);
}


//-----------------------------------------------------------------------------
void web::showCss(void) {
    mWeb->send(200, "text/css", FPSTR(style_css));
}


//-----------------------------------------------------------------------------
void web::showFavicon(void) {
    static const char favicon_type[] PROGMEM = "image/x-icon";
    static const char favicon_content[] PROGMEM = FAVICON_PANEL_16;
    mWeb->send_P(200, favicon_type, favicon_content, sizeof(favicon_content));
}


//-----------------------------------------------------------------------------
void web::showNotFound(void) {
    DPRINTLN(DBG_VERBOSE, F("showNotFound - ") + mWeb->uri());
    String msg = F("File Not Found\n\nURI: ");
    msg += mWeb->uri();
    mWeb->send(404, F("text/plain"), msg);
}


//-----------------------------------------------------------------------------
void web::showUptime(void) {
    char time[21] = {0};
    uint32_t uptime = mMain->getUptime();

    uint32_t upTimeSc = uint32_t((uptime) % 60);
    uint32_t upTimeMn = uint32_t((uptime / (60)) % 60);
    uint32_t upTimeHr = uint32_t((uptime / (60 * 60)) % 24);
    uint32_t upTimeDy = uint32_t((uptime / (60 * 60 * 24)) % 365);

    snprintf(time, 20, "%d Days, %02d:%02d:%02d", upTimeDy, upTimeHr, upTimeMn, upTimeSc);

    mWeb->send(200, "text/plain", String(time) + "; now: " + mMain->getDateTimeStr(mMain->getTimestamp()));
}


//-----------------------------------------------------------------------------
void web::showReboot(void) {
    mWeb->send(200, F("text/html"), F("<!doctype html><html><head><title>Rebooting ...</title><meta http-equiv=\"refresh\" content=\"10; URL=/\"></head><body>rebooting ... auto reload after 10s</body></html>"));
    delay(1000);
    ESP.restart();
}


//-----------------------------------------------------------------------------
void web::showErase() {
    DPRINTLN(DBG_VERBOSE, F("showErase"));
    mMain->eraseSettings();
    showReboot();
}


//-----------------------------------------------------------------------------
void web::showFactoryRst(void) {
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


//-----------------------------------------------------------------------------
void web::showSetup(void) {
    DPRINTLN(DBG_VERBOSE, F("showSetup"));
    String html = FPSTR(setup_html);
    html.replace(F("{SSID}"), mSysCfg->stationSsid);
    // PWD will be left at the default value (for protection)
    // -> the PWD will only be changed if it does not match the default "{PWD}"
    html.replace(F("{DEVICE}"), String(mSysCfg->deviceName));
    html.replace(F("{VERSION}"), String(mVersion));
    if(mMain->getWifiApActive())
        html.replace("{IP}", String(F("http://192.168.1.1")));
    else
        html.replace("{IP}", (F("http://") + String(WiFi.localIP().toString())));

    String inv = "";
    Inverter<> *iv;
    for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
        iv = mMain->mSys->getInverterByPos(i);

        inv += F("<p class=\"subdes\">Inverter ") + String(i) + "</p>";
        inv += F("<label for=\"inv") + String(i) + F("Addr\">Address*</label>");
        inv += F("<input type=\"text\" class=\"text\" name=\"inv") + String(i) + F("Addr\" value=\"");
        if(NULL != iv)
            inv += String(iv->serial.u64, HEX);
        inv += F("\"/ maxlength=\"12\">");

        inv += F("<label for=\"inv") + String(i) + F("Name\">Name*</label>");
        inv += F("<input type=\"text\" class=\"text\" name=\"inv") + String(i) + F("Name\" value=\"");
        if(NULL != iv)
            inv += String(iv->name);
        inv += F("\"/ maxlength=\"") + String(MAX_NAME_LENGTH) + "\">";

        inv += F("<label for=\"inv") + String(i) + F("ActivePowerLimit\">Active Power Limit</label>");
        inv += F("<input type=\"text\" class=\"text\" name=\"inv") + String(i) + F("ActivePowerLimit\" value=\"");
        if(NULL != iv)
            inv += String(iv->powerLimit[0]);
        inv += F("\"/ maxlength=\"") + String(6) + "\">";

        inv += F("<label for=\"inv") + String(i) + F("ActivePowerLimitConType\">Active Power Limit Control Type</label>");
        inv += F("<select name=\"inv") + String(i) + F("PowerLimitControl\">");
        for(uint8_t j = 0; j < 5; j++) {
            inv += F("<option value=\"") + String(pwrLimitOptionValues[j]) + F("\"");
            if(NULL != iv) {
                if(iv->powerLimit[1] == pwrLimitOptionValues[j])
                    inv += F(" selected");
            }
            inv += F(">") + String(pwrLimitOptions[j]) + F("</option>");
        }
        inv += F("</select>");
        
        inv += F("<label for=\"inv") + String(i) + F("ModPwr0\" name=\"lbl") + String(i);
        inv += F("ModPwr\">Max Module Power (Wp)</label><div class=\"modpwr\">");
        for(uint8_t j = 0; j < 4; j++) {
            inv += F("<input type=\"text\" class=\"text sh\" name=\"inv") + String(i) + F("ModPwr") + String(j) + F("\" value=\"");
            if(NULL != iv)
                inv += String(iv->chMaxPwr[j]);
            inv += F("\"/ maxlength=\"4\">");
        }
        inv += F("</div><br/><label for=\"inv") + String(i) + F("ModName0\" name=\"lbl") + String(i);
        inv += F("ModName\">Module Name</label><div class=\"modname\">");
        for(uint8_t j = 0; j < 4; j++) {
            inv += F("<input type=\"text\" class=\"text sh\" name=\"inv") + String(i) + F("ModName") + String(j) + F("\" value=\"");
            if(NULL != iv)
                inv += String(iv->chName[j]);
            inv += F("\"/ maxlength=\"") + String(MAX_NAME_LENGTH) + "\">";
        }
        inv += F("</div>");
    }
    html.replace(F("{INVERTERS}"), String(inv));


    // pinout
    String pinout;
    for(uint8_t i = 0; i < 3; i++) {
        pinout += F("<label for=\"") + String(pinArgNames[i]) + "\">" + String(pinNames[i]) + F("</label>");
        pinout += F("<select name=\"") + String(pinArgNames[i]) + "\">";
        for(uint8_t j = 0; j <= 16; j++) {
            pinout += F("<option value=\"") + String(j) + "\"";
            switch(i) {
                default: if(j == mConfig->pinCs)  pinout += F(" selected"); break;
                case 1:  if(j == mConfig->pinCe)  pinout += F(" selected"); break;
                case 2:  if(j == mConfig->pinIrq) pinout += F(" selected"); break;
            }
            pinout += ">" + String(wemosPins[j]) + F("</option>");
        }
        pinout += F("</select>");
    }
    html.replace(F("{PINOUT}"), String(pinout));


    // nrf24l01+
    String rf24;
    for(uint8_t i = 0; i <= 3; i++) {
        rf24 += F("<option value=\"") + String(i) + "\"";
        if(i == mConfig->amplifierPower)
            rf24 += F(" selected");
        rf24 += ">" + String(rf24AmpPowerNames[i]) + F("</option>");
    }
    html.replace(F("{RF24}"), String(rf24));


    html.replace(F("{INV_INTVL}"), String(mConfig->sendInterval));
    html.replace(F("{INV_RETRIES}"), String(mConfig->maxRetransPerPyld));

    html.replace(F("{SER_INTVL}"), String(mConfig->serialInterval));
    html.replace(F("{SER_VAL_CB}"), (mConfig->serialShowIv) ? "checked" : "");
    html.replace(F("{SER_DBG_CB}"), (mConfig->serialDebug) ? "checked" : "");

    html.replace(F("{NTP_ADDR}"),  String(mConfig->ntpAddr));
    html.replace(F("{NTP_PORT}"),  String(mConfig->ntpPort));

    html.replace(F("{MQTT_ADDR}"),  String(mConfig->mqtt.broker));
    html.replace(F("{MQTT_PORT}"),  String(mConfig->mqtt.port));
    html.replace(F("{MQTT_USER}"),  String(mConfig->mqtt.user));
    html.replace(F("{MQTT_PWD}"),   String(mConfig->mqtt.pwd));
    html.replace(F("{MQTT_TOPIC}"), String(mConfig->mqtt.topic));
    
    mWeb->send(200, F("text/html"), html);
}


//-----------------------------------------------------------------------------
void web::showSave(void) {
    DPRINTLN(DBG_VERBOSE, F("showSave"));

    if(mWeb->args() > 0) {
        char buf[20] = {0};

        // general
        if(mWeb->arg("ssid") != "")
            mWeb->arg("ssid").toCharArray(mSysCfg->stationSsid, SSID_LEN);
        if(mWeb->arg("pwd") != "{PWD}")
            mWeb->arg("pwd").toCharArray(mSysCfg->stationPwd, PWD_LEN);
        if(mWeb->arg("device") != "")
            mWeb->arg("device").toCharArray(mSysCfg->deviceName, DEVNAME_LEN);

        // inverter
        Inverter<> *iv;
        for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
            iv = mMain->mSys->getInverterByPos(i, false);
            // address
            mWeb->arg("inv" + String(i) + "Addr").toCharArray(buf, 20);
            if(strlen(buf) == 0)
                memset(buf, 0, 20);
            iv->serial.u64 = mMain->Serial2u64(buf);

            // active power limit
            uint16_t actPwrLimit = mWeb->arg("inv" + String(i) + "ActivePowerLimit").toInt();
            uint16_t actPwrLimitControl = mWeb->arg("inv" + String(i) + "PowerLimitControl").toInt();
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
                
            // name
            mWeb->arg("inv" + String(i) + "Name").toCharArray(iv->name, MAX_NAME_LENGTH);

            // max channel power / name
            for(uint8_t j = 0; j < 4; j++) {
                iv->chMaxPwr[j] = mWeb->arg("inv" + String(i) + "ModPwr" + String(j)).toInt() & 0xffff;
                mWeb->arg("inv" + String(i) + "ModName" + String(j)).toCharArray(iv->chName[j], MAX_NAME_LENGTH);
            }
            iv->initialized = true;
        }
        if(mWeb->arg("invInterval") != "")
            mConfig->sendInterval = mWeb->arg("invInterval").toInt();
        if(mWeb->arg("invRetry") != "")
            mConfig->maxRetransPerPyld = mWeb->arg("invRetry").toInt();

        // pinout
        uint8_t pin;
        for(uint8_t i = 0; i < 3; i ++) {
            pin = mWeb->arg(String(pinArgNames[i])).toInt();
            switch(i) {
                default: mConfig->pinCs  = pin; break;
                case 1:  mConfig->pinCe  = pin; break;
                case 2:  mConfig->pinIrq = pin; break;
            }
        }

        // nrf24 amplifier power
        mConfig->amplifierPower = mWeb->arg("rf24Power").toInt() & 0x03;

        // ntp
        if(mWeb->arg("ntpAddr") != "") {
            mWeb->arg("ntpAddr").toCharArray(mConfig->ntpAddr, NTP_ADDR_LEN);
            mConfig->ntpPort = mWeb->arg("ntpPort").toInt() & 0xffff;
        }

        // mqtt
        if(mWeb->arg("mqttAddr") != "") {
            String addr = mWeb->arg("mqttAddr");
            addr.trim();
            addr.toCharArray(mConfig->mqtt.broker, MQTT_ADDR_LEN);
            mWeb->arg("mqttUser").toCharArray(mConfig->mqtt.user, MQTT_USER_LEN);
            mWeb->arg("mqttPwd").toCharArray(mConfig->mqtt.pwd, MQTT_PWD_LEN);
            mWeb->arg("mqttTopic").toCharArray(mConfig->mqtt.topic, MQTT_TOPIC_LEN);
            mConfig->mqtt.port = mWeb->arg("mqttPort").toInt();
        }

        // serial console
        if(mWeb->arg("serIntvl") != "") {
            mConfig->serialInterval = mWeb->arg("serIntvl").toInt() & 0xffff;

            mConfig->serialDebug  = (mWeb->arg("serDbg") == "on");
            mConfig->serialShowIv = (mWeb->arg("serEn") == "on");
            // Needed to log TX buffers to serial console
            mMain->mSys->Radio.mSerialDebug = mConfig->serialDebug;
        }

        mMain->saveValues();

        if(mWeb->arg("reboot") == "on")
            showReboot();
        else
            mWeb->send(200, F("text/html"), F("<!doctype html><html><head><title>Setup saved</title><meta http-equiv=\"refresh\" content=\"0; URL=/setup\"></head><body>"
                "<p>saved</p></body></html>"));
    }
}


//-----------------------------------------------------------------------------
void web::showStatistics(void) {
    DPRINTLN(DBG_VERBOSE, F("web::showStatistics"));
    mWeb->send(200, F("text/plain"), mMain->getStatistics());
}


//-----------------------------------------------------------------------------
void web::showVisualization(void) {
    DPRINTLN(DBG_VERBOSE, F("web::showVisualization"));
    String html = FPSTR(visualization_html);
    html.replace(F("{DEVICE}"), mSysCfg->deviceName);
    html.replace(F("{VERSION}"), mVersion);
    html.replace(F("{TS}"), String(mConfig->sendInterval) + " ");
    html.replace(F("{JS_TS}"), String(mConfig->sendInterval * 1000));
    mWeb->send(200, F("text/html"), html);
}


//-----------------------------------------------------------------------------
void web::showLiveData(void) {
    DPRINTLN(DBG_VERBOSE, F("web::showLiveData"));

    String modHtml, totalModHtml;
    float totalYield = 0, totalYieldToday = 0, totalActual = 0;
    uint8_t count = 0;

    for (uint8_t id = 0; id < mMain->mSys->getNumInverters(); id++) {
        count++;

        Inverter<> *iv = mMain->mSys->getInverterByPos(id);
        if (NULL != iv) {
#ifdef LIVEDATA_VISUALIZED
            uint8_t modNum, pos;
            switch (iv->type) {
                default:
                case INV_TYPE_1CH: modNum = 1; break;
                case INV_TYPE_2CH: modNum = 2; break;
                case INV_TYPE_4CH: modNum = 4; break;
            }

            modHtml += F("<div class=\"iv\">"
                         "<div class=\"ch-iv\"><span class=\"head\">")
                    + String(iv->name) + F(" Limit ")
                    + String(iv->actPowerLimit) + F("%");
            if(NoPowerLimit == iv->powerLimit[1])
                modHtml += F(" (not controlled)");
            modHtml += F(" | last Alarm: ") + iv->lastAlarmMsg + F("</span>");

            uint8_t list[] = {FLD_UAC, FLD_IAC, FLD_PAC, FLD_F, FLD_PCT, FLD_T, FLD_YT, FLD_YD, FLD_PDC, FLD_EFF, FLD_PRA, FLD_ALARM_MES_ID};

            for (uint8_t fld = 0; fld < 11; fld++) {
                pos = (iv->getPosByChFld(CH0, list[fld]));

                if(fld == 6){
                    totalYield += iv->getValue(pos);
                }

                if(fld == 7){
                    totalYieldToday += iv->getValue(pos);
                }

                if(fld == 2){
                    totalActual += iv->getValue(pos);
                }

                if (0xff != pos) {
                    modHtml += F("<div class=\"subgrp\">");
                    modHtml += F("<span class=\"value\">") + String(iv->getValue(pos));
                    modHtml += F("<span class=\"unit\">") + String(iv->getUnit(pos)) + F("</span></span>");
                    modHtml += F("<span class=\"info\">") + String(iv->getFieldName(pos)) + F("</span>");
                    modHtml += F("</div>");
                }
            }
            modHtml += "</div>";

            for (uint8_t ch = 1; ch <= modNum; ch++) {
                modHtml += F("<div class=\"ch\"><span class=\"head\">");
                if (iv->chName[ch - 1][0] == 0)
                    modHtml += F("CHANNEL ") + String(ch);
                else
                    modHtml += String(iv->chName[ch - 1]);
                modHtml += F("</span>");
                for (uint8_t j = 0; j < 6; j++) {
                    switch (j) {
                        default: pos = (iv->getPosByChFld(ch, FLD_UDC)); break;
                        case 1:  pos = (iv->getPosByChFld(ch, FLD_IDC)); break;
                        case 2:  pos = (iv->getPosByChFld(ch, FLD_PDC)); break;
                        case 3:  pos = (iv->getPosByChFld(ch, FLD_YD));  break;
                        case 4:  pos = (iv->getPosByChFld(ch, FLD_YT));  break;
                        case 5:  pos = (iv->getPosByChFld(ch, FLD_IRR)); break;
                    }
                    if (0xff != pos) {
                        modHtml += F("<span class=\"value\">") + String(iv->getValue(pos));
                        modHtml += F("<span class=\"unit\">") + String(iv->getUnit(pos)) + F("</span></span>");
                        modHtml += F("<span class=\"info\">") + String(iv->getFieldName(pos)) + F("</span>");
                    }
                }
                modHtml += "</div>";
                yield();
            }
            modHtml += F("<div class=\"ts\">Last received data requested at: ") + mMain->getDateTimeStr(iv->ts) + F("</div>");
            modHtml += F("</div>");
#else
            // dump all data to web frontend
            modHtml = F("<pre>");
            char topic[30], val[10];
            for (uint8_t i = 0; i < iv->listLen; i++) {
                snprintf(topic, 30, "%s/ch%d/%s", iv->name, iv->assign[i].ch, iv->getFieldName(i));
                snprintf(val, 10, "%.3f %s", iv->getValue(i), iv->getUnit(i));
                modHtml += String(topic) + ": " + String(val) + "\n";
            }
            modHtml += F("</pre>");
#endif
        }
    }

    if(count > 1){
        totalModHtml += F("<div class=\"iv\">"
                        "<div class=\"ch-all\"><span class=\"head\">Gesamt</span>");

        totalModHtml += F("<div class=\"subgrp\">");
        totalModHtml += F("<span class=\"value\">") + String(totalActual);
        totalModHtml += F("<span class=\"unit\">W</span></span>");
        totalModHtml += F("<span class=\"info\">P_AC All</span>");
        totalModHtml += F("</div>");

        totalModHtml += F("<div class=\"subgrp\">");
        totalModHtml += F("<span class=\"value\">") + String(totalYieldToday);
        totalModHtml += F("<span class=\"unit\">Wh</span></span>");
        totalModHtml += F("<span class=\"info\">YieldDayAll</span>");
        totalModHtml += F("</div>");

        totalModHtml += F("<div class=\"subgrp\">");
        totalModHtml += F("<span class=\"value\">") + String(totalYield);
        totalModHtml += F("<span class=\"unit\">kWh</span></span>");
        totalModHtml += F("<span class=\"info\">YieldTotalAll</span>");
        totalModHtml += F("</div>");

        totalModHtml += F("</div>");
        totalModHtml += F("</div>");
        mWeb->send(200, F("text/html"), totalModHtml + modHtml);
    } else {
        mWeb->send(200, F("text/html"), modHtml);
    }
}


//-----------------------------------------------------------------------------
#ifdef ENABLE_JSON_EP
void web::showJson(void) {
    DPRINTLN(DBG_VERBOSE, F("web::showJson"));

    String modJson;

    modJson = F("{\n");
    for(uint8_t id = 0; id < mMain->mSys->getNumInverters(); id++) {
        Inverter<> *iv = mMain->mSys->getInverterByPos(id);
        if(NULL != iv) {
            char topic[40], val[25];
            snprintf(topic, 30, "\"%s\": {\n", iv->name);
            modJson += String(topic);
            for(uint8_t i = 0; i < iv->listLen; i++) {
                snprintf(topic, 40, "\t\"ch%d/%s\"", iv->assign[i].ch, iv->getFieldName(i));
                snprintf(val, 25, "[%.3f, \"%s\"]", iv->getValue(i), iv->getUnit(i));
                modJson += String(topic) + ": " + String(val) + F(",\n");
            }
            modJson += F("\t\"last_msg\": \"") + mMain->getDateTimeStr(iv->ts) + F("\"\n\t},\n");
        }
    }
    modJson += F("\"json_ts\": \"") + String(mMain->getDateTimeStr(mMain->mTimestamp)) + F("\"\n}\n");

    mWeb->send(200, F("application/json"), modJson);
}
#endif

//-----------------------------------------------------------------------------
#ifdef ENABLE_PROMETHEUS_EP

std::pair<String, String> web::convertToPromUnits(String shortUnit) {

    if(shortUnit == "A")    return {"ampere", "gauge"};
    if(shortUnit == "V")    return {"volt", "gauge"};
    if(shortUnit == "%")    return {"ratio", "gauge"};
    if(shortUnit == "W")    return {"watt", "gauge"};
    if(shortUnit == "Wh")   return {"watt_daily", "counter"};
    if(shortUnit == "kWh")  return {"watt_total", "counter"};
    if(shortUnit == "°C")   return {"celsius", "gauge"};

    return {"", "gauge"};
}

void web::showMetrics(void) {
    DPRINTLN(DBG_VERBOSE, F("web::showMetrics"));

    String metrics;
    char headline[80];
    
    snprintf(headline, 80, "ahoy_solar_info{version=\"%s\",image=\"\",devicename=\"%s\"} 1", mVersion, mSysCfg->deviceName);
    metrics += "# TYPE ahoy_solar_info gauge\n" + String(headline) + "\n";

    for(uint8_t id = 0; id < mMain->mSys->getNumInverters(); id++) {
        Inverter<> *iv = mMain->mSys->getInverterByPos(id);
        if(NULL != iv) {
            char type[60], topic[60], val[25];
            for(uint8_t i = 0; i < iv->listLen; i++) {
                uint8_t channel = iv->assign[i].ch;
                if(channel == 0) {
                    String promUnit, promType;
                    std::tie(promUnit, promType) = convertToPromUnits( iv->getUnit(i) );
                    snprintf(type, 60, "# TYPE ahoy_solar_%s_%s %s", iv->getFieldName(i), promUnit.c_str(), promType.c_str());
                    snprintf(topic, 60, "ahoy_solar_%s_%s{inverter=\"%s\"}", iv->getFieldName(i), promUnit.c_str(), iv->name);
                    snprintf(val, 25, "%.3f", iv->getValue(i));
                    metrics += String(type) + "\n" + String(topic) + " " + String(val) + "\n";
                }
            }
        }
    }

    mWeb->send(200, F("text/plain"), metrics);
}
#endif

//-----------------------------------------------------------------------------
void web::showWebApi(void)
{
    DPRINTLN(DBG_VERBOSE, F("web::showWebApi"));
    DPRINTLN(DBG_DEBUG, mWeb->arg("plain"));
    const size_t capacity = 200; // Use arduinojson.org/assistant to compute the capacity.
    DynamicJsonDocument response(capacity);

    // Parse JSON object
    deserializeJson(response, mWeb->arg("plain"));
    // ToDo: error handling for payload
    uint8_t iv_id = response["inverter"];
    uint8_t cmd = response["cmd"];
    Inverter<> *iv = mMain->mSys->getInverterByPos(iv_id);
    if (NULL != iv)
    {
        if (response["tx_request"] == (uint8_t)TX_REQ_INFO)
        {
            // if the AlarmData is requested set the Alarm Index to the requested one
            if (cmd == AlarmData || cmd == AlarmUpdate){
                // set the AlarmMesIndex for the request from user input
                iv->alarmMesIndex = response["payload"]; 
            }
            DPRINTLN(DBG_INFO, F("Will make tx-request 0x15 with subcmd ") + String(cmd) + F(" and payload ") + String((uint16_t) response["payload"]));
            // process payload from web request corresponding to the cmd
            iv->enqueCommand<InfoCommand>(cmd);
        }
        

        if (response["tx_request"] == (uint8_t)TX_REQ_DEVCONTROL)
        {
            if (response["cmd"] == (uint8_t)ActivePowerContr)
            {
                uint16_t webapiPayload = response["payload"];
                uint16_t webapiPayload2 = response["payload2"];
                if (webapiPayload > 0 && webapiPayload < 10000)
                {
                    iv->devControlCmd = ActivePowerContr;
                    iv->powerLimit[0] = webapiPayload;
                    if (webapiPayload2 > 0)
                    {
                        iv->powerLimit[1] = webapiPayload2; // dev option, no sanity check
                    }
                    else
                    {                                             // if not set, set it to 0x0000 default
                        iv->powerLimit[1] = AbsolutNonPersistent; // payload will be seted temporay in Watt absolut
                    }
                    if (iv->powerLimit[1] & 0x0001)
                    {
                        DPRINTLN(DBG_INFO, F("Power limit for inverter ") + String(iv->id) + F(" set to ") + String(iv->powerLimit[0]) + F("% via REST API"));
                    }
                    else
                    {
                        DPRINTLN(DBG_INFO, F("Power limit for inverter ") + String(iv->id) + F(" set to ") + String(iv->powerLimit[0]) + F("W via REST API"));
                    }
                    iv->devControlRequest = true; // queue it in the request loop
                }
            }
            if (response["cmd"] == (uint8_t)TurnOff){
                iv->devControlCmd = TurnOff;
                iv->devControlRequest = true; // queue it in the request loop
            }
            if (response["cmd"] == (uint8_t)TurnOn){
                iv->devControlCmd = TurnOn;
                iv->devControlRequest = true; // queue it in the request loop
            }
        }
    }
    mWeb->send(200, "text/json", "{success:true}");
}
