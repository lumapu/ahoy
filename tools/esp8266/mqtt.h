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

            memset(mUser,  0, MQTT_USER_LEN);
            memset(mPwd,   0, MQTT_PWD_LEN);
            memset(mTopic, 0, MQTT_TOPIC_LEN);
        }

        ~mqtt() { }

        void setup(const char *broker, const char *topic, const char *user, const char *pwd, uint16_t port) {
            DPRINTLN(DBG_VERBOSE, F("mqtt.h:setup"));
            mAddressSet = true;
            mClient->setServer(broker, port);
            mClient->setBufferSize(MQTT_MAX_PACKET_SIZE);

            mPort = port;
            snprintf(mUser, MQTT_USER_LEN, "%s", user);
            snprintf(mPwd, MQTT_PWD_LEN, "%s", pwd);
            snprintf(mTopic, MQTT_TOPIC_LEN, "%s", topic);
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

        uint16_t getPort(void) {
            return mPort;
        }

        void loop() {
            //DPRINT(F("m"));
            //if(!mClient->connected())
            //    reconnect();
            mClient->loop();
        }

    private:
        void reconnect(void) {
            //DPRINTLN(DBG_VERBOSE, F("mqtt.h:reconnect"));
            if(!mClient->connected()) {
                if((strlen(mUser) > 0) && (strlen(mPwd) > 0))
                    mClient->connect(DEF_DEVICE_NAME, mUser, mPwd);
                else
                    mClient->connect(DEF_DEVICE_NAME);
            }
        }

        WiFiClient mEspClient;
        PubSubClient *mClient;

        bool mAddressSet;
        uint16_t mPort;
        char mUser[MQTT_USER_LEN];
        char mPwd[MQTT_PWD_LEN];
        char mTopic[MQTT_TOPIC_LEN];
};

#endif /*__MQTT_H_*/
