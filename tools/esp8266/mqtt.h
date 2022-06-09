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

        ~mqtt() {
            delete mClient;
        }

        void setup(const char *broker, const char *topic, const char *user, const char *pwd, uint16_t port) {
            DPRINTLN(F("mqtt.h:setup"));
            mAddressSet = true;
            mClient->setServer(broker, port);

            mPort = port;
            snprintf(mUser, MQTT_USER_LEN, "%s", user);
            snprintf(mPwd, MQTT_PWD_LEN, "%s", pwd);
            snprintf(mTopic, MQTT_TOPIC_LEN, "%s", topic);
        }

        void sendMsg(const char *topic, const char *msg) {
            //DPRINTLN(F("mqtt.h:sendMsg"));
            if(mAddressSet) {
                char top[64];
                snprintf(top, 64, "%s/%s", mTopic, topic);

                if(!mClient->connected())
                    reconnect();
                if(mClient->connected())
                    mClient->publish(top, msg);
            }
        }

        bool isConnected(bool doRecon = false) {
            //DPRINTLN(F("mqtt.h:isConnected"));
            if(doRecon)
                reconnect();
            return mClient->connected();
        }

        char *getUser(void) {
            //DPRINTLN(F("mqtt.h:getUser"));
            return mUser;
        }

        char *getPwd(void) {
            //DPRINTLN(F("mqtt.h:getPwd"));
            return mPwd;
        }

        char *getTopic(void) {
            //DPRINTLN(F("mqtt.h:getTopic"));
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
            //DPRINTLN(F("mqtt.h:reconnect"));
            if(!mClient->connected()) {
                String mqttId = "ESP-" + String(random(0xffff), HEX);
                if((strlen(mUser) > 0) && (strlen(mPwd) > 0))
                    mClient->connect(mqttId.c_str(), mUser, mPwd);
                else
                    mClient->connect(mqttId.c_str());
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
