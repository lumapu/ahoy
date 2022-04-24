#include "app.h"

#include "html/h/index_html.h"
#include "html/h/hoymiles_html.h"
extern String setup_html;


#define DUMMY_RADIO_ID          ((uint64_t)0xDEADBEEF01ULL)

//-----------------------------------------------------------------------------
app::app() : Main() {
    mSendCnt    = 0;
    mSendTicker = new Ticker();
    mFlagSend   = false;

    mMqttTicker = NULL;
    mMqttEvt    = false;

    memset(mCmds, 0, sizeof(uint32_t));
    memset(mChannelStat, 0, sizeof(uint32_t));

    mSys = new HmSystemType();
}


//-----------------------------------------------------------------------------
app::~app(void) {

}


//-----------------------------------------------------------------------------
void app::setup(const char *ssid, const char *pwd, uint32_t timeout) {
    Main::setup(ssid, pwd, timeout);

    mWeb->on("/",          std::bind(&app::showIndex,         this));
    mWeb->on("/setup",     std::bind(&app::showSetup,         this));
    mWeb->on("/save",      std::bind(&app::showSave,          this));
    mWeb->on("/cmdstat",   std::bind(&app::showCmdStatistics, this));
    mWeb->on("/hoymiles",  std::bind(&app::showHoymiles,      this));
    mWeb->on("/livedata",  std::bind(&app::showLiveData,      this));
    mWeb->on("/mqttstate", std::bind(&app::showMqtt,          this));

    if(mSettingsValid) {
        uint16_t interval;
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
                Serial.println("add inverter: " + String(invName) + ", SN: " + String(invSerial, HEX) + ", type: " + String(invType));
            }
        }

        mEep->read(ADDR_INV_INTERVAL, &interval);
        if(interval < 1000)
            interval = 1000;
        mSendTicker->attach_ms(interval, std::bind(&app::sendTicker, this));


        // mqtt
        uint8_t mqttAddr[MQTT_ADDR_LEN];
        char mqttUser[MQTT_USER_LEN];
        char mqttPwd[MQTT_PWD_LEN];
        char mqttTopic[MQTT_TOPIC_LEN];
        mEep->read(ADDR_MQTT_ADDR,     mqttAddr,  MQTT_ADDR_LEN);
        mEep->read(ADDR_MQTT_USER,     mqttUser,  MQTT_USER_LEN);
        mEep->read(ADDR_MQTT_PWD,      mqttPwd,   MQTT_PWD_LEN);
        mEep->read(ADDR_MQTT_TOPIC,    mqttTopic, MQTT_TOPIC_LEN);
        mEep->read(ADDR_MQTT_INTERVAL, &interval);

        char addr[16] = {0};
        sprintf(addr, "%d.%d.%d.%d", mqttAddr[0], mqttAddr[1], mqttAddr[2], mqttAddr[3]);

        if(interval < 1000)
            interval = 1000;
        mMqtt.setup(addr, mqttTopic, mqttUser, mqttPwd);
        mMqttTicker = new Ticker();
        mMqttTicker->attach_ms(interval, std::bind(&app::mqttTicker, this));

        mMqtt.sendMsg("version", mVersion);
    }

    initRadio();

    if(!mSettingsValid)
        Serial.println("Warn: your settings are not valid! check [IP]/setup");
}


//-----------------------------------------------------------------------------
void app::loop(void) {
    Main::loop();

    if(!mSys->BufCtrl.empty()) {
        uint8_t len, rptCnt;
        packet_t *p = mSys->BufCtrl.getBack();
        //dumpBuf("RAW ", p->packet, MAX_RF_PAYLOAD_SIZE);

        if(mSys->Radio.checkCrc(p->packet, &len, &rptCnt)) {
            // process buffer only on first occurrence
            if((0 != len) && (0 == rptCnt)) {
                //Serial.println("CMD " + String(*cmd, HEX));
                //dumpBuf("Payload ", p->packet, len);

                uint8_t *cmd = &p->packet[11];
                inverter_t *iv = mSys->findInverter(&p->packet[3]);
                if(NULL != iv) {
                    for(uint8_t i = 0; i < iv->listLen; i++) {
                        if(iv->assign[i].cmdId == *cmd)
                            mSys->addValue(iv, i, &p->packet[11]);
                    }
                }

                if(*cmd == 0x01)      mCmds[0]++;
                else if(*cmd == 0x02) mCmds[1]++;
                else if(*cmd == 0x03) mCmds[2]++;
                else if(*cmd == 0x81) mCmds[3]++;
                else if(*cmd == 0x84) mCmds[4]++;
                else                  mCmds[5]++;

                if(p->sendCh == 23)      mChannelStat[0]++;
                else if(p->sendCh == 40) mChannelStat[1]++;
                else if(p->sendCh == 61) mChannelStat[2]++;
                else                     mChannelStat[3]++;
            }
        }
        mSys->BufCtrl.popBack();
    }

    if(mFlagSend) {
        mFlagSend = false;

        uint8_t size = 0;
        inverter_t *inv;

        for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
            inv = mSys->getInverterByPos(i);
            if(NULL != inv) {
                //if((mSendCnt % 6) == 0)
                    size = mSys->Radio.getTimePacket(&inv->radioId.u64, mSendBuf, mTimestamp);
                /*else if((mSendCnt % 6) == 1)
                    size = mSys->Radio.getCmdPacket(&inv->radioId.u64, mSendBuf, 0x15, 0x81);
                else if((mSendCnt % 6) == 2)
                    size = mSys->Radio.getCmdPacket(&inv->radioId.u64, mSendBuf, 0x15, 0x80);
                else if((mSendCnt % 6) == 3)
                    size = mSys->Radio.getCmdPacket(&inv->radioId.u64, mSendBuf, 0x15, 0x83);
                else if((mSendCnt % 6) == 4)
                    size = mSys->Radio.getCmdPacket(&inv->radioId.u64, mSendBuf, 0x15, 0x82);
                else if((mSendCnt % 6) == 5)
                    size = mSys->Radio.getCmdPacket(&inv->radioId.u64, mSendBuf, 0x15, 0x84);*/

                //Serial.println("sent packet: #" + String(mSendCnt));
                //dumpBuf("SEN ", mSendBuf, size);
                sendPacket(inv, mSendBuf, size);
                mSendCnt++;

                delay(20);
            }
        }
    }


    // mqtt
    mMqtt.loop();
    if(mMqttEvt) {
        mMqttEvt = false;
        mMqtt.isConnected(true);
        char topic[30], val[10];
        for(uint8_t id = 0; id < mSys->getNumInverters(); id++) {
            inverter_t *iv = mSys->getInverterByPos(id);
            if(NULL != iv) {
                for(uint8_t i = 0; i < iv->listLen; i++) {
                    if(0.0f != mSys->getValue(iv, i)) {
                        snprintf(topic, 30, "%s/ch%d/%s", iv->name, iv->assign[i].ch, fields[iv->assign[i].fieldId]);
                        snprintf(val, 10, "%.3f", mSys->getValue(iv, i));
                        mMqtt.sendMsg(topic, val);
                        delay(20);
                    }
                }
            }
        }

        // Serial debug
        //char topic[30], val[10];
        for(uint8_t id = 0; id < mSys->getNumInverters(); id++) {
            inverter_t *iv = mSys->getInverterByPos(id);
            if(NULL != iv) {
                for(uint8_t i = 0; i < iv->listLen; i++) {
                    if(0.0f != mSys->getValue(iv, i)) {
                        snprintf(topic, 30, "%s/ch%d/%s", iv->name, iv->assign[i].ch, mSys->getFieldName(iv, i));
                        snprintf(val, 10, "%.3f %s", mSys->getValue(iv, i), mSys->getUnit(iv, i));
                        Serial.println(String(topic) + ": " + String(val));
                    }
                }
            }
        }
    }
}


//-----------------------------------------------------------------------------
void app::handleIntr(void) {
    uint8_t pipe, len;
    packet_t *p;

    DISABLE_IRQ;

    while(mRadio->available(&pipe)) {
        if(!mSys->BufCtrl.full()) {
            p = mSys->BufCtrl.getFront();
            memset(p->packet, 0xcc, MAX_RF_PAYLOAD_SIZE);
            p->sendCh = mSendChannel;
            len = mRadio->getPayloadSize();
            if(len > MAX_RF_PAYLOAD_SIZE)
                len = MAX_RF_PAYLOAD_SIZE;

            mRadio->read(p->packet, len);
            mSys->BufCtrl.pushFront(p);
        }
        else {
            bool tx_ok, tx_fail, rx_ready;
            mRadio->whatHappened(tx_ok, tx_fail, rx_ready); // reset interrupt status
            mRadio->flush_rx(); // drop the packet
        }
    }

    RESTORE_IRQ;
}


//-----------------------------------------------------------------------------
void app::initRadio(void) {
    mRadio = new RF24(RF24_CE_PIN, RF24_CS_PIN);

    mRadio->begin();
    mRadio->setAutoAck(false);
    mRadio->setRetries(0, 0);

    mRadio->setChannel(DEFAULT_RECV_CHANNEL);
    mRadio->setDataRate(RF24_250KBPS);
    mRadio->disableCRC();
    mRadio->setAutoAck(false);
    mRadio->setPayloadSize(MAX_RF_PAYLOAD_SIZE);
    mRadio->setAddressWidth(5);
    mRadio->openReadingPipe(1, DTU_RADIO_ID);

    // enable only receiving interrupts
    mRadio->maskIRQ(true, true, false);

    // Use lo PA level, as a higher level will disturb CH340 serial usb adapter
    mRadio->setPALevel(RF24_PA_MAX);
    mRadio->startListening();

    Serial.println("Radio Config:");
    mRadio->printPrettyDetails();

    mSendChannel = mSys->Radio.getDefaultChannel();
}


//-----------------------------------------------------------------------------
void app::sendPacket(inverter_t *inv, uint8_t buf[], uint8_t len) {
    DISABLE_IRQ;
    mRadio->stopListening();

#ifdef CHANNEL_HOP
    //if(mSendCnt % 6 == 0)
        mSendChannel = mSys->Radio.getNxtChannel();
    //else
    //    mSendChannel = mSys->Radio.getLastChannel();
#else
    mSendChannel = mSys->Radio.getDefaultChannel();
#endif
    mRadio->setChannel(mSendChannel);
    //Serial.println("CH: " + String(mSendChannel));

    mRadio->openWritingPipe(inv->radioId.u64);
    mRadio->setCRCLength(RF24_CRC_16);
    mRadio->enableDynamicPayloads();
    mRadio->setAutoAck(true);
    mRadio->setRetries(3, 15);

    mRadio->write(buf, len);

    // Try to avoid zero payload acks (has no effect)
    mRadio->openWritingPipe(DUMMY_RADIO_ID); // TODO: why dummy radio id?

    mRadio->setAutoAck(false);
    mRadio->setRetries(0, 0);
    mRadio->disableDynamicPayloads();
    mRadio->setCRCLength(RF24_CRC_DISABLED);

    mRadio->setChannel(DEFAULT_RECV_CHANNEL);
    mRadio->startListening();

    RESTORE_IRQ;
}


//-----------------------------------------------------------------------------
void app::sendTicker(void) {
    mFlagSend = true;
}


//-----------------------------------------------------------------------------
void app::mqttTicker(void) {
    mMqttEvt = true;
}


//-----------------------------------------------------------------------------
void app::showIndex(void) {
    String html = index_html;
    html.replace("{DEVICE}", mDeviceName);
    html.replace("{VERSION}", mVersion);
    mWeb->send(200, "text/html", html);
}


//-----------------------------------------------------------------------------
void app::showSetup(void) {
    // overrides same method in main.cpp

    uint16_t interval;

    String html = setup_html;
    html.replace("{SSID}", mStationSsid);
    // PWD will be left at the default value (for protection)
    // -> the PWD will only be changed if it does not match the placeholder "{PWD}"

    html.replace("{DEVICE}", String(mDeviceName));
    html.replace("{VERSION}", String(mVersion));

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

    if(mSettingsValid) {
        mEep->read(ADDR_INV_INTERVAL, &interval);
        html.replace("{INV_INTERVAL}", String(interval));

        uint8_t mqttAddr[MQTT_ADDR_LEN] = {0};
        mEep->read(ADDR_MQTT_ADDR,     mqttAddr, MQTT_ADDR_LEN);
        mEep->read(ADDR_MQTT_INTERVAL, &interval);

        char addr[16] = {0};
        sprintf(addr, "%d.%d.%d.%d", mqttAddr[0], mqttAddr[1], mqttAddr[2], mqttAddr[3]);
        html.replace("{MQTT_ADDR}",     String(addr));
        html.replace("{MQTT_USER}",     String(mMqtt.getUser()));
        html.replace("{MQTT_PWD}",      String(mMqtt.getPwd()));
        html.replace("{MQTT_TOPIC}",    String(mMqtt.getTopic()));
        html.replace("{MQTT_INTERVAL}", String(interval));
    }
    else {
        html.replace("{INV_INTERVAL}", "1000");

        html.replace("{MQTT_ADDR}", "");
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
void app::showCmdStatistics(void) {
    String content = "CMDs:\n";
    content += String("0x01: ") + String(mCmds[0]) + String("\n");
    content += String("0x02: ") + String(mCmds[1]) + String("\n");
    content += String("0x03: ") + String(mCmds[2]) + String("\n");
    content += String("0x81: ") + String(mCmds[3]) + String("\n");
    content += String("0x84: ") + String(mCmds[4]) + String("\n");
    content += String("other: ") + String(mCmds[5]) + String("\n");

    content += "\nCHANNELs:\n";
    content += String("23: ") + String(mChannelStat[0]) + String("\n");
    content += String("40: ") + String(mChannelStat[1]) + String("\n");
    content += String("61: ") + String(mChannelStat[2]) + String("\n");
    content += String("75: ") + String(mChannelStat[3]) + String("\n");
    mWeb->send(200, "text/plain", content);
}


//-----------------------------------------------------------------------------
void app::showHoymiles(void) {
    String html = hoymiles_html;
    html.replace("{DEVICE}", mDeviceName);
    html.replace("{VERSION}", mVersion);
    mWeb->send(200, "text/html", html);
}


//-----------------------------------------------------------------------------
void app::showLiveData(void) {
    String modHtml;
    for(uint8_t id = 0; id < mSys->getNumInverters(); id++) {
        inverter_t *iv = mSys->getInverterByPos(id);
        if(NULL != iv) {
#ifdef LIVEDATA_VISUALIZED
            uint8_t modNum, pos;
            switch(iv->type) {
                default:              modNum = 1; break;
                case INV_TYPE_HM600:  modNum = 2; break;
                case INV_TYPE_HM1200: modNum = 4; break;
            }

            for(uint8_t ch = 1; ch <= modNum; ch ++) {
                modHtml += "<div class=\"ch\"><span class=\"head\">CHANNEL " + String(ch) + "</span>";
                for(uint8_t j = 0; j < 5; j++) {
                    switch(j) {
                        default: pos = (mSys->getPosByChField(iv, ch, FLD_UDC)); break;
                        case 1:  pos = (mSys->getPosByChField(iv, ch, FLD_IDC)); break;
                        case 2:  pos = (mSys->getPosByChField(iv, ch, FLD_PDC)); break;
                        case 3:  pos = (mSys->getPosByChField(iv, ch, FLD_YD));  break;
                        case 4:  pos = (mSys->getPosByChField(iv, ch, FLD_YT));  break;
                    }
                    if(0xff != pos) {
                        modHtml += "<span class=\"value\">" + String(mSys->getValue(iv, pos));
                        modHtml += "<span class=\"unit\">" + String(mSys->getUnit(iv, pos)) + "</span></span>";
                        modHtml += "<span class=\"info\">" + String(mSys->getFieldName(iv, pos)) + "</span>";
                    }
                }
                modHtml += "</div>";
            }
#else
            // dump all data to web frontend
            modHtml = "<pre>";
            char topic[30], val[10];
            for(uint8_t i = 0; i < iv->listLen; i++) {
                snprintf(topic, 30, "%s/ch%d/%s", iv->name, iv->assign[i].ch, mSys->getFieldName(iv, i));
                snprintf(val, 10, "%.3f %s", mSys->getValue(iv, i), mSys->getUnit(iv, i));
                modHtml += String(topic) + ": " + String(val) + "\n";
            }
            modHtml += "</pre>";
#endif
        }
    }


    mWeb->send(200, "text/html", modHtml);
}


//-----------------------------------------------------------------------------
void app::showMqtt(void) {
    String txt = "connected";
    if(mMqtt.isConnected())
        txt = "not " + txt;
    mWeb->send(200, "text/plain", txt);
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
            mEep->write(ADDR_INV_TYPE + (i * MAX_NAME_LENGTH), type);
        }

        interval = mWeb->arg("invInterval").toInt();
        mEep->write(ADDR_INV_INTERVAL, interval);


        // mqtt
        uint8_t mqttAddr[MQTT_ADDR_LEN] = {0};
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
        mEep->write(ADDR_MQTT_ADDR, mqttAddr, MQTT_ADDR_LEN);
        mEep->write(ADDR_MQTT_USER, mqttUser, MQTT_USER_LEN);
        mEep->write(ADDR_MQTT_PWD,  mqttPwd,  MQTT_PWD_LEN);
        mEep->write(ADDR_MQTT_TOPIC, mqttTopic, MQTT_TOPIC_LEN);
        mEep->write(ADDR_MQTT_INTERVAL, interval);

        updateCrc();
        if((mWeb->arg("reboot") == "on"))
            showReboot();
        else {
            mWeb->send(200, "text/html", "<!doctype html><html><head><title>Setup saved</title><meta http-equiv=\"refresh\" content=\"0; URL=/setup\"></head><body>"
                "<p>saved</p></body></html>");
        }
    }
    else {
        mWeb->send(200, "text/html", "<!doctype html><html><head><title>Error</title><meta http-equiv=\"refresh\" content=\"3; URL=/setup\"></head><body>"
            "<p>Error while saving</p></body></html>");
    }
}
