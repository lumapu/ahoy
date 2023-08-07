//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#if defined(F) && defined(ESP32)
  #undef F
  #define F(sl) (sl)
#endif

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "../defines.h"
#include "../utils/dbg.h"
#include "../utils/helper.h"

#if defined(ESP32)
    #define MAX_ALLOWED_BUF_SIZE   ESP.getMaxAllocHeap() - 1024
#else
    #define MAX_ALLOWED_BUF_SIZE   ESP.getMaxFreeBlockSize() - 1024
#endif

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
    bool darkMode;
    bool schedReboot;

#if defined(ETHERNET)
    // ethernet

#else /* defined(ETHERNET) */
    // wifi
    char stationSsid[SSID_LEN];
    char stationPwd[PWD_LEN];
    char apPwd[PWD_LEN];
    bool isHidden;
#endif /* defined(ETHERNET) */

    cfgIp_t ip;
} cfgSys_t;

typedef struct {
    bool enabled;
    uint16_t sendInterval;
    uint8_t maxRetransPerPyld;
    uint8_t pinCs;
    uint8_t pinCe;
    uint8_t pinIrq;
    uint8_t pinMiso;
    uint8_t pinMosi;
    uint8_t pinSclk;
    uint8_t amplifierPower;
} cfgNrf24_t;

typedef struct {
    bool enabled;
    uint8_t pinCsb;
    uint8_t pinFcsb;
    uint8_t pinIrq;
} cfgCmt_t;

typedef struct {
    char addr[NTP_ADDR_LEN];
    uint16_t port;
    uint16_t interval; // in minutes
} cfgNtp_t;

typedef struct {
    float lat;
    float lon;
    bool disNightCom;  // disable night communication
    uint16_t offsetSec;
} cfgSun_t;

typedef struct {
    uint16_t interval;
    bool showIv;
    bool debug;
} cfgSerial_t;

typedef struct {
    uint8_t led0;  // first LED pin
    uint8_t led1;  // second LED pin
    bool led_high_active;  // determines if LEDs are high or low active
} cfgLed_t;

typedef struct {
    char broker[MQTT_ADDR_LEN];
    uint16_t port;
    char clientId[MQTT_CLIENTID_LEN];
    char user[MQTT_USER_LEN];
    char pwd[MQTT_PWD_LEN];
    char topic[MQTT_TOPIC_LEN];
    uint16_t interval;
} cfgMqtt_t;

typedef struct {
    bool enabled;
    char name[MAX_NAME_LENGTH];
    serial_u serial;
    uint16_t chMaxPwr[6];
    double yieldCor[6];  // YieldTotal correction value
    char chName[6][MAX_NAME_LENGTH];
} cfgIv_t;

typedef struct {
    bool enabled;
    cfgIv_t iv[MAX_NUM_INVERTERS];

    bool rstYieldMidNight;
    bool rstValsNotAvail;
    bool rstValsCommStop;
    bool startWithoutTime;
    float yieldEffiency;
} cfgInst_t;

typedef struct {
    uint8_t type;
    bool pwrSaveAtIvOffline;
    bool pxShift;
    uint8_t rot;
    //uint16_t wakeUp;
    //uint16_t sleepAt;
    uint8_t contrast;
    uint8_t disp_data;
    uint8_t disp_clk;
    uint8_t disp_cs;
    uint8_t disp_reset;
    uint8_t disp_busy;
    uint8_t disp_dc;
} display_t;

typedef struct {
    display_t display;
} plugins_t;

typedef struct {
    cfgSys_t    sys;
    cfgNrf24_t  nrf;
    cfgCmt_t    cmt;
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
        settings() {
            mLastSaveSucceed = false;
        }

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

        inline bool getLastSaveSucceed() {
            return mLastSaveSucceed;
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
            loadDefaults();
            File fp = LittleFS.open(path, "r");
            if(!fp)
                DPRINTLN(DBG_WARN, F("failed to load json, using default config"));
            else {
                //DPRINTLN(DBG_INFO, fp.readString());
                //fp.seek(0, SeekSet);
                DynamicJsonDocument root(MAX_ALLOWED_BUF_SIZE);
                DeserializationError err = deserializeJson(root, fp);
                root.shrinkToFit();
                if(!err && (root.size() > 0)) {
                    mCfg.valid = true;
                    if(root.containsKey(F("wifi"))) jsonNetwork(root[F("wifi")]);
                    if(root.containsKey(F("nrf"))) jsonNrf(root[F("nrf")]);
                    #if defined(ESP32)
                    if(root.containsKey(F("cmt"))) jsonCmt(root[F("cmt")]);
                    #endif
                    if(root.containsKey(F("ntp"))) jsonNtp(root[F("ntp")]);
                    if(root.containsKey(F("sun"))) jsonSun(root[F("sun")]);
                    if(root.containsKey(F("serial"))) jsonSerial(root[F("serial")]);
                    if(root.containsKey(F("mqtt"))) jsonMqtt(root[F("mqtt")]);
                    if(root.containsKey(F("led"))) jsonLed(root[F("led")]);
                    if(root.containsKey(F("plugin"))) jsonPlugin(root[F("plugin")]);
                    if(root.containsKey(F("inst"))) jsonInst(root[F("inst")]);
                }
                else {
                    Serial.println(F("failed to parse json, using default config"));
                }

                fp.close();
            }
            return mCfg.valid;
        }

        bool saveSettings() {
            DPRINTLN(DBG_DEBUG, F("save settings"));

            DynamicJsonDocument json(MAX_ALLOWED_BUF_SIZE);
            JsonObject root = json.to<JsonObject>();
            jsonNetwork(root.createNestedObject(F("wifi")), true);
            jsonNrf(root.createNestedObject(F("nrf")), true);
            #if defined(ESP32)
            jsonCmt(root.createNestedObject(F("cmt")), true);
            #endif
            jsonNtp(root.createNestedObject(F("ntp")), true);
            jsonSun(root.createNestedObject(F("sun")), true);
            jsonSerial(root.createNestedObject(F("serial")), true);
            jsonMqtt(root.createNestedObject(F("mqtt")), true);
            jsonLed(root.createNestedObject(F("led")), true);
            jsonPlugin(root.createNestedObject(F("plugin")), true);
            jsonInst(root.createNestedObject(F("inst")), true);

            DPRINT(DBG_INFO, F("memory usage: "));
            DBGPRINTLN(String(json.memoryUsage()));
            DPRINT(DBG_INFO, F("capacity: "));
            DBGPRINTLN(String(json.capacity()));
            DPRINT(DBG_INFO, F("max alloc: "));
            DBGPRINTLN(String(MAX_ALLOWED_BUF_SIZE));

            if(json.overflowed()) {
                DPRINTLN(DBG_ERROR, F("buffer too small!"));
                mLastSaveSucceed = false;
                return false;
            }

            File fp = LittleFS.open("/settings.json", "w");
            if(!fp) {
                DPRINTLN(DBG_ERROR, F("can't open settings file!"));
                mLastSaveSucceed = false;
                return false;
            }

            if(0 == serializeJson(root, fp)) {
                DPRINTLN(DBG_ERROR, F("can't write settings file!"));
                mLastSaveSucceed = false;
                return false;
            }
            fp.close();

            DPRINTLN(DBG_INFO, F("settings saved"));
            mLastSaveSucceed = true;
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
            mCfg.sys.darkMode = false;
            mCfg.sys.schedReboot = false;
            // restore temp settings
            #if defined(ETHERNET)
            memcpy(&mCfg.sys, &tmp, sizeof(cfgSys_t));
            #else /* defined(ETHERNET) */
            if(keepWifi)
                memcpy(&mCfg.sys, &tmp, sizeof(cfgSys_t));
            else {
                snprintf(mCfg.sys.stationSsid, SSID_LEN, FB_WIFI_SSID);
                snprintf(mCfg.sys.stationPwd,  PWD_LEN,  FB_WIFI_PWD);
                snprintf(mCfg.sys.apPwd,       PWD_LEN,  WIFI_AP_PWD);
                mCfg.sys.isHidden = false;
            }
            #endif /* defined(ETHERNET) */

            snprintf(mCfg.sys.deviceName,  DEVNAME_LEN, DEF_DEVICE_NAME);

            mCfg.nrf.sendInterval      = SEND_INTERVAL;
            mCfg.nrf.maxRetransPerPyld = DEF_MAX_RETRANS_PER_PYLD;
            mCfg.nrf.pinCs             = DEF_CS_PIN;
            mCfg.nrf.pinCe             = DEF_CE_PIN;
            mCfg.nrf.pinIrq            = DEF_IRQ_PIN;
            mCfg.nrf.pinMiso           = DEF_MISO_PIN;
            mCfg.nrf.pinMosi           = DEF_MOSI_PIN;
            mCfg.nrf.pinSclk           = DEF_SCLK_PIN;

            mCfg.nrf.amplifierPower    = DEF_AMPLIFIERPOWER & 0x03;
            mCfg.nrf.enabled           = true;

            mCfg.cmt.pinCsb            = DEF_PIN_OFF;
            mCfg.cmt.pinFcsb           = DEF_PIN_OFF;
            mCfg.cmt.pinIrq            = DEF_PIN_OFF;
            mCfg.cmt.enabled           = false;

            snprintf(mCfg.ntp.addr, NTP_ADDR_LEN, "%s", DEF_NTP_SERVER_NAME);
            mCfg.ntp.port = DEF_NTP_PORT;
            mCfg.ntp.interval = 720;

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

            mCfg.inst.rstYieldMidNight = false;
            mCfg.inst.rstValsNotAvail  = false;
            mCfg.inst.rstValsCommStop  = false;
            mCfg.inst.startWithoutTime = false;
            mCfg.inst.yieldEffiency    = 0.955f;

            mCfg.led.led0 = DEF_PIN_OFF;
            mCfg.led.led1 = DEF_PIN_OFF;
            mCfg.led.led_high_active = false;

            memset(&mCfg.inst, 0, sizeof(cfgInst_t));

            mCfg.plugin.display.pwrSaveAtIvOffline = false;
            mCfg.plugin.display.contrast = 60;
            mCfg.plugin.display.pxShift = true;
            mCfg.plugin.display.rot = 0;
            mCfg.plugin.display.disp_data  = DEF_PIN_OFF;  // SDA
            mCfg.plugin.display.disp_clk   = DEF_PIN_OFF;   // SCL
            mCfg.plugin.display.disp_cs    = DEF_PIN_OFF;
            mCfg.plugin.display.disp_reset = DEF_PIN_OFF;
            mCfg.plugin.display.disp_busy  = DEF_PIN_OFF;
            mCfg.plugin.display.disp_dc    = DEF_PIN_OFF;
       }

        void jsonNetwork(JsonObject obj, bool set = false) {
            if(set) {
                char buf[16];
                #if !defined(ETHERNET)
                obj[F("ssid")] = mCfg.sys.stationSsid;
                obj[F("pwd")]  = mCfg.sys.stationPwd;
                obj[F("ap_pwd")]  = mCfg.sys.apPwd;
                obj[F("hidd")] = (bool) mCfg.sys.isHidden;
                #endif /* !defined(ETHERNET) */
                obj[F("dev")]  = mCfg.sys.deviceName;
                obj[F("adm")]  = mCfg.sys.adminPwd;
                obj[F("prot_mask")] = mCfg.sys.protectionMask;
                obj[F("dark")] = mCfg.sys.darkMode;
                obj[F("reb")] = (bool) mCfg.sys.schedReboot;
                ah::ip2Char(mCfg.sys.ip.ip, buf);      obj[F("ip")]   = String(buf);
                ah::ip2Char(mCfg.sys.ip.mask, buf);    obj[F("mask")] = String(buf);
                ah::ip2Char(mCfg.sys.ip.dns1, buf);    obj[F("dns1")] = String(buf);
                ah::ip2Char(mCfg.sys.ip.dns2, buf);    obj[F("dns2")] = String(buf);
                ah::ip2Char(mCfg.sys.ip.gateway, buf); obj[F("gtwy")] = String(buf);
            } else {
                #if !defined(ETHERNET)
                getChar(obj, F("ssid"), mCfg.sys.stationSsid, SSID_LEN);
                getChar(obj, F("pwd"), mCfg.sys.stationPwd, PWD_LEN);
                getChar(obj, F("ap_pwd"), mCfg.sys.apPwd, PWD_LEN);
                getVal<bool>(obj, F("hidd"), &mCfg.sys.isHidden);
                #endif /* !defined(ETHERNET) */
                getChar(obj, F("dev"), mCfg.sys.deviceName, DEVNAME_LEN);
                getChar(obj, F("adm"), mCfg.sys.adminPwd, PWD_LEN);
                getVal<uint16_t>(obj, F("prot_mask"), &mCfg.sys.protectionMask);
                getVal<bool>(obj, F("dark"), &mCfg.sys.darkMode);
                getVal<bool>(obj, F("reb"), &mCfg.sys.schedReboot);
                if(obj.containsKey(F("ip"))) ah::ip2Arr(mCfg.sys.ip.ip,      obj[F("ip")].as<const char*>());
                if(obj.containsKey(F("mask"))) ah::ip2Arr(mCfg.sys.ip.mask,    obj[F("mask")].as<const char*>());
                if(obj.containsKey(F("dns1"))) ah::ip2Arr(mCfg.sys.ip.dns1,    obj[F("dns1")].as<const char*>());
                if(obj.containsKey(F("dns2"))) ah::ip2Arr(mCfg.sys.ip.dns2,    obj[F("dns2")].as<const char*>());
                if(obj.containsKey(F("gtwy"))) ah::ip2Arr(mCfg.sys.ip.gateway, obj[F("gtwy")].as<const char*>());

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
                obj[F("sclk")]      = mCfg.nrf.pinSclk;
                obj[F("mosi")]      = mCfg.nrf.pinMosi;
                obj[F("miso")]      = mCfg.nrf.pinMiso;
                obj[F("pwr")]       = mCfg.nrf.amplifierPower;
                obj[F("en")]        = (bool) mCfg.nrf.enabled;
            } else {
                getVal<uint16_t>(obj, F("intvl"), &mCfg.nrf.sendInterval);
                getVal<uint8_t>(obj, F("maxRetry"), &mCfg.nrf.maxRetransPerPyld);
                getVal<uint8_t>(obj, F("cs"), &mCfg.nrf.pinCs);
                getVal<uint8_t>(obj, F("ce"), &mCfg.nrf.pinCe);
                getVal<uint8_t>(obj, F("irq"), &mCfg.nrf.pinIrq);
                getVal<uint8_t>(obj, F("sclk"), &mCfg.nrf.pinSclk);
                getVal<uint8_t>(obj, F("mosi"), &mCfg.nrf.pinMosi);
                getVal<uint8_t>(obj, F("miso"), &mCfg.nrf.pinMiso);
                getVal<uint8_t>(obj, F("pwr"), &mCfg.nrf.amplifierPower);
                #if !defined(ESP32)
                mCfg.nrf.enabled = true; // ESP8266, read always as enabled
                #else
                mCfg.nrf.enabled = (bool) obj[F("en")];
                #endif
                if((obj[F("cs")] == obj[F("ce")])) {
                    mCfg.nrf.pinCs   = DEF_CS_PIN;
                    mCfg.nrf.pinCe   = DEF_CE_PIN;
                    mCfg.nrf.pinIrq  = DEF_IRQ_PIN;
                    mCfg.nrf.pinSclk = DEF_SCLK_PIN;
                    mCfg.nrf.pinMosi = DEF_MOSI_PIN;
                    mCfg.nrf.pinMiso = DEF_MISO_PIN;
                }
            }
        }

        void jsonCmt(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("csb")]  = mCfg.cmt.pinCsb;
                obj[F("fcsb")] = mCfg.cmt.pinFcsb;
                obj[F("irq")]  = mCfg.cmt.pinIrq;
                obj[F("en")]   = (bool) mCfg.cmt.enabled;
            } else {
                mCfg.cmt.pinCsb  = obj[F("csb")];
                mCfg.cmt.pinFcsb = obj[F("fcsb")];
                mCfg.cmt.pinIrq  = obj[F("irq")];
                mCfg.cmt.enabled = (bool) obj[F("en")];
            }
        }

        void jsonNtp(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("addr")] = mCfg.ntp.addr;
                obj[F("port")] = mCfg.ntp.port;
                obj[F("intvl")] = mCfg.ntp.interval;
            } else {
                getChar(obj, F("addr"), mCfg.ntp.addr, NTP_ADDR_LEN);
                getVal<uint16_t>(obj, F("port"), &mCfg.ntp.port);
                getVal<uint16_t>(obj, F("intvl"), &mCfg.ntp.interval);

                if(mCfg.ntp.interval < 5) // minimum 5 minutes
                    mCfg.ntp.interval = 720; // default -> 12 hours
            }
        }

        void jsonSun(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("lat")]  = mCfg.sun.lat;
                obj[F("lon")]  = mCfg.sun.lon;
                obj[F("dis")]  = mCfg.sun.disNightCom;
                obj[F("offs")] = mCfg.sun.offsetSec;
            } else {
                getVal<float>(obj, F("lat"), &mCfg.sun.lat);
                getVal<float>(obj, F("lon"), &mCfg.sun.lon);
                getVal<bool>(obj, F("dis"), &mCfg.sun.disNightCom);
                getVal<uint16_t>(obj, F("offs"), &mCfg.sun.offsetSec);
            }
        }

        void jsonSerial(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("intvl")] = mCfg.serial.interval;
                obj[F("show")]  = mCfg.serial.showIv;
                obj[F("debug")] = mCfg.serial.debug;
            } else {
                getVal<uint16_t>(obj, F("intvl"), &mCfg.serial.interval);
                getVal<bool>(obj, F("show"), &mCfg.serial.showIv);
                getVal<bool>(obj, F("debug"), &mCfg.serial.debug);
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

            } else {
                getVal<uint16_t>(obj, F("port"), &mCfg.mqtt.port);
                getVal<uint16_t>(obj, F("intvl"), &mCfg.mqtt.interval);
                getChar(obj, F("broker"), mCfg.mqtt.broker, MQTT_ADDR_LEN);
                getChar(obj, F("user"), mCfg.mqtt.user, MQTT_USER_LEN);
                getChar(obj, F("pwd"), mCfg.mqtt.pwd, MQTT_PWD_LEN);
                getChar(obj, F("topic"), mCfg.mqtt.topic, MQTT_TOPIC_LEN);
            }
        }

        void jsonLed(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("0")] = mCfg.led.led0;
                obj[F("1")] = mCfg.led.led1;
                obj[F("act_high")] = mCfg.led.led_high_active;
            } else {
                getVal<uint8_t>(obj, F("0"), &mCfg.led.led0);
                getVal<uint8_t>(obj, F("1"), &mCfg.led.led1);
                getVal<bool>(obj, F("act_high"), &mCfg.led.led_high_active);
            }
        }

        void jsonPlugin(JsonObject obj, bool set = false) {
            if(set) {
                JsonObject disp = obj.createNestedObject("disp");
                disp[F("type")]     = mCfg.plugin.display.type;
                disp[F("pwrSafe")]  = (bool)mCfg.plugin.display.pwrSaveAtIvOffline;
                disp[F("pxShift")] = (bool)mCfg.plugin.display.pxShift;
                disp[F("rotation")] = mCfg.plugin.display.rot;
                //disp[F("wake")] = mCfg.plugin.display.wakeUp;
                //disp[F("sleep")] = mCfg.plugin.display.sleepAt;
                disp[F("contrast")] = mCfg.plugin.display.contrast;
                disp[F("data")] = mCfg.plugin.display.disp_data;
                disp[F("clock")] = mCfg.plugin.display.disp_clk;
                disp[F("cs")] = mCfg.plugin.display.disp_cs;
                disp[F("reset")] = mCfg.plugin.display.disp_reset;
                disp[F("busy")] = mCfg.plugin.display.disp_busy;
                disp[F("dc")] = mCfg.plugin.display.disp_dc;
            } else {
                JsonObject disp = obj["disp"];
                getVal<uint8_t>(disp, F("type"), &mCfg.plugin.display.type);
                getVal<bool>(disp, F("pwrSafe"), &mCfg.plugin.display.pwrSaveAtIvOffline);
                getVal<bool>(disp, F("pxShift"), &mCfg.plugin.display.pxShift);
                getVal<uint8_t>(disp, F("rotation"), &mCfg.plugin.display.rot);
                //mCfg.plugin.display.wakeUp = disp[F("wake")];
                //mCfg.plugin.display.sleepAt = disp[F("sleep")];
                getVal<uint8_t>(disp, F("contrast"), &mCfg.plugin.display.contrast);
                getVal<uint8_t>(disp, F("data"), &mCfg.plugin.display.disp_data);
                getVal<uint8_t>(disp, F("clock"), &mCfg.plugin.display.disp_clk);
                getVal<uint8_t>(disp, F("cs"), &mCfg.plugin.display.disp_cs);
                getVal<uint8_t>(disp, F("reset"), &mCfg.plugin.display.disp_reset);
                getVal<uint8_t>(disp, F("busy"), &mCfg.plugin.display.disp_busy);
                getVal<uint8_t>(disp, F("dc"), &mCfg.plugin.display.disp_dc);
            }
        }

        void jsonInst(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("en")] = (bool)mCfg.inst.enabled;
                obj[F("rstMidNight")] = (bool)mCfg.inst.rstYieldMidNight;
                obj[F("rstNotAvail")] = (bool)mCfg.inst.rstValsNotAvail;
                obj[F("rstComStop")]  = (bool)mCfg.inst.rstValsCommStop;
                obj[F("strtWthtTime")] = (bool)mCfg.inst.startWithoutTime;
                obj[F("yldEff")]       = mCfg.inst.yieldEffiency;
            }
            else {
                getVal<bool>(obj, F("en"), &mCfg.inst.enabled);
                getVal<bool>(obj, F("rstMidNight"), &mCfg.inst.rstYieldMidNight);
                getVal<bool>(obj, F("rstNotAvail"), &mCfg.inst.rstValsNotAvail);
                getVal<bool>(obj, F("rstComStop"), &mCfg.inst.rstValsCommStop);
                getVal<bool>(obj, F("strtWthtTime"), &mCfg.inst.startWithoutTime);
                getVal<float>(obj, F("yldEff"), &mCfg.inst.yieldEffiency);

                if(mCfg.inst.yieldEffiency < 0.5)
                    mCfg.inst.yieldEffiency = 1.0f;
                else if(mCfg.inst.yieldEffiency > 1.0f)
                    mCfg.inst.yieldEffiency = 1.0f;
            }

            JsonArray ivArr;
            if(set)
                ivArr = obj.createNestedArray(F("iv"));
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                if(set) {
                    if(mCfg.inst.iv[i].serial.u64 != 0ULL)
                        jsonIv(ivArr.createNestedObject(), &mCfg.inst.iv[i], true);
                } else if(!obj[F("iv")][i].isNull())
                    jsonIv(obj[F("iv")][i], &mCfg.inst.iv[i]);
            }
        }

        void jsonIv(JsonObject obj, cfgIv_t *cfg, bool set = false) {
            if(set) {
                obj[F("en")]   = (bool)cfg->enabled;
                obj[F("name")] = cfg->name;
                obj[F("sn")]   = cfg->serial.u64;
                for(uint8_t i = 0; i < 6; i++) {
                    obj[F("yield")][i]  = cfg->yieldCor[i];
                    obj[F("pwr")][i]    = cfg->chMaxPwr[i];
                    obj[F("chName")][i] = cfg->chName[i];
                }
            } else {
                getVal<bool>(obj, F("en"), &cfg->enabled);
                getChar(obj, F("name"), cfg->name, MAX_NAME_LENGTH);
                getVal<uint64_t>(obj, F("sn"), &cfg->serial.u64);
                uint8_t size = 4;
                if(obj.containsKey(F("pwr")))
                    size = obj[F("pwr")].size();
                for(uint8_t i = 0; i < size; i++) {
                    if(obj.containsKey(F("yield"))) cfg->yieldCor[i] = obj[F("yield")][i];
                    if(obj.containsKey(F("pwr"))) cfg->chMaxPwr[i] = obj[F("pwr")][i];
                    if(obj.containsKey(F("chName"))) snprintf(cfg->chName[i], MAX_NAME_LENGTH, "%s", obj[F("chName")][i].as<const char*>());
                }
            }
        }

    #if defined(ESP32)
        void getChar(JsonObject obj, const char *key, char *dst, int maxLen) {
            if(obj.containsKey(key))
                snprintf(dst, maxLen, "%s", obj[key].as<const char*>());
        }

        template<typename T=uint8_t>
        void getVal(JsonObject obj, const char *key, T *dst) {
            if(obj.containsKey(key))
                *dst = obj[key];
        }
    #else
        void getChar(JsonObject obj, const __FlashStringHelper *key, char *dst, int maxLen) {
            if(obj.containsKey(key))
                snprintf(dst, maxLen, "%s", obj[key].as<const char*>());
        }

        template<typename T=uint8_t>
        void getVal(JsonObject obj, const __FlashStringHelper *key, T *dst) {
            if(obj.containsKey(key))
                *dst = obj[key];
        }
    #endif

        settings_t mCfg;
        bool mLastSaveSucceed;
};

#endif /*__SETTINGS_H__*/
