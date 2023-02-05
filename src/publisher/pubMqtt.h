//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

// https://bert.emelis.net/espMqttClient/

#ifndef __PUB_MQTT_H__
#define __PUB_MQTT_H__

#ifdef ESP8266
    #include <ESP8266WiFi.h>
#elif defined(ESP32)
    #include <WiFi.h>
#endif

#include "../utils/dbg.h"
#include "../config/config.h"
#include <espMqttClient.h>
#include <ArduinoJson.h>
#include "../defines.h"
#include "../hm/hmSystem.h"

#define QOS_0   0

typedef std::function<void(JsonObject)> subscriptionCb;

struct alarm_t {
    uint16_t code;
    uint32_t start;
    uint32_t end;
    alarm_t(uint16_t c, uint32_t s, uint32_t e) : code(c), start(s), end(e) {}
};

template<class HMSYSTEM>
class PubMqtt {
    public:
        PubMqtt() {
            mRxCnt = 0;
            mTxCnt = 0;
            mSubscriptionCb = NULL;
            memset(mLastIvState, MQTT_STATUS_NOT_AVAIL_NOT_PROD, MAX_NUM_INVERTERS);
        }

        ~PubMqtt() { }

        void setup(cfgMqtt_t *cfg_mqtt, const char *devName, const char *version, HMSYSTEM *sys, uint32_t *utcTs) {
            mCfgMqtt         = cfg_mqtt;
            mDevName         = devName;
            mVersion         = version;
            mSys             = sys;
            mUtcTimestamp    = utcTs;
            mIntervalTimeout = 1;
            mReconnectRequest = false;

            snprintf(mLwtTopic, MQTT_TOPIC_LEN + 5, "%s/mqtt", mCfgMqtt->topic);

            if((strlen(mCfgMqtt->user) > 0) && (strlen(mCfgMqtt->pwd) > 0))
                mClient.setCredentials(mCfgMqtt->user, mCfgMqtt->pwd);
            mClient.setClientId(mDevName); // TODO: add mac?
            mClient.setServer(mCfgMqtt->broker, mCfgMqtt->port);
            mClient.setWill(mLwtTopic, QOS_0, true, mLwtOffline);
            mClient.onConnect(std::bind(&PubMqtt::onConnect, this, std::placeholders::_1));
            mClient.onDisconnect(std::bind(&PubMqtt::onDisconnect, this, std::placeholders::_1));
            mClient.onMessage(std::bind(&PubMqtt::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
        }

        void loop() {
            #if defined(ESP8266)
            mClient.loop();
            yield();
            #endif
        }

        void connect() {
            mReconnectRequest = false;
            if(!mClient.connected())
                mClient.connect();
        }

        void tickerSecond() {
            if(0 == mCfgMqtt->interval) // no fixed interval, publish once new data were received (from inverter)
                sendIvData();
            else { // send mqtt data in a fixed interval
                if(--mIntervalTimeout == 0) {
                    mIntervalTimeout = mCfgMqtt->interval;
                    mSendList.push(RealTimeRunData_Debug);
                    sendIvData();
                }
            }
            if(mReconnectRequest) {
                connect();
                return;
            }
        }

        void tickerMinute() {
            processIvStatus();
            char val[12];
            snprintf(val, 12, "%ld", millis() / 1000);
            publish("uptime", val);
            publish("wifi_rssi", String(WiFi.RSSI()).c_str());
            publish("free_heap", String(ESP.getFreeHeap()).c_str());
        }

        bool tickerSun(uint32_t sunrise, uint32_t sunset, uint32_t offs, bool disNightCom) {
            if (!mClient.connected())
                return false;

            publish("sunrise", String(sunrise).c_str(), true);
            publish("sunset", String(sunset).c_str(), true);
            publish("comm_start", String(sunrise - offs).c_str(), true);
            publish("comm_stop", String(sunset + offs).c_str(), true);
            publish("dis_night_comm", ((disNightCom) ? "true" : "false"), true);

            return true;
        }

        bool tickerComm(bool disabled) {
             if (!mClient.connected())
                return false;

            publish("comm_disabled", ((disabled) ? "true" : "false"), true);
            publish("comm_dis_ts", String(*mUtcTimestamp).c_str(), true);

            if(disabled && (mCfgMqtt->rstValsCommStop))
                zeroAllInverters();

            return true;
        }

        void tickerMidnight() {
            Inverter<> *iv;
            record_t<> *rec;
            char topic[7 + MQTT_TOPIC_LEN], val[4];

            // set YieldDay to zero
            for (uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                iv = mSys->getInverterByPos(id);
                if (NULL == iv)
                    continue; // skip to next inverter
                rec = iv->getRecordStruct(RealTimeRunData_Debug);
                uint8_t pos = iv->getPosByChFld(CH0, FLD_YD, rec);
                iv->setValue(pos, rec, 0.0f);

                snprintf(topic, 32 + MAX_NAME_LENGTH, "%s/ch0/%s", iv->config->name, fields[FLD_YD]);
                snprintf(val, 2, "0");
                publish(topic, val, true);
            }
            // set Total YieldDay to zero
            snprintf(topic, 32 + MAX_NAME_LENGTH, "total/%s", fields[FLD_YD]);
            snprintf(val, 2, "0");
            publish(topic, val, true);        }

        void payloadEventListener(uint8_t cmd) {
            if(mClient.connected()) { // prevent overflow if MQTT broker is not reachable but set
                if((0 == mCfgMqtt->interval) || (RealTimeRunData_Debug != cmd)) // no interval or no live data
                    mSendList.push(cmd);
            }
        }

        void alarmEventListener(uint16_t code, uint32_t start, uint32_t endTime) {
            if(mClient.connected()) {
                mAlarmList.push(alarm_t(code, start, endTime));
            }
        }

        void publish(const char *subTopic, const char *payload, bool retained = false, bool addTopic = true) {
            if(!mClient.connected())
                return;

            String topic = "";
            if(addTopic)
                topic = String(mCfgMqtt->topic) + "/";
            topic += String(subTopic);

            do {
                if(0 != mClient.publish(topic.c_str(), QOS_0, retained, payload))
                    break;
                if(!mClient.connected())
                    break;
                #if defined(ESP8266)
                mClient.loop();
                #endif
                yield();
            } while(1);

            mTxCnt++;
        }

        void subscribe(const char *subTopic) {
            char topic[MQTT_TOPIC_LEN + 20];
            snprintf(topic, (MQTT_TOPIC_LEN + 20), "%s/%s", mCfgMqtt->topic, subTopic);
            mClient.subscribe(topic, QOS_0);
        }

        void setSubscriptionCb(subscriptionCb cb) {
            mSubscriptionCb = cb;
        }

        inline bool isConnected() {
            return mClient.connected();
        }

        inline uint32_t getTxCnt(void) {
            return mTxCnt;
        }

        inline uint32_t getRxCnt(void) {
            return mRxCnt;
        }

        void sendDiscoveryConfig(void) {
            DPRINTLN(DBG_VERBOSE, F("sendMqttDiscoveryConfig"));

            char topic[64], name[32], uniq_id[32];
            DynamicJsonDocument doc(256);

            uint8_t fldTotal[4] = {FLD_PAC, FLD_YT, FLD_YD, FLD_PDC};
            const char* unitTotal[4] = {"W", "kWh", "Wh", "W"};

            String node_mac = WiFi.macAddress().substring(12,14)+ WiFi.macAddress().substring(15,17);
            String node_id = "AHOY_DTU_" + node_mac;
            bool total = false;

            for (uint8_t id = 0; id < mSys->getNumInverters() ; id++) {
                doc.clear();

                if (total) // total become true at iv = NULL next cycle
                    continue;

                Inverter<> *iv = mSys->getInverterByPos(id);
                if (NULL == iv)
                    total = true;
                record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);

                if (!total) {
                    doc[F("name")] = iv->config->name;
                    doc[F("ids")] = String(iv->config->serial.u64, HEX);
                    doc[F("mdl")] = iv->config->name;
                }
                else {
                    doc[F("name")] = node_id;
                    doc[F("ids")] = node_id;
                    doc[F("mdl")] = node_id;
                }

                doc[F("cu")] = F("http://") + String(WiFi.localIP().toString());
                doc[F("mf")] = F("Hoymiles");
                JsonObject deviceObj = doc.as<JsonObject>(); // deviceObj is only pointer!?

                for (uint8_t i = 0; i < ((!total) ? (rec->length) : (4) ) ; i++) {
                    const char *devCls, *stateCls;
                    if (!total) {
                        if (rec->assign[i].ch == CH0)
                            snprintf(name, 32, "%s %s", iv->config->name, iv->getFieldName(i, rec));
                        else
                            snprintf(name, 32, "%s CH%d %s", iv->config->name, rec->assign[i].ch, iv->getFieldName(i, rec));
                        snprintf(topic, 64, "/ch%d/%s", rec->assign[i].ch, iv->getFieldName(i, rec));
                        snprintf(uniq_id, 32, "ch%d_%s", rec->assign[i].ch, iv->getFieldName(i, rec));

                        devCls = getFieldDeviceClass(rec->assign[i].fieldId);
                        stateCls = getFieldStateClass(rec->assign[i].fieldId);
                    }

                    else { // total values
                        snprintf(name, 32, "Total %s", fields[fldTotal[i]]);
                        snprintf(topic, 64, "/%s", fields[fldTotal[i]]);
                        snprintf(uniq_id, 32, "total_%s", fields[fldTotal[i]]);
                        devCls = getFieldDeviceClass(fldTotal[i]);
                        stateCls = getFieldStateClass(fldTotal[i]);
                    }

                    DynamicJsonDocument doc2(512);
                    doc2[F("name")] = name;
                    doc2[F("stat_t")] = String(mCfgMqtt->topic) + "/" + ((!total) ? String(iv->config->name) : "total" ) + String(topic);
                    doc2[F("unit_of_meas")] = ((!total) ? (iv->getUnit(i,rec)) : (unitTotal[i]));
                    doc2[F("uniq_id")] = ((!total) ? (String(iv->config->serial.u64, HEX)) : (node_id)) + "_" + uniq_id;
                    doc2[F("dev")] = deviceObj;
                    if (!(String(stateCls) == String("total_increasing")))
                        doc2[F("exp_aft")] = MQTT_INTERVAL + 5;  // add 5 sec if connection is bad or ESP too slow @TODO: stimmt das wirklich als expire!?
                    if (devCls != NULL)
                        doc2[F("dev_cla")] = String(devCls);
                    if (stateCls != NULL)
                        doc2[F("stat_cla")] = String(stateCls);

                    if (!total)
                        snprintf(topic, 64, "%s/sensor/%s/ch%d_%s/config", MQTT_DISCOVERY_PREFIX, iv->config->name, rec->assign[i].ch, iv->getFieldName(i, rec));
                    else // total values
                        snprintf(topic, 64, "%s/sensor/%s/total_%s/config", MQTT_DISCOVERY_PREFIX, node_id.c_str(),fields[fldTotal[i]]);
                    size_t size = measureJson(doc2) + 1;
                    char *buf = new char[size];
                    memset(buf, 0, size);
                    serializeJson(doc2, buf, size);
                    publish(topic, buf, true, false);
                    delete[] buf;
                }

                yield();
            }
        }

        void setPowerLimitAck(Inverter<> *iv) {
            if (NULL != iv) {
                char topic[7 + MQTT_TOPIC_LEN];

                snprintf(topic, 32 + MAX_NAME_LENGTH, "%s/ack_pwr_limit", iv->config->name);
                publish(topic, "true", true);
            }
        }

    private:
        void onConnect(bool sessionPreset) {
            DPRINTLN(DBG_INFO, F("MQTT connected"));

            publish("version", mVersion, true);
            publish("device", mDevName, true);
            publish("ip_addr", WiFi.localIP().toString().c_str(), true);
            tickerMinute();
            publish(mLwtTopic, mLwtOnline, true, false);

            subscribe("ctrl/#");
            subscribe("setup/#");
            //subscribe("status/#");
        }

        void onDisconnect(espMqttClientTypes::DisconnectReason reason) {
            DPRINT(DBG_INFO, F("MQTT disconnected, reason: "));
            switch (reason) {
                case espMqttClientTypes::DisconnectReason::TCP_DISCONNECTED:
                    DBGPRINTLN(F("TCP disconnect"));
                    mReconnectRequest = true;
                    break;
                case espMqttClientTypes::DisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
                    DBGPRINTLN(F("wrong protocol version"));
                    break;
                case espMqttClientTypes::DisconnectReason::MQTT_IDENTIFIER_REJECTED:
                    DBGPRINTLN(F("identifier rejected"));
                    break;
                case espMqttClientTypes::DisconnectReason::MQTT_SERVER_UNAVAILABLE:
                    DBGPRINTLN(F("broker unavailable"));
                    break;
                case espMqttClientTypes::DisconnectReason::MQTT_MALFORMED_CREDENTIALS:
                    DBGPRINTLN(F("malformed credentials"));
                    break;
                case espMqttClientTypes::DisconnectReason::MQTT_NOT_AUTHORIZED:
                    DBGPRINTLN(F("not authorized"));
                    break;
                default:
                    DBGPRINTLN(F("unknown"));
            }
        }

        void onMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total) {
            DPRINTLN(DBG_INFO, F("MQTT got topic: ") + String(topic));
            if(NULL == mSubscriptionCb)
                return;

            char *tpc = new char[strlen(topic) + 1];
            uint8_t cnt = 0;
            DynamicJsonDocument json(128);
            JsonObject root = json.to<JsonObject>();

            strncpy(tpc, topic, strlen(topic) + 1);
            if(len > 0) {
                char *pyld = new char[len + 1];
                strncpy(pyld, (const char*)payload, len);
                pyld[len] = '\0';
                root["val"] = atoi(pyld);
                delete[] pyld;
            }

            char *p = strtok(tpc, "/");
            p = strtok(NULL, "/"); // remove mCfgMqtt->topic
            while(NULL != p) {
                if(0 == cnt) {
                    if(0 == strncmp(p, "ctrl", 4)) {
                        if(NULL != (p = strtok(NULL, "/"))) {
                            root[F("path")] = F("ctrl");
                            root[F("cmd")] = p;
                        }
                    } else if(0 == strncmp(p, "setup", 5)) {
                        if(NULL != (p = strtok(NULL, "/"))) {
                            root[F("path")] = F("setup");
                            root[F("cmd")] = p;
                        }
                    } else if(0 == strncmp(p, "status", 6)) {
                        if(NULL != (p = strtok(NULL, "/"))) {
                            root[F("path")] = F("status");
                            root[F("cmd")] = p;
                        }
                    }
                }
                else if(1 == cnt) {
                    root[F("id")] = atoi(p);
                }
                p = strtok(NULL, "/");
                cnt++;
            }
            delete[] tpc;

            /*char out[128];
            serializeJson(root, out, 128);
            DPRINTLN(DBG_INFO, "json: " + String(out));*/
            if(NULL != mSubscriptionCb)
                (mSubscriptionCb)(root);

            mRxCnt++;
        }

        const char *getFieldDeviceClass(uint8_t fieldId) {
            uint8_t pos = 0;
            for (; pos < DEVICE_CLS_ASSIGN_LIST_LEN; pos++) {
                if (deviceFieldAssignment[pos].fieldId == fieldId)
                    break;
            }
            return (pos >= DEVICE_CLS_ASSIGN_LIST_LEN) ? NULL : deviceClasses[deviceFieldAssignment[pos].deviceClsId];
        }

        const char *getFieldStateClass(uint8_t fieldId) {
            uint8_t pos = 0;
            for (; pos < DEVICE_CLS_ASSIGN_LIST_LEN; pos++) {
                if (deviceFieldAssignment[pos].fieldId == fieldId)
                    break;
            }
            return (pos >= DEVICE_CLS_ASSIGN_LIST_LEN) ? NULL : stateClasses[deviceFieldAssignment[pos].stateClsId];
        }

        bool processIvStatus() {
            // returns true if all inverters are available
            bool allAvail = true;   // shows if all enabled inverters are available
            bool anyAvail = false;  // shows if at least one enabled inverter is available
            bool changed = false;
            char topic[7 + MQTT_TOPIC_LEN], val[40];
            Inverter<> *iv;
            record_t<> *rec;

            for (uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                iv = mSys->getInverterByPos(id);
                if (NULL == iv)
                    continue; // skip to next inverter

                rec = iv->getRecordStruct(RealTimeRunData_Debug);

                // inverter status
                uint8_t status = MQTT_STATUS_NOT_AVAIL_NOT_PROD;
                if (iv->config->enabled) {
                    if (iv->isAvailable(*mUtcTimestamp))
                        status = (iv->isProducing(*mUtcTimestamp)) ? MQTT_STATUS_AVAIL_PROD : MQTT_STATUS_AVAIL_NOT_PROD;
                    else // inverter is enabled but not available
                        allAvail = false;
                }

                if(mLastIvState[id] != status) {
                    mLastIvState[id] = status;
                    changed = true;

                    if((MQTT_STATUS_NOT_AVAIL_NOT_PROD == status) && (mCfgMqtt->rstValsNotAvail))
                        zeroValues(iv);

                    snprintf(topic, 32 + MAX_NAME_LENGTH, "%s/available", iv->config->name);
                    snprintf(val, 40, "%d", status);
                    publish(topic, val, true);

                    snprintf(topic, 32 + MAX_NAME_LENGTH, "%s/last_success", iv->config->name);
                    snprintf(val, 40, "%d", iv->getLastTs(rec));
                    publish(topic, val, true);
                }
            }

            if(changed) {
                snprintf(val, 32, "%d", ((allAvail) ? MQTT_STATUS_ONLINE : ((anyAvail) ? MQTT_STATUS_PARTIAL : MQTT_STATUS_OFFLINE)));
                publish("status", val, true);
                //sendIvData(false); // false prevents loop of same function
            }

            return allAvail;
        }

        void sendAlarmData() {
            if(mAlarmList.empty())
                return;
            Inverter<> *iv = mSys->getInverterByPos(0, false);
            while(!mAlarmList.empty()) {
                alarm_t alarm = mAlarmList.front();
                publish("alarm", iv->getAlarmStr(alarm.code).c_str());
                publish("alarm_start", String(alarm.start).c_str());
                publish("alarm_end", String(alarm.end).c_str());
                mAlarmList.pop();
            }
        }

        void sendIvData(bool sendTotals = true) {
            if(mSendList.empty())
                return;

            char topic[7 + MQTT_TOPIC_LEN], val[40];
            float total[4];
            bool sendTotal = false;

            while(!mSendList.empty()) {
                memset(total, 0, sizeof(float) * 4);
                for (uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                    Inverter<> *iv = mSys->getInverterByPos(id);
                    if ((NULL == iv) || (MQTT_STATUS_NOT_AVAIL_NOT_PROD == mLastIvState[id]))
                        continue; // skip to next inverter

                    record_t<> *rec = iv->getRecordStruct(mSendList.front());

                    // data
                    //if(iv->isAvailable(*mUtcTimestamp, rec) || (0 != mCfgMqtt->interval)) { // is avail or fixed pulish interval was set
                    for (uint8_t i = 0; i < rec->length; i++) {
                        bool retained = false;
                        if (mSendList.front() == RealTimeRunData_Debug) {
                            switch (rec->assign[i].fieldId) {
                                case FLD_YT:
                                case FLD_YD:
                                    if ((rec->assign[i].ch == CH0) && (!iv->isProducing(*mUtcTimestamp))) // avoids returns to 0 on restart
                                        continue;
                                    retained = true;
                                    break;
                            }
                        }

                        snprintf(topic, 32 + MAX_NAME_LENGTH, "%s/ch%d/%s", iv->config->name, rec->assign[i].ch, fields[rec->assign[i].fieldId]);
                        snprintf(val, 40, "%g", ah::round3(iv->getValue(i, rec)));
                        publish(topic, val, retained);

                        // calculate total values for RealTimeRunData_Debug
                        if (mSendList.front() == RealTimeRunData_Debug) {
                            if (CH0 == rec->assign[i].ch) {
                                switch (rec->assign[i].fieldId) {
                                    case FLD_PAC:
                                        total[0] += iv->getValue(i, rec);
                                        break;
                                    case FLD_YT:
                                        total[1] += iv->getValue(i, rec);
                                        break;
                                    case FLD_YD:
                                        total[2] += iv->getValue(i, rec);
                                        break;
                                    case FLD_PDC:
                                        total[3] += iv->getValue(i, rec);
                                        break;
                                }
                            }
                            sendTotal = true;
                        }
                        yield();
                    }
                    //}
                }

                mSendList.pop(); // remove from list once all inverters were processed

                if(!sendTotals) // skip total value calculation
                    continue;

                if ((true == sendTotal) && processIvStatus()) {
                    uint8_t fieldId;
                    for (uint8_t i = 0; i < 4; i++) {
                        switch (i) {
                            default:
                            case 0:
                                fieldId = FLD_PAC;
                                break;
                            case 1:
                                fieldId = FLD_YT;
                                break;
                            case 2:
                                fieldId = FLD_YD;
                                break;
                            case 3:
                                fieldId = FLD_PDC;
                                break;
                        }
                        snprintf(topic, 32 + MAX_NAME_LENGTH, "total/%s", fields[fieldId]);
                        snprintf(val, 40, "%g", ah::round3(total[i]));
                        publish(topic, val, true);
                    }
                }
            }
        }

        void zeroAllInverters() {
            Inverter<> *iv;

            // set values to zero, exept yields
            for (uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                iv = mSys->getInverterByPos(id);
                if (NULL == iv)
                    continue; // skip to next inverter

                zeroValues(iv);
            }
            sendIvData();
        }

        void zeroValues(Inverter<> *iv) {
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
            for(uint8_t ch = 0; ch <= iv->channels; ch++) {
                uint8_t pos = 0;
                uint8_t fld = 0;
                while(0xff != pos) {
                    switch(fld) {
                        case FLD_YD:
                        case FLD_YT:
                        case FLD_FW_VERSION:
                        case FLD_FW_BUILD_YEAR:
                        case FLD_FW_BUILD_MONTH_DAY:
                        case FLD_FW_BUILD_HOUR_MINUTE:
                        case FLD_HW_ID:
                        case FLD_ACT_ACTIVE_PWR_LIMIT:
                            fld++;
                            continue;
                            break;
                    }
                    pos = iv->getPosByChFld(ch, fld, rec);
                    iv->setValue(pos, rec, 0.0f);
                    fld++;
                }
            }

            mSendList.push(RealTimeRunData_Debug);
        }

        espMqttClient mClient;
        cfgMqtt_t *mCfgMqtt;
        #if defined(ESP8266)
        WiFiEventHandler mHWifiCon, mHWifiDiscon;
        #endif

        HMSYSTEM *mSys;
        uint32_t *mUtcTimestamp;
        uint32_t mRxCnt, mTxCnt;
        std::queue<uint8_t> mSendList;
        std::queue<alarm_t> mAlarmList;
        subscriptionCb mSubscriptionCb;
        bool mReconnectRequest;
        uint8_t mLastIvState[MAX_NUM_INVERTERS];
        uint16_t mIntervalTimeout;

        // last will topic and payload must be available trough lifetime of 'espMqttClient'
        char mLwtTopic[MQTT_TOPIC_LEN+5];
        const char* mLwtOnline = "connected";
        const char* mLwtOffline = "not connected";
        const char *mDevName, *mVersion;
};

#endif /*__PUB_MQTT_H__*/
