//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#include "app.h"

#include "favicon.h"
#include "html/h/index_html.h"
#include "html/h/setup_html.h"
#include "html/h/hoymiles_html.h"
#include <ArduinoJson.h>



//-----------------------------------------------------------------------------
app::app() : Main() {
    DPRINTLN(DBG_VERBOSE, F("app::app():Main"));
    mSendTicker     = 0xffff;
    mSendInterval   = SEND_INTERVAL;
    mMqttTicker     = 0xffff;
    mMqttInterval   = MQTT_INTERVAL;
    mSerialTicker   = 0xffff;
    mSerialInterval = SERIAL_INTERVAL;
    mMqttActive     = false;

    mTicker = 0;
    mRxTicker = 0;

    mSendLastIvId = 0;

    mShowRebootRequest = false;

    mSerialValues = true;
    mSerialDebug  = false;

    memset(mPayload, 0, (MAX_NUM_INVERTERS * sizeof(invPayload_t)));
    mRxFailed     = 0;
    mRxSuccess    = 0;
    mFrameCnt     = 0;
    mLastPacketId = 0x00;

    mSys = new HmSystemType();
}


//-----------------------------------------------------------------------------
app::~app(void) {

}


//-----------------------------------------------------------------------------
void app::setup(uint32_t timeout) {
    DPRINTLN(DBG_VERBOSE, F("app::setup"));
    Main::setup(timeout);

    mWeb->on("/",               std::bind(&app::showIndex,      this));
    mWeb->on("/favicon.ico",    std::bind(&app::showFavicon,    this));
    mWeb->on("/setup",          std::bind(&app::showSetup,      this));
    mWeb->on("/save",           std::bind(&app::showSave,       this));
    mWeb->on("/erase",          std::bind(&app::showErase,      this));
    mWeb->on("/cmdstat",        std::bind(&app::showStatistics, this));
    mWeb->on("/hoymiles",       std::bind(&app::showHoymiles,   this));
    mWeb->on("/livedata",       std::bind(&app::showLiveData,   this));
    mWeb->on("/json",           std::bind(&app::showJSON,       this));
    mWeb->on("/api",HTTP_POST,  std::bind(&app::webapi,         this));
    
    if(mSettingsValid) {
        mEep->read(ADDR_INV_INTERVAL, &mSendInterval);
        if(mSendInterval < MIN_SEND_INTERVAL)
            mSendInterval = MIN_SEND_INTERVAL;
        mSendTicker = mSendInterval;

        // inverter
        uint64_t invSerial;
        char name[MAX_NAME_LENGTH + 1] = {0};
        uint16_t modPwr[4];
        Inverter<> *iv;
        for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
            mEep->read(ADDR_INV_ADDR + (i * 8),               &invSerial);
            mEep->read(ADDR_INV_NAME + (i * MAX_NAME_LENGTH), name, MAX_NAME_LENGTH);
            mEep->read(ADDR_INV_CH_PWR + (i * 2 * 4),         modPwr, 4);
            if(0ULL != invSerial) {
                iv = mSys->addInverter(name, invSerial, modPwr);
                if(NULL != iv) {
                    mEep->read(ADDR_INV_PWR_LIM + (i * 2),(uint16_t *)&(iv->powerLimit[0]));
                    if (iv->powerLimit[0] != 0xffff) { // only set it, if it is changed by user. Default value in the html setup page is -1 = 0xffff
                        iv->powerLimit[1] = 0x0100; // set the limit as persistent
                        iv->devControlCmd = ActivePowerContr; // set active power limit
                        iv->devControlRequest = true; // set to true to update the active power limit from setup html page 
                        DPRINTLN(DBG_INFO, F("add inverter: ") + String(name) + ", SN: " + String(invSerial, HEX) + ", Power Limit: " + String(iv->powerLimit[0]));
                    }
                    for(uint8_t j = 0; j < 4; j++) {
                        mEep->read(ADDR_INV_CH_NAME + (i * 4 * MAX_NAME_LENGTH) + j * MAX_NAME_LENGTH, iv->chName[j], MAX_NAME_LENGTH);
                    }
                }


                mMqttInterval += mSendInterval;
            }
        }

        mEep->read(ADDR_INV_MAX_RTRY, &mMaxRetransPerPyld);
        if(0 == mMaxRetransPerPyld)
            mMaxRetransPerPyld = DEF_MAX_RETRANS_PER_PYLD;

        // pinout
        mEep->read(ADDR_PINOUT,   &mSys->Radio.pinCs);
        mEep->read(ADDR_PINOUT+1, &mSys->Radio.pinCe);
        mEep->read(ADDR_PINOUT+2, &mSys->Radio.pinIrq);
        if(mSys->Radio.pinCs == mSys->Radio.pinCe) {
            mSys->Radio.pinCs  = RF24_CS_PIN;
            mSys->Radio.pinCe  = RF24_CE_PIN;
            mSys->Radio.pinIrq = RF24_IRQ_PIN;
        }

        // nrf24 amplifier power
        mEep->read(ADDR_RF24_AMP_PWR, &mSys->Radio.AmplifierPower);

        // serial console
        uint8_t tmp;
        mEep->read(ADDR_SER_INTERVAL, &mSerialInterval);
        if(mSerialInterval < MIN_SERIAL_INTERVAL)
            mSerialInterval = MIN_SERIAL_INTERVAL;
        mEep->read(ADDR_SER_ENABLE, &tmp);
        mSerialValues = (tmp == 0x01);
        mEep->read(ADDR_SER_DEBUG, &tmp);
        mSerialDebug = (tmp == 0x01);
        mSys->Radio.mSerialDebug = mSerialDebug;

        // ntp
        char ntpAddr[NTP_ADDR_LEN];
        uint16_t ntpPort;
        mEep->read(ADDR_NTP_ADDR,   ntpAddr,    NTP_ADDR_LEN);
        mEep->read(ADDR_NTP_PORT,   &ntpPort);
        // TODO set ntpAddr & ntpPort in main

        // mqtt
        uint16_t mqttPort;
        char mqttAddr[MQTT_ADDR_LEN];
        char mqttUser[MQTT_USER_LEN];
        char mqttPwd[MQTT_PWD_LEN];
        char mqttTopic[MQTT_TOPIC_LEN];
        char mqttDevName[DEVNAME_LEN];
        mEep->read(ADDR_MQTT_ADDR,  mqttAddr,    MQTT_ADDR_LEN);
        mEep->read(ADDR_MQTT_USER,  mqttUser,    MQTT_USER_LEN);
        mEep->read(ADDR_MQTT_PWD,   mqttPwd,     MQTT_PWD_LEN);
        mEep->read(ADDR_MQTT_TOPIC, mqttTopic,   MQTT_TOPIC_LEN);
        mEep->read(ADDR_DEVNAME,    mqttDevName, DEVNAME_LEN);
        //mEep->read(ADDR_MQTT_INTERVAL, &mMqttInterval);
        mEep->read(ADDR_MQTT_PORT,  &mqttPort);

        if(mqttAddr[0] > 0) {
            mMqttActive = true;
            if(mMqttInterval < MIN_MQTT_INTERVAL)
                mMqttInterval = MIN_MQTT_INTERVAL;
        }
        else
            mMqttInterval = 0xffff;

        if(0 == mqttPort)
            mqttPort = 1883;

        mMqtt.setup(mqttAddr, mqttTopic, mqttUser, mqttPwd, mqttDevName, mqttPort);
        mMqtt.mClient->setCallback(std::bind(&app::cbMqtt, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        mMqttTicker = 0;

#ifdef __MQTT_TEST__
        // für mqtt test
        mMqttTicker = mMqttInterval -10;
#endif
        mSerialTicker = 0;

        if(mqttAddr[0] > 0) {
            char topic[30];
            mMqtt.sendMsg("device", mqttDevName);
            mMqtt.sendMsg("version", mVersion);
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
                iv = mSys->getInverterByPos(i);
                if(NULL != iv) {
                    for(uint8_t i = 0; i < 4; i++) {
                        if(0 != iv->chName[i][0]) {
                            snprintf(topic, 30, "%s/ch%d/%s", iv->name, i+1, "name");
                            mMqtt.sendMsg(topic, iv->chName[i]);
                            yield();
                        }
                    }
                }
            }
        }
    }
    else {
        DPRINTLN(DBG_DEBUG, F("CRC pos: ") + String(ADDR_SETTINGS_CRC));
        DPRINTLN(DBG_DEBUG, F("NXT pos: ") + String(ADDR_NEXT));
        DPRINTLN(DBG_INFO, F("Settings not valid, erasing ..."));
        eraseSettings();
        saveValues(false);
        delay(100);
        DPRINTLN(DBG_INFO, F("... restarting ..."));
        delay(100);
        ESP.restart();
    }

    mSys->setup();

    if(!mWifiSettingsValid)
        DPRINTLN(DBG_WARN, F("your settings are not valid! check [IP]/setup"));
    else {
        DPRINTLN(DBG_INFO, F("\n\n----------------------------------------"));
        DPRINTLN(DBG_INFO, F("Welcome to AHOY!"));
        DPRINT(DBG_INFO, F("\npoint your browser to http://"));
        if(mApActive)
            DBGPRINTLN(F("192.168.1.1"));
        else
            DBGPRINTLN(WiFi.localIP());
        DPRINTLN(DBG_INFO, F("to configure your device"));
        DPRINTLN(DBG_INFO, F("----------------------------------------\n"));
    }
}


//-----------------------------------------------------------------------------
void app::loop(void) {
    DPRINTLN(DBG_VERBOSE, F("app::loop"));
    Main::loop();
    
    mSys->Radio.loop();

    yield();

    if(checkTicker(&mRxTicker, 5)) {
        //DPRINTLN(DBG_VERBOSE, F("app_loops =") + String(app_loops));
        app_loops=0;
        DPRINT(DBG_VERBOSE, F("a"));

        bool rxRdy = mSys->Radio.switchRxCh();

        if(!mSys->BufCtrl.empty()) {
            uint8_t len;
            packet_t *p = mSys->BufCtrl.getBack();

            if(mSys->Radio.checkPaketCrc(p->packet, &len, p->rxCh)) {
                // process buffer only on first occurrence
                if(mSerialDebug) {
                    DPRINT(DBG_INFO, "RX " + String(len) + "B Ch" + String(p->rxCh) + " | ");
                    mSys->Radio.dumpBuf(NULL, p->packet, len);
                }
                mFrameCnt++;

                if(0 != len) {
                    Inverter<> *iv = mSys->findInverter(&p->packet[1]);
                    if(NULL != iv && p->packet[0] == (TX_REQ_INFO + 0x80)) { // response from get information command
                        DPRINTLN(DBG_DEBUG, F("Response from info request received"));
                        switch (mSys->InfoCmd){
                            case InverterDevInform_Simple:
                            {
                                DPRINT(DBG_INFO, "Response from inform simple\n");
                                mSys->InfoCmd = RealTimeRunData_Debug; // Set back to default
                                break;
                            }
                            case InverterDevInform_All:
                            {
                                DPRINT(DBG_INFO, "Response from inform all\n");
                                mSys->InfoCmd = RealTimeRunData_Debug; // Set back to default
                                break;
                            }
                            case GetLossRate:
                            {
                                DPRINT(DBG_INFO, "Response from get loss rate\n");
                                mSys->InfoCmd = RealTimeRunData_Debug; // Set back to default
                                break;
                            }
                            case AlarmData:
                            {
                                DPRINT(DBG_INFO, "Response from AlarmData\n");
                                mSys->InfoCmd = RealTimeRunData_Debug; // Set back to default
                                break;
                            }
                            case AlarmUpdate:
                            {
                                DPRINT(DBG_INFO, "Response from AlarmUpdate\n");
                                mSys->InfoCmd = RealTimeRunData_Debug; // Set back to default
                                break;
                            }
                            case RealTimeRunData_Debug:
                            {
                                uint8_t *pid = &p->packet[9];
                                if (*pid == 0x00) {
                                    DPRINT(DBG_DEBUG, "fragment number zero received and ignored");
                                } else {
                                    if((*pid & 0x7F) < 5) {
                                        memcpy(mPayload[iv->id].data[(*pid & 0x7F) - 1], &p->packet[10], len-11);
                                        mPayload[iv->id].len[(*pid & 0x7F) - 1] = len-11;
                                    }

                                    if((*pid & 0x80) == 0x80) { // Last packet
                                        if((*pid & 0x7f) > mPayload[iv->id].maxPackId) {
                                            mPayload[iv->id].maxPackId = (*pid & 0x7f);
                                            if(*pid > 0x81)
                                                mLastPacketId = *pid;
                                        }
                                    }
                                }
                                break;
                            }
                        }
                    }
                    if(NULL != iv && p->packet[0] == (TX_REQ_DEVCONTROL + 0x80)) { // response from dev control command
                        DPRINTLN(DBG_DEBUG, F("Response from devcontrol request received"));
                        iv->devControlRequest = false; 
                        switch (p->packet[12]){
                        case ActivePowerContr:
                            if (iv->devControlCmd >= ActivePowerContr && iv->devControlCmd <= PFSet){ // ok inverter accepted the set point copy it to dtu eeprom
                                if (iv->powerLimit[1]>0){ // User want to have it persistent
                                    mEep->write(ADDR_INV_PWR_LIM + iv->id * 2,iv->powerLimit[0]);
                                    updateCrc();
                                    mEep->commit();
                                    DPRINTLN(DBG_INFO, F("Inverter has accepted power limit set point, written to dtu eeprom"));    
                                } else {
                                    DPRINTLN(DBG_INFO, F("Inverter has accepted power limit set point"));
                                }
                                iv->devControlCmd = Init;
                            }
                            break;
                        default:
                            if (iv->devControlCmd == ActivePowerContr){
                            //case inverter did not accept the sent limit; set back to last stored limit
                            mEep->read(ADDR_INV_PWR_LIM + iv->id * 2, (uint16_t *)&(iv->powerLimit[0]));
                            DPRINTLN(DBG_INFO, F("Inverter has not accepted power limit set point"));    
                            }
                            iv->devControlCmd = Init;
                            break;
                        }
                    }
                }
            }
            mSys->BufCtrl.popBack();
        }
        yield();


        if(rxRdy) {
            processPayload(true);
        }
    }

    if(mMqttActive)
        mMqtt.loop();

    if(checkTicker(&mTicker, 1000)) {
        if((++mMqttTicker >= mMqttInterval) && (mMqttInterval != 0xffff)) {
            mMqttTicker = 0;
            mMqtt.isConnected(true);
            char topic[30], val[10];
            for(uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                Inverter<> *iv = mSys->getInverterByPos(id);
                if(NULL != iv) {
                    if(iv->isAvailable(mTimestamp)) {
                        for(uint8_t i = 0; i < iv->listLen; i++) {
                            snprintf(topic, 30, "%s/ch%d/%s", iv->name, iv->assign[i].ch, fields[iv->assign[i].fieldId]);
                            snprintf(val, 10, "%.3f", iv->getValue(i));
                            mMqtt.sendMsg(topic, val);
                            yield();
                        }
                    }
                }
            }
            snprintf(val, 10, "%ld", millis()/1000);
            
#ifndef __MQTT_NO_DISCOVERCONFIG__
            // MQTTDiscoveryConfig nur wenn nicht abgeschaltet.
            sendMqttDiscoveryConfig();
#endif            
            mMqtt.sendMsg("uptime", val);

#ifdef __MQTT_TEST__
            // für einfacheren Test mit MQTT, den MQTT abschnitt in 10 Sekunden wieder ausführen
            mMqttTicker = mMqttInterval -10;
#endif
        }

        if(mSerialValues) {
            if(++mSerialTicker >= mSerialInterval) {
                mSerialTicker = 0;
                char topic[30], val[10];
                for(uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                    Inverter<> *iv = mSys->getInverterByPos(id);
                    if(NULL != iv) {
                        if(iv->isAvailable(mTimestamp)) {
                            DPRINTLN(DBG_INFO, "Inverter: " + String(id));
                            for(uint8_t i = 0; i < iv->listLen; i++) {
                                if(0.0f != iv->getValue(i)) {
                                    snprintf(topic, 30, "%s/ch%d/%s", iv->name, iv->assign[i].ch, iv->getFieldName(i));
                                    snprintf(val, 10, "%.3f %s", iv->getValue(i), iv->getUnit(i));
                                    DPRINTLN(DBG_INFO, String(topic) + ": " + String(val));
                                }
                                yield();
                            }
                            DPRINTLN(DBG_INFO, "");                            
                        }
                    }
                }
            }
        }

        if(++mSendTicker >= mSendInterval) {
            mSendTicker = 0;

            if(0 != mTimestamp) {
                if(mSerialDebug)
                    DPRINTLN(DBG_DEBUG, F("Free heap: 0x") + String(ESP.getFreeHeap(), HEX));

                if(!mSys->BufCtrl.empty()) {
                    if(mSerialDebug)
                        DPRINTLN(DBG_DEBUG, F("recbuf not empty! #") + String(mSys->BufCtrl.getFill()));
                }

                int8_t maxLoop = MAX_NUM_INVERTERS;
                Inverter<> *iv = mSys->getInverterByPos(mSendLastIvId);
                do {
                    if(NULL != iv)
                        mPayload[iv->id].requested = false;
                    mSendLastIvId = ((MAX_NUM_INVERTERS-1) == mSendLastIvId) ? 0 : mSendLastIvId + 1;
                    iv = mSys->getInverterByPos(mSendLastIvId);
                } while((NULL == iv) && ((maxLoop--) > 0));

                if(NULL != iv) {
                    if(!mPayload[iv->id].complete)
                        processPayload(false);

                    if(!mPayload[iv->id].complete) {
                        mRxFailed++;
                        if(mSerialDebug) {
                            DPRINT(DBG_INFO, F("Inverter #") + String(iv->id) + " ");
                            DPRINTLN(DBG_INFO, F("no Payload received! (retransmits: ") + String(mPayload[iv->id].retransmits) + ")");
                        }
                    }

                    // reset payload data
                    memset(mPayload[iv->id].len, 0, MAX_PAYLOAD_ENTRIES);
                    mPayload[iv->id].retransmits = 0;
                    mPayload[iv->id].maxPackId = 0;
                    mPayload[iv->id].complete  = false;
                    mPayload[iv->id].requested = true;
                    mPayload[iv->id].ts = mTimestamp;

                    yield();
                    if(mSerialDebug)
                        DPRINTLN(DBG_DEBUG, F("app:loop WiFi WiFi.status ") + String(WiFi.status()) );
                        DPRINTLN(DBG_INFO, F("Requesting Inverter SN ") + String(iv->serial.u64, HEX));
                    if(iv->devControlRequest && iv->powerLimit[0] > 0){ // prevent to "switch off"
                        if(mSerialDebug)
                            DPRINTLN(DBG_INFO, F("Devcontrol request ") + String(iv->devControlCmd) + F(" power limit ") + String(iv->powerLimit[0]));
                        mSys->Radio.sendControlPacket(iv->radioId.u64,iv->devControlCmd ,iv->powerLimit);
                    } else {
                        mSys->Radio.sendTimePacket(iv->radioId.u64, mSys->InfoCmd, mPayload[iv->id].ts,iv->alarmMesIndex);
                        mRxTicker = 0;
                    }
                }
            }
            else if(mSerialDebug)
                DPRINTLN(DBG_WARN, F("time not set, can't request inverter!"));
            yield();
        }
    }
}


//-----------------------------------------------------------------------------
void app::handleIntr(void) {
    DPRINTLN(DBG_VERBOSE, F("app::handleIntr"));
    mSys->Radio.handleIntr();
}


//-----------------------------------------------------------------------------
bool app::buildPayload(uint8_t id) {
    DPRINTLN(DBG_VERBOSE, F("app::buildPayload"));
    uint16_t crc = 0xffff, crcRcv = 0x0000;
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
        yield();
    }
    if(crc == crcRcv)
        return true;
    return false;
}


//-----------------------------------------------------------------------------
void app::processPayload(bool retransmit) {

#ifdef __MQTT_AFTER_RX__
    boolean doMQTT = false;
#endif

    DPRINTLN(DBG_VERBOSE, F("app::processPayload"));
    for(uint8_t id = 0; id < mSys->getNumInverters(); id++) {
        Inverter<> *iv = mSys->getInverterByPos(id);
        if(NULL != iv) {
            if(!mPayload[iv->id].complete) {
                if(!buildPayload(iv->id)) {
                    if(mPayload[iv->id].requested) {
                        if(retransmit) {
                            if(mPayload[iv->id].retransmits < mMaxRetransPerPyld) {
                                mPayload[iv->id].retransmits++;
                                if(mPayload[iv->id].maxPackId != 0) {
                                    for(uint8_t i = 0; i < (mPayload[iv->id].maxPackId-1); i ++) {
                                        if(mPayload[iv->id].len[i] == 0) {
                                            if(mSerialDebug)
                                                DPRINTLN(DBG_ERROR, F("while retrieving data: Frame ") + String(i+1) + F(" missing: Request Retransmit"));
                                            mSys->Radio.sendCmdPacket(iv->radioId.u64, TX_REQ_INFO, (SINGLE_FRAME+i), true);
                                            break; // only retransmit one frame per loop
                                        }
                                        yield();
                                    }
                                }
                                else {
                                    if(mSerialDebug)
                                        DPRINTLN(DBG_ERROR, F("while retrieving data: last frame missing: Request Retransmit"));
                                    if(0x00 != mLastPacketId)
                                        mSys->Radio.sendCmdPacket(iv->radioId.u64, TX_REQ_INFO, mLastPacketId, true);
                                    else
                                        mSys->Radio.sendTimePacket(iv->radioId.u64, mSys->InfoCmd, mPayload[iv->id].ts,iv->alarmMesIndex);
                                }
                                mSys->Radio.switchRxCh(100);
                            }
                        }
                    }
                }
                else {
                    mPayload[iv->id].complete = true;
                    iv->ts = mPayload[iv->id].ts;
                    uint8_t payload[128] = {0};
                    uint8_t offs = 0;
                    for(uint8_t i = 0; i < (mPayload[iv->id].maxPackId); i ++) {
                        memcpy(&payload[offs], mPayload[iv->id].data[i], (mPayload[iv->id].len[i]));
                        offs += (mPayload[iv->id].len[i]);
                        yield();
                    }
                    offs-=2;
                    if(mSerialDebug) {
                        DPRINT(DBG_INFO, F("Payload (") + String(offs) + "): ");
                        mSys->Radio.dumpBuf(NULL, payload, offs);
                    }
                    mRxSuccess++;

                    for(uint8_t i = 0; i < iv->listLen; i++) {
                        iv->addValue(i, payload);
                        yield();
                    }
                    iv->doCalculations();

#ifdef __MQTT_AFTER_RX__
                    doMQTT = true;
#endif

                }
            }
            yield();
        }
    }

#ifdef __MQTT_AFTER_RX__
    //  ist MQTT aktiviert und es wurden Daten vom einem oder mehreren WR aufbereitet ( doMQTT = true) 
    //  dann die den mMqttTicker auf mMqttIntervall -2 setzen, also  
    //  MQTT aussenden in 2 sek aktivieren 
    //  dies sollte noch über einen Schalter im Setup aktivier / deaktivierbar gemacht werden
    if( (mMqttInterval != 0xffff) && doMQTT ) {
        ++mMqttTicker = mMqttInterval -2;
        DPRINT(DBG_DEBUG, F("MQTTticker auf Intervall -2 sec ")) ;
    }
#endif
}


//-----------------------------------------------------------------------------
void app::showIndex(void) {
    DPRINTLN(DBG_VERBOSE, F("app::showIndex"));
    String html = FPSTR(index_html);
    html.replace(F("{DEVICE}"), mDeviceName);
    html.replace(F("{VERSION}"), mVersion);
    html.replace(F("{TS}"), String(mSendInterval) + " ");
    html.replace(F("{JS_TS}"), String(mSendInterval * 1000));
    html.replace(F("{BUILD}"), String(AUTO_GIT_HASH));
    mWeb->send(200, "text/html", html);
}


//-----------------------------------------------------------------------------
void app::showSetup(void) {
    DPRINTLN(DBG_VERBOSE, F("app::showSetup"));
    // overrides same method in main.cpp

    String html = FPSTR(setup_html);
    html.replace(F("{SSID}"), mStationSsid);
    // PWD will be left at the default value (for protection)
    // -> the PWD will only be changed if it does not match the placeholder "{PWD}"

    html.replace(F("{DEVICE}"), String(mDeviceName));
    html.replace(F("{VERSION}"), String(mVersion));
    if(mApActive)
        html.replace(F("{IP}"), String(F("http://192.168.1.1")));
    else
        html.replace(F("{IP}"), ("http://" + String(WiFi.localIP().toString())));

    String inv;
    uint64_t invSerial;
    char name[MAX_NAME_LENGTH + 1] = {0};
    uint16_t modPwr[4];
    uint16_t invActivePowerLimit = -1;
    for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
        mEep->read(ADDR_INV_ADDR + (i * 8),               &invSerial);
        mEep->read(ADDR_INV_NAME + (i * MAX_NAME_LENGTH), name, MAX_NAME_LENGTH);
        mEep->read(ADDR_INV_CH_PWR + (i * 2 * 4), modPwr, 4);
        mEep->read(ADDR_INV_PWR_LIM + (i * 2),(uint16_t *) &invActivePowerLimit);
        inv += F("<p class=\"subdes\">Inverter ") + String(i) + "</p>";

        inv += F("<label for=\"inv") + String(i) + F("Addr\">Address</label>");
        inv += F("<input type=\"text\" class=\"text\" name=\"inv") + String(i) + F("Addr\" value=\"");
        if(0ULL != invSerial)
            inv += String(invSerial, HEX);
        inv += F("\"/ maxlength=\"12\" onkeyup=\"checkSerial()\">");

        inv += F("<label for=\"inv") + String(i) + F("Name\">Name</label>");
        inv += F("<input type=\"text\" class=\"text\" name=\"inv") + String(i) + F("Name\" value=\"");
        inv += String(name);
        inv += F("\"/ maxlength=\"") + String(MAX_NAME_LENGTH) + "\">";

        inv += F("<label for=\"inv") + String(i) + F("ActivePowerLimit\">Active Power Limit (W)</label>");
        inv += F("<input type=\"text\" class=\"text\" name=\"inv") + String(i) + F("ActivePowerLimit\" value=\"");
        if (name[0] == 0){
            // If this value will be "saved" on next reboot the command to set the power limit will not be executed.
            inv += String(65535); 
        } else {
            inv += String(invActivePowerLimit);
        }
        inv += F("\"/ maxlength=\"") + String(6) + "\">";


        inv += F("<label for=\"inv") + String(i) + F("ModPwr0\" name=\"lbl") + String(i);
        inv += F("ModPwr\">Max Module Power (Wp)</label>");
        for(uint8_t j = 0; j < 4; j++) {
            inv += F("<input type=\"text\" class=\"text sh\" name=\"inv") + String(i) + F("ModPwr") + String(j) + F("\" value=\"");
            inv += String(modPwr[j]);
            inv += F("\"/ maxlength=\"4\">");
        }
        inv += F("<br/><label for=\"inv") + String(i) + F("ModName0\" name=\"lbl") + String(i);
        inv += F("ModName\">Module Name</label>");
        for(uint8_t j = 0; j < 4; j++) {
            mEep->read(ADDR_INV_CH_NAME + (i * 4 * MAX_NAME_LENGTH) + j * MAX_NAME_LENGTH, name, MAX_NAME_LENGTH);
            inv += F("<input type=\"text\" class=\"text sh\" name=\"inv") + String(i) + F("ModName") + String(j) + F("\" value=\"");
            inv += String(name);
            inv += F("\"/ maxlength=\"") + String(MAX_NAME_LENGTH) + "\">";
        }
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
                default: if(j == mSys->Radio.pinCs)  pinout += F(" selected"); break;
                case 1:  if(j == mSys->Radio.pinCe)  pinout += F(" selected"); break;
                case 2:  if(j == mSys->Radio.pinIrq) pinout += F(" selected"); break;
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
        if(i == mSys->Radio.AmplifierPower)
            rf24 += F(" selected");
        rf24 += ">" + String(rf24AmpPower[i]) + F("</option>");
    }
    html.replace(F("{RF24}"), String(rf24));


    if(mSettingsValid) {
        html.replace(F("{INV_INTVL}"), String(mSendInterval));
        html.replace(F("{INV_RETRIES}"), String(mMaxRetransPerPyld));

        uint8_t tmp;
        mEep->read(ADDR_SER_ENABLE, &tmp);
        html.replace(F("{SER_INTVL}"), String(mSerialInterval));
        html.replace(F("{SER_VAL_CB}"), (tmp == 0x01) ? "checked" : "");
        mEep->read(ADDR_SER_DEBUG, &tmp);
        html.replace(F("{SER_DBG_CB}"), (tmp == 0x01) ? "checked" : "");

        char ntpAddr[NTP_ADDR_LEN] = {0};
        uint16_t ntpPort;
        mEep->read(ADDR_NTP_ADDR,      ntpAddr, NTP_ADDR_LEN);
        mEep->read(ADDR_NTP_PORT,      &ntpPort);
        html.replace(F("{NTP_ADDR}"),  String(ntpAddr));
        html.replace(F("{NTP_PORT}"),  String(ntpPort));

        char mqttAddr[MQTT_ADDR_LEN] = {0};
        uint16_t mqttPort;
        mEep->read(ADDR_MQTT_ADDR,     mqttAddr, MQTT_ADDR_LEN);
        mEep->read(ADDR_MQTT_PORT,     &mqttPort);

        html.replace(F("{MQTT_ADDR}"),  String(mqttAddr));
        html.replace(F("{MQTT_PORT}"),  String(mMqtt.getPort()));
        html.replace(F("{MQTT_USER}"),  String(mMqtt.getUser()));
        html.replace(F("{MQTT_PWD}"),   String(mMqtt.getPwd()));
        html.replace(F("{MQTT_TOPIC}"), String(mMqtt.getTopic()));
        html.replace(F("{MQTT_INTVL}"), String(mMqttInterval));
    }

    mWeb->send(200, F("text/html"), html);
}


//-----------------------------------------------------------------------------
void app::showSave(void) {
    DPRINTLN(DBG_VERBOSE, F("app::showSave"));
    saveValues(true);
}


//-----------------------------------------------------------------------------
void app::showErase() {
    DPRINTLN(DBG_VERBOSE, F("app::showErase"));
    eraseSettings();
    showReboot();
}

//-----------------------------------------------------------------------------
void app::cbMqtt(char* topic, byte* payload, unsigned int length) {
    // callback handling on subscribed devcontrol topic
    DPRINTLN(DBG_INFO, F("app::cbMqtt"));
    // subcribed topics are mTopic + "/devcontrol/#" where # is <inverter_id>/<subcmd in dec>
    // eg. mypvsolar/devcontrol/1/11 with payload "400" --> inverter 1 active power limit 400 Watt
    const char *token = strtok(topic, "/");
    while (token != NULL)
    {   
        if (std::strcmp(token,"devcontrol")==0){
            token = strtok(NULL, "/");
            uint8_t iv_id = std::stoi(token);
            if (iv_id >= 0  && iv_id <= MAX_NUM_INVERTERS){
                Inverter<> *iv = this->mSys->getInverterByPos(iv_id);
                if(NULL != iv) {
                    if (!iv->devControlRequest) { // still pending
                        token = strtok(NULL, "/");
                        switch ( std::stoi(token) ){
                            case ActivePowerContr:  // Active Power Control
                                if (true){ // if (std::stoi((char*)payload) > 0) error handling powerlimit needed?
                                    iv->devControlCmd = ActivePowerContr;
                                    iv->powerLimit[0] = std::stoi((char*)payload);
                                    iv->powerLimit[1] = 0x0000; // if power limit is set via external interface --> set it temporay
                                    DPRINTLN(DBG_DEBUG, F("Power limit for inverter ") + String(iv->id) + F(" set to ") + String(iv->powerLimit[0]) + F("W") );
                                }
                                iv->devControlRequest = true;
                                break;
                            case TurnOn: // Turn On
                                iv->devControlCmd = TurnOn;
                                DPRINTLN(DBG_INFO, F("Turn on inverter ") + String(iv->id) );
                                iv->devControlRequest = true;
                                break;
                            case TurnOff: // Turn Off
                                iv->devControlCmd = TurnOff;
                                DPRINTLN(DBG_INFO, F("Turn off inverter ") + String(iv->id) );
                                iv->devControlRequest = true;
                                break;
                            case Restart: // Restart
                                iv->devControlCmd = Restart;
                                DPRINTLN(DBG_INFO, F("Restart inverter ") + String(iv->id) );
                                iv->devControlRequest = true;
                                break;
                            case ReactivePowerContr: // Reactive Power Control
                                iv->devControlCmd = ReactivePowerContr;
                                if (true){ // if (std::stoi((char*)payload) > 0) error handling powerlimit needed?
                                    iv->devControlCmd = ReactivePowerContr;
                                    iv->powerLimit[0] = std::stoi((char*)payload);
                                    iv->powerLimit[1] = 0x0000; // if reactivepower limit is set via external interface --> set it temporay
                                    DPRINTLN(DBG_DEBUG, F("Reactivepower limit for inverter ") + String(iv->id) + F(" set to ") + String(iv->powerLimit[0]) + F("W") );
                                    iv->devControlRequest = true;
                                }
                                break;
                            case PFSet: // Set Power Factor
                                // iv->devControlCmd = PFSet;
                                // uint16_t power_factor = std::stoi(strtok(NULL, "/"));
                                DPRINTLN(DBG_INFO, F("Set Power Factor not implemented for inverter ") + String(iv->id) );
                                break;
                            default:
                                DPRINTLN(DBG_INFO, "Not implemented");
                                break;
                        }
                    }
                }
            }
            break;
        }
        token = strtok(NULL, "/");
    }
    DPRINTLN(DBG_INFO, F("app::cbMqtt finished"));
}



//-----------------------------------------------------------------------------
void app::showStatistics(void) {
    DPRINTLN(DBG_VERBOSE, F("app::showStatistics"));
    String content = F("Receive success: ") + String(mRxSuccess) + "\n";
    content += F("Receive fail: ") + String(mRxFailed) + "\n";
    content += F("Frames received: ") + String(mFrameCnt) + "\n";
    content += F("Send Cnt: ") + String(mSys->Radio.mSendCnt) + String("\n\n");

    Inverter<> *iv;
    for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
        iv = mSys->getInverterByPos(i);
        if(NULL != iv) {
            bool avail = true;
            content += F("Inverter '") + String(iv->name) + F("' is ");
            if(!iv->isAvailable(mTimestamp)) {
                content += F("not ");
                avail = false;
            }
            content += F("available and is ");
            if(!iv->isProducing(mTimestamp))
                content += F("not ");
            content += F("producing\n");

            if(!avail) {
                if(iv->getLastTs() > 0)
                    content += F("-> last successful transmission: ") + getDateTimeStr(iv->getLastTs()) + "\n";
            }
        }
        else {
            content += F("Inverter ") + String(i) + F(" not (correctly) configured\n");
        }
    }

    if(!mSys->Radio.isChipConnected())
        content += F("WARNING! your NRF24 module can't be reached, check the wiring and pinout (<a href=\"/setup\">setup</a>)\n");

    if(mShowRebootRequest)
        content += F("INFO: reboot your ESP to apply all your configuration changes!\n");

    if(!mSettingsValid)
        content += F("INFO: your settings are invalid, please switch to <a href=\"/setup\">Setup</a> to correct this.\n");

    content += F("MQTT: ");
    if(!mMqtt.isConnected())
        content += F("not ");
    content += F("connected\n");

    mWeb->send(200, F("text/plain"), content);
}

//-----------------------------------------------------------------------------
void app::webapi(void) { // ToDo
    DPRINTLN(DBG_VERBOSE, F("app::api"));
    DPRINTLN(DBG_DEBUG, mWeb->arg("plain"));
    const size_t capacity = 200; // Use arduinojson.org/assistant to compute the capacity.
    DynamicJsonDocument payload(capacity);
  
   // Parse JSON object
    deserializeJson(payload, mWeb->arg("plain"));
    // ToDo: error handling for payload
    if (payload["tx_request"] == TX_REQ_INFO){
        mSys->InfoCmd = payload["cmd"];
        DPRINTLN(DBG_INFO, F("Will make tx-request 0x15 with subcmd ") + String(mSys->InfoCmd));
    }
    mWeb->send ( 200, "text/json", "{success:true}" );
}


//-----------------------------------------------------------------------------
void app::showHoymiles(void) {
    DPRINTLN(DBG_VERBOSE, F("app::showHoymiles"));
    String html = FPSTR(hoymiles_html);
    html.replace(F("{DEVICE}"), mDeviceName);
    html.replace(F("{VERSION}"), mVersion);
    html.replace(F("{TS}"), String(mSendInterval) + " ");
    html.replace(F("{JS_TS}"), String(mSendInterval * 1000));
    mWeb->send(200, F("text/html"), html);
}


//-----------------------------------------------------------------------------
void app::showFavicon(void) {
    DPRINTLN(DBG_VERBOSE, F("app::showFavicon"));
    static const char favicon_type[] PROGMEM = "image/x-icon";
    static const char favicon_content[] PROGMEM = FAVICON_PANEL_16;
    mWeb->send_P(200, favicon_type, favicon_content, sizeof(favicon_content));
}


//-----------------------------------------------------------------------------
void app::showLiveData(void) {
    DPRINTLN(DBG_VERBOSE, F("app::showLiveData"));
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

            modHtml += F("<div class=\"iv\">"
                    "<div class=\"ch-iv\"><span class=\"head\">") + String(iv->name) + F(" Limit ") + String(iv->powerLimit[0]) + F(" W</span>");
            uint8_t list[] = {FLD_UAC, FLD_IAC, FLD_PAC, FLD_F, FLD_PCT, FLD_T, FLD_YT, FLD_YD, FLD_PDC, FLD_EFF, FLD_PRA, FLD_ALARM_MES_ID};

            for(uint8_t fld = 0; fld < 12; fld++) {
                pos = (iv->getPosByChFld(CH0, list[fld]));
                if(0xff != pos) {
                    modHtml += F("<div class=\"subgrp\">");
                    modHtml += F("<span class=\"value\">") + String(iv->getValue(pos));
                    modHtml += F("<span class=\"unit\">") + String(iv->getUnit(pos)) + F("</span></span>");
                    modHtml += F("<span class=\"info\">") + String(iv->getFieldName(pos)) + F("</span>");
                    modHtml += F("</div>");
                }
            }
            modHtml += "</div>";

            for(uint8_t ch = 1; ch <= modNum; ch ++) {
                modHtml += F("<div class=\"ch\"><span class=\"head\">");
                if(iv->chName[ch-1][0] == 0)
                    modHtml += F("CHANNEL ") + String(ch);
                else
                    modHtml += String(iv->chName[ch-1]);
                modHtml += F("</span>");
                for(uint8_t j = 0; j < 6; j++) {
                    switch(j) {
                        default: pos = (iv->getPosByChFld(ch, FLD_UDC)); break;
                        case 1:  pos = (iv->getPosByChFld(ch, FLD_IDC)); break;
                        case 2:  pos = (iv->getPosByChFld(ch, FLD_PDC)); break;
                        case 3:  pos = (iv->getPosByChFld(ch, FLD_YD));  break;
                        case 4:  pos = (iv->getPosByChFld(ch, FLD_YT));  break;
                        case 5:  pos = (iv->getPosByChFld(ch, FLD_IRR));  break;
                    }
                    if(0xff != pos) {
                        modHtml += F("<span class=\"value\">") + String(iv->getValue(pos));
                        modHtml += F("<span class=\"unit\">") + String(iv->getUnit(pos)) + F("</span></span>");
                        modHtml += F("<span class=\"info\">") + String(iv->getFieldName(pos)) + F("</span>");
                    }
                }
                modHtml += "</div>";
                yield();
            }
            modHtml += F("<div class=\"ts\">Last received data requested at: ") + getDateTimeStr(iv->ts) + F("</div>");
            modHtml += F("</div>");
#else
            // dump all data to web frontend
            modHtml = F("<pre>");
            char topic[30], val[10];
            for(uint8_t i = 0; i < iv->listLen; i++) {
                snprintf(topic, 30, "%s/ch%d/%s", iv->name, iv->assign[i].ch, iv->getFieldName(i));
                snprintf(val, 10, "%.3f %s", iv->getValue(i), iv->getUnit(i));
                modHtml += String(topic) + ": " + String(val) + "\n";
            }
            modHtml += F("</pre>");
#endif
        }
    }
    mWeb->send(200, F("text/html"), modHtml);
}


//-----------------------------------------------------------------------------
void app::showJSON(void) {
    DPRINTLN(DBG_VERBOSE, F("app::showJSON"));
    String modJson;

    modJson = F("{\n");
    for(uint8_t id = 0; id < mSys->getNumInverters(); id++) {
        Inverter<> *iv = mSys->getInverterByPos(id);
        if(NULL != iv) {
            char topic[40], val[25];
            snprintf(topic, 30, "\"%s\": {\n", iv->name);
            modJson += String(topic);
            for(uint8_t i = 0; i < iv->listLen; i++) {
                snprintf(topic, 40, "\t\"ch%d/%s\"", iv->assign[i].ch, iv->getFieldName(i));
                snprintf(val, 25, "[%.3f, \"%s\"]", iv->getValue(i), iv->getUnit(i));
                modJson += String(topic) + ": " + String(val) + F(",\n");
            }
            modJson += F("\t\"last_msg\": \"") + getDateTimeStr(iv->ts) + F("\"\n\t},\n");
        }
    }
    modJson += F("\"json_ts\": \"") + String(getDateTimeStr(mTimestamp)) + F("\"\n}\n");

    // mWeb->send(200, F("text/json"), modJson);
    mWeb->send(200, F("application/json"), modJson); // the preferred content-type (https://stackoverflow.com/questions/22406077/what-is-the-exact-difference-between-content-type-text-json-and-application-jso)
}


//-----------------------------------------------------------------------------
void app::saveValues(bool webSend = true) {
    DPRINTLN(DBG_VERBOSE, F("app::saveValues"));
    Main::saveValues(false); // general configuration

    if(mWeb->args() > 0) {
        char buf[20] = {0};
        uint8_t i = 0;
        uint16_t interval;
        uint16_t activepowerlimit=-1;

        // inverter
        serial_u addr;
        for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
            // address
            mWeb->arg("inv" + String(i) + "Addr").toCharArray(buf, 20);
            if(strlen(buf) == 0)
                memset(buf, 0, 20);
            addr.u64 = Serial2u64(buf);
            mEep->write(ADDR_INV_ADDR + (i * 8), addr.u64);

            // active power limit
            activepowerlimit = mWeb->arg("inv" + String(i) + "ActivePowerLimit").toInt();
            if (activepowerlimit != 0xffff && activepowerlimit > 0) {
                mEep->write(ADDR_INV_PWR_LIM + i * 2,activepowerlimit);
            }

            // name
            mWeb->arg("inv" + String(i) + "Name").toCharArray(buf, 20);
            mEep->write(ADDR_INV_NAME + (i * MAX_NAME_LENGTH), buf, MAX_NAME_LENGTH);

            // max channel power / name
            for(uint8_t j = 0; j < 4; j++) {
                uint16_t pwr = mWeb->arg("inv" + String(i) + "ModPwr" + String(j)).toInt();
                mEep->write(ADDR_INV_CH_PWR + (i * 2 * 4) + (j*2), pwr);
                memset(buf, 0, 20);
                mWeb->arg("inv" + String(i) + "ModName" + String(j)).toCharArray(buf, 20);
                mEep->write(ADDR_INV_CH_NAME + (i * 4 * MAX_NAME_LENGTH) + j * MAX_NAME_LENGTH, buf, MAX_NAME_LENGTH);
            }
        }

        interval = mWeb->arg("invInterval").toInt();
        mEep->write(ADDR_INV_INTERVAL, interval);
        i = mWeb->arg("invRetry").toInt();
        mEep->write(ADDR_INV_MAX_RTRY, i);


        // pinout
        for(uint8_t i = 0; i < 3; i ++) {
            uint8_t pin = mWeb->arg(String(pinArgNames[i])).toInt();
            mEep->write(ADDR_PINOUT + i, pin);
        }


        // nrf24 amplifier power
        mSys->Radio.AmplifierPower = mWeb->arg("rf24Power").toInt() & 0x03;
        mEep->write(ADDR_RF24_AMP_PWR, mSys->Radio.AmplifierPower);

        // ntp
        char ntpAddr[NTP_ADDR_LEN] = {0};
        uint16_t ntpPort;
        mWeb->arg("ntpAddr").toCharArray(ntpAddr, NTP_ADDR_LEN);
        ntpPort = mWeb->arg("ntpPort").toInt();
        mEep->write(ADDR_NTP_ADDR, ntpAddr, NTP_ADDR_LEN);
        mEep->write(ADDR_NTP_PORT, ntpPort);

        // mqtt
        char mqttAddr[MQTT_ADDR_LEN] = {0};
        uint16_t mqttPort;
        char mqttUser[MQTT_USER_LEN];
        char mqttPwd[MQTT_PWD_LEN];
        char mqttTopic[MQTT_TOPIC_LEN];
        mWeb->arg("mqttAddr").toCharArray(mqttAddr, MQTT_ADDR_LEN);
        mWeb->arg("mqttUser").toCharArray(mqttUser, MQTT_USER_LEN);
        mWeb->arg("mqttPwd").toCharArray(mqttPwd, MQTT_PWD_LEN);
        mWeb->arg("mqttTopic").toCharArray(mqttTopic, MQTT_TOPIC_LEN);
        //interval = mWeb->arg("mqttIntvl").toInt();
        mqttPort = mWeb->arg("mqttPort").toInt();
        mEep->write(ADDR_MQTT_ADDR, mqttAddr, MQTT_ADDR_LEN);
        mEep->write(ADDR_MQTT_PORT, mqttPort);
        mEep->write(ADDR_MQTT_USER, mqttUser, MQTT_USER_LEN);
        mEep->write(ADDR_MQTT_PWD,  mqttPwd,  MQTT_PWD_LEN);
        mEep->write(ADDR_MQTT_TOPIC, mqttTopic, MQTT_TOPIC_LEN);
        //mEep->write(ADDR_MQTT_INTERVAL, interval);


        // serial console
        bool tmp;
        interval = mWeb->arg("serIntvl").toInt();
        mEep->write(ADDR_SER_INTERVAL, interval);
        tmp = (mWeb->arg("serEn") == "on");
        mEep->write(ADDR_SER_ENABLE, (uint8_t)((tmp) ? 0x01 : 0x00));
        mSerialDebug = (mWeb->arg("serDbg") == "on");
        mEep->write(ADDR_SER_DEBUG, (uint8_t)((mSerialDebug) ? 0x01 : 0x00));
        DPRINT(DBG_INFO, "Serial debug is ");
        if(mSerialDebug) DPRINTLN(DBG_INFO, "on"); else DPRINTLN(DBG_INFO, "off");
        mSys->Radio.mSerialDebug = mSerialDebug;

        updateCrc();
        mEep->commit();
        if((mWeb->arg("reboot") == "on"))
            showReboot();
        else {
            mShowRebootRequest = true;
            mWeb->send(200, F("text/html"), F("<!doctype html><html><head><title>Setup saved</title><meta http-equiv=\"refresh\" content=\"1; URL=/setup\"></head><body>"
                "<p>saved</p></body></html>"));
        }
    }
    else {
        updateCrc();
        mEep->commit();
        mWeb->send(200, F("text/html"), F("<!doctype html><html><head><title>Error</title><meta http-equiv=\"refresh\" content=\"3; URL=/setup\"></head><body>"
            "<p>Error while saving</p></body></html>"));
    }
}


//-----------------------------------------------------------------------------
void app::updateCrc(void) {
    DPRINTLN(DBG_VERBOSE, F("app::updateCrc"));
    Main::updateCrc();

    uint16_t crc;
    crc = buildEEpCrc(ADDR_START_SETTINGS, ((ADDR_NEXT) - (ADDR_START_SETTINGS)));
    DPRINTLN(DBG_DEBUG, F("new CRC: ") + String(crc, HEX));
    mEep->write(ADDR_SETTINGS_CRC, crc);
}

void app::sendMqttDiscoveryConfig(void) {
    DPRINTLN(DBG_VERBOSE, F("app::sendMqttDiscoveryConfig"));

    char stateTopic[64], discoveryTopic[64], buffer[512], name[32], uniq_id[32];
    for(uint8_t id = 0; id < mSys->getNumInverters(); id++) {
        Inverter<> *iv = mSys->getInverterByPos(id);
        if(NULL != iv) {
            if(iv->isAvailable(mTimestamp) && mMqttConfigSendState[id] != true) {
                DynamicJsonDocument deviceDoc(128);
                deviceDoc["name"] = iv->name;
                deviceDoc["ids"] = String(iv->serial.u64, HEX);
                deviceDoc["cu"] = F("http://") + String(WiFi.localIP().toString());
                JsonObject deviceObj = deviceDoc.as<JsonObject>();
                DynamicJsonDocument doc(384);

                for(uint8_t i = 0; i < iv->listLen; i++) {
                    if (iv->assign[i].ch == CH0) {
                        snprintf(name, 32, "%s %s", iv->name, iv->getFieldName(i));
                    } else {
                        snprintf(name, 32, "%s CH%d %s", iv->name, iv->assign[i].ch, iv->getFieldName(i));
                    }
                    snprintf(stateTopic, 64, "%s/%s/ch%d/%s", mMqtt.getTopic(), iv->name, iv->assign[i].ch, iv->getFieldName(i));
                    snprintf(discoveryTopic, 64, "%s/sensor/%s/ch%d_%s/config", MQTT_DISCOVERY_PREFIX, iv->name, iv->assign[i].ch, iv->getFieldName(i));
                    snprintf(uniq_id, 32, "ch%d_%s", iv->assign[i].ch, iv->getFieldName(i));
                    const char* devCls = getFieldDeviceClass(iv->assign[i].fieldId);
                    const char* stateCls = getFieldStateClass(iv->assign[i].fieldId);

                    doc["name"] = name;
                    doc["stat_t"]  = stateTopic;
                    doc["unit_of_meas"] = iv->getUnit(i);
                    doc["uniq_id"] = String(iv->serial.u64, HEX) + "_" + uniq_id;
                    doc["dev"] = deviceObj;
                    doc["exp_aft"] = mMqttInterval + 5; // add 5 sec if connection is bad or ESP too slow
                    if (devCls != NULL) {
                        doc["dev_cla"] = devCls;
                    }
                    if (stateCls != NULL) {
                        doc["stat_cla"] = stateCls;
                    }

                    serializeJson(doc, buffer);
                    mMqtt.sendMsg2(discoveryTopic, buffer, true);
                    doc.clear();

                    yield();
                }

                mMqttConfigSendState[id] = true;
            }
        }
    }
}

const char* app::getFieldDeviceClass(uint8_t fieldId) {
    uint8_t pos = 0;
    for(; pos < DEVICE_CLS_ASSIGN_LIST_LEN; pos++) {
        if(deviceFieldAssignment[pos].fieldId == fieldId)
            break;
    }
    return (pos >= DEVICE_CLS_ASSIGN_LIST_LEN) ? NULL : deviceClasses[deviceFieldAssignment[pos].deviceClsId];
}

const char* app::getFieldStateClass(uint8_t fieldId) {
    uint8_t pos = 0;
    for(; pos < DEVICE_CLS_ASSIGN_LIST_LEN; pos++) {
        if(deviceFieldAssignment[pos].fieldId == fieldId)
            break;
    }
    return (pos >= DEVICE_CLS_ASSIGN_LIST_LEN) ? NULL : stateClasses[deviceFieldAssignment[pos].stateClsId];
}
