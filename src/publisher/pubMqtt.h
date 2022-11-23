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
        }

        ~PubMqtt() { }

        void setup(cfgMqtt_t *cfg_mqtt, const char *devName, const char *version, HMSYSTEM *sys, uint32_t *utcTs, uint32_t *sunrise, uint32_t *sunset) {
            mCfgMqtt        = cfg_mqtt;
            mDevName        = devName;
            mVersion        = version;
            mSys            = sys;
            mUtcTimestamp   = utcTs;
            mSunrise        = sunrise;
            mSunset         = sunset;

            snprintf(mLwtTopic, MQTT_TOPIC_LEN + 7, "%s/status", mCfgMqtt->topic);

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
            sendIvData();
        }

        void tickerMinute() {
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

        void tickerHour() {
            publish("sunrise", String(*mSunrise).c_str(), true);
            publish("sunset", String(*mSunset).c_str(), true);
        }

        void publish(const char *subTopic, const char *payload, bool retained = false, bool addTopic = true) {
            char topic[MQTT_TOPIC_LEN + 2];
            snprintf(topic, (MQTT_TOPIC_LEN + 2), "%s/%s", mCfgMqtt->topic, subTopic);
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

        void payloadEventListener(uint8_t cmd) {
            mSendList.push(cmd);
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

        void sendMqttDiscoveryConfig(const char *topic) {
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
                        snprintf(stateTopic, 64, "%s/%s/ch%d/%s", topic, iv->config->name, rec->assign[i].ch, iv->getFieldName(i, rec));
                        snprintf(discoveryTopic, 64, "%s/sensor/%s/ch%d_%s/config", MQTT_DISCOVERY_PREFIX, iv->config->name, rec->assign[i].ch, iv->getFieldName(i, rec));
                        snprintf(uniq_id, 32, "ch%d_%s", rec->assign[i].ch, iv->getFieldName(i, rec));
                        const char *devCls = getFieldDeviceClass(rec->assign[i].fieldId);
                        const char *stateCls = getFieldStateClass(rec->assign[i].fieldId);

                        doc[F("name")] = name;
                        doc[F("stat_t")] = stateTopic;
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

    private:
        #if defined(ESP8266)
        void onWifiConnect(const WiFiEventStationModeGotIP& event) {
            DPRINTLN(DBG_VERBOSE, F("MQTT connecting"));
            mClient.connect();
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

            publish("version", mVersion, true);
            publish("device", mDevName, true);
            publish("uptime", "0");
            publish(mLwtTopic, mLwtOnline, true, false);

            subscribe("ctrl/#");
            subscribe("setup/#");
            subscribe("status/#");
        }

        void onDisconnect(espMqttClientTypes::DisconnectReason reason) {
            DBGPRINT(F("MQTT disconnected, reason: "));
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
            DPRINTLN(DBG_VERBOSE, F("MQTT got topic: ") + String(topic));
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

        void sendIvData(void) {
            if(mSendList.empty())
                return;

            char topic[32 + MAX_NAME_LENGTH], val[40];
            float total[4];
            bool sendTotal = false;
            bool totalIncomplete = false;

            while(!mSendList.empty()) {
                memset(total, 0, sizeof(float) * 4);
                for (uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                    Inverter<> *iv = mSys->getInverterByPos(id);
                    if (NULL == iv)
                        continue; // skip to next inverter

                    record_t<> *rec = iv->getRecordStruct(mSendList.front());

                    if(mSendList.front() == RealTimeRunData_Debug) {
                        // inverter status
                        uint8_t status = MQTT_STATUS_AVAIL_PROD;
                        if (!iv->isAvailable(*mUtcTimestamp, rec)) {
                            status = MQTT_STATUS_NOT_AVAIL_NOT_PROD;
                            totalIncomplete = true;
                        }
                        else if (!iv->isProducing(*mUtcTimestamp, rec)) {
                            if (MQTT_STATUS_AVAIL_PROD == status)
                                status = MQTT_STATUS_AVAIL_NOT_PROD;
                        }
                        snprintf(topic, 32 + MAX_NAME_LENGTH, "%s/available_text", iv->config->name);
                        snprintf(val, 40, "%s%s%s%s",
                            (status == MQTT_STATUS_NOT_AVAIL_NOT_PROD) ? "not yet " : "",
                            "available and ",
                            (status == MQTT_STATUS_AVAIL_NOT_PROD) ? "not " : "",
                            (status == MQTT_STATUS_NOT_AVAIL_NOT_PROD) ? "" : "producing"
                        );
                        publish(topic, val);

                        snprintf(topic, 32 + MAX_NAME_LENGTH, "%s/available", iv->config->name);
                        snprintf(val, 40, "%d", status);
                        publish(topic, val);

                        snprintf(topic, 32 + MAX_NAME_LENGTH, "%s/last_success", iv->config->name);
                        snprintf(val, 40, "%i", iv->getLastTs(rec) * 1000);
                        publish(topic, val);
                    }

                    // data
                    if(iv->isAvailable(*mUtcTimestamp, rec)) {
                        for (uint8_t i = 0; i < rec->length; i++) {
                            snprintf(topic, 32 + MAX_NAME_LENGTH, "%s/ch%d/%s", iv->config->name, rec->assign[i].ch, fields[rec->assign[i].fieldId]);
                            snprintf(val, 40, "%.3f", iv->getValue(i, rec));
                            publish(topic, val);

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

                if ((true == sendTotal) && (false == totalIncomplete)) {
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
                        snprintf(val, 40, "%.3f", total[i]);
                        publish(topic, val);
                    }
                }
            }
        }

        espMqttClient mClient;
        cfgMqtt_t *mCfgMqtt;
        #if defined(ESP8266)
        WiFiEventHandler mHWifiCon, mHWifiDiscon;
        #endif

        uint32_t *mSunrise, *mSunset;
        HMSYSTEM *mSys;
        uint32_t *mUtcTimestamp;
        uint32_t mRxCnt, mTxCnt;
        std::queue<uint8_t> mSendList;
        bool mEnReconnect;
        subscriptionCb mSubscriptionCb;

        // last will topic and payload must be available trough lifetime of 'espMqttClient'
        char mLwtTopic[MQTT_TOPIC_LEN+7];
        const char* mLwtOnline = "online";
        const char* mLwtOffline = "offline";
        const char *mDevName, *mVersion;
};

#endif /*__PUB_MQTT_H__*/
