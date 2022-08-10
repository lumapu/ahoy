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
        PubSubClient *mClient;

        mqtt() {
            mClient     = new PubSubClient(mEspClient);
            mAddressSet = false;

            memset(mBroker,  0, MQTT_ADDR_LEN);
            memset(mUser,    0, MQTT_USER_LEN);
            memset(mPwd,     0, MQTT_PWD_LEN);
            memset(mTopic,   0, MQTT_TOPIC_LEN);
            memset(mDevName, 0, DEVNAME_LEN);
        }

        ~mqtt() { }

        void setup(const char *broker, const char *topic, const char *user, const char *pwd, const char *devname, uint16_t port) {
            DPRINTLN(DBG_VERBOSE, F("mqtt.h:setup"));
            mAddressSet = true;

            mPort = port;
            snprintf(mBroker,  MQTT_ADDR_LEN,  "%s", broker);
            snprintf(mUser,    MQTT_USER_LEN,  "%s", user);
            snprintf(mPwd,     MQTT_PWD_LEN,   "%s", pwd);
            snprintf(mTopic,   MQTT_TOPIC_LEN, "%s", topic);
            snprintf(mDevName, DEVNAME_LEN,    "%s", devname);

            mClient->setServer(mBroker, mPort);
            mClient->setBufferSize(MQTT_MAX_PACKET_SIZE);
        }

        void setCallback(void (*func)(const char* topic, byte* payload, unsigned int length)){
            mClient->setCallback(func);
        }

        void sendMsg(const char *topic, const char *msg) {
            //DPRINTLN(DBG_VERBOSE, F("mqtt.h:sendMsg"));
            char top[64];
            snprintf(top, 64, "%s/%s", mTopic, topic);
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

        char *getBroker(void) {
            //DPRINTLN(DBG_VERBOSE, F("mqtt.h:getBroker"));
            return mBroker;
        }

        char *getUser(void) {
            //DPRINTLN(DBG_VERBOSE, F("mqtt.h:getUser"));
            return mUser;
        }

        char *getPwd(void) {
            //DPRINTLN(DBG_VERBOSE, F("mqtt.h:getPwd"));
            return mPwd;
        }

        char *getTopic(void) {
            //DPRINTLN(DBG_VERBOSE, F("mqtt.h:getTopic"));
            return mTopic;
        }

        char *getDevName(void) {
            //DPRINTLN(DBG_VERBOSE, F("mqtt.h:getDevName"));
            return mDevName;
        }

        uint16_t getPort(void) {
            return mPort;
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
                    mClient->setServer(mBroker, mPort);
                    mClient->setBufferSize(MQTT_MAX_PACKET_SIZE);
                    if((strlen(mUser) > 0) && (strlen(mPwd) > 0))
                        mClient->connect(mDevName, mUser, mPwd);
                    else
                        mClient->connect(mDevName);
                }
                // ein Subscribe ist nur nach einem connect notwendig
                char topic[MQTT_TOPIC_LEN + 13 ]; // "/devcontrol/#" --> + 6 byte
                // ToDo: "/devcontrol/#" is hardcoded 
                snprintf(topic, MQTT_TOPIC_LEN + 13, "%s/devcontrol/#", mTopic); 
                DPRINTLN(DBG_INFO, F("subscribe to ") + String(topic));
                mClient->subscribe(topic); // subscribe to mTopic + "/devcontrol/#"
            }
        }

        WiFiClient mEspClient;
        
        bool mAddressSet;
        uint16_t mPort;
        char mBroker[MQTT_ADDR_LEN];
        char mUser[MQTT_USER_LEN];
        char mPwd[MQTT_PWD_LEN];
        char mTopic[MQTT_TOPIC_LEN];
        char mDevName[DEVNAME_LEN];
};

#endif /*__MQTT_H_*/
