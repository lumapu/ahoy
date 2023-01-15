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
#include "../utils/ahoyTimer.h"
#include "../config/config.h"
#include <espMqttClient.h>
#include <ArduinoJson.h>
#include "../defines.h"
#include "../hm/hmSystem.h"

#define QOS_0   0

typedef std::function<void(JsonObject)> subscriptionCb;

template<class HMSYSTEM>
class PubMqtt {
    public:
        PubMqtt() {
            mRxCnt = 0;
            mTxCnt = 0;
            mEnReconnect = false;
            mSubscriptionCb = NULL;
            mIvAvail = true;
            memset(mLastIvState, 0xff, MAX_NUM_INVERTERS);
        }

        ~PubMqtt() { }

        void setup(cfgMqtt_t *cfg_mqtt, const char *devName, const char *version, HMSYSTEM *sys, uint32_t *utcTs) {
            mCfgMqtt         = cfg_mqtt;
            mDevName         = devName;
            mVersion         = version;
            mSys             = sys;
            mUtcTimestamp    = utcTs;
            mExeOnce         = true;
            mIntervalTimeout = 1;

            snprintf(mLwtTopic, MQTT_TOPIC_LEN + 5, "%s/mqtt", mCfgMqtt->topic);

            #if defined(ESP8266)
            mHWifiCon = WiFi.onStationModeGotIP(std::bind(&PubMqtt::onWifiConnect, this, std::placeholders::_1));
            mHWifiDiscon = WiFi.onStationModeDisconnected(std::bind(&PubMqtt::onWifiDisconnect, this, std::placeholders::_1));
            #else
            WiFi.onEvent(std::bind(&PubMqtt::onWiFiEvent, this, std::placeholders::_1));
            #endif

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
            #endif
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

        }

        void tickerMinute() {
            processIvStatus();
            char val[12];
            snprintf(val, 12, "%ld", millis() / 1000);
            publish("uptime", val);
            publish("wifi_rssi", String(WiFi.RSSI()).c_str());
            publish("free_heap", String(ESP.getFreeHeap()).c_str());

            if(!mClient.connected()) {
                if(mEnReconnect)
                    mClient.connect();
            }
        }

        void tickerSun(uint32_t sunrise, uint32_t sunset, uint32_t offs, bool disNightCom) {
            publish("sunrise", String(sunrise).c_str(), true);
            publish("sunset", String(sunset).c_str(), true);
            publish("comm_start", String(sunrise - offs).c_str(), true);
            publish("comm_stop", String(sunset + offs).c_str(), true);
            publish("dis_night_comm", ((disNightCom) ? "true" : "false"), true);
        }

        void tickerComm(bool disabled) {
            publish("comm_disabled", ((disabled) ? "true" : "false"), true);
            publish("comm_dis_ts", String(*mUtcTimestamp).c_str(), true);
        }

        void payloadEventListener(uint8_t cmd) {
            if(mClient.connected()) { // prevent overflow if MQTT broker is not reachable but set
                if((0 == mCfgMqtt->interval) || (RealTimeRunData_Debug != cmd)) // no interval or no live data
                    mSendList.push(cmd);
            }
        }

        void publish(const char *subTopic, const char *payload, bool retained = false, bool addTopic = true) {
            if(!mClient.connected())
                return;

            char topic[(MQTT_TOPIC_LEN << 1) + 2];
            snprintf(topic, ((MQTT_TOPIC_LEN << 1) + 2), "%s/%s", mCfgMqtt->topic, subTopic);
            if(addTopic)
                mClient.publish(topic, QOS_0, retained, payload);
            else
                mClient.publish(subTopic, QOS_0, retained, payload);
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

            char stateTopic[64], discoveryTopic[64], buffer[512], name[32], uniq_id[32];
            for (uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                Inverter<> *iv = mSys->getInverterByPos(id);
                if (NULL != iv) {
                    record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
                    DynamicJsonDocument deviceDoc(128);
                    deviceDoc[F("name")] = iv->config->name;
                    deviceDoc[F("ids")] = String(iv->config->serial.u64, HEX);
                    deviceDoc[F("cu")] = F("http://") + String(WiFi.localIP().toString());
                    deviceDoc[F("mf")] = F("Hoymiles");
                    deviceDoc[F("mdl")] = iv->config->name;
                    JsonObject deviceObj = deviceDoc.as<JsonObject>();
                    DynamicJsonDocument doc(384);

                    for (uint8_t i = 0; i < rec->length; i++) {
                        if (rec->assign[i].ch == CH0) {
                            snprintf(name, 32, "%s %s", iv->config->name, iv->getFieldName(i, rec));
                        } else {
                            snprintf(name, 32, "%s CH%d %s", iv->config->name, rec->assign[i].ch, iv->getFieldName(i, rec));
                        }
                        snprintf(stateTopic, 64, "/ch%d/%s", rec->assign[i].ch, iv->getFieldName(i, rec));
                        snprintf(discoveryTopic, 64, "%s/sensor/%s/ch%d_%s/config", MQTT_DISCOVERY_PREFIX, iv->config->name, rec->assign[i].ch, iv->getFieldName(i, rec));
                        snprintf(uniq_id, 32, "ch%d_%s", rec->assign[i].ch, iv->getFieldName(i, rec));
                        const char *devCls = getFieldDeviceClass(rec->assign[i].fieldId);
                        const char *stateCls = getFieldStateClass(rec->assign[i].fieldId);

                        doc[F("name")] = name;
                        doc[F("stat_t")] = String(mCfgMqtt->topic) + "/" + String(iv->config->name) + String(stateTopic);
                        doc[F("unit_of_meas")] = iv->getUnit(i, rec);
                        doc[F("uniq_id")] = String(iv->config->serial.u64, HEX) + "_" + uniq_id;
                        doc[F("dev")] = deviceObj;
                        doc[F("exp_aft")] = MQTT_INTERVAL + 5;  // add 5 sec if connection is bad or ESP too slow @TODO: stimmt das wirklich als expire!?
                        if (devCls != NULL)
                            doc[F("dev_cla")] = devCls;
                        if (stateCls != NULL)
                            doc[F("stat_cla")] = stateCls;

                        serializeJson(doc, buffer);
                        publish(discoveryTopic, buffer, true, false);
                        doc.clear();
                    }

                    yield();
                }
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
        #if defined(ESP8266)
        void onWifiConnect(const WiFiEventStationModeGotIP& event) {
            DPRINTLN(DBG_VERBOSE, F("MQTT connecting"));
            mClient.connect();
            mEnReconnect = true;
        }

        void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
            mEnReconnect = false;
        }

        #else
        void onWiFiEvent(WiFiEvent_t event) {
            switch(event) {
                case SYSTEM_EVENT_STA_GOT_IP:
                    DPRINTLN(DBG_VERBOSE, F("MQTT connecting"));
                    mClient.connect();
                    mEnReconnect = true;
                    break;

                case SYSTEM_EVENT_STA_DISCONNECTED:
                    mEnReconnect = false;
                    break;

                default:
                    break;
            }
        }
        #endif

        void onConnect(bool sessionPreset) {
            DPRINTLN(DBG_INFO, F("MQTT connected"));
            mEnReconnect = true;

            if(mExeOnce) {
                publish("version", mVersion, true);
                publish("device", mDevName, true);
                publish("ip_addr", WiFi.localIP().toString().c_str(), true);
                mExeOnce = false;
            }
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
            bool allAvail = true;
            bool first = true;
            bool changed = false;
            char topic[7 + MQTT_TOPIC_LEN], val[40];
            Inverter<> *iv;
            record_t<> *rec;
            bool totalComplete = true;

            for (uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                iv = mSys->getInverterByPos(id);
                if (NULL == iv)
                    continue; // skip to next inverter

                rec = iv->getRecordStruct(RealTimeRunData_Debug);
                if(first)
                    mIvAvail = false;
                first = false;

                // inverter status
                uint8_t status = MQTT_STATUS_AVAIL_PROD;
                if ((!iv->isAvailable(*mUtcTimestamp, rec)) || (!iv->config->enabled)) {
                    status = MQTT_STATUS_NOT_AVAIL_NOT_PROD;
                    if(iv->config->enabled) { // only change all-avail if inverter is enabled!
                        totalComplete = false;
                        allAvail = false;
                    }
                }
                else if (!iv->isProducing(*mUtcTimestamp, rec)) {
                    mIvAvail = true;
                    if (MQTT_STATUS_AVAIL_PROD == status)
                        status = MQTT_STATUS_AVAIL_NOT_PROD;
                }
                else
                    mIvAvail = true;

                if(mLastIvState[id] != status) {
                    mLastIvState[id] = status;
                    changed = true;

                    snprintf(topic, 32 + MAX_NAME_LENGTH, "%s/available", iv->config->name);
                    snprintf(val, 40, "%d", status);
                    publish(topic, val, true);

                    snprintf(topic, 32 + MAX_NAME_LENGTH, "%s/last_success", iv->config->name);
                    snprintf(val, 40, "%d", iv->getLastTs(rec));
                    publish(topic, val, true);
                }
            }

            if(changed) {
                snprintf(val, 32, "%d", ((allAvail) ? MQTT_STATUS_ONLINE : ((mIvAvail) ? MQTT_STATUS_PARTIAL : MQTT_STATUS_OFFLINE)));
                publish("status", val, true);
            }

            return totalComplete;
        }

        void sendIvData(void) {
            if(mSendList.empty())
                return;

            char topic[7 + MQTT_TOPIC_LEN], val[40];
            float total[4];
            bool sendTotal = false;

            while(!mSendList.empty()) {
                memset(total, 0, sizeof(float) * 4);
                for (uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                    Inverter<> *iv = mSys->getInverterByPos(id);
                    if (NULL == iv)
                        continue; // skip to next inverter

                    record_t<> *rec = iv->getRecordStruct(mSendList.front());

                    // data
                    if(iv->isAvailable(*mUtcTimestamp, rec)) {
                        for (uint8_t i = 0; i < rec->length; i++) {
                            bool retained = false;
                            if (mSendList.front() == RealTimeRunData_Debug) {
                                switch (rec->assign[i].fieldId) {
                                    case FLD_YT:
                                    case FLD_YD:
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
                    }
                }

                mSendList.pop(); // remove from list once all inverters were processed

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

        espMqttClient mClient;
        cfgMqtt_t *mCfgMqtt;
        #if defined(ESP8266)
        WiFiEventHandler mHWifiCon, mHWifiDiscon;
        #endif

        HMSYSTEM *mSys;
        uint32_t *mUtcTimestamp;
        uint32_t mRxCnt, mTxCnt;
        std::queue<uint8_t> mSendList;
        bool mEnReconnect;
        subscriptionCb mSubscriptionCb;
        bool mIvAvail; // shows if at least one inverter is available
        uint8_t mLastIvState[MAX_NUM_INVERTERS];
        bool mExeOnce;
        uint16_t mIntervalTimeout;

        // last will topic and payload must be available trough lifetime of 'espMqttClient'
        char mLwtTopic[MQTT_TOPIC_LEN+5];
        const char* mLwtOnline = "connected";
        const char* mLwtOffline = "not connected";
        const char *mDevName, *mVersion;
};

#endif /*__PUB_MQTT_H__*/
