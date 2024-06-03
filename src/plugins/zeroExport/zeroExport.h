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
#include "utils/DynamicJsonHandler.h"

template <class HMSYSTEM>

class ZeroExport {
   public:
    /** ZeroExport
     * constructor
     */
    ZeroExport() {}

    /** ~ZeroExport
     * destructor
     */
    ~ZeroExport() {}

    /** setup
     * Initialisierung
     * @param *cfg
     * @param *sys
     * @param *config
     * @param *api
     * @param *mqtt
     * @returns void
     */
    void setup(IApp *app, uint32_t *timestamp, zeroExport_t *cfg, HMSYSTEM *sys, settings_t *config, RestApiType *api, PubMqttType *mqtt) {
        mApp = app;
        mTimestamp = timestamp;
        mCfg = cfg;
        mSys = sys;
        mConfig = config;
        mApi = api;
        mMqtt = mqtt;

        mIsInitialized = mPowermeter.setup(mApp, mCfg, mqtt, &_log);
    }

    /*void printJson() {
        serializeJson(doc, Serial);
        Serial.println();
        serializeJsonPretty(doc, Serial);
    }*/

    /** loop
     * Arbeitsschleife
     * @param void
     * @returns void
     * @todo emergency
     */
    void loop(void) {
        if ((!mIsInitialized) || (!mCfg->enabled)) return;

        mPowermeter.loop();
        //        sendLog();
        clearLog();

        // Takt
        unsigned long Tsp = millis();
        if (mLastRun > (Tsp - 1000)) return;
        mLastRun = Tsp;

        if (mCfg->debug) DBGPRINTLN(F("Takt:"));

        // Exit if Queue is empty
        zeroExportQueue_t Queue;
        if (!getQueue(&Queue)) return;

        if (mCfg->debug) DBGPRINTLN(F("Queue:"));

        // Load Data from Queue
        uint8_t group = Queue.group;
        uint8_t inv = Queue.inv;
        zeroExportGroup_t *CfgGroup = &mCfg->groups[group];
        zeroExportGroupInverter_t *CfgGroupInv = &CfgGroup->inverters[inv];
        Inverter<> *iv = mSys->getInverterByPos(Queue.id);
        if (NULL == iv) return;

        if (!CfgGroup->battSwitch && !CfgGroup->battSwitchInit) {
            if (!iv->alarmCnt) return;
            bool stb_flag = false;

            for (int16_t i = 0; i < iv->alarmCnt; i++) {
                if (iv->lastAlarm[i].code == 124) {
                    stb_flag = true;
                    _log.addProperty("alarm1", stb_flag);
                    _log.addProperty("start", iv->lastAlarm[i].start);
                    _log.addProperty("end", iv->lastAlarm[i].end);

                    if (iv->lastAlarm[i].end > iv->lastAlarm[i].start) {
                        stb_flag = false;
                        _log.addProperty("alarm2", stb_flag);
                    }
                    sendLog();
                    clearLog();
                }
            }
            if (!stb_flag) CfgGroup->battSwitch = true;
            CfgGroup->battSwitchInit = true;
        }

        _log.addProperty("g", group);
        _log.addProperty("i", inv);

        // Check Data->iv
        if (!iv->isAvailable()) {
            if (mCfg->debug) {
                _log.addProperty("nA", "!isAvailable");
                sendLog();
            }
            clearLog();
            return;
        }

        // Check Data->waitAck
        if (CfgGroupInv->waitAck > 0) {
            if (mCfg->debug) {
                _log.addProperty("wA", CfgGroupInv->waitAck);
                sendLog();
            }
            clearLog();
            return;
        }

        // Wird nur zum debuggen benötigt?
        uint16_t groupPower = 0;
        uint16_t groupLimit = 0;
        for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
            groupPower += mCfg->groups[group].inverters[inv].power;  // Calc Data->groupPower
            groupLimit += mCfg->groups[group].inverters[inv].limit;  // Calc Data->groupLimit
        }

        _log.addProperty("gP", groupPower);
        _log.addProperty("gL", groupLimit);
        // Wird nur zum debuggen benötigt?

        // Batteryprotection
        _log.addProperty("bEn", (uint8_t)CfgGroup->battCfg);

        switch (CfgGroup->battCfg) {
            case zeroExportBatteryCfg::none:
                if (CfgGroup->battSwitch != true) {
                    CfgGroup->battSwitch = true;

                    _log.addProperty("bA", "turn on");
                }
                break;
            case zeroExportBatteryCfg::invUdc:
            case zeroExportBatteryCfg::mqttU:
            case zeroExportBatteryCfg::mqttSoC:
                if (CfgGroup->battSwitch != true) {
                    if (CfgGroup->battValue > CfgGroup->battLimitOn) {
                        CfgGroup->battSwitch = true;
                        _log.addProperty("bA", "turn on");
                    }
                    // if ((CfgGroup->battValue > CfgGroup->battLimitOff) && (CfgGroupInv->power > 0)) {
                    //     CfgGroup->battSwitch = true;
                    //     _log.addProperty("bA", "turn on");
                    // }
                } else {
                    if (CfgGroup->battValue < CfgGroup->battLimitOff) {
                        CfgGroup->battSwitch = false;
                        _log.addProperty("bA", "turn off");
                    }
                }
                _log.addProperty("bU", ah::round1(CfgGroup->battValue));
                break;
            default:
                if (CfgGroup->battSwitch == true) {
                    CfgGroup->battSwitch = false;
                    _log.addProperty("bA", "turn off");
                }
                break;
        }
        _log.addProperty("bSw", CfgGroup->battSwitch);

        // Controller

        // Führungsgröße w in Watt
        int16_t w = CfgGroup->setPoint;
        _log.addProperty("w", w);

        // Regelgröße x in Watt
        int16_t x = 0.0;
        if (CfgGroup->minimum) {
            x = mPowermeter.getDataMIN(group);
        } else {
            x = mPowermeter.getDataAVG(group);
        }
        _log.addProperty("x", x);

        // Regelabweichung e in Watt
        int16_t e = w - x;
        _log.addProperty("e", e);

        // Keine Regelung innerhalb der Toleranzgrenzen
        if ((e < CfgGroup->powerTolerance) && (e > -CfgGroup->powerTolerance)) {
            e = 0;
            _log.addProperty("eK", e);
            sendLog();
            clearLog();
            return;
        }

        // Regler
        float Kp = CfgGroup->Kp * -0.01;
        float Ki = CfgGroup->Ki * -0.001;
        float Kd = CfgGroup->Kd * -0.001;
        unsigned long Ta = Tsp - CfgGroup->lastRefresh;
        CfgGroup->lastRefresh = Tsp;
        int16_t yP = Kp * e;
        CfgGroup->eSum += e;
        int16_t yI = Ki * Ta * CfgGroup->eSum;
        if (Ta == 0) {
            _log.addProperty("Error", "Ta = 0");
            sendLog();
            clearLog();
            return;
        }
        int16_t yD = Kd * (e - CfgGroup->eOld) / Ta;

        if (mCfg->debug) {
            _log.addProperty("Kp", Kp);
            _log.addProperty("Ki", Ki);
            _log.addProperty("Kd", Kd);
            _log.addProperty("Ta", Ta);
            _log.addProperty("yP", yP);
            _log.addProperty("yI", yI);
            _log.addProperty("eSum", CfgGroup->eSum);
            _log.addProperty("yD", yD);
            _log.addProperty("eOld", CfgGroup->eOld);
        }

        CfgGroup->eOld = e;
        int16_t y = yP + yI + yD;

        _log.addProperty("y", y);

        // Regelbegrenzung
        // TODO: Hier könnte man den maximalen Sprung begrenzen

        // Stellgröße y in W
        CfgGroupInv->limitNew += y;

        // Check

        if (CfgGroupInv->action == zeroExportAction_t::doNone) {
            if ((CfgGroup->battSwitch == true) && (CfgGroupInv->limitNew > CfgGroupInv->powerMin) && (CfgGroupInv->power == 0) && (mCfg->sleep != true) && (CfgGroup->sleep != true)) {
                if (CfgGroupInv->actionTimer < 0) CfgGroupInv->actionTimer = 0;
                if (CfgGroupInv->actionTimer == 0) CfgGroupInv->actionTimer = 1;
                if (CfgGroupInv->actionTimer > 10) {
                    CfgGroupInv->action = zeroExportAction_t::doTurnOn;
                    _log.addProperty("do", "doTurnOn");
                }
            }
            if ((CfgGroupInv->turnOff) && (CfgGroupInv->limitNew <= 0) && (CfgGroupInv->power > 0)) {
                if (CfgGroupInv->actionTimer > 0) CfgGroupInv->actionTimer = 0;
                if (CfgGroupInv->actionTimer == 0) CfgGroupInv->actionTimer = -1;
                if (CfgGroupInv->actionTimer < 30) {
                    CfgGroupInv->action = zeroExportAction_t::doTurnOff;
                    _log.addProperty("do", "doTurnOff");
                }
            }
            if (((CfgGroup->battSwitch == false) || (mCfg->sleep == true) || (CfgGroup->sleep == true)) && (CfgGroupInv->power > 0)) {
                CfgGroupInv->action = zeroExportAction_t::doTurnOff;
                _log.addProperty("do", "sleep");
            }
        }
        _log.addProperty("doT", CfgGroupInv->action);

        if (CfgGroupInv->action == zeroExportAction_t::doNone) {
            _log.addProperty("l", CfgGroupInv->limit);
            _log.addProperty("ln", CfgGroupInv->limitNew);

            // groupMax
            uint16_t otherIvLimit = groupLimit - CfgGroupInv->limit;
            if ((otherIvLimit + CfgGroupInv->limitNew) > CfgGroup->powerMax) {
                CfgGroupInv->limitNew = CfgGroup->powerMax - otherIvLimit;
            }
            if (mCfg->debug) {
                _log.addProperty("gPM", CfgGroup->powerMax);
            }

            // PowerMax
            uint16_t powerMax = 100;
            if (CfgGroupInv->MaxPower > 100) powerMax = CfgGroupInv->MaxPower;
            if (CfgGroupInv->powerMax < powerMax) powerMax = CfgGroupInv->powerMax;
            if (CfgGroupInv->limitNew > powerMax) CfgGroupInv->limitNew = powerMax;

            // PowerMin
            uint16_t powerMin = CfgGroupInv->MaxPower / 100 * 2;
            if (CfgGroupInv->powerMin > powerMin) powerMin = CfgGroupInv->powerMin;
            if (CfgGroupInv->limitNew < powerMin) CfgGroupInv->limitNew = powerMin;

            // Sleep
            if (mCfg->sleep || CfgGroup->sleep) CfgGroupInv->limitNew = powerMin;

            // Standby -> PowerMin
            if (CfgGroupInv->power == 0) CfgGroupInv->limitNew = powerMin;

            // Mindeständerung ZEROEXPORT_GROUP_WR_LIMIT_MIN_DIFF
            if ((CfgGroupInv->limitNew < (CfgGroupInv->limit + ZEROEXPORT_GROUP_WR_LIMIT_MIN_DIFF)) && (CfgGroupInv->limitNew > (CfgGroupInv->limit - ZEROEXPORT_GROUP_WR_LIMIT_MIN_DIFF))) CfgGroupInv->limitNew = CfgGroupInv->limit;

            if (CfgGroupInv->limit != CfgGroupInv->limitNew) CfgGroupInv->action = zeroExportAction_t::doActivePowerContr;

            if ((CfgGroupInv->limit == powerMin) && (CfgGroupInv->power == 0)) {
                CfgGroupInv->action = zeroExportAction_t::doNone;
                if (!mCfg->debug) {
                    clearLog();
                    return;
                }
            }

            //            CfgGroupInv->actionTimer = 0;
            // TODO: Timer stoppen wenn Limit gesetzt wird.
            _log.addProperty("lN", CfgGroupInv->limitNew);

            CfgGroupInv->limit = CfgGroupInv->limitNew;
        }

        // doAction
        _log.addProperty("a", CfgGroupInv->action);

        switch (CfgGroupInv->action) {
            case zeroExportAction_t::doRestart:
                if (iv->setDevControlRequest(Restart)) {
                    mApp->triggerTickSend(iv->id);
                    CfgGroupInv->waitAck = 120;
                    CfgGroupInv->action = zeroExportAction_t::doNone;
                    CfgGroupInv->actionTimer = 0;
                    CfgGroupInv->actionTimestamp = Tsp;
                }
                break;
            case zeroExportAction_t::doTurnOn:
                if (iv->setDevControlRequest(TurnOn)) {
                    mApp->triggerTickSend(iv->id);
                    CfgGroupInv->waitAck = 120;
                    CfgGroupInv->action = zeroExportAction_t::doNone;
                    CfgGroupInv->actionTimer = 0;
                    CfgGroupInv->actionTimestamp = Tsp;
                }
                break;
            case zeroExportAction_t::doTurnOff:
                if (iv->setDevControlRequest(TurnOff)) {
                    mApp->triggerTickSend(iv->id);
                    CfgGroupInv->waitAck = 120;
                    CfgGroupInv->action = zeroExportAction_t::doNone;
                    CfgGroupInv->actionTimer = 0;
                    CfgGroupInv->actionTimestamp = Tsp;
                }
                break;
            case zeroExportAction_t::doActivePowerContr:
                iv->powerLimit[0] = static_cast<uint16_t>(CfgGroupInv->limit * 10.0);
                iv->powerLimit[1] = AbsolutNonPersistent;
                if (iv->setDevControlRequest(ActivePowerContr)) {
                    mApp->triggerTickSend(iv->id);
                    CfgGroupInv->waitAck = 60;
                    CfgGroupInv->action = zeroExportAction_t::doNone;
                    CfgGroupInv->actionTimer = 0;
                    CfgGroupInv->actionTimestamp = Tsp;
                }
                break;
            default:
                break;
        }

        // MQTT - Inverter
        if (mMqtt->isConnected()) {
            mqttPublish(String("zero/state/groups/" + String(group) + "/inverter/" + String(inv)).c_str(), _log.toString().c_str());
        }

        sendLog();
        clearLog();

        return;
    }

    /** tickSecond
     * Time pulse every second
     * @param void
     * @returns void
     * @todo Eventuell ein waitAck für alle 3 Set-Befehle
     * @todo Eventuell ein waitAck für alle Inverter einer Gruppe
     */
    void tickSecond() {
        if ((!mIsInitialized) || (!mCfg->enabled)) return;

        // Reduce WaitAck every second
        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            if (!mCfg->groups[group].enabled) continue;

            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                if (!mCfg->groups[group].inverters[inv].enabled) continue;

                if (mCfg->groups[group].inverters[inv].waitAck > 0) {
                    mCfg->groups[group].inverters[inv].waitAck--;
                }

                if (mCfg->groups[group].inverters[inv].actionTimer > 0) mCfg->groups[group].inverters[inv].actionTimer++;
                if (mCfg->groups[group].inverters[inv].actionTimer < 0) mCfg->groups[group].inverters[inv].actionTimer--;
            }
        }
    }

    /** tickerMidnight
     * Time pulse Midnicht
     * Reboots Inverter at Midnight to reset YieldDay and clean start environment
     * @param void
     * @returns void
     * @todo activate
     * @todo tickMidnight wird nicht nur um Mitternacht ausgeführt sondern auch beim Reboot von Ahoy.
     * @todo Reboot der Inverter um Mitternacht in Ahoy selbst verschieben mit separater Config-Checkbox
     * @todo Ahoy Config-Checkbox Reboot Inverter at Midnight beim groupInit() automatisch setzen.
     */
    void tickMidnight(void) {
        if ((!mIsInitialized) || (!mCfg->enabled)) return;

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            if (!mCfg->groups[group].enabled) continue;

            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                if (!mCfg->groups[group].inverters[inv].enabled) continue;

                mCfg->groups[group].inverters[inv].action = zeroExportAction_t::doRestart;
            }
        }
    }

    /** eventAckSetLimit
     * Reset waiting time limit
     * @param iv
     * @returns void
     */
    void eventAckSetLimit(Inverter<> *iv) {
        if ((!mIsInitialized) || (!mCfg->enabled)) return;

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            if (!mCfg->groups[group].enabled) continue;

            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                if (!mCfg->groups[group].inverters[inv].enabled) continue;

                if (iv->id == (uint8_t)mCfg->groups[group].inverters[inv].id) {
                    _log.addProperty("g", group);
                    _log.addProperty("i", inv);

                    mCfg->groups[group].inverters[inv].waitAck = 0;

                    _log.addProperty("wA", mCfg->groups[group].inverters[inv].waitAck);

                    // Wenn ein neuer LimitWert da ist. Soll es in group schreiben.
                    if (iv->actPowerLimit != 0xffff) {
                        _log.addProperty("l", mCfg->groups[group].inverters[inv].limit);

                        mCfg->groups[group].inverters[inv].limit = iv->actPowerLimit;

                        _log.addProperty("lF", mCfg->groups[group].inverters[inv].limit);
                    }
                    sendLog();
                    clearLog();
                }
            }
        }
    }

    /** eventAckSetPower
     * Reset waiting time power
     * @param iv
     * @returns void
     */
    void eventAckSetPower(Inverter<> *iv) {
        if ((!mIsInitialized) || (!mCfg->enabled)) return;

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            if (!mCfg->groups[group].enabled) continue;

            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                if (!mCfg->groups[group].inverters[inv].enabled) continue;

                if (iv->id == mCfg->groups[group].inverters[inv].id) {
                    _log.addProperty("g", group);
                    _log.addProperty("i", inv);

                    mCfg->groups[group].inverters[inv].waitAck = 0;

                    _log.addProperty("wA", mCfg->groups[group].inverters[inv].waitAck);
                    sendLog();
                    clearLog();
                }
            }
        }
    }

    /** eventAckSetReboot
     * Reset waiting time reboot
     * @param iv
     * @returns void
     */
    void eventAckSetReboot(Inverter<> *iv) {
        if ((!mIsInitialized) || (!mCfg->enabled)) return;

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            if (!mCfg->groups[group].enabled) continue;

            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                if (!mCfg->groups[group].inverters[inv].enabled) continue;

                if (iv->id == mCfg->groups[group].inverters[inv].id) {
                    _log.addProperty("g", group);
                    _log.addProperty("i", inv);

                    mCfg->groups[group].inverters[inv].waitAck = 0;

                    _log.addProperty("wA", mCfg->groups[group].inverters[inv].waitAck);

                    mCfg->groups[group].inverters[inv].limit = mCfg->groups[group].inverters[inv].powerMin;
                    iv->powerLimit[0] = static_cast<uint16_t>(mCfg->groups[group].inverters[inv].limit * 10.0);
                    iv->powerLimit[1] = AbsolutNonPersistent;
                    if (iv->setDevControlRequest(ActivePowerContr)) {
                        mApp->triggerTickSend(iv->id);
                        mCfg->groups[group].inverters[inv].waitAck = 60;
                        mCfg->groups[group].inverters[inv].action = zeroExportAction_t::doNone;
                        mCfg->groups[group].inverters[inv].actionTimer = 0;
                        mCfg->groups[group].inverters[inv].actionTimestamp = millis();
                    }
                    sendLog();
                    clearLog();
                }
            }
        }
    }

    /** eventNewDataAvailable
     *
     * @param iv
     * @returns void
     */
    void eventNewDataAvailable(Inverter<> *iv) {
        if ((!mIsInitialized) || (!mCfg->enabled)) return;

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            zeroExportGroup_t *CfgGroup = &mCfg->groups[group];
            if (!CfgGroup->enabled) continue;

            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                zeroExportGroupInverter_t *CfgGroupInv = &CfgGroup->inverters[inv];
                if (!CfgGroupInv->enabled) continue;
                if (CfgGroupInv->id != iv->id) continue;

                _log.addProperty("g", group);
                _log.addProperty("i", inv);

                // TODO: Ist nach eventAckSetLimit verschoben
                // if (iv->actPowerLimit != 0xffff) {
                //    mLog["l"] = mCfg->groups[group].inverters[inv].limit;
                //    mCfg->groups[group].inverters[inv].limit = iv->actPowerLimit;
                //    mLog["lF"] = mCfg->groups[group].inverters[inv].limit;
                //}

                // TODO: Es dauert bis getMaxPower übertragen wird.
                if (iv->getMaxPower() > 0) {
                    CfgGroupInv->MaxPower = iv->getMaxPower();

                    _log.addProperty("pM", CfgGroupInv->MaxPower);
                }

                record_t<> *rec;
                rec = iv->getRecordStruct(RealTimeRunData_Debug);
                if (iv->getLastTs(rec) > (millis() - 15000)) {
                    CfgGroupInv->power = iv->getChannelFieldValue(CH0, FLD_PAC, rec);

                    _log.addProperty("pM", CfgGroupInv->MaxPower);

                    CfgGroupInv->dcVoltage = iv->getChannelFieldValue(CH1, FLD_UDC, rec);
                    _log.addProperty("bU", ah::round1(CfgGroupInv->dcVoltage));

                    // Batterieüberwachung - Überwachung über die DC-Spannung am PV-Eingang 1 des Inverters
                    if (CfgGroup->battCfg == zeroExportBatteryCfg::invUdc) {
                        if ((CfgGroup->battSwitch == false) && (CfgGroup->battValue < CfgGroupInv->dcVoltage)) {
                            CfgGroup->battValue = CfgGroupInv->dcVoltage;
                        }
                        if ((CfgGroup->battSwitch == true) && (CfgGroup->battValue > CfgGroupInv->dcVoltage)) {
                            CfgGroup->battValue = CfgGroupInv->dcVoltage;
                        }
                    }

                    // Fallschirm 2: Für nicht übernommene Limits bzw. nicht regelnde Inverter
                    // Bisher ist nicht geklärt ob der Inverter das Limit bestätigt hat
                    // Erstmalig aufgetreten bei @knickohr am 28.04.2024 ... l=300 pM=300, p=9
                    if (CfgGroupInv->MaxPower > 0) {
                        uint16_t limitPercent = 100 / CfgGroupInv->MaxPower * CfgGroupInv->limit;
                        uint16_t powerPercent = 100 / CfgGroupInv->MaxPower * CfgGroupInv->power;
                        uint16_t delta = abs(limitPercent - powerPercent);
                        if ((delta > 10) && (CfgGroupInv->power > 0)) {
                            _log.addProperty("delta", delta);
                            unsigned long delay = iv->getLastTs(rec) - CfgGroupInv->actionTimestamp;
                            _log.addProperty("delay", delay);
                            if (delay > 30000) {
                                CfgGroupInv->action = zeroExportAction_t::doActivePowerContr;
                                _log.addProperty("do", "doActivePowerContr");
                            }
                            if (delay > 60000) {
                                CfgGroupInv->action = zeroExportAction_t::doRestart;
                                _log.addProperty("do", "doRestart");
                            }
                        }
                    }
                }

                zeroExportQueue_t Entry;
                Entry.group = group;
                Entry.inv = inv;
                Entry.id = iv->id;
                putQueue(Entry);

                sendLog();
                clearLog();
                return;
            }
        }
        return;
    }

    /** onMqttConnect
     * Connect section
     * @returns void
     */
    void onMqttConnect(void) {
        if (!mCfg->enabled) return;

        mPowermeter.onMqttConnect();

        // "topic":"userdefined battSoCTopic" oder "userdefinedUTopic"
        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            if (!mCfg->groups[group].enabled) continue;

            if ((!mCfg->groups[group].battCfg == zeroExportBatteryCfg::mqttU) && (!mCfg->groups[group].battCfg == zeroExportBatteryCfg::mqttSoC)) continue;

            if (!strcmp(mCfg->groups[group].battTopic, "")) continue;

            mMqtt->subscribeExtern(String(mCfg->groups[group].battTopic).c_str(), QOS_2);
        }
    }

    /** onMqttMessage
     * Subscribe section
     * @param
     * @returns void
     */
    void onMqttMessage(JsonObject obj) {
        if (!mIsInitialized) return;

        mPowermeter.onMqttMessage(obj);

        String topic = String(obj["topic"]);

        // "topic":"userdefined battSoCTopic" oder "userdefinedUTopic"
        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            if (!mCfg->groups[group].enabled) continue;

            if ((!mCfg->groups[group].battCfg == zeroExportBatteryCfg::mqttU) && (!mCfg->groups[group].battCfg == zeroExportBatteryCfg::mqttSoC)) continue;

            if (!strcmp(mCfg->groups[group].battTopic, "")) continue;

            if (strcmp(mCfg->groups[group].battTopic, String(topic).c_str())) {
                mCfg->groups[group].battValue = (bool)obj["val"];

                _log.addProperty("k", mCfg->groups[group].battTopic);
                _log.addProperty("v", mCfg->groups[group].battValue);
            }
        }

        // "topic":"ctrl/zero"
        if (topic.indexOf("ctrl/zero") == -1) return;

        _log.addProperty("d", obj);

        if (obj["path"] == "ctrl" && obj["cmd"] == "zero") {
            int8_t topicGroup = getGroupFromTopic(topic.c_str());
            int8_t topicInverter = getInverterFromTopic(topic.c_str());

            if (topicGroup != -1) {
                _log.addProperty("g", topicGroup);
            }
            if (topicInverter == -1) {
                _log.addProperty("i", topicInverter);
            }

            _log.addProperty("k", topic);

            // "topic":"ctrl/zero/enabled"
            if (topic.indexOf("ctrl/zero/enabled") != -1) {
                _log.addProperty("v", (bool)obj["val"]);
                mCfg->enabled = (bool)obj["val"];
            }

            // "topic":"ctrl/zero/sleep"
            else if (topic.indexOf("ctrl/zero/sleep") != -1) {
                _log.addProperty("v", (bool)obj["val"]);
                mCfg->sleep = (bool)obj["val"];
            }

            else if ((topicGroup >= 0) && (topicGroup < ZEROEXPORT_MAX_GROUPS)) {
                String stopicGroup = String(topicGroup);

                // "topic":"ctrl/zero/groups/+/enabled"
                if (topic.endsWith("/enabled")) {
                    _log.addProperty("v", (bool)obj["val"]);
                    mCfg->groups[topicGroup].enabled = (bool)obj["val"];
                }

                // "topic":"ctrl/zero/groups/+/sleep"
                else if (topic.endsWith("/sleep")) {
                    _log.addProperty("v", (bool)obj["val"]);
                    mCfg->groups[topicGroup].sleep = (bool)obj["val"];
                }

                // Auf Eis gelegt, dafür 2 Gruppen mehr
                // 0.8.103008.2
                //                // "topic":"ctrl/zero/groups/+/pm_ip"
                //                if (topic.indexOf("ctrl/zero/groups/" + String(topicGroup) + "/pm_ip") != -1) {
                //                    snprintf(mCfg->groups[topicGroup].pm_src, ZEROEXPORT_GROUP_MAX_LEN_PM_SRC, "%s", obj[F("val")].as<const char *>());
                /// TODO:
                //                    snprintf(mCfg->groups[topicGroup].pm_src, ZEROEXPORT_GROUP_MAX_LEN_PM_SRC, "%s", obj[F("val")].as<const char *>());
                //                    strncpy(mCfg->groups[topicGroup].pm_src, obj[F("val")], ZEROEXPORT_GROUP_MAX_LEN_PM_SRC);
                //                    strncpy(mCfg->groups[topicGroup].pm_src, String(obj[F("val")]).c_str(), ZEROEXPORT_GROUP_MAX_LEN_PM_SRC);
                //                    snprintf(mCfg->groups[topicGroup].pm_src, ZEROEXPORT_GROUP_MAX_LEN_PM_SRC, "%s", String(obj[F("val")]).c_str());
                //                    mLog["k"] = "ctrl/zero/groups/" + String(topicGroup) + "/pm_ip";
                //                    mLog["v"] = mCfg->groups[topicGroup].pm_src;
                //                }
                //
                //                // "topic":"ctrl/zero/groups/+/pm_jsonPath"
                //                if (topic.indexOf("ctrl/zero/groups/" + String(topicGroup) + "/pm_jsonPath") != -1) {
                /// TODO:
                //                    snprintf(mCfg->groups[topicGroup].pm_jsonPath, ZEROEXPORT_GROUP_MAX_LEN_PM_JSONPATH, "%s", obj[F("val")].as<const char *>());
                //                    mLog["k"] = "ctrl/zero/groups/" + String(topicGroup) + "/pm_jsonPath";
                //                    mLog["v"] =  mCfg->groups[topicGroup].pm_jsonPath;
                //                }

                // "topic":"ctrl/zero/groups/+/battery/switch"
                else if (topic.endsWith("/battery/switch")) {
                    _log.addProperty("v", (bool)obj["val"]);
                    mCfg->groups[topicGroup].battSwitch = (bool)obj["val"];
                }

                else if (topic.indexOf("/advanced/") != -1) {
                    // "topic":"ctrl/zero/groups/+/advanced/setPoint"
                    if (topic.endsWith("/setPoint")) {
                        _log.addProperty("v", (int16_t)obj["val"]);
                        mCfg->groups[topicGroup].setPoint = (int16_t)obj["val"];
                    }

                    // "topic":"ctrl/zero/groups/+/advanced/powerTolerance"
                    else if (topic.endsWith("/powerTolerance")) {
                        _log.addProperty("v", (uint8_t)obj["val"]);
                        mCfg->groups[topicGroup].powerTolerance = (uint8_t)obj["val"];
                    }

                    // "topic":"ctrl/zero/groups/+/advanced/powerMax"
                    else if (topic.endsWith("/powerMax")) {
                        _log.addProperty("v", (uint16_t)obj["val"]);
                        mCfg->groups[topicGroup].powerMax = (uint16_t)obj["val"];
                    }
                } else if (topic.indexOf("/inverter/") != -1) {
                    if ((topicInverter >= 0) && (topicInverter < ZEROEXPORT_GROUP_MAX_INVERTERS)) {
                        // "topic":"ctrl/zero/groups/+/inverter/+/enabled"
                        if (topic.endsWith("/enabled")) {
                            _log.addProperty("v", (bool)obj["val"]);
                            mCfg->groups[topicGroup].inverters[topicInverter].enabled = (bool)obj["val"];
                        }

                        // "topic":"ctrl/zero/groups/+/inverter/+/powerMin"
                        else if (topic.endsWith("/powerMin")) {
                            _log.addProperty("v", (uint16_t)obj["val"]);
                            mCfg->groups[topicGroup].inverters[topicInverter].powerMin = (uint16_t)obj["val"];
                        }
                        // "topic":"ctrl/zero/groups/+/inverter/+/powerMax"
                        else if (topic.endsWith("/powerMax")) {
                            _log.addProperty("v", (uint16_t)obj["val"]);
                            mCfg->groups[topicGroup].inverters[topicInverter].powerMax = (uint16_t)obj["val"];
                        } else {
                            _log.addProperty("k", "error");
                        }
                    }
                } else {
                    _log.addProperty("k", "error");
                }
            }
        }

        sendLog();
        clearLog();
        return;
    }

   private:
    /** putQueue
     * Fügt einen Eintrag zur Queue hinzu.
     * @param item
     * @returns true/false
     */
    bool putQueue(zeroExportQueue_t item) {
        if ((mQueueIdxWrite + 1) % ZEROEXPORT_MAX_QUEUE_ENTRIES == mQueueIdxRead) return false;
        mQueue[mQueueIdxWrite] = item;
        mQueueIdxWrite = (mQueueIdxWrite + 1) % ZEROEXPORT_MAX_QUEUE_ENTRIES;
        return true;
    }

    /** getQueue
     * Holt einen Eintrag aus der Queue.
     * @param *value
     * @returns true/false
     */
    bool getQueue(zeroExportQueue_t *value) {
        if (mQueueIdxRead == mQueueIdxWrite) return false;
        *value = mQueue[mQueueIdxRead];
        mQueueIdxRead = (mQueueIdxRead + 1) % ZEROEXPORT_MAX_QUEUE_ENTRIES;
        return true;
    }

    /** getGroupFromTopic
     * Extahiert die Gruppe aus dem mqttTopic.
     * @param *topic
     * @returns group
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

        _log.addProperty("getGroupFromTopic", "group");
        return group;
    }

    /** getInverterFromTopic
     * Extrahiert dden Inverter aus dem mqttTopic
     * @param *topic
     * @returns inv
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

    /** mqttSubscribe
     * when a MQTT Msg is needed to subscribe, then a publish is leading
     * @param gr
     * @param payload
     * @returns void
     */
    void mqttSubscribe(String gr, String payload) {
        mqttPublish(gr, payload);
        mMqtt->subscribe(gr.c_str(), QOS_2);
    }

    /** mqttPublish
     * when a MQTT Msg is needed to Publish, but not to subscribe.
     * @param gr
     * @param payload
     * @param retain
     * @returns void
     */
    void mqttPublish(String gr, String payload, bool retain = false) {
        mMqtt->publish(gr.c_str(), payload.c_str(), retain);
    }

    /** sendLog
     * Sendet den LogSpeicher über Webserial und/oder MQTT
     */
    void sendLog(void) {
        // Log over Webserial
        if (mCfg->log_over_webserial) {
            DPRINTLN(DBG_INFO, String("ze: ") + _log.toString());
        }

        // Log over MQTT
        if (mCfg->log_over_mqtt) {
            if (mMqtt->isConnected()) {
                mMqtt->publish("zero/log", _log.toString().c_str(), false);
            }
        }
    }

    /** clearLog
     * Löscht den LogSpeicher
     */
    void clearLog(void) {
        _log.clear();
    }

    // private member variables
    bool mIsInitialized = false;

    // Maximale Größe des JSON-Dokuments
    static const size_t max_size = 5000;

    IApp *mApp = nullptr;
    uint32_t *mTimestamp = nullptr;
    zeroExport_t *mCfg = nullptr;
    settings_t *mConfig = nullptr;
    HMSYSTEM *mSys = nullptr;
    RestApiType *mApi = nullptr;

    zeroExportQueue_t mQueue[ZEROEXPORT_MAX_QUEUE_ENTRIES];
    uint8_t mQueueIdxWrite = 0;
    uint8_t mQueueIdxRead = 0;

    unsigned long mLastRun = 0;

    powermeter mPowermeter;

    PubMqttType *mMqtt = nullptr;
    bool mIsSubscribed = false;

    DynamicJsonHandler _log;
};

#endif /*__ZEROEXPORT__*/

#endif /* #if defined(PLUGIN_ZEROEXPORT) */
