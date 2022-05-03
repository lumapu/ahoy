#include "app.h"

#include "html/h/index_html.h"
#include "html/h/setup_html.h"
#include "html/h/hoymiles_html.h"


//-----------------------------------------------------------------------------
app::app() : Main() {
    mSendTicker     = 0xffffffff;
    mSendInterval   = 0;
    mMqttTicker     = 0xffffffff;
    mMqttInterval   = 0;
    mSerialTicker   = 0xffffffff;
    mSerialInterval = 0;
    mMqttActive     = false;

    mShowRebootRequest = false;


    memset(mCmds, 0, sizeof(uint32_t)*DBG_CMD_LIST_LEN);
    //memset(mChannelStat, 0, sizeof(uint32_t) * 4);

    mSys = new HmSystemType();
}


//-----------------------------------------------------------------------------
app::~app(void) {

}


//-----------------------------------------------------------------------------
void app::setup(const char *ssid, const char *pwd, uint32_t timeout) {
    Main::setup(ssid, pwd, timeout);

    mWeb->on("/",          std::bind(&app::showIndex,      this));
    mWeb->on("/setup",     std::bind(&app::showSetup,      this));
    mWeb->on("/save",      std::bind(&app::showSave,       this));
    mWeb->on("/erase",     std::bind(&app::showErase,      this));
    mWeb->on("/cmdstat",   std::bind(&app::showStatistics, this));
    mWeb->on("/hoymiles",  std::bind(&app::showHoymiles,   this));
    mWeb->on("/livedata",  std::bind(&app::showLiveData,   this));

    if(mSettingsValid) {
        uint64_t invSerial;
        char invName[MAX_NAME_LENGTH + 1] = {0};
        uint8_t invType;

        // inverter
        for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
            mEep->read(ADDR_INV_ADDR + (i * 8),               &invSerial);
            mEep->read(ADDR_INV_NAME + (i * MAX_NAME_LENGTH), invName, MAX_NAME_LENGTH);
            mEep->read(ADDR_INV_TYPE + i,                     &invType);
            if(0ULL != invSerial) {
                mSys->addInverter(invName, invSerial, invType);
                DPRINTLN("add inverter: " + String(invName) + ", SN: " + String(invSerial, HEX) + ", type: " + String(invType));
            }
        }

        mEep->read(ADDR_INV_INTERVAL, &mSendInterval);
        if(mSendInterval < 1000)
            mSendInterval = 1000;
        mSendTicker = 0;


        // pinout
        mEep->read(ADDR_PINOUT,   &mSys->Radio.pinCs);
        mEep->read(ADDR_PINOUT+1, &mSys->Radio.pinCe);
        mEep->read(ADDR_PINOUT+2, &mSys->Radio.pinIrq);


        // nrf24 amplifier power
        mEep->read(ADDR_RF24_AMP_PWR, &mSys->Radio.AmplifierPower);

        // mqtt
        uint8_t mqttAddr[MQTT_ADDR_LEN];
        uint16_t mqttPort;
        char mqttUser[MQTT_USER_LEN];
        char mqttPwd[MQTT_PWD_LEN];
        char mqttTopic[MQTT_TOPIC_LEN];
        mEep->read(ADDR_MQTT_ADDR,     mqttAddr,  MQTT_ADDR_LEN);
        mEep->read(ADDR_MQTT_USER,     mqttUser,  MQTT_USER_LEN);
        mEep->read(ADDR_MQTT_PWD,      mqttPwd,   MQTT_PWD_LEN);
        mEep->read(ADDR_MQTT_TOPIC,    mqttTopic, MQTT_TOPIC_LEN);
        mEep->read(ADDR_MQTT_INTERVAL, &mMqttInterval);
        mEep->read(ADDR_MQTT_PORT,     &mqttPort);

        char addr[16] = {0};
        sprintf(addr, "%d.%d.%d.%d", mqttAddr[0], mqttAddr[1], mqttAddr[2], mqttAddr[3]);
        mMqttActive = (mqttAddr[0] > 0);


        if(mMqttInterval < 1000)
            mMqttInterval = 1000;
        mMqtt.setup(addr, mqttTopic, mqttUser, mqttPwd, mqttPort);
        mMqttTicker = 0;

        mSerialTicker = 0;
        mSerialInterval = mMqttInterval; // TODO: add extra setting for this!

        mMqtt.sendMsg("version", mVersion);
    }

    mSys->setup();

    if(!mWifiSettingsValid)
        DPRINTLN("Warn: your settings are not valid! check [IP]/setup");
    else {
        DPRINTLN("\n\n----------------------------------------");
        DPRINTLN("Welcome to AHOY!");
        DPRINT("\npoint your browser to http://");
        DPRINTLN(WiFi.localIP());
        DPRINTLN("to configure your device");
        DPRINTLN("----------------------------------------\n");
    }
}


//-----------------------------------------------------------------------------
void app::loop(void) {
    Main::loop();

    if(!mSys->BufCtrl.empty()) {
        uint8_t len, rptCnt;
        packet_t *p = mSys->BufCtrl.getBack();
        //mSys->Radio.dumpBuf("RAW ", p->packet, MAX_RF_PAYLOAD_SIZE);

        if(mSys->Radio.checkCrc(p->packet, &len, &rptCnt)) {
            // process buffer only on first occurrence
            if((0 != len) && (0 == rptCnt)) {
                uint8_t *cmd = &p->packet[11];
                //DPRINTLN("CMD " + String(*cmd, HEX));
                //mSys->Radio.dumpBuf("Payload ", p->packet, len);

                Inverter<> *iv = mSys->findInverter(&p->packet[3]);
                if(NULL != iv) {
                    for(uint8_t i = 0; i < iv->listLen; i++) {
                        if(iv->assign[i].cmdId == *cmd)
                            iv->addValue(i, &p->packet[11]);
                    }
                    iv->doCalculations();
                }

                if(*cmd == 0x01)      mCmds[0]++;
                else if(*cmd == 0x02) mCmds[1]++;
                else if(*cmd == 0x03) mCmds[2]++;
                else if(*cmd == 0x81) mCmds[3]++;
                else if(*cmd == 0x82) mCmds[4]++;
                else if(*cmd == 0x83) mCmds[5]++;
                else if(*cmd == 0x84) mCmds[6]++;
                else                  mCmds[7]++;

                /*if(p->sendCh == 23)      mChannelStat[0]++;
                else if(p->sendCh == 40) mChannelStat[1]++;
                else if(p->sendCh == 61) mChannelStat[2]++;
                else                     mChannelStat[3]++;*/
            }
        }
        mSys->BufCtrl.popBack();
    }

    if(checkTicker(&mSendTicker, mSendInterval)) {
        Inverter<> *inv;
        for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
            inv = mSys->getInverterByPos(i);
            if(NULL != inv) {
                mSys->Radio.sendTimePacket(inv->radioId.u64, mTimestamp);
                yield();
            }
        }
    }


    // mqtt
    if(mMqttActive) {
        mMqtt.loop();
        if(checkTicker(&mMqttTicker, mMqttInterval)) {
            mMqtt.isConnected(true);
            char topic[30], val[10];
            for(uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                Inverter<> *iv = mSys->getInverterByPos(id);
                if(NULL != iv) {
                    for(uint8_t i = 0; i < iv->listLen; i++) {
                        if(0.0f != iv->getValue(i)) {
                            snprintf(topic, 30, "%s/ch%d/%s", iv->name, iv->assign[i].ch, fields[iv->assign[i].fieldId]);
                            snprintf(val, 10, "%.3f", iv->getValue(i));
                            mMqtt.sendMsg(topic, val);
                            yield();
                        }
                    }
                }
            }
        }
    }


    // Serial debug
    if(checkTicker(&mSerialTicker, mSerialInterval)) {
        char topic[30], val[10];
        for(uint8_t id = 0; id < mSys->getNumInverters(); id++) {
            Inverter<> *iv = mSys->getInverterByPos(id);
            if(NULL != iv) {
                for(uint8_t i = 0; i < iv->listLen; i++) {
                    if(0.0f != iv->getValue(i)) {
                        snprintf(topic, 30, "%s/ch%d/%s", iv->name, iv->assign[i].ch, iv->getFieldName(i));
                        snprintf(val, 10, "%.3f %s", iv->getValue(i), iv->getUnit(i));
                        DPRINTLN(String(topic) + ": " + String(val));
                    }
                    yield();
                }
            }
        }
    }
}


//-----------------------------------------------------------------------------
void app::handleIntr(void) {
    mSys->Radio.handleIntr();
}


//-----------------------------------------------------------------------------
void app::showIndex(void) {
    String html = FPSTR(index_html);
    html.replace("{DEVICE}", mDeviceName);
    html.replace("{VERSION}", mVersion);
    mWeb->send(200, "text/html", html);
}


//-----------------------------------------------------------------------------
void app::showSetup(void) {
    // overrides same method in main.cpp

    uint16_t interval;

    String html = FPSTR(setup_html);
    html.replace("{SSID}", mStationSsid);
    // PWD will be left at the default value (for protection)
    // -> the PWD will only be changed if it does not match the placeholder "{PWD}"

    html.replace("{DEVICE}", String(mDeviceName));
    html.replace("{VERSION}", String(mVersion));
    if(mApActive)
        html.replace("{IP}", String("http://192.168.1.1"));
    else
        html.replace("{IP}", ("http://" + String(WiFi.localIP().toString())));

    String inv;
    uint64_t invSerial;
    char invName[MAX_NAME_LENGTH + 1] = {0};
    uint8_t invType;
    for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
        mEep->read(ADDR_INV_ADDR + (i * 8),               &invSerial);
        mEep->read(ADDR_INV_NAME + (i * MAX_NAME_LENGTH), invName, MAX_NAME_LENGTH);
        mEep->read(ADDR_INV_TYPE + i,                     &invType);
        inv += "<p class=\"subdes\">Inverter "+ String(i) + "</p>";

        inv += "<label for=\"inv" + String(i) + "Addr\">Address</label>";
        inv += "<input type=\"text\" class=\"text\" name=\"inv" + String(i) + "Addr\" value=\"";
        if(0ULL != invSerial)
            inv += String(invSerial, HEX);
        inv += "\"/ maxlength=\"12\">";

        inv += "<label for=\"inv" + String(i) + "Name\">Name</label>";
        inv += "<input type=\"text\" class=\"text\" name=\"inv" + String(i) + "Name\" value=\"";
        inv += String(invName);
        inv += "\"/ maxlength=\"" + String(MAX_NAME_LENGTH) + "\">";

        inv += "<label for=\"inv" + String(i) + "Type\">Type</label>";
        inv += "<select name=\"inv" + String(i) + "Type\">";
        for(uint8_t t = 0; t < NUM_INVERTER_TYPES; t++) {
            inv += "<option value=\"" + String(t) + "\"";
            if(invType == t)
                inv += " selected";
            inv += ">" + String(invTypes[t]) + "</option>";
        }
        inv += "</select>";
    }
    html.replace("{INVERTERS}", String(inv));


    // pinout
    String pinout;
    for(uint8_t i = 0; i < 3; i++) {
        pinout += "<label for=\"" + String(pinArgNames[i]) + "\">" + String(pinNames[i]) + "</label>";
        pinout += "<select name=\"" + String(pinArgNames[i]) + "\">";
        for(uint8_t j = 0; j <= 16; j++) {
            pinout += "<option value=\"" + String(j) + "\"";
            switch(i) {
                default: if(j == mSys->Radio.pinCs)  pinout += " selected"; break;
                case 1:  if(j == mSys->Radio.pinCe)  pinout += " selected"; break;
                case 2:  if(j == mSys->Radio.pinIrq) pinout += " selected"; break;
            }
            pinout += ">" + String(wemosPins[j]) + "</option>";
        }
        pinout += "</select>";
    }
    html.replace("{PINOUT}", String(pinout));


    // nrf24l01+
    String rf24;
    for(uint8_t i = 0; i <= 3; i++) {
        rf24 += "<option value=\"" + String(i) + "\"";
        if(i == mSys->Radio.AmplifierPower)
            rf24 += " selected";
        rf24 += ">" + String(rf24AmpPower[i]) + "</option>";
    }
    html.replace("{RF24}", String(rf24));


    if(mSettingsValid) {
        mEep->read(ADDR_INV_INTERVAL, &interval);
        html.replace("{INV_INTERVAL}", String(interval));

        uint8_t mqttAddr[MQTT_ADDR_LEN] = {0};
        uint16_t mqttPort;
        mEep->read(ADDR_MQTT_ADDR,     mqttAddr, MQTT_ADDR_LEN);
        mEep->read(ADDR_MQTT_INTERVAL, &interval);
        mEep->read(ADDR_MQTT_PORT,     &mqttPort);

        char addr[16] = {0};
        sprintf(addr, "%d.%d.%d.%d", mqttAddr[0], mqttAddr[1], mqttAddr[2], mqttAddr[3]);
        html.replace("{MQTT_ADDR}",     String(addr));
        html.replace("{MQTT_PORT}",     String(mqttPort));
        html.replace("{MQTT_USER}",     String(mMqtt.getUser()));
        html.replace("{MQTT_PWD}",      String(mMqtt.getPwd()));
        html.replace("{MQTT_TOPIC}",    String(mMqtt.getTopic()));
        html.replace("{MQTT_INTERVAL}", String(interval));
    }
    else {
        html.replace("{INV_INTERVAL}", "1000");

        html.replace("{MQTT_ADDR}", "");
        html.replace("{MQTT_PORT}", "1883");
        html.replace("{MQTT_USER}", "");
        html.replace("{MQTT_PWD}", "");
        html.replace("{MQTT_TOPIC}", "/inverter");
        html.replace("{MQTT_INTERVAL}", "10000");
    }

    mWeb->send(200, "text/html", html);
}


//-----------------------------------------------------------------------------
void app::showSave(void) {
    saveValues(true);
}


//-----------------------------------------------------------------------------
void app::showErase() {
    eraseSettings();
    showReboot();
}


//-----------------------------------------------------------------------------
void app::showStatistics(void) {
    String content = "CMDs:\n";
    for(uint8_t i = 0; i < DBG_CMD_LIST_LEN; i ++) {
        content += String("0x") + String(dbgCmds[i], HEX) + String(": ") + String(mCmds[i]) + String("\n");
    }
    content += String("other: ") + String(mCmds[DBG_CMD_LIST_LEN]) + String("\n\n");

    content += "Send Cnt: " + String(mSys->Radio.mSendCnt) + String("\n\n");

    /*content += "\nCHANNELs:\n";
    content += String("23: ") + String(mChannelStat[0]) + String("\n");
    content += String("40: ") + String(mChannelStat[1]) + String("\n");
    content += String("61: ") + String(mChannelStat[2]) + String("\n");
    content += String("75: ") + String(mChannelStat[3]) + String("\n");*/

    if(!mSys->Radio.isChipConnected())
        content += "WARNING! your NRF24 module can't be reached, check the wiring and pinout (<a href=\"/setup\">setup</a>)\n";

    if(mShowRebootRequest)
        content += "INFO: reboot your ESP to apply all your configuration changes!\n";

    if(!mSettingsValid)
        content += "INFO: your settings are invalid, please switch to <a href=\"/setup\">setup</a> to correct this.\n";

    content += "MQTT: ";
    if(!mMqtt.isConnected())
        content += "not ";
    content += "connected\n";

    mWeb->send(200, "text/plain", content);
}


//-----------------------------------------------------------------------------
void app::showHoymiles(void) {
    String html = FPSTR(hoymiles_html);
    html.replace("{DEVICE}", mDeviceName);
    html.replace("{VERSION}", mVersion);
    mWeb->send(200, "text/html", html);
}


//-----------------------------------------------------------------------------
void app::showLiveData(void) {
    String modHtml;
    for(uint8_t id = 0; id < mSys->getNumInverters(); id++) {
        Inverter<> *iv = mSys->getInverterByPos(id);
        if(NULL != iv) {
#ifdef LIVEDATA_VISUALIZED
            uint8_t modNum, pos;
            switch(iv->type) {
                default:              modNum = 1; break;
                case INV_TYPE_HM600:
                case INV_TYPE_HM800:  modNum = 2; break;
                case INV_TYPE_HM1200: modNum = 4; break;
            }

            modHtml += "<div class=\"iv\">";
            modHtml += "<div class=\"ch-iv\"><span class=\"head\">" + String(iv->name) + "</span>";
            uint8_t list[8] = {FLD_UAC, FLD_IAC, FLD_PAC, FLD_F, FLD_PCT, FLD_T, FLD_YT, FLD_YD};

            for(uint8_t fld = 0; fld < 8; fld++) {
                pos = (iv->getPosByChFld(CH0, list[fld]));
                if(0xff != pos) {
                    modHtml += "<div class=\"subgrp\">";
                    modHtml += "<span class=\"value\">" + String(iv->getValue(pos));
                    modHtml += "<span class=\"unit\">" + String(iv->getUnit(pos)) + "</span></span>";
                    modHtml += "<span class=\"info\">" + String(iv->getFieldName(pos)) + "</span>";
                    modHtml += "</div>";
                }
            }
            modHtml += "</div>";

            for(uint8_t ch = 1; ch <= modNum; ch ++) {
                modHtml += "<div class=\"ch\"><span class=\"head\">CHANNEL " + String(ch) + "</span>";
                for(uint8_t j = 0; j < 5; j++) {
                    switch(j) {
                        default: pos = (iv->getPosByChFld(ch, FLD_UDC)); break;
                        case 1:  pos = (iv->getPosByChFld(ch, FLD_IDC)); break;
                        case 2:  pos = (iv->getPosByChFld(ch, FLD_PDC)); break;
                        case 3:  pos = (iv->getPosByChFld(ch, FLD_YD));  break;
                        case 4:  pos = (iv->getPosByChFld(ch, FLD_YT));  break;
                    }
                    if(0xff != pos) {
                        modHtml += "<span class=\"value\">" + String(iv->getValue(pos));
                        modHtml += "<span class=\"unit\">" + String(iv->getUnit(pos)) + "</span></span>";
                        modHtml += "<span class=\"info\">" + String(iv->getFieldName(pos)) + "</span>";
                    }
                }
                modHtml += "</div>";
            }

            modHtml += "</div>";
#else
            // dump all data to web frontend
            modHtml = "<pre>";
            char topic[30], val[10];
            for(uint8_t i = 0; i < iv->listLen; i++) {
                snprintf(topic, 30, "%s/ch%d/%s", iv->name, iv->assign[i].ch, iv->getFieldName(i));
                snprintf(val, 10, "%.3f %s", iv->getValue(i), iv->getUnit(i));
                modHtml += String(topic) + ": " + String(val) + "\n";
            }
            modHtml += "</pre>";
#endif
        }
    }


    mWeb->send(200, "text/html", modHtml);
}


//-----------------------------------------------------------------------------
void app::saveValues(bool webSend = true) {
    Main::saveValues(false); // general configuration

    if(mWeb->args() > 0) {
        char *p;
        char buf[20] = {0};
        uint8_t i = 0;
        uint16_t interval;

        // inverter
        serial_u addr;
        for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
            // address
            mWeb->arg("inv" + String(i) + "Addr").toCharArray(buf, 20);
            if(strlen(buf) == 0)
                snprintf(buf, 20, "\0");
            addr.u64 = Serial2u64(buf);
            mEep->write(ADDR_INV_ADDR + (i * 8), addr.u64);

            // name
            mWeb->arg("inv" + String(i) + "Name").toCharArray(buf, 20);
            mEep->write(ADDR_INV_NAME + (i * MAX_NAME_LENGTH), buf, MAX_NAME_LENGTH);

            // type
            mWeb->arg("inv" + String(i) + "Type").toCharArray(buf, 20);
            uint8_t type = atoi(buf);
            mEep->write(ADDR_INV_TYPE + i, type);
        }

        interval = mWeb->arg("invInterval").toInt();
        mEep->write(ADDR_INV_INTERVAL, interval);


        // pinout
        for(uint8_t i = 0; i < 3; i ++) {
            uint8_t pin = mWeb->arg(String(pinArgNames[i])).toInt();
            mEep->write(ADDR_PINOUT + i, pin);
        }


        // nrf24 amplifier power
        mSys->Radio.AmplifierPower = mWeb->arg("rf24Power").toInt() & 0x03;
        mEep->write(ADDR_RF24_AMP_PWR, mSys->Radio.AmplifierPower);

        // mqtt
        uint8_t mqttAddr[MQTT_ADDR_LEN] = {0};
        uint16_t mqttPort;
        char mqttUser[MQTT_USER_LEN];
        char mqttPwd[MQTT_PWD_LEN];
        char mqttTopic[MQTT_TOPIC_LEN];
        mWeb->arg("mqttAddr").toCharArray(buf, 20);
        i = 0;
        p = strtok(buf, ".");
        while(NULL != p) {
            mqttAddr[i++] = atoi(p);
            p = strtok(NULL, ".");
        }
        mWeb->arg("mqttUser").toCharArray(mqttUser, MQTT_USER_LEN);
        mWeb->arg("mqttPwd").toCharArray(mqttPwd, MQTT_PWD_LEN);
        mWeb->arg("mqttTopic").toCharArray(mqttTopic, MQTT_TOPIC_LEN);
        interval = mWeb->arg("mqttInterval").toInt();
        mqttPort = mWeb->arg("mqttPort").toInt();
        mEep->write(ADDR_MQTT_ADDR, mqttAddr, MQTT_ADDR_LEN);
        mEep->write(ADDR_MQTT_PORT, mqttPort);
        mEep->write(ADDR_MQTT_USER, mqttUser, MQTT_USER_LEN);
        mEep->write(ADDR_MQTT_PWD,  mqttPwd,  MQTT_PWD_LEN);
        mEep->write(ADDR_MQTT_TOPIC, mqttTopic, MQTT_TOPIC_LEN);
        mEep->write(ADDR_MQTT_INTERVAL, interval);

        updateCrc();
        if((mWeb->arg("reboot") == "on"))
            showReboot();
        else {
            mShowRebootRequest = true;
            mWeb->send(200, "text/html", "<!doctype html><html><head><title>Setup saved</title><meta http-equiv=\"refresh\" content=\"0; URL=/setup\"></head><body>"
                "<p>saved</p></body></html>");
        }
    }
    else {
        mWeb->send(200, "text/html", "<!doctype html><html><head><title>Error</title><meta http-equiv=\"refresh\" content=\"3; URL=/setup\"></head><body>"
            "<p>Error while saving</p></body></html>");
    }
}


//-----------------------------------------------------------------------------
void app::updateCrc(void) {
    Main::updateCrc();

    uint16_t crc;
    crc = buildEEpCrc(ADDR_START_SETTINGS, (ADDR_NEXT - ADDR_START_SETTINGS));
    //DPRINTLN("new CRC: " + String(crc, HEX));
    mEep->write(ADDR_SETTINGS_CRC, crc);
}
