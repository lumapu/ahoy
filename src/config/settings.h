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
#include "../utils/helper.h"
#include "../defines.h"

/**
 * More info:
 * https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html#flash-layout
 * */
#define DEF_PIN_OFF         255


#define PROT_MASK_INDEX     0x0001
#define PROT_MASK_LIVE      0x0002
#define PROT_MASK_SERIAL    0x0004
#define PROT_MASK_SETUP     0x0008
#define PROT_MASK_UPDATE    0x0010
#define PROT_MASK_SYSTEM    0x0020
#define PROT_MASK_API       0x0040
#define PROT_MASK_MQTT      0x0080

#define DEF_PROT_INDEX      0x0001
#define DEF_PROT_LIVE       0x0000
#define DEF_PROT_SERIAL     0x0004
#define DEF_PROT_SETUP      0x0008
#define DEF_PROT_UPDATE     0x0010
#define DEF_PROT_SYSTEM     0x0020
#define DEF_PROT_API        0x0000
#define DEF_PROT_MQTT       0x0000


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
    uint16_t protectionMask;

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
    uint16_t offsetSec;
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
    uint16_t interval;
    bool rstYieldMidNight;
    bool rstValsNotAvail;
    bool rstValsCommStop;
} cfgMqtt_t;

typedef struct {
    bool enabled;
    char name[MAX_NAME_LENGTH];
    serial_u serial;
    uint16_t chMaxPwr[4];
    char chName[4][MAX_NAME_LENGTH];
    uint32_t yieldCor; // YieldTotal correction value
} cfgIv_t;

typedef struct {
    bool enabled;
    cfgIv_t iv[MAX_NUM_INVERTERS];
} cfgInst_t;

typedef struct {
    uint8_t type;
    bool pwrSaveAtIvOffline;
    bool logoEn;
    bool pxShift;
    uint16_t wakeUp;
    uint16_t sleepAt;
    uint8_t contrast;
    uint8_t pin0;
    uint8_t pin1;
} display_t;

typedef struct {
    display_t display;
} plugins_t;

typedef struct {
    cfgSys_t    sys;
    cfgNrf24_t  nrf;
    cfgNtp_t    ntp;
    cfgSun_t    sun;
    cfgSerial_t serial;
    cfgMqtt_t   mqtt;
    cfgLed_t    led;
    cfgInst_t   inst;
    plugins_t   plugin;
    bool        valid;
} settings_t;

class settings {
    public:
        settings() {}

        void setup() {
            DPRINTLN(DBG_INFO, F("Initializing FS .."));

            mCfg.valid = false;
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
                if(LittleFS.begin(LITTLFS_TRUE)) {
                    DPRINTLN(DBG_INFO, F(".. success"));
                } else {
                    DPRINTLN(DBG_INFO, F(".. failed"));
                }

            }
            else
                DPRINTLN(DBG_INFO, F(" .. done"));

            readSettings("/settings.json");
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
            return mCfg.valid;
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

        bool readSettings(const char* path) {
            bool success = false;
            loadDefaults();
            File fp = LittleFS.open(path, "r");
            if(!fp)
                DPRINTLN(DBG_WARN, F("failed to load json, using default config"));
            else {
                //DPRINTLN(DBG_INFO, fp.readString());
                //fp.seek(0, SeekSet);
                DynamicJsonDocument root(4096);
                DeserializationError err = deserializeJson(root, fp);
                if(!err && (root.size() > 0)) {
                    mCfg.valid = true;
                    jsonWifi(root[F("wifi")]);
                    jsonNrf(root[F("nrf")]);
                    jsonNtp(root[F("ntp")]);
                    jsonSun(root[F("sun")]);
                    jsonSerial(root[F("serial")]);
                    jsonMqtt(root[F("mqtt")]);
                    jsonLed(root[F("led")]);
                    jsonPlugin(root[F("plugin")]);
                    jsonInst(root[F("inst")]);
                    success = true;
                }
                else {
                    Serial.println(F("failed to parse json, using default config"));
                }

                fp.close();
            }
            return success;
        }

        bool saveSettings(void) {
            DPRINTLN(DBG_DEBUG, F("save settings"));
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
            jsonPlugin(root.createNestedObject(F("plugin")), true);
            jsonInst(root.createNestedObject(F("inst")), true);

            if(0 == serializeJson(root, fp)) {
                DPRINTLN(DBG_ERROR, F("can't write settings file!"));
                return false;
            }
            fp.close();

            return true;
        }

        bool eraseSettings(bool eraseWifi = false) {
            if(true == eraseWifi)
                return LittleFS.format();
            loadDefaults(!eraseWifi);
            return saveSettings();
        }

    private:
        void loadDefaults(bool keepWifi = false) {
            DPRINTLN(DBG_VERBOSE, F("loadDefaults"));

            cfgSys_t tmp;
            if(keepWifi) {
                // copy contents which should not be deleted
                memset(&tmp.adminPwd, 0, PWD_LEN);
                memcpy(&tmp, &mCfg.sys, sizeof(cfgSys_t));
            }
            // erase all settings and reset to default
            memset(&mCfg, 0, sizeof(settings_t));
            mCfg.sys.protectionMask = DEF_PROT_INDEX | DEF_PROT_LIVE | DEF_PROT_SERIAL | DEF_PROT_SETUP
                                    | DEF_PROT_UPDATE | DEF_PROT_SYSTEM | DEF_PROT_API | DEF_PROT_MQTT;
            // restore temp settings
            if(keepWifi)
                memcpy(&mCfg.sys, &tmp, sizeof(cfgSys_t));
            else {
                snprintf(mCfg.sys.stationSsid, SSID_LEN, FB_WIFI_SSID);
                snprintf(mCfg.sys.stationPwd,  PWD_LEN,  FB_WIFI_PWD);
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
            mCfg.sun.offsetSec   = 0;

            mCfg.serial.interval = SERIAL_INTERVAL;
            mCfg.serial.showIv   = false;
            mCfg.serial.debug    = false;

            mCfg.mqtt.port = DEF_MQTT_PORT;
            snprintf(mCfg.mqtt.broker, MQTT_ADDR_LEN,  "%s", DEF_MQTT_BROKER);
            snprintf(mCfg.mqtt.user,   MQTT_USER_LEN,  "%s", DEF_MQTT_USER);
            snprintf(mCfg.mqtt.pwd,    MQTT_PWD_LEN,   "%s", DEF_MQTT_PWD);
            snprintf(mCfg.mqtt.topic,  MQTT_TOPIC_LEN, "%s", DEF_MQTT_TOPIC);
            mCfg.mqtt.interval = 0; // off
            mCfg.mqtt.rstYieldMidNight  = false;
            mCfg.mqtt.rstValsNotAvail = false;
            mCfg.mqtt.rstValsCommStop   = false;

            mCfg.led.led0 = DEF_PIN_OFF;
            mCfg.led.led1 = DEF_PIN_OFF;

            memset(&mCfg.inst, 0, sizeof(cfgInst_t));

            mCfg.plugin.display.pwrSaveAtIvOffline = false;
            mCfg.plugin.display.contrast           = 60;
            mCfg.plugin.display.logoEn             = true;
            mCfg.plugin.display.pxShift            = true;
            mCfg.plugin.display.pin0               = DEF_PIN_OFF; // SCL
            mCfg.plugin.display.pin1               = DEF_PIN_OFF; // SDA
        }

        void jsonWifi(JsonObject obj, bool set = false) {
            if(set) {
                char buf[16];
                obj[F("ssid")] = mCfg.sys.stationSsid;
                obj[F("pwd")]  = mCfg.sys.stationPwd;
                obj[F("dev")]  = mCfg.sys.deviceName;
                obj[F("adm")]  = mCfg.sys.adminPwd;
                obj[F("prot_mask")] = mCfg.sys.protectionMask;
                ah::ip2Char(mCfg.sys.ip.ip, buf);      obj[F("ip")]   = String(buf);
                ah::ip2Char(mCfg.sys.ip.mask, buf);    obj[F("mask")] = String(buf);
                ah::ip2Char(mCfg.sys.ip.dns1, buf);    obj[F("dns1")] = String(buf);
                ah::ip2Char(mCfg.sys.ip.dns2, buf);    obj[F("dns2")] = String(buf);
                ah::ip2Char(mCfg.sys.ip.gateway, buf); obj[F("gtwy")] = String(buf);
            } else {
                snprintf(mCfg.sys.stationSsid, SSID_LEN,    "%s", obj[F("ssid")].as<const char*>());
                snprintf(mCfg.sys.stationPwd,  PWD_LEN,     "%s", obj[F("pwd")].as<const char*>());
                snprintf(mCfg.sys.deviceName,  DEVNAME_LEN, "%s", obj[F("dev")].as<const char*>());
                snprintf(mCfg.sys.adminPwd,    PWD_LEN,     "%s", obj[F("adm")].as<const char*>());
                mCfg.sys.protectionMask = obj[F("prot_mask")];
                ah::ip2Arr(mCfg.sys.ip.ip,      obj[F("ip")].as<const char*>());
                ah::ip2Arr(mCfg.sys.ip.mask,    obj[F("mask")].as<const char*>());
                ah::ip2Arr(mCfg.sys.ip.dns1,    obj[F("dns1")].as<const char*>());
                ah::ip2Arr(mCfg.sys.ip.dns2,    obj[F("dns2")].as<const char*>());
                ah::ip2Arr(mCfg.sys.ip.gateway, obj[F("gtwy")].as<const char*>());

                if(mCfg.sys.protectionMask == 0)
                    mCfg.sys.protectionMask = DEF_PROT_INDEX | DEF_PROT_LIVE | DEF_PROT_SERIAL | DEF_PROT_SETUP
                                            | DEF_PROT_UPDATE | DEF_PROT_SYSTEM | DEF_PROT_API | DEF_PROT_MQTT;
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
                obj[F("lat")]  = mCfg.sun.lat;
                obj[F("lon")]  = mCfg.sun.lon;
                obj[F("dis")]  = mCfg.sun.disNightCom;
                obj[F("offs")] = mCfg.sun.offsetSec;
            } else {
                mCfg.sun.lat         = obj[F("lat")];
                mCfg.sun.lon         = obj[F("lon")];
                mCfg.sun.disNightCom = obj[F("dis")];
                mCfg.sun.offsetSec   = obj[F("offs")];
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
                obj[F("intvl")]  = mCfg.mqtt.interval;
                obj[F("rstMidNight")] = (bool)mCfg.mqtt.rstYieldMidNight;
                obj[F("rstNotAvail")] = (bool)mCfg.mqtt.rstValsNotAvail;
                obj[F("rstComStop")]  = (bool)mCfg.mqtt.rstValsCommStop;

            } else {
                mCfg.mqtt.port     = obj[F("port")];
                mCfg.mqtt.interval = obj[F("intvl")];
                mCfg.mqtt.rstYieldMidNight = (bool)obj["rstMidNight"];
                mCfg.mqtt.rstValsNotAvail  = (bool)obj["rstNotAvail"];
                mCfg.mqtt.rstValsCommStop  = (bool)obj["rstComStop"];
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

        void jsonPlugin(JsonObject obj, bool set = false) {
            if(set) {
                JsonObject disp = obj.createNestedObject("disp");
                disp[F("type")]     = mCfg.plugin.display.type;
                disp[F("pwrSafe")]  = (bool)mCfg.plugin.display.pwrSaveAtIvOffline;
                disp[F("logo")]     = (bool)mCfg.plugin.display.logoEn;
                disp[F("pxShift")]  = (bool)mCfg.plugin.display.pxShift;
                disp[F("wake")]     = mCfg.plugin.display.wakeUp;
                disp[F("sleep")]    = mCfg.plugin.display.sleepAt;
                disp[F("contrast")] = mCfg.plugin.display.contrast;
                disp[F("pin0")]     = mCfg.plugin.display.pin0;
                disp[F("pin1")]     = mCfg.plugin.display.pin1;
            } else {
                JsonObject disp = obj["disp"];
                mCfg.plugin.display.type               = disp[F("type")];
                mCfg.plugin.display.pwrSaveAtIvOffline = (bool) disp[F("pwrSafe")];
                mCfg.plugin.display.logoEn             = (bool) disp[F("logo")];
                mCfg.plugin.display.pxShift            = (bool) disp[F("pxShift")];
                mCfg.plugin.display.wakeUp             = disp[F("wake")];
                mCfg.plugin.display.sleepAt            = disp[F("sleep")];
                mCfg.plugin.display.contrast           = disp[F("contrast")];
                mCfg.plugin.display.pin0               = disp[F("pin0")];
                mCfg.plugin.display.pin1               = disp[F("pin1")];
            }
        }

        void jsonInst(JsonObject obj, bool set = false) {
            if(set)
                obj[F("en")] = (bool)mCfg.inst.enabled;
            else
                mCfg.inst.enabled = (bool)obj[F("en")];

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
                obj[F("en")]    = (bool)cfg->enabled;
                obj[F("name")]  = cfg->name;
                obj[F("sn")]    = cfg->serial.u64;
                obj[F("yield")] = cfg->yieldCor;
                for(uint8_t i = 0; i < 4; i++) {
                    obj[F("pwr")][i]  = cfg->chMaxPwr[i];
                    obj[F("chName")][i] = cfg->chName[i];
                }
            } else {
                cfg->enabled = (bool)obj[F("en")];
                snprintf(cfg->name, MAX_NAME_LENGTH, "%s", obj[F("name")].as<const char*>());
                cfg->serial.u64 = obj[F("sn")];
                cfg->yieldCor   = obj[F("yield")];
                for(uint8_t i = 0; i < 4; i++) {
                    cfg->chMaxPwr[i] = obj[F("pwr")][i];
                    snprintf(cfg->chName[i], MAX_NAME_LENGTH, "%s", obj[F("chName")][i].as<const char*>());
                }
            }
        }

        settings_t mCfg;
};

#endif /*__SETTINGS_H__*/
