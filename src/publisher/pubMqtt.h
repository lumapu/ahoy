//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

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
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "../defines.h"
#include "../hm/hmSystem.h"

template<class HMSYSTEM>
class PubMqtt {
    public:
        PubMqtt() {
            mClient     = new PubSubClient(mEspClient);
            mAddressSet = false;

            mLastReconnect = 0;
            mTxCnt = 0;
        }

        ~PubMqtt() { }

        void setup(cfgMqtt_t *cfg_mqtt, const char *devName, const char *version, HMSYSTEM *sys, uint32_t *utcTs, uint32_t *sunrise, uint32_t *sunset) {
            DPRINTLN(DBG_VERBOSE, F("PubMqtt.h:setup"));
            mAddressSet = true;

            mCfg_mqtt       = cfg_mqtt;
            mDevName        = devName;
            mSys            = sys;
            mUtcTimestamp   = utcTs;
            mSunrise        = sunrise;
            mSunset         = sunset;

            mClient->setServer(mCfg_mqtt->broker, mCfg_mqtt->port);
            mClient->setBufferSize(MQTT_MAX_PACKET_SIZE);

            setCallback(std::bind(&PubMqtt<HMSYSTEM>::cbMqtt, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

            sendMsg("version", version);
            sendMsg("device", devName);
            sendMsg("uptime", "0");
        }

        void loop() {
            if(mAddressSet)
                mClient->loop();
        }

        void tickerSecond() {
            if(mAddressSet) {
                if(!mClient->connected())
                    reconnect();
            }
            sendIvData();
        }

        void tickerMinute() {
            if(mAddressSet) {
                char val[40];
                snprintf(val, 40, "%ld", millis() / 1000);
                sendMsg("uptime", val);

                sendMsg("wifi_rssi", String(WiFi.RSSI()).c_str());
            }
        }

        void tickerHour() {
            if(mAddressSet) {
                sendMsg("sunrise", String(*mSunrise).c_str());
                sendMsg("sunset", String(*mSunset).c_str());
            }
        }

        void setCallback(MQTT_CALLBACK_SIGNATURE) {
            mClient->setCallback(callback);
        }

        void sendMsg(const char *topic, const char *msg) {
            //DPRINTLN(DBG_VERBOSE, F("mqtt.h:sendMsg"));
            if(mAddressSet) {
                char top[66];
                snprintf(top, 66, "%s/%s", mCfg_mqtt->topic, topic);
                sendMsg2(top, msg, false);
            }
        }

        void sendMsg2(const char *topic, const char *msg, boolean retained) {
            if(mAddressSet) {
                if(!mClient->connected())
                    reconnect();
                if(mClient->connected())
                    mClient->publish(topic, msg, retained);
                mTxCnt++;
            }
        }

        bool isConnected(bool doRecon = false) {
            //DPRINTLN(DBG_VERBOSE, F("mqtt.h:isConnected"));
            if(!mAddressSet)
                return false;
            if(doRecon && !mClient->connected())
                reconnect();
            return mClient->connected();
        }

        void payloadEventListener(uint8_t cmd) {
            mSendList.push(cmd);
        }

        uint32_t getTxCnt(void) {
            return mTxCnt;
        }

        void sendMqttDiscoveryConfig(const char *topic) {
            DPRINTLN(DBG_VERBOSE, F("sendMqttDiscoveryConfig"));

            char stateTopic[64], discoveryTopic[64], buffer[512], name[32], uniq_id[32];
            for (uint8_t id = 0; id < mSys->getNumInverters(); id++) {
                Inverter<> *iv = mSys->getInverterByPos(id);
                if (NULL != iv) {
                    record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
                    DynamicJsonDocument deviceDoc(128);
                    deviceDoc["name"] = iv->config->name;
                    deviceDoc["ids"] = String(iv->config->serial.u64, HEX);
                    deviceDoc["cu"] = F("http://") + String(WiFi.localIP().toString());
                    deviceDoc["mf"] = "Hoymiles";
                    deviceDoc["mdl"] = iv->config->name;
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

                        doc["name"] = name;
                        doc["stat_t"] = stateTopic;
                        doc["unit_of_meas"] = iv->getUnit(i, rec);
                        doc["uniq_id"] = String(iv->config->serial.u64, HEX) + "_" + uniq_id;
                        doc["dev"] = deviceObj;
                        doc["exp_aft"] = MQTT_INTERVAL + 5;  // add 5 sec if connection is bad or ESP too slow @TODO: stimmt das wirklich als expire!?
                        if (devCls != NULL)
                            doc["dev_cla"] = devCls;
                        if (stateCls != NULL)
                            doc["stat_cla"] = stateCls;

                        serializeJson(doc, buffer);
                        sendMsg2(discoveryTopic, buffer, true);
                        // DPRINTLN(DBG_INFO, F("mqtt sent"));
                        doc.clear();
                    }

                    yield();
                }
            }
        }

    private:
        void reconnect(void) {
            DPRINTLN(DBG_DEBUG, F("mqtt.h:reconnect"));
            DPRINTLN(DBG_DEBUG, F("MQTT mClient->_state ") + String(mClient->state()) );

            #ifdef ESP8266
                DPRINTLN(DBG_DEBUG, F("WIFI mEspClient.status ") + String(mEspClient.status()) );
            #endif

            boolean resub = false;
            if(!mClient->connected() && (millis() - mLastReconnect) > MQTT_RECONNECT_DELAY ) {
                mLastReconnect = millis();
                if(strlen(mDevName) > 0) {
                    // der Server und der Port müssen neu gesetzt werden,
                    // da ein MQTT_CONNECTION_LOST -3 die Werte zerstört hat.
                    mClient->setServer(mCfg_mqtt->broker, mCfg_mqtt->port);
                    mClient->setBufferSize(MQTT_MAX_PACKET_SIZE);

                    char lwt[MQTT_TOPIC_LEN + 7 ]; // "/uptime" --> + 7 byte
                    snprintf(lwt, MQTT_TOPIC_LEN + 7, "%s/uptime", mCfg_mqtt->topic);

                    if((strlen(mCfg_mqtt->user) > 0) && (strlen(mCfg_mqtt->pwd) > 0))
                        resub = mClient->connect(mDevName, mCfg_mqtt->user, mCfg_mqtt->pwd, lwt, 0, false, "offline");
                    else
                        resub = mClient->connect(mDevName, lwt, 0, false, "offline");
                        // ein Subscribe ist nur nach einem connect notwendig
                    if(resub) {
                        char topic[MQTT_TOPIC_LEN + 13 ]; // "/devcontrol/#" --> + 6 byte
                        // ToDo: "/devcontrol/#" is hardcoded
                        snprintf(topic, MQTT_TOPIC_LEN + 13, "%s/devcontrol/#", mCfg_mqtt->topic);
                        DPRINTLN(DBG_INFO, F("subscribe to ") + String(topic));
                        mClient->subscribe(topic); // subscribe to mTopic + "/devcontrol/#"
                    }
                }
            }
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

            isConnected(true);  // really needed? See comment from HorstG-57 #176
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
                        sendMsg(topic, val);

                        snprintf(topic, 32 + MAX_NAME_LENGTH, "%s/available", iv->config->name);
                        snprintf(val, 40, "%d", status);
                        sendMsg(topic, val);

                        snprintf(topic, 32 + MAX_NAME_LENGTH, "%s/last_success", iv->config->name);
                        snprintf(val, 40, "%i", iv->getLastTs(rec) * 1000);
                        sendMsg(topic, val);
                    }

                    // data
                    if(iv->isAvailable(*mUtcTimestamp, rec)) {
                        for (uint8_t i = 0; i < rec->length; i++) {
                            snprintf(topic, 32 + MAX_NAME_LENGTH, "%s/ch%d/%s", iv->config->name, rec->assign[i].ch, fields[rec->assign[i].fieldId]);
                            snprintf(val, 40, "%.3f", iv->getValue(i, rec));
                            sendMsg(topic, val);

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
                        sendMsg(topic, val);
                    }
                }
            }
        }

        void cbMqtt(char *topic, byte *payload, unsigned int length) {
            // callback handling on subscribed devcontrol topic
            DPRINTLN(DBG_INFO, F("cbMqtt"));
            // subcribed topics are mTopic + "/devcontrol/#" where # is <inverter_id>/<subcmd in dec>
            // eg. mypvsolar/devcontrol/1/11 with payload "400" --> inverter 1 active power limit 400 Watt
            const char *token = strtok(topic, "/");
            while (token != NULL) {
                if (strcmp(token, "devcontrol") == 0) {
                    token = strtok(NULL, "/");
                    uint8_t iv_id = std::stoi(token);

                    if (iv_id >= 0 && iv_id <= MAX_NUM_INVERTERS) {
                        Inverter<> *iv = mSys->getInverterByPos(iv_id);
                        if (NULL != iv) {
                            if (!iv->devControlRequest) {  // still pending
                                token = strtok(NULL, "/");

                                switch (std::stoi(token)) {
                                    // Active Power Control
                                    case ActivePowerContr:
                                        token = strtok(NULL, "/");  // get ControlMode aka "PowerPF.Desc" in DTU-Pro Code from topic string
                                        if (token == NULL)          // default via mqtt ommit the LimitControlMode
                                            iv->powerLimit[1] = AbsolutNonPersistent;
                                        else
                                            iv->powerLimit[1] = std::stoi(token);
                                        if (length <= 5) {  // if (std::stoi((char*)payload) > 0) more error handling powerlimit needed?
                                            if (iv->powerLimit[1] >= AbsolutNonPersistent && iv->powerLimit[1] <= RelativPersistent) {
                                                iv->devControlCmd = ActivePowerContr;
                                                iv->powerLimit[0] = std::stoi(std::string((char *)payload, (unsigned int)length));  // THX to @silversurfer
                                                /*if (iv->powerLimit[1] & 0x0001)
                                                    DPRINTLN(DBG_INFO, F("Power limit for inverter ") + String(iv->id) + F(" set to ") + String(iv->powerLimit[0]) + F("%"));
                                                else
                                                    DPRINTLN(DBG_INFO, F("Power limit for inverter ") + String(iv->id) + F(" set to ") + String(iv->powerLimit[0]) + F("W"));*/

                                                DPRINTLN(DBG_INFO, F("Power limit for inverter ") + String(iv->id) + F(" set to ") + String(iv->powerLimit[0]) + String(iv->powerLimit[1] & 0x0001) ? F("%") : F("W"));
                                            }
                                            iv->devControlRequest = true;
                                        } else {
                                            DPRINTLN(DBG_INFO, F("Invalid mqtt payload recevied: ") + String((char *)payload));
                                        }
                                        break;

                                    // Turn On
                                    case TurnOn:
                                        iv->devControlCmd = TurnOn;
                                        DPRINTLN(DBG_INFO, F("Turn on inverter ") + String(iv->id));
                                        iv->devControlRequest = true;
                                        break;

                                    // Turn Off
                                    case TurnOff:
                                        iv->devControlCmd = TurnOff;
                                        DPRINTLN(DBG_INFO, F("Turn off inverter ") + String(iv->id));
                                        iv->devControlRequest = true;
                                        break;

                                    // Restart
                                    case Restart:
                                        iv->devControlCmd = Restart;
                                        DPRINTLN(DBG_INFO, F("Restart inverter ") + String(iv->id));
                                        iv->devControlRequest = true;
                                        break;

                                    // Reactive Power Control
                                    case ReactivePowerContr:
                                        iv->devControlCmd = ReactivePowerContr;
                                        if (true) {  // if (std::stoi((char*)payload) > 0) error handling powerlimit needed?
                                            iv->devControlCmd = ReactivePowerContr;
                                            iv->powerLimit[0] = std::stoi(std::string((char *)payload, (unsigned int)length));
                                            iv->powerLimit[1] = 0x0000;  // if reactivepower limit is set via external interface --> set it temporay
                                            DPRINTLN(DBG_DEBUG, F("Reactivepower limit for inverter ") + String(iv->id) + F(" set to ") + String(iv->powerLimit[0]) + F("W"));
                                            iv->devControlRequest = true;
                                        }
                                        break;

                                    // Set Power Factor
                                    case PFSet:
                                        // iv->devControlCmd = PFSet;
                                        // uint16_t power_factor = std::stoi(strtok(NULL, "/"));
                                        DPRINTLN(DBG_INFO, F("Set Power Factor not implemented for inverter ") + String(iv->id));
                                        break;

                                    // CleanState lock & alarm
                                    case CleanState_LockAndAlarm:
                                        iv->devControlCmd = CleanState_LockAndAlarm;
                                        DPRINTLN(DBG_INFO, F("CleanState lock & alarm for inverter ") + String(iv->id));
                                        iv->devControlRequest = true;
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

        uint32_t *mSunrise, *mSunset;
        WiFiClient mEspClient;
        PubSubClient *mClient;
        HMSYSTEM *mSys;
        uint32_t *mUtcTimestamp;

        bool mAddressSet;
        cfgMqtt_t *mCfg_mqtt;
        const char *mDevName;
        uint32_t mLastReconnect;
        uint32_t mTxCnt;
        std::queue<uint8_t> mSendList;
};

#endif /*__PUB_MQTT_H__*/
