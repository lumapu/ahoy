//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
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

#include "pubMqttDefs.h"
#include "pubMqttIvData.h"

typedef std::function<void(JsonObject)> subscriptionCb;

typedef struct {
    bool running;
    uint8_t lastIvId;
    uint8_t sub;
    uint8_t foundIvCnt;
} discovery_t;

template<class HMSYSTEM>
class PubMqtt {
    public:
        PubMqtt() {
            mRxCnt = 0;
            mTxCnt = 0;
            mSubscriptionCb = NULL;
            memset(mLastIvState, (uint8_t)InverterStatus::OFF, MAX_NUM_INVERTERS);
            memset(mIvLastRTRpub, 0, MAX_NUM_INVERTERS * 4);
            mLastAnyAvail = false;
            mZeroValues = false;
        }

        ~PubMqtt() { }

        void setup(cfgMqtt_t *cfg_mqtt, const char *devName, const char *version, HMSYSTEM *sys, uint32_t *utcTs, uint32_t *uptime) {
            mCfgMqtt         = cfg_mqtt;
            mDevName         = devName;
            mVersion         = version;
            mSys             = sys;
            mUtcTimestamp    = utcTs;
            mUptime          = uptime;
            mIntervalTimeout = 1;

            mSendIvData.setup(sys, utcTs, &mSendList);
            mSendIvData.setPublishFunc([this](const char *subTopic, const char *payload, bool retained, uint8_t qos) {
                publish(subTopic, payload, retained, true, qos);
            });
            mDiscovery.running = false;

            snprintf(mLwtTopic, MQTT_TOPIC_LEN + 5, "%s/mqtt", mCfgMqtt->topic);

            if((strlen(mCfgMqtt->user) > 0) && (strlen(mCfgMqtt->pwd) > 0))
                mClient.setCredentials(mCfgMqtt->user, mCfgMqtt->pwd);

            if(strlen(mCfgMqtt->clientId) > 0)
                snprintf(mClientId, 23, "%s", mCfgMqtt->clientId);
            else{
                snprintf(mClientId, 24, "%s-", mDevName);
                uint8_t pos = strlen(mClientId);
                mClientId[pos++] = WiFi.macAddress().substring( 9, 10).c_str()[0];
                mClientId[pos++] = WiFi.macAddress().substring(10, 11).c_str()[0];
                mClientId[pos++] = WiFi.macAddress().substring(12, 13).c_str()[0];
                mClientId[pos++] = WiFi.macAddress().substring(13, 14).c_str()[0];
                mClientId[pos++] = WiFi.macAddress().substring(15, 16).c_str()[0];
                mClientId[pos++] = WiFi.macAddress().substring(16, 17).c_str()[0];
                mClientId[pos++] = '\0';
            }

            mClient.setClientId(mClientId);
            mClient.setServer(mCfgMqtt->broker, mCfgMqtt->port);
            mClient.setWill(mLwtTopic, QOS_0, true, mqttStr[MQTT_STR_LWT_NOT_CONN]);
            mClient.onConnect(std::bind(&PubMqtt::onConnect, this, std::placeholders::_1));
            mClient.onDisconnect(std::bind(&PubMqtt::onDisconnect, this, std::placeholders::_1));
            mClient.onMessage(std::bind(&PubMqtt::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
        }

        void loop() {
            mSendIvData.loop();

            #if defined(ESP8266)
            mClient.loop();
            yield();
            #endif

            if(mDiscovery.running)
                discoveryConfigLoop();
        }

        void tickerSecond() {
            if (mIntervalTimeout > 0)
                mIntervalTimeout--;

            if(mClient.disconnected()) {
                mClient.connect();
                return; // next try in a second
            }

            if(0 == mCfgMqtt->interval) // no fixed interval, publish once new data were received (from inverter)
                sendIvData();
            else { // send mqtt data in a fixed interval
                if(mIntervalTimeout == 0) {
                    mIntervalTimeout = mCfgMqtt->interval;
                    mSendList.push(sendListCmdIv(RealTimeRunData_Debug, NULL));
                    sendIvData();
                }
            }

            sendAlarmData();
        }

        void tickerMinute() {
            snprintf(mVal, 40, "%d", (*mUptime));
            publish(subtopics[MQTT_UPTIME], mVal);
            publish(subtopics[MQTT_RSSI], String(WiFi.RSSI()).c_str());
            publish(subtopics[MQTT_FREE_HEAP], String(ESP.getFreeHeap()).c_str());
            #ifndef ESP32
            publish(subtopics[MQTT_HEAP_FRAG], String(ESP.getHeapFragmentation()).c_str());
            #endif
        }

        bool tickerSun(uint32_t sunrise, uint32_t sunset, int16_t offsM, int16_t offsE) {
            if (!mClient.connected())
                return false;

            publish(subtopics[MQTT_SUNRISE], String(sunrise).c_str(), true);
            publish(subtopics[MQTT_SUNSET], String(sunset).c_str(), true);
            publish(subtopics[MQTT_COMM_START], String(sunrise + offsM).c_str(), true);
            publish(subtopics[MQTT_COMM_STOP], String(sunset + offsE).c_str(), true);

            Inverter<> *iv;
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                iv = mSys->getInverterByPos(i);
                if(NULL == iv)
                    continue;

                snprintf(mSubTopic, 32 + MAX_NAME_LENGTH, "%s/dis_night_comm", iv->config->name);
                publish(mSubTopic, ((iv->commEnabled) ? dict[STR_TRUE] : dict[STR_FALSE]), true);
            }


            snprintf(mSubTopic, 32 + MAX_NAME_LENGTH, "comm_disabled");
            publish(mSubTopic, (((*mUtcTimestamp > (sunset + offsE)) || (*mUtcTimestamp < (sunrise + offsM))) ? dict[STR_TRUE] : dict[STR_FALSE]), true);

            return true;
        }

        bool tickerComm(bool disabled) {
             if (!mClient.connected())
                return false;

            publish(subtopics[MQTT_COMM_DISABLED], ((disabled) ? dict[STR_TRUE] : dict[STR_FALSE]), true);
            publish(subtopics[MQTT_COMM_DIS_TS], String(*mUtcTimestamp).c_str(), true);

            return true;
        }

        void tickerMidnight() {
            // set Total YieldDay to zero
            snprintf(mSubTopic, 32 + MAX_NAME_LENGTH, "total/%s", fields[FLD_YD]);
            snprintf(mVal, 2, "0");
            publish(mSubTopic, mVal, true);
        }

        void payloadEventListener(uint8_t cmd, Inverter<> *iv) {
            if(mClient.connected()) { // prevent overflow if MQTT broker is not reachable but set
                if((0 == mCfgMqtt->interval) || (RealTimeRunData_Debug != cmd)) // no interval or no live data
                    mSendList.push(sendListCmdIv(cmd, iv));
            }
        }

        void alarmEvent(Inverter<> *iv) {
            mSendAlarm[iv->id] = true;
        }

        void publish(const char *subTopic, const char *payload, bool retained = false, bool addTopic = true, uint8_t qos = QOS_0) {
            if(!mClient.connected())
                return;

            if(addTopic)
                snprintf(mTopic, MQTT_TOPIC_LEN + 32 + MAX_NAME_LENGTH + 1, "%s/%s", mCfgMqtt->topic, subTopic);
            else
                snprintf(mTopic, MQTT_TOPIC_LEN + 32 + MAX_NAME_LENGTH + 1, "%s", subTopic);

            mClient.publish(mTopic, qos, retained, payload);
            yield();
            mTxCnt++;
        }

        void subscribe(const char *subTopic, uint8_t qos = QOS_0) {
            char topic[MQTT_TOPIC_LEN + 20];
            snprintf(topic, (MQTT_TOPIC_LEN + 20), "%s/%s", mCfgMqtt->topic, subTopic);
            mClient.subscribe(topic, qos);
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
            mDiscovery.running  = true;
            mDiscovery.lastIvId = 0;
            mDiscovery.sub = 0;
            mDiscovery.foundIvCnt = 0;
        }

        void setPowerLimitAck(Inverter<> *iv) {
            if (NULL != iv) {
                snprintf(mSubTopic, 32 + MAX_NAME_LENGTH, "%s/%s", iv->config->name, subtopics[MQTT_ACK_PWR_LMT]);
                publish(mSubTopic, "true", true, true, QOS_2);
            }
        }

        void setZeroValuesEnable(void) {
            mZeroValues = true;
        }

    private:
        void onConnect(bool sessionPreset) {
            DPRINTLN(DBG_INFO, F("MQTT connected"));

            publish(subtopics[MQTT_VERSION], mVersion, true);
            publish(subtopics[MQTT_DEVICE], mDevName, true);
            #if defined(ETHERNET)
            publish(subtopics[MQTT_IP_ADDR], ETH.localIP().toString().c_str(), true);
            #else
            publish(subtopics[MQTT_IP_ADDR], WiFi.localIP().toString().c_str(), true);
            #endif
            tickerMinute();
            publish(mLwtTopic, mqttStr[MQTT_STR_LWT_CONN], true, false);

            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                snprintf(mVal, 20, "ctrl/limit/%d", i);
                subscribe(mVal, QOS_2);
                snprintf(mVal, 20, "ctrl/restart/%d", i);
                subscribe(mVal);
                snprintf(mVal, 20, "ctrl/power/%d", i);
                subscribe(mVal);
            }
            subscribe(subscr[MQTT_SUBS_SET_TIME]);
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
            if(len == 0)
                return;
            DPRINT(DBG_INFO, mqttStr[MQTT_STR_GOT_TOPIC]);
            DBGPRINTLN(String(topic));
            if(NULL == mSubscriptionCb)
                return;

            DynamicJsonDocument json(128);
            JsonObject root = json.to<JsonObject>();

            bool limitAbs = false;
            if(len > 0) {
                char *pyld = new char[len + 1];
                strncpy(pyld, (const char*)payload, len);
                pyld[len] = '\0';
                if(NULL == strstr(topic, "limit"))
                    root[F("val")] = atoi(pyld);
                else
                    root[F("val")] = (int)(atof(pyld) * 10.0f);

                if(pyld[len-1] == 'W')
                    limitAbs = true;
                delete[] pyld;
            }

            const char *p = topic + strlen(mCfgMqtt->topic);
            uint8_t pos = 0, elm = 0;
            char tmp[30];

            while(1) {
                if(('/' == p[pos]) || ('\0' == p[pos])) {
                    strncpy(tmp, p, pos);
                    tmp[pos] = '\0';
                    switch(elm++) {
                        case 1: root[F("path")] = String(tmp); break;
                        case 2:
                            if(strncmp("limit", tmp, 5) == 0) {
                                if(limitAbs)
                                    root[F("cmd")] = F("limit_nonpersistent_absolute");
                                else
                                    root[F("cmd")] = F("limit_nonpersistent_relative");
                            } else
                                root[F("cmd")] = String(tmp);
                            break;
                        case 3: root[F("id")] = atoi(tmp);   break;
                        default: break;
                    }
                    if('\0' == p[pos])
                        break;
                    p = p + pos + 1;
                    pos = 0;
                }
                pos++;
            }

            /*char out[128];
            serializeJson(root, out, 128);
            DPRINTLN(DBG_INFO, "json: " + String(out));*/
            (mSubscriptionCb)(root);

            mRxCnt++;
        }

        void discoveryConfigLoop(void) {
            char topic[64], name[32], uniq_id[32], buf[350];
            DynamicJsonDocument doc(256);

            uint8_t fldTotal[4] = {FLD_PAC, FLD_YT, FLD_YD, FLD_PDC};
            const char* unitTotal[4] = {"W", "kWh", "Wh", "W"};

            String node_id = String(mDevName) + "_TOTAL";
            bool total = (mDiscovery.lastIvId == MAX_NUM_INVERTERS);

            Inverter<> *iv = mSys->getInverterByPos(mDiscovery.lastIvId);
            record_t<> *rec = NULL;
            if (NULL != iv) {
                rec = iv->getRecordStruct(RealTimeRunData_Debug);
                if(0 == mDiscovery.sub)
                mDiscovery.foundIvCnt++;
            }

            if ((NULL != iv) || total) {
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

                const char *devCls, *stateCls;
                if (!total) {
                    if (rec->assign[mDiscovery.sub].ch == CH0)
                        snprintf(name, 32, "%s", iv->getFieldName(mDiscovery.sub, rec));
                    else
                        snprintf(name, 32, "CH%d_%s", rec->assign[mDiscovery.sub].ch, iv->getFieldName(mDiscovery.sub, rec));
                    snprintf(topic, 64, "/ch%d/%s", rec->assign[mDiscovery.sub].ch, iv->getFieldName(mDiscovery.sub, rec));
                    snprintf(uniq_id, 32, "ch%d_%s", rec->assign[mDiscovery.sub].ch, iv->getFieldName(mDiscovery.sub, rec));

                    devCls = getFieldDeviceClass(rec->assign[mDiscovery.sub].fieldId);
                    stateCls = getFieldStateClass(rec->assign[mDiscovery.sub].fieldId);
                }

                else { // total values
                    snprintf(name, 32, "Total %s", fields[fldTotal[mDiscovery.sub]]);
                    snprintf(topic, 64, "/%s", fields[fldTotal[mDiscovery.sub]]);
                    snprintf(uniq_id, 32, "total_%s", fields[fldTotal[mDiscovery.sub]]);
                    devCls = getFieldDeviceClass(fldTotal[mDiscovery.sub]);
                    stateCls = getFieldStateClass(fldTotal[mDiscovery.sub]);
                }

                DynamicJsonDocument doc2(512);
                doc2[F("name")] = name;
                doc2[F("stat_t")] = String(mCfgMqtt->topic) + "/" + ((!total) ? String(iv->config->name) : "total" ) + String(topic);
                doc2[F("unit_of_meas")] = ((!total) ? (iv->getUnit(mDiscovery.sub, rec)) : (unitTotal[mDiscovery.sub]));
                doc2[F("uniq_id")] = ((!total) ? (String(iv->config->serial.u64, HEX)) : (node_id)) + "_" + uniq_id;
                doc2[F("dev")] = deviceObj;
                if (!(String(stateCls) == String("total_increasing")))
                    doc2[F("exp_aft")] = MQTT_INTERVAL + 5;  // add 5 sec if connection is bad or ESP too slow @TODO: stimmt das wirklich als expire!?
                if (devCls != NULL)
                    doc2[F("dev_cla")] = String(devCls);
                if (stateCls != NULL)
                    doc2[F("stat_cla")] = String(stateCls);

                if (!total)
                    snprintf(topic, 64, "%s/sensor/%s/ch%d_%s/config", MQTT_DISCOVERY_PREFIX, iv->config->name, rec->assign[mDiscovery.sub].ch, iv->getFieldName(mDiscovery.sub, rec));
                else // total values
                    snprintf(topic, 64, "%s/sensor/%s/total_%s/config", MQTT_DISCOVERY_PREFIX, node_id.c_str(), fields[fldTotal[mDiscovery.sub]]);
                size_t size = measureJson(doc2) + 1;
                memset(buf, 0, size);
                serializeJson(doc2, buf, size);
                publish(topic, buf, true, false);

                if(++mDiscovery.sub == ((!total) ? (rec->length) : 4)) {
                    mDiscovery.sub = 0;
                    checkDiscoveryEnd();
                }
            } else {
                mDiscovery.sub = 0;
                checkDiscoveryEnd();
            }

            yield();
        }

        void checkDiscoveryEnd(void) {
            if(++mDiscovery.lastIvId == MAX_NUM_INVERTERS) {
                // check if only one inverter was found, then don't create 'total' sensor
                if(mDiscovery.foundIvCnt == 1)
                    mDiscovery.running = false;
            } else if(mDiscovery.lastIvId == (MAX_NUM_INVERTERS + 1))
                mDiscovery.running = false;
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
            // returns true if any inverter is available
            bool allAvail = true;   // shows if all enabled inverters are available
            bool anyAvail = false;  // shows if at least one enabled inverter is available
            bool changed = false;
            Inverter<> *iv;

            for (uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                iv = mSys->getInverterByPos(id);
                if (NULL == iv)
                    continue; // skip to next inverter
                if (!iv->config->enabled)
                    continue; // skip to next inverter

                // inverter status
                InverterStatus status = iv->getStatus();
                if (InverterStatus::OFF < status)
                    anyAvail = true;
                else // inverter is enabled but not available
                    allAvail = false;

                if(mLastIvState[id] != status) {
                    // if status changed from producing to not producing send last data immediately
                    if (InverterStatus::WAS_PRODUCING == mLastIvState[id])
                        sendData(iv, RealTimeRunData_Debug);

                    mLastIvState[id] = status;
                    changed = true;

                    snprintf(mSubTopic, 32 + MAX_NAME_LENGTH, "%s/available", iv->config->name);
                    snprintf(mVal, 40, "%d", (uint8_t)status);
                    publish(mSubTopic, mVal, true);
                }
            }

            if(changed) {
                snprintf(mVal, 32, "%d", ((allAvail) ? MQTT_STATUS_ONLINE : ((anyAvail) ? MQTT_STATUS_PARTIAL : MQTT_STATUS_OFFLINE)));
                publish("status", mVal, true);
            }

            return anyAvail;
        }

        void sendAlarmData() {
            Inverter<> *iv;
            uint32_t localTime = gTimezone.toLocal(*mUtcTimestamp);
            uint32_t lastMidnight = gTimezone.toUTC(localTime - (localTime % 86400));  // last midnight local time
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                if(!mSendAlarm[i])
                    continue;

                iv = mSys->getInverterByPos(i, false);
                if (NULL == iv)
                    continue;
                if (!iv->config->enabled)
                    continue;

                mSendAlarm[i] = false;

                snprintf(mSubTopic, 32 + MAX_NAME_LENGTH, "%s/alarm/cnt", iv->config->name);
                snprintf(mVal, 40, "%d", iv->alarmCnt);
                publish(mSubTopic, mVal, false);

                for(uint8_t j = 0; j < 10; j++) {
                    if(0 != iv->lastAlarm[j].code) {
                        snprintf(mSubTopic, 32 + MAX_NAME_LENGTH, "%s/alarm/%d", iv->config->name, j);
                        snprintf(mVal, 100, "{\"code\":%d,\"str\":\"%s\",\"start\":%d,\"end\":%d}",
                            iv->lastAlarm[j].code,
                            iv->getAlarmStr(iv->lastAlarm[j].code).c_str(),
                            iv->lastAlarm[j].start + lastMidnight,
                            iv->lastAlarm[j].end + lastMidnight);
                        publish(mSubTopic, mVal, false);
                        yield();
                    }
                }
            }
        }

        void sendData(Inverter<> *iv, uint8_t curInfoCmd) {
            record_t<> *rec = iv->getRecordStruct(curInfoCmd);

            uint32_t lastTs = iv->getLastTs(rec);
            bool pubData = (lastTs > 0);
            if (curInfoCmd == RealTimeRunData_Debug)
                pubData &= (lastTs != mIvLastRTRpub[iv->id]);

            if (pubData) {
                mIvLastRTRpub[iv->id] = lastTs;
                for (uint8_t i = 0; i < rec->length; i++) {
                    bool retained = false;
                    if (curInfoCmd == RealTimeRunData_Debug) {
                        switch (rec->assign[i].fieldId) {
                            case FLD_YT:
                            case FLD_YD:
                                if ((rec->assign[i].ch == CH0) && (!iv->isProducing())) // avoids returns to 0 on restart
                                    continue;
                                retained = true;
                                break;
                        }
                    }

                    snprintf(mSubTopic, 32 + MAX_NAME_LENGTH, "%s/ch%d/%s", iv->config->name, rec->assign[i].ch, fields[rec->assign[i].fieldId]);
                    snprintf(mVal, 40, "%g", ah::round3(iv->getValue(i, rec)));
                    publish(mSubTopic, mVal, retained);

                    yield();
                }
            }
        }

        void sendIvData() {
            bool anyAvail = processIvStatus();
            if (mLastAnyAvail != anyAvail)
                mSendList.push(sendListCmdIv(RealTimeRunData_Debug, NULL));  // makes sure that total values are calculated

            if(mSendList.empty())
                return;

            mSendIvData.start(mZeroValues);
            mZeroValues = false;
            mLastAnyAvail = anyAvail;
        }

        espMqttClient mClient;
        cfgMqtt_t *mCfgMqtt;
        #if defined(ESP8266)
        WiFiEventHandler mHWifiCon, mHWifiDiscon;
        #endif

        HMSYSTEM *mSys;
        PubMqttIvData<HMSYSTEM> mSendIvData;

        uint32_t *mUtcTimestamp, *mUptime;
        uint32_t mRxCnt, mTxCnt;
        std::queue<sendListCmdIv> mSendList;
        std::array<bool, MAX_NUM_INVERTERS> mSendAlarm{};
        subscriptionCb mSubscriptionCb;
        bool mLastAnyAvail;
        bool mZeroValues;
        InverterStatus mLastIvState[MAX_NUM_INVERTERS];
        uint32_t mIvLastRTRpub[MAX_NUM_INVERTERS];
        uint16_t mIntervalTimeout;

        // last will topic and payload must be available through lifetime of 'espMqttClient'
        char mLwtTopic[MQTT_TOPIC_LEN+5];
        const char *mDevName, *mVersion;
        char mClientId[24]; // number of chars is limited to 23 up to v3.1 of MQTT
        // global buffer for mqtt topic. Used when publishing mqtt messages.
        char mTopic[MQTT_TOPIC_LEN + 32 + MAX_NAME_LENGTH + 1];
        char mSubTopic[32 + MAX_NAME_LENGTH + 1];
        char mVal[100];
        discovery_t mDiscovery;
};

#endif /*__PUB_MQTT_H__*/
