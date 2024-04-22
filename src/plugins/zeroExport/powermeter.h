//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __POWERMETER_H__
#define __POWERMETER_H__

#include <AsyncJson.h>
#include <HTTPClient.h>

#include "config/settings.h"

#if defined(ZEROEXPORT_POWERMETER_TIBBER)
#include <base64.h>
#include <string.h>

#include <list>

#include "plugins/zeroExport/lib/sml.h"

typedef struct {
    const unsigned char OBIS[6];
    void (*Fn)(double &);
    float *Arg;
} OBISHandler;
#endif

typedef struct {
    float P;
    float P1;
    float P2;
    float P3;
} PowermeterBuffer_t;

class powermeter {
   public:
    /** powermeter
     * constructor
     */
    powermeter() {}

    /** ~powermeter
     * destructor
     */
    ~powermeter() {}

    /** setup
     * Initialisierung
     * @param *cfg
     * @param *mqtt
     * @param *log
     * @returns void
     */
    bool setup(zeroExport_t *cfg, PubMqttType *mqtt, JsonObject *log) {
        mCfg = cfg;
        mMqtt = mqtt;
        mLog = log;

        return true;
    }

    /** loop
     * Arbeitsschleife
     * @param void
     * @returns void
     * @todo emergency
     */
    void loop(void) {
        if (millis() - mPreviousTsp <= 1000) return;  // skip when it is to fast
        mPreviousTsp = millis();

        PowermeterBuffer_t power;

        for (u_short group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            if ((!mCfg->groups[group].enabled) || (mCfg->groups[group].sleep)) continue;

            switch (mCfg->groups[group].pm_type) {
#if defined(ZEROEXPORT_POWERMETER_SHELLY)
                case zeroExportPowermeterType_t::Shelly:
                    power = getPowermeterWattsShelly(*mLog, group);
                    break;
#endif
#if defined(ZEROEXPORT_POWERMETER_TASMOTA)
                case zeroExportPowermeterType_t::Tasmota:
                    power = getPowermeterWattsTasmota(*mLog, group);
                    break;
#endif
#if defined(ZEROEXPORT_POWERMETER_MQTT)
                case zeroExportPowermeterType_t::Mqtt:
                    power = getPowermeterWattsMqtt(*mLog, group);
                    break;
#endif
#if defined(ZEROEXPORT_POWERMETER_HICHI)
                case zeroExportPowermeterType_t::Hichi:
                    power = getPowermeterWattsHichi(*mLog, group);
                    break;
#endif
#if defined(ZEROEXPORT_POWERMETER_TIBBER)
                case zeroExportPowermeterType_t::Tibber:
                    power = getPowermeterWattsTibber(*mLog, group);
                    mPreviousTsp += 2000;  // Zusätzliche Pause
                    break;
#endif
#if defined(ZEROEXPORT_POWERMETER_SHRDZM)
                case zeroExportPowermeterType_t::Shrdzm:
                    power = getPowermeterWattsShrdzm(*mLog, group);
                    break;
#endif
            }

            bufferWrite(power, group);

            // MQTT - Powermeter
            if (mMqtt->isConnected()) {
                // P
                mqttObj["Sum"] = ah::round1(power.P);
                mqttObj["L1"] = ah::round1(power.P1);
                mqttObj["L2"] = ah::round1(power.P2);
                mqttObj["L3"] = ah::round1(power.P3);
                mMqtt->publish(String("zero/state/groups/" + String(group) + "/powermeter/P").c_str(), mqttDoc.as<std::string>().c_str(), false);
                mqttDoc.clear();

                // W (TODO)
            }
        }
    }

    /** groupGetPowermeter
     * Holt die Daten vom Powermeter
     * @param group
     * @returns true/false
     */
    PowermeterBuffer_t getDataAVG(uint8_t group) {
        PowermeterBuffer_t avg;
        avg.P = avg.P1 = avg.P2 = avg.P2 = avg.P3 = 0;

        for (int i = 0; i < 5; i++) {
            avg.P += mPowermeterBuffer[group][i].P;
            avg.P1 += mPowermeterBuffer[group][i].P1;
            avg.P2 += mPowermeterBuffer[group][i].P2;
            avg.P3 += mPowermeterBuffer[group][i].P3;
        }
        avg.P = avg.P / 5;
        avg.P1 = avg.P1 / 5;
        avg.P2 = avg.P2 / 5;
        avg.P3 = avg.P3 / 5;

        return avg;
    }

    /**
     *
     */
    void onMqttConnect(void) {
    }

    /**
     *
     */
    void onMqttMessage(JsonObject obj) {
        String topic = String(obj["topic"]);

#if defined(ZEROEXPORT_POWERMETER_MQTT)
        // topic for powermeter?
        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            if (mCfg->groups[group].pm_type == zeroExportPowermeterType_t::Mqtt) {
                //                mLog["mqttDevice"] = "topicInverter";
                if (!topic.equals(mCfg->groups[group].pm_jsonPath)) return;
                mCfg->groups[group].pm_P = (int32_t)obj["val"];
            }
        }
#endif /*defined(ZEROEXPORT_POWERMETER_MQTT)*/
    }

   private:
    HTTPClient http;

    zeroExport_t *mCfg;
    PubMqttType *mMqtt = nullptr;
    JsonObject *mLog;

    unsigned long mPreviousTsp = 0;

    PowermeterBuffer_t mPowermeterBuffer[ZEROEXPORT_MAX_GROUPS][5] = {0};
    short mPowermeterBufferPos[ZEROEXPORT_MAX_GROUPS] = {0};

    StaticJsonDocument<512> mqttDoc;  // DynamicJsonDocument mqttDoc(512);
    JsonObject mqttObj = mqttDoc.to<JsonObject>();

    /**
     *
     */
    void setHeader(HTTPClient *h) {
        h->setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        h->setUserAgent("Ahoy-Agent");
        // TODO: Ahoy-0.8.850024-zero
        h->setConnectTimeout(500);
        h->setTimeout(1000);
        h->addHeader("Content-Type", "application/json");
        h->addHeader("Accept", "application/json");
    }

#if defined(ZEROEXPORT_POWERMETER_SHELLY)
    /** getPowermeterWattsShelly
     * ...
     * @param logObj
     * @param group
     * @returns true/false
     */
    PowermeterBuffer_t getPowermeterWattsShelly(JsonObject logObj, uint8_t group) {
        PowermeterBuffer_t result;
        result.P = result.P1 = result.P2 = result.P3 = 0;

        logObj["mod"] = "getPowermeterWattsShelly";

        setHeader(&http);

        String url = String("http://") + String(mCfg->groups[group].pm_url) + String("/") + String(mCfg->groups[group].pm_jsonPath);
        logObj["HTTP_URL"] = url;

        http.begin(url);

        if (http.GET() == HTTP_CODE_OK) {
            // Parsing
            DynamicJsonDocument doc(2048);
            DeserializationError error = deserializeJson(doc, http.getString());
            if (error) {
                logObj["err"] = "deserializeJson: " + String(error.c_str());
                return result;
            } else {
                if (doc.containsKey(F("total_power"))) {
                    // Shelly 3EM
                    result.P = doc["total_power"];
                } else if (doc.containsKey(F("em:0"))) {
                    // Shelly pro 3EM
                    result.P = doc["em:0"]["total_act_power"];
                } else if (doc.containsKey(F("total_act_power"))) {
                    // Shelly pro 3EM
                    result.P = doc["total_act_power"];
                } else {
                    // Keine Daten
                    result.P = 0;
                }

                if (doc.containsKey(F("emeters"))) {
                    // Shelly 3EM
                    result.P1 = doc["emeters"][0]["power"];
                } else if (doc.containsKey(F("em:0"))) {
                    // Shelly pro 3EM
                    result.P1 = doc["em:0"]["a_act_power"];
                } else if (doc.containsKey(F("a_act_power"))) {
                    // Shelly pro 3EM
                    result.P1 = doc["a_act_power"];
                } else if (doc.containsKey(F("switch:0"))) {
                    // Shelly plus1pm plus2pm
                    result.P1 = doc["switch:0"]["apower"];
                    result.P += result.P1;
                } else if (doc.containsKey(F("apower"))) {
                    // Shelly Alternative
                    result.P1 = doc["apower"];
                    result.P += result.P1;
                } else {
                    // Keine Daten
                    result.P1 = 0;
                }

                if (doc.containsKey(F("emeters"))) {
                    // Shelly 3EM
                    result.P2 = doc["emeters"][1]["power"];
                } else if (doc.containsKey(F("em:0"))) {
                    // Shelly pro 3EM
                    result.P2 = doc["em:0"]["b_act_power"];
                } else if (doc.containsKey(F("b_act_power"))) {
                    // Shelly pro 3EM
                    result.P2 = doc["b_act_power"];
                } else if (doc.containsKey(F("switch:1"))) {
                    // Shelly plus1pm plus2pm
                    result.P2 = doc["switch.1"]["apower"];
                    result.P += result.P2;
                    //} else if (doc.containsKey(F("apower"))) {
                    // Shelly Alternative
                    //    mCfg->groups[group].pmPowerL2 = doc["apower"];
                    //    mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL2;
                    //    ret = true;
                } else {
                    // Keine Daten
                    result.P2 = 0;
                }

                if (doc.containsKey(F("emeters"))) {
                    // Shelly 3EM
                    result.P3 = doc["emeters"][2]["power"];
                } else if (doc.containsKey(F("em:0"))) {
                    // Shelly pro 3EM
                    result.P3 = doc["em:0"]["c_act_power"];
                } else if (doc.containsKey(F("c_act_power"))) {
                    // Shelly pro 3EM
                    result.P3 = doc["c_act_power"];
                } else if (doc.containsKey(F("switch:2"))) {
                    // Shelly plus1pm plus2pm
                    result.P3 = doc["switch:2"]["apower"];
                    result.P += result.P3;
                    //} else if (doc.containsKey(F("apower"))) {
                    // Shelly Alternative
                    //    mCfg->groups[group].pmPowerL3 = doc["apower"];
                    //    mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL3;
                    //    result = true;
                } else {
                    // Keine Daten
                    result.P3 = 0;
                }
            }
        }
        http.end();
        return result;
    }
#endif

#if defined(ZEROEXPORT_POWERMETER_TASMOTA)
    /** getPowermeterWattsTasmota
     * ...
     * @param logObj
     * @param group
     * @returns true/false
     */
    PowermeterBuffer_t getPowermeterWattsTasmota(JsonObject logObj, uint8_t group) {
        PowermeterBuffer_t result;
        result.P = result.P1 = result.P2 = result.P3 = 0;

        logObj["mod"] = "getPowermeterWattsTasmota";
        /*
        // TODO: nicht komplett

                    HTTPClient http;
                    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
                    http.setUserAgent("Ahoy-Agent");
        // TODO: Ahoy-0.8.850024-zero
                    http.setConnectTimeout(500);
                    http.setTimeout(500);
        // TODO: Timeout von 1000 reduzieren?
                    http.addHeader("Content-Type", "application/json");
                    http.addHeader("Accept", "application/json");

        //            String url = String("http://") + String(mCfg->groups[group].pm_url) + String("/") + String(mCfg->groups[group].pm_jsonPath);
                    String url = String(mCfg->groups[group].pm_url);
                    logObj["HTTP_URL"] = url;

                    http.begin(url);

                    if (http.GET() == HTTP_CODE_OK)
                    {

                        // Parsing
                        DynamicJsonDocument doc(2048);
                        DeserializationError error = deserializeJson(doc, http.getString());
                        if (error)
                        {
                            logObj["error"] = "deserializeJson() failed: " + String(error.c_str());
                            return result;
                        }

        // TODO: Sum
                            result = true;

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
        /*
                        logObj["P"]   = mCfg->groups[group].pmPower;
                        logObj["P1"] = mCfg->groups[group].pmPowerL1;
                        logObj["P2"] = mCfg->groups[group].pmPowerL2;
                        logObj["P3"] = mCfg->groups[group].pmPowerL3;
                    }
                    http.end();
        */
        return result;
    }
#endif

#if defined(ZEROEXPORT_POWERMETER_MQTT)
    /** getPowermeterWattsMqtt
     * ...
     * @param logObj
     * @param group
     * @returns true/false
     */
    PowermeterBuffer_t getPowermeterWattsMqtt(JsonObject logObj, uint8_t group) {
        PowermeterBuffer_t result;
        result.P = result.P1 = result.P2 = result.P3 = 0;

        logObj["mod"] = "getPowermeterWattsMqtt";

        // topic for powermeter?
        result.P = mCfg->groups[group].pm_P;
        result.P1 = result.P2 = result.P3 = mCfg->groups[group].pm_P / 3;

        return result;
    }
#endif

#if defined(ZEROEXPORT_POWERMETER_HICHI)
    /** getPowermeterWattsHichi
     * ...
     * @param logObj
     * @param group
     * @returns true/false
     */
    PowermeterBuffer_t getPowermeterWattsHichi(JsonObject logObj, uint8_t group) {
        PowermeterBuffer_t result;
        result.P = result.P1 = result.P2 = result.P3 = 0;

        logObj["mod"] = "getPowermeterWattsHichi";

        // Hier neuer Code - Anfang

        // TODO: Noch nicht komplett

        // Hier neuer Code - Ende

        return result;
    }
#endif

#if defined(ZEROEXPORT_POWERMETER_TIBBER)
    /** getPowermeterWattsTibber
     * ...
     * @param logObj
     * @param group
     * @returns true/false
     * @TODO: Username & Passwort wird mittels base64 verschlüsselt. Dies wird für die Authentizierung benötigt. Wichtig diese im WebUI unkenntlich zu machen und base64 im eeprom zu speichern, statt klartext.
     * @TODO: Abfrage Interval einbauen. Info: Datei-Size kann auch mal 0-bytes sein!
     */

    sml_states_t currentState;

    float _powerMeterTotal = 0.0;

    float _powerMeter1Power = 0.0;
    float _powerMeter2Power = 0.0;
    float _powerMeter3Power = 0.0;

    float _powerMeterImport = 0.0;
    float _powerMeterExport = 0.0;

    /*
     07 81 81 c7 82 03 ff		#objName: OBIS Kennzahl für den Hersteller
     07 01 00 01 08 00 ff		#objName: OBIS Kennzahl für Wirkenergie Bezug gesamt tariflos
     07 01 00 01 08 01 ff 		#objName: OBIS-Kennzahl für Wirkenergie Bezug Tarif1
     07 01 00 01 08 02 ff		#objName: OBIS-Kennzahl für Wirkenergie Bezug Tarif2
     07 01 00 02 08 00 ff		#objName: OBIS-Kennzahl für Wirkenergie Einspeisung gesamt tariflos
     07 01 00 02 08 01 ff		#objName: OBIS-Kennzahl für Wirkenergie Einspeisung Tarif1
     07 01 00 02 08 02 ff		#objName: OBIS-Kennzahl für Wirkenergie Einspeisung Tarif2
    */
    const std::list<OBISHandler> smlHandlerList{
        {{0x01, 0x00, 0x10, 0x07, 0x00, 0xff}, &smlOBISW, &_powerMeterTotal},   // total - OBIS-Kennzahl für momentane Gesamtwirkleistung
        {{0x01, 0x00, 0x24, 0x07, 0x00, 0xff}, &smlOBISW, &_powerMeter1Power},  // OBIS-Kennzahl für momentane Wirkleistung in Phase L1
        {{0x01, 0x00, 0x38, 0x07, 0x00, 0xff}, &smlOBISW, &_powerMeter2Power},  // OBIS-Kennzahl für momentane Wirkleistung in Phase L2
        {{0x01, 0x00, 0x4c, 0x07, 0x00, 0xff}, &smlOBISW, &_powerMeter3Power},  // OBIS-Kennzahl für momentane Wirkleistung in Phase L3
        {{0x01, 0x00, 0x01, 0x08, 0x00, 0xff}, &smlOBISWh, &_powerMeterImport},
        {{0x01, 0x00, 0x02, 0x08, 0x00, 0xff}, &smlOBISWh, &_powerMeterExport}};

    PowermeterBuffer_t getPowermeterWattsTibber(JsonObject logObj, uint8_t group) {
        mPreviousTsp = mPreviousTsp + 2000;  // Zusätzliche Pause

        PowermeterBuffer_t result;
        result.P = result.P1 = result.P2 = result.P3 = 0;

        logObj["mod"] = "getPowermeterWattsTibber";

        String auth;
        if (strlen(mCfg->groups[group].pm_user) > 0 && strlen(mCfg->groups[group].pm_pass) > 0) {
            auth = base64::encode(String(mCfg->groups[group].pm_user) + String(":") + String(mCfg->groups[group].pm_pass));
            snprintf(mCfg->groups[group].pm_user, ZEROEXPORT_GROUP_MAX_LEN_PM_USER, "%s", DEF_ZEXPORT);
            snprintf(mCfg->groups[group].pm_pass, ZEROEXPORT_GROUP_MAX_LEN_PM_PASS, "%s", auth.c_str());
            //@TODO:mApp->saveSettings(false);
        } else {
            auth = mCfg->groups[group].pm_pass;
        }

        String url = String("http://") + mCfg->groups[group].pm_url + String("/") + String(mCfg->groups[group].pm_jsonPath);

        setHeader(&http);
        http.begin(url);
        http.addHeader("Authorization", "Basic " + auth);

        if (http.GET() == HTTP_CODE_OK && http.getSize() > 0) {
            String myString = http.getString();
            double readVal = 0;
            unsigned char c;

            for (int i = 0; i < http.getSize(); ++i) {
                c = myString[i];
                sml_states_t smlCurrentState = smlState(c);

                switch (smlCurrentState) {
                    case SML_FINAL:
                        result.P = _powerMeterTotal;
                        result.P1 = _powerMeter1Power;
                        result.P2 = _powerMeter2Power;
                        result.P3 = _powerMeter3Power;

                        if (!(_powerMeter1Power && _powerMeter2Power && _powerMeter3Power)) {
                            result.P1 = result.P2 = result.P3 = _powerMeterTotal / 3;
                        }
                        break;
                    case SML_LISTEND:
                        // check handlers on last received list
                        for (auto &handler : smlHandlerList) {
                            if (smlOBISCheck(handler.OBIS)) {
                                handler.Fn(readVal);
                                *handler.Arg = readVal;
                            }
                        }
                        break;
                }
            }
        }

        http.end();
        return result;
    }
#endif

#if defined(ZEROEXPORT_POWERMETER_SHRDZM)
    /** getPowermeterWattsShrdzm
     * ...
     * @param logObj
     * @param group
     * @returns true/false
     * @TODO: Username & Passwort wird mittels base64 verschlüsselt. Dies wird für die Authentizierung benötigt. Wichtig diese im WebUI unkenntlich zu machen und base64 im eeprom zu speichern, statt klartext.
     * @TODO: Abfrage Interval einbauen. Info: Datei-Size kann auch mal 0-bytes sein?
     */
    PowermeterBuffer_t getPowermeterWattsShrdzm(JsonObject logObj, uint8_t group) {
        PowermeterBuffer_t result;
        result.P = result.P1 = result.P2 = result.P3 = 0;

        logObj["mod"] = "getPowermeterWattsShrdzm";

        setHeader(&http);

        String url =
            String("http://") + String(mCfg->groups[group].pm_url) +
            String("/") + String(mCfg->groups[group].pm_jsonPath + String("?user=") + String(mCfg->groups[group].pm_user) + String("&password=") + String(mCfg->groups[group].pm_pass));

        http.begin(url);

        if (http.GET() == HTTP_CODE_OK && http.getSize() > 0) {
            // Parsing
            DynamicJsonDocument doc(512);
            DeserializationError error = deserializeJson(doc, http.getString());
            if (error) {
                logObj["err"] = "deserializeJson: " + String(error.c_str());
                return result;
            } else {
                if (doc.containsKey(F("16.7.0"))) {
                    result.P = doc["16.7.0"];
                }

                if (!(result.P1 && result.P2 && result.P3)) {
                    result.P1 = result.P2 = result.P3 = result.P / 3;
                }
            }
        }
        http.end();

        return result;
    }
#endif

    /**
     *
     */
    void bufferWrite(PowermeterBuffer_t raw, short group) {
        mPowermeterBuffer[group][mPowermeterBufferPos[group]] = raw;
        mPowermeterBufferPos[group]++;
        if (mPowermeterBufferPos[group] >= 5) mPowermeterBufferPos[group] = 0;
    }
};

#endif /*__POWERMETER_H__*/
