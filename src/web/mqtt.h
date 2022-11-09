//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __MQTT_H__
#define __MQTT_H__

#ifdef ESP8266
    #include <ESP8266WiFi.h>
#elif defined(ESP32)
    #include <WiFi.h>
#endif

#if defined(ESP32) && defined(F)
  #undef F
  #define F(sl) (sl)
#endif

#include "../utils/dbg.h"
#include <PubSubClient.h>
#include "../defines.h"
#include "../hm/hmSystem.h"
#include <ArduinoJson.h>

template<class HMSYSTEM>
class mqtt {
    public:
        mqtt() {
            mClient     = new PubSubClient(mEspClient);
            mAddressSet = false;

            mLastReconnect = 0;
            mTxCnt = 0;

            memset(mDevName, 0, DEVNAME_LEN);
        }

        ~mqtt() { }

        void setup(mqttConfig_t *cfg, const char *devname) {
            DPRINTLN(DBG_VERBOSE, F("mqtt.h:setup"));
            mAddressSet = true;

            mCfg = cfg;
            snprintf(mDevName, DEVNAME_LEN,    "%s", devname);

            mClient->setServer(mCfg->broker, mCfg->port);
            mClient->setBufferSize(MQTT_MAX_PACKET_SIZE);
        }

        void setCallback(MQTT_CALLBACK_SIGNATURE) {
            mClient->setCallback(callback);
        }

        void sendMsg(const char *topic, const char *msg) {
            //DPRINTLN(DBG_VERBOSE, F("mqtt.h:sendMsg"));
            if(mAddressSet) {
                char top[64];
                snprintf(top, 64, "%s/%s", mCfg->topic, topic);
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

        void loop() {
            //DPRINT(F("m"));
            if(mAddressSet) {
                if(!mClient->connected())
                    reconnect();
                mClient->loop();
            }
        }

        uint32_t getTxCnt(void) {
            return mTxCnt;
        }

        void sendMqttDiscoveryConfig(HMSYSTEM *sys, const char *topic, uint32_t invertval) {
    DPRINTLN(DBG_VERBOSE, F("app::sendMqttDiscoveryConfig"));

    char stateTopic[64], discoveryTopic[64], buffer[512], name[32], uniq_id[32];
    for (uint8_t id = 0; id < sys->getNumInverters(); id++) {
        Inverter<> *iv = sys->getInverterByPos(id);
        if (NULL != iv) {
            record_t<> *rec = iv->getRecordStruct(RealTimeRunData_Debug);
            DynamicJsonDocument deviceDoc(128);
            deviceDoc["name"] = iv->name;
            deviceDoc["ids"] = String(iv->serial.u64, HEX);
            deviceDoc["cu"] = F("http://") + String(WiFi.localIP().toString());
            deviceDoc["mf"] = "Hoymiles";
            deviceDoc["mdl"] = iv->name;
            JsonObject deviceObj = deviceDoc.as<JsonObject>();
            DynamicJsonDocument doc(384);

            for (uint8_t i = 0; i < rec->length; i++) {
                if (rec->assign[i].ch == CH0) {
                    snprintf(name, 32, "%s %s", iv->name, iv->getFieldName(i, rec));
                } else {
                    snprintf(name, 32, "%s CH%d %s", iv->name, rec->assign[i].ch, iv->getFieldName(i, rec));
                }
                snprintf(stateTopic, 64, "%s/%s/ch%d/%s", topic, iv->name, rec->assign[i].ch, iv->getFieldName(i, rec));
                snprintf(discoveryTopic, 64, "%s/sensor/%s/ch%d_%s/config", MQTT_DISCOVERY_PREFIX, iv->name, rec->assign[i].ch, iv->getFieldName(i, rec));
                snprintf(uniq_id, 32, "ch%d_%s", rec->assign[i].ch, iv->getFieldName(i, rec));
                const char *devCls = getFieldDeviceClass(rec->assign[i].fieldId);
                const char *stateCls = getFieldStateClass(rec->assign[i].fieldId);

                doc["name"] = name;
                doc["stat_t"] = stateTopic;
                doc["unit_of_meas"] = iv->getUnit(i, rec);
                doc["uniq_id"] = String(iv->serial.u64, HEX) + "_" + uniq_id;
                doc["dev"] = deviceObj;
                doc["exp_aft"] = invertval + 5;  // add 5 sec if connection is bad or ESP too slow @TODO: stimmt das wirklich als expire!?
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


//-----------------------------------------------------------------------------
const char *getFieldDeviceClass(uint8_t fieldId) {
    uint8_t pos = 0;
    for (; pos < DEVICE_CLS_ASSIGN_LIST_LEN; pos++) {
        if (deviceFieldAssignment[pos].fieldId == fieldId)
            break;
    }
    return (pos >= DEVICE_CLS_ASSIGN_LIST_LEN) ? NULL : deviceClasses[deviceFieldAssignment[pos].deviceClsId];
}

//-----------------------------------------------------------------------------
const char *getFieldStateClass(uint8_t fieldId) {
    uint8_t pos = 0;
    for (; pos < DEVICE_CLS_ASSIGN_LIST_LEN; pos++) {
        if (deviceFieldAssignment[pos].fieldId == fieldId)
            break;
    }
    return (pos >= DEVICE_CLS_ASSIGN_LIST_LEN) ? NULL : stateClasses[deviceFieldAssignment[pos].stateClsId];
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
                    mClient->setServer(mCfg->broker, mCfg->port);
                    mClient->setBufferSize(MQTT_MAX_PACKET_SIZE);

                    char lwt[MQTT_TOPIC_LEN + 7 ]; // "/uptime" --> + 7 byte
                    snprintf(lwt, MQTT_TOPIC_LEN + 7, "%s/uptime", mCfg->topic);

                    if((strlen(mCfg->user) > 0) && (strlen(mCfg->pwd) > 0))
                        resub = mClient->connect(mDevName, mCfg->user, mCfg->pwd, lwt, 0, false, "offline");
                    else
                        resub = mClient->connect(mDevName, lwt, 0, false, "offline");
                        // ein Subscribe ist nur nach einem connect notwendig
                    if(resub) {
                        char topic[MQTT_TOPIC_LEN + 13 ]; // "/devcontrol/#" --> + 6 byte
                        // ToDo: "/devcontrol/#" is hardcoded 
                        snprintf(topic, MQTT_TOPIC_LEN + 13, "%s/devcontrol/#", mCfg->topic);
                        DPRINTLN(DBG_INFO, F("subscribe to ") + String(topic));
                        mClient->subscribe(topic); // subscribe to mTopic + "/devcontrol/#"
                    }
                }
            }
        }

        WiFiClient mEspClient;
        PubSubClient *mClient;
        
        bool mAddressSet;
        mqttConfig_t *mCfg;
        char mDevName[DEVNAME_LEN];
        uint32_t mLastReconnect;
        uint32_t mTxCnt;
};

#endif /*__MQTT_H_*/
