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

// TODO: Anbindung an MQTT für Logausgabe.
// TODO: Powermeter erweitern
// TODO: Der Teil der noch in app.cpp steckt komplett hier in die Funktion verschieben.

class ZeroExport {
   public:
    /** ZeroExport
     * Konstruktor
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
     * Destruktor
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

        mIsInitialized = mPowermeter.setup(mCfg);
    }

    /** loop
     * Arbeitsschleife
     */
    void loop(void) {
        if ((!mIsInitialized) || (!mCfg->enabled)) {
            return;
        }

        mPowermeter.loop();

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            if (!mCfg->groups[group].enabled) {
                continue;
            }

            switch (mCfg->groups[group].state) {
                case zeroExportState::INIT:
                    if (groupInit(group)) sendLog();
                    break;
                case zeroExportState::WAIT:
                    if (groupWait(group)) sendLog();
                    break;
                case zeroExportState::WAITREFRESH:
                    if (groupWaitRefresh(group)) sendLog();
                    break;
                case zeroExportState::GETINVERTERACKS:
                    if (groupGetInverterAcks(group)) sendLog();
                    break;
                case zeroExportState::GETINVERTERDATA:
                    if (groupGetInverterData(group)) sendLog();
                    break;
                case zeroExportState::BATTERYPROTECTION:
                    if (groupBatteryprotection(group)) sendLog();
                    break;
                case zeroExportState::GETPOWERMETER:
                    if (groupGetPowermeter(group)) sendLog();
                    break;
                case zeroExportState::CONTROLLER:
                    if (groupController(group)) sendLog();
                    break;
                case zeroExportState::PROGNOSE:
                    if (groupPrognose(group)) sendLog();
                    break;
                case zeroExportState::AUFTEILEN:
                    if (groupAufteilen(group)) sendLog();
                    break;
                case zeroExportState::SETLIMIT:
                    if (groupSetLimit(group)) sendLog();
                    break;
                case zeroExportState::SETPOWER:
                    if (groupSetPower(group)) sendLog();
                    break;
                case zeroExportState::SETREBOOT:
                    if (groupSetReboot(group)) sendLog();
                    break;
                case zeroExportState::FINISH:
                case zeroExportState::ERROR:
                default:
                    mCfg->groups[group].state = zeroExportState::INIT;
                    mCfg->groups[group].stateNext = zeroExportState::INIT;
                    if (millis() > (mCfg->groups[group].lastRefresh + (mCfg->groups[group].refresh * 1000UL))) {
                        mCfg->groups[group].lastRefresh = millis();
                    }
                    break;
            }
        }
    }

    /** tickerSecond
     * Zeitimpuls
     */
    void tickerSecond() {
        // TODO: Warten ob benötigt, ggf ein bit setzen, das in der loop() abgearbeitet wird.
        if ((!mIsInitialized) || (!mCfg->enabled)) {
            return;
        }

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

    /** resetWaitLimitAck
     *
     * @param
     */
    void resetWaitLimitAck(Inverter<> *iv) {
        if ((!mIsInitialized) || (!mCfg->enabled)) {
            return;
        }

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                if (iv->id == (uint8_t)mCfg->groups[group].inverters[inv].id) {
                    unsigned long bTsp = millis();

                    mLog["t"] = "resetWaitLimitAck";
                    mLog["g"] = group;
                    mLog["i"] = inv;
                    mLog["id"] = iv->id;
                    mCfg->groups[group].inverters[inv].waitLimitAck = 0;
                    mLog["w"] = 0;

                    unsigned long eTsp = millis();
                    mLog["B"] = bTsp;
                    mLog["E"] = eTsp;
                    mLog["D"] = eTsp - bTsp;
                    sendLog();
                    return;
                }
            }
        }
    }

    /** setPowerAck
     *
     * @param
     */
    void resetWaitPowerAck(Inverter<> *iv) {
        if ((!mIsInitialized) || (!mCfg->enabled)) {
            return;
        }

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                if (iv->id == mCfg->groups[group].inverters[inv].id) {
                    unsigned long bTsp = millis();

                    mLog["t"] = "resetWaitPowerAck";
                    mLog["g"] = group;
                    mLog["i"] = inv;
                    mLog["id"] = iv->id;
                    mCfg->groups[group].inverters[inv].waitPowerAck = 30;
                    mLog["w"] = 30;

                    unsigned long eTsp = millis();
                    mLog["B"] = bTsp;
                    mLog["E"] = eTsp;
                    mLog["D"] = eTsp - bTsp;
                    sendLog();
                    return;
                }
            }
        }
    }

    /** resetWaitRebootAck
     *
     * @param
     */
    void resetWaitRebootAck(Inverter<> *iv) {
        if ((!mIsInitialized) || (!mCfg->enabled)) {
            return;
        }

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                if (iv->id == mCfg->groups[group].inverters[inv].id) {
                    unsigned long bTsp = millis();

                    mLog["t"] = "resetWaitRebootAck";
                    mLog["g"] = group;
                    mLog["i"] = inv;
                    mLog["id"] = iv->id;
                    mCfg->groups[group].inverters[inv].waitRebootAck = 30;
                    mLog["w"] = 30;

                    unsigned long eTsp = millis();
                    mLog["B"] = bTsp;
                    mLog["E"] = eTsp;
                    mLog["D"] = eTsp - bTsp;
                    sendLog();
                    return;
                }
            }
        }
    }

    /** newDataAvailable
     *
     */
    void newDataAvailable(Inverter<> *iv) {
        if ((!mIsInitialized) || (!mCfg->enabled)) {
            return;
        }

        //        if (!iv->isAvailable()) {
        //            return;
        //        }

        if (iv->actPowerLimit == 65535) {
            return;
        }

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                if (iv->id == mCfg->groups[group].inverters[inv].id) {
                    unsigned long bTsp = millis();

                    mLog["t"] = "newDataAvailable";
                    mLog["g"] = group;
                    mLog["i"] = inv;
                    mLog["id"] = iv->id;
                    mLog["a"] = iv->isAvailable();
                    mLog["ivL%"] = iv->actPowerLimit;
                    mLog["ivPm"] = iv->getMaxPower();
                    mLog["ivL"] = (uint16_t)(iv->getMaxPower() / 100 * iv->actPowerLimit);
                    mLog["zeL"] = (uint16_t)mCfg->groups[group].inverters[inv].limit;
                    mCfg->groups[group].inverters[inv].limit = (iv->getMaxPower() / 100 * iv->actPowerLimit);
                    unsigned long eTsp = millis();
                    mLog["B"] = bTsp;
                    mLog["E"] = eTsp;
                    mLog["D"] = eTsp - bTsp;
                    sendLog();
                    return;
                }
            }
        }
    }

   private:
    /** groupInit
     * initialisiert die Gruppe und sucht die ivPointer
     * @param group
     * @returns true/false
     * @todo getInverterById statt getInverterByPos, dann würde die Variable *iv und die Schleife nicht gebraucht.
     */
    bool groupInit(uint8_t group) {
        bool doLog = false;
        unsigned long bTsp = millis();

        mLog["t"] = "groupInit";
        mLog["g"] = group;

        mCfg->groups[group].stateLast = zeroExportState::INIT;

        // Init ivPointer
        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
            mIv[group][inv] = nullptr;
            doLog = true;
        }

        // Search/Set ivPointer
        JsonArray logArr = mLog.createNestedArray("ix");
        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
            JsonObject logObj = logArr.createNestedObject();
            logObj["i"] = inv;

            mIv[group][inv] = nullptr;

            // Inverter not enabled -> ignore
            if (!mCfg->groups[group].inverters[inv].enabled) {
                continue;
            }

            // Inverter not selected -> ignore
            if (mCfg->groups[group].inverters[inv].id <= 0) {
                continue;
            }

            // Load Config
            Inverter<> *iv;
            for (uint8_t i = 0; i < mSys->getNumInverters(); i++) {
                iv = mSys->getInverterByPos(i);

                // Inverter not configured -> ignore
                if (iv == NULL) {
                    continue;
                }

                // Inverter not matching -> ignore
                if (iv->id != (uint8_t)mCfg->groups[group].inverters[inv].id) {
                    continue;
                }

                // Save Inverter
                logObj["pos"] = i;
                logObj["id"] = iv->id;
                mIv[group][inv] = mSys->getInverterByPos(i);

                doLog = true;
            }
        }

        // Init Acks
        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
            mCfg->groups[group].inverters[inv].waitLimitAck = 0;
            mCfg->groups[group].inverters[inv].waitPowerAck = 0;
            mCfg->groups[group].inverters[inv].waitRebootAck = 0;
        }

        // Next
        mCfg->groups[group].state = zeroExportState::WAIT;
        mCfg->groups[group].stateNext = zeroExportState::WAIT;

        unsigned long eTsp = millis();
        mLog["B"] = bTsp;
        mLog["E"] = eTsp;
        mLog["D"] = eTsp - bTsp;

        mCfg->groups[group].lastRun = eTsp;
        mCfg->groups[group].lastRefresh = eTsp;

        return doLog;
    }

    /** groupWait
     * pausiert die Gruppe
     * @param group
     * @returns true/false
     */
    bool groupWait(uint8_t group) {
        bool doLog = false;
        unsigned long bTsp = millis();

        mLog["t"] = "groupWait";
        mLog["g"] = group;

        // Wait 60s
        if (bTsp <= (mCfg->groups[group].lastRun + 60000UL)) {
            return doLog;
        }

        mCfg->groups[group].stateLast = zeroExportState::WAIT;

        doLog = true;

        // Next
        if (mCfg->groups[group].state != mCfg->groups[group].stateNext) {
            mCfg->groups[group].state = mCfg->groups[group].stateNext;
        } else {
            mCfg->groups[group].state = zeroExportState::WAITREFRESH;
            mCfg->groups[group].stateNext = zeroExportState::WAITREFRESH;
        }

        unsigned long eTsp = millis();
        mLog["B"] = bTsp;
        mLog["E"] = eTsp;
        mLog["D"] = eTsp - bTsp;

        mCfg->groups[group].lastRun = eTsp;
        mCfg->groups[group].lastRefresh = eTsp;

        return doLog;
    }

    /** groupWaitRefresh
     * pausiert die Gruppe
     * @param group
     * @returns true/false
     */
    bool groupWaitRefresh(uint8_t group) {
        bool doLog = false;
        unsigned long bTsp = millis();

        mLog["t"] = "groupWaitRefresh";
        mLog["g"] = group;

        // Wait Refreshtime
        if (bTsp <= (mCfg->groups[group].lastRefresh + (mCfg->groups[group].refresh * 1000UL))) {
            return doLog;
        }

        mCfg->groups[group].stateLast = zeroExportState::WAITREFRESH;

        doLog = true;

        // Next
#if defined(ZEROEXPORT_DEV_POWERMETER)
        mCfg->groups[group].state = zeroExportState::GETPOWERMETER;
        mCfg->groups[group].stateNext = zeroExportState::GETPOWERMETER;
#else
        mCfg->groups[group].state = zeroExportState::GETINVERTERACKS;
        mCfg->groups[group].stateNext = zeroExportState::GETINVERTERACKS;
#endif

        unsigned long eTsp = millis();
        mLog["B"] = bTsp;
        mLog["E"] = eTsp;
        mLog["D"] = eTsp - bTsp;

        mCfg->groups[group].lastRun = eTsp;

        return doLog;
    }

    /** groupGetInverterAcks
     * aktualisiert die Gruppe mit den ACKs der Inverter für neue Limits
     * @param group
     * @returns true/false
     */
    bool groupGetInverterAcks(uint8_t group) {
        bool doLog = false;
        unsigned long bTsp = millis();

        mLog["t"] = "groupGetInverterAcks";
        mLog["g"] = group;

        // Wait 250ms
        if (mCfg->groups[group].stateLast == zeroExportState::GETINVERTERACKS) {
            if (bTsp <= (mCfg->groups[group].lastRun + 250UL)) {
                return doLog;
            }
        }

        mCfg->groups[group].stateLast = zeroExportState::GETINVERTERACKS;

        doLog = true;

        // Wait Acks
        JsonArray logArr = mLog.createNestedArray("ix");
        bool wait = false;
        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
            JsonObject logObj = logArr.createNestedObject();
            logObj["i"] = inv;

            // Inverter not enabled -> ignore
            if (!mCfg->groups[group].inverters[inv].enabled) {
                continue;
            }
            // Inverter not selected -> ignore
            if (mCfg->groups[group].inverters[inv].id <= 0) {
                continue;
            }
            // Inverter is not available -> wait
            if (!mIv[group][inv]->isAvailable()) {
                logObj["a"] = false;
                wait = true;
            }
            // waitLimitAck
            if (mCfg->groups[group].inverters[inv].waitLimitAck > 0) {
                logObj["wL"] = mCfg->groups[group].inverters[inv].waitLimitAck;
                wait = true;
            }
            // waitPowerAck
            if (mCfg->groups[group].inverters[inv].waitPowerAck > 0) {
                logObj["wP"] = mCfg->groups[group].inverters[inv].waitPowerAck;
                wait = true;
            }
            // waitRebootAck
            if (mCfg->groups[group].inverters[inv].waitRebootAck > 0) {
                logObj["wR"] = mCfg->groups[group].inverters[inv].waitRebootAck;
                wait = true;
            }
        }

        mLog["w"] = wait;

        // Next
        if (!wait) {
            mCfg->groups[group].state = zeroExportState::GETINVERTERDATA;
            mCfg->groups[group].stateNext = zeroExportState::GETINVERTERDATA;
        }

        unsigned long eTsp = millis();
        mLog["B"] = bTsp;
        mLog["E"] = eTsp;
        mLog["D"] = eTsp - bTsp;

        mCfg->groups[group].lastRun = eTsp;

        return doLog;
    }

    /** groupGetInverterData
     *
     * @param group
     * @returns true/false
     */
    bool groupGetInverterData(uint8_t group) {
        bool doLog = false;
        unsigned long bTsp = millis();

        mLog["t"] = "groupGetInverterData";
        mLog["g"] = group;

        mCfg->groups[group].stateLast = zeroExportState::GETINVERTERDATA;

        doLog = true;

        // Get Data
        JsonArray logArr = mLog.createNestedArray("ix");
        bool wait = false;
        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
            JsonObject logObj = logArr.createNestedObject();
            logObj["i"] = inv;

            // Inverter not enabled -> ignore
            if (!mCfg->groups[group].inverters[inv].enabled) {
                continue;
            }
            // Inverter not selected -> ignore
            if (mCfg->groups[group].inverters[inv].id <= 0) {
                continue;
            }

            if (!mIv[group][inv]->isAvailable()) {
                continue;
            }

            // Get Pac
            record_t<> *rec;
            rec = mIv[group][inv]->getRecordStruct(RealTimeRunData_Debug);
            float p = mIv[group][inv]->getChannelFieldValue(CH0, FLD_PAC, rec);

            // TODO: Save Hole Power für die Webanzeige

            // Get Limit
            //                uint16_t Limit = mIv[group][inv]->actPowerLimit;
        }

        // Next
        mCfg->groups[group].state = zeroExportState::BATTERYPROTECTION;
        mCfg->groups[group].stateNext = zeroExportState::BATTERYPROTECTION;

        unsigned long eTsp = millis();
        mLog["B"] = bTsp;
        mLog["E"] = eTsp;
        mLog["D"] = eTsp - bTsp;

        mCfg->groups[group].lastRun = eTsp;

        return doLog;
    }

    /** groupBatteryprotection
     * Batterieschutzfunktion
     * @param group
     * @returns true/false
     */
    bool groupBatteryprotection(uint8_t group) {
        bool doLog = false;
        unsigned long bTsp = millis();

        mLog["t"] = "groupBatteryprotection";
        mLog["g"] = group;

        mCfg->groups[group].stateLast = zeroExportState::BATTERYPROTECTION;
        mCfg->groups[group].state = zeroExportState::GETPOWERMETER;
        mCfg->groups[group].stateNext = zeroExportState::GETPOWERMETER;

        doLog = true;

        // Protection
        if (mCfg->groups[group].battEnabled) {
            mLog["en"] = true;

            // Config - parameter check
            if (mCfg->groups[group].battVoltageOn <= mCfg->groups[group].battVoltageOff) {
                mCfg->groups[group].battSwitch = false;
                mLog["err"] = "Config - battVoltageOn(" + (String)mCfg->groups[group].battVoltageOn + ") <= battVoltageOff(" + (String)mCfg->groups[group].battVoltageOff + ")";
                return doLog;
            }

            // Config - parameter check
            if (mCfg->groups[group].battVoltageOn <= (mCfg->groups[group].battVoltageOff + 1)) {
                mCfg->groups[group].battSwitch = false;
                mLog["err"] = "Config - battVoltageOn(" + (String)mCfg->groups[group].battVoltageOn + ") <= battVoltageOff(" + (String)mCfg->groups[group].battVoltageOff + " + 1V)";
                return doLog;
            }

            // Config - parameter check
            if (mCfg->groups[group].battVoltageOn <= 22) {
                mCfg->groups[group].battSwitch = false;
                mLog["err"] = "Config - battVoltageOn(" + (String)mCfg->groups[group].battVoltageOn + ") <= 22V)";
                return doLog;
            }

            int8_t id = 0;
            float U = 0;

            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                zeroExportGroupInverter_t *cfgGroupInv = &mCfg->groups[group].inverters[inv];

                // Ignore disabled Inverter
                if (!cfgGroupInv->enabled) {
                    continue;
                }
                if (cfgGroupInv->id <= 0) {
                    continue;
                }

                if (!mIv[group][inv]->isAvailable()) {
                    if (U > 0) {
                        continue;
                    }
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

                    // Inverter not enabled -> ignore
                    if (!mCfg->groups[group].inverters[inv].enabled) {
                        continue;
                    }
                    // Inverter not selected -> ignore
                    if (mCfg->groups[group].inverters[inv].id <= 0) {
                        continue;
                    }

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
                    mCfg->groups[group].stateNext = zeroExportState::SETPOWER;
                }
            }

            // Switch to OFF
            if (U < mCfg->groups[group].battVoltageOff) {
                mCfg->groups[group].battSwitch = false;
                mLog["act"] = "Off";

                for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                    //                        zeroExportGroupInverter_t *cfgGroupInv = &mCfg->groups[group].inverters[inv];

                    // Inverter not enabled -> ignore
                    if (!mCfg->groups[group].inverters[inv].enabled) {
                        continue;
                    }
                    // Inverter not selected -> ignore
                    if (mCfg->groups[group].inverters[inv].id <= 0) {
                        continue;
                    }

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
                    mCfg->groups[group].stateNext = zeroExportState::SETPOWER;
                }
            }
        } else {
            mLog["en"] = false;

            mCfg->groups[group].battSwitch = true;
        }

        mLog["sw"] = mCfg->groups[group].battSwitch;

        unsigned long eTsp = millis();
        mLog["B"] = bTsp;
        mLog["E"] = eTsp;
        mLog["D"] = eTsp - bTsp;

        mCfg->groups[group].lastRun = eTsp;

        return doLog;
    }

    /** groupGetPowermeter
     * Holt die Daten vom Powermeter
     * @param group
     * @returns true/false
     */
    bool groupGetPowermeter(uint8_t group) {
        bool doLog = false;
        unsigned long bTsp = millis();

        mLog["t"] = "groupGetPowermeter";
        mLog["g"] = group;

        mCfg->groups[group].stateLast = zeroExportState::GETPOWERMETER;

        doLog = true;

        bool result = false;
        result = mPowermeter.getData(mLog, group);
        if (
            (mCfg->groups[group].pmPower == 0) &&
            (mCfg->groups[group].pmPower == 0) &&
            (mCfg->groups[group].pmPower == 0) &&
            (mCfg->groups[group].pmPower == 0)) {
            return doLog;
        }

        // TODO: eventuell muss hier geprüft werden ob die Daten vom Powermeter plausibel sind.

        // Next
#if defined(ZEROEXPORT_DEV_POWERMETER)
        mCfg->groups[group].state = zeroExportState::WAITREFRESH;
        mCfg->groups[group].stateNext = zeroExportState::WAITREFRESH;
        mCfg->groups[group].lastRefresh = millis();
#else
        if (result) {
            mCfg->groups[group].state = zeroExportState::CONTROLLER;
            mCfg->groups[group].stateNext = zeroExportState::CONTROLLER;
        }
#endif

        unsigned long eTsp = millis();
        mLog["B"] = bTsp;
        mLog["E"] = eTsp;
        mLog["D"] = eTsp - bTsp;

        mCfg->groups[group].lastRun = eTsp;

        return doLog;
    }

    /** groupController
     * PID-Regler
     * @param group
     * @returns true/false
     */
    bool groupController(uint8_t group) {
        bool doLog = false;
        unsigned long bTsp = millis();

        mLog["t"] = "groupController";
        mLog["g"] = group;

        mCfg->groups[group].stateLast = zeroExportState::GETPOWERMETER;

        doLog = true;

        // Führungsgröße w in Watt
        float w = mCfg->groups[group].setPoint;

        mLog["w"] = w;

        // Regelgröße x in Watt
        float x = mCfg->groups[group].pmPower;
        float x1 = mCfg->groups[group].pmPowerL1;
        float x2 = mCfg->groups[group].pmPowerL2;
        float x3 = mCfg->groups[group].pmPowerL3;

        mLog["x"] = x;
        mLog["x1"] = x1;
        mLog["x2"] = x2;
        mLog["x3"] = x3;

        // Regelabweichung e in Watt
        float e = w - x;
        float e1 = w - x1;
        float e2 = w - x2;
        float e3 = w - x3;

        mLog["e"] = e;
        mLog["e1"] = e1;
        mLog["e2"] = e2;
        mLog["e3"] = e3;

        if (
            (e < mCfg->groups[group].powerTolerance) && (e > -mCfg->groups[group].powerTolerance) &&
            (e1 < mCfg->groups[group].powerTolerance) && (e1 > -mCfg->groups[group].powerTolerance) &&
            (e2 < mCfg->groups[group].powerTolerance) && (e2 > -mCfg->groups[group].powerTolerance) &&
            (e3 < mCfg->groups[group].powerTolerance) && (e3 > -mCfg->groups[group].powerTolerance)) {
            mCfg->groups[group].state = zeroExportState::WAITREFRESH;
            mCfg->groups[group].stateNext = zeroExportState::WAITREFRESH;
            mCfg->groups[group].lastRun = bTsp;
            mLog["tol"] = mCfg->groups[group].powerTolerance;
            return doLog;
        }

        // Regler
        // TODO: Regelparameter unter Advanced konfigurierbar? Aber erst wenn Regler komplett ingegriert.
        float Kp = mCfg->groups[group].Kp;
        float Ki = mCfg->groups[group].Ki;
        float Kd = mCfg->groups[group].Kd;
        unsigned long Ta = bTsp - mCfg->groups[group].lastRefresh;
        mLog["Kp"] = Kp;
        mLog["Ki"] = Ki;
        mLog["Kd"] = Kd;
        mLog["Ta"] = Ta;
        // - P-Anteil
        float yP = Kp * e;
        float yP1 = Kp * e1;
        float yP2 = Kp * e2;
        float yP3 = Kp * e3;
        mLog["yP"] = yP;
        mLog["yP1"] = yP1;
        mLog["yP2"] = yP2;
        mLog["yP3"] = yP3;
        // - I-Anteil
        mCfg->groups[group].eSum += e;
        mCfg->groups[group].eSum1 += e1;
        mCfg->groups[group].eSum2 += e2;
        mCfg->groups[group].eSum3 += e3;
        mLog["esum"] = mCfg->groups[group].eSum;
        mLog["esum1"] = mCfg->groups[group].eSum1;
        mLog["esum2"] = mCfg->groups[group].eSum2;
        mLog["esum3"] = mCfg->groups[group].eSum3;
        float yI = Ki * Ta * mCfg->groups[group].eSum;
        float yI1 = Ki * Ta * mCfg->groups[group].eSum1;
        float yI2 = Ki * Ta * mCfg->groups[group].eSum2;
        float yI3 = Ki * Ta * mCfg->groups[group].eSum3;
        mLog["yI"] = yI;
        mLog["yI1"] = yI1;
        mLog["yI2"] = yI2;
        mLog["yI3"] = yI3;
        // - D-Anteil
        mLog["ealt"] = mCfg->groups[group].eOld;
        mLog["ealt1"] = mCfg->groups[group].eOld1;
        mLog["ealt2"] = mCfg->groups[group].eOld2;
        mLog["ealt3"] = mCfg->groups[group].eOld3;
        float yD = Kd * (e - mCfg->groups[group].eOld) / Ta;
        float yD1 = Kd * (e1 - mCfg->groups[group].eOld1) / Ta;
        float yD2 = Kd * (e2 - mCfg->groups[group].eOld2) / Ta;
        float yD3 = Kd * (e3 - mCfg->groups[group].eOld3) / Ta;
        mLog["yD"] = yD;
        mLog["yD1"] = yD1;
        mLog["yD2"] = yD2;
        mLog["yD3"] = yD3;
        mCfg->groups[group].eOld = e;
        mCfg->groups[group].eOld1 = e1;
        mCfg->groups[group].eOld2 = e2;
        mCfg->groups[group].eOld3 = e3;
        // - PID-Anteil
        float y = yP + yI + yD;
        float y1 = yP1 + yI1 + yD1;
        float y2 = yP2 + yI2 + yD2;
        float y3 = yP3 + yI3 + yD3;

        // Regelbegrenzung
        // TODO: Hier könnte man den maximalen Sprung begrenzen

        mLog["y"] = y;
        mLog["y1"] = y1;
        mLog["y2"] = y2;
        mLog["y3"] = y3;

        mCfg->groups[group].grpPower = y;
        mCfg->groups[group].grpPowerL1 = y1;
        mCfg->groups[group].grpPowerL2 = y2;
        mCfg->groups[group].grpPowerL3 = y3;

        // Next
        mCfg->groups[group].state = zeroExportState::PROGNOSE;
        mCfg->groups[group].stateNext = zeroExportState::PROGNOSE;

        unsigned long eTsp = millis();
        mLog["B"] = bTsp;
        mLog["E"] = eTsp;
        mLog["D"] = eTsp - bTsp;

        mCfg->groups[group].lastRun = eTsp;
        mCfg->groups[group].lastRefresh = eTsp;

        return doLog;
    }

    /** groupPrognose
     *
     * @param group
     * @returns true/false
     */
    bool groupPrognose(uint8_t group) {
        bool result = false;
        unsigned long bTsp = millis();

        mLog["t"] = "groupPrognose";
        mLog["g"] = group;

        mCfg->groups[group].stateLast = zeroExportState::PROGNOSE;

        result = true;

        // Next
        mCfg->groups[group].state = zeroExportState::AUFTEILEN;
        mCfg->groups[group].stateNext = zeroExportState::AUFTEILEN;

        unsigned long eTsp = millis();
        mLog["B"] = bTsp;
        mLog["E"] = eTsp;
        mLog["D"] = eTsp - bTsp;

        mCfg->groups[group].lastRun = eTsp;

        return result;
    }

    /** groupAufteilen
     *
     * @param group
     * @returns true/false
     */
    bool groupAufteilen(uint8_t group) {
        bool result = false;
        unsigned long bTsp = millis();

        mLog["t"] = "groupAufteilen";
        mLog["g"] = group;

        mCfg->groups[group].stateLast = zeroExportState::AUFTEILEN;

        result = true;

        float y = mCfg->groups[group].grpPower;
        float y1 = mCfg->groups[group].grpPowerL1;
        float y2 = mCfg->groups[group].grpPowerL2;
        float y3 = mCfg->groups[group].grpPowerL3;

        mLog["y"] = y;
        mLog["y1"] = y1;
        mLog["y2"] = y2;
        mLog["y3"] = y3;

        bool grpTarget[7] = {false, false, false, false, false, false, false};
        uint8_t ivId_Pmin[7] = {0, 0, 0, 0, 0, 0, 0};
        uint8_t ivId_Pmax[7] = {0, 0, 0, 0, 0, 0, 0};
        uint16_t ivPmin[7] = {65535, 65535, 65535, 65535, 65535, 65535, 65535};
        uint16_t ivPmax[7] = {0, 0, 0, 0, 0, 0, 0};

        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
            zeroExportGroupInverter_t *cfgGroupInv = &mCfg->groups[group].inverters[inv];

            if (!cfgGroupInv->enabled) {
                continue;
            }

            //                if ((cfgGroup->battSwitch) && (!cfgGroupInv->state)) {
            //                    setPower(&logObj, group, inv, true);
            //                    continue;
            //                }

            //                if ((!cfgGroup->battSwitch) && (cfgGroupInv->state)) {
            //                    setPower(&logObj, group, inv, false);
            //                    continue;
            //                }

            //                if (!cfgGroupInv->state) {
            //                    continue;
            //                }
            record_t<> *rec;
            rec = mIv[group][inv]->getRecordStruct(RealTimeRunData_Debug);
            cfgGroupInv->power = mIv[group][inv]->getChannelFieldValue(CH0, FLD_PAC, rec);

            if ((uint16_t)cfgGroupInv->power < ivPmin[cfgGroupInv->target]) {
                grpTarget[cfgGroupInv->target] = true;
                ivPmin[cfgGroupInv->target] = (uint16_t)cfgGroupInv->power;
                ivId_Pmin[cfgGroupInv->target] = inv;
                // Hier kein return oder continue sonst dauerreboot
            }
            if ((uint16_t)cfgGroupInv->power > ivPmax[cfgGroupInv->target]) {
                grpTarget[cfgGroupInv->target] = true;
                ivPmax[cfgGroupInv->target] = (uint16_t)cfgGroupInv->power;
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

            float *deltaP;
            switch (i) {
                case 6:
                case 3:
                    deltaP = &mCfg->groups[group].grpPowerL3;
                    break;
                case 5:
                case 2:
                    deltaP = &mCfg->groups[group].grpPowerL2;
                    break;
                case 4:
                case 1:
                    deltaP = &mCfg->groups[group].grpPowerL1;
                    break;
                case 0:
                    deltaP = &mCfg->groups[group].grpPower;
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

        // Next
        mCfg->groups[group].state = zeroExportState::SETLIMIT;
        mCfg->groups[group].stateNext = zeroExportState::SETLIMIT;

        unsigned long eTsp = millis();
        mLog["B"] = bTsp;
        mLog["E"] = eTsp;
        mLog["D"] = eTsp - bTsp;

        mCfg->groups[group].lastRun = eTsp;

        return result;
    }

    /** groupSetLimit
     *
     * @param group
     * @returns true/false
     */
    bool groupSetLimit(uint8_t group) {
        bool doLog = false;
        unsigned long bTsp = millis();

        mLog["t"] = "groupSetLimit";
        mLog["g"] = group;

        mCfg->groups[group].stateLast = zeroExportState::SETLIMIT;

        // Set limit
        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
            mLog["i"] = inv;

            // Inverter not enabled -> ignore
            if (!mCfg->groups[group].inverters[inv].enabled) {
                continue;
            }
            // Inverter not selected -> ignore
            if (mCfg->groups[group].inverters[inv].id <= 0) {
                continue;
            }
            // Nothing todo
            if (mCfg->groups[group].inverters[inv].limit == mCfg->groups[group].inverters[inv].limitNew) {
                continue;
            }

            // Abbruch weil Inverter nicht verfügbar
            if (!mIv[group][inv]->isAvailable()) {
                continue;
            }

            // Abbruch weil Inverter produziert nicht
            if (!mIv[group][inv]->isProducing()) {
                continue;
            }

            // do

            // Set Power on/off
            // TODO: testen
            /*
                            if (mCfg->groups[group].inverters[inv].limitNew < mCfg->groups[group].inverters[inv].powerMin) {
                                if (mIv[group][inv]->isProducing()) {
                                    mCfg->groups[group].inverters[inv].switch = false;
                                    mCfg->groups[group].stateNext = zeroExportState::SETPOWER;
                                }
                            } else {
                                if (!mIv[group][inv]->isProducing()) {
                                    mCfg->groups[group].inverters[inv].switch = true;
                                    mCfg->groups[group].stateNext = zeroExportState::SETPOWER;
                                }
                            }
            */

            // Restriction LimitNew >= Pmin
            if (mCfg->groups[group].inverters[inv].limitNew < mCfg->groups[group].inverters[inv].powerMin) {
                mCfg->groups[group].inverters[inv].limitNew = mCfg->groups[group].inverters[inv].powerMin;
            }

            // Restriction LimitNew <= Pmax
            if (mCfg->groups[group].inverters[inv].limitNew > mCfg->groups[group].inverters[inv].powerMax) {
                mCfg->groups[group].inverters[inv].limitNew = mCfg->groups[group].inverters[inv].powerMax;
            }

            // Reject limit if difference < 5 W
            //                if ((cfgGroupInv->limitNew > cfgGroupInv->limit + 5) && (cfgGroupInv->limitNew < cfgGroupInv->limit - 5)) {
            //                    objLog["err"] = "Diff < 5W";
            //                    return false;
            //                }

            // Nothing todo
            if (mCfg->groups[group].inverters[inv].limit == mCfg->groups[group].inverters[inv].limitNew) {
                continue;
            }

            doLog = true;

            mCfg->groups[group].inverters[inv].limit = mCfg->groups[group].inverters[inv].limitNew;
            mLog["zeL"] = (uint16_t)mCfg->groups[group].inverters[inv].limit;

            // wait for Ack
            mCfg->groups[group].inverters[inv].waitLimitAck = 60;
            mLog["wL"] = mCfg->groups[group].inverters[inv].waitLimitAck;

            // send Command
            DynamicJsonDocument doc(512);
            JsonObject obj = doc.to<JsonObject>();
            obj["val"] = (uint16_t)mCfg->groups[group].inverters[inv].limit;
            obj["id"] = mCfg->groups[group].inverters[inv].id;
            obj["path"] = "ctrl";
            obj["cmd"] = "limit_nonpersistent_absolute";
            mApi->ctrlRequest(obj);

            mLog["d"] = obj;
        }

        // Next
        mCfg->groups[group].state = zeroExportState::WAITREFRESH;
        mCfg->groups[group].stateNext = zeroExportState::WAITREFRESH;

        unsigned long eTsp = millis();
        mLog["B"] = bTsp;
        mLog["E"] = eTsp;
        mLog["D"] = eTsp - bTsp;

        mCfg->groups[group].lastRun = eTsp;

        return doLog;
    }

    /** groupSetPower
     *
     * @param group
     * @returns true/false
     */
    bool groupSetPower(uint8_t group) {
        bool doLog = false;
        unsigned long bTsp = millis();

        mLog["t"] = "groupSetPower";
        mLog["g"] = group;

        mCfg->groups[group].stateLast = zeroExportState::SETPOWER;

        // Set power
        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
            mLog["i"] = inv;

            // Inverter not enabled -> ignore
            if (!mCfg->groups[group].inverters[inv].enabled) {
                continue;
            }
            // Inverter not selected -> ignore
            if (mCfg->groups[group].inverters[inv].id <= 0) {
                continue;
            }

            // do
            doLog = true;

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

            mLog["act"] = String("switch to: ") + String(mCfg->groups[group].battSwitch);

            // wait for Ack
            mCfg->groups[group].inverters[inv].waitPowerAck = 120;
            mLog["wP"] = mCfg->groups[group].inverters[inv].waitPowerAck;

            // send Command
            DynamicJsonDocument doc(512);
            JsonObject obj = doc.to<JsonObject>();
            obj["val"] = mCfg->groups[group].battSwitch;
            obj["id"] = mCfg->groups[group].inverters[inv].id;
            obj["path"] = "ctrl";
            obj["cmd"] = "power";
            mApi->ctrlRequest(obj);

            mLog["d"] = obj;
        }

        // Next
        mCfg->groups[group].state = zeroExportState::WAITREFRESH;
        mCfg->groups[group].stateNext = zeroExportState::WAITREFRESH;
        mCfg->groups[group].lastRefresh = millis();

        unsigned long eTsp = millis();
        mLog["B"] = bTsp;
        mLog["E"] = eTsp;
        mLog["D"] = eTsp - bTsp;

        mCfg->groups[group].lastRun = eTsp;

        return doLog;
    }

    /** groupSetReboot
     *
     * @param group
     * @returns true/false
     */
    bool groupSetReboot(uint8_t group) {
        bool doLog = false;
        unsigned long bTsp = millis();

        mLog["t"] = "groupSetReboot";
        mLog["g"] = group;

        mCfg->groups[group].stateLast = zeroExportState::SETREBOOT;

        // Set reboot
        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
            mLog["i"] = inv;

            // Inverter not enabled -> ignore
            if (!mCfg->groups[group].inverters[inv].enabled) {
                continue;
            }
            // Inverter not selected -> ignore
            if (mCfg->groups[group].inverters[inv].id <= 0) {
                continue;
            }

            // Abbruch weil Inverter nicht verfügbar
            //                if (!mIv[group][inv]->isAvailable()) {
            //                    continue;
            //                }

            // do
            doLog = true;

            mLog["act"] = String("reboot");

            // wait for Ack
            mCfg->groups[group].inverters[inv].waitRebootAck = 120;
            mLog["wR"] = mCfg->groups[group].inverters[inv].waitRebootAck;

            // send Command
            DynamicJsonDocument doc(512);
            JsonObject obj = doc.to<JsonObject>();
            obj["id"] = mCfg->groups[group].inverters[inv].id;
            obj["path"] = "ctrl";
            obj["cmd"] = "restart";
            mApi->ctrlRequest(obj);

            mLog["d"] = obj;
        }

        // Next
        mCfg->groups[group].state = zeroExportState::WAITREFRESH;
        mCfg->groups[group].stateNext = zeroExportState::WAITREFRESH;
        mCfg->groups[group].lastRefresh = millis();

        unsigned long eTsp = millis();
        mLog["B"] = bTsp;
        mLog["E"] = eTsp;
        mLog["D"] = eTsp - bTsp;

        mCfg->groups[group].lastRun = eTsp;

        return doLog;
    }

    /** sendLog
     * Sendet das Log über Webserial und/oder MQTT
     */
    void sendLog(void) {
        if (mCfg->log_over_webserial) {
            DBGPRINTLN(String("ze: ") + mDocLog.as<String>());
        }

        if (mCfg->log_over_mqtt) {
            if (mMqtt->isConnected()) {
                mMqtt->publish("ze", mDocLog.as<std::string>().c_str(), false);
            }
        }

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

    Inverter<> *mIv[ZEROEXPORT_MAX_GROUPS][ZEROEXPORT_GROUP_MAX_INVERTERS];
};

#endif /*__ZEROEXPORT__*/

#endif /* #if defined(PLUGIN_ZEROEXPORT) */