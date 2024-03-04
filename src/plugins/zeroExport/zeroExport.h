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
                                mCfg->groups[group].state = zeroExportState::GETINVERTERDATA;
                            } else {
                                // Wartezeit wenn Keine Daten vom Powermeter
                                mCfg->groups[group].lastRun = (millis() - (mCfg->groups[group].refresh * 100UL));
                            }
                        }
                        break;
                    case zeroExportState::GETINVERTERDATA:
                        if ((millis() - mCfg->groups[group].lastRun) > (mCfg->groups[group].refresh * 1000UL)) {
                            if (getInverterData(group)) {
                                mCfg->groups[group].state = zeroExportState::BATTERYPROTECTION;
                            } else {
                                // Wartezeit wenn Keine Daten von Wechselrichtern
                                mCfg->groups[group].lastRun = (millis() - (mCfg->groups[group].refresh * 100UL));
                            }
                        }
                        break;
                    case zeroExportState::BATTERYPROTECTION:
                        if (batteryProtection(group)) {
                            mCfg->groups[group].state = zeroExportState::CONTROL;
                        //} else {
                            // Wartezeit
                        }
                        break;
                    case zeroExportState::CONTROL:
                        if (controller(group)) {
                            mCfg->groups[group].state = zeroExportState::SETCONTROL;
                        //} else {
                            // Wartezeit
                        }
                        break;
                    case zeroExportState::SETCONTROL:
                        if (setControl(group)) {
                            mCfg->groups[group].state = zeroExportState::WAITACCEPT;
                        //} else {
                            // Wartezeit
                        }
                        break;
                    case zeroExportState::WAITACCEPT:
                        if (waitAccept(group)) {
                            mCfg->groups[group].state = zeroExportState::FINISH;
                        //} else {
                            // Wartezeit
                        }
                        break;
                    default:
                        DBGPRINT(String("ze: "));
                        DBGPRINTLN(mDocLog.as<String>());
                        mDocLog.clear();
                        mCfg->groups[group].state = zeroExportState::RESET;
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

        bool getPowermeterWatts(uint8_t group) {
            JsonObject logObj = mLog.createNestedObject("getPowermeterWatts");
            logObj["grp"] = group;

            bool ret = false;

            switch (mCfg->groups[group].pm_type) {
                case 1:
                    ret = getPowermeterWattsShelly(logObj, group);
                    break;
                case 2:
                    ret = getPowermeterWattsTasmota(logObj, group);
                    break;
//                case 3:
//                    ret = getPowermeterWattsMqtt(logObj, group);
//                    break;
//                case 4:
//                    ret = getPowermeterWattsHichi(logObj, group);
//                    break;
//                case 5:
//                    ret = getPowermeterWattsTibber(logObj, group);
//                    break;
            }

            return ret;
        }

        int getPowermeterWattsShelly(JsonObject logObj, uint8_t group) {
            bool ret = false;

            HTTPClient http;
//            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            http.setUserAgent("Ahoy-Agent");
// TODO: Ahoy-0.8.850024-zero
//            http.setConnectTimeout(1000);
//            http.setTimeout(1000);
//            http.addHeader("Content-Type", "application/json");
//            http.addHeader("Accept", "application/json");
// TODO: Erst aktivieren wenn Timing klar ist, Timeout reduzieren.
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

                // Shelly 3EM
                if (doc.containsKey(F("total_power"))) {
                    mCfg->groups[group].pmPower = doc["total_power"];
                    ret = true;
                // Shelly pro 3EM
                } else if (doc.containsKey(F("em:0"))) {
                    mCfg->groups[group].pmPower = doc["em:0"]["total_act_power"];
                    ret = true;
                // Keine Daten
                } else {
                    mCfg->groups[group].pmPower = 0;
                }

                // Shelly 3EM
                if (doc.containsKey(F("emeters"))) {
                    mCfg->groups[group].pmPowerL1 = doc["emeters"][0]["power"];
                    ret = true;
                // Shelly pro 3EM
                } else if (doc.containsKey(F("em:0"))) {
                    mCfg->groups[group].pmPowerL1 = doc["em:0"]["a_act_power"];
                    ret = true;
                // Shelly plus1pm plus2pm
                } else if (doc.containsKey(F("switch:0"))) {
                    mCfg->groups[group].pmPowerL1 = doc["switch:0"]["apower"];
                    mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL1;
                    ret = true;
                // Shelly Alternative
                } else if (doc.containsKey(F("apower"))) {
                    mCfg->groups[group].pmPowerL1 = doc["apower"];
                    mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL1;
                    ret = true;
                // Keine Daten
                } else {
                    mCfg->groups[group].pmPowerL1 = 0;
                }

                // Shelly 3EM
                if (doc.containsKey(F("emeters"))) {
                    mCfg->groups[group].pmPowerL2 = doc["emeters"][1]["power"];
                    ret = true;
                // Shelly pro 3EM
                } else if (doc.containsKey(F("em:0"))) {
                    mCfg->groups[group].pmPowerL2 = doc["em:0"]["b_act_power"];
                    ret = true;
                // Shelly plus1pm plus2pm
                } else if (doc.containsKey(F("switch:1"))) {
                    mCfg->groups[group].pmPowerL2 = doc["switch.1"]["apower"];
                    mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL2;
                    ret = true;
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
                    ret = true;
                // Shelly pro 3EM
                } else if (doc.containsKey(F("em:0"))) {
                    mCfg->groups[group].pmPowerL3 = doc["em:0"]["c_act_power"];
                    ret = true;
                // Shelly plus1pm plus2pm
                } else if (doc.containsKey(F("switch:2"))) {
                    mCfg->groups[group].pmPowerL3 = doc["switch:2"]["apower"];
                    mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL3;
                    ret = true;
//                // Shelly Alternative
//                } else if (doc.containsKey(F("apower"))) {
//                    mCfg->groups[group].pmPowerL3 = doc["apower"];
//                    mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL3;
//                    ret = true;
                // Keine Daten
                } else {
                    mCfg->groups[group].pmPowerL3 = 0;
                }

                logObj["pmPower"]   = mCfg->groups[group].pmPower;
                logObj["pmPowerL1"] = mCfg->groups[group].pmPowerL1;
                logObj["pmPowerL2"] = mCfg->groups[group].pmPowerL2;
                logObj["pmPowerL3"] = mCfg->groups[group].pmPowerL3;
            }
            http.end();

            return ret;
        }

        int getPowermeterWattsTasmota(JsonObject logObj, uint8_t group) {
// TODO: nicht komplett
            bool ret = false;

            HTTPClient http;
//            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            http.setUserAgent("Ahoy-Agent");
// TODO: Ahoy-0.8.850024-zero
//            http.setConnectTimeout(1000);
//            http.setTimeout(1000);
//            http.addHeader("Content-Type", "application/json");
//            http.addHeader("Accept", "application/json");
// TODO: Erst aktivieren wenn Timing klar ist, Timeout reduzieren.
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

                logObj["pmPower"]   = mCfg->groups[group].pmPower;
                logObj["pmPowerL1"] = mCfg->groups[group].pmPowerL1;
                logObj["pmPowerL2"] = mCfg->groups[group].pmPowerL2;
                logObj["pmPowerL3"] = mCfg->groups[group].pmPowerL3;
            }
            http.end();

            return ret;
        }

        int getPowermeterWattsMqtt(JsonObject logObj, uint8_t group) {
// TODO: nicht komplett
            bool ret = false;



//                logObj["pmPower"]   = mCfg->groups[group].pmPower;
//                logObj["pmPowerL1"] = mCfg->groups[group].pmPowerL1;
//                logObj["pmPowerL2"] = mCfg->groups[group].pmPowerL2;
//                logObj["pmPowerL3"] = mCfg->groups[group].pmPowerL3;
//            }
//            http.end();

            return ret;
        }

        int getPowermeterWattsHichi(JsonObject logObj, uint8_t group) {
// TODO: nicht komplett
            bool ret = false;



//                logObj["pmPower"]   = mCfg->groups[group].pmPower;
//                logObj["pmPowerL1"] = mCfg->groups[group].pmPowerL1;
//                logObj["pmPowerL2"] = mCfg->groups[group].pmPowerL2;
//                logObj["pmPowerL3"] = mCfg->groups[group].pmPowerL3;
//            }
//            http.end();

            return ret;
        }

        int getPowermeterWattsTibber(JsonObject logObj, uint8_t group) {
// TODO: nicht komplett
            bool ret = false;



//                logObj["pmPower"]   = mCfg->groups[group].pmPower;
//                logObj["pmPowerL1"] = mCfg->groups[group].pmPowerL1;
//                logObj["pmPowerL2"] = mCfg->groups[group].pmPowerL2;
//                logObj["pmPowerL3"] = mCfg->groups[group].pmPowerL3;
//            }
//            http.end();

            return ret;
        }

        // Inverter

        bool getInverterData(uint8_t group) {
            JsonObject logObj = mLog.createNestedObject("getInverterPowerWatts");
            logObj["grp"] = group;

            bool ret = false;

            JsonArray logArrInv = logObj.createNestedArray("iv");

            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                JsonObject logObjInv = logArrInv.createNestedObject();
                logObjInv["iv"] = inv;

                // Wenn Inverter deaktiviert -> Eintrag ignorieren
                if (!mCfg->groups[group].inverters[inv].enabled) {
                    continue;
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
                    if (iv->id != (uint8_t)mCfg->groups[group].inverters[inv].id) {
                        continue;
                    }

                    // wenn Inverter deaktiviert -> Daten ignorieren
//                    if (!iv->enabled()) {
//DBGPRINT(String(" aber deaktiviert "));
//                        continue;
//                    }
                    // wenn Inverter nicht verfügbar -> Daten ignorieren
                    if (!iv->isAvailable()) {
DBGPRINT(String(" aber nicht erreichbar "));
                        continue;
                    }
                    // wenn Inverter nicht produziert -> Daten ignorieren
//                    if (!iv->isProducing()) {
//DBGPRINT(String(" aber produziert nix "));
//                        continue;
//                    }
                    // Daten abrufen
                    rec = iv->getRecordStruct(RealTimeRunData_Debug);

// TODO: gibts hier nen Timestamp? Wenn die Daten nicht aktueller sind als beim letzten Durchlauf dann brauch ich nix machen

                    mCfg->groups[group].inverters[inv].power = iv->getChannelFieldValue(CH0, FLD_PAC, rec);
                    logObjInv["P_ac"] = mCfg->groups[group].inverters[inv].power;

//mCfg->groups[group].inverters[inv].limit = iv->actPowerLimit;
//DBGPRINT(String("Li="));
//DBGPRINT(String(mCfg->groups[group].inverters[inv].limit));
//DBGPRINT(String("% "));

                    mCfg->groups[group].inverters[inv].limitAck = iv->powerLimitAck;
//DBGPRINT(String("Ack= "));
//DBGPRINT(String(mCfg->groups[group].inverters[inv].limitAck));
//DBGPRINT(String(" "));

                    mCfg->groups[group].inverters[inv].dcVoltage = iv->getChannelFieldValue(CH1, FLD_UDC, rec);
                    logObjInv["U_dc"] = mCfg->groups[group].inverters[inv].dcVoltage;
// TODO: Eingang muss konfigurierbar sein

                    ret = true;
                }
            }

            return ret;
        }

        // Battery

        bool batteryProtection(uint8_t group) {
            JsonObject logObj = mLog.createNestedObject("batteryProtection");
            logObj["grp"] = group;
            bool ret = false;
DBGPRINT(String("batteryProtection: ("));
DBGPRINT(String(group));
DBGPRINT(String("): "));
            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
DBGPRINT(String("iv: "));
DBGPRINT(String(inv));
DBGPRINT(String(" "));
                if (mCfg->groups[group].battEnabled) {
                    // Konfigurationstest
                    if (mCfg->groups[group].battVoltageOn <= mCfg->groups[group].battVoltageOff) {
                        mCfg->groups[group].battSwitch = false;
DBGPRINT(String("Konfigurationsfehler: battVoltageOn ist < battVoltageOff"));
                        return false;
                    }
                    // Konfigurationstest
                    if (mCfg->groups[group].battVoltageOn >= (mCfg->groups[group].battVoltageOff + 1)) {
                        mCfg->groups[group].battSwitch = false;
DBGPRINT(String("Konfigurationsfehler: battVoltageOn muss >= (battVoltageOff + 1)"));
                        return false;
                    }
                    // Konfigurationstest
                    if (mCfg->groups[group].battVoltageOn <= 22) {
                        mCfg->groups[group].battSwitch = false;
DBGPRINT(String("Konfigurationsfehler: battVoltageOn ist <= 22"));
                        return false;
                    }
                    // Einschalten
                    if (mCfg->groups[group].inverters[inv].dcVoltage > mCfg->groups[group].battVoltageOn) {
                        mCfg->groups[group].battSwitch = true;
                        ret = true;
DBGPRINT(String("Einschalten"));
                        continue;
                    }
                    // Ausschalten
                    if (mCfg->groups[group].inverters[inv].dcVoltage < mCfg->groups[group].battVoltageOff) {
                        mCfg->groups[group].battSwitch = false;
DBGPRINT(String("Ausschalten"));
                        return true;
                    }
                } else {
                    mCfg->groups[group].battSwitch = false;
                    ret = true;
                }
            }
DBGPRINTLN(String(""));
            return ret;
        }

        // Controller

        bool controller(uint8_t group) {
// TODO: Die Funktion controller() soll die Regelabweichung berechnen und alle nötigen Korrekuren speichern
            JsonObject logObj = mLog.createNestedObject("controller");
            logObj["grp"] = group;

// TODO: Regelparameter unter Advanced konfigurierbar? Aber erst wenn Regler komplett ingegriert.
            const float Kp = 1;
            const float Ki = 1;
            const float Kd = 1;

            unsigned long tsp = millis();

            float xSum = 0;
            float xL1  = 0;
            float xL2  = 0;
            float xL3  = 0;

            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {

                // Wenn Inverter disabled -> überspringen
                if (!mCfg->groups[group].inverters[inv].enabled) {
                    continue;
                }

// TODO: Wenn der Inverter nicht produziert -> überspringen

                // Istwert
                if (mCfg->groups[group].inverters[inv].target == 0) {
                    // Sum
                    xSum += mCfg->groups[group].inverters[inv].limit;
                }
                if (mCfg->groups[group].inverters[inv].target == 1) {
                    // L1
                    xL1 += mCfg->groups[group].inverters[inv].limit;
                }
                if (mCfg->groups[group].inverters[inv].target == 2) {
                    // L2
                    xL2 += mCfg->groups[group].inverters[inv].limit;
                }
                if (mCfg->groups[group].inverters[inv].target == 3) {
                    // L3
                    xL3 += mCfg->groups[group].inverters[inv].limit;
                }
                if (mCfg->groups[group].inverters[inv].target == 4) {
                    // L1 + Sum
                    xSum += mCfg->groups[group].inverters[inv].limit;
                    xL1 += mCfg->groups[group].inverters[inv].limit;
                }
                if (mCfg->groups[group].inverters[inv].target == 5) {
                    // L2 + Sum
                    xSum += mCfg->groups[group].inverters[inv].limit;
                    xL2 += mCfg->groups[group].inverters[inv].limit;
                }
                if (mCfg->groups[group].inverters[inv].target == 6) {
                    // L3 + Sum
                    xSum += mCfg->groups[group].inverters[inv].limit;
                    xL3 += mCfg->groups[group].inverters[inv].limit;
                }
            }
            logObj["xSum"] = xSum;
            logObj["xL1"] = xL1;
            logObj["xL2"] = xL2;
            logObj["xL3"] = xL3;

            // Regelabweichung Sum
            float e_Sum = mCfg->groups[group].pmPower - mCfg->groups[group].setPoint;
            float e_L1  = mCfg->groups[group].pmPowerL1 - (mCfg->groups[group].setPoint / 3);
            float e_L2  = mCfg->groups[group].pmPowerL2 - (mCfg->groups[group].setPoint / 3);
            float e_L3  = mCfg->groups[group].pmPowerL3 - (mCfg->groups[group].setPoint / 3);

            logObj["e_Sum"] = e_Sum;
            logObj["e_L1"]  = e_L1;
            logObj["e_L2"]  = e_L2;
            logObj["e_L3"]  = e_L3;

            // Regler
            // P-Anteil
            float yP_Sum = Kp * e_Sum;
            float yP_L1  = Kp * e_L1;
            float yP_L2  = Kp * e_L2;
            float yP_L3  = Kp * e_L3;
            // I-Anteil
//            float esum = esum + e;
//            float yI = Ki * Ta * esum;
            float yI_Sum = 0;
            float yI_L1 = 0;
            float yI_L2 = 0;
            float yI_L3 = 0;
            // D-Anteil
//            float yD = Kd * (e -ealt) / Ta;
//            float ealt = e;
            float yD_Sum = 0;
            float yD_L1 = 0;
            float yD_L2 = 0;
            float yD_L3 = 0;
            // PID-Anteil
            float yPID_Sum = yP_Sum + yI_Sum + yD_Sum;
            float yPID_L1  = yP_L1 + yI_L1 + yD_L1;
            float yPID_L2  = yP_L2 + yI_L2 + yD_L2;
            float yPID_L3  = yP_L3 + yI_L3 + yD_L3;
            // Regelbegrenzung
//            if (yPID < 5) yPID = 5;
//            if (yPID > 95) yPID = 95;

            logObj["yPID_Sum"] = yPID_Sum;
            logObj["yPID_L1"]  = yPID_L1;
            logObj["yPID_L2"]  = yPID_L2;
            logObj["yPID_L3"]  = yPID_L3;

            JsonArray logArrInv = logObj.createNestedArray("iv");

            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                JsonObject logObjInv = logArrInv.createNestedObject();
                logObjInv["iv"] = inv;

                if (!mCfg->groups[group].inverters[inv].enabled) {
                    continue;
                }

                // Inverterdaten
                logObjInv["limit"] = mCfg->groups[group].inverters[inv].limit;
                logObjInv["powerMin"] = mCfg->groups[group].inverters[inv].powerMin;
                logObjInv["powerMax"] = mCfg->groups[group].inverters[inv].powerMax;
                logObjInv["power"] = mCfg->groups[group].inverters[inv].power;

                // limitNew berechnen
                if (mCfg->groups[group].inverters[inv].target == 0) {
                    // Sum
                    mCfg->groups[group].inverters[inv].limitNew = (uint16_t)(mCfg->groups[group].inverters[inv].limit + (int16_t)yPID_Sum);
                }
                if (mCfg->groups[group].inverters[inv].target == 1) {
                    // L1
                    mCfg->groups[group].inverters[inv].limitNew = (uint16_t)(mCfg->groups[group].inverters[inv].limit + (int16_t)yPID_L1);
                }
                if (mCfg->groups[group].inverters[inv].target == 2) {
                    // L2
                    mCfg->groups[group].inverters[inv].limitNew = (uint16_t)(mCfg->groups[group].inverters[inv].limit + (int16_t)yPID_L2);
                }
                if (mCfg->groups[group].inverters[inv].target == 3) {
                    // L3
                    mCfg->groups[group].inverters[inv].limitNew = (uint16_t)(mCfg->groups[group].inverters[inv].limit + (int16_t)yPID_L3);
                }
                if (mCfg->groups[group].inverters[inv].target == 4) {
                    // L1 + Sum
                }
                if (mCfg->groups[group].inverters[inv].target == 5) {
                    // L2 + Sum
                }
                if (mCfg->groups[group].inverters[inv].target == 6) {
                    // L3 + Sum
                }
            }

            return true;
        }

        bool setControl(uint8_t group) {
// TODO: Die Funktion setControl soll alle gespeicherten Aufgaben in der korrekten Reihenfolge abarbeiten.
            JsonObject logObj = mLog.createNestedObject("setControl");
            logObj["grp"] = group;

            bool ret = true;

            JsonArray logArrInv = logObj.createNestedArray("iv");
            unsigned long tsp = millis();

            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                JsonObject logObjInv = logArrInv.createNestedObject();
                logObjInv["iv"] = inv;

                if (!mCfg->groups[group].inverters[inv].enabled) {
                    continue;
                }

                if (mCfg->groups[group].inverters[inv].limitNew != mCfg->groups[group].inverters[inv].limit) {

                    // Wenn keine Freigabe für neues Limit vorhanden ist -> überspringen
                    if (mCfg->groups[group].inverters[inv].limitTsp != 0) {
                        continue;
                    }

                    // Begrenzen
                    if (mCfg->groups[group].inverters[inv].limitNew <= mCfg->groups[group].inverters[inv].powerMin) {
                        mCfg->groups[group].inverters[inv].limitNew = mCfg->groups[group].inverters[inv].powerMin;
                    }
                    if (mCfg->groups[group].inverters[inv].limitNew >= mCfg->groups[group].inverters[inv].powerMax) {
                        mCfg->groups[group].inverters[inv].limitNew = mCfg->groups[group].inverters[inv].powerMax;
                    }

                    logObjInv["limitOld"] = mCfg->groups[group].inverters[inv].limit;
                    logObjInv["limitNew"] = mCfg->groups[group].inverters[inv].limitNew;

                    setLimit(group, inv);
                    ret = false;
                }
            }

            if (ret) {
                logObj["todo"] = "- nothing todo - ";
            }

            return ret;
        }

        bool waitAccept(uint8_t group) {
            JsonObject logObj = mLog.createNestedObject("waitAccept");
            logObj["grp"] = group;

            bool ret = true;

            JsonArray logArrInv = logObj.createNestedArray("iv");
            unsigned long tsp = millis();

            for (uint8_t inv = 0; inv < ZEROEXPORT_GROUP_MAX_INVERTERS; inv++) {
                JsonObject logObjInv = logArrInv.createNestedObject();
                logObjInv["iv"] = inv;

                if (!mCfg->groups[group].inverters[inv].enabled) {
                    continue;
                }

                if (mCfg->groups[group].inverters[inv].limitAck) {
                    mCfg->groups[group].inverters[inv].limitAck = 0;
                    mCfg->groups[group].inverters[inv].limitTsp = 0;
                    ret = false;
                    continue;
                }

                if ((mCfg->groups[group].inverters[inv].limitTsp + 5000UL) < tsp) {
                    mCfg->groups[group].inverters[inv].limitTsp = 0;
                    ret = false;
                    continue;
                }

            }

            return true;
        }










// TODO: Hier folgen Unterfunktionen für SetControl die Erweitert werden müssen
// setLimit, checkLimit
// setPower, checkPower
// setReboot, checkReboot

        bool setLimit(uint8_t group, uint8_t inv) {
DBGPRINT(String("setLimit: ("));
DBGPRINT(String(group));
DBGPRINT(String("): "));
DBGPRINT(String("iv: ("));
DBGPRINT(String(inv));
DBGPRINT(String(") -> "));

            // Limit verweigern wenn Abweichung < 5 W
//            if ((mCfg->groups[group].inverters[inv].limitNew > mCfg->groups[group].inverters[inv].limit - 5) && (mCfg->groups[group].inverters[inv].limitNew < mCfg->groups[group].inverters[inv].limit + 5)) {
// TODO: 5W Toleranz konfigurierbar?
//DBGPRINT(String(mCfg->groups[group].inverters[inv].limit));
//DBGPRINTLN(String(" W"));
//                return true;
//            }

            // Limit speichern
//DBGPRINT(String(mCfg->groups[group].inverters[inv].limit));
//DBGPRINTLN(String(" W -> "));
//DBGPRINT(String(mCfg->groups[group].inverters[inv].limitNew));
//DBGPRINTLN(String(" W"));
            mCfg->groups[group].inverters[inv].limit = mCfg->groups[group].inverters[inv].limitNew;
            mCfg->groups[group].inverters[inv].limitTsp = millis();

            // Limit übergeben
            DynamicJsonDocument doc(512);
            JsonObject obj = doc.to<JsonObject>();
            obj["val"] = mCfg->groups[group].inverters[inv].limit;
            obj["id"] = mCfg->groups[group].inverters[inv].id;
            obj["path"] = "ctrl";
            obj["cmd"] = "limit_nonpersistent_absolute";
            String data;
            serializeJsonPretty(obj, data);
DBGPRINTLN(data);
            mApi->ctrlRequest(obj);
            return true;
        }

        bool checkLimitAllowed(uint8_t group, uint8_t inv) {
//DBGPRINT(String("checkLimit: ("));
//DBGPRINT(String(group));
//DBGPRINT(String("): "));
//DBGPRINT(String("iv: ("));
//DBGPRINT(String(inv));
//DBGPRINT(String(") -> "));
//DBGPRINT(String(mCfg->groups[group].inverters[inv].limitAck));

            if (mCfg->groups[group].inverters[inv].limitAck == 0) {
                return true;
            } else {
                mCfg->groups[group].inverters[inv].limitAck = 0;
                return false;
            }
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