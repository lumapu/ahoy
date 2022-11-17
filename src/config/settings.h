//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "../utils/dbg.h"
#include "../defines.h"

/**
 * More info:
 * https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html#flash-layout
 * */

typedef struct {
    uint8_t ip[4];      // ip address
    uint8_t mask[4];    // sub mask
    uint8_t dns1[4];    // dns 1
    uint8_t dns2[4];    // dns 2
    uint8_t gateway[4]; // standard gateway
} cfgIp_t;

typedef struct {
    char deviceName[DEVNAME_LEN];
    char adminPwd[PWD_LEN];

    // wifi
    char stationSsid[SSID_LEN];
    char stationPwd[PWD_LEN];

    cfgIp_t ip;
} cfgSys_t;

typedef struct {
    uint16_t sendInterval;
    uint8_t maxRetransPerPyld;
    uint8_t pinCs;
    uint8_t pinCe;
    uint8_t pinIrq;
    uint8_t amplifierPower;
} cfgNrf24_t;

typedef struct {
    char addr[NTP_ADDR_LEN];
    uint16_t port;
} cfgNtp_t;

typedef struct {
    float lat;
    float lon;
    bool disNightCom; // disable night communication
} cfgSun_t;

typedef struct {
    uint16_t interval;
    bool showIv;
    bool debug;
} cfgSerial_t;

typedef struct {
    uint8_t led0; // first LED pin
    uint8_t led1; // second LED pin
} cfgLed_t;

typedef struct {
    char broker[MQTT_ADDR_LEN];
    uint16_t port;
    char user[MQTT_USER_LEN];
    char pwd[MQTT_PWD_LEN];
    char topic[MQTT_TOPIC_LEN];
} cfgMqtt_t;

typedef struct {
    bool enabled;
    char name[MAX_NAME_LENGTH];
    serial_u serial;
    uint16_t chMaxPwr[4];
    char chName[4][MAX_NAME_LENGTH];
} cfgIv_t;

typedef struct {
    bool enabled;
    cfgIv_t iv[MAX_NUM_INVERTERS];
} cfgInst_t;

typedef struct {
    cfgSys_t    sys;
    cfgNrf24_t  nrf;
    cfgNtp_t    ntp;
    cfgSun_t    sun;
    cfgSerial_t serial;
    cfgMqtt_t   mqtt;
    cfgLed_t    led;
    cfgInst_t   inst;
} settings_t;

class settings {
    public:
        settings() {}

        void setup() {
            DPRINTLN(DBG_INFO, F("Initializing FS .."));

            mValid = false;
            #if !defined(ESP32)
                LittleFSConfig cfg;
                cfg.setAutoFormat(false);
                LittleFS.setConfig(cfg);
                #define LITTLFS_TRUE
                #define LITTLFS_FALSE
            #else
                #define LITTLFS_TRUE true
                #define LITTLFS_FALSE false
            #endif

            if(!LittleFS.begin(LITTLFS_FALSE)) {
                DPRINTLN(DBG_INFO, F(".. format .."));
                LittleFS.format();
                if(LittleFS.begin(LITTLFS_TRUE))
                    DPRINTLN(DBG_INFO, F(".. success"));
                else
                    DPRINTLN(DBG_INFO, F(".. failed"));

            }
            else
                DPRINTLN(DBG_INFO, F(" .. done"));

            readSettings();
        }

        // should be used before OTA
        void stop() {
            LittleFS.end();
            DPRINTLN(DBG_INFO, F("FS stopped"));
        }

        void getPtr(settings_t *&cfg) {
            cfg = &mCfg;
        }

        bool getValid(void) {
            return mValid;
        }

        void getInfo(uint32_t *used, uint32_t *size) {
            #if !defined(ESP32)
                FSInfo info;
                LittleFS.info(info);
                *used = info.usedBytes;
                *size = info.totalBytes;

                DPRINTLN(DBG_INFO, F("-- FILESYSTEM INFO --"));
                DPRINTLN(DBG_INFO, String(info.usedBytes) + F(" of ") + String(info.totalBytes)  + F(" used"));
            #else
                DPRINTLN(DBG_WARN, F("not supported by ESP32"));
            #endif
        }

        void readSettings(void) {
            loadDefaults();
            File fp = LittleFS.open("/settings.json", "r");
            if(!fp)
                DPRINTLN(DBG_WARN, F("failed to load json, using default config"));
            else {
                //DPRINTLN(DBG_INFO, fp.readString());
                //fp.seek(0, SeekSet);
                DynamicJsonDocument root(4096);
                DeserializationError err = deserializeJson(root, fp);
                if(!err) {
                    mValid = true;
                    jsonWifi(root["wifi"]);
                    jsonNrf(root["nrf"]);
                    jsonNtp(root["ntp"]);
                    jsonSun(root["sun"]);
                    jsonSerial(root["serial"]);
                    jsonMqtt(root["mqtt"]);
                    jsonLed(root["led"]);
                    jsonInst(root["inst"]);
                }
                else {
                    Serial.println(F("failed to parse json, using default config"));
                }

                fp.close();
            }
        }

        bool saveSettings(void) {
            DPRINTLN(DBG_INFO, F("save settings"));
            File fp = LittleFS.open("/settings.json", "w");
            if(!fp) {
                DPRINTLN(DBG_ERROR, F("can't open settings file!"));
                return false;
            }

            DynamicJsonDocument json(4096);
            JsonObject root = json.to<JsonObject>();
            jsonWifi(root.createNestedObject(F("wifi")), true);
            jsonNrf(root.createNestedObject(F("nrf")), true);
            jsonNtp(root.createNestedObject(F("ntp")), true);
            jsonSun(root.createNestedObject(F("sun")), true);
            jsonSerial(root.createNestedObject(F("serial")), true);
            jsonMqtt(root.createNestedObject(F("mqtt")), true);
            jsonLed(root.createNestedObject(F("led")), true);
            jsonInst(root.createNestedObject(F("inst")), true);

            if(0 == serializeJson(root, fp)) {
                DPRINTLN(DBG_ERROR, F("can't write settings file!"));
                return false;
            }
            fp.close();

            return true;
        }

        bool eraseSettings(bool eraseWifi = false) {
            loadDefaults(eraseWifi);
            return saveSettings();
        }

        String ip2Str(uint8_t ip[]) {
            return String(ip[0]) + F(".")
                + String(ip[1]) + F(".")
                + String(ip[2]) + F(".")
                + String(ip[3]);
        }

        void ip2Arr(uint8_t ip[], const char *ipStr) {
            char *tmp = new char[strlen(ipStr)];
            strncpy(tmp, ipStr, strlen(ipStr));
            char *p = strtok(tmp, ".");
            uint8_t i = 0;
            while(NULL != p) {
                ip[i++] = atoi(p);
                p = strtok(NULL, ".");
            }
            delete[] tmp;
        }

    private:
        void loadDefaults(bool wifi = true) {
            DPRINTLN(DBG_INFO, F("loadDefaults"));
            if(wifi) {
                snprintf(mCfg.sys.stationSsid, SSID_LEN,    FB_WIFI_SSID);
                snprintf(mCfg.sys.stationPwd,  PWD_LEN,     FB_WIFI_PWD);
            }
            else {
                cfgSys_t tmp;
                memset(&tmp.adminPwd, 0, PWD_LEN);
                memcpy(&tmp, &mCfg.sys, sizeof(cfgSys_t));
                memset(&mCfg, 0, sizeof(settings_t));
                memcpy(&mCfg.sys, &tmp, sizeof(cfgSys_t));
            }
            snprintf(mCfg.sys.deviceName,  DEVNAME_LEN, DEF_DEVICE_NAME);

            mCfg.nrf.sendInterval      = SEND_INTERVAL;
            mCfg.nrf.maxRetransPerPyld = DEF_MAX_RETRANS_PER_PYLD;
            mCfg.nrf.pinCs             = DEF_CS_PIN;
            mCfg.nrf.pinCe             = DEF_CE_PIN;
            mCfg.nrf.pinIrq            = DEF_IRQ_PIN;
            mCfg.nrf.amplifierPower    = DEF_AMPLIFIERPOWER & 0x03;

            snprintf(mCfg.ntp.addr, NTP_ADDR_LEN, "%s", DEF_NTP_SERVER_NAME);
            mCfg.ntp.port = DEF_NTP_PORT;

            mCfg.sun.lat         = 0.0;
            mCfg.sun.lon         = 0.0;
            mCfg.sun.disNightCom = false;

            mCfg.serial.interval = SERIAL_INTERVAL;
            mCfg.serial.showIv   = false;
            mCfg.serial.debug    = false;

            mCfg.mqtt.port = DEF_MQTT_PORT;
            snprintf(mCfg.mqtt.broker, MQTT_ADDR_LEN,  "%s", DEF_MQTT_BROKER);
            snprintf(mCfg.mqtt.user,   MQTT_USER_LEN,  "%s", DEF_MQTT_USER);
            snprintf(mCfg.mqtt.pwd,    MQTT_PWD_LEN,   "%s", DEF_MQTT_PWD);
            snprintf(mCfg.mqtt.topic,  MQTT_TOPIC_LEN, "%s", DEF_MQTT_TOPIC);

            mCfg.led.led0 = DEF_LED0_PIN;
            mCfg.led.led1 = DEF_LED1_PIN;
        }

        void jsonWifi(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("ssid")] = mCfg.sys.stationSsid;
                obj[F("pwd")]  = mCfg.sys.stationPwd;
                obj[F("dev")]  = mCfg.sys.deviceName;
                obj[F("adm")]  = mCfg.sys.adminPwd;
                obj[F("ip")]   = ip2Str(mCfg.sys.ip.ip);
                obj[F("mask")] = ip2Str(mCfg.sys.ip.mask);
                obj[F("dns1")] = ip2Str(mCfg.sys.ip.dns1);
                obj[F("dns2")] = ip2Str(mCfg.sys.ip.dns2);
                obj[F("gtwy")] = ip2Str(mCfg.sys.ip.gateway);
            } else {
                snprintf(mCfg.sys.stationSsid, SSID_LEN,    "%s", obj[F("ssid")].as<const char*>());
                snprintf(mCfg.sys.stationPwd,  PWD_LEN,     "%s", obj[F("pwd")].as<const char*>());
                snprintf(mCfg.sys.deviceName,  DEVNAME_LEN, "%s", obj[F("dev")].as<const char*>());
                snprintf(mCfg.sys.adminPwd,    PWD_LEN,     "%s", obj[F("adm")].as<const char*>());
                ip2Arr(mCfg.sys.ip.ip,      obj[F("ip")]);
                ip2Arr(mCfg.sys.ip.mask,    obj[F("mask")]);
                ip2Arr(mCfg.sys.ip.dns1,    obj[F("dns1")]);
                ip2Arr(mCfg.sys.ip.dns2,    obj[F("dns2")]);
                ip2Arr(mCfg.sys.ip.gateway, obj[F("gtwy")]);
            }
        }

        void jsonNrf(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("intvl")]     = mCfg.nrf.sendInterval;
                obj[F("maxRetry")]  = mCfg.nrf.maxRetransPerPyld;
                obj[F("cs")]        = mCfg.nrf.pinCs;
                obj[F("ce")]        = mCfg.nrf.pinCe;
                obj[F("irq")]       = mCfg.nrf.pinIrq;
                obj[F("pwr")]       = mCfg.nrf.amplifierPower;
            } else {
                mCfg.nrf.sendInterval      = obj[F("intvl")];
                mCfg.nrf.maxRetransPerPyld = obj[F("maxRetry")];
                mCfg.nrf.pinCs             = obj[F("cs")];
                mCfg.nrf.pinCe             = obj[F("ce")];
                mCfg.nrf.pinIrq            = obj[F("irq")];
                mCfg.nrf.amplifierPower    = obj[F("pwr")];
            }
        }

        void jsonNtp(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("addr")] = mCfg.ntp.addr;
                obj[F("port")] = mCfg.ntp.port;
            } else {
                snprintf(mCfg.ntp.addr, NTP_ADDR_LEN, "%s", obj[F("addr")].as<const char*>());
                mCfg.ntp.port = obj[F("port")];
            }
        }

        void jsonSun(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("lat")] = mCfg.sun.lat;
                obj[F("lon")] = mCfg.sun.lon;
                obj[F("dis")] = mCfg.sun.disNightCom;
            } else {
                mCfg.sun.lat         = obj[F("lat")];
                mCfg.sun.lon         = obj[F("lon")];
                mCfg.sun.disNightCom = obj[F("dis")];
            }
        }

        void jsonSerial(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("intvl")] = mCfg.serial.interval;
                obj[F("show")]  = mCfg.serial.showIv;
                obj[F("debug")] = mCfg.serial.debug;
            } else {
                mCfg.serial.interval = obj[F("intvl")];
                mCfg.serial.showIv   = obj[F("show")];
                mCfg.serial.debug    = obj[F("debug")];
            }
        }

        void jsonMqtt(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("broker")] = mCfg.mqtt.broker;
                obj[F("port")]   = mCfg.mqtt.port;
                obj[F("user")]   = mCfg.mqtt.user;
                obj[F("pwd")]    = mCfg.mqtt.pwd;
                obj[F("topic")]  = mCfg.mqtt.topic;
            } else {
                mCfg.mqtt.port = obj[F("port")];
                snprintf(mCfg.mqtt.broker, MQTT_ADDR_LEN,  "%s", obj[F("broker")].as<const char*>());
                snprintf(mCfg.mqtt.user,   MQTT_USER_LEN,  "%s", obj[F("user")].as<const char*>());
                snprintf(mCfg.mqtt.pwd,    MQTT_PWD_LEN,   "%s", obj[F("pwd")].as<const char*>());
                snprintf(mCfg.mqtt.topic,  MQTT_TOPIC_LEN, "%s", obj[F("topic")].as<const char*>());
            }
        }

        void jsonLed(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("0")] = mCfg.led.led0;
                obj[F("1")] = mCfg.led.led1;
            } else {
                mCfg.led.led0 = obj[F("0")];
                mCfg.led.led1 = obj[F("1")];
            }
        }

        void jsonInst(JsonObject obj, bool set = false) {
            if(set)
                obj[F("en")]    = mCfg.inst.enabled;
            else
                mCfg.inst.enabled = obj[F("en")];

            JsonArray ivArr;
            if(set)
                ivArr = obj.createNestedArray(F("iv"));
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                if(set)
                    jsonIv(ivArr.createNestedObject(), &mCfg.inst.iv[i], true);
                else
                    jsonIv(obj[F("iv")][i], &mCfg.inst.iv[i]);
            }
        }

        void jsonIv(JsonObject obj, cfgIv_t *cfg, bool set = false) {
            if(set) {
                obj[F("en")]     = cfg->enabled;
                obj[F("name")]   = cfg->name;
                obj[F("sn")] = cfg->serial.u64;
                for(uint8_t i = 0; i < 4; i++) {
                    obj[F("pwr")][i]  = cfg->chMaxPwr[i];
                    obj[F("chName")][i] = cfg->chName[i];
                }
            } else {
                cfg->enabled = obj[F("en")];
                snprintf(cfg->name, MAX_NAME_LENGTH, "%s", obj[F("name")].as<const char*>());
                cfg->serial.u64 = obj[F("sn")];
                for(uint8_t i = 0; i < 4; i++) {
                    cfg->chMaxPwr[i] = obj[F("pwr")][i];
                    snprintf(cfg->chName[i], MAX_NAME_LENGTH, "%s", obj[F("chName")][i].as<const char*>());
                }
            }
        }

        settings_t mCfg;
        bool mValid;
};

#endif /*__SETTINGS_H__*/
