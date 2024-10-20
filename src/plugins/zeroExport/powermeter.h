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
#include "utils/DynamicJsonHandler.h"

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
    bool setup(IApp *app, zeroExport_t *cfg, settings_t *config, PubMqttType *mqtt, DynamicJsonHandler *log) {
        mApp = app;
        mCfg = cfg;
        mConfig = config;
        mMqtt = mqtt;
        mLog = log;

        return true;
    }

    /**
     * @brief Main loop function for processing power meter data and publishing to MQTT.
     *
     * This function performs the following tasks:
     * - Checks if the interval since the last execution is sufficient.
     * - Updates the timestamp of the last execution.
     * - Checks if the MQTT connection is active.
     * - Iterates through all configured power meter groups.
     * - For each group, checks if it is enabled and not in sleep mode.
     * - Avoids unnecessary calculations if the refresh interval has not passed.
     * - Depending on the power meter type, retrieves the current power consumption.
     * - Writes the power data to a buffer and updates the group's power value.
     * - If MQTT is connected, publishes the power data to the MQTT broker.
     *
     * @note This function assumes that the following macros and functions are defined:
     * - ZEROEXPORT_DEBUG
     * - ZEROEXPORT_MAX_GROUPS
     * - ZEROEXPORT_POWERMETER_SHELLY
     * - ZEROEXPORT_POWERMETER_TASMOTA
     * - ZEROEXPORT_POWERMETER_HICHI
     * - ZEROEXPORT_POWERMETER_TIBBER
     * - ZEROEXPORT_POWERMETER_SHRDZM
     * - millis()
     * - DBGPRINTLN()
     * - getPowermeterWattsShelly()
     * - getPowermeterWattsTasmota()
     * - getPowermeterWattsHichi()
     * - getPowermeterWattsTibber()
     * - getPowermeterWattsShrdzm()
     * - bufferWrite()
     * - ah::round1()
     *
     * @param void No parameters.
     * @return void No return value.
     */
    void loop(void) {
        constexpr uint32_t interval = 1000;  // Konstante für das Intervall
        constexpr uint8_t bufferSize = 5;    // Puffergröße

        uint32_t currentMillis = millis();   // Einmal die aktuelle Zeit abrufen
        if (currentMillis - mPreviousTsp <= interval) return;  // Bei zu kurzem Intervall überspringen
        mPreviousTsp = currentMillis;

        #ifdef ZEROEXPORT_DEBUG
            if (mCfg->debug) DBGPRINTLN(F("pm Takt:"));
        #endif

        // Überprüfen, ob die MQTT-Verbindung einmalig aktiv ist
        const bool mqttConnected = mMqtt->isConnected();

        char topic[64];     // Puffer für das Topic
        char payload[16];   // Puffer für das Payload

        for (u_short group = 0; group < ZEROEXPORT_MAX_GROUPS; ++group) {
            auto& groupCfg = mCfg->groups[group];
            if (!groupCfg.enabled || groupCfg.sleep) continue;

            // Optimierung: Berechnungen vermeiden, wenn keine Aktualisierung nötig ist
            uint32_t refreshInterval = static_cast<uint32_t>(groupCfg.pm_refresh) * interval;
            if (currentMillis - groupCfg.pm_peviousTsp < refreshInterval) continue;
            groupCfg.pm_peviousTsp = currentMillis;

            #ifdef ZEROEXPORT_DEBUG
                if (mCfg->debug) DBGPRINTLN(F("pm Do:"));
            #endif

            bool result = false;
            float power = 0.0;

            // Verwenden von `switch-case`, um abhängig vom Powermeter-Typ zu entscheiden
            switch (groupCfg.pm_type) {
    #if defined(ZEROEXPORT_POWERMETER_SHELLY)
                case zeroExportPowermeterType_t::Shelly:
                    result = getPowermeterWattsShelly(group, &power);
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
                    // Mindestens 3 Sekunden Refresh für Tibber
                    if (groupCfg.pm_refresh < 3) groupCfg.pm_refresh = 3;
                    result = getPowermeterWattsTibber(group, &power);
                    break;
    #endif
    #if defined(ZEROEXPORT_POWERMETER_SHRDZM)
                case zeroExportPowermeterType_t::Shrdzm:
                    result = getPowermeterWattsShrdzm(group, &power);
                    break;
    #endif
                default:
                    continue;  // Ungültiger Powermeter-Typ, überspringen
            }

            if (result) {
                bufferWrite(power, group);  // Power in den Puffer schreiben
                groupCfg.power = power;     // Aktualisiere die Gruppenleistung

                // Nur wenn MQTT verbunden ist, eine Nachricht senden
                if (mqttConnected) {
                    snprintf(topic, sizeof(topic), "zero/state/groups/%d/powermeter/P", group);
                    snprintf(payload, sizeof(payload), "%.1f", ah::round1(power));
                    mMqtt->publish(topic, payload, false);
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
            if (i == 0)
                min = mPowermeterBuffer[group][i];
            if (min > mPowermeterBuffer[group][i])
                min = mPowermeterBuffer[group][i];
        }

        return min;
    }

    /** getDataMAX
     * Holt die Daten vom Powermeter
     * @param group
     * @returns value
     */
    float getDataMAX(uint8_t group) {
        float max = 0.0;

        for (int i = 0; i < 5; i++) {
            if (i == 0)
                max = mPowermeterBuffer[group][i];
            if (max < mPowermeterBuffer[group][i])
                max = mPowermeterBuffer[group][i];
        }

        return max;
    }

    /** onMqttConnect
     *
     */
    void onMqttConnect(void) {
#if defined(ZEROEXPORT_POWERMETER_MQTT)

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            if (!strcmp(mCfg->groups[group].pm_src, "")) continue;

            if (!mCfg->groups[group].enabled) continue;

            if (mCfg->groups[group].pm_type == zeroExportPowermeterType_t::Mqtt) {
                mMqtt->subscribeExtern(String(mCfg->groups[group].pm_src).c_str(), QOS_2);
            }
        }

#endif /*defined(ZEROEXPORT_POWERMETER_MQTT)*/
    }

    /** onMqttMessage
     * This function is needed for all mqtt connections between ahoy and other devices.
     */
    bool onMqttMessage(const char* topic, const uint8_t* payload, size_t len)
    {
        bool result = false;

        #if defined(ZEROEXPORT_POWERMETER_MQTT)
            for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++)
            {
                if (!mCfg->groups[group].enabled) continue;
                if (!mCfg->groups[group].pm_type == zeroExportPowermeterType_t::Mqtt) continue;
                if (!strcmp(mCfg->groups[group].pm_src, "")) continue;
                if (strcmp(mCfg->groups[group].pm_src, topic) != 0) continue;    // strcmp liefert 0 wenn gleich

                float power = 0.0;
                String sPayload = String((const char*)payload).substring(0, len);

                if (sPayload.startsWith("{") && sPayload.endsWith("}") || sPayload.startsWith("[") && sPayload.endsWith("]"))
                {
                    #ifdef ZEROEXPORT_DEBUG
                        DPRINTLN(DBG_INFO, String("ze: mqtt powermeter val: ") + sPayload);
                    #endif /*ZEROEXPORT_DEBUG*/

                    DynamicJsonDocument datajson(2048); // TODO: JSON größe dynamisch machen?
                    if(!deserializeJson(datajson, sPayload.c_str()))
                    {
                        #ifdef ZEROEXPORT_DEBUG
                            DPRINTLN(DBG_INFO, String("ze: mqtt powermeter deserialize ok"));
                            DPRINTLN(DBG_INFO, String(datajson.as<String>()));
                        #endif /*ZEROEXPORT_DEBUG*/
                        power = extractJsonKey(datajson, mCfg->groups[group].pm_jsonPath);
                    }

                }
                else
                {
                    #ifdef ZEROEXPORT_DEBUG
                        DPRINTLN(DBG_INFO, String("ze: mqtt powermeter kein json"));
                    #endif /*ZEROEXPORT_DEBUG*/
                    power = sPayload.toFloat();
                }

                bufferWrite(power, group);
                mCfg->groups[group].power = power;

                // MQTT - Powermeter
                DPRINTLN(DBG_INFO, String("ze: mqtt powermeter ") + String(power));
                if (mCfg->debug) {
                    if (mMqtt->isConnected()) {
                        mMqtt->publish(String("zero/state/groups/" + String(group) + "/powermeter/P").c_str(), String(ah::round1(power)).c_str(), false);
                    }
                }

                result = true;
            }

        #endif /*defined(ZEROEXPORT_POWERMETER_MQTT)*/

        return result;
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

    /*uint16_t mqttUnsubscribe(const char *subTopic,)
    { TODO: hier weiter?
        return mMqtt->unsubscribe(topic);  // add as many topics as you like
    }*/

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
    settings_t *mConfig = nullptr;
    PubMqttType *mMqtt = nullptr;
    DynamicJsonHandler *mLog;
    IApp *mApp = nullptr;

    unsigned long mPreviousTsp = millis();

    float mPowermeterBuffer[ZEROEXPORT_MAX_GROUPS][5] = {0};
    short mPowermeterBufferPos[ZEROEXPORT_MAX_GROUPS] = {0};

    StaticJsonDocument<512> mqttDoc;  // DynamicJsonDocument mqttDoc(512);
    JsonObject mqttObj = mqttDoc.to<JsonObject>();

    /** setHeader
     *
     */
    void setHeader(HTTPClient *h, String auth = "", u8_t realm = 0) {
        h->setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        ///        h->setUserAgent("Ahoy-Agent");
        ///        // TODO: Ahoy-0.8.850024-zero
        h->setUserAgent(mApp->getVersion());
        h->setConnectTimeout(500);
        h->setTimeout(1000);
        h->addHeader("Content-Type", "application/json");
        h->addHeader("Accept", "application/json");

        /*
        Shelly PM Mini Gen3
        Shelly Plus 1PM
        Shelly Plus 2PM
        Shelly Pro 3EM - 120A
        Shelly Pro 4PM
        Shelly Pro Dual Cover / Shutter PM
        Shelly Pro 1PM
        Shelly Pro 2PM
        Shelly Pro EM - 50
        Shelly Qubino Wave 1PM Mini
        Shelly Qubino Wave PM Mini
        Shelly Qubino Wave Shutter
        Shelly Qubino Wave 1PM
        Shelly Qubino Wave 2PM
        Shelly Qubino Wave Pro 1PM
        Shelly Qubino Wave Pro 2PM
        Shelly 3EM
        Shelly EM + 120A Clamp

        */

        /*if (auth != NULL && realm) http.addHeader("WWW-Authenticate", "Digest qop=\"auth\", realm=\"" + shellypro4pm-f008d1d8b8b8 + "\", nonce=\"60dc59c6\", algorithm=SHA-256");
        else if (auth != NULL) http.addHeader("Authorization", "Basic " + auth);*/
        /*
            All Required:
            realm: string, device_id of the Shelly device.
            username: string, must be set to admin.
            nonce: number, random or pseudo-random number to prevent replay attacks, taken from the error message.
            cnonce: number, client nonce, random number generated by the client.
            response: string, encoding of the string <ha1> + ":" + <nonce> + ":" + <nc> + ":" + <cnonce> + ":" + "auth" + ":" + <ha2> in SHA256.
                ha1: string, <user>:<realm>:<password> encoded in SHA256
                ha2: string, "dummy_method:dummy_uri" encoded in SHA256
            algorithm: string, SHA-256.
        */
    }

    /**
     *
     *
     */
    float extractJsonKey(DynamicJsonDocument data, const char* key)
    {
        if (data.containsKey(key))
            return (float)data[key];
        else {
            DPRINTLN(DBG_INFO, String("ze: mqtt powermeter deserialize no key ") + String(key));
            return 0.0F;
        }
    }

#if defined(ZEROEXPORT_POWERMETER_SHELLY)
    /** getPowermeterWattsShelly
     * ...
     * @param logObj
     * @param group
     * @returns true/false
     */
    bool getPowermeterWattsShelly(uint8_t group, float *power) {
        mLog->addProperty("mod", "getPowermeterWattsShelly");

        setHeader(&http);

        String url = String("http://") + String(mCfg->groups[group].pm_src) + String("/") + String(mCfg->groups[group].pm_jsonPath);
        mLog->addProperty("HTTP_URL", url);

        http.begin(url);

        if (http.GET() == HTTP_CODE_OK) {
            // Parsing
            DynamicJsonDocument doc(2048);
            DeserializationError error = deserializeJson(doc, http.getString());
            if (error) {
                mLog->addProperty("err", "deserializeJson: " + String(error.c_str()));
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
    bool getPowermeterWattsTasmota(DynamicJsonHandler logObj, uint8_t group, float *power) {
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

        //            String url = String("http://") + String(mCfg->groups[group].pm_src) + String("/") + String(mCfg->groups[group].pm_jsonPath);
                    String url = String(mCfg->groups[group].pm_src);
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
    bool getPowermeterWattsHichi(DynamicJsonHandler logObj, uint8_t group, float *power) {
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

    /*
    Daniel92: https://tibber.com/de/api/lookup/price-overview?postalCode=
    Hab ich mal ausgelesen... hintendran die PLZ eingeben
    energy/todayHours/<aktuelleStunde>/priceIncludingVat
    */
    bool getPowermeterWattsTibber(uint8_t group, float *power) {
        bool result = false;
        mLog->addProperty("mod", "getPowermeterWattsTibber");

        String auth = mCfg->groups[group].pm_pass;
        String url = String("http://") + mCfg->groups[group].pm_src + String("/") + String(mCfg->groups[group].pm_jsonPath);

        setHeader(&http, auth);
        http.begin(url);

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
// TODO: pm_taget auswerten und damit eine Regelung auf Sum, L1, L2, L3 ermöglichen (setup.html nicht vergessen)
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
    bool getPowermeterWattsShrdzm(uint8_t group, float *power) {
        mLog->addProperty("mod", "getPowermeterWattsShrdzm");

        setHeader(&http);

        String url =
            String("http://") + String(mCfg->groups[group].pm_src) +
            String("/") + String(mCfg->groups[group].pm_jsonPath + String("?user=") + String(mCfg->groups[group].pm_user) + String("&password=") + String(mCfg->groups[group].pm_pass));

        http.begin(url);

        if (http.GET() == HTTP_CODE_OK && http.getSize() > 0) {
            // Parsing
            DynamicJsonDocument doc(512);
            DeserializationError error = deserializeJson(doc, http.getString());
            if (error) {
                mLog->addProperty("err", "deserializeJson: " + String(error.c_str()));
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
        mPowermeterBufferPos[group] = (mPowermeterBufferPos[group] + 1) % 5;
    }
};

#endif /*__POWERMETER_H__*/

#endif /* #if defined(PLUGIN_ZEROEXPORT) */
