//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#if defined(PLUGIN_ZEROEXPORT)

#ifndef __POWERMETER_H__
#define __POWERMETER_H__

#include <AsyncJson.h>
#include <HTTPClient.h>

#include "config/settings.h"

#if defined(ZEROEXPORT_POWERMETER_TIBBER)
#include <string.h>

#include <list>

#include "plugins/zeroExport/lib/sml.h"

typedef struct {
    const unsigned char OBIS[6];
    void (*Fn)(double &);
    float *Arg;
} OBISHandler;
#endif

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

        bool result = false;
        float power = 0.0;

        for (u_short group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            if ((!mCfg->groups[group].enabled) || (mCfg->groups[group].sleep)) continue;

            if ((millis() - mCfg->groups[group].pm_peviousTsp) <= ((uint16_t)mCfg->groups[group].pm_refresh * 1000)) continue;
            mCfg->groups[group].pm_peviousTsp = millis();

            switch (mCfg->groups[group].pm_type) {
#if defined(ZEROEXPORT_POWERMETER_SHELLY)
                case zeroExportPowermeterType_t::Shelly:
                    result = getPowermeterWattsShelly(*mLog, group, &power);
                    break;
#endif
#if defined(ZEROEXPORT_POWERMETER_TASMOTA)
                case zeroExportPowermeterType_t::Tasmota:
                    result = getPowermeterWattsTasmota(*mLog, group, &power);
                    break;
#endif
#if defined(ZEROEXPORT_POWERMETER_HICHI)
                case zeroExportPowermeterType_t::Hichi:
                    result = getPowermeterWattsHichi(*mLog, group, &power);
                    break;
#endif
#if defined(ZEROEXPORT_POWERMETER_TIBBER)
                /*  Anscheinend nutzt bei mir Tibber auch diese Freq.
                    862.75 MHz - keine Verbindung
                    863.00 MHz - geht (standard) jedoch hat Tibber dann Probleme... => 4 & 5 Balken
                    863.25 MHz - geht (ohne Tibber Probleme) => 3 & 4 Balken
                */
                case zeroExportPowermeterType_t::Tibber:
                    result = getPowermeterWattsTibber(*mLog, group, &power);
                    mPreviousTsp += 2000;  // Zusätzliche Pause
                    break;
#endif
#if defined(ZEROEXPORT_POWERMETER_SHRDZM)
                case zeroExportPowermeterType_t::Shrdzm:
                    result = getPowermeterWattsShrdzm(*mLog, group, &power);
                    break;
#endif
            }

            if (result) {
                bufferWrite(power, group);

                // MQTT - Powermeter
                if (mCfg->debug) {
                    if (mMqtt->isConnected()) {
                        // P
                        mqttObj["Sum"] = ah::round1(power);
                        //                    mqttObj["L1"] = ah::round1(power.P1);
                        //                    mqttObj["L2"] = ah::round1(power.P2);
                        //                    mqttObj["L3"] = ah::round1(power.P3);
                        mMqtt->publish(String("zero/state/groups/" + String(group) + "/powermeter/P").c_str(), mqttDoc.as<std::string>().c_str(), false);
                        mqttDoc.clear();

                        // W (TODO)
                    }
                }
            }
        }
    }

    /** getDataAVG
     * Holt die Daten vom Powermeter
     * @param group
     * @returns value
     */
    float getDataAVG(uint8_t group) {
        float avg = 0.0;

        for (int i = 0; i < 5; i++) {
            avg += mPowermeterBuffer[group][i];
        }
        avg = avg / 5.0;

        return avg;
    }

    /** getDataMIN
     * Holt die Daten vom Powermeter
     * @param group
     * @returns value
     */
    float getDataMIN(uint8_t group) {
        float min = 0.0;

        for (int i = 0; i < 5; i++) {
            if (i == 0) min = mPowermeterBuffer[group][i];
            if ( min > mPowermeterBuffer[group][i]) min = mPowermeterBuffer[group][i];
        }

        return min;
    }

    /** onMqttConnect
     *
     */
    void onMqttConnect(void) {
#if defined(ZEROEXPORT_POWERMETER_MQTT)

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            if (!strcmp(mCfg->groups[group].pm_jsonPath, "")) continue;

            if (!mCfg->groups[group].enabled) continue;

            if (mCfg->groups[group].pm_type == zeroExportPowermeterType_t::Mqtt) {
                mMqtt->subscribeExtern(String(mCfg->groups[group].pm_jsonPath).c_str(), QOS_2);
            }
        }

#endif /*defined(ZEROEXPORT_POWERMETER_MQTT)*/
    }

    /** onMqttMessage
     *
     */
    void onMqttMessage(JsonObject obj) {
        String topic = String(obj["topic"]);

#if defined(ZEROEXPORT_POWERMETER_MQTT)

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            if (!mCfg->groups[group].enabled) continue;

            if (!mCfg->groups[group].pm_type == zeroExportPowermeterType_t::Mqtt) continue;

            if (!strcmp(mCfg->groups[group].pm_jsonPath, "")) continue;

            if (strcmp(mCfg->groups[group].pm_jsonPath, String(topic).c_str())) continue;

            float power = 0.0;
            power = (uint16_t)obj["val"];

            bufferWrite(power, group);

            // MQTT - Powermeter
            if (mCfg->debug) {
                if (mMqtt->isConnected()) {
                    // P
                    mqttObj["Sum"] = ah::round1(power);
                    ///                    mqttObj["L1"] = ah::round1(power.P1);
                    ///                    mqttObj["L2"] = ah::round1(power.P2);
                    ///                    mqttObj["L3"] = ah::round1(power.P3);
                    mMqtt->publish(String("zero/state/groups/" + String(group) + "/powermeter/P").c_str(), mqttDoc.as<std::string>().c_str(), false);
                    mqttDoc.clear();

                    // W (TODO)
                }
            }

            return;
        }

#endif /*defined(ZEROEXPORT_POWERMETER_MQTT)*/
    }

   private:
    /** mqttSubscribe
     * when a MQTT Msg is needed to subscribe, then a publish is leading
     * @param gr
     * @param payload
     * @returns void
     */
    void mqttSubscribe(String gr, String payload) {
        //        mqttPublish(gr, payload);
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

    HTTPClient http;

    zeroExport_t *mCfg;
    PubMqttType *mMqtt = nullptr;
    JsonObject *mLog;

    unsigned long mPreviousTsp = millis();

    float mPowermeterBuffer[ZEROEXPORT_MAX_GROUPS][5] = {0};
    short mPowermeterBufferPos[ZEROEXPORT_MAX_GROUPS] = {0};

    StaticJsonDocument<512> mqttDoc;  // DynamicJsonDocument mqttDoc(512);
    JsonObject mqttObj = mqttDoc.to<JsonObject>();

    /** setHeader
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
    bool getPowermeterWattsShelly(JsonObject logObj, uint8_t group, float *power) {
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
                return false;
            } else {
                switch (mCfg->groups[group].pm_target) {
                    case zeroExportPowermeterTarget::L1:
                        if (doc.containsKey(F("emeters"))) {
                            // Shelly 3EM
                            *power = doc["emeters"][0]["power"];
                        } else if (doc.containsKey(F("em:0"))) {
                            // Shelly pro 3EM
                            *power = doc["em:0"]["a_act_power"];
                        } else if (doc.containsKey(F("a_act_power"))) {
                            // Shelly pro 3EM
                            *power = doc["a_act_power"];
                        }
                        break;
                    case zeroExportPowermeterTarget::L2:
                        if (doc.containsKey(F("emeters"))) {
                            // Shelly 3EM
                            *power = doc["emeters"][1]["power"];
                        } else if (doc.containsKey(F("em:0"))) {
                            // Shelly pro 3EM
                            *power = doc["em:0"]["b_act_power"];
                        } else if (doc.containsKey(F("b_act_power"))) {
                            // Shelly pro 3EM
                            *power = doc["b_act_power"];
                        }
                        break;
                    case zeroExportPowermeterTarget::L3:
                        if (doc.containsKey(F("emeters"))) {
                            // Shelly 3EM
                            *power = doc["emeters"][2]["power"];
                        } else if (doc.containsKey(F("em:0"))) {
                            // Shelly pro 3EM
                            *power = doc["em:0"]["c_act_power"];
                        } else if (doc.containsKey(F("c_act_power"))) {
                            // Shelly pro 3EM
                            *power = doc["c_act_power"];
                        }
                        break;
                    case zeroExportPowermeterTarget::Sum:
                    default:
                        if (doc.containsKey(F("total_power"))) {
                            // Shelly 3EM
                            *power = doc["total_power"];
                        } else if (doc.containsKey(F("em:0"))) {
                            // Shelly pro 3EM
                            *power = doc["em:0"]["total_act_power"];
                        } else if (doc.containsKey(F("total_act_power"))) {
                            // Shelly pro 3EM
                            *power = doc["total_act_power"];
                        }
                        break;
                }
            }
        }
        http.end();
        return true;
    }
#endif

#if defined(ZEROEXPORT_POWERMETER_TASMOTA)
    /** getPowermeterWattsTasmota
     * ...
     * @param logObj
     * @param group
     * @returns true/false
     */
    bool getPowermeterWattsTasmota(JsonObject logObj, uint8_t group, float *power) {
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
        return false;
    }
#endif

#if defined(ZEROEXPORT_POWERMETER_HICHI)
    /** getPowermeterWattsHichi
     * ...
     * @param logObj
     * @param group
     * @returns true/false
     */
    bool getPowermeterWattsHichi(JsonObject logObj, uint8_t group, float *power) {
        logObj["mod"] = "getPowermeterWattsHichi";

        // Hier neuer Code - Anfang

        // TODO: Noch nicht komplett

        // Hier neuer Code - Ende

        return false;
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

    bool getPowermeterWattsTibber(JsonObject logObj, uint8_t group, float *power) {
        mPreviousTsp = mPreviousTsp + 2000;  // Zusätzliche Pause

        bool result = false;

        logObj["mod"] = "getPowermeterWattsTibber";

        String auth = mCfg->groups[group].pm_pass;
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
                        *power = _powerMeterTotal;
                        result = true;
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
    bool getPowermeterWattsShrdzm(JsonObject logObj, uint8_t group, float *power) {
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
                return false;
            } else {
                if (doc.containsKey(F("16.7.0"))) {
                    *power = doc["16.7.0"];
                }
            }
        }
        http.end();

        return true;
    }
#endif

    /**
     *
     */
    void bufferWrite(float raw, short group) {
        mPowermeterBuffer[group][mPowermeterBufferPos[group]] = raw;
        mPowermeterBufferPos[group]++;
        if (mPowermeterBufferPos[group] >= 5) mPowermeterBufferPos[group] = 0;
    }
};

#endif /*__POWERMETER_H__*/

#endif /* #if defined(PLUGIN_ZEROEXPORT) */
