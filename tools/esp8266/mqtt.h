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
            mInsend = false;

            memset(mUser,  0, MQTT_USER_LEN);
            memset(mPwd,   0, MQTT_PWD_LEN);
            memset(mTopic, 0, MQTT_TOPIC_LEN);
            memset(mDeviceName, 0, DEVNAME_LEN);
            memset(mBroker, 0, MQTT_ADDR_LEN);
        }

        ~mqtt() { }

        void setup(const char *broker, const char *topic, const char *user, const char *pwd, uint16_t port, const char *devname ) {
            DPRINTLN(DBG_VERBOSE, F("mqtt.h:setup"));
            mAddressSet = true;
            mInsend = false;

            mReconnect = 1;

            mPort = port;
            snprintf(mUser, MQTT_USER_LEN, "%s", user);
            snprintf(mPwd, MQTT_PWD_LEN, "%s", pwd);
            snprintf(mTopic, MQTT_TOPIC_LEN, "%s", topic);
            snprintf(mDeviceName, DEVNAME_LEN, "%s", devname);
            snprintf(mBroker, MQTT_ADDR_LEN, "%s", broker);
            
            mClient->setServer(mBroker, mPort);
            mClient->setBufferSize(MQTT_MAX_PACKET_SIZE);

        }

        void sendMsg(const char *topic, const char *msg) {
            //DPRINTLN(DBG_VERBOSE, F("mqtt.h:sendMsg"));
            char top[64];
            snprintf(top, 64, "%s/%s", mTopic, topic);
            sendMsg2(top, msg, false);
        }

        void sendMsg2(const char *topic, const char *msg, boolean retained) {
            if(mAddressSet) {
                if (!mInsend) {
                    mInsend = true;
                    if(!mClient->connected())
                        reconnect();
                    if(mClient->connected()) {
                        mClient->publish(topic, msg, retained);
                    }
                    mInsend = false;
                }
                else
                    DPRINTLN(DBG_INFO, F("   SendMsg2 inSend is set ") + String(mClient->state()) + "  ");

            }
        }

        bool isConnected(bool doRecon = false) {
            DPRINTLN(DBG_DEBUG, F("mqtt.h:isConnected"));
            if(doRecon)
                DPRINTLN(DBG_DEBUG, F(" doRecon ") + String(doRecon) + " ");
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

        void decReconnect(void) {
            if ( mReconnect > 1) {
                mReconnect--;
                DPRINTLN(DBG_INFO, F(" mqtt.h:decReconnect mReconnect = ") + String(mReconnect) + " ");
            }
            // return mReconnect;
        }

        void loop() {
            //DPRINT(F("m"));
            //if(!mClient->connected())
            //    reconnect();
            mClient->loop();
        }

    private:
        void reconnect(void) {
            boolean rc;
            if (mReconnect < 2 ) {
                DPRINTLN(DBG_DEBUG, F("----------------"));
                DPRINTLN(DBG_DEBUG, F("mqtt.h:reconnect"));
                
                rc = mClient->connected();
                DPRINTLN(DBG_DEBUG, F(" Connected 1 = ") + String(rc) + " ");
                DPRINTLN(DBG_DEBUG, F("      MQTT Client->_state   ") + String(mClient->state()) + "  ");
                DPRINTLN(DBG_DEBUG, F("      WIFI Client.status    ") + String(mEspClient.status()) + "  ");
          }
            else
                rc = false;


            if(!rc) {
                // HorstG-57; Test wegen MQTT Reconnect problem

                if ( mReconnect == 0 ) {
                   // es hat schon mal einen Connect gegeben
                   DPRINTLN(DBG_INFO, F("mqtt.h:Disconnect"));
                   DPRINTLN(DBG_INFO, F("   1. MQTT Client->_state   ") + String(mClient->state()) + "  ");
                   DPRINTLN(DBG_INFO, F("      WIFI Client.status    ") + String(mEspClient.status()) + "  ");
                   
                   // der Server und der Port müssen neu gesetzt werden, 
                   // da ein Loss_Connect die Werte zerstört hat.
                   mClient->setServer(mBroker, mPort);
                   mClient->setBufferSize(MQTT_MAX_PACKET_SIZE);
                
                   // Verzögerung des MQTT reconnects um 5 Sekunden
                   mReconnect = 5;

                }   
                
                
                if ( mReconnect < 2 ) {
                    // ein MQTT Connect findet statt wenn mreconnect auf 1 bzw 0 steht.
                    //   1 = es hat noch nie ein MQTT Connect stattgefunden
                    //   0 = ein MQTT Connect war schon mal erfogreich
                    
                    if((strlen(mUser) > 0) && (strlen(mPwd) > 0))
                        rc = mClient->connect(mDeviceName, mUser, mPwd);
                    else
                        rc = mClient->connect(mDeviceName);
                    
                    DPRINTLN(DBG_DEBUG, F(" Connect = ") + String(rc) + " ");
                    DPRINTLN(DBG_DEBUG, F("   2. MQTT Client->_state ") + String(mClient->state()) + "  ");
                    DPRINTLN(DBG_DEBUG, F("      WIFI Client.status   ") + String(mEspClient.status()) + "  ");
 
                    if ( rc ) 
                        mReconnect = 0;
                    else
                        mReconnect = 5;

                    // rc = mClient->connected();
                    // DPRINTLN(DBG_DEBUG, F(" Connected  = ") + String(rc) + " ");

                }


            }
        }

        WiFiClient mEspClient;
        PubSubClient *mClient;

        bool mAddressSet;
        bool mInsend;
        uint16_t mPort;
        uint8_t mReconnect;
        char mUser[MQTT_USER_LEN];
        char mPwd[MQTT_PWD_LEN];
        char mTopic[MQTT_TOPIC_LEN];
        char mDeviceName[DEVNAME_LEN];
        char mBroker[MQTT_ADDR_LEN];
};

#endif /*__MQTT_H_*/
