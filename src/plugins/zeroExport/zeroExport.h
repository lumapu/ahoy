#if defined(PLUGIN_ZEROEXPORT)

#ifndef __ZEROEXPORT__
#define __ZEROEXPORT__

#include <HTTPClient.h>
#include <string.h>
#include "AsyncJson.h"

#include "SML.h"

template <class HMSYSTEM>

// TODO: Anbindung an MQTT für Logausgabe.
// TODO: Powermeter erweitern
// TODO: Der Teil der noch in app.pp steckt komplett hier in die Funktion verschieben.

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
            mCfg     = cfg;
            mSys     = sys;
            mConfig  = config;
            mApi     = api;
            mMqtt    = mqtt;

//            mIsInitialized = true;
// TODO: Sicherheitsreturn weil noch Sicherheitsfunktionen fehlen.
            mIsInitialized = false;
        }

        /** loop
         * Arbeitsschleife
         */
        void loop(void) {
            if ((!mIsInitialized) || (!mCfg->enabled)) {
                return;
            }

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
                        mCfg->groups[group].state = zeroExportState::WAITREFRESH;
                        mCfg->groups[group].stateNext = zeroExportState::WAITREFRESH;
mCfg->groups[group].lastRefresh = millis();;
                        break;
                    case zeroExportState::ERROR:
                        mCfg->groups[group].state = zeroExportState::INIT;
                        mCfg->groups[group].stateNext = zeroExportState::INIT;
                        break;
                    default:
                        mCfg->groups[group].state = zeroExportState::INIT;
                        mCfg->groups[group].stateNext = zeroExportState::INIT;
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
        }

        /** resetWaitLimitAck
         *
         * @param
         */
        void resetWaitLimitAck(Inverter<> *iv) {
            for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
//                bool doLog = false;
                for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                    if (iv->id == (uint8_t)mCfg->groups[group].inverters[inv].id) {
                        mLog["group"] = group;
                        mLog["type"] = "resetWaitLimitAck";
                        unsigned long bTsp = millis();
                        mLog["B"] = bTsp;
                        mLog["id"] = iv->id;
                        mCfg->groups[group].inverters[inv].waitLimitAck = false;
                        mLog["iv"] = inv;
                        mLog["wait"] = false;
//                        doLog = true;
                        unsigned long eTsp = millis();
                        mLog["E"] = eTsp;
                        mLog["D"] = eTsp - bTsp;
//                        if (doLog) {
                            sendLog();
//                        }
                    }
                }
            }
        }

        /** setPowerAck
         *
         * @param
         */
        void resetWaitPowerAck(Inverter<> *iv) {
            for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
                for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                    if (iv->id == mCfg->groups[group].inverters[inv].id) {
                        mCfg->groups[group].inverters[inv].waitPowerAck = false;
                    }
                }
            }
        }

        /** resetWaitRebootAck
         *
         * @param
         */
        void resetWaitRebootAck(Inverter<> *iv) {
            for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
                for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                    if (iv->id == mCfg->groups[group].inverters[inv].id) {
                        mCfg->groups[group].inverters[inv].waitRebootAck = false;
                    }
                }
            }
        }


    private:

        /** groupInit
         * initialisiert die Gruppe und sucht die ivPointer
         * @param group
         * @returns true/false
         */
        bool groupInit(uint8_t group) {
            bool doLog = false;
            unsigned long bTsp = millis();
            mLog["group"] = group;
            mLog["type"] = "groupInit";
            mLog["B"] = bTsp;

            mCfg->groups[group].stateLast = zeroExportState::INIT;

            // Init ivPointer
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                mIv[group][inv] = nullptr;
                doLog = true;
            }

            // Search ivPointer
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                // Init ivPointer
                mIv[group][inv] = nullptr;
                // Inverter not enabled -> ignore
                if (!mCfg->groups[group].inverters[inv].enabled) {
                    continue;
                }
                // Inverter not selected -> ignore
                if (mCfg->groups[group].inverters[inv].id <= 0) {
                    continue;
                }
                //
                Inverter<> *iv;
// TODO: getInverterById, dann würde die Variable *iv und die Schleife nicht gebraucht.
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
                    mIv[group][inv] = mSys->getInverterByPos(i);
                    doLog = true;
                }
            }

            // Init Acks
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                mCfg->groups[group].inverters[inv].waitLimitAck = false;
                mCfg->groups[group].inverters[inv].waitPowerAck = false;
                mCfg->groups[group].inverters[inv].waitRebootAck = false;
            }

            // Next
            mCfg->groups[group].state = zeroExportState::WAIT;
            mCfg->groups[group].stateNext = zeroExportState::WAIT;
            mLog["next"] = "WAIT";

            unsigned long eTsp = millis();
            mLog["E"] = eTsp;
            mLog["D"] = eTsp - bTsp;
            mCfg->groups[group].lastRun = eTsp;
            return doLog;
        }

        /** groupWait
         * pausiert die Gruppe
         * @param group
         * @returns true/false
         */
        bool groupWait(uint8_t group) {
            bool result = false;
            unsigned long bTsp = millis();
            mLog["group"] = group;
            mLog["type"] = "groupWait";
            mLog["B"] = bTsp;

            // Wait 60s
            if (mCfg->groups[group].lastRun >= (bTsp - 60000UL)) {
                return result;
            }

            mCfg->groups[group].stateLast = zeroExportState::WAIT;

            result = true;

            // Next
            if (mCfg->groups[group].state != mCfg->groups[group].stateNext) {
                mCfg->groups[group].state = mCfg->groups[group].stateNext;
                mLog["next"] = "unknown";
            } else {
                mCfg->groups[group].state = zeroExportState::WAITREFRESH;
                mCfg->groups[group].stateNext = zeroExportState::WAITREFRESH;
                mLog["next"] = "WAITREFRESH";
            }

            unsigned long eTsp = millis();
            mLog["E"] = eTsp;
            mLog["D"] = eTsp - bTsp;
            mCfg->groups[group].lastRun = eTsp;
            return result;
        }

        /** groupWaitRefresh
         * pausiert die Gruppe
         * @param group
         * @returns true/false
         */
        bool groupWaitRefresh(uint8_t group) {
            bool result = false;
            unsigned long bTsp = millis();
            mLog["group"] = group;
            mLog["type"] = "groupWaitRefresh";
            mLog["B"] = bTsp;

            // Wait Refreshtime
            if (mCfg->groups[group].lastRefresh >= (bTsp - (mCfg->groups[group].refresh * 1000UL))) {
                return result;
            }

            mCfg->groups[group].stateLast = zeroExportState::WAITREFRESH;

            result = true;

            // Next
            mCfg->groups[group].state = zeroExportState::GETINVERTERACKS;
            mCfg->groups[group].stateNext = zeroExportState::GETINVERTERACKS;
            mLog["next"] = "GETINVERTERACKS";

            unsigned long eTsp = millis();
            mLog["E"] = eTsp;
            mLog["D"] = eTsp - bTsp;
//            mCfg->groups[group].lastRefresh = eTsp;
            mCfg->groups[group].lastRun = eTsp;
            return result;
        }

        /** groupGetInverterAcks
         * aktualisiert die Gruppe mit den ACKs der Inverter für neue Limits
         * @param group
         * @returns true/false
         */
        bool groupGetInverterAcks(uint8_t group) {
            bool doLog = false;
            unsigned long bTsp = millis();
            mLog["group"] = group;
            mLog["type"] = "groupGetInverterAcks";
            mLog["B"] = bTsp;

            // Wait 100ms
            if (mCfg->groups[group].stateLast == zeroExportState::GETINVERTERACKS) {
                if (mCfg->groups[group].lastRun >= (bTsp - 100UL)) {
                    return doLog;
                }
            }

            mCfg->groups[group].stateLast = zeroExportState::GETINVERTERACKS;

            doLog = true;

            // Wait Acks
            JsonArray logArr = mLog.createNestedArray("iv");
            bool wait = false;
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                JsonObject logObj = logArr.createNestedObject();
                logObj["id"] = inv;

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
                    logObj["notAvail"] = true;
                    wait = true;
                }
                // waitLimitAck
                if (mCfg->groups[group].inverters[inv].waitLimitAck) {
                    logObj["limit"] = true;
                    wait = true;
                }
                // waitPowerAck
                if (mCfg->groups[group].inverters[inv].waitPowerAck) {
                    logObj["power"] = true;
                    wait = true;
                }
                // waitRebootAck
                if (mCfg->groups[group].inverters[inv].waitRebootAck) {
                    logObj["reboot"] = true;
                    wait = true;
                }
            }

            mLog["wait"] = wait;

            // Next
            if (!wait) {
                mCfg->groups[group].state = zeroExportState::GETINVERTERDATA;
                mCfg->groups[group].stateNext = zeroExportState::GETINVERTERDATA;
            }

            unsigned long eTsp = millis();
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
            mLog["group"] = group;
            mLog["type"] = "groupGetInverterData";
            mLog["B"] = bTsp;

            mCfg->groups[group].stateLast = zeroExportState::GETINVERTERDATA;

            doLog = true;

            // Get Data
            JsonArray logArr = mLog.createNestedArray("iv");
            bool wait = false;
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                JsonObject logObj = logArr.createNestedObject();
                logObj["id"] = inv;

                // Inverter not enabled -> ignore
                if (!mCfg->groups[group].inverters[inv].enabled) {
                    continue;
                }
                // Inverter not selected -> ignore
                if (mCfg->groups[group].inverters[inv].id <= 0) {
                    continue;
                }

                // Get Pac
                record_t<> *rec;
                rec = mIv[group][inv]->getRecordStruct(RealTimeRunData_Debug);
                float p = mIv[group][inv]->getChannelFieldValue(CH0, FLD_PAC, rec);

// TODO: Save Hole Power für die Webanzeige

            }

            // Next
            mCfg->groups[group].state = zeroExportState::BATTERYPROTECTION;
            mCfg->groups[group].stateNext = zeroExportState::BATTERYPROTECTION;

            unsigned long eTsp = millis();
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
            mLog["group"] = group;
            mLog["type"] = "groupBatteryprotection";
            mLog["B"] = bTsp;

            mCfg->groups[group].stateLast = zeroExportState::BATTERYPROTECTION;

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

                uint8_t id = 0;
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

                mLog["iv"] = id;
                mLog["U"] = U;

                // Switch to ON
                if (U > mCfg->groups[group].battVoltageOn) {
                    mCfg->groups[group].battSwitch = true;
                    mLog["action"] = "On";
                }

                // Switch to OFF
                if (U < mCfg->groups[group].battVoltageOff) {
                    mCfg->groups[group].battSwitch = false;
                    mLog["action"] = "Off";
                }
            } else {
                mLog["en"] = false;

                mCfg->groups[group].battSwitch = true;
            }

            mLog["sw"] = mCfg->groups[group].battSwitch;

            // Next
            mCfg->groups[group].state = zeroExportState::GETPOWERMETER;
            mCfg->groups[group].stateNext = zeroExportState::GETPOWERMETER;

            unsigned long eTsp = millis();
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
            mLog["group"] = group;
            mLog["type"] = "groupGetPowermeter";
            mLog["B"] = bTsp;

            mCfg->groups[group].stateLast = zeroExportState::GETPOWERMETER;

            doLog = true;

            bool result = false;
            switch (mCfg->groups[group].pm_type) {
                case 1:
                    result = getPowermeterWattsShelly(mLog, group);
                    break;
                case 2:
                    result = getPowermeterWattsTasmota(mLog, group);
                    break;
                case 3:
                    result = getPowermeterWattsMqtt(mLog, group);
                    break;
                case 4:
                    result = getPowermeterWattsHichi(mLog, group);
                    break;
                case 5:
                    result = getPowermeterWattsTibber(mLog, group);
                    break;
            }
            if (!result) {
                mLog["err"] = "type: " + String(mCfg->groups[group].pm_type);
            }

// TODO: eventuell muss hier geprüft werden ob die Daten vom Powermeter plausibel sind.

            // Next
            if (result) {
                mCfg->groups[group].state = zeroExportState::CONTROLLER;
                mCfg->groups[group].stateNext = zeroExportState::CONTROLLER;
            }

            unsigned long eTsp = millis();
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
            mLog["group"] = group;
            mLog["type"] = "groupController";
            mLog["B"] = bTsp;

            mCfg->groups[group].stateLast = zeroExportState::GETPOWERMETER;

            doLog = true;

            // Führungsgröße w in Watt
            float w = mCfg->groups[group].setPoint;

            mLog["w"] = w;

            // Regelgröße x in Watt
            float x  = mCfg->groups[group].pmPower;
            float x1 = mCfg->groups[group].pmPowerL1;
            float x2 = mCfg->groups[group].pmPowerL2;
            float x3 = mCfg->groups[group].pmPowerL3;

            mLog["x"]  = x;
            mLog["x1"] = x1;
            mLog["x2"] = x2;
            mLog["x3"] = x3;

            // Regelabweichung e in Watt
            float e  = w - x;
            float e1 = w - x1;
            float e2 = w - x2;
            float e3 = w - x3;

            mLog["e"]  = e;
            mLog["e1"] = e1;
            mLog["e2"] = e2;
            mLog["e3"] = e3;

            // Regler
// TODO: Regelparameter unter Advanced konfigurierbar? Aber erst wenn Regler komplett ingegriert.
            const float Kp = -1;
            const float Ki = -1;
            const float Kd = -1;
            // - P-Anteil
            float yP  = Kp * e;
            float yP1 = Kp * e1;
            float yP2 = Kp * e2;
            float yP3 = Kp * e3;
            // - I-Anteil
//            float esum = esum + e;
//            float yI = Ki * Ta * esum;
            float yI  = 0;
            float yI1 = 0;
            float yI2 = 0;
            float yI3 = 0;
            // - D-Anteil
//            float yD = Kd * (e -ealt) / Ta;
//            float ealt = e;
            float yD  = 0;
            float yD1 = 0;
            float yD2 = 0;
            float yD3 = 0;
            // - PID-Anteil
            float yPID  = yP  + yI  + yD;
            float yPID1 = yP1 + yI1 + yD1;
            float yPID2 = yP2 + yI2 + yD2;
            float yPID3 = yP3 + yI3 + yD3;

            // Regelbegrenzung
// TODO: Hier könnte man den maximalen Sprung begrenzen

            mLog["yPID"]  = yPID;
            mLog["yPID1"] = yPID1;
            mLog["yPID2"] = yPID2;
            mLog["yPID3"] = yPID3;

            mCfg->groups[group].grpPower   = yPID;
            mCfg->groups[group].grpPowerL1 = yPID1;
            mCfg->groups[group].grpPowerL2 = yPID2;
            mCfg->groups[group].grpPowerL3 = yPID3;

            // Next
            mCfg->groups[group].state = zeroExportState::PROGNOSE;
            mCfg->groups[group].stateNext = zeroExportState::PROGNOSE;

            unsigned long eTsp = millis();
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
            mLog["group"] = group;
            mLog["type"] = "groupPrognose";
            mLog["B"] = bTsp;

            mCfg->groups[group].stateLast = zeroExportState::PROGNOSE;



result = true;
            // Next
//            if (!mCfg->groups[group].waitForAck) {
                mCfg->groups[group].state = zeroExportState::AUFTEILEN;
                mCfg->groups[group].stateNext = zeroExportState::AUFTEILEN;
//            } else {
//                mCfg->groups[group].state = zeroExportState::GETINVERTERACKS;
//                mCfg->groups[group].stateNext = zeroExportState::GETINVERTERACKS;
//            }



            unsigned long eTsp = millis();
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
            mLog["group"] = group;
            mLog["type"] = "groupAufteilen";
            mLog["B"] = bTsp;

            mCfg->groups[group].stateLast = zeroExportState::AUFTEILEN;

            result = true;

            float y  = mCfg->groups[group].grpPower;
            float y1 = mCfg->groups[group].grpPowerL1;
            float y2 = mCfg->groups[group].grpPowerL2;
            float y3 = mCfg->groups[group].grpPowerL3;

            mLog["y"]  = y;
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

            for (uint8_t i = 7; i > 0; --i) {
                if (!grpTarget[i]) {
                    continue;
                }
                mLog[String(String("10")+String(i))] = String(i);

                float *deltaP;
                switch(i) {
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

                // Leistung erhöhen
                if (*deltaP > 0) {
                    // Toleranz
                    if (*deltaP < mCfg->groups[group].powerTolerance) {
                        continue;
                    }
                    mLog["+deltaP"] = *deltaP;
                    zeroExportGroupInverter_t *cfgGroupInv = &mCfg->groups[group].inverters[ivId_Pmin[i]];
                    cfgGroupInv->limitNew = (uint16_t)((float)cfgGroupInv->limit + *deltaP);
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
                    // Toleranz
                    if (*deltaP > -mCfg->groups[group].powerTolerance) {
                        continue;
                    }
                    mLog["-deltaP"] = *deltaP;
                    zeroExportGroupInverter_t *cfgGroupInv = &mCfg->groups[group].inverters[ivId_Pmax[i]];
                    cfgGroupInv->limitNew = (uint16_t)((float)cfgGroupInv->limit + *deltaP);
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
//            if (!mCfg->groups[group].waitForAck) {
                mCfg->groups[group].state = zeroExportState::SETLIMIT;
                mCfg->groups[group].stateNext = zeroExportState::SETLIMIT;
//            } else {
//                mCfg->groups[group].state = zeroExportState::GETINVERTERACKS;
//                mCfg->groups[group].stateNext = zeroExportState::GETINVERTERACKS;
//            }



            unsigned long eTsp = millis();
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
            mLog["group"] = group;
            mLog["type"] = "groupSetLimit";
            mLog["B"] = bTsp;

            // Wait 5s
//            if (mCfg->groups[group].stateLast == zeroExportState::SETPOWER) {
//                if (mCfg->groups[group].lastRun >= (bTsp - 5000UL)) {
//                    return doLog;
//                }
//            }

            mCfg->groups[group].stateLast = zeroExportState::SETLIMIT;

            // Set limit
//            bool wait = false;
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                mLog["inv"] = inv;

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
                doLog = true;
//                wait = true;

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

                mCfg->groups[group].inverters[inv].limit = mCfg->groups[group].inverters[inv].limitNew;
                mLog["limit"] = mCfg->groups[group].inverters[inv].limit;

				// wait for Ack
                mCfg->groups[group].inverters[inv].waitLimitAck = true;

				// send Command
                DynamicJsonDocument doc(512);
                JsonObject obj = doc.to<JsonObject>();
                obj["val"] = mCfg->groups[group].inverters[inv].limit;
                obj["id"] = mCfg->groups[group].inverters[inv].id;
                obj["path"] = "ctrl";
                obj["cmd"] = "limit_nonpersistent_absolute";
                mApi->ctrlRequest(obj);

                mLog["data"] = obj;
            }

            // Next
//            if (!wait) {
//                if (mCfg->groups[group].state != mCfg->groups[group].stateNext) {
//                    mCfg->groups[group].state = mCfg->groups[group].stateNext;
//                } else {
//                    mLog["action"] = "-";
                    mCfg->groups[group].state = zeroExportState::WAITREFRESH;
                    mCfg->groups[group].stateNext = zeroExportState::WAITREFRESH;
//                }
//            }

            unsigned long eTsp = millis();
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
            mLog["group"] = group;
            mLog["type"] = "groupSetPower";
            mLog["B"] = bTsp;

            // Wait 5s
//            if (mCfg->groups[group].stateLast == zeroExportState::SETPOWER) {
//                if (mCfg->groups[group].lastRun >= (bTsp - 5000UL)) {
//                    return doLog;
//                }
//            }

            mCfg->groups[group].stateLast = zeroExportState::SETPOWER;

            // Set power
//            bool wait = false;
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                mLog["inv"] = inv;

				// Nothing todo
                if (mCfg->groups[group].battSwitch == mIv[group][inv]->isProducing()) {
                    continue;
                }

				// Abbruch weil Inverter nicht verfügbar
                if (!mIv[group][inv]->isAvailable()) {
                    continue;
                }

				// do
                doLog = true;
//                wait = true;

                mLog["action"] = String("switch to: ") + String(mCfg->groups[group].battSwitch);

				// wait for Ack
                mCfg->groups[group].inverters[inv].waitPowerAck = true;

				// send Command
                DynamicJsonDocument doc(512);
                JsonObject obj = doc.to<JsonObject>();
                obj["val"] = mCfg->groups[group].battSwitch;
                obj["id"] = inv;
                obj["path"] = "ctrl";
                obj["cmd"] = "power";
                mApi->ctrlRequest(obj);

                mLog["data"] = obj;
            }

            // Next
//            if (!wait) {
//                if (mCfg->groups[group].state != mCfg->groups[group].stateNext) {
//                    mCfg->groups[group].state = mCfg->groups[group].stateNext;
//                } else {
//                    mLog["action"] = "-";
                    mCfg->groups[group].state = zeroExportState::WAITREFRESH;
                    mCfg->groups[group].stateNext = zeroExportState::WAITREFRESH;
//                }
//            }

            unsigned long eTsp = millis();
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
            mLog["group"] = group;
            mLog["type"] = "groupSetReboot";
            mLog["B"] = bTsp;

//            // Wait 5s
//            if (mCfg->groups[group].stateLast == zeroExportState::SETREBOOT) {
//                if (mCfg->groups[group].lastRun >= (bTsp - 5000UL)) {
//                    return doLog;
//                }
//            }

            mCfg->groups[group].stateLast = zeroExportState::SETREBOOT;

            // Set reboot
//            bool wait = false;
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                mLog["inv"] = inv;

				// Abbruch weil Inverter nicht verfügbar
//                if (!mIv[group][inv]->isAvailable()) {
//                    continue;
//                }

				// do
                doLog = true;
//                wait = true;

                mLog["action"] = String("reboot");

				// wait for Ack
                mCfg->groups[group].inverters[inv].waitRebootAck = true;

				// send Command
                DynamicJsonDocument doc(512);
                JsonObject obj = doc.to<JsonObject>();
                obj["id"] = inv;
                obj["path"] = "ctrl";
                obj["cmd"] = "restart";
                mApi->ctrlRequest(obj);

                mLog["data"] = obj;
            }

            // Next
//            if (!wait) {
//                if (mCfg->groups[group].state != mCfg->groups[group].stateNext) {
//                    mCfg->groups[group].state = mCfg->groups[group].stateNext;
//                } else {
//                    mLog["action"] = "-";
                    mCfg->groups[group].state = zeroExportState::WAITREFRESH;
                    mCfg->groups[group].stateNext = zeroExportState::WAITREFRESH;
//                }
//            }

            unsigned long eTsp = millis();
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





/*
// TODO: Vorlage für nachfolgende Funktion getPowermeterWatts. Funktionen erst zusammenführen, wenn keine weiteren Powermeter mehr kommen.
    //C2T2-B91B
        HTTPClient httpClient;

        // TODO: Need to improve here. 2048 for a JSON Obj is to big!?
        bool zero()
        {
            httpClient.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            httpClient.setUserAgent("Ahoy-Agent");
            httpClient.setConnectTimeout(1000);
            httpClient.setTimeout(1000);
            httpClient.addHeader("Content-Type", "application/json");
            httpClient.addHeader("Accept", "application/json");

            if (!httpClient.begin(mCfg->monitor_url)) {
                DPRINTLN(DBG_INFO, "httpClient.begin failed");
                httpClient.end();
                return false;
            }

            if(String(mCfg->monitor_url).endsWith("data.json?node_id=1")){
                httpClient.setAuthorization("admin", mCfg->tibber_pw);
            }

            int httpCode = httpClient.GET();
            if (httpCode == HTTP_CODE_OK)
            {
                String responseBody = httpClient.getString();
                DynamicJsonDocument json(2048);
                DeserializationError err = deserializeJson(json, responseBody);

                // Parse succeeded?
                if (err) {
                    DPRINTLN(DBG_INFO, (F("ZeroExport() JSON error returned: ")));
                    DPRINTLN(DBG_INFO, String(err.f_str()));
                }

                // check if it HICHI
                if(json.containsKey(F("StatusSNS")) ) {
                    int index = responseBody.indexOf(String(mCfg->json_path));  // find first match position
                    responseBody = responseBody.substring(index);               // cut it and store it in value
                    index = responseBody.indexOf(",");                          // find the first seperation - Bingo!?

                    mCfg->total_power = responseBody.substring(responseBody.indexOf(":"), index).toDouble();
                } else if(json.containsKey(F("emeters"))) {
                    mCfg->total_power = (double)json[F("total_power")];
                } else if(String(mCfg->monitor_url).endsWith("data.json?node_id=1") ) {
                    tibber_parse();
                } else {
                     DPRINTLN(DBG_INFO, (F("ZeroExport() json error: cant find value in this query: ") + responseBody));
                     return false;
                }
            }
            else
            {
                DPRINTLN(DBG_INFO, F("ZeroExport(): Error ") + String(httpCode));
                return false;
            }
            httpClient.end();
            return true;
        }

        void tibber_parse()
        {

        }
*/

        // Powermeter

        /**
         * getPowermeterWattsShelly
         */
        bool getPowermeterWattsShelly(JsonObject logObj, uint8_t group) {
            bool result = false;

            mCfg->groups[group].pmPower   = 0;
            mCfg->groups[group].pmPowerL1 = 0;
            mCfg->groups[group].pmPowerL2 = 0;
            mCfg->groups[group].pmPowerL3 = 0;

            long int bTsp = millis();

            HTTPClient http;
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            http.setUserAgent("Ahoy-Agent");
// TODO: Ahoy-0.8.850024-zero
            http.setConnectTimeout(500);
            http.setTimeout(500);
            http.addHeader("Content-Type", "application/json");
            http.addHeader("Accept", "application/json");
// TODO: Timeout von 1000 reduzieren?
            http.begin(mCfg->groups[group].pm_url);
            if (http.GET() == HTTP_CODE_OK)
            {

                // Parsing
                DynamicJsonDocument doc(2048);
                DeserializationError error = deserializeJson(doc, http.getString());
                if (error)
                {
                    logObj["err"] = "deserializeJson: " + String(error.c_str());
                    return false;
                } else {

                    // Shelly 3EM
                    if (doc.containsKey(F("total_power"))) {
                        mCfg->groups[group].pmPower = doc["total_power"];
                        result = true;
                    // Shelly pro 3EM
                    } else if (doc.containsKey(F("em:0"))) {
                        mCfg->groups[group].pmPower = doc["em:0"]["total_act_power"];
                        result = true;
                    // Keine Daten
                    } else {
                        mCfg->groups[group].pmPower = 0;
                    }

                    // Shelly 3EM
                    if (doc.containsKey(F("emeters"))) {
                        mCfg->groups[group].pmPowerL1 = doc["emeters"][0]["power"];
                        result = true;
                    // Shelly pro 3EM
                    } else if (doc.containsKey(F("em:0"))) {
                        mCfg->groups[group].pmPowerL1 = doc["em:0"]["a_act_power"];
                        result = true;
                    // Shelly plus1pm plus2pm
                    } else if (doc.containsKey(F("switch:0"))) {
                        mCfg->groups[group].pmPowerL1 = doc["switch:0"]["apower"];
                        mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL1;
                        result = true;
                    // Shelly Alternative
                    } else if (doc.containsKey(F("apower"))) {
                        mCfg->groups[group].pmPowerL1 = doc["apower"];
                        mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL1;
                        result = true;
                    // Keine Daten
                    } else {
                        mCfg->groups[group].pmPowerL1 = 0;
                    }

                    // Shelly 3EM
                    if (doc.containsKey(F("emeters"))) {
                        mCfg->groups[group].pmPowerL2 = doc["emeters"][1]["power"];
                        result = true;
                    // Shelly pro 3EM
                    } else if (doc.containsKey(F("em:0"))) {
                        mCfg->groups[group].pmPowerL2 = doc["em:0"]["b_act_power"];
                        result = true;
                    // Shelly plus1pm plus2pm
                    } else if (doc.containsKey(F("switch:1"))) {
                        mCfg->groups[group].pmPowerL2 = doc["switch.1"]["apower"];
                        mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL2;
                        result = true;
//                // Shelly Alternative
//                } else if (doc.containsKey(F("apower"))) {
//                    mCfg->groups[group].pmPowerL2 = doc["apower"];
//                    mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL2;
//                    ret = true;
                    // Keine Daten
                    } else {
                        mCfg->groups[group].pmPowerL2 = 0;
                    }

                    // Shelly 3EM
                    if (doc.containsKey(F("emeters"))) {
                        mCfg->groups[group].pmPowerL3 = doc["emeters"][2]["power"];
                        result = true;
                    // Shelly pro 3EM
                    } else if (doc.containsKey(F("em:0"))) {
                        mCfg->groups[group].pmPowerL3 = doc["em:0"]["c_act_power"];
                        result = true;
                    // Shelly plus1pm plus2pm
                    } else if (doc.containsKey(F("switch:2"))) {
                        mCfg->groups[group].pmPowerL3 = doc["switch:2"]["apower"];
                        mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL3;
                        result = true;
//                // Shelly Alternative
//                } else if (doc.containsKey(F("apower"))) {
//                    mCfg->groups[group].pmPowerL3 = doc["apower"];
//                    mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL3;
//                    result = true;
                    // Keine Daten
                    } else {
                        mCfg->groups[group].pmPowerL3 = 0;
                    }
                }

            }
            http.end();

            long int eTsp = millis();
            logObj["b"] = bTsp;
            logObj["e"] = eTsp;
            logObj["d"] = eTsp - bTsp;
            logObj["P"]  = mCfg->groups[group].pmPower;
            logObj["P1"] = mCfg->groups[group].pmPowerL1;
            logObj["P2"] = mCfg->groups[group].pmPowerL2;
            logObj["P3"] = mCfg->groups[group].pmPowerL3;

            return result;
        }

        /**
         * getPowermeterWattsTasmota
         */
        bool getPowermeterWattsTasmota(JsonObject logObj, uint8_t group) {
// TODO: nicht komplett
            bool ret = false;

            HTTPClient http;
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            http.setUserAgent("Ahoy-Agent");
// TODO: Ahoy-0.8.850024-zero
            http.setConnectTimeout(500);
            http.setTimeout(500);
            http.addHeader("Content-Type", "application/json");
            http.addHeader("Accept", "application/json");
// TODO: Timeout von 1000 reduzieren?
            http.begin(mCfg->groups[group].pm_url);
            if (http.GET() == HTTP_CODE_OK)
            {

                // Parsing
                DynamicJsonDocument doc(2048);
                DeserializationError error = deserializeJson(doc, http.getString());
                if (error)
                {
                    logObj["error"] = "deserializeJson() failed: " + String(error.c_str());
                    return ret;
                }

// TODO: Sum
                    ret = true;

// TODO: L1

// TODO: L2

// TODO: L3

/*
                JsonObject Tasmota_ENERGY = doc["StatusSNS"]["ENERGY"];
                int Tasmota_Power = Tasmota_ENERGY["Power"]; // 0
                return Tasmota_Power;
*/
                /*
                String url = "http://" + String(TASMOTA_IP) + "/cm?cmnd=status%2010";
                ParsedData = http.get(url).json();
                int Watts = ParsedData[TASMOTA_JSON_STATUS][TASMOTA_JSON_PAYLOAD_MQTT_PREFIX][TASMOTA_JSON_POWER_MQTT_LABEL].toInt();
                return Watts;
                */

                logObj["P"]   = mCfg->groups[group].pmPower;
                logObj["P1"] = mCfg->groups[group].pmPowerL1;
                logObj["P2"] = mCfg->groups[group].pmPowerL2;
                logObj["P3"] = mCfg->groups[group].pmPowerL3;
            }
            http.end();

            return ret;
        }

        /**
         * getPowermeterWattsMqtt
         */
        bool getPowermeterWattsMqtt(JsonObject logObj, uint8_t group) {
// TODO: nicht komplett
            bool ret = false;



//                logObj["P"]   = mCfg->groups[group].pmPower;
//                logObj["P1"] = mCfg->groups[group].pmPowerL1;
//                logObj["P2"] = mCfg->groups[group].pmPowerL2;
//                logObj["P3"] = mCfg->groups[group].pmPowerL3;
//            }
//            http.end();

            return ret;
        }

        /**
         * getPowermeterWattsHichi
         */
        bool getPowermeterWattsHichi(JsonObject logObj, uint8_t group) {
// TODO: nicht komplett
            bool ret = false;



//                logObj["P"]   = mCfg->groups[group].pmPower;
//                logObj["P1"] = mCfg->groups[group].pmPowerL1;
//                logObj["P2"] = mCfg->groups[group].pmPowerL2;
//                logObj["P3"] = mCfg->groups[group].pmPowerL3;
//            }
//            http.end();

            return ret;
        }

        /**
         * getPowermeterWattsTibber
         */
        bool getPowermeterWattsTibber(JsonObject logObj, uint8_t group) {
// TODO: nicht komplett
            bool ret = false;



//                logObj["P"]   = mCfg->groups[group].pmPower;
//                logObj["P1"] = mCfg->groups[group].pmPowerL1;
//                logObj["P2"] = mCfg->groups[group].pmPowerL2;
//                logObj["P3"] = mCfg->groups[group].pmPowerL3;
//            }
//            http.end();

            return ret;
        }

/*
// TODO: Vorlage für Berechnung
        Inverter<> *iv = mSys.getInverterByPos(mConfig->plugin.zeroExport.Iv);

        DynamicJsonDocument doc(512);
        JsonObject object = doc.to<JsonObject>();

        double nValue = round(mZeroExport.getPowertoSetnewValue());
        double twoPerVal = nValue <= (iv->getMaxPower() / 100 * 2 );

        if(mConfig->plugin.zeroExport.two_percent && (nValue <= twoPerVal))
            nValue = twoPerVal;

        if(mConfig->plugin.zeroExport.max_power <= nValue)
            nValue = mConfig->plugin.zeroExport.max_power;

        if(iv->actPowerLimit == nValue) {
            mConfig->plugin.zeroExport.lastTime = millis();    // set last timestamp
            return; // if PowerLimit same as befor, then skip
        }

        object["val"] = nValue;
        object["id"] = mConfig->plugin.zeroExport.Iv;
        object["path"] = "ctrl";
        object["cmd"] = "limit_nonpersistent_absolute";

        String data;
        serializeJsonPretty(object, data);
        DPRINTLN(DBG_INFO, data);
        mApi.ctrlRequest(object);

        mConfig->plugin.zeroExport.lastTime = millis();    // set last timestamp
*/

        // private member variables
        bool mIsInitialized = false;
        zeroExport_t *mCfg;
        settings_t *mConfig;
        HMSYSTEM *mSys;
        RestApiType *mApi;
        StaticJsonDocument<5000> mDocLog;
        JsonObject mLog = mDocLog.to<JsonObject>();
        PubMqttType *mMqtt;

        Inverter<> *mIv[ZEROEXPORT_MAX_GROUPS][ZEROEXPORT_GROUP_MAX_INVERTERS];
};










// TODO: Vorlagen für Powermeter-Analyse
/*
Shelly 1pm
Der Shelly 1pm verfügt über keine eigene Spannungsmessung sondern geht von 220V * Korrekturfaktor aus. Dadurch wird die Leistungsmessung verfälscht und der Shelly ist ungeeignet.


http://192.168.xxx.xxx/status
Shelly 3em (oder em3) :
ok
{"wifi_sta":{"connected":true,"ssid":"Odyssee2001","ip":"192.168.100.85","rssi":-23},"cloud":{"enabled":false,"connected":false},"mqtt":{"connected":true},"time":"17:13","unixtime":1709223219,"serial":27384,"has_update":false,"mac":"3494547B94EE","cfg_changed_cnt":1,"actions_stats":{"skipped":0},"relays":[{"ison":false,"has_timer":false,"timer_started":0,"timer_duration":0,"timer_remaining":0,"overpower":false,"is_valid":true,"source":"input"}],"emeters":[{"power":51.08,"pf":0.27,"current":0.78,"voltage":234.90,"is_valid":true,"total":1686297.2,"total_returned":428958.4},{"power":155.02,"pf":0.98,"current":0.66,"voltage":235.57,"is_valid":true,"total":878905.6,"total_returned":4.1},{"power":6.75,"pf":0.26,"current":0.11,"voltage":234.70,"is_valid":true,"total":206151.1,"total_returned":0.0}],"total_power":212.85,"emeter_n":{"current":0.00,"ixsum":1.29,"mismatch":false,"is_valid":false},"fs_mounted":true,"v_data":1,"ct_calst":0,"update":{"status":"idle","has_update":false,"new_version":"20230913-114244/v1.14.0-gcb84623","old_version":"20230913-114244/v1.14.0-gcb84623","beta_version":"20231107-165007/v1.14.1-rc1-g0617c15"},"ram_total":49920,"ram_free":30192,"fs_size":233681,"fs_free":154616,"uptime":13728721}


Shelly plus 2pm :
ok
{"ble":{},"cloud":{"connected":false},"input:0":{"id":0,"state":false},"input:1":{"id":1,"state":false},"mqtt":{"connected":true},"switch:0":{"id":0, "source":"MQTT", "output":false, "apower":0.0, "voltage":237.0, "freq":50.0, "current":0.000, "pf":0.00, "aenergy":{"total":62758.285,"by_minute":[0.000,0.000,0.000],"minute_ts":1709223337},"temperature":{"tC":35.5, "tF":96.0}},"switch:1":{"id":1, "source":"MQTT", "output":false, "apower":0.0, "voltage":237.1, "freq":50.0, "current":0.000, "pf":0.00, "aenergy":{"total":61917.211,"by_minute":[0.000,0.000,0.000],"minute_ts":1709223337},"temperature":{"tC":35.5, "tF":96.0}},"sys":{"mac":"B0B21C10A478","restart_required":false,"time":"17:15","unixtime":1709223338,"uptime":8746115,"ram_size":245016,"ram_free":141004,"fs_size":458752,"fs_free":135168,"cfg_rev":7,"kvs_rev":0,"schedule_rev":0,"webhook_rev":0,"available_updates":{"stable":{"version":"1.2.2"}}},"wifi":{"sta_ip":"192.168.100.87","status":"got ip","ssid":"Odyssee2001","rssi":-62},"ws":{"connected":false}}

http://192.168.xxx.xxx/rpc/Shelly.GetStatus
Shelly plus 1pm :
nok keine negativen Leistungswerte
{"ble":{},"cloud":{"connected":false},"input:0":{"id":0,"state":false},"mqtt":{"connected":true},"switch:0":{"id":0, "source":"MQTT", "output":false, "apower":0.0, "voltage":235.9, "current":0.000, "aenergy":{"total":20393.619,"by_minute":[0.000,0.000,0.000],"minute_ts":1709223441},"temperature":{"tC":34.6, "tF":94.3}},"sys":{"mac":"FCB467A66E3C","restart_required":false,"time":"17:17","unixtime":1709223443,"uptime":8644483,"ram_size":246256,"ram_free":143544,"fs_size":458752,"fs_free":147456,"cfg_rev":9,"kvs_rev":0,"schedule_rev":0,"webhook_rev":0,"available_updates":{"stable":{"version":"1.2.2"}}},"wifi":{"sta_ip":"192.168.100.88","status":"got ip","ssid":"Odyssee2001","rssi":-42},"ws":{"connected":false}}


Tasmota
http://192.168.100.81/cm?cmnd=Status0
{"Status":{"Module":1,"DeviceName":"Tasmota","FriendlyName":["Tasmota"],"Topic":"Tasmota","ButtonTopic":"0","Power":0,"PowerOnState":3,"LedState":1,"LedMask":"FFFF","SaveData":1,"SaveState":1,"SwitchTopic":"0","SwitchMode":[0,0,0,0,0,0,0,0],"ButtonRetain":0,"SwitchRetain":0,"SensorRetain":0,"PowerRetain":0,"InfoRetain":0,"StateRetain":0},"StatusPRM":{"Baudrate":9600,"SerialConfig":"8N1","GroupTopic":"tasmotas","OtaUrl":"http://ota.tasmota.com/tasmota/release/tasmota.bin.gz","RestartReason":"Software/System restart","Uptime":"202T01:24:51","StartupUTC":"2023-08-13T15:21:13","Sleep":50,"CfgHolder":4617,"BootCount":27,"BCResetTime":"2023-02-04T16:45:38","SaveCount":150,"SaveAddress":"F5000"},"StatusFWR":{"Version":"11.1.0(tasmota)","BuildDateTime":"2022-05-05T03:23:22","Boot":31,"Core":"2_7_4_9","SDK":"2.2.2-dev(38a443e)","CpuFrequency":80,"Hardware":"ESP8266EX","CR":"378/699"},"StatusLOG":{"SerialLog":0,"WebLog":2,"MqttLog":0,"SysLog":0,"LogHost":"","LogPort":514,"SSId":["Odyssee2001",""],"TelePeriod":300,"Resolution":"558180C0","SetOption":["00008009","2805C80001000600003C5A0A190000000000","00000080","00006000","00004000"]},"StatusMEM":{"ProgramSize":658,"Free":344,"Heap":17,"ProgramFlashSize":1024,"FlashSize":1024,"FlashChipId":"14325E","FlashFrequency":40,"FlashMode":3,"Features":["00000809","87DAC787","043E8001","000000CF","010013C0","C000F989","00004004","00001000","04000020"],"Drivers":"1,2,3,4,5,6,7,8,9,10,12,16,18,19,20,21,22,24,26,27,29,30,35,37,45,56,62","Sensors":"1,2,3,4,5,6,53"},"StatusNET":{"Hostname":"Tasmota","IPAddress":"192.168.100.81","Gateway":"192.168.100.1","Subnetmask":"255.255.255.0","DNSServer1":"192.168.100.1","DNSServer2":"0.0.0.0","Mac":"4C:11:AE:11:F8:50","Webserver":2,"HTTP_API":1,"WifiConfig":4,"WifiPower":17.0},"StatusMQT":{"MqttHost":"192.168.100.80","MqttPort":1883,"MqttClientMask":"Tasmota","MqttClient":"Tasmota","MqttUser":"mqttuser","MqttCount":156,"MAX_PACKET_SIZE":1200,"KEEPALIVE":30,"SOCKET_TIMEOUT":4},"StatusTIM":{"UTC":"2024-03-02T16:46:04","Local":"2024-03-02T17:46:04","StartDST":"2024-03-31T02:00:00","EndDST":"2024-10-27T03:00:00","Timezone":"+01:00","Sunrise":"07:29","Sunset":"18:35"},"StatusSNS":{"Time":"2024-03-02T17:46:04","PV":{"Bezug":0.364,"Einspeisung":3559.439,"Leistung":-14}},"StatusSTS":{"Time":"2024-03-02T17:46:04","Uptime":"202T01:24:51","UptimeSec":17457891,"Heap":16,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"MqttCount":156,"POWER":"OFF","Wifi":{"AP":1,"SSId":"Odyssee2001","BSSId":"34:31:C4:22:92:74","Channel":6,"Mode":"11n","RSSI":100,"Signal":-50,"LinkCount":15,"Downtime":"0T00:08:22"}}}










*/

#endif /*__ZEROEXPORT__*/

#endif /* #if defined(ESP32) */