//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#if defined(PLUGIN_ZEROEXPORT)

#ifndef __ZEROEXPORT__
#define __ZEROEXPORT__

#include <HTTPClient.h>
#include <base64.h>
#include <string.h>

#include "AsyncJson.h"
#include "powermeter.h"

template <class HMSYSTEM>

// TODO: groupAufteilen und groupSetLimit ... 4W Regel? für alle Parameter

class ZeroExport {
   public:
    /** ZeroExport
     * constructor
     */
    ZeroExport() {
        mIsInitialized = false;
        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                mIv[group][inv] = nullptr;
            }
        }
    }

    /** ~ZeroExport
     * destructor
     */
    ~ZeroExport() {}

    /** setup
     * Initialisierung
     */
    void setup(zeroExport_t *cfg, HMSYSTEM *sys, settings_t *config, RestApiType *api, PubMqttType *mqtt) {
        mCfg = cfg;
        mSys = sys;
        mConfig = config;
        mApi = api;
        mMqtt = mqtt;

        mIsSubscribed = false;
        mIsInitialized = mPowermeter.setup(mCfg, &mLog);

    }

    /** loop
     * Arbeitsschleife
     * @param
     * @returns
     * @todo publish
     * @todo emergency
     */
    void loop(void) {
        mqttInitTopic();

        if ((!mIsInitialized) || (!mCfg->enabled)) return;

        bool DoLog = false;
        unsigned long Tsp = millis();

        mPowermeter.loop(&Tsp, &DoLog);
        //        if (DoLog) sendLog();
        clearLog();
        DoLog = false;

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            zeroExportGroup_t *cfgGroup = &mCfg->groups[group];

            if (!cfgGroup->enabled) continue;

            // wait
            if (Tsp <= (cfgGroup->lastRun + cfgGroup->wait)) continue;
            cfgGroup->wait = 0;

            mLog["g"] = group;
            mLog["s"] = (uint8_t)cfgGroup->state;

            switch (cfgGroup->state) {
                case zeroExportState::INIT:
                    if (groupInit(group, &Tsp, &DoLog)) {
                        cfgGroup->state = zeroExportState::WAITREFRESH;
                        cfgGroup->wait = 60000;
                    } else {
                        cfgGroup->state = zeroExportState::INIT;
                        cfgGroup->wait = 60000;
#if defined(ZEROEXPORT_DEV_POWERMETER)
                        cfgGroup->state = zeroExportState::WAITREFRESH;
                        cfgGroup->wait = 10000;
#endif
                    }
                    break;
                case zeroExportState::WAITREFRESH:
                    if (groupWaitRefresh(group, &Tsp, &DoLog)) {
                        cfgGroup->state = zeroExportState::GETINVERTERDATA;
#if defined(ZEROEXPORT_DEV_POWERMETER)
                        cfgGroup->state = zeroExportState::GETPOWERMETER;
#endif
                    }
                    break;
                case zeroExportState::GETINVERTERDATA:
                    if (groupGetInverterData(group, &Tsp, &DoLog)) {
                        cfgGroup->state = zeroExportState::BATTERYPROTECTION;
                    } else {
                        cfgGroup->wait = 500;
                    }
                    break;
                case zeroExportState::BATTERYPROTECTION:
                    if (groupBatteryprotection(group, &Tsp, &DoLog)) {
                        cfgGroup->state = zeroExportState::GETPOWERMETER;
                    } else {
                        cfgGroup->wait = 1000;
                    }
                    break;
                case zeroExportState::GETPOWERMETER:
                    if (groupGetPowermeter(group, &Tsp, &DoLog)) {
                        cfgGroup->state = zeroExportState::CONTROLLER;
#if defined(ZEROEXPORT_DEV_POWERMETER)
                        cfgGroup->lastRefresh = millis();
                        cfgGroup->state = zeroExportState::PUBLISH;
#endif
                    } else {
                        cfgGroup->wait = 3000;
                    }
                    break;
                case zeroExportState::CONTROLLER:
                    if (groupController(group, &Tsp, &DoLog)) {
                        cfgGroup->lastRefresh = Tsp;
                        cfgGroup->state = zeroExportState::PROGNOSE;
                    } else {
                        cfgGroup->state = zeroExportState::WAITREFRESH;
                    }
                    break;
                case zeroExportState::PROGNOSE:
                    if (groupPrognose(group, &Tsp, &DoLog)) {
                        cfgGroup->state = zeroExportState::AUFTEILEN;
                    } else {
                        cfgGroup->wait = 500;
                    }
                    break;
                case zeroExportState::AUFTEILEN:
                    if (groupAufteilen(group, &Tsp, &DoLog)) {
                        cfgGroup->state = zeroExportState::SETREBOOT;
                    } else {
                        cfgGroup->wait = 500;
                    }
                    break;
                case zeroExportState::SETREBOOT:
                    if (groupSetReboot(group, &Tsp, &DoLog)) {
                        cfgGroup->state = zeroExportState::SETPOWER;
                    } else {
                        cfgGroup->wait = 1000;
                    }
                    break;
                case zeroExportState::SETPOWER:
                    if (groupSetPower(group, &Tsp, &DoLog)) {
                        cfgGroup->lastRefresh = Tsp;
                        cfgGroup->state = zeroExportState::SETLIMIT;
                    } else {
                        cfgGroup->wait = 1000;
                    }
                    break;
                case zeroExportState::SETLIMIT:
                    if (groupSetLimit(group, &Tsp, &DoLog)) {
                        cfgGroup->state = zeroExportState::PUBLISH;
                    } else {
                        cfgGroup->wait = 1000;
                    }
                    break;
                case zeroExportState::PUBLISH:
                    if (groupPublish(group, &Tsp, &DoLog)) {
                        cfgGroup->state = zeroExportState::WAITREFRESH;
                        //} else {
                        // TODO: fehlt
                    }
                    break;
                case zeroExportState::EMERGENCY:
                    if (groupEmergency(group, &Tsp, &DoLog)) {
                        cfgGroup->lastRefresh = Tsp;
                        cfgGroup->state = zeroExportState::INIT;
                        //} else {
                        // TODO: fehlt
                    }
                    break;
                case zeroExportState::FINISH:
                case zeroExportState::ERROR:
                default:
                    cfgGroup->lastRun = Tsp;
                    cfgGroup->lastRefresh = Tsp;
                    cfgGroup->wait = 1000;
                    cfgGroup->state = zeroExportState::INIT;
                    break;
            }

            if (mCfg->debug) {
                unsigned long eTsp = millis();
                mLog["B"] = Tsp;
                mLog["E"] = eTsp;
                mLog["D"] = eTsp - Tsp;
            }

            if (DoLog) sendLog();
            clearLog();
            DoLog = false;
        }
        return;
    }

    /** tickSecond
     * Time pulse every second
     * @param void
     * @returns void
     */
    void tickSecond() {
        if ((!mIsInitialized) || (!mCfg->enabled)) return;

        // Wait for ACK
        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                // Limit
                if (mCfg->groups[group].inverters[inv].waitLimitAck > 0) {
                    mCfg->groups[group].inverters[inv].waitLimitAck--;
                }
                // Power
                if (mCfg->groups[group].inverters[inv].waitPowerAck > 0) {
                    mCfg->groups[group].inverters[inv].waitPowerAck--;
                }
                // Reboot
                if (mCfg->groups[group].inverters[inv].waitRebootAck > 0) {
                    mCfg->groups[group].inverters[inv].waitRebootAck--;
                }
            }
        }
    }

    /** tickerMidnight
     * Time pulse Midnicht
     * @param void
     * @returns void
     */
    void tickMidnight(void) {
        if ((!mIsInitialized) || (!mCfg->enabled)) return;

        // Select all Inverter to reboot
        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                //                mCfg->groups[group].inverters[inv].doReboot = 1;
            }
        }
    }

    /** resetWaitLimitAck
     * Reset waiting time limit
     * @param iv
     * @returns void
     */
    void resetWaitLimitAck(Inverter<> *iv) {
        if ((!mIsInitialized) || (!mCfg->enabled)) return;

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            bool DoLog = false;
            unsigned long bTsp = millis();

            mLog["g"] = group;
            mLog["t"] = "resetWaitLimitAck";

            JsonArray logArr = mLog.createNestedArray("ix");

            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                if (iv->id == (uint8_t)mCfg->groups[group].inverters[inv].id) {
                    JsonObject logObj = logArr.createNestedObject();

                    logObj["i"] = inv;
                    logObj["id"] = iv->id;
                    mCfg->groups[group].inverters[inv].waitLimitAck = 0;
                    logObj["wL"] = mCfg->groups[group].inverters[inv].waitLimitAck;

                    DoLog = true;
                }
            }

            if (mCfg->debug) {
                unsigned long eTsp = millis();
                mLog["B"] = bTsp;
                mLog["E"] = eTsp;
                mLog["D"] = eTsp - bTsp;
            }

            if (DoLog) sendLog();
            clearLog();
            DoLog = false;
        }
    }

    /** resetPowerAck
     * Reset waiting time power
     * @param iv
     * @returns void
     */
    void resetWaitPowerAck(Inverter<> *iv) {
        if ((!mIsInitialized) || (!mCfg->enabled)) return;

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            bool DoLog = false;
            unsigned long bTsp = millis();

            mLog["g"] = group;
            mLog["t"] = "resetWaitPowerAck";

            JsonArray logArr = mLog.createNestedArray("ix");

            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                if (iv->id == mCfg->groups[group].inverters[inv].id) {
                    JsonObject logObj = logArr.createNestedObject();

                    logObj["i"] = inv;
                    logObj["id"] = iv->id;
                    mCfg->groups[group].inverters[inv].waitPowerAck = 30;
                    logObj["wP"] = mCfg->groups[group].inverters[inv].waitPowerAck;

                    DoLog = true;
                }
            }

            if (mCfg->debug) {
                unsigned long eTsp = millis();
                mLog["B"] = bTsp;
                mLog["E"] = eTsp;
                mLog["D"] = eTsp - bTsp;
            }

            if (DoLog) sendLog();
            clearLog();
            DoLog = false;
        }
    }

    /** resetWaitRebootAck
     * Reset waiting time reboot
     * @param iv
     * @returns void
     */
    void resetWaitRebootAck(Inverter<> *iv) {
        if ((!mIsInitialized) || (!mCfg->enabled)) return;

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            bool DoLog = false;
            unsigned long bTsp = millis();

            mLog["g"] = group;
            mLog["t"] = "resetWaitRebootAck";

            JsonArray logArr = mLog.createNestedArray("ix");

            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                if (iv->id == mCfg->groups[group].inverters[inv].id) {
                    JsonObject logObj = logArr.createNestedObject();

                    logObj["i"] = inv;
                    logObj["id"] = iv->id;
                    mCfg->groups[group].inverters[inv].waitRebootAck = 30;
                    logObj["wR"] = mCfg->groups[group].inverters[inv].waitRebootAck;

                    DoLog = true;
                }
            }

            if (mCfg->debug) {
                unsigned long eTsp = millis();
                mLog["B"] = bTsp;
                mLog["E"] = eTsp;
                mLog["D"] = eTsp - bTsp;
            }

            if (DoLog) sendLog();
            clearLog();
            DoLog = false;
        }
    }

    /** newDataAvailable
     *
     * @param iv
     * @returns void
     */
    void newDataAvailable(Inverter<> *iv) {
        if ((!mIsInitialized) || (!mCfg->enabled)) return;

        if (!iv->isAvailable()) return;

        if (!iv->isProducing()) return;

        if (iv->actPowerLimit == 65535) return;

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                // Wrong Inverter -> ignore
                if (iv->id != mCfg->groups[group].inverters[inv].id) continue;

                // Keine Datenübernahme wenn nicht enabled
                if (!mCfg->groups[group].inverters[inv].enabled) continue;

                // Keine Datenübernahme wenn setReboot läuft
                if (mCfg->groups[group].inverters[inv].waitRebootAck > 0) continue;

                // Keine Datenübernahme wenn setPower läuft
                if (mCfg->groups[group].inverters[inv].waitPowerAck > 0) continue;

                // Keine Datenübernahme wenn setLimit läuft
                if (mCfg->groups[group].inverters[inv].waitLimitAck > 0) continue;

                // Calculate
                int32_t ivLp = iv->actPowerLimit;
                int32_t ivPm = iv->getMaxPower();
                ;
                int32_t ivL = (ivPm * ivLp) / 100;
                int32_t zeL = mCfg->groups[group].inverters[inv].limit;

                // Keine Datenübernahme wenn zeL gleich ivL
                if (zeL == ivL) continue;

                unsigned long bTsp = millis();

                mLog["t"] = "newDataAvailable";
                mLog["g"] = group;
                mLog["i"] = inv;
                mLog["id"] = iv->id;
                mLog["ivL%"] = ivLp;
                mLog["ivPm"] = ivPm;
                mLog["ivL"] = ivL;
                mLog["zeL"] = zeL;
                mCfg->groups[group].inverters[inv].limit = ivL;

                if (mCfg->debug) {
                    unsigned long eTsp = millis();
                    mLog["B"] = bTsp;
                    mLog["E"] = eTsp;
                    mLog["D"] = eTsp - bTsp;
                }
                sendLog();
                clearLog();
                return;
            }
        }
    }

    /** onMqttMessage
     * Subscribe section
     */
    void onMqttMessage(JsonObject obj) {
        if (!mIsInitialized) return;

        String topic = String(obj["topic"]);
        if (!topic.indexOf("/zero/set/")) return;

        mLog["t"] = "onMqttMessage";

        if (obj["path"] == "zero" && obj["cmd"] == "set") {
            int8_t topicGroup = getGroupFromTopic(topic.c_str());
            mLog["topicGroup"] = topicGroup;
            int8_t topicInverter = getInverterFromTopic(topic.c_str());
            mLog["topicInverter"] = topicInverter;

            if ((topicGroup == -1) && (topicInverter == -1)) {
                // "topic":"???/zero/set/enabled"
                if (topic.indexOf("/zero/set/enabled") != -1) {
                    mCfg->enabled = (bool)obj["val"];
                    mLog["mCfg->enabled"] = mCfg->enabled;
                    // Initialize groups
                    for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
                        mCfg->groups[group].state = zeroExportState::INIT;
                        mCfg->groups[group].wait = 0;
                    }
                }
                // "topic":"???/zero/set/sleep"
                if (topic.indexOf("/zero/set/sleep") != -1) {
                    mCfg->sleep = (bool)obj["val"];
                    mLog["mCfg->sleep"] = mCfg->sleep;
                }
            } else if ((topicGroup != -1) && (topicInverter == -1)) {
                // "topic":"???/zero/set/groups/0/???"
                mLog["g"] = topicGroup;
                // "topic":"???/zero/set/groups/0/enabled"
                if (topic.indexOf("enabled") != -1) {
                    mCfg->groups[topicGroup].enabled = (bool)obj["val"];
                    // Initialize group
                    mCfg->groups[topicGroup].state = zeroExportState::INIT;
                    mCfg->groups[topicGroup].wait = 0;
                }
                // "topic":"???/zero/set/groups/0/sleep"
                if (topic.indexOf("sleep") != -1) {
                    mCfg->groups[topicGroup].sleep = (bool)obj["val"];
                }
                // "topic":"???/zero/set/groups/0/battery/switch"
                if (topic.indexOf("battery/switch") != -1) {
                    mCfg->groups[topicGroup].battSwitch = (bool)obj["val"];
                }
                // "topic":"???/zero/set/groups/0/advanced/setPoint"
                if (topic.indexOf("advanced/setPoint") != -1) {
                    mCfg->groups[topicGroup].setPoint = (int32_t)obj["val"];
                }
                // "topic":"???/zero/set/groups/0/advanced/powerTolerance"
                if (topic.indexOf("advanced/powerTolerance") != -1) {
                    mCfg->groups[topicGroup].powerTolerance = (uint16_t)obj["val"];
                }
                // "topic":"???/zero/set/groups/0/advanced/powerMax"
                if (topic.indexOf("advanced/powerMax") != -1) {
                    mCfg->groups[topicGroup].powerMax = (uint16_t)obj["val"];
                }
            } else if ((topicGroup != -1) && (topicInverter != -1)) {
                // "topic":"???/zero/set/groups/0/inverter/0/enabled"
                if (topic.indexOf("enabled") != -1) {
                    mCfg->groups[topicGroup].inverters[topicInverter].enabled = (bool)obj["val"];
                }
                // "topic":"???/zero/set/groups/0/inverter/0/powerMin"
                if (topic.indexOf("powerMin") != -1) {
                    mCfg->groups[topicGroup].inverters[topicInverter].powerMin = (uint16_t)obj["val"];
                }
                // "topic":"???/zero/set/groups/0/inverter/0/powerMax"
                if (topic.indexOf("powerMax") != -1) {
                    mCfg->groups[topicGroup].inverters[topicInverter].powerMax = (uint16_t)obj["val"];
                }
            }
        }

        mLog["Msg"] = obj;
        sendLog();
        clearLog();
        return;
    }

   private:
    /** NotEnabledOrNotSelected
     * Inverter not enabled -> ignore || Inverter not selected -> ignore
     * @param group
     * @param inv
     * @returns true/false
     */
    bool NotEnabledOrNotSelected(uint8_t group, uint8_t inv) {
        return ((!mCfg->groups[group].inverters[inv].enabled) || (mCfg->groups[group].inverters[inv].id < 0));
    }

    /** getGroupFromTopic
     *
     * @param
     * @returns
     */
    int8_t getGroupFromTopic(const char *topic) {
        const char *pGroupSection = strstr(topic, "groups/");
        if (pGroupSection == NULL) return -1;
        pGroupSection += 7;
        char strGroup[3];
        uint8_t digitsCopied = 0;
        while (*pGroupSection != '/' && digitsCopied < 2) strGroup[digitsCopied++] = *pGroupSection++;
        strGroup[digitsCopied] = '\0';
        int8_t group = atoi(strGroup);
        return group;
    }

    /** getInverterFromTopic
     *
     * @param
     * @returns
     */
    int8_t getInverterFromTopic(const char *topic) {
        const char *pInverterSection = strstr(topic, "inverters/");
        if (pInverterSection == NULL) return -1;
        pInverterSection += 10;
        char strInverter[3];
        uint8_t digitsCopied = 0;
        while (*pInverterSection != '/' && digitsCopied < 2) strInverter[digitsCopied++] = *pInverterSection++;
        strInverter[digitsCopied] = '\0';
        int8_t inverter = atoi(strInverter);
        return inverter;
    }

    /** groupInit
     * Initialize the group and search the InverterPointer
     * @param group
     * @returns true/false
     * @todo getInverterById statt getInverterByPos, dann würde die Variable *iv und die Schleife nicht gebraucht.
     */
    bool groupInit(uint8_t group, unsigned long *tsp, bool *doLog) {
        uint8_t result = false;

        if (mCfg->debug) mLog["t"] = "groupInit";

        mCfg->groups[group].lastRun = *tsp;

        *doLog = true;

        // Init ivPointer
        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
            mIv[group][inv] = nullptr;
        }

        // Search/Set ivPointer
        JsonArray logArr = mLog.createNestedArray("ix");

        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
            JsonObject logObj = logArr.createNestedObject();
            logObj["i"] = inv;

            mIv[group][inv] = nullptr;

            // Inverter not enabled or not selected -> ignore
            if (NotEnabledOrNotSelected(group, inv)) continue;

            // Load Config
            Inverter<> *iv;
            for (uint8_t i = 0; i < mSys->getNumInverters(); i++) {
                iv = mSys->getInverterByPos(i);

                // Inverter not configured -> ignore
                if (iv == NULL) continue;

                // Inverter not matching -> ignore
                if (iv->id != (uint8_t)mCfg->groups[group].inverters[inv].id) continue;

                // Save Inverter
                logObj["pos"] = i;
                logObj["id"] = iv->id;
                mIv[group][inv] = mSys->getInverterByPos(i);

                result = true;
            }
        }

        // Init Acks
        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
            mCfg->groups[group].inverters[inv].waitLimitAck = 0;
            mCfg->groups[group].inverters[inv].waitPowerAck = 0;
            mCfg->groups[group].inverters[inv].waitRebootAck = 0;
        }

        mCfg->groups[group].lastRefresh = *tsp;

        return result;
    }

    /** groupWaitRefresh
     * Pauses the group until the wait time since the lastRefresh has expired.
     * @param group
     * @returns true/false
     */
    bool groupWaitRefresh(uint8_t group, unsigned long *tsp, bool *doLog) {
        if (mCfg->debug) mLog["t"] = "groupWaitRefresh";

        // Wait Refreshtime
        if (*tsp <= (mCfg->groups[group].lastRefresh + (mCfg->groups[group].refresh * 1000UL))) return false;

        mCfg->groups[group].lastRun = *tsp;

        *doLog = true;

        return true;
    }

    /** groupGetInverterData
     *
     * @param group
     * @returns true/false
     */
    bool groupGetInverterData(uint8_t group, unsigned long *tsp, bool *doLog) {
        if (mCfg->debug) mLog["t"] = "groupGetInverterData";

        mCfg->groups[group].lastRun = *tsp;

        mCfg->groups[group].power = 0;

//        *doLog = true;

        // Get Data
        JsonArray logArr = mLog.createNestedArray("ix");
///        bool wait = false;
        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
            JsonObject logObj = logArr.createNestedObject();
            logObj["i"] = inv;

            // Inverter not enabled or not selected -> ignore
            if (NotEnabledOrNotSelected(group, inv)) continue;

//            if (!mIv[group][inv]->isAvailable()) continue;

            // Get Pac
            record_t<> *rec;
            rec = mIv[group][inv]->getRecordStruct(RealTimeRunData_Debug);
            mCfg->groups[group].power += mIv[group][inv]->getChannelFieldValue(CH0, FLD_PAC, rec);

            // TODO: Save Hole Power für die Webanzeige
        }

        return true;
    }

    /** groupBatteryprotection
     * Batterieschutzfunktion
     * @param group
     * @returns true/false
     */
    bool groupBatteryprotection(uint8_t group, unsigned long *tsp, bool *doLog) {
        if (mCfg->debug) mLog["t"] = "groupBatteryprotection";

        mCfg->groups[group].lastRun = *tsp;

        *doLog = true;

        // Protection
        if (mCfg->groups[group].battEnabled) {
            mLog["en"] = true;

            // Config - parameter check
            if (mCfg->groups[group].battVoltageOn <= mCfg->groups[group].battVoltageOff) {
                mCfg->groups[group].battSwitch = false;
                mLog["err"] = "Config - battVoltageOn(" + (String)mCfg->groups[group].battVoltageOn + ") <= battVoltageOff(" + (String)mCfg->groups[group].battVoltageOff + ")";
                return false;
            }

            // Config - parameter check
            if (mCfg->groups[group].battVoltageOn <= (mCfg->groups[group].battVoltageOff + 1)) {
                mCfg->groups[group].battSwitch = false;
                mLog["err"] = "Config - battVoltageOn(" + (String)mCfg->groups[group].battVoltageOn + ") <= battVoltageOff(" + (String)mCfg->groups[group].battVoltageOff + " + 1V)";
                return false;
            }

            // Config - parameter check
            if (mCfg->groups[group].battVoltageOn <= 22) {
                mCfg->groups[group].battSwitch = false;
                mLog["err"] = "Config - battVoltageOn(" + (String)mCfg->groups[group].battVoltageOn + ") <= 22V)";
                return false;
            }

            int8_t id = 0;
            float U = 0;

            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                zeroExportGroupInverter_t *cfgGroupInv = &mCfg->groups[group].inverters[inv];

                // Ignore disabled Inverter
                if (!cfgGroupInv->enabled) continue;

                if (cfgGroupInv->id < 0) continue;

                if (!mIv[group][inv]->isAvailable()) {
                    if (U > 0) continue;
                    U = 0;
                    id = cfgGroupInv->id;
                    continue;
                }

                // Get U
                record_t<> *rec;
                rec = mIv[group][inv]->getRecordStruct(RealTimeRunData_Debug);
                float tmp = mIv[group][inv]->getChannelFieldValue(CH1, FLD_UDC, rec);
                if (U == 0) {
                    U = tmp;
                    id = cfgGroupInv->id;
                }
                if (U > tmp) {
                    U = tmp;
                    id = cfgGroupInv->id;
                }
            }

            mLog["i"] = id;
            mLog["U"] = U;

            // Switch to ON
            if (U > mCfg->groups[group].battVoltageOn) {
                mCfg->groups[group].battSwitch = true;
                mLog["act"] = "On";

                for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                    //                        zeroExportGroupInverter_t *cfgGroupInv = &mCfg->groups[group].inverters[inv];

                    // Inverter not enabled or not selected -> ignore
                    if (NotEnabledOrNotSelected(group, inv)) continue;

                    // Abbruch weil Inverter nicht verfügbar
                    if (!mIv[group][inv]->isAvailable()) {
                        mLog["err"] = "is not Available";
                        continue;
                    }

                    // Nothing todo
                    if (mCfg->groups[group].battSwitch == mIv[group][inv]->isProducing()) {
                        mLog["err"] = "battSwitch " + String(mCfg->groups[group].battSwitch) + " == isProducing()" + String(mIv[group][inv]->isProducing());
                        continue;
                    }

                    mCfg->groups[group].state = zeroExportState::SETPOWER;
                }
            }

            // Switch to OFF
            if (U < mCfg->groups[group].battVoltageOff) {
                mCfg->groups[group].battSwitch = false;
                mLog["act"] = "Off";

                for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                    //                        zeroExportGroupInverter_t *cfgGroupInv = &mCfg->groups[group].inverters[inv];

                    // Inverter not enabled or not selected -> ignore
                    if (NotEnabledOrNotSelected(group, inv)) continue;

                    // Abbruch weil Inverter nicht verfügbar
                    if (!mIv[group][inv]->isAvailable()) {
                        mLog["err"] = "is not Available";
                        continue;
                    }

                    // Nothing todo
                    if (mCfg->groups[group].battSwitch == mIv[group][inv]->isProducing()) {
                        mLog["err"] = "battSwitch " + String(mCfg->groups[group].battSwitch) + " == isProducing()" + String(mIv[group][inv]->isProducing());
                        continue;
                    }

                    mCfg->groups[group].state = zeroExportState::SETPOWER;
                }
            }
        } else {
            mLog["en"] = false;

            mCfg->groups[group].battSwitch = true;
        }

        mLog["sw"] = mCfg->groups[group].battSwitch;

        return true;
    }

    /** groupGetPowermeter
     * Holt die Daten vom Powermeter
     * @param group
     * @returns true/false
     * @todo Eventuell muss am Ende geprüft werden ob die Daten vom Powermeter plausibel sind.
     */
    bool groupGetPowermeter(uint8_t group, unsigned long *tsp, bool *doLog) {
        if (mCfg->debug) mLog["t"] = "groupGetPowermeter";
        mCfg->groups[group].lastRun = *tsp;

        *doLog = true;

        mCfg->groups[group].pm_P = mPowermeter.getDataAVG(group).P;
        mCfg->groups[group].pm_P1 = mPowermeter.getDataAVG(group).P1;
        mCfg->groups[group].pm_P2 = mPowermeter.getDataAVG(group).P2;
        mCfg->groups[group].pm_P3 = mPowermeter.getDataAVG(group).P3;

        if (
            (mCfg->groups[group].pm_P == 0) &&
            (mCfg->groups[group].pm_P1 == 0) &&
            (mCfg->groups[group].pm_P2 == 0) &&
            (mCfg->groups[group].pm_P3 == 0)) {
            return false;
        }

        mLog["P"] = mCfg->groups[group].pm_P;
        mLog["P1"] = mCfg->groups[group].pm_P1;
        mLog["P2"] = mCfg->groups[group].pm_P2;
        mLog["P3"] = mCfg->groups[group].pm_P3;

        // Powermeter
        //            if (cfgGroup->publishPower) {
        //                cfgGroup->publishPower = false;
        mqttObj["Sum"] = mCfg->groups[group].pm_P;
        mqttObj["L1"] = mCfg->groups[group].pm_P1;
        mqttObj["L2"] = mCfg->groups[group].pm_P2;
        mqttObj["L3"] = mCfg->groups[group].pm_P3;
        mMqtt->publish("zero/state/powermeter/P", mqttDoc.as<std::string>().c_str(), false);
        mqttDoc.clear();
        //            }

        //            if (cfgGroup->pm_Publish_W) {
        //                cfgGroup->pm_Publish_W = false;
        //                obj["todo"]  = "true";
        //                obj["Sum"] = cfgGroup->pm_P;
        //                obj["L1"]  = cfgGroup->pm_P1;
        //                obj["L2"]  = cfgGroup->pm_P2;
        //                obj["L2"]  = cfgGroup->pm_P3;
        //                mMqtt->publish("zero/powermeter/W", doc.as<std::string>().c_str(), false);
        //                doc.clear();
        //            }
        return true;
    }

    /** groupController
     * PID-Regler
     * @param group
     * @returns true/false
     */
    bool groupController(uint8_t group, unsigned long *tsp, bool *doLog) {
        zeroExportGroup_t *cfgGroup = &mCfg->groups[group];

        if (mCfg->debug) mLog["t"] = "groupController";

        cfgGroup->lastRun = *tsp;

        *doLog = true;

        // Führungsgröße w in Watt
        int32_t w = cfgGroup->setPoint;
        mLog["w"] = w;

        // Regelgröße x in Watt
        int32_t x = cfgGroup->pm_P;
        int32_t x1 = cfgGroup->pm_P1;
        int32_t x2 = cfgGroup->pm_P2;
        int32_t x3 = cfgGroup->pm_P3;
        mLog["x"] = x;
        mLog["x1"] = x1;
        mLog["x2"] = x2;
        mLog["x3"] = x3;

        // Regelabweichung e in Watt
        int32_t e = w - x;
        int32_t e1 = w - x1;
        int32_t e2 = w - x2;
        int32_t e3 = w - x3;
        mLog["e"] = e;
        mLog["e1"] = e1;
        mLog["e2"] = e2;
        mLog["e3"] = e3;
        if (
            (e < cfgGroup->powerTolerance) && (e > -cfgGroup->powerTolerance) &&
            (e1 < cfgGroup->powerTolerance) && (e1 > -cfgGroup->powerTolerance) &&
            (e2 < cfgGroup->powerTolerance) && (e2 > -cfgGroup->powerTolerance) &&
            (e3 < cfgGroup->powerTolerance) && (e3 > -cfgGroup->powerTolerance)) {
            mLog["tol"] = cfgGroup->powerTolerance;
            return false;
        }

        // Regler
        float Kp = cfgGroup->Kp;
        float Ki = cfgGroup->Ki;
        float Kd = cfgGroup->Kd;
        unsigned long Ta = *tsp - mCfg->groups[group].lastRefresh;
        mLog["Kp"] = Kp;
        mLog["Ki"] = Ki;
        mLog["Kd"] = Kd;
        mLog["Ta"] = Ta;
        // - P-Anteil
        int32_t yP = Kp * e;
        int32_t yP1 = Kp * e1;
        int32_t yP2 = Kp * e2;
        int32_t yP3 = Kp * e3;
        mLog["yP"] = yP;
        mLog["yP1"] = yP1;
        mLog["yP2"] = yP2;
        mLog["yP3"] = yP3;
        // - I-Anteil
        cfgGroup->eSum += e;
        cfgGroup->eSum1 += e1;
        cfgGroup->eSum2 += e2;
        cfgGroup->eSum3 += e3;
        mLog["esum"] = cfgGroup->eSum;
        mLog["esum1"] = cfgGroup->eSum1;
        mLog["esum2"] = cfgGroup->eSum2;
        mLog["esum3"] = cfgGroup->eSum3;
        int32_t yI = Ki * Ta * cfgGroup->eSum;
        int32_t yI1 = Ki * Ta * cfgGroup->eSum1;
        int32_t yI2 = Ki * Ta * cfgGroup->eSum2;
        int32_t yI3 = Ki * Ta * cfgGroup->eSum3;
        mLog["yI"] = yI;
        mLog["yI1"] = yI1;
        mLog["yI2"] = yI2;
        mLog["yI3"] = yI3;
        // - D-Anteil
        mLog["ealt"] = cfgGroup->eOld;
        mLog["ealt1"] = cfgGroup->eOld1;
        mLog["ealt2"] = cfgGroup->eOld2;
        mLog["ealt3"] = cfgGroup->eOld3;
        int32_t yD = Kd * (e - cfgGroup->eOld) / Ta;
        int32_t yD1 = Kd * (e1 - cfgGroup->eOld1) / Ta;
        int32_t yD2 = Kd * (e2 - cfgGroup->eOld2) / Ta;
        int32_t yD3 = Kd * (e3 - cfgGroup->eOld3) / Ta;
        mLog["yD"] = yD;
        mLog["yD1"] = yD1;
        mLog["yD2"] = yD2;
        mLog["yD3"] = yD3;
        cfgGroup->eOld = e;
        cfgGroup->eOld1 = e1;
        cfgGroup->eOld2 = e2;
        cfgGroup->eOld3 = e3;
        // - PID-Anteil
        int32_t y = yP + yI + yD;
        int32_t y1 = yP1 + yI1 + yD1;
        int32_t y2 = yP2 + yI2 + yD2;
        int32_t y3 = yP3 + yI3 + yD3;

        // Regelbegrenzung
        // TODO: Hier könnte man den maximalen Sprung begrenzen

        cfgGroup->y = y;
        cfgGroup->y1 = y1;
        cfgGroup->y2 = y2;
        cfgGroup->y3 = y3;
        mLog["y"] = y;
        mLog["y1"] = y1;
        mLog["y2"] = y2;
        mLog["y3"] = y3;

        return true;
    }

    /** groupPrognose
     *
     * @param group
     * @returns true/false
     */
    bool groupPrognose(uint8_t group, unsigned long *tsp, bool *doLog) {
        if (mCfg->debug) mLog["t"] = "groupPrognose";

        mCfg->groups[group].lastRun = *tsp;

        //        *doLog = true;

        return true;
    }

    /** groupAufteilen
     *
     * @param group
     * @returns true/false
     */
    bool groupAufteilen(uint8_t group, unsigned long *tsp, bool *doLog) {
        if (mCfg->debug) mLog["t"] = "groupAufteilen";

        mCfg->groups[group].lastRun = *tsp;

        *doLog = true;

        int32_t y = mCfg->groups[group].y;
        int32_t y1 = mCfg->groups[group].y1;
        int32_t y2 = mCfg->groups[group].y2;
        int32_t y3 = mCfg->groups[group].y3;

if (mCfg->groups[group].power > mCfg->groups[group].powerMax) {
    int32_t diff = mCfg->groups[group].power - mCfg->groups[group].powerMax;
    y = y - diff;
    y1 = y1 - (diff * y1 / y);
    y1 = y1 - (diff * y2 / y);
    y1 = y1 - (diff * y3 / y);
}

        // TDOD: nochmal durchdenken ... es muss für Sum und L1-3 sein
        //        uint16_t groupPmax = 0;
        //        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
        //            groupPmax = mCfg->groups[group].inverters[inv].limit;
        //        }
        //        int16_t groupPavail = mCfg->groups[group].powerMax - groupPmax;
        //
        //        if ( y > groupPavail ) {
        //            y = groupPavail;
        //        }

        mLog["y"] = y;
        mLog["y1"] = y1;
        mLog["y2"] = y2;
        mLog["y3"] = y3;

        bool grpTarget[7] = {false, false, false, false, false, false, false};
        int8_t ivId_Pmin[7] = {-1, -1, -1, -1, -1, -1, -1};
        int8_t ivId_Pmax[7] = {-1, -1, -1, -1, -1, -1, -1};
        uint16_t ivPmin[7] = {65535, 65535, 65535, 65535, 65535, 65535, 65535};
        uint16_t ivPmax[7] = {0, 0, 0, 0, 0, 0, 0};

        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
            zeroExportGroupInverter_t *cfgGroupInv = &mCfg->groups[group].inverters[inv];

            if (!cfgGroupInv->enabled) {
                continue;
            }

            record_t<> *rec;
            rec = mIv[group][inv]->getRecordStruct(RealTimeRunData_Debug);
            cfgGroupInv->power = mIv[group][inv]->getChannelFieldValue(CH0, FLD_PAC, rec);

            if ((cfgGroupInv->power < ivPmin[cfgGroupInv->target]) && (cfgGroupInv->limit < cfgGroupInv->powerMax) && (cfgGroupInv->limit < mIv[group][inv]->getMaxPower())) {
                grpTarget[cfgGroupInv->target] = true;
                ivPmin[cfgGroupInv->target] = cfgGroupInv->power;
                ivId_Pmin[cfgGroupInv->target] = inv;
                // Hier kein return oder continue sonst dauerreboot
            }

            if ((cfgGroupInv->power > ivPmax[cfgGroupInv->target]) && (cfgGroupInv->limit > cfgGroupInv->powerMin) && (cfgGroupInv->limit > (mIv[group][inv]->getMaxPower() * 2 / 100))) {
                grpTarget[cfgGroupInv->target] = true;
                ivPmax[cfgGroupInv->target] = cfgGroupInv->power;
                ivId_Pmax[cfgGroupInv->target] = inv;
                // Hier kein return oder continue sonst dauerreboot
            }
        }
        for (uint8_t i = 0; i < 7; i++) {
            mLog[String(i)] = String(i) + String(" grpTarget: ") + String(grpTarget[i]) + String(": ivPmin: ") + String(ivPmin[i]) + String(": ivPmax: ") + String(ivPmax[i]) + String(": ivId_Pmin: ") + String(ivId_Pmin[i]) + String(": ivId_Pmax: ") + String(ivId_Pmax[i]);
        }

        for (int8_t i = (7 - 1); i >= 0; i--) {
            if (!grpTarget[i]) {
                continue;
            }
            mLog[String(String("10") + String(i))] = String(i);

            int32_t *deltaP;
            switch (i) {
                case 6:
                case 3:
                    deltaP = &mCfg->groups[group].y3;
                    break;
                case 5:
                case 2:
                    deltaP = &mCfg->groups[group].y2;
                    break;
                case 4:
                case 1:
                    deltaP = &mCfg->groups[group].y1;
                    break;
                case 0:
                    deltaP = &mCfg->groups[group].y;
                    break;
            }

            mLog["dP"] = *deltaP;

            // Leistung erhöhen
            if (*deltaP > 0) {
                zeroExportGroupInverter_t *cfgGroupInv = &mCfg->groups[group].inverters[ivId_Pmin[i]];
                cfgGroupInv->limitNew = cfgGroupInv->limit + *deltaP;
                //                    cfgGroupInv->limitNew = (uint16_t)((float)cfgGroupInv->limit + *deltaP);
                //                    if (i != 0) {
                //                        cfgGroup->grpPower - *deltaP;
                //                    }
                *deltaP = 0;
                //                    if (cfgGroupInv->limitNew > cfgGroupInv->powerMax) {
                //                        *deltaP = cfgGroupInv->limitNew - cfgGroupInv->powerMax;
                //                        cfgGroupInv->limitNew = cfgGroupInv->powerMax;
                //                        if (i != 0) {
                //                            cfgGroup->grpPower + *deltaP;
                //                        }
                //                    }
                ///                    setLimit(&mLog, group, ivId_Pmin[i]);
                ///                    mCfg->groups[group].waitForAck = true;
                // TODO: verschieben in anderen Step.
                continue;
            }

            // Leistung reduzieren
            if (*deltaP < 0) {
                zeroExportGroupInverter_t *cfgGroupInv = &mCfg->groups[group].inverters[ivId_Pmax[i]];
                cfgGroupInv->limitNew = cfgGroupInv->limit + *deltaP;
                //                    cfgGroupInv->limitNew = (uint16_t)((float)cfgGroupInv->limit + *deltaP);
                //                    if (i != 0) {
                //                        cfgGroup->grpPower - *deltaP;
                //                    }
                *deltaP = 0;
                //                    if (cfgGroupInv->limitNew < cfgGroupInv->powerMin) {
                //                        *deltaP = cfgGroupInv->limitNew - cfgGroupInv->powerMin;
                //                        cfgGroupInv->limitNew = cfgGroupInv->powerMin;
                //                        if (i != 0) {
                //                            cfgGroup->grpPower + *deltaP;
                //                        }
                //                    }
                ///                    setLimit(&mLog, group, ivId_Pmax[i]);
                ///                    mCfg->groups[group].waitForAck = true;
                // TODO: verschieben in anderen Step.
                continue;
            }
        }

        return true;
    }

    /** groupSetReboot
     *
     * @param group
     * @param tsp
     * @param doLog
     * @returns true/false
     */
    bool groupSetReboot(uint8_t group, unsigned long *tsp, bool *doLog) {
        zeroExportGroup_t *cfgGroup = &mCfg->groups[group];
        bool result = true;

        if (mCfg->debug) mLog["t"] = "groupSetReboot";

        cfgGroup->lastRun = *tsp;

        JsonArray logArr = mLog.createNestedArray("ix");
        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
            JsonObject logObj = logArr.createNestedObject();
            logObj["i"] = inv;

            zeroExportGroupInverter_t *cfgGroupInv = &cfgGroup->inverters[inv];

            // Inverter not enabled or not selected -> ignore
            if (NotEnabledOrNotSelected(group, inv)) continue;

            // Inverter not available -> ignore
            if (!mIv[group][inv]->isAvailable()) {
                logObj["a"] = false;
                continue;
            }

            logObj["dR"] = cfgGroupInv->doReboot;
            logObj["wR"] = cfgGroupInv->waitRebootAck;

            // Wait
            if (cfgGroupInv->waitRebootAck > 0) {
                result = false;
                if (mCfg->debug) *doLog = true;
                continue;
            }

            // Reset
            if ((cfgGroupInv->doReboot == 2) && (cfgGroupInv->waitRebootAck == 0)) {
                cfgGroupInv->doReboot = -1;
                if (mCfg->debug) {
                    logObj["act"] = "done";
                    *doLog = true;
                }
                continue;
            }

            // Calculate
            if (cfgGroupInv->doReboot == 1) {
                cfgGroupInv->doReboot = 2;
                cfgGroupInv->waitRebootAck = 120;
            }

            // Inverter nothing to do -> ignore
            if (cfgGroupInv->doReboot == -1) {
                if (mCfg->debug) {
                    logObj["act"] = "nothing to do";
                    *doLog = true;
                }
                continue;
            }

            result = false;
            *doLog = true;

            // send Command
            DynamicJsonDocument doc(512);
            JsonObject obj = doc.to<JsonObject>();
            obj["id"] = cfgGroupInv->id;
            obj["path"] = "ctrl";
            obj["cmd"] = "restart";
            mApi->ctrlRequest(obj);
            if (mCfg->debug) logObj["d"] = obj;
        }

        return result;
    }

    /** groupSetPower
     *
     * @param group
     * @param tsp
     * @param doLog
     * @returns true/false
     */
    bool groupSetPower(uint8_t group, unsigned long *tsp, bool *doLog) {
        zeroExportGroup_t *cfgGroup = &mCfg->groups[group];
        bool result = true;

        if (mCfg->debug) mLog["t"] = "groupSetPower";

        cfgGroup->lastRun = *tsp;

        JsonArray logArr = mLog.createNestedArray("ix");
        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
            JsonObject logObj = logArr.createNestedObject();
            logObj["i"] = inv;

            zeroExportGroupInverter_t *cfgGroupInv = &cfgGroup->inverters[inv];

            // Inverter not enabled or not selected -> ignore
            if (NotEnabledOrNotSelected(group, inv)) continue;

            // Inverter not available -> ignore
            if (!mIv[group][inv]->isAvailable()) {
                logObj["a"] = false;
                continue;
            }

            logObj["dP"] = cfgGroupInv->doPower;
            logObj["wP"] = cfgGroupInv->waitPowerAck;

            // Wait
            if (cfgGroupInv->waitPowerAck > 0) {
                result = false;
                if (mCfg->debug) *doLog = true;
                continue;
            }

            // Reset
            if ((cfgGroupInv->doPower != -1) && (cfgGroupInv->waitPowerAck == 0)) {
                cfgGroupInv->doPower = -1;
                if (mCfg->debug) {
                    logObj["act"] = "done";
                    *doLog = true;
                }
                continue;
            }

            // Calculate
            logObj["battSw"] = mCfg->groups[group].battSwitch;
            logObj["ivL"] = cfgGroupInv->limitNew;
            logObj["ivSw"] = mIv[group][inv]->isProducing();
            if (
                (mCfg->groups[group].battSwitch == true) &&
                (cfgGroupInv->limitNew > cfgGroupInv->powerMin) &&
                (mIv[group][inv]->isProducing() == false)) {
                // On
                cfgGroupInv->doPower = 1;
                cfgGroupInv->waitPowerAck = 120;
                logObj["act"] = "on";
            }
            if (
                (
                    (mCfg->groups[group].battSwitch == false) ||
                    (cfgGroupInv->limitNew < (cfgGroupInv->powerMin - 50))) &&
                (mIv[group][inv]->isProducing() == true)) {
                // Off
                cfgGroupInv->doPower = 0;
                cfgGroupInv->waitPowerAck = 120;
                logObj["act"] = "off";
            }

            // Inverter nothing to do -> ignore
            if (cfgGroupInv->doPower == -1) {
                if (mCfg->debug) {
                    logObj["act"] = "nothing to do";
                    *doLog = true;
                }
                continue;
            }

            result = false;
            *doLog = true;

            // send Command
            DynamicJsonDocument doc(512);
            JsonObject obj = doc.to<JsonObject>();
            obj["val"] = cfgGroupInv->doPower;
            obj["id"] = cfgGroupInv->id;
            obj["path"] = "ctrl";
            obj["cmd"] = "power";
            mApi->ctrlRequest(obj);
            if (mCfg->debug) logObj["d"] = obj;
        }

        return result;
    }

    /** groupSetLimit
     * Sets the calculated Limit to the Inverter and waits for ACK.
     * @param group
     * @param tsp
     * @param doLog
     * @returns true/false
     */
    bool groupSetLimit(uint8_t group, unsigned long *tsp, bool *doLog) {
        zeroExportGroup_t *cfgGroup = &mCfg->groups[group];
        bool result = true;

        if (mCfg->debug) mLog["t"] = "groupSetLimit";

        cfgGroup->lastRun = *tsp;

        JsonArray logArr = mLog.createNestedArray("ix");
        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
            JsonObject logObj = logArr.createNestedObject();
            logObj["i"] = inv;

            zeroExportGroupInverter_t *cfgGroupInv = &cfgGroup->inverters[inv];

            // Inverter not enabled or not selected -> ignore
            if (NotEnabledOrNotSelected(group, inv)) continue;

            // Inverter not available -> ignore
            if (!mIv[group][inv]->isAvailable()) {
                logObj["a"] = false;
                continue;
            }

            logObj["dL"] = cfgGroupInv->doLimit;
            logObj["wL"] = cfgGroupInv->waitLimitAck;
            logObj["L"] = cfgGroupInv->limit;

            // Wait
            if (cfgGroupInv->waitLimitAck > 0) {
                result = false;
                if (mCfg->debug) *doLog = true;
                continue;
            }

            // Reset
            if ((cfgGroupInv->doLimit != -1) && (cfgGroupInv->waitLimitAck == 0)) {
                cfgGroupInv->doLimit = -1;
                if (mCfg->debug) {
                    logObj["act"] = "done";
                    *doLog = true;
                }
                continue;
            }

            // Calculate
            uint16_t power100proz = mIv[group][inv]->getMaxPower();
            uint16_t power2proz = (power100proz * 2) / 100;

            // if isOff -> Limit Pmin
            if (!mIv[group][inv]->isProducing()) {
                cfgGroupInv->limitNew = cfgGroupInv->powerMin;
            }

            // Restriction LimitNew < Pmin
            if (cfgGroupInv->limitNew < cfgGroupInv->powerMin) {
                cfgGroupInv->limitNew = cfgGroupInv->powerMin;
            }

            // Restriction LimitNew < 2%
            if (cfgGroupInv->limitNew < power2proz) {
                cfgGroupInv->limitNew = power2proz;
            }

            ///            // Restriction Power + 10% < Limit
            ///            if ((cfgGroupInv->power + (power100proz * 10 / 100)) < cfgGroupInv->limit) {
            ///                cfgGroupInv->limitNew += (power100proz * 10 / 100);
            ///            }

            // Restriction LimitNew > Pmax
            if (cfgGroupInv->limitNew > cfgGroupInv->powerMax) {
                cfgGroupInv->limitNew = cfgGroupInv->powerMax;
            }

            // Restriction LimitNew > 100%
            if (cfgGroupInv->limitNew > power100proz) {
                cfgGroupInv->limitNew = power100proz;
            }

            // Restriction modulo 4 bzw 2
            cfgGroupInv->limitNew = cfgGroupInv->limitNew - (cfgGroupInv->limitNew % 4);
            // TODO: HM-800 kann vermutlich nur in 4W Schritten ... 20W -> 16W ... 100W -> 96W

            // Restriction deltaLimitNew < 5 W
            /*
                        if (
                            (cfgGroupInv->limitNew > (cfgGroupInv->powerMin + ZEROEXPORT_GROUP_WR_LIMIT_MIN_DIFF)) &&
                            (cfgGroupInv->limitNew > (cfgGroupInv->limit + ZEROEXPORT_GROUP_WR_LIMIT_MIN_DIFF)) &&
                            (cfgGroupInv->limitNew < (cfgGroupInv->limit - ZEROEXPORT_GROUP_WR_LIMIT_MIN_DIFF))) {
                            logObj["err"] = String("Diff < ") + String(ZEROEXPORT_GROUP_WR_LIMIT_MIN_DIFF) + String("W");

                            *doLog = true;
                            return false;
                        }
            */

            if (cfgGroupInv->limit != cfgGroupInv->limitNew) {
                cfgGroupInv->doLimit = 1;
                cfgGroupInv->waitLimitAck = 60;
            }

            cfgGroupInv->limit = cfgGroupInv->limitNew;
            logObj["zeL"] = cfgGroupInv->limit;

            // Inverter nothing to do -> ignore
            if (cfgGroupInv->doLimit == -1) {
                if (mCfg->debug) {
                    logObj["act"] = "nothing to do";
                    *doLog = true;
                }
                continue;
            }

            result = false;
            *doLog = true;

            // send Command
            DynamicJsonDocument doc(512);
            JsonObject obj = doc.to<JsonObject>();
            if (cfgGroupInv->limit > 0) {
                obj["val"] = cfgGroupInv->limit;
            } else {
                obj["val"] = 0;
            }
            obj["id"] = cfgGroupInv->id;
            obj["path"] = "ctrl";
            obj["cmd"] = "limit_nonpersistent_absolute";
            mApi->ctrlRequest(obj);
            if (mCfg->debug) logObj["d"] = obj;
        }

        return result;
    }

    void PubSubInit(String gr, String payload)
    {
        mMqtt->publish(gr.c_str(), payload.c_str(), false);
        mMqtt->subscribe(gr.c_str(), QOS_2);
    }

    void mqttInitTopic()
    {
        if (mIsSubscribed) return;
        if (!mMqtt->isConnected()) return;

        mIsSubscribed = true;

        // Global (zeroExport)
        // TODO: Global wird fälschlicherweise hier je nach anzahl der aktivierten Gruppen bis zu 6x ausgeführt.
        mMqtt->publish("zero/set/enabled", ((mCfg->enabled) ? dict[STR_TRUE] : dict[STR_FALSE]), false);
        mMqtt->subscribe("zero/set/enabled", QOS_2);
        // TODO: Global wird fälschlicherweise hier je nach anzahl der aktivierten Gruppen bis zu 6x ausgeführt.
        mMqtt->publish("zero/set/sleep", ((mCfg->sleep) ? dict[STR_TRUE] : dict[STR_FALSE]), false);
        mMqtt->subscribe("zero/set/sleep", QOS_2);


        String gr;
        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++)
        {
            zeroExportGroup_t *cfgGroup = &mCfg->groups[group];
             gr = "zero/set/groups/" + String(group);

            // General
            PubSubInit(gr + "/enabled", ((cfgGroup->enabled) ? dict[STR_TRUE] : dict[STR_FALSE]));
            PubSubInit(gr + "/sleep", ((cfgGroup->enabled) ? dict[STR_TRUE] : dict[STR_FALSE]));

            // Powermeter

            // Inverters
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                zeroExportGroupInverter_t *cfgGroupInv = &cfgGroup->inverters[inv];

                PubSubInit(gr + "/inverters/" + String(inv) + "/enabled", ((cfgGroupInv->enabled) ? dict[STR_TRUE] : dict[STR_FALSE]));
                PubSubInit(gr + "/inverters/" + String(inv) + "/powerMin", String(cfgGroupInv->powerMin));
                PubSubInit(gr + "/inverters/" + String(inv) + "/powerMax", String(cfgGroupInv->powerMax));
            }

            // Battery
            PubSubInit(gr + "/battery/switch", ((cfgGroup->battSwitch) ? dict[STR_TRUE] : dict[STR_FALSE]));

            // Advanced
            PubSubInit(gr + "/advanced/setPoint", String(cfgGroup->setPoint));
            PubSubInit(gr + "/advanced/powerTolerance", String(cfgGroup->powerTolerance));
            PubSubInit(gr + "/advanced/powerMax", String(cfgGroup->powerMax));
        }
    }
    /** groupPublish
     *
     */
    bool groupPublish(uint8_t group, unsigned long *tsp, bool *doLog) {
        zeroExportGroup_t *cfgGroup = &mCfg->groups[group];

        if (mCfg->debug) mLog["t"] = "groupPublish";

        cfgGroup->lastRun = *tsp;

        if (mMqtt->isConnected()) {

            //            *doLog = true;
            String gr;

            // Global (zeroExport)
            // TODO: Global wird fälschlicherweise hier je nach anzahl der aktivierten Gruppen bis zu 6x ausgeführt.
            mMqtt->publish("zero/state/enabled", ((mCfg->enabled) ? dict[STR_TRUE] : dict[STR_FALSE]), false);
            // TODO: Global wird fälschlicherweise hier je nach anzahl der aktivierten Gruppen bis zu 6x ausgeführt.
            mMqtt->publish("zero/state/sleep", ((mCfg->sleep) ? dict[STR_TRUE] : dict[STR_FALSE]), false);

            // General
            gr = "zero/state/groups/" + String(group) + "/enabled";
            mMqtt->publish(gr.c_str(), ((cfgGroup->enabled) ? dict[STR_TRUE] : dict[STR_FALSE]), false);

            gr = "zero/state/groups/" + String(group) + "/sleep";
            mMqtt->publish(gr.c_str(), ((cfgGroup->sleep) ? dict[STR_TRUE] : dict[STR_FALSE]), false);

            gr = "zero/state/groups/" + String(group) + "/name";
            mMqtt->publish(gr.c_str(), cfgGroup->name, false);

            // Inverters
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                zeroExportGroupInverter_t *cfgGroupInv = &cfgGroup->inverters[inv];
                gr = "zero/state/groups/" + String(group) + "/inverters/" + String(inv);
                mqttObj["enabled"] = cfgGroupInv->enabled;
                mqttObj["id"] = cfgGroupInv->id;
                mqttObj["target"] = cfgGroupInv->target;
                mqttObj["powerMin"] = cfgGroupInv->powerMin;
                mqttObj["powerMax"] = cfgGroupInv->powerMax;
                mMqtt->publish(gr.c_str(), mqttDoc.as<std::string>().c_str(), false);
                mqttDoc.clear();
            }

            // Battery
            gr = "zero/state/groups/" + String(group) + "/battery";
            mqttObj["enabled"] = cfgGroup->battEnabled;
            mqttObj["voltageOn"] = cfgGroup->battVoltageOn;
            mqttObj["voltageOff"] = cfgGroup->battVoltageOff;
            mqttObj["switch"] = cfgGroup->battSwitch;
            mMqtt->publish(gr.c_str(), mqttDoc.as<std::string>().c_str(), false);
            mqttDoc.clear();

            // Advanced
            gr = "zero/state/groups/" + String(group) + "/advanced";
            mqttObj["setPoint"] = cfgGroup->setPoint;
            mqttObj["refresh"] = cfgGroup->refresh;
            mqttObj["powerTolerance"] = cfgGroup->powerTolerance;
            mqttObj["powerMax"] = cfgGroup->powerMax;
            mqttObj["Kp"] = cfgGroup->Kp;
            mqttObj["Ki"] = cfgGroup->Ki;
            mqttObj["Kd"] = cfgGroup->Kd;
            mMqtt->publish(gr.c_str(), mqttDoc.as<std::string>().c_str(), false);
            mqttDoc.clear();
        }

        return true;
    }

    /** groupEmergency
     *
     * @param group
     * @returns true/false
     * @todo Hier ist noch keine Funktion
     */
    bool groupEmergency(uint8_t group, unsigned long *tsp, bool *doLog) {
        zeroExportGroup_t *cfgGroup = &mCfg->groups[group];
        if (mCfg->debug) mLog["t"] = "groupEmergency";

        cfgGroup->lastRun = *tsp;

        //        *doLog = true;

        //      if (!korrect) {
        //          do
        //          return;
        //      }

        return true;
    }

    /** sendLog
     * Sendet den LogSpeicher über Webserial und/oder MQTT
     */
    void sendLog(void) {
        if (mCfg->log_over_webserial) {
            //            DBGPRINTLN(String("ze: ") + mDocLog.as<String>());
            DPRINTLN(DBG_INFO, String("ze: ") + mDocLog.as<String>());
        }

        if (mCfg->log_over_mqtt) {
            if (mMqtt->isConnected()) {
                mMqtt->publish("ze", mDocLog.as<std::string>().c_str(), false);
            }
        }
    }

    /** clearLog
     * Löscht den LogSpeicher
     */
    void clearLog(void) {
        mDocLog.clear();
    }

    // private member variables
    bool mIsInitialized = false;
    zeroExport_t *mCfg;
    settings_t *mConfig;
    HMSYSTEM *mSys;
    RestApiType *mApi;
    StaticJsonDocument<5000> mDocLog;
    JsonObject mLog = mDocLog.to<JsonObject>();
    PubMqttType *mMqtt;
    powermeter mPowermeter;
    bool mIsSubscribed = false;

    StaticJsonDocument<512> mqttDoc;    //DynamicJsonDocument mqttDoc(512);
    JsonObject mqttObj = mqttDoc.to<JsonObject>();

    Inverter<> *mIv[ZEROEXPORT_MAX_GROUPS][ZEROEXPORT_GROUP_MAX_INVERTERS];
};

#endif /*__ZEROEXPORT__*/

#endif /* #if defined(PLUGIN_ZEROEXPORT) */
