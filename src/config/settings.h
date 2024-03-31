//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
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
#include <algorithm>
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

#define CONFIG_VERSION      11


#define PROT_MASK_INDEX     0x0001
#define PROT_MASK_LIVE      0x0002
#define PROT_MASK_SERIAL    0x0004
#define PROT_MASK_SETUP     0x0008
#define PROT_MASK_UPDATE    0x0010
#define PROT_MASK_SYSTEM    0x0020
#define PROT_MASK_HISTORY   0x0040
#define PROT_MASK_API       0x0080
#define PROT_MASK_MQTT      0x0100

#define DEF_PROT_INDEX      0x0001
#define DEF_PROT_LIVE       0x0000
#define DEF_PROT_SERIAL     0x0004
#define DEF_PROT_SETUP      0x0008
#define DEF_PROT_UPDATE     0x0010
#define DEF_PROT_SYSTEM     0x0020
#define DEF_PROT_HISTORY    0x0000
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
    uint8_t region;
    int8_t timezone;

#if !defined(ETHERNET)
    // wifi
    char stationSsid[SSID_LEN];
    char stationPwd[PWD_LEN];
    char apPwd[PWD_LEN];
    bool isHidden;
#endif /* !defined(ETHERNET) */

    cfgIp_t ip;
} cfgSys_t;

typedef struct {
    bool enabled;
    uint8_t pinCs;
    uint8_t pinCe;
    uint8_t pinIrq;
    uint8_t pinMiso;
    uint8_t pinMosi;
    uint8_t pinSclk;
} cfgNrf24_t;

typedef struct {
    bool enabled;
    uint8_t pinSclk;
    uint8_t pinSdio;
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
    int16_t offsetSecMorning;
    int16_t offsetSecEvening;
} cfgSun_t;

typedef struct {
    bool showIv;
    bool debug;
    bool privacyLog;
    bool printWholeTrace;
    bool log2mqtt;
} cfgSerial_t;

typedef struct {
    uint8_t led[3];    // LED pins
    bool high_active;  // determines if LEDs are high or low active
    uint8_t luminance; // luminance of LED
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
    uint8_t frequency;
    uint8_t powerLevel;
    bool disNightCom;  // disable night communication
} cfgIv_t;

typedef struct {
//    bool enabled;
    cfgIv_t iv[MAX_NUM_INVERTERS];

    uint16_t sendInterval;
    bool rstYieldMidNight;
    bool rstValsNotAvail;
    bool rstValsCommStop;
    bool rstMaxValsMidNight;
    bool startWithoutTime;
    bool readGrid;
} cfgInst_t;

#if defined(PLUGIN_DISPLAY)
typedef struct {
    uint8_t type;
    bool pwrSaveAtIvOffline;
    uint8_t screenSaver;
    uint8_t graph_ratio;
    uint8_t graph_size;
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
    uint8_t pirPin;
} display_t;
#endif

// Plugin ZeroExport
#if defined(PLUGIN_ZEROEXPORT)

#define ZEROEXPORT_DEV_POWERMETER
#define ZEROEXPORT_MAX_GROUPS                       6
#define ZEROEXPORT_GROUP_MAX_LEN_NAME              25
#define ZEROEXPORT_GROUP_MAX_LEN_PM_URL           100
#define ZEROEXPORT_GROUP_MAX_LEN_PM_JSONPATH      100
#define ZEROEXPORT_GROUP_MAX_LEN_PM_USER           25
#define ZEROEXPORT_GROUP_MAX_LEN_PM_PASS           25
#define ZEROEXPORT_GROUP_MAX_INVERTERS              3
#define ZEROEXPORT_POWERMETER_MAX_ERRORS            5
#define ZEROEXPORT_DEF_INV_WAITINGTIME_MS       10000
#define ZEROEXPORT_GROUP_WR_LIMIT_MIN_DIFF          5


enum class zeroExportState : uint8_t {
    INIT,
    WAITREFRESH,
    GETINVERTERACKS,
    GETINVERTERDATA,
    BATTERYPROTECTION,
    GETPOWERMETER,
	CONTROLLER,
	PROGNOSE,
	AUFTEILEN,
	SETREBOOT,
	SETPOWER,
	SETLIMIT,
    PUBLISH,
    EMERGENCY,
    FINISH,
    ERROR
};

typedef enum {
    None        = 0,
    Shelly      = 1,
    Tasmota     = 2,
    Mqtt        = 3,
    Hichi       = 4,
    Tibber      = 5,
} zeroExportPowermeterType_t;
/*
typedef struct {
    uint8_t type;
    uint8_t ip;
    uint8_t url;
    bool login;
    uint8_t username;
    uint8_t password;
    uint8_t group;
    uint8_t phase;
    uint16_t nextRun;
    uint16_t interval;
    uint8_t error;
    uint16_t power;
} zeroExportPowermeter_t;
*/
typedef enum {
    Sum     = 0,
    L1      = 1,
    L2      = 2,
    L3      = 3,
    L1Sum   = 4,
    L2Sum   = 5,
    L3Sum   = 6,
} zeroExportInverterTarget_t;

typedef struct {
    bool enabled;
    int8_t id;
    int8_t target;
    uint16_t powerMin;
    uint16_t powerMax;
    //

    float power;
    float limit;
    float limitNew;
    uint8_t waitLimitAck;
    uint8_t waitPowerAck;
    uint8_t waitRebootAck;
    unsigned long limitTsp;
    float dcVoltage;
    bool state;
    //
    bool doReboot;
    int8_t doPower;
} zeroExportGroupInverter_t;

typedef struct {
    // General
    bool enabled;
    char name[ZEROEXPORT_GROUP_MAX_LEN_NAME];
    // Powermeter
    uint8_t pm_type;
    char pm_url[ZEROEXPORT_GROUP_MAX_LEN_PM_URL];
    char pm_jsonPath[ZEROEXPORT_GROUP_MAX_LEN_PM_JSONPATH];
    char pm_user[ZEROEXPORT_GROUP_MAX_LEN_PM_USER];
    char pm_pass[ZEROEXPORT_GROUP_MAX_LEN_PM_PASS];
    // Inverters
    zeroExportGroupInverter_t inverters[ZEROEXPORT_GROUP_MAX_INVERTERS];
    // Battery
    bool battEnabled;
    float battVoltageOn;
    float battVoltageOff;
    // Advanced
    int16_t setPoint;
    uint8_t refresh;
    uint8_t powerTolerance;
    uint16_t powerMax;
    //

    zeroExportState state;
//    zeroExportState stateNext;
    unsigned long lastRun;
    unsigned long lastRefresh;
    uint16_t sleep;

    float eSum;
    float eSum1;
    float eSum2;
    float eSum3;
    float eOld;
    float eOld1;
    float eOld2;
    float eOld3;
    float Kp;
    float Ki;
    float Kd;

//float pm_P[5];
//float pm_P1[5];
//float pm_P2[5];
//float pm_P3[5];
//uint8_t pm_iIn = 0;
//uint8_t pm_iOut = 0;

    float pmPower;
    float pmPowerL1;
    float pmPowerL2;
    float pmPowerL3;
bool publishPower = false;

    bool battSwitch;
    float grpPower;
    float grpPowerL1;
    float grpPowerL2;
    float grpPowerL3;
//    float grpLimit;
//    float grpLimitL1;
//    float grpLimitL2;
//    float grpLimitL3;

//    uint16_t power;             // Aktueller Verbrauch
//    uint16_t powerLimitAkt;     // Aktuelles Limit
//    uint16_t powerHyst;         // Hysterese
} zeroExportGroup_t;

typedef struct {
    bool enabled;
    bool log_over_webserial;
    bool log_over_mqtt;
    bool debug;
    zeroExportGroup_t groups[ZEROEXPORT_MAX_GROUPS];



//    uint8_t query_device;   // 0 - Tibber, 1 - Shelly, 2 - other (rs232?)
//    char monitor_url[ZEXPORT_ADDR_LEN];
//    char json_path[ZEXPORT_ADDR_LEN];
//    char tibber_pw[10];   // needed for tibber QWGH-ED12
//    uint8_t Iv;         // saves the inverter that is used for regulation
//    float power_avg;
//    uint8_t count_avg;
//    double total_power;
//    unsigned long lastTime; // tic toc
//    double max_power;
//    bool two_percent;   // ask if not go lower then 2%
} zeroExport_t;

#endif
// Plugin ZeroExport - Ende

typedef struct {
    #if defined(PLUGIN_DISPLAY)
    display_t display;
    #endif
    char customLink[MAX_CUSTOM_LINK_LEN];
    char customLinkText[MAX_CUSTOM_LINK_TEXT_LEN];
    #if defined(PLUGIN_ZEROEXPORT)
    zeroExport_t zeroExport;
    #endif
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
    uint16_t    configVersion;
} settings_t;

class settings {
    public:
        settings() {
            std::fill(reinterpret_cast<char*>(&mCfg), reinterpret_cast<char*>(&mCfg) + sizeof(mCfg), 0);
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
                    getConfigVersion(root.as<JsonObject>());
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
            json[F("version")] = CONFIG_VERSION;
            jsonNetwork(root[F("wifi")].to<JsonObject>(), true);
            jsonNrf(root[F("nrf")].to<JsonObject>(), true);
            #if defined(ESP32)
            jsonCmt(root[F("cmt")].to<JsonObject>(), true);
            #endif
            jsonNtp(root[F("ntp")].to<JsonObject>(), true);
            jsonSun(root[F("sun")].to<JsonObject>(), true);
            jsonSerial(root[F("serial")].to<JsonObject>(), true);
            jsonMqtt(root[F("mqtt")].to<JsonObject>(), true);
            jsonLed(root[F("led")].to<JsonObject>(), true);
            jsonPlugin(root[F("plugin")].to<JsonObject>(), true);
            jsonInst(root[F("inst")].to<JsonObject>(), true);


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
            std::fill(reinterpret_cast<char*>(&mCfg), reinterpret_cast<char*>(&mCfg) + sizeof(mCfg), 0);
            mCfg.sys.protectionMask = DEF_PROT_INDEX | DEF_PROT_LIVE | DEF_PROT_SERIAL | DEF_PROT_SETUP
                                    | DEF_PROT_UPDATE | DEF_PROT_SYSTEM | DEF_PROT_API | DEF_PROT_MQTT | DEF_PROT_HISTORY;
            mCfg.sys.darkMode = false;
            mCfg.sys.schedReboot = false;
            // restore temp settings
            if(keepWifi)
                memcpy(&mCfg.sys, &tmp, sizeof(cfgSys_t));
            #if !defined(ETHERNET)
            else {
                snprintf(mCfg.sys.stationSsid, SSID_LEN, FB_WIFI_SSID);
                snprintf(mCfg.sys.stationPwd,  PWD_LEN,  FB_WIFI_PWD);
                snprintf(mCfg.sys.apPwd,       PWD_LEN,  WIFI_AP_PWD);
                mCfg.sys.isHidden = false;
            }
            #endif /* !defined(ETHERNET) */

            snprintf(mCfg.sys.deviceName,  DEVNAME_LEN, DEF_DEVICE_NAME);
            mCfg.sys.region   = 0; // Europe
            mCfg.sys.timezone = 1;

            mCfg.nrf.pinCs             = DEF_NRF_CS_PIN;
            mCfg.nrf.pinCe             = DEF_NRF_CE_PIN;
            mCfg.nrf.pinIrq            = DEF_NRF_IRQ_PIN;
            mCfg.nrf.pinMiso           = DEF_NRF_MISO_PIN;
            mCfg.nrf.pinMosi           = DEF_NRF_MOSI_PIN;
            mCfg.nrf.pinSclk           = DEF_NRF_SCLK_PIN;

            mCfg.nrf.enabled           = true;

            #if defined(ESP32)
            mCfg.cmt.pinSclk           = DEF_CMT_SCLK;
            mCfg.cmt.pinSdio           = DEF_CMT_SDIO;
            mCfg.cmt.pinCsb            = DEF_CMT_CSB;
            mCfg.cmt.pinFcsb           = DEF_CMT_FCSB;
            mCfg.cmt.pinIrq            = DEF_CMT_IRQ;
            #else
            mCfg.cmt.pinSclk           = DEF_PIN_OFF;
            mCfg.cmt.pinSdio           = DEF_PIN_OFF;
            mCfg.cmt.pinCsb            = DEF_PIN_OFF;
            mCfg.cmt.pinFcsb           = DEF_PIN_OFF;
            mCfg.cmt.pinIrq            = DEF_PIN_OFF;
            #endif
            mCfg.cmt.enabled           = false;

            snprintf(mCfg.ntp.addr, NTP_ADDR_LEN, "%s", DEF_NTP_SERVER_NAME);
            mCfg.ntp.port = DEF_NTP_PORT;
            mCfg.ntp.interval = 720;

            mCfg.sun.lat         = 51.1; // mid of Germany
            mCfg.sun.lon         = 10.5; // mid of Germany
            mCfg.sun.offsetSecMorning = 0;
            mCfg.sun.offsetSecEvening = 0;

            mCfg.serial.showIv   = false;
            mCfg.serial.debug    = false;
            mCfg.serial.privacyLog = true;
            mCfg.serial.printWholeTrace = false;
            mCfg.serial.log2mqtt = false;

            mCfg.mqtt.port = DEF_MQTT_PORT;
            snprintf(mCfg.mqtt.broker, MQTT_ADDR_LEN,  "%s", DEF_MQTT_BROKER);
            snprintf(mCfg.mqtt.user,   MQTT_USER_LEN,  "%s", DEF_MQTT_USER);
            snprintf(mCfg.mqtt.pwd,    MQTT_PWD_LEN,   "%s", DEF_MQTT_PWD);
            snprintf(mCfg.mqtt.topic,  MQTT_TOPIC_LEN, "%s", DEF_MQTT_TOPIC);
            mCfg.mqtt.interval = 0; // off

            mCfg.inst.sendInterval     = SEND_INTERVAL;
            mCfg.inst.rstYieldMidNight = false;
            mCfg.inst.rstValsNotAvail  = false;
            mCfg.inst.rstValsCommStop  = false;
            mCfg.inst.startWithoutTime = false;
            mCfg.inst.rstMaxValsMidNight = false;
            mCfg.inst.readGrid         = true;

            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                mCfg.inst.iv[i].powerLevel  = 0xff; // impossible high value
                mCfg.inst.iv[i].frequency   = 0x12; // 863MHz (minimum allowed frequency)
                mCfg.inst.iv[i].disNightCom = false;
            }

            mCfg.led.led[0]      = DEF_LED0;
            mCfg.led.led[1]      = DEF_LED1;
            mCfg.led.led[2]      = DEF_LED2;
            mCfg.led.high_active = LED_HIGH_ACTIVE;
            mCfg.led.luminance   = 255;

            #if defined(PLUGIN_DISPLAY)
            mCfg.plugin.display.pwrSaveAtIvOffline = false;
            mCfg.plugin.display.contrast = 140;
            mCfg.plugin.display.screenSaver = 1;  // default: 1 .. pixelshift for OLED for downward compatibility
            mCfg.plugin.display.graph_ratio = 0;
            mCfg.plugin.display.graph_size  = 2;
            mCfg.plugin.display.rot = 0;
            mCfg.plugin.display.disp_data  = DEF_PIN_OFF; // SDA
            mCfg.plugin.display.disp_clk   = DEF_PIN_OFF; // SCL
            mCfg.plugin.display.disp_cs    = DEF_PIN_OFF;
            mCfg.plugin.display.disp_reset = DEF_PIN_OFF;
            mCfg.plugin.display.disp_busy  = DEF_PIN_OFF;
            mCfg.plugin.display.disp_dc    = DEF_PIN_OFF;
            mCfg.plugin.display.pirPin     = DEF_PIN_OFF;
            #endif

            // Plugin ZeroExport
            #if defined(PLUGIN_ZEROEXPORT)
            mCfg.plugin.zeroExport.enabled = false;
            mCfg.plugin.zeroExport.log_over_webserial = false;
            mCfg.plugin.zeroExport.log_over_mqtt = false;
            mCfg.plugin.zeroExport.debug = false;
            for(uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
                // General
                mCfg.plugin.zeroExport.groups[group].enabled = false;
                snprintf(mCfg.plugin.zeroExport.groups[group].name, ZEROEXPORT_GROUP_MAX_LEN_NAME,  "%s", DEF_ZEXPORT);
                // Powermeter
                mCfg.plugin.zeroExport.groups[group].pm_type = zeroExportPowermeterType_t::None;
                snprintf(mCfg.plugin.zeroExport.groups[group].pm_url, ZEROEXPORT_GROUP_MAX_LEN_PM_URL,  "%s", DEF_ZEXPORT);
                snprintf(mCfg.plugin.zeroExport.groups[group].pm_jsonPath, ZEROEXPORT_GROUP_MAX_LEN_PM_JSONPATH,  "%s", DEF_ZEXPORT);
                snprintf(mCfg.plugin.zeroExport.groups[group].pm_user, ZEROEXPORT_GROUP_MAX_LEN_PM_USER,  "%s", DEF_ZEXPORT);
                snprintf(mCfg.plugin.zeroExport.groups[group].pm_pass, ZEROEXPORT_GROUP_MAX_LEN_PM_PASS,  "%s", DEF_ZEXPORT);
                // Inverters
                for(uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                    mCfg.plugin.zeroExport.groups[group].inverters[inv].enabled = false;
                    mCfg.plugin.zeroExport.groups[group].inverters[inv].id = -1;
                    mCfg.plugin.zeroExport.groups[group].inverters[inv].target = -1;
                    mCfg.plugin.zeroExport.groups[group].inverters[inv].powerMin = 10;
                    mCfg.plugin.zeroExport.groups[group].inverters[inv].powerMax = 600;
                    //
                    mCfg.plugin.zeroExport.groups[group].inverters[inv].waitLimitAck = false;
                    mCfg.plugin.zeroExport.groups[group].inverters[inv].waitPowerAck = false;
                    mCfg.plugin.zeroExport.groups[group].inverters[inv].waitRebootAck = false;
                    mCfg.plugin.zeroExport.groups[group].inverters[inv].doReboot = false;
                    mCfg.plugin.zeroExport.groups[group].inverters[inv].doPower = -1;
                }
                // Battery
                mCfg.plugin.zeroExport.groups[group].battEnabled = false;
                mCfg.plugin.zeroExport.groups[group].battVoltageOn = 0;
                mCfg.plugin.zeroExport.groups[group].battVoltageOff = 0;
                // Advanced
                mCfg.plugin.zeroExport.groups[group].setPoint = 0;
                mCfg.plugin.zeroExport.groups[group].refresh = 10;
                mCfg.plugin.zeroExport.groups[group].powerTolerance = 10;
                mCfg.plugin.zeroExport.groups[group].powerMax = 600;
                mCfg.plugin.zeroExport.groups[group].Kp = -1;
                mCfg.plugin.zeroExport.groups[group].Ki = 0;
                mCfg.plugin.zeroExport.groups[group].Kd = 0;
                //
                mCfg.plugin.zeroExport.groups[group].state = zeroExportState::INIT;
                mCfg.plugin.zeroExport.groups[group].lastRun = 0;
                mCfg.plugin.zeroExport.groups[group].lastRefresh = 0;
                mCfg.plugin.zeroExport.groups[group].sleep = 0;
                mCfg.plugin.zeroExport.groups[group].pmPower = 0;
                mCfg.plugin.zeroExport.groups[group].pmPowerL1 = 0;
                mCfg.plugin.zeroExport.groups[group].pmPowerL2 = 0;
                mCfg.plugin.zeroExport.groups[group].pmPowerL3 = 0;

                mCfg.plugin.zeroExport.groups[group].battSwitch = false;
            }
//            snprintf(mCfg.plugin.zeroExport.monitor_url, ZEXPORT_ADDR_LEN,  "%s", DEF_ZEXPORT);
//            snprintf(mCfg.plugin.zeroExport.tibber_pw, ZEXPORT_ADDR_LEN,  "%s", DEF_ZEXPORT);
//            snprintf(mCfg.plugin.zeroExport.json_path, ZEXPORT_ADDR_LEN,  "%s", DEF_ZEXPORT);
//            mCfg.plugin.zeroExport.enabled = false;
//            mCfg.plugin.zeroExport.count_avg = 10;
//            mCfg.plugin.zeroExport.lastTime =  millis();   // do not change!

//            mCfg.plugin.zeroExport.query_device = 1;       // Standard shelly
//            mCfg.plugin.zeroExport.power_avg = 10;
//            mCfg.plugin.zeroExport.Iv = 0;
//            mCfg.plugin.zeroExport.max_power = 600;        // Max 600W to stay safe
//            mCfg.plugin.zeroExport.two_percent = true;
//    uint8_t ip;
//    uint8_t url;
//    bool login;
//    uint8_t username;
//    uint8_t password;
//            snprintf(mCfg.plugin.zeroExport.monitor_url, ZEXPORT_ADDR_LEN,  "%s", DEF_ZEXPORT);
//            snprintf(mCfg.plugin.zeroExport.monitor_url, ZEXPORT_ADDR_LEN,  "%s", DEF_ZEXPORT);
//            snprintf(mCfg.plugin.zeroExport.monitor_url, ZEXPORT_ADDR_LEN,  "%s", DEF_ZEXPORT);
//    uint8_t group;
//    uint8_t phase;
//                mCfg.plugin.zeroExport.groups[group].powermeter.nextRun = 0;
//                mCfg.plugin.zeroExport.groups[group].powermeter.interval = 10000;
//                mCfg.plugin.zeroExport.groups[group].powermeter.error = 0;
//                mCfg.plugin.zeroExport.groups[group].powermeter.power = 0;
//                    mCfg.plugin.zeroExport.groups[group].inverters[inv].waitingTime = 0;
//                    mCfg.plugin.zeroExport.groups[group].inverters[inv].limit = -1;
//                    mCfg.plugin.zeroExport.groups[group].inverters[inv].limitAck = false;
            // Plugin ZeroExport - Ende
            #endif

        }

        void loadAddedDefaults() {
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                if(mCfg.configVersion < 1) {
                    mCfg.inst.iv[i].powerLevel = 0xff; // impossible high value
                    mCfg.inst.iv[i].frequency  = 0x0; // 860MHz (backward compatibility)
                }
                if(mCfg.configVersion < 2) {
                    mCfg.inst.iv[i].disNightCom = false;
                }
                if(mCfg.configVersion < 3) {
                    mCfg.serial.printWholeTrace = false;
                }
                if(mCfg.configVersion < 5) {
                    mCfg.inst.sendInterval = SEND_INTERVAL;
                    mCfg.serial.printWholeTrace = false;
                }
                if(mCfg.configVersion < 6) {
                    mCfg.inst.readGrid = true;
                }
                if(mCfg.configVersion < 7) {
                    mCfg.led.luminance = 255;
                }
                if(mCfg.configVersion < 8) {
                    mCfg.sun.offsetSecEvening = mCfg.sun.offsetSecMorning;
                }
                if(mCfg.configVersion < 10) {
                    mCfg.sys.region   = 0; // Europe
                    mCfg.sys.timezone = 1;
                }
                if(mCfg.configVersion < 11) {
                    mCfg.serial.log2mqtt = false;
                }
            }
        }

        void getConfigVersion(JsonObject obj) {
            getVal<uint16_t>(obj, F("version"), &mCfg.configVersion);
            DPRINT(DBG_INFO, F("Config Version: "));
            DBGPRINTLN(String(mCfg.configVersion));
            if(CONFIG_VERSION != mCfg.configVersion)
                loadAddedDefaults();
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
                obj[F("region")] = mCfg.sys.region;
                obj[F("timezone")] = mCfg.sys.timezone;
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
                getVal<uint8_t>(obj, F("region"), &mCfg.sys.region);
                getVal<int8_t>(obj, F("timezone"), &mCfg.sys.timezone);
                if(obj.containsKey(F("ip"))) ah::ip2Arr(mCfg.sys.ip.ip,      obj[F("ip")].as<const char*>());
                if(obj.containsKey(F("mask"))) ah::ip2Arr(mCfg.sys.ip.mask,    obj[F("mask")].as<const char*>());
                if(obj.containsKey(F("dns1"))) ah::ip2Arr(mCfg.sys.ip.dns1,    obj[F("dns1")].as<const char*>());
                if(obj.containsKey(F("dns2"))) ah::ip2Arr(mCfg.sys.ip.dns2,    obj[F("dns2")].as<const char*>());
                if(obj.containsKey(F("gtwy"))) ah::ip2Arr(mCfg.sys.ip.gateway, obj[F("gtwy")].as<const char*>());

                if(mCfg.sys.protectionMask == 0)
                    mCfg.sys.protectionMask = DEF_PROT_INDEX | DEF_PROT_LIVE | DEF_PROT_SERIAL | DEF_PROT_SETUP
                                            | DEF_PROT_UPDATE | DEF_PROT_SYSTEM | DEF_PROT_API | DEF_PROT_MQTT | DEF_PROT_HISTORY;
            }
        }

        void jsonNrf(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("cs")]        = mCfg.nrf.pinCs;
                obj[F("ce")]        = mCfg.nrf.pinCe;
                obj[F("irq")]       = mCfg.nrf.pinIrq;
                obj[F("sclk")]      = mCfg.nrf.pinSclk;
                obj[F("mosi")]      = mCfg.nrf.pinMosi;
                obj[F("miso")]      = mCfg.nrf.pinMiso;
                obj[F("en")]        = (bool) mCfg.nrf.enabled;
            } else {
                getVal<uint8_t>(obj, F("cs"), &mCfg.nrf.pinCs);
                getVal<uint8_t>(obj, F("ce"), &mCfg.nrf.pinCe);
                getVal<uint8_t>(obj, F("irq"), &mCfg.nrf.pinIrq);
                getVal<uint8_t>(obj, F("sclk"), &mCfg.nrf.pinSclk);
                getVal<uint8_t>(obj, F("mosi"), &mCfg.nrf.pinMosi);
                getVal<uint8_t>(obj, F("miso"), &mCfg.nrf.pinMiso);
                #if !defined(ESP32)
                mCfg.nrf.enabled = true; // ESP8266, read always as enabled
                #else
                mCfg.nrf.enabled = (bool) obj[F("en")];
                #endif
                if((obj[F("cs")] == obj[F("ce")])) {
                    mCfg.nrf.pinCs   = DEF_NRF_CS_PIN;
                    mCfg.nrf.pinCe   = DEF_NRF_CE_PIN;
                    mCfg.nrf.pinIrq  = DEF_NRF_IRQ_PIN;
                    mCfg.nrf.pinSclk = DEF_NRF_SCLK_PIN;
                    mCfg.nrf.pinMosi = DEF_NRF_MOSI_PIN;
                    mCfg.nrf.pinMiso = DEF_NRF_MISO_PIN;
                }
            }
        }

        #if defined(ESP32)
        void jsonCmt(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("csb")]  = mCfg.cmt.pinCsb;
                obj[F("fcsb")] = mCfg.cmt.pinFcsb;
                obj[F("irq")]  = mCfg.cmt.pinIrq;
                obj[F("dio")]  = mCfg.cmt.pinSdio;
                obj[F("clk")]  = mCfg.cmt.pinSclk;
                obj[F("en")]   = (bool) mCfg.cmt.enabled;
            } else {
                mCfg.cmt.pinCsb  = obj[F("csb")];
                mCfg.cmt.pinFcsb = obj[F("fcsb")];
                mCfg.cmt.pinIrq  = obj[F("irq")];
                mCfg.cmt.pinSdio = obj[F("dio")];
                mCfg.cmt.pinSclk = obj[F("clk")];
                mCfg.cmt.enabled = (bool) obj[F("en")];
                if(0 == mCfg.cmt.pinSclk)
                    mCfg.cmt.pinSclk = DEF_CMT_SCLK;
                if(0 == mCfg.cmt.pinSdio)
                    mCfg.cmt.pinSdio = DEF_CMT_SDIO;
            }
        }
        #endif

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
                obj[F("offs")] = mCfg.sun.offsetSecMorning;
                obj[F("offsEve")] = mCfg.sun.offsetSecEvening;
            } else {
                getVal<float>(obj, F("lat"), &mCfg.sun.lat);
                getVal<float>(obj, F("lon"), &mCfg.sun.lon);
                getVal<int16_t>(obj, F("offs"), &mCfg.sun.offsetSecMorning);
                getVal<int16_t>(obj, F("offsEve"), &mCfg.sun.offsetSecEvening);
            }
        }

        void jsonSerial(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("show")]  = mCfg.serial.showIv;
                obj[F("debug")] = mCfg.serial.debug;
                obj[F("prv")] = (bool) mCfg.serial.privacyLog;
                obj[F("trc")] = (bool) mCfg.serial.printWholeTrace;
                obj[F("mqtt")] = (bool) mCfg.serial.log2mqtt;
            } else {
                getVal<bool>(obj, F("show"), &mCfg.serial.showIv);
                getVal<bool>(obj, F("debug"), &mCfg.serial.debug);
                getVal<bool>(obj, F("prv"), &mCfg.serial.privacyLog);
                getVal<bool>(obj, F("trc"), &mCfg.serial.printWholeTrace);
                getVal<bool>(obj, F("mqtt"), &mCfg.serial.log2mqtt);
            }
        }

        void jsonMqtt(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("broker")]   = mCfg.mqtt.broker;
                obj[F("port")]     = mCfg.mqtt.port;
                obj[F("clientId")] = mCfg.mqtt.clientId;
                obj[F("user")]     = mCfg.mqtt.user;
                obj[F("pwd")]      = mCfg.mqtt.pwd;
                obj[F("topic")]    = mCfg.mqtt.topic;
                obj[F("intvl")]    = mCfg.mqtt.interval;

            } else {
                getVal<uint16_t>(obj, F("port"), &mCfg.mqtt.port);
                getVal<uint16_t>(obj, F("intvl"), &mCfg.mqtt.interval);
                getChar(obj, F("broker"), mCfg.mqtt.broker, MQTT_ADDR_LEN);
                getChar(obj, F("user"), mCfg.mqtt.user, MQTT_USER_LEN);
                getChar(obj, F("clientId"), mCfg.mqtt.clientId, MQTT_CLIENTID_LEN);
                getChar(obj, F("pwd"), mCfg.mqtt.pwd, MQTT_PWD_LEN);
                getChar(obj, F("topic"), mCfg.mqtt.topic, MQTT_TOPIC_LEN);
            }
        }

        void jsonLed(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("0")]        = mCfg.led.led[0];
                obj[F("1")]        = mCfg.led.led[1];
                obj[F("2")]        = mCfg.led.led[2];
                obj[F("act_high")] = mCfg.led.high_active;
                obj[F("lum")]      = mCfg.led.luminance;
            } else {
                getVal<uint8_t>(obj, F("0"), &mCfg.led.led[0]);
                getVal<uint8_t>(obj, F("1"), &mCfg.led.led[1]);
                getVal<uint8_t>(obj, F("2"), &mCfg.led.led[2]);
                getVal<bool>(obj, F("act_high"), &mCfg.led.high_active);
                getVal<uint8_t>(obj, F("lum"), &mCfg.led.luminance);
            }
        }

        // Plugin ZeroExport
        #if defined(PLUGIN_ZEROEXPORT)

        void jsonZeroExportGroupInverter(JsonObject obj, uint8_t group, uint8_t inv, bool set = false) {
            if(set) {
                obj[F("enabled")] = mCfg.plugin.zeroExport.groups[group].inverters[inv].enabled;
                obj[F("id")] = mCfg.plugin.zeroExport.groups[group].inverters[inv].id;
                obj[F("target")] = mCfg.plugin.zeroExport.groups[group].inverters[inv].target;
                obj[F("powerMin")] = mCfg.plugin.zeroExport.groups[group].inverters[inv].powerMin;
                obj[F("powerMax")] = mCfg.plugin.zeroExport.groups[group].inverters[inv].powerMax;
            } else {
                if (obj.containsKey(F("enabled")))
                    getVal<bool>(obj, F("enabled"), &mCfg.plugin.zeroExport.groups[group].inverters[inv].enabled);
                if (obj.containsKey(F("id")))
                    getVal<int8_t>(obj, F("id"), &mCfg.plugin.zeroExport.groups[group].inverters[inv].id);
                if (obj.containsKey(F("target")))
                    getVal<int8_t>(obj, F("target"), &mCfg.plugin.zeroExport.groups[group].inverters[inv].target);
                if (obj.containsKey(F("powerMin")))
                    getVal<uint16_t>(obj, F("powerMin"), &mCfg.plugin.zeroExport.groups[group].inverters[inv].powerMin);
                if (obj.containsKey(F("powerMax")))
                    getVal<uint16_t>(obj, F("powerMax"), &mCfg.plugin.zeroExport.groups[group].inverters[inv].powerMax);
            }
        }

        void jsonZeroExportGroup(JsonObject obj, uint8_t group, bool set = false) {
            if(set) {
                // General
                obj[F("enabled")] = mCfg.plugin.zeroExport.groups[group].enabled;
                obj[F("name")] = mCfg.plugin.zeroExport.groups[group].name;
                // Powermeter
                obj[F("pm_type")] = mCfg.plugin.zeroExport.groups[group].pm_type;
                obj[F("pm_url")] = mCfg.plugin.zeroExport.groups[group].pm_url;
                obj[F("pm_jsonPath")] = mCfg.plugin.zeroExport.groups[group].pm_jsonPath;
                obj[F("pm_user")] = mCfg.plugin.zeroExport.groups[group].pm_user;
                obj[F("pm_pass")] = mCfg.plugin.zeroExport.groups[group].pm_pass;
                // Inverters
                JsonArray invArr = obj.createNestedArray(F("inverters"));
                for(uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                    jsonZeroExportGroupInverter(invArr.createNestedObject(), group, inv, set);
                }
                // Battery
                obj[F("battEnabled")] = mCfg.plugin.zeroExport.groups[group].battEnabled;
                obj[F("battVoltageOn")] = mCfg.plugin.zeroExport.groups[group].battVoltageOn;
                obj[F("battVoltageOff")] = mCfg.plugin.zeroExport.groups[group].battVoltageOff;
                // Advanced
                obj[F("setPoint")] = mCfg.plugin.zeroExport.groups[group].setPoint;
                obj[F("refresh")] = mCfg.plugin.zeroExport.groups[group].refresh;
                obj[F("powerTolerance")] = mCfg.plugin.zeroExport.groups[group].powerTolerance;
                obj[F("powerMax")] = mCfg.plugin.zeroExport.groups[group].powerMax;
                obj[F("Kp")] = mCfg.plugin.zeroExport.groups[group].Kp;
                obj[F("Ki")] = mCfg.plugin.zeroExport.groups[group].Ki;
                obj[F("Kd")] = mCfg.plugin.zeroExport.groups[group].Kd;
            } else {
                // General
                if (obj.containsKey(F("enabled")))
                    getVal<bool>(obj, F("enabled"), &mCfg.plugin.zeroExport.groups[group].enabled);
                if (obj.containsKey(F("name")))
                    getChar(obj, F("name"), mCfg.plugin.zeroExport.groups[group].name, ZEXPORT_ADDR_LEN);
                // Powermeter
                if (obj.containsKey(F("pm_type")))
                    getVal<uint8_t>(obj, F("pm_type"), &mCfg.plugin.zeroExport.groups[group].pm_type);
                if (obj.containsKey(F("pm_url")))
                    getChar(obj, F("pm_url"), mCfg.plugin.zeroExport.groups[group].pm_url, ZEROEXPORT_GROUP_MAX_LEN_PM_URL);
                if (obj.containsKey(F("pm_jsonPath")))
                    getChar(obj, F("pm_jsonPath"), mCfg.plugin.zeroExport.groups[group].pm_jsonPath, ZEROEXPORT_GROUP_MAX_LEN_PM_JSONPATH);
                if (obj.containsKey(F("pm_user")))
                    getChar(obj, F("pm_user"), mCfg.plugin.zeroExport.groups[group].pm_user, ZEROEXPORT_GROUP_MAX_LEN_PM_USER);
                if (obj.containsKey(F("pm_pass")))
                    getChar(obj, F("pm_pass"), mCfg.plugin.zeroExport.groups[group].pm_pass, ZEROEXPORT_GROUP_MAX_LEN_PM_PASS);
                // Inverters
                if (obj.containsKey(F("inverters"))) {
                    for(uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                        jsonZeroExportGroupInverter(obj[F("inverters")][inv], group, inv, set);
                    }
                }
                // Battery
                if (obj.containsKey(F("battEnabled")))
                    getVal<bool>(obj, F("battEnabled"), &mCfg.plugin.zeroExport.groups[group].battEnabled);
                if (obj.containsKey(F("battVoltageOn")))
                    getVal<float>(obj, F("battVoltageOn"), &mCfg.plugin.zeroExport.groups[group].battVoltageOn);
                if (obj.containsKey(F("battVoltageOff")))
                    getVal<float>(obj, F("battVoltageOff"), &mCfg.plugin.zeroExport.groups[group].battVoltageOff);
                // Advanced
                if (obj.containsKey(F("setPoint")))
                    getVal<int16_t>(obj, F("setPoint"), &mCfg.plugin.zeroExport.groups[group].setPoint);
                if (obj.containsKey(F("refresh")))
                    getVal<uint8_t>(obj, F("refresh"), &mCfg.plugin.zeroExport.groups[group].refresh);
                if (obj.containsKey(F("powerTolerance")))
                    getVal<uint8_t>(obj, F("powerTolerance"), &mCfg.plugin.zeroExport.groups[group].powerTolerance);
                if (obj.containsKey(F("powerMax")))
                    getVal<uint16_t>(obj, F("powerMax"), &mCfg.plugin.zeroExport.groups[group].powerMax);
                if (obj.containsKey(F("Kp")))
                    getVal<float>(obj, F("Kp"), &mCfg.plugin.zeroExport.groups[group].Kp);
                if (obj.containsKey(F("Ki")))
                    getVal<float>(obj, F("Ki"), &mCfg.plugin.zeroExport.groups[group].Ki);
                if (obj.containsKey(F("Kd")))
                    getVal<float>(obj, F("Kd"), &mCfg.plugin.zeroExport.groups[group].Kd);
            }
        }

        void jsonZeroExport(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("enabled")] = mCfg.plugin.zeroExport.enabled;
                obj[F("log_over_webserial")] = mCfg.plugin.zeroExport.log_over_webserial;
                obj[F("log_over_mqtt")] = mCfg.plugin.zeroExport.log_over_mqtt;
                obj[F("debug")] = mCfg.plugin.zeroExport.debug;
                JsonArray grpArr = obj.createNestedArray(F("groups"));
                for(uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
                    jsonZeroExportGroup(grpArr.createNestedObject(), group, set);
                }
            }
            else
            {
                if (obj.containsKey(F("enabled")))
                    getVal<bool>(obj, F("enabled"), &mCfg.plugin.zeroExport.enabled);
                if (obj.containsKey(F("log_over_webserial")))
                    getVal<bool>(obj, F("log_over_webserial"), &mCfg.plugin.zeroExport.log_over_webserial);
                if (obj.containsKey(F("log_over_mqtt")))
                    getVal<bool>(obj, F("log_over_mqtt"), &mCfg.plugin.zeroExport.log_over_mqtt);
                if (obj.containsKey(F("debug")))
                    getVal<bool>(obj, F("debug"), &mCfg.plugin.zeroExport.debug);
                if (obj.containsKey(F("groups"))) {
                    for(uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
                        jsonZeroExportGroup(obj[F("groups")][group], group, set);
                    }
                }
            }
        }
        #endif
        // Plugin ZeroExport - Ende

        void jsonPlugin(JsonObject obj, bool set = false) {
            if(set) {
                #if defined(PLUGIN_DISPLAY)
                JsonObject disp = obj.createNestedObject("disp");
                disp[F("type")]     = mCfg.plugin.display.type;
                disp[F("pwrSafe")]  = (bool)mCfg.plugin.display.pwrSaveAtIvOffline;
                disp[F("screenSaver")] = mCfg.plugin.display.screenSaver;
                disp[F("graph_ratio")] = mCfg.plugin.display.graph_ratio;
                disp[F("graph_size")] = mCfg.plugin.display.graph_size;
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
                disp[F("pirPin")] = mCfg.plugin.display.pirPin;
                #endif
                obj[F("cst_lnk")] = mCfg.plugin.customLink;
                obj[F("cst_lnk_txt")] = mCfg.plugin.customLinkText;
                // Plugin ZeroExport
                #if defined(PLUGIN_ZEROEXPORT)
                jsonZeroExport(obj.createNestedObject("zeroExport"), set);
                #endif
                // Plugin ZeroExport - Ende
            } else {
                #if defined(PLUGIN_DISPLAY)
                JsonObject disp = obj["disp"];
                getVal<uint8_t>(disp, F("type"), &mCfg.plugin.display.type);
                getVal<bool>(disp, F("pwrSafe"), &mCfg.plugin.display.pwrSaveAtIvOffline);
                getVal<uint8_t>(disp, F("screenSaver"), &mCfg.plugin.display.screenSaver);
                getVal<uint8_t>(disp, F("graph_ratio"), &mCfg.plugin.display.graph_ratio);
                getVal<uint8_t>(disp, F("graph_size"), &mCfg.plugin.display.graph_size);
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
                getVal<uint8_t>(disp, F("pirPin"), &mCfg.plugin.display.pirPin);
                #endif
                getChar(obj, F("cst_lnk"), mCfg.plugin.customLink, MAX_CUSTOM_LINK_LEN);
                getChar(obj, F("cst_lnk_txt"), mCfg.plugin.customLinkText, MAX_CUSTOM_LINK_TEXT_LEN);
                // Plugin ZeroExport
                #if defined(PLUGIN_ZEROEXPORT)
                jsonZeroExport(obj["zeroExport"], set);
                #endif
                // Plugin ZeroExport - Ende
            }
        }

        void jsonInst(JsonObject obj, bool set = false) {
            if(set) {
                obj[F("intvl")]          = mCfg.inst.sendInterval;
//                obj[F("en")] = (bool)mCfg.inst.enabled;
                obj[F("rstMidNight")]    = (bool)mCfg.inst.rstYieldMidNight;
                obj[F("rstNotAvail")]    = (bool)mCfg.inst.rstValsNotAvail;
                obj[F("rstComStop")]     = (bool)mCfg.inst.rstValsCommStop;
                obj[F("strtWthtTime")]   = (bool)mCfg.inst.startWithoutTime;
                obj[F("rstMaxMidNight")] = (bool)mCfg.inst.rstMaxValsMidNight;
                obj[F("rdGrid")]         = (bool)mCfg.inst.readGrid;
            }
            else {
                getVal<uint16_t>(obj, F("intvl"), &mCfg.inst.sendInterval);
//                getVal<bool>(obj, F("en"), &mCfg.inst.enabled);
                getVal<bool>(obj, F("rstMidNight"), &mCfg.inst.rstYieldMidNight);
                getVal<bool>(obj, F("rstNotAvail"), &mCfg.inst.rstValsNotAvail);
                getVal<bool>(obj, F("rstComStop"), &mCfg.inst.rstValsCommStop);
                getVal<bool>(obj, F("strtWthtTime"), &mCfg.inst.startWithoutTime);
                getVal<bool>(obj, F("rstMaxMidNight"), &mCfg.inst.rstMaxValsMidNight);
                getVal<bool>(obj, F("rdGrid"), &mCfg.inst.readGrid);
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
                obj[F("freq")] = cfg->frequency;
                obj[F("pa")]   = cfg->powerLevel;
                obj[F("dis")]  = cfg->disNightCom;
                for(uint8_t i = 0; i < 6; i++) {
                    obj[F("yield")][i]  = cfg->yieldCor[i];
                    obj[F("pwr")][i]    = cfg->chMaxPwr[i];
                    obj[F("chName")][i] = cfg->chName[i];
                }
            } else {
                getVal<bool>(obj, F("en"), &cfg->enabled);
                getChar(obj, F("name"), cfg->name, MAX_NAME_LENGTH);
                getVal<uint64_t>(obj, F("sn"), &cfg->serial.u64);
                getVal<uint8_t>(obj, F("freq"), &cfg->frequency);
                getVal<uint8_t>(obj, F("pa"), &cfg->powerLevel);
                getVal<bool>(obj, F("dis"), &cfg->disNightCom);
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
            if(obj.containsKey(key)) {
                snprintf(dst, maxLen, "%s", obj[key].as<const char*>());
                dst[maxLen-1] = '\0';
            }
        }

        template<typename T=uint8_t>
        void getVal(JsonObject obj, const char *key, T *dst) {
            if(obj.containsKey(key))
                *dst = obj[key];
        }
    #else
        void getChar(JsonObject obj, const __FlashStringHelper *key, char *dst, int maxLen) {
            if(obj.containsKey(key)) {
                snprintf(dst, maxLen, "%s", obj[key].as<const char*>());
                dst[maxLen-1] = '\0';
            }
        }

        template<typename T=uint8_t>
        void getVal(JsonObject obj, const __FlashStringHelper *key, T *dst) {
            if(obj.containsKey(key))
                *dst = obj[key];
        }
    #endif

        settings_t mCfg;
        bool mLastSaveSucceed = 0;
};

#endif /*__SETTINGS_H__*/
