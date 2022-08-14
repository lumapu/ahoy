//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __MQTT_H__
#define __MQTT_H__

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "defines.h"

class mqtt {
    public:
        mqtt() {
            mClient     = new PubSubClient(mEspClient);
            mAddressSet = false;

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

        void setCallback(MQTT_CALLBACK_SIGNATURE){
            mClient->setCallback(callback);
        }

        void sendMsg(const char *topic, const char *msg) {
            //DPRINTLN(DBG_VERBOSE, F("mqtt.h:sendMsg"));
            char top[64];
            snprintf(top, 64, "%s/%s", mCfg->topic, topic);
            sendMsg2(top, msg, false);
        }

        void sendMsg2(const char *topic, const char *msg, boolean retained) {
            if(mAddressSet) {
                if(!mClient->connected())
                    reconnect();
                if(mClient->connected())
                    mClient->publish(topic, msg, retained);
            }
        }

        bool isConnected(bool doRecon = false) {
            //DPRINTLN(DBG_VERBOSE, F("mqtt.h:isConnected"));
            if(doRecon)
                reconnect();
            return mClient->connected();
        }

        void loop() {
            //DPRINT(F("m"));
            if(!mClient->connected())
                reconnect();
            mClient->loop();
        }

    private:
        void reconnect(void) {
            DPRINTLN(DBG_DEBUG, F("mqtt.h:reconnect"));
            DPRINTLN(DBG_DEBUG, F("MQTT mClient->_state ") + String(mClient->state()) );
            DPRINTLN(DBG_DEBUG, F("WIFI mEspClient.status ") + String(mEspClient.status()) );
            if(!mClient->connected()) {
                if(strlen(mDevName) > 0) {
                    // der Server und der Port müssen neu gesetzt werden, 
                    // da ein MQTT_CONNECTION_LOST -3 die Werte zerstört hat.
                    mClient->setServer(mCfg->broker, mCfg->port);
                    mClient->setBufferSize(MQTT_MAX_PACKET_SIZE);
                    if((strlen(mCfg->user) > 0) && (strlen(mCfg->pwd) > 0))
                        mClient->connect(mDevName, mCfg->user, mCfg->pwd);
                    else
                        mClient->connect(mDevName);
                }
                // ein Subscribe ist nur nach einem connect notwendig
                char topic[MQTT_TOPIC_LEN + 13 ]; // "/devcontrol/#" --> + 6 byte
                // ToDo: "/devcontrol/#" is hardcoded 
                snprintf(topic, MQTT_TOPIC_LEN + 13, "%s/devcontrol/#", mCfg->topic);
                DPRINTLN(DBG_INFO, F("subscribe to ") + String(topic));
                mClient->subscribe(topic); // subscribe to mTopic + "/devcontrol/#"
            }
        }

        WiFiClient mEspClient;
        PubSubClient *mClient;
        
        bool mAddressSet;
        mqttConfig_t *mCfg;
        char mDevName[DEVNAME_LEN];
};

#endif /*__MQTT_H_*/
