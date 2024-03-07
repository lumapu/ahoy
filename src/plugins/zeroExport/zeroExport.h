#if defined(PLUGIN_ZEROEXPORT)

#ifndef __ZEROEXPORT__
#define __ZEROEXPORT__

#include <HTTPClient.h>
#include <string.h>
#include "AsyncJson.h"

#include "SML.h"

template <class HMSYSTEM>

// TODO: Anbindung an MQTT für Logausgabe zuzüglich DBG-Ausgabe in json. Deshalb alle Debugausgaben ersetzten durch json, dazu sollte ein jsonObject an die Funktion übergeben werden, zu dem die Funktion dann ihren Teil hinzufügt.
// TODO: Powermeter erweitern
// TODO: Der Teil der noch in app.pp steckt komplett hier in die Funktion verschieben.

class ZeroExport {

    public:

        ZeroExport() {
            mIsInitialized = false;
        }

        void setup(zeroExport_t *cfg, HMSYSTEM *sys, settings_t *config, RestApiType *api) {
            mCfg     = cfg;
            mSys     = sys;
            mConfig  = config;
            mApi     = api;

// TODO: Sicherheitsreturn weil noch Sicherheitsfunktionen fehlen.
//            mIsInitialized = true;
        }

        void loop(void) {
            if ((!mIsInitialized) || (!mCfg->enabled)) {
                return;
            }
// TODO: Sicherheitsreturn weil noch Sicherheitsfunktionen fehlen.
return;
            for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
                if (!mCfg->groups[group].enabled) {
                    continue;
                }

                switch (mCfg->groups[group].state) {
                    case zeroExportState::RESET:
                        mCfg->groups[group].lastRun = millis();
                        mCfg->groups[group].state = zeroExportState::GETPOWERMETER;
                        break;
                    case zeroExportState::GETPOWERMETER:
                        if ((millis() - mCfg->groups[group].lastRun) > (mCfg->groups[group].refresh * 1000UL)) {
                            if (getPowermeterWatts(group)) {
                                mCfg->groups[group].state = zeroExportState::FINISH;
                                mCfg->groups[group].stateNext = zeroExportState::GETINVERTERDATA;
                            } else {
                                // Wartezeit wenn Keine Daten vom Powermeter
                                mCfg->groups[group].lastRun = (millis() - (mCfg->groups[group].refresh * 100UL));
                                mCfg->groups[group].state = zeroExportState::FINISH;
                                mCfg->groups[group].stateNext = zeroExportState::RESET;
                            }
                        }
                        break;
                    case zeroExportState::GETINVERTERDATA:
                        if ((millis() - mCfg->groups[group].lastRun) > (mCfg->groups[group].refresh * 1000UL)) {
                            if (getInverterData(group)) {
                                mCfg->groups[group].state = zeroExportState::FINISH;
                                mCfg->groups[group].stateNext = zeroExportState::BATTERYPROTECTION;
                            } else {
                                // Wartezeit wenn Keine Daten von Wechselrichtern
                                mCfg->groups[group].lastRun = (millis() - (mCfg->groups[group].refresh * 100UL));
                                mCfg->groups[group].state = zeroExportState::FINISH;
                                mCfg->groups[group].stateNext = zeroExportState::RESET;
                            }
                        }
                        break;
                    case zeroExportState::BATTERYPROTECTION:
                        if ((millis() - mCfg->groups[group].lastRun) > (mCfg->groups[group].refresh * 1000UL)) {
                            if (batteryProtection(group)) {
                                mCfg->groups[group].state = zeroExportState::FINISH;
                                mCfg->groups[group].stateNext = zeroExportState::CONTROL;
                            } else {
                                // Wartezeit
                                mCfg->groups[group].lastRun = (millis() - (mCfg->groups[group].refresh * 100UL));
                                mCfg->groups[group].state = zeroExportState::FINISH;
                                mCfg->groups[group].stateNext = zeroExportState::RESET;
                            }
                        }
                        break;
                    case zeroExportState::CONTROL:
                        if ((millis() - mCfg->groups[group].lastRun) > (mCfg->groups[group].refresh * 1000UL)) {
                            if (controller(group)) {
                                mCfg->groups[group].state = zeroExportState::FINISH;
                                mCfg->groups[group].stateNext = zeroExportState::SETCONTROL;
                            } else {
                                // Wartezeit
                                mCfg->groups[group].lastRun = (millis() - (mCfg->groups[group].refresh * 100UL));
                                mCfg->groups[group].state = zeroExportState::FINISH;
                                mCfg->groups[group].stateNext = zeroExportState::RESET;
                            }
                        }
                        break;
                    case zeroExportState::SETCONTROL:
                        if ((millis() - mCfg->groups[group].lastRun) > (mCfg->groups[group].refresh * 1000UL)) {
                            if (setControl(group)) {
                                mCfg->groups[group].state = zeroExportState::FINISH;
                                mCfg->groups[group].stateNext = zeroExportState::RESET;
                            } else {
                                // Wartezeit
                                mCfg->groups[group].lastRun = (millis() - (mCfg->groups[group].refresh * 100UL));
                                mCfg->groups[group].state = zeroExportState::FINISH;
                                mCfg->groups[group].stateNext = zeroExportState::RESET;
                            }
                        }
                        break;
                    default:
                        DBGPRINT(String("ze: "));
                        DBGPRINTLN(mDocLog.as<String>());
                        mDocLog.clear();
                        if (mCfg->groups[group].stateNext != mCfg->groups[group].state) {
                            mCfg->groups[group].state = mCfg->groups[group].stateNext;
                        } else {
                            mCfg->groups[group].state = zeroExportState::RESET;
                            mCfg->groups[group].stateNext = zeroExportState::RESET;
                        }
                        break;
                }
            }
        }

        void tickerSecond() {
// TODO: Warten ob benötigt, ggf ein bit setzen, das in der loop() abgearbeitet wird.
            if ((!mIsInitialized) || (!mCfg->enabled)) {
                return;
            }
        }





/*
// TODO: Inverter sortieren nach Leistung
// -> Aufsteigend bei Leistungserhöhung
// -> Absteigend bei Leistungsreduzierung
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                if (!mCfg->groups[group].inverters[inv].enabled) {
                    continue;
                }
                if (mCfg->groups[group].inverters[inv].waitingTime) {
                    mCfg->groups[group].inverters[inv].waitingTime--;
                    continue;
                }
                // Leistung erhöhen
                if (mCfg->groups[group].power < mCfg->groups[group].powerLimitAkt - mCfg->groups[group].powerHyst) {
//                    mCfg->groups[group].powerLimitAkt = mCfg->groups[group].power


                    mCfg->groups[group].inverters[inv].waitingTime = ZEROEXPORT_DEF_INV_WAITINGTIME_MS;
                    return;
                }
                // Leistung reduzieren
                if (mCfg->groups[group].power > mCfg->groups[group].powerLimitAkt + mCfg->groups[group].powerHyst) {



                    mCfg->groups[group].inverters[inv].waitingTime = ZEROEXPORT_DEF_INV_WAITINGTIME_MS;
                    return;
                }



                if ((Power < PowerLimit - Hyst) || (Power > PowerLimit + Hyst)) {
                    if (Limit < 2%) {
                        setPower(Off);
                        setPowerLimit(100%)
                    } else {
                        setPower(On);
                        setPowerLimit(Limit);
                        mCfg->Inv[inv].waitingTime = ZEROEXPORT_DEF_INV_WAITINGTIME_MS;
                    }
                }



            }
*/

    private:
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
         * getPowermeterWatts
         * @param group
         * @returns true/false
         */
        bool getPowermeterWatts(uint8_t group) {
            zeroExportGroup_t *cfgGroup = &mCfg->groups[group];

            JsonObject logObj = mLog.createNestedObject("pm");
            logObj["grp"] = group;

            bool result = false;

            switch (cfgGroup->pm_type) {
                case 1:
                    result = getPowermeterWattsShelly(logObj, group);
                    break;
                case 2:
                    result = getPowermeterWattsTasmota(logObj, group);
                    break;
                case 3:
                    result = getPowermeterWattsMqtt(logObj, group);
                    break;
                case 4:
                    result = getPowermeterWattsHichi(logObj, group);
                    break;
                case 5:
                    result = getPowermeterWattsTibber(logObj, group);
                    break;
            }

            if (!result) {
                logObj["error"] = "type: " + String(cfgGroup->pm_type);
            }

            return result;
        }

        int getPowermeterWattsShelly(JsonObject logObj, uint8_t group) {
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

        int getPowermeterWattsTasmota(JsonObject logObj, uint8_t group) {
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

        int getPowermeterWattsMqtt(JsonObject logObj, uint8_t group) {
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

        int getPowermeterWattsHichi(JsonObject logObj, uint8_t group) {
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

        int getPowermeterWattsTibber(JsonObject logObj, uint8_t group) {
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

        // Inverter

        bool getInverterData(uint8_t group) {
            zeroExportGroup_t *cfgGroup = &mCfg->groups[group];

            JsonObject logObj = mLog.createNestedObject("iv");
            logObj["grp"] = group;

            bool ret = false;

            long int bTsp = millis();

            JsonArray logArrInv = logObj.createNestedArray("iv");

            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                zeroExportGroupInverter_t *cfgGroupInv = &cfgGroup->inverters[inv];

                JsonObject logObjInv = logArrInv.createNestedObject();
                logObjInv["iv"] = inv;

                // Wenn Inverter deaktiviert -> Eintrag ignorieren
                logObjInv["en"] = cfgGroupInv->enabled;
                if (!cfgGroupInv->enabled) {
                    continue;
                }
                if (cfgGroupInv->id <= 0) {
                    logObjInv["err"] = "WR aktiviert aber nicht ausgewählt";
                    return false;
                }

                // Daten holen
                Inverter<> *iv;
                record_t<> *rec;
// TODO: getInverterById
                for (uint8_t i = 0; i < mSys->getNumInverters(); i++) {
                    iv = mSys->getInverterByPos(i);

                    // Wenn kein Inverter -> ignorieren
                    if (iv == NULL) {
                        continue;
                    }

                    // Wenn falscher Inverter -> ignorieren
                    if (iv->id != (uint8_t)cfgGroupInv->id) {
                        continue;
                    }

                    // wenn Inverter deaktiviert -> Daten ignorieren
//                    logObjInv["cfgEnabled"] = iv->enabled();
//                    if (!iv->enabled()) {
//                        continue;
//                    }
                    // wenn Inverter nicht verfügbar -> Daten ignorieren
                    logObjInv["Available"] = iv->isAvailable();
                    if (!iv->isAvailable()) {
                        continue;;
                    }
                    // wenn Inverter nicht produziert -> Daten ignorieren
                    logObjInv["Producing"] = iv->isProducing();
                    if (!iv->isProducing()) {
                        continue;;
                    }

                    // Daten abrufen
                    rec = iv->getRecordStruct(RealTimeRunData_Debug);

// TODO: gibts hier nen Timestamp? Wenn die Daten nicht aktueller sind als beim letzten Durchlauf dann brauch ich nix machen

                    cfgGroupInv->power = iv->getChannelFieldValue(CH0, FLD_PAC, rec);
                    logObjInv["P_ac"] = cfgGroupInv->power;

//mCfg->groups[group].inverters[inv].limit = iv->actPowerLimit;

//                    cfgGroupInv->limitAck = iv->powerLimitAck;

                    cfgGroupInv->dcVoltage = iv->getChannelFieldValue(CH1, FLD_UDC, rec);
                    logObjInv["U_dc"] = cfgGroupInv->dcVoltage;
// TODO: Eingang muss konfigurierbar sein

                    // ACK
                    if (cfgGroupInv->limitTsp != 0) {
                        if (iv->powerLimitAck) {
                            iv->powerLimitAck = false;
                            cfgGroupInv->limitTsp = 0;
                        }
                        if ((millis() + 10000) > cfgGroupInv->limitTsp) {
                            cfgGroupInv->limitTsp = 0;
                        }
                    }

                    ret = true;
                }
            }

            long int eTsp = millis();
            logObj["b"] = bTsp;
            logObj["e"] = eTsp;
            logObj["d"] = eTsp - bTsp;

            return ret;
        }

        // Battery

        /**
         * batteryProtection
         * @param uint8_t group
         * @returns bool true
         */
        bool batteryProtection(uint8_t group) {
            zeroExportGroup_t *cfgGroup = &mCfg->groups[group];
// TODO: Wenn kein WR gefunden wird, wird nicht abgeschaltet!!!
            JsonObject logObj = mLog.createNestedObject("bp");
            logObj["grp"] = group;

            long int bTsp = millis();

            if (cfgGroup->battEnabled) {
                logObj["enabled"] = 1;

                // Config - parameter check
                if (cfgGroup->battVoltageOn <= cfgGroup->battVoltageOff) {
                    cfgGroup->battSwitch = false;
                    logObj["error"] = "Config - battVoltageOn(" + (String)cfgGroup->battVoltageOn + ") <= battVoltageOff(" + (String)cfgGroup->battVoltageOff + ")";
                    return true;
                }

                // Config - parameter check
                if (cfgGroup->battVoltageOn <= (cfgGroup->battVoltageOff + 1)) {
                    cfgGroup->battSwitch = false;
                    logObj["error"] = "Config - battVoltageOn(" + (String)cfgGroup->battVoltageOn + ") <= battVoltageOff(" + (String)cfgGroup->battVoltageOff + " + 1V)";
                    return true;
                }

                // Config - parameter check
                if (cfgGroup->battVoltageOn <= 22) {
                    cfgGroup->battSwitch = false;
                    logObj["error"] = "Config - battVoltageOn(" + (String)cfgGroup->battVoltageOn + ") <= 22V)";
                    return true;
                }

                JsonArray logArrInv = logObj.createNestedArray("iv");

                for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                    zeroExportGroupInverter_t *cfgGroupInv = &cfgGroup->inverters[inv];

                    JsonObject logObjInv = logArrInv.createNestedObject();
                    logObjInv["iv"] = inv;

                    // Ignore disabled Inverter
                    if (!cfgGroupInv->enabled) {
                        continue;
                    }
                    if (cfgGroupInv->id <= 0) {
                        continue;
                    }

                    logObjInv["battU"] = cfgGroupInv->dcVoltage;

                    // Switch to ON
                    if (cfgGroupInv->dcVoltage > cfgGroup->battVoltageOn) {
                        cfgGroup->battSwitch = true;
                        continue;
                    }

                    // Switch to OFF
                    if (cfgGroupInv->dcVoltage < cfgGroup->battVoltageOff) {
                        cfgGroup->battSwitch = false;
                        return true;
                    }

                }
            } else {
                logObj["enabled"] = 0;

                cfgGroup->battSwitch = true;
            }

            logObj["switch"] = cfgGroup->battSwitch;

            long int eTsp = millis();
            logObj["b"] = bTsp;
            logObj["e"] = eTsp;
            logObj["d"] = eTsp - bTsp;

            return true;
        }

        // Controller

        /**
         * controller
         * @param group
         * @returns true/false
         * Die Funktion berechnet alle Regelabweichungen und speichert die nötigen Korrekturen
         */
        bool controller(uint8_t group) {
            zeroExportGroup_t *cfgGroup = &mCfg->groups[group];

            JsonObject logObj = mLog.createNestedObject("co");
            logObj["grp"] = group;

            long int bTsp = millis();

            // Führungsgröße w in Watt
            float w_Sum = cfgGroup->setPoint;
            float w_L1  = cfgGroup->setPoint / 3;
            float w_L2  = cfgGroup->setPoint / 3;
            float w_L3  = cfgGroup->setPoint / 3;

            logObj["w_P"] = w_Sum;
            logObj["w_P1"]  = w_L1;
            logObj["w_P2"]  = w_L2;
            logObj["w_P3"]  = w_L3;

            // Regelgröße x in Watt
            float x_Sum = cfgGroup->pmPower;
            float x_L1  = cfgGroup->pmPowerL1;
            float x_L2  = cfgGroup->pmPowerL2;
            float x_L3  = cfgGroup->pmPowerL3;

            logObj["x_P"] = x_Sum;
            logObj["x_P1"]  = x_L1;
            logObj["x_P2"]  = x_L2;
            logObj["x_P3"]  = x_L3;

            // Regelabweichung e in Watt
            float e_Sum = 0 - (w_Sum - x_Sum);
            float e_L1  = 0 - (w_L1  - x_L1);
            float e_L2  = 0 - (w_L2  - x_L2);
            float e_L3  = 0 - (w_L3  - x_L3);

            logObj["e_P"] = e_Sum;
            logObj["e_P1"]  = e_L1;
            logObj["e_P2"]  = e_L2;
            logObj["e_P3"]  = e_L3;

            // Regler
// TODO: Regelparameter unter Advanced konfigurierbar? Aber erst wenn Regler komplett ingegriert.
            const float Kp = 1;
            const float Ki = 1;
            const float Kd = 1;

//            unsigned long tsp = millis();

            // - P-Anteil
            float yP_Sum = Kp * e_Sum;
            float yP_L1  = Kp * e_L1;
            float yP_L2  = Kp * e_L2;
            float yP_L3  = Kp * e_L3;
            // - I-Anteil
//            float esum = esum + e;
//            float yI = Ki * Ta * esum;
            float yI_Sum = 0;
            float yI_L1 = 0;
            float yI_L2 = 0;
            float yI_L3 = 0;
            // - D-Anteil
//            float yD = Kd * (e -ealt) / Ta;
//            float ealt = e;
            float yD_Sum = 0;
            float yD_L1 = 0;
            float yD_L2 = 0;
            float yD_L3 = 0;
            // - PID-Anteil
            float yPID_Sum = yP_Sum + yI_Sum + yD_Sum;
            float yPID_L1  = yP_L1 + yI_L1 + yD_L1;
            float yPID_L2  = yP_L2 + yI_L2 + yD_L2;
            float yPID_L3  = yP_L3 + yI_L3 + yD_L3;

            // Regelbegrenzung
// TODO: Hier könnte man den maximalen Sprung begrenzen

            logObj["yPID_P"] = yPID_Sum;
            logObj["yPID_P1"]  = yPID_L1;
            logObj["yPID_P2"]  = yPID_L2;
            logObj["yPID_P3"]  = yPID_L3;

            cfgGroup->grpPower   += yPID_Sum;
            cfgGroup->grpPowerL1 += yPID_L1;
            cfgGroup->grpPowerL2 += yPID_L2;
            cfgGroup->grpPowerL3 += yPID_L3;

            long int eTsp = millis();
            logObj["b"] = bTsp;
            logObj["e"] = eTsp;
            logObj["d"] = eTsp - bTsp;

            return true;
        }

        /**
         * setControl
         * @param group
         * @returns true/false
         * Die Funktion liest alle gespeicherten Aufgaben und arbeitet diese in der korrekten Reihenfolge ab.
         */
        bool setControl(uint8_t group) {

            zeroExportGroup_t *cfgGroup = &mCfg->groups[group];

            JsonObject logObj = mLog.createNestedObject("sl");
            logObj["grp"] = group;

//            bool ret = true;

            long int bTsp = millis();

//            JsonArray logArrInv = logObj.createNestedArray("iv");
//            unsigned long tsp = millis();

            float deltaY_Sum = cfgGroup->grpPower;
            float deltaY_L1  = cfgGroup->grpPowerL1;
            float deltaY_L2  = cfgGroup->grpPowerL2;
            float deltaY_L3  = cfgGroup->grpPowerL3;

            logObj["y_P"] = cfgGroup->grpPower;
            logObj["y_P1"]  = cfgGroup->grpPowerL1;
            logObj["y_P2"]  = cfgGroup->grpPowerL2;
            logObj["y_P3"]  = cfgGroup->grpPowerL3;

            bool grpTarget[7] = {false, false, false, false, false, false, false};
            uint8_t ivId_Pmin[7] = {0, 0, 0, 0, 0, 0, 0};
            uint8_t ivId_Pmax[7] = {0, 0, 0, 0, 0, 0, 0};
            uint16_t ivPmin[7] = {65535, 65535, 65535, 65535, 65535, 65535, 65535};
            uint16_t ivPmax[7] = {0, 0, 0, 0, 0, 0, 0};

            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                zeroExportGroupInverter_t *cfgGroupInv = &cfgGroup->inverters[inv];

                if (!cfgGroupInv->enabled) {
                    continue;
                }

// TODO: überspringen wenn keine Freigabe?
                if (cfgGroupInv->limitTsp != 0) {
                    continue;
                }

                if (cfgGroupInv->power < ivPmin[cfgGroupInv->target]) {
                    grpTarget[cfgGroupInv->target] = true;
                    ivPmin[cfgGroupInv->target] = cfgGroupInv->power;
                    ivId_Pmin[cfgGroupInv->target] = inv;
                    // Hier kein return oder continue sonst dauerreboot
                }
                if (cfgGroupInv->power > ivPmax[cfgGroupInv->target]) {
                    grpTarget[cfgGroupInv->target] = true;
                    ivPmax[cfgGroupInv->target] = cfgGroupInv->power;
                    ivId_Pmax[cfgGroupInv->target] = inv;
                    // Hier kein return oder continue sonst dauerreboot
                }
            }
            for (uint8_t i = 0; i < 7; i++) {
                logObj[String(i)] = String(i) + String(" grpTarget: ") + String(grpTarget[i]) + String(": ivPmin: ") + String(ivPmin[i]) + String(": ivPmax: ") + String(ivPmax[i]) + String(": ivId_Pmin: ") + String(ivId_Pmin[i]) + String(": ivId_Pmax: ") + String(ivId_Pmax[i]);
            }

            for (uint8_t i = 7; i > 0; --i) {
                if (!grpTarget[i]) {
                    continue;
                }
                logObj[String(String("10")+String(i))] = String(i) + String(" grpTarget: " + String(grpTarget[i]));

                float *deltaP;
                switch(i) {
                    case 6:
                    case 3:
                        deltaP = &cfgGroup->grpPowerL3;
                        break;
                    case 5:
                    case 2:
                        deltaP = &cfgGroup->grpPowerL2;
                        break;
                    case 4:
                    case 1:
                        deltaP = &cfgGroup->grpPowerL1;
                        break;
                    case 0:
                        deltaP = &cfgGroup->grpPower;
                        break;
                }

                // Leistung erhöhen
                if (*deltaP > 0) {
                    logObj["+deltaP"] = *deltaP;
                    zeroExportGroupInverter_t *cfgGroupInv = &cfgGroup->inverters[ivId_Pmin[i]];
                    cfgGroupInv->limitNew = cfgGroupInv->limit + *deltaP;
                    if (i != 0) {
                        cfgGroup->grpPower - *deltaP;
                    }
                    *deltaP = 0;
                    if (cfgGroupInv->limitNew > cfgGroupInv->powerMax) {
                        *deltaP = cfgGroupInv->limitNew - cfgGroupInv->powerMax;
                        cfgGroupInv->limitNew = cfgGroupInv->powerMax;
                        if (i != 0) {
                            cfgGroup->grpPower + *deltaP;
                        }
                    }
                    setLimit(&logObj, group, ivId_Pmin[i]);
                    continue;
                }

                // Leistung reduzieren
                if (*deltaP < 0) {
                    logObj["-deltaP"] = *deltaP;
                    zeroExportGroupInverter_t *cfgGroupInv = &cfgGroup->inverters[ivId_Pmax[i]];
                    cfgGroupInv->limitNew = cfgGroupInv->limit - *deltaP;
                    if (i != 0) {
                        cfgGroup->grpPower - *deltaP;
                    }
                    *deltaP = 0;
                    if (cfgGroupInv->limitNew < cfgGroupInv->powerMin) {
                        *deltaP = cfgGroupInv->limitNew - cfgGroupInv->powerMin;
                        cfgGroupInv->limitNew = cfgGroupInv->powerMin;
                        if (i != 0) {
                            cfgGroup->grpPower + *deltaP;
                        }
                    }
                    setLimit(&logObj, group, ivId_Pmax[i]);
                    continue;
                }

            }

//            if (ret) {
//                logObj["todo"] = "- nothing todo - ";
//            }

            long int eTsp = millis();
            logObj["b"] = bTsp;
            logObj["e"] = eTsp;
            logObj["d"] = eTsp - bTsp;

//            return ret;
            return true;
        }










// TODO: Hier folgen Unterfunktionen für SetControl die Erweitert werden müssen
// setLimit, checkLimit
// setPower, checkPower
// setReboot, checkReboot

        bool setLimit(JsonObject *objlog, uint8_t group, uint8_t inv) {
            zeroExportGroupInverter_t *cfgGroupInv = &mCfg->groups[group].inverters[inv];

            JsonObject objLog = objlog->createNestedObject("setLimit");
            objLog["grp"] = group;
            objLog["iv"] = inv;

            // Reject limit if difference < 5 W
//            if ((cfgGroupInv->limitNew > cfgGroupInv->limit - 5) && (cfgGroupInv->limitNew < cfgGroupInv->limit + 5)) {
// TODO: 5W Toleranz konfigurierbar?
//                objLog["error"] = "Diff < 5W";
//                return true;
//            }

            cfgGroupInv->limit = cfgGroupInv->limitNew;
            cfgGroupInv->limitTsp = millis();

            objLog["P"] = cfgGroupInv->limit;
            objLog["tsp"] = cfgGroupInv->limitTsp;

            // Limit übergeben
            DynamicJsonDocument doc(512);
            JsonObject obj = doc.to<JsonObject>();
            obj["val"] = cfgGroupInv->limit;
            obj["id"] = cfgGroupInv->id;
            obj["path"] = "ctrl";
            obj["cmd"] = "limit_nonpersistent_absolute";
            mApi->ctrlRequest(obj);

            objLog["data"] = obj;

            return true;
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