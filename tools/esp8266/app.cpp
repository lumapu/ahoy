#include "app.h"

#include "html/h/index_html.h"
#include "html/h/setup_html.h"
#include "html/h/hoymiles_html.h"


//-----------------------------------------------------------------------------
app::app() : Main() {
    mSendTicker     = 0xffff;
    mSendInterval   = 0;
    mMqttTicker     = 0xffff;
    mMqttInterval   = 0;
    mSerialTicker   = 0xffff;
    mSerialInterval = 0;
    mMqttActive     = false;

    mTicker = 0;
    mRxTicker = 0;

    mShowRebootRequest = false;

    mSerialValues = true;
    mSerialDebug  = false;

    memset(mPayload, 0, (MAX_NUM_INVERTERS * sizeof(invPayload_t)));
    mRxFailed = 0;

    mSys = new HmSystemType();
}


//-----------------------------------------------------------------------------
app::~app(void) {

}


//-----------------------------------------------------------------------------
void app::setup(uint32_t timeout) {
    Main::setup(timeout);

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

        // inverter
        for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
            mEep->read(ADDR_INV_ADDR + (i * 8),               &invSerial);
            mEep->read(ADDR_INV_NAME + (i * MAX_NAME_LENGTH), invName, MAX_NAME_LENGTH);
            if(0ULL != invSerial) {
                mSys->addInverter(invName, invSerial);
                DPRINTLN("add inverter: " + String(invName) + ", SN: " + String(invSerial, HEX));
            }
        }
        mEep->read(ADDR_INV_INTERVAL, &mSendInterval);
        if(mSendInterval < 5)
            mSendInterval = 5;
        mSendTicker = mSendInterval;

        // pinout
        mEep->read(ADDR_PINOUT,   &mSys->Radio.pinCs);
        mEep->read(ADDR_PINOUT+1, &mSys->Radio.pinCe);
        mEep->read(ADDR_PINOUT+2, &mSys->Radio.pinIrq);


        // nrf24 amplifier power
        mEep->read(ADDR_RF24_AMP_PWR, &mSys->Radio.AmplifierPower);

        // serial console
        uint8_t tmp;
        mEep->read(ADDR_SER_INTERVAL, &mSerialInterval);
        mEep->read(ADDR_SER_ENABLE, &tmp);
        mSerialValues = (tmp == 0x01);
        mEep->read(ADDR_SER_DEBUG, &tmp);
        mSerialDebug = (tmp == 0x01);
        if(mSerialInterval < 1)
            mSerialInterval = 1;
        mSys->Radio.mSerialDebug = mSerialDebug;


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


        if(mMqttInterval < 1)
            mMqttInterval = 1;
        mMqtt.setup(addr, mqttTopic, mqttUser, mqttPwd, mqttPort);
        mMqttTicker = 0;

        mSerialTicker = 0;

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

    if(checkTicker(&mRxTicker, 5)) {
        bool rxRdy = mSys->Radio.switchRxCh();

        if(!mSys->BufCtrl.empty()) {
            uint8_t len;
            packet_t *p = mSys->BufCtrl.getBack();

            if(mSys->Radio.checkPaketCrc(p->packet, &len, p->rxCh)) {
                // process buffer only on first occurrence
                if(mSerialDebug) {
                    DPRINT("Received " + String(len) + " bytes channel " + String(p->rxCh) + ": ");
                    mSys->Radio.dumpBuf(NULL, p->packet, len);
                }

                if(0 != len) {
                    Inverter<> *iv = mSys->findInverter(&p->packet[1]);
                    if(NULL != iv) {
                        uint8_t *pid = &p->packet[9];
                        if((*pid & 0x7F) < 5) {
                            memcpy(mPayload[iv->id].data[(*pid & 0x7F) - 1], &p->packet[10], len-11);
                            mPayload[iv->id].len[(*pid & 0x7F) - 1] = len-11;
                        }

                        if((*pid & 0x80) == 0x80) {
                            if((*pid & 0x7f) > mPayload[iv->id].maxPackId)
                                mPayload[iv->id].maxPackId = (*pid & 0x7f);
                        }
                    }
                }
            }

            mSys->BufCtrl.popBack();
        }


        if(rxRdy) {
            processPayload(true);
        }
    }

    if(checkTicker(&mTicker, 1000)) {
        if(mMqttActive) {
            mMqtt.loop();
            if(++mMqttTicker >= mMqttInterval) {
                mMqttTicker = 0;
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

        if(mSerialValues) {
            if(++mSerialTicker >= mSerialInterval) {
                mSerialTicker = 0;
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

        if(++mSendTicker >= mSendInterval) {
            mSendTicker = 0;

            if(!mSys->BufCtrl.empty()) {
                if(mSerialDebug)
                    DPRINTLN("recbuf not empty! #" + String(mSys->BufCtrl.getFill()));
            }
            Inverter<> *iv;
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
                iv = mSys->getInverterByPos(i);
                if(NULL != iv) {
                    // reset payload data
                    memset(mPayload[iv->id].len, 0, MAX_PAYLOAD_ENTRIES);
                    mPayload[iv->id].maxPackId = 0;
                    if(mSerialDebug) {
                        if(!mPayload[iv->id].complete)
                            processPayload(false);

                        if(!mPayload[iv->id].complete) {
                            DPRINT("Inverter #" + String(iv->id) + " ");
                            DPRINTLN("no Payload received!");
                            mRxFailed++;
                        }
                    }
                    mPayload[iv->id].complete = false;
                    mPayload[iv->id].ts = mTimestamp;

                    yield();
                    if(mSerialDebug)
                        DPRINTLN("Requesting Inverter SN " + String(iv->serial.u64, HEX));
                    mSys->Radio.sendTimePacket(iv->radioId.u64, mPayload[iv->id].ts);
                    mRxTicker = 0;
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
bool app::buildPayload(uint8_t id) {
    //DPRINTLN("Payload");
    uint16_t crc = 0xffff, crcRcv;
    if(mPayload[id].maxPackId > MAX_PAYLOAD_ENTRIES)
        mPayload[id].maxPackId = MAX_PAYLOAD_ENTRIES;

    for(uint8_t i = 0; i < mPayload[id].maxPackId; i ++) {
        if(mPayload[id].len[i] > 0) {
            if(i == (mPayload[id].maxPackId-1)) {
                crc = crc16(mPayload[id].data[i], mPayload[id].len[i] - 2, crc);
                crcRcv = (mPayload[id].data[i][mPayload[id].len[i] - 2] << 8)
                    | (mPayload[id].data[i][mPayload[id].len[i] - 1]);
            }
            else
                crc = crc16(mPayload[id].data[i], mPayload[id].len[i], crc);
        }
    }
    if(crc == crcRcv)
        return true;
    return false;
}


//-----------------------------------------------------------------------------
void app::processPayload(bool retransmit) {
    for(uint8_t id = 0; id < mSys->getNumInverters(); id++) {
        Inverter<> *iv = mSys->getInverterByPos(id);
        if(NULL != iv) {
            if(!mPayload[iv->id].complete) {
                if(!buildPayload(iv->id)) {
                    if(retransmit) {
                        if(mPayload[iv->id].maxPackId != 0) {
                            for(uint8_t i = 0; i < (mPayload[iv->id].maxPackId-1); i ++) {
                                if(mPayload[iv->id].len[i] == 0) {
                                    if(mSerialDebug)
                                        DPRINTLN("Error while retrieving data: Frame " + String(i+1) + " missing: Request Retransmit");
                                    mSys->Radio.sendCmdPacket(iv->radioId.u64, 0x15, (0x81+i), true);
                                }
                            }
                        }
                        else {
                            if(mSerialDebug)
                                DPRINTLN("Error while retrieving data: last frame missing: Request Retransmit");
                            mSys->Radio.sendTimePacket(iv->radioId.u64, mPayload[iv->id].ts);
                        }
                        mSys->Radio.switchRxCh(100);
                    }
                }
                else {
                    mPayload[iv->id].complete = true;
                    uint8_t payload[128] = {0};
                    uint8_t offs = 0;
                    for(uint8_t i = 0; i < (mPayload[iv->id].maxPackId); i ++) {
                        memcpy(&payload[offs], mPayload[iv->id].data[i], (mPayload[iv->id].len[i]));
                        offs += (mPayload[iv->id].len[i]);
                    }
                    offs-=2;
                    if(mSerialDebug) {
                        DPRINT("Payload (" + String(offs) + "): ");
                        mSys->Radio.dumpBuf(NULL, payload, offs);
                    }

                    for(uint8_t i = 0; i < iv->listLen; i++) {
                        iv->addValue(i, payload);
                    }
                    iv->doCalculations();
                }
            }
        }
    }
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
        html.replace("{INV_INTVL}", String(interval));

        uint8_t tmp;
        mEep->read(ADDR_SER_INTERVAL, &interval);
        mEep->read(ADDR_SER_ENABLE, &tmp);
        html.replace("{SER_INTVL}", String(interval));
        html.replace("{SER_VAL_CB}", (tmp == 0x01) ? "checked" : "");
        mEep->read(ADDR_SER_DEBUG, &tmp);
        html.replace("{SER_DBG_CB}", (tmp == 0x01) ? "checked" : "");

        uint8_t mqttAddr[MQTT_ADDR_LEN] = {0};
        uint16_t mqttPort;
        mEep->read(ADDR_MQTT_ADDR,     mqttAddr, MQTT_ADDR_LEN);
        mEep->read(ADDR_MQTT_INTERVAL, &interval);
        mEep->read(ADDR_MQTT_PORT,     &mqttPort);

        char addr[16] = {0};
        sprintf(addr, "%d.%d.%d.%d", mqttAddr[0], mqttAddr[1], mqttAddr[2], mqttAddr[3]);
        html.replace("{MQTT_ADDR}",  String(addr));
        html.replace("{MQTT_PORT}",  String(mqttPort));
        html.replace("{MQTT_USER}",  String(mMqtt.getUser()));
        html.replace("{MQTT_PWD}",   String(mMqtt.getPwd()));
        html.replace("{MQTT_TOPIC}", String(mMqtt.getTopic()));
        html.replace("{MQTT_INTVL}", String(interval));
    }
    else {
        html.replace("{INV_INTVL}", "5");

        html.replace("{SER_VAL_CB}", "checked");
        html.replace("{SER_DBG_CB}", "");
        html.replace("{SER_INTVL}", "10");

        html.replace("{MQTT_ADDR}", "");
        html.replace("{MQTT_PORT}", "1883");
        html.replace("{MQTT_USER}", "");
        html.replace("{MQTT_PWD}", "");
        html.replace("{MQTT_TOPIC}", "inverter");
        html.replace("{MQTT_INTVL}", "10");

        html.replace("{SER_INTVL}", "10");
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
    String content = "Failed Payload: " + String(mRxFailed) + "\n";
    content += "Send Cnt: " + String(mSys->Radio.mSendCnt) + String("\n\n");

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
                default:
                case INV_TYPE_1CH: modNum = 1; break;
                case INV_TYPE_2CH: modNum = 2; break;
                case INV_TYPE_4CH: modNum = 4; break;
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
        interval = mWeb->arg("mqttIntvl").toInt();
        mqttPort = mWeb->arg("mqttPort").toInt();
        mEep->write(ADDR_MQTT_ADDR, mqttAddr, MQTT_ADDR_LEN);
        mEep->write(ADDR_MQTT_PORT, mqttPort);
        mEep->write(ADDR_MQTT_USER, mqttUser, MQTT_USER_LEN);
        mEep->write(ADDR_MQTT_PWD,  mqttPwd,  MQTT_PWD_LEN);
        mEep->write(ADDR_MQTT_TOPIC, mqttTopic, MQTT_TOPIC_LEN);
        mEep->write(ADDR_MQTT_INTERVAL, interval);


        // serial console
        bool tmp;
        interval = mWeb->arg("serIntvl").toInt();
        mEep->write(ADDR_SER_INTERVAL, interval);
        tmp = (mWeb->arg("serEn") == "on");
        mEep->write(ADDR_SER_ENABLE, (uint8_t)((tmp) ? 0x01 : 0x00));
        mSerialDebug = (mWeb->arg("serDbg") == "on");
        mEep->write(ADDR_SER_DEBUG, (uint8_t)((mSerialDebug) ? 0x01 : 0x00));
        DPRINT("Info: Serial debug is ");
        if(mSerialDebug) DPRINTLN("on"); else DPRINTLN("off");
        mSys->Radio.mSerialDebug = mSerialDebug;

        updateCrc();
        if((mWeb->arg("reboot") == "on"))
            showReboot();
        else {
            mShowRebootRequest = true;
            mWeb->send(200, "text/html", "<!doctype html><html><head><title>Setup saved</title><meta http-equiv=\"refresh\" content=\"1; URL=/setup\"></head><body>"
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
