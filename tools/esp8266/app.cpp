//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#if defined(ESP32) && defined(F)
  #undef F
  #define F(sl) (sl)
#endif

#include "app.h"
#include <ArduinoJson.h>


//-----------------------------------------------------------------------------
app::app() {
    Serial.begin(115200);
    DPRINTLN(DBG_VERBOSE, F("app::app"));
    mEep = new eep();
    mWifi = new ahoywifi(this, &mSysConfig, &mConfig);

    resetSystem();
   loadDefaultConfig();

    mSys = new HmSystemType();
}


//-----------------------------------------------------------------------------
void app::setup(uint32_t timeout) {
    DPRINTLN(DBG_VERBOSE, F("app::setup"));

    mWifiSettingsValid = checkEEpCrc(ADDR_START, ADDR_WIFI_CRC, ADDR_WIFI_CRC);
    mSettingsValid = checkEEpCrc(ADDR_START_SETTINGS, ((ADDR_NEXT)-(ADDR_START_SETTINGS)), ADDR_SETTINGS_CRC);
    loadEEpconfig();

    mWifi->setup(timeout, mWifiSettingsValid);

    #ifndef AP_ONLY
        setupMqtt();
    #endif
    mSys->setup(&mConfig);

    mWebInst = new web(this, &mSysConfig, &mConfig, mVersion);
    mWebInst->setup();
}

//-----------------------------------------------------------------------------
void app::loop(void) {
    DPRINTLN(DBG_VERBOSE, F("app::loop"));

    bool apActive = mWifi->loop();
    mWebInst->loop();

    if(checkTicker(&mUptimeTicker, mUptimeInterval)) {
        mUptimeSecs++;
        if(0 != mTimestamp)
            mTimestamp++;
        else {
            if(!apActive) {
                mTimestamp  = mWifi->getNtpTime();
                DPRINTLN(DBG_INFO, "[NTP]: " + getDateTimeStr(mTimestamp));
            }
        }
    }

    
    mSys->Radio.loop();

    yield();

    if(checkTicker(&mRxTicker, 5)) {
        bool rxRdy = mSys->Radio.switchRxCh();

        if(!mSys->BufCtrl.empty()) {
            uint8_t len;
            packet_t *p = mSys->BufCtrl.getBack();

            if(mSys->Radio.checkPaketCrc(p->packet, &len, p->rxCh)) {
                // process buffer only on first occurrence
                if(mConfig.serialDebug) {
                    DPRINT(DBG_INFO, "RX " + String(len) + "B Ch" + String(p->rxCh) + " | ");
                    mSys->Radio.dumpBuf(NULL, p->packet, len);
                }
                mFrameCnt++;

                if(0 != len) {
                    Inverter<> *iv = mSys->findInverter(&p->packet[1]);
                    if(NULL != iv && p->packet[0] == (TX_REQ_INFO + 0x80)) { // response from get information command
                        DPRINTLN(DBG_DEBUG, F("Response from info request received"));
                        uint8_t *pid = &p->packet[9];
                        if (*pid == 0x00)
                        {
                            DPRINT(DBG_DEBUG, "fragment number zero received and ignored");
                        }
                        else
                        {
                            if ((*pid & 0x7F) < 5)
                            {
                                memcpy(mPayload[iv->id].data[(*pid & 0x7F) - 1], &p->packet[10], len - 11);
                                mPayload[iv->id].len[(*pid & 0x7F) - 1] = len - 11;
                            }

                            if ((*pid & 0x80) == 0x80)
                            { // Last packet
                                if ((*pid & 0x7f) > mPayload[iv->id].maxPackId)
                                {
                                    mPayload[iv->id].maxPackId = (*pid & 0x7f);
                                    if (*pid > 0x81)
                                        mLastPacketId = *pid;
                                }
                            }
                        }
                    }
                    if(NULL != iv && p->packet[0] == (TX_REQ_DEVCONTROL + 0x80)) { // response from dev control command
                        DPRINTLN(DBG_DEBUG, F("Response from devcontrol request received"));
                        iv->devControlRequest = false; 
                        switch (p->packet[12]){
                        case ActivePowerContr:
                            if (iv->devControlCmd >= ActivePowerContr && iv->devControlCmd <= PFSet){ // ok inverter accepted the set point copy it to dtu eeprom
                                if ((iv->powerLimit[1] & 0xff00) >0){ // User want to have it persistent
                                    mEep->write(ADDR_INV_PWR_LIM + iv->id * 2,iv->powerLimit[0]);
                                    mEep->write(ADDR_INV_PWR_LIM_CON + iv->id * 2,iv->powerLimit[1]);
                                    updateCrc();
                                    mEep->commit();
                                    DPRINTLN(DBG_INFO, F("Inverter ") + String(iv->id) + F(" has accepted power limit set point ") + String(iv->powerLimit[0]) + F(" with PowerLimitControl ")  + String(iv->powerLimit[1]) + F(", written to dtu eeprom"));    
                                } else {
                                    DPRINTLN(DBG_INFO, F("Inverter ") + String(iv->id) + F(" has accepted power limit set point ") + String(iv->powerLimit[0]) + F(" with PowerLimitControl ")  + String(iv->powerLimit[1]));
                                }
                                iv->devControlCmd = Init;
                            }
                            break;
                        default:
                            if (iv->devControlCmd == ActivePowerContr){
                            //case inverter did not accept the sent limit; set back to last stored limit
                            mEep->read(ADDR_INV_PWR_LIM + iv->id * 2, (uint16_t *)&(iv->powerLimit[0]));
                            mEep->read(ADDR_INV_PWR_LIM_CON + iv->id * 2, (uint16_t *)&(iv->powerLimit[1]));
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
        if((++mMqttTicker >= mMqttInterval) && (mMqttInterval != 0xffff) && mMqttActive) {
            mMqttTicker = 0;
            mMqtt.isConnected(true); // really needed? See comment from HorstG-57 #176
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

        if(mConfig.serialShowIv) {
            if(++mSerialTicker >= mConfig.serialInterval) {
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

        if(++mSendTicker >= mConfig.sendInterval) {
            mSendTicker = 0;

            if(0 != mTimestamp) {
                if(mConfig.serialDebug)
                    DPRINTLN(DBG_DEBUG, F("Free heap: 0x") + String(ESP.getFreeHeap(), HEX));

                if(!mSys->BufCtrl.empty()) {
                    if(mConfig.serialDebug)
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
                        if(mConfig.serialDebug) {
                            DPRINT(DBG_INFO, F("Inverter #") + String(iv->id) + " ");
                            DPRINTLN(DBG_INFO, F("no Payload received! (retransmits: ") + String(mPayload[iv->id].retransmits) + ")");
                        }
                    }

                    resetPayload(iv);

                    yield();
                    if(mConfig.serialDebug)
                        DPRINTLN(DBG_DEBUG, F("app:loop WiFi WiFi.status ") + String(WiFi.status()) );
                        DPRINTLN(DBG_INFO, F("Requesting Inverter SN ") + String(iv->serial.u64, HEX));
                    if(iv->devControlRequest && iv->powerLimit[0] > 0){ // prevent to "switch off"
                        if(mConfig.serialDebug)
                            DPRINTLN(DBG_INFO, F("Devcontrol request ") + String(iv->devControlCmd) + F(" power limit ") + String(iv->powerLimit[0]));
                        mSys->Radio.sendControlPacket(iv->radioId.u64,iv->devControlCmd ,iv->powerLimit);
                        iv->enqueCommand<InfoCommand>(SystemConfigPara);
                    } else {
                        mSys->Radio.sendTimePacket(iv->radioId.u64,iv->getQueuedCmd(), mPayload[iv->id].ts,iv->alarmMesIndex);
                        mRxTicker = 0;
                    }
                }
            }
            else if(mConfig.serialDebug)
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
                crc = Hoymiles::crc16(mPayload[id].data[i], mPayload[id].len[i] - 2, crc);
                crcRcv = (mPayload[id].data[i][mPayload[id].len[i] - 2] << 8)
                    | (mPayload[id].data[i][mPayload[id].len[i] - 1]);
            }
            else
                crc = Hoymiles::crc16(mPayload[id].data[i], mPayload[id].len[i], crc);
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
                            if(mPayload[iv->id].retransmits < mConfig.maxRetransPerPyld) {
                                mPayload[iv->id].retransmits++;
                                if(mPayload[iv->id].maxPackId != 0) {
                                    for(uint8_t i = 0; i < (mPayload[iv->id].maxPackId-1); i ++) {
                                        if(mPayload[iv->id].len[i] == 0) {
                                            if(mConfig.serialDebug)
                                                DPRINTLN(DBG_ERROR, F("while retrieving data: Frame ") + String(i+1) + F(" missing: Request Retransmit"));
                                            mSys->Radio.sendCmdPacket(iv->radioId.u64, TX_REQ_INFO, (SINGLE_FRAME+i), true);
                                            break; // only retransmit one frame per loop
                                        }
                                        yield();
                                    }
                                }
                                else {
                                    if(mConfig.serialDebug)
                                        DPRINTLN(DBG_ERROR, F("while retrieving data: last frame missing: Request Retransmit"));
                                    if(0x00 != mLastPacketId)
                                        mSys->Radio.sendCmdPacket(iv->radioId.u64, TX_REQ_INFO, mLastPacketId, true);
                                    else
                                        mSys->Radio.sendTimePacket(iv->radioId.u64, iv->getQueuedCmd(), mPayload[iv->id].ts,iv->alarmMesIndex);
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
                    if(mConfig.serialDebug) {
                        DPRINT(DBG_INFO, F("Payload (") + String(offs) + "): ");
                        mSys->Radio.dumpBuf(NULL, payload, offs);
                    }
                    mRxSuccess++;
                    
                    iv->getAssignment(); // choose the parser
                    for(uint8_t i = 0; i < iv->listLen; i++) {
                        iv->addValue(i, payload); // cmd value decides which parser is used to decode payload
                        yield();
                    }
                    iv->doCalculations(); // cmd value decides which parser is used to decode payload

#ifdef __MQTT_AFTER_RX__
                    doMQTT = true;
#endif
                    iv->setQueuedCmdFinished();
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
void app::cbMqtt(char* topic, byte* payload, unsigned int length) {
    // callback handling on subscribed devcontrol topic
    DPRINTLN(DBG_INFO, F("app::cbMqtt"));
    // subcribed topics are mTopic + "/devcontrol/#" where # is <inverter_id>/<subcmd in dec>
    // eg. mypvsolar/devcontrol/1/11 with payload "400" --> inverter 1 active power limit 400 Watt
    const char *token = strtok(topic, "/");
    while (token != NULL)
    {   
        if (strcmp(token,"devcontrol")==0){
            token = strtok(NULL, "/");
            uint8_t iv_id = std::stoi(token);
            if (iv_id >= 0  && iv_id <= MAX_NUM_INVERTERS){
                Inverter<> *iv = this->mSys->getInverterByPos(iv_id);
                if(NULL != iv) {
                    if (!iv->devControlRequest) { // still pending
                        token = strtok(NULL, "/");
                        switch ( std::stoi(token) ){
                            case ActivePowerContr:  // Active Power Control
                                token = strtok(NULL, "/"); // get ControlMode aka "PowerPF.Desc" in DTU-Pro Code from topic string
                                if (token == NULL) // default via mqtt ommit the LimitControlMode
                                    iv->powerLimit[1] = AbsolutNonPersistent;
                                else
                                    iv->powerLimit[1] = std::stoi(token);
                                if (length<=5){ // if (std::stoi((char*)payload) > 0) more error handling powerlimit needed?
                                    if (iv->powerLimit[1] >= AbsolutNonPersistent && iv->powerLimit[1] <= RelativPersistent){
                                        iv->devControlCmd = ActivePowerContr;
                                        iv->powerLimit[0] = std::stoi(std::string((char*)payload, (unsigned int)length)); // THX to @silversurfer
                                        if (iv->powerLimit[1] & 0x0001)
                                            DPRINTLN(DBG_INFO, F("Power limit for inverter ") + String(iv->id) + F(" set to ") + String(iv->powerLimit[0]) + F("%") );    
                                        else
                                            DPRINTLN(DBG_INFO, F("Power limit for inverter ") + String(iv->id) + F(" set to ") + String(iv->powerLimit[0]) + F("W") );
                                    }
                                    iv->devControlRequest = true;
                                } else {
                                    DPRINTLN(DBG_INFO, F("Invalid mqtt payload recevied: ") + String((char*)payload));
                                }
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
                                    iv->powerLimit[0] = std::stoi(std::string((char*)payload, (unsigned int)length));
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
String app::getStatistics(void) {
    String content = F("Receive success: ") + String(mRxSuccess) + "\n";
    content += F("Receive fail: ") + String(mRxFailed) + "\n";
    content += F("Frames received: ") + String(mFrameCnt) + "\n";
    content += F("Send Cnt: ") + String(mSys->Radio.mSendCnt) + String("\n\n");

    Inverter<> *iv;
    for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
        iv = mSys->getInverterByPos(i);
        if(NULL != iv) {
            bool avail = true;
            content += F("Inverter '") + String(iv->name) + F(" (FW-Version: ") + String(iv->fwVersion) +F(")") + F("' is ");
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

    return content;
}



//-----------------------------------------------------------------------------
String app::getLiveData(void)
{   
    String modHtml;
    for (uint8_t id = 0; id < mSys->getNumInverters(); id++)
    {
        Inverter<> *iv = mSys->getInverterByPos(id);
        if (NULL != iv)
        {
#ifdef LIVEDATA_VISUALIZED
            uint8_t modNum, pos;
            switch (iv->type)
            {
            default:
            case INV_TYPE_1CH:
                modNum = 1;
                break;
            case INV_TYPE_2CH:
                modNum = 2;
                break;
            case INV_TYPE_4CH:
                modNum = 4;
                break;
            }

            modHtml += F("<div class=\"iv\">"
                         "<div class=\"ch-iv\"><span class=\"head\">") +
                       String(iv->name) + F(" Limit ") + String(iv->actPowerLimit);
            if (true)
            { // live Power Limit from inverter is always in %
                modHtml += F(" %</span>");
            }
            else
            {
                modHtml += F(" W</span>");
            }
            uint8_t list[] = {FLD_UAC, FLD_IAC, FLD_PAC, FLD_F, FLD_PCT, FLD_T, FLD_YT, FLD_YD, FLD_PDC, FLD_EFF, FLD_PRA, FLD_ALARM_MES_ID};

            for (uint8_t fld = 0; fld < 12; fld++)
            {
                pos = (iv->getPosByChFld(CH0, list[fld]));
                if (0xff != pos)
                {
                    modHtml += F("<div class=\"subgrp\">");
                    modHtml += F("<span class=\"value\">") + String(iv->getValue(pos));
                    modHtml += F("<span class=\"unit\">") + String(iv->getUnit(pos)) + F("</span></span>");
                    modHtml += F("<span class=\"info\">") + String(iv->getFieldName(pos)) + F("</span>");
                    modHtml += F("</div>");
                }
            }
            modHtml += "</div>";

            for (uint8_t ch = 1; ch <= modNum; ch++)
            {
                modHtml += F("<div class=\"ch\"><span class=\"head\">");
                if (iv->chName[ch - 1][0] == 0)
                    modHtml += F("CHANNEL ") + String(ch);
                else
                    modHtml += String(iv->chName[ch - 1]);
                modHtml += F("</span>");
                for (uint8_t j = 0; j < 6; j++)
                {
                    switch (j)
                    {
                    default:
                        pos = (iv->getPosByChFld(ch, FLD_UDC));
                        break;
                    case 1:
                        pos = (iv->getPosByChFld(ch, FLD_IDC));
                        break;
                    case 2:
                        pos = (iv->getPosByChFld(ch, FLD_PDC));
                        break;
                    case 3:
                        pos = (iv->getPosByChFld(ch, FLD_YD));
                        break;
                    case 4:
                        pos = (iv->getPosByChFld(ch, FLD_YT));
                        break;
                    case 5:
                        pos = (iv->getPosByChFld(ch, FLD_IRR));
                        break;
                    }
                    if (0xff != pos)
                    {
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
            for (uint8_t i = 0; i < iv->listLen; i++)
            {
                snprintf(topic, 30, "%s/ch%d/%s", iv->name, iv->assign[i].ch, iv->getFieldName(i));
                snprintf(val, 10, "%.3f %s", iv->getValue(i), iv->getUnit(i));
                modHtml += String(topic) + ": " + String(val) + "\n";
            }
            modHtml += F("</pre>");
#endif
        }
    }
    return modHtml;
}

//-----------------------------------------------------------------------------
String app::getJson(void) {
    DPRINTLN(DBG_VERBOSE, F("app::showJson"));
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

   return modJson;
}


//-----------------------------------------------------------------------------
bool app::getWifiApActive(void) {
    return mWifi->getApActive();
}


//-----------------------------------------------------------------------------
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
                deviceDoc["mf"] = "Hoymiles";
                deviceDoc["mdl"] = iv->name;
                JsonObject deviceObj = deviceDoc.as<JsonObject>();
                DynamicJsonDocument doc(384);

                for(uint8_t i = 0; i < iv->listLen; i++) {
                    if (iv->assign[i].ch == CH0) {
                        snprintf(name, 32, "%s %s", iv->name, iv->getFieldName(i));
                    } else {
                        snprintf(name, 32, "%s CH%d %s", iv->name, iv->assign[i].ch, iv->getFieldName(i));
                    }
                    snprintf(stateTopic, 64, "%s/%s/ch%d/%s", mConfig.mqtt.topic, iv->name, iv->assign[i].ch, iv->getFieldName(i));
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


//-----------------------------------------------------------------------------
const char* app::getFieldDeviceClass(uint8_t fieldId) {
    uint8_t pos = 0;
    for(; pos < DEVICE_CLS_ASSIGN_LIST_LEN; pos++) {
        if(deviceFieldAssignment[pos].fieldId == fieldId)
            break;
    }
    return (pos >= DEVICE_CLS_ASSIGN_LIST_LEN) ? NULL : deviceClasses[deviceFieldAssignment[pos].deviceClsId];
}


//-----------------------------------------------------------------------------
const char* app::getFieldStateClass(uint8_t fieldId) {
    uint8_t pos = 0;
    for(; pos < DEVICE_CLS_ASSIGN_LIST_LEN; pos++) {
        if(deviceFieldAssignment[pos].fieldId == fieldId)
            break;
    }
    return (pos >= DEVICE_CLS_ASSIGN_LIST_LEN) ? NULL : stateClasses[deviceFieldAssignment[pos].stateClsId];
}


//-----------------------------------------------------------------------------
void app::resetSystem(void) {
    mUptimeSecs     = 0;
    mUptimeTicker   = 0xffffffff;
    mUptimeInterval = 1000;

#ifdef AP_ONLY
    mTimestamp = 1;
#else
    mTimestamp = 0;
#endif

    mHeapStatCnt = 0;

    mSendTicker     = 0xffff;
    mMqttTicker     = 0xffff;
    mMqttInterval   = MQTT_INTERVAL;
    mSerialTicker   = 0xffff;
    mMqttActive     = false;

    mTicker = 0;
    mRxTicker = 0;

    mSendLastIvId = 0;

    mShowRebootRequest = false;


    memset(mPayload, 0, (MAX_NUM_INVERTERS * sizeof(invPayload_t)));
    mRxFailed     = 0;
    mRxSuccess    = 0;
    mFrameCnt     = 0;
    mLastPacketId = 0x00;
}


//-----------------------------------------------------------------------------
void app::loadDefaultConfig(void) {
    memset(&mSysConfig, 0, sizeof(sysConfig_t));
    memset(&mConfig, 0, sizeof(config_t));
    snprintf(mVersion, 12, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

    snprintf(mSysConfig.deviceName, DEVNAME_LEN, "%s", DEF_DEVICE_NAME);

    // wifi
    snprintf(mSysConfig.stationSsid, SSID_LEN, "%s", FB_WIFI_SSID);
    snprintf(mSysConfig.stationPwd, PWD_LEN, "%s", FB_WIFI_PWD);


    // nrf24
    mConfig.sendInterval      = SEND_INTERVAL;
    mConfig.maxRetransPerPyld = DEF_MAX_RETRANS_PER_PYLD;
    mConfig.pinCs             = DEF_RF24_CS_PIN;
    mConfig.pinCe             = DEF_RF24_CE_PIN;
    mConfig.pinIrq            = DEF_RF24_IRQ_PIN;
    mConfig.amplifierPower    = DEF_AMPLIFIERPOWER & 0x03;

    // ntp
    snprintf(mConfig.ntpAddr, NTP_ADDR_LEN, "%s", DEF_NTP_SERVER_NAME);
    mConfig.ntpPort = DEF_NTP_PORT;

    // mqtt
    snprintf(mConfig.mqtt.broker, MQTT_ADDR_LEN, "%s", DEF_MQTT_BROKER);
    mConfig.mqtt.port = DEF_MQTT_PORT;
    snprintf(mConfig.mqtt.user, MQTT_USER_LEN, "%s", DEF_MQTT_USER);
    snprintf(mConfig.mqtt.pwd, MQTT_PWD_LEN, "%s", DEF_MQTT_PWD);
    snprintf(mConfig.mqtt.topic, MQTT_TOPIC_LEN, "%s", DEF_MQTT_TOPIC);

    // serial
    mConfig.serialInterval = SERIAL_INTERVAL;
    mConfig.serialShowIv   = false;
    mConfig.serialDebug    = false;
}


//-----------------------------------------------------------------------------
void app::loadEEpconfig(void) {
    DPRINTLN(DBG_VERBOSE, F("app::loadEEpconfig"));

    if(mWifiSettingsValid)
        mEep->read(ADDR_CFG_SYS, (uint8_t*) &mSysConfig, CFG_SYS_LEN);
    if(mSettingsValid) {
        mEep->read(ADDR_CFG, (uint8_t*) &mConfig, CFG_LEN);

        mSendTicker   = mConfig.sendInterval;
        mSerialTicker = 0;

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
                if(NULL != iv) { // will run once on every dtu boot
                    mEep->read(ADDR_INV_PWR_LIM + (i * 2),(uint16_t *)&(iv->powerLimit[0]));
                    mEep->read(ADDR_INV_PWR_LIM_CON + (i * 2),(uint16_t *)&(iv->powerLimit[1]));
                    // only set it, if it is changed by user. Default value in the html setup page is -1 = 0xffff
                    // it is "doppelt-gemoppelt" because the inverter shall remember the setting if the dtu makes a power cycle / reboot
                    if (iv->powerLimit[0] != 0xffff) { 
                        iv->devControlCmd = ActivePowerContr; // set active power limit
                        if (iv->powerLimit[1] & 0x0001){
                            DPRINTLN(DBG_INFO, F("add inverter: ") + String(name) + ", SN: " + String(invSerial, HEX) + ", Power Limit: " + String(iv->powerLimit[0]) + " in %");    
                        } else {
                            DPRINTLN(DBG_INFO, F("add inverter: ") + String(name) + ", SN: " + String(invSerial, HEX) + ", Power Limit: " + String(iv->powerLimit[0]) + " in Watt");
                        }
                    }
                    for(uint8_t j = 0; j < 4; j++) {
                        mEep->read(ADDR_INV_CH_NAME + (i * 4 * MAX_NAME_LENGTH) + j * MAX_NAME_LENGTH, iv->chName[j], MAX_NAME_LENGTH);
                    }
                }

                // TODO: the original mqttinterval value is not needed any more
                mMqttInterval += mConfig.sendInterval;
            }
        }
    }
}


//-----------------------------------------------------------------------------
void app::saveValues(void) {
    DPRINTLN(DBG_VERBOSE, F("app::saveValues"));

    mEep->write(ADDR_CFG_SYS, (uint8_t*)&mSysConfig, CFG_SYS_LEN);
    mEep->write(ADDR_CFG, (uint8_t*)&mConfig, CFG_LEN);
    Inverter<> *iv;
    for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i ++) {
        iv = mSys->getInverterByPos(i);
        if(NULL != iv) {
            mEep->write(ADDR_INV_ADDR + (i * 8), iv->serial.u64);
            mEep->write(ADDR_INV_PWR_LIM + i * 2, iv->powerLimit[0]);
            mEep->write(ADDR_INV_PWR_LIM_CON + i * 2, iv->powerLimit[1]);
            mEep->write(ADDR_INV_NAME + (i * MAX_NAME_LENGTH), iv->name, MAX_NAME_LENGTH);
            // max channel power / name
            for(uint8_t j = 0; j < 4; j++) {
                mEep->write(ADDR_INV_CH_PWR + (i * 2 * 4) + (j*2), iv->chMaxPwr[j]);
                mEep->write(ADDR_INV_CH_NAME + (i * 4 * MAX_NAME_LENGTH) + j * MAX_NAME_LENGTH, iv->chName[j], MAX_NAME_LENGTH);
            }
        }
    }

    updateCrc();
    mEep->commit();
}


//-----------------------------------------------------------------------------
void app::setupMqtt(void) {
    if(mSettingsValid) {
        if(mConfig.mqtt.broker[0] > 0) {
            mMqttActive = true;
            if(mMqttInterval < MIN_MQTT_INTERVAL)
                mMqttInterval = MIN_MQTT_INTERVAL;
        }
        else
            mMqttInterval = 0xffff;

        mMqttTicker = 0;
        mMqtt.setup(&mConfig.mqtt, mSysConfig.deviceName);
        mMqtt.setCallback(std::bind(&app::cbMqtt, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));


        if(mMqttActive) {
            mMqtt.sendMsg("version", mVersion);
            if(mMqtt.isConnected())
                mMqtt.sendMsg("device", mSysConfig.deviceName);

            /*char topic[30];
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
            }*/
        }
    }
}

//-----------------------------------------------------------------------------
void app::resetPayload(Inverter<>* iv)
{
    // reset payload data
    memset(mPayload[iv->id].len, 0, MAX_PAYLOAD_ENTRIES);
    mPayload[iv->id].retransmits = 0;
    mPayload[iv->id].maxPackId = 0;
    mPayload[iv->id].complete = false;
    mPayload[iv->id].requested = true;
    mPayload[iv->id].ts = mTimestamp;
}