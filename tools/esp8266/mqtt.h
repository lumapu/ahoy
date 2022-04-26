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
            mAddressSet = true;
            mClient->setServer(broker, port);

            snprintf(mUser, MQTT_USER_LEN, "%s", user);
            snprintf(mPwd, MQTT_PWD_LEN, "%s", pwd);
            snprintf(mTopic, MQTT_TOPIC_LEN, "%s", topic);
        }

        void sendMsg(const char *topic, const char *msg) {
            if(mAddressSet) {
                char top[64];
                snprintf(top, 64, "%s/%s", mTopic, topic);

                if(!mClient->connected())
                    reconnect();
                mClient->publish(top, msg);
            }
        }

        bool isConnected(bool doRecon = false) {
            if(doRecon)
                reconnect();
            return mClient->connected();
        }

        char *getUser(void) {
            return mUser;
        }

        char *getPwd(void) {
            return mPwd;
        }

        char *getTopic(void) {
            return mTopic;
        }

        void loop() {
            //if(!mClient->connected())
            //    reconnect();
            mClient->loop();
        }

    private:
        void reconnect(void) {
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
        char mUser[MQTT_USER_LEN];
        char mPwd[MQTT_PWD_LEN];
        char mTopic[MQTT_TOPIC_LEN];
};

#endif /*__MQTT_H_*/
