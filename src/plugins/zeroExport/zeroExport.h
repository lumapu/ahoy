#if defined(PLUGIN_ZEROEXPORT)

#ifndef __ZEROEXPORT__
#define __ZEROEXPORT__

#include <HTTPClient.h>
#include <string.h>
#include "AsyncJson.h"

#include "SML.h"

template <class HMSYSTEM>

class ZeroExport {

//    bool enabled;                         // true
//    uint8_t query_device;                 // 0 - Tibber, 1 - Shelly, 2 - other (rs232?)
//    char monitor_url[ZEXPORT_ADDR_LEN];   //
//    char json_path[ZEXPORT_ADDR_LEN];     //
//    char tibber_pw[10];                   //
//    uint8_t Iv;                           // Id gemäß anlegen
//    float power_avg;                      //
//    uint8_t count_avg;                    //
//    double total_power;                   //
//    unsigned long lastTime;               // tic toc
//    double max_power;                     // 600 W
//    bool two_percent;                     // P >= 2% von max_power

//  mCfg                                    // -> siehe oben
//  mSys                                    // ->
//  mConfig                                 // ->

   public:
    ZeroExport() {
        mIsInitialized = false;
    }

    void setup(zeroExport_t *cfg, HMSYSTEM *sys, settings_t *config) {
        mCfg     = cfg;
        mSys     = sys;
        mConfig  = config;

        if (!mCfg->enabled) {
            return;
        }

        mIsInitialized = true;
    }

    void loop(void) {
        if ((!mIsInitialized) || (!mCfg->enabled)) {
            return;
        }

        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            if (!mCfg->groups[group].enabled) {
                continue;
            }

            switch (mCfg->groups[group].state) {
                case zeroExportState::RESET:
//DBGPRINT(F("State::RESET: "));
//DBGPRINTLN(String(group));
                    mCfg->groups[group].lastRun = millis();
                    // Weiter zum nächsten State
                    mCfg->groups[group].state = zeroExportState::GETPOWERMETER;
                    break;
                case zeroExportState::GETPOWERMETER:
                    if ((millis() - mCfg->groups[group].lastRun) > (mCfg->groups[group].refresh * 1000UL)) {
//DBGPRINT(F("State::GETPOWERMETER:"));
//DBGPRINTLN(String(group));
                        if (getPowermeterWatts(group)) {
                            // Weiter zum nächsten State
                            mCfg->groups[group].state = zeroExportState::GETINVERTERPOWER;
                        } else {
                            // Wartezeit wenn Keine Daten vom Powermeter
                            mCfg->groups[group].lastRun = (millis() - (mCfg->groups[group].refresh * 100UL));
                        }
                    }
                    break;
                case zeroExportState::GETINVERTERPOWER:
                    if ((millis() - mCfg->groups[group].lastRun) > (mCfg->groups[group].refresh * 1000UL)) {
//DBGPRINT(F("State::GETINVERTERPOWER:"));
//DBGPRINTLN(String(group));
                        if (getInverterPowerWatts(group)) {
                            // Weiter zum nächsten State
                            mCfg->groups[group].state = zeroExportState::FINISH;
                        } else {
                            // Wartezeit wenn Keine Daten vom Powermeter
                            mCfg->groups[group].lastRun = (millis() - (mCfg->groups[group].refresh * 100UL));
                        }
                    }
                    break;
/*
                case 4:
                    //
                    mCfg->groups[group].state = zeroExportState::RESET;
                    break;
                case 5:
                    //
                    mCfg->groups[group].state = zeroExportState::RESET;
                    break;
*/
                default:
                    //
//DBGPRINT(F("State::?: "));
//DBGPRINTLN(String(group));
                    mCfg->groups[group].state = zeroExportState::RESET;
                    break;
            }














/*
            if (!mCfg->groups[group].powermeter.error) {
DBGPRINTLN(String("ok verarbeiten."));
            }

            if (mCfg->groups[group].powermeter.error >= ZEROEXPORT_POWERMETER_MAX_ERRORS) {
DBGPRINTLN(String("nok Notmodus."));
            }
*/

/*
            // getPowermeterGroup
            mCfg->zeGroup[group].PowermeterSum = getPowermeterSum();
            // getPowermeterPhase
            for (uint8_t phase = 1; phase <= 3; phase++) {
                mCfg->zeGroup[group].PowermeterPhase[phase] = getPowermeterPhase();
            }
            // getInverterPower
            for (uint8_t inv = 0; inv < MAX; inv++) {
                mCfg->zeGroup[group].InverterpowerGroup = getInverterpowerGroup();
                for (uint8_t phase = 1; phase <= 3; phase++) {
                    mCfg->zeGroup[group].InverterpowerPhase[phase] = getInverterpowerPhase();
                }
            }
            // calcPowerSum
            mCfg->zeGroup[group].LimitSumNew = mCfg->zeGroup[group].LimitSumOld + mCfg->zeGroup[group].PowermeterSum;
            // calcPowerPhase
            for (uint8_t phase = 1; phase <= 3; phase++) {
                mCfg->zeGroup[group].LimitPhaseNew[phase] = mCfg->zeGroup[group].LimitPhaseOld[phase] - mCfg->zeGroup[group].PowermeterPhase[phase];
            }
            // calcPowerInverter
            for (uint8_t inv = 0; inv < MAX; inv++) {

            }
*/
        }

        // setPower
        for (uint8_t group = 0; group < ZEROEXPORT_MAX_GROUPS; group++) {
            if (!mCfg->groups[group].enabled) {
                continue;
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
        }

    }

    void tickerSecond() {
        if ((!mIsInitialized) || (!mCfg->enabled)) {
            return;
        }





        //DPRINTLN(DBG_INFO, (F("tickerSecond()")));
//        if (millis() - mCfg->lastTime < mCfg->count_avg * 1000UL) {
///            zero(); // just refresh when it is needed. To get cpu load low.
//        }
    }

    // Sums up the power values ​​of all phases and returns them.
    // If the value is negative, all power values ​​from the inverter are taken into account
    double getPowertoSetnewValue()
    {
/*
        float ivPower = 0;
        Inverter<> *iv;
        record_t<> *rec;
        for (uint8_t i = 0; i < mSys->getNumInverters(); i++) {
            iv = mSys->getInverterByPos(i);
            rec = iv->getRecordStruct(RealTimeRunData_Debug);
            if (iv == NULL)
                continue;
            ivPower += iv->getChannelFieldValue(CH0, FLD_PAC, rec);
        }
*/
//        return ((unsigned)(mCfg->total_power - mCfg->power_avg) >= mCfg->power_avg) ?  ivPower + mCfg->total_power : ivPower - mCfg->total_power;
        return 0;
    }
    //C2T2-B91B
    private:
        HTTPClient httpClient;

        // TODO: Need to improve here. 2048 for a JSON Obj is to big!?
        bool zero()
        {
/*
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
*/
            return true;
        }

        void tibber_parse()
        {

        }




        bool getPowermeterWatts(uint8_t group) {
            bool ret = false;
DBGPRINT(String("getPowermeterWatts: "));
DBGPRINTLN(String(group));
            switch (mCfg->groups[group].pm_type) {
                case 1:
                    ret = getPowermeterWattsShelly(group);
                    break;
//                case 2:
//                    ret = getPowermeterWattsTasmota(group);
//                    break;
//                case 3:
//                    ret = getPowermeterWattsMqtt(group);
//                    break;
//                case 4:
//                    ret = getPowermeterWattsHichi(group);
//                    break;
//                case 5:
//                    ret = getPowermeterWattsTibber(group);
//                    break;
///                default:
///                    ret = false;
///                    break;
            }
            return ret;
        }

        int getPowermeterWattsShelly(uint8_t group) {
            bool ret = false;
            HTTPClient http;
//            httpClient.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            httpClient.setUserAgent("Ahoy-Agent");
// TODO: Ahoy-0.8.850024-zero
//            httpClient.setConnectTimeout(1000);
//            httpClient.setTimeout(1000);
//            httpClient.addHeader("Content-Type", "application/json");
//            httpClient.addHeader("Accept", "application/json");
            http.begin(mCfg->groups[group].pm_url);
            if (http.GET() == HTTP_CODE_OK)
            {

                // Parsing
                DynamicJsonDocument doc(2048);
                DeserializationError error = deserializeJson(doc, http.getString());
                if (error)
                {
DBGPRINT(String("deserializeJson() failed: "));
DBGPRINTLN(String(error.c_str()));
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
DBGPRINT(String("pmPowerL1: "));
DBGPRINTLN(String(mCfg->groups[group].pmPowerL1));

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
DBGPRINT(String("pmPowerL2: "));
DBGPRINTLN(String(mCfg->groups[group].pmPowerL2));

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
DBGPRINT(String("pmPowerL3: "));
DBGPRINTLN(String(mCfg->groups[group].pmPowerL3));

DBGPRINT(String("pmPower: "));
DBGPRINTLN(String(mCfg->groups[group].pmPower));
            }
            http.end();

            return ret;
        }

        int getPowermeterWattsTasmota(void) {
/*
            HTTPClient http;
            char url[100] = "http://";
            strcat(url, mCfg->monitor_url);
            strcat(url, "/cm?cmd=status%2010");
            http.begin(url);

            if (http.GET() == 200) {
                // Parsing
                DynamicJsonDocument doc(384);
                DeserializationError error = deserializeJson(doc, http.getString());
                if (error) {
                    Serial.print("deserializeJson() failed: ");
                    Serial.println(error.c_str());
                    return 0;
                }

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
//            }
//            http.end();
            return 0;
        }

        int getPowermeterWattsMqtt(void) {
            // TODO:
            return 0;
        }

        int getPowermeterWattsHichi(void) {
            // TODO:
            return 0;
        }

        int getPowermeterWattsTibber(void) {
            // TODO:
            return 0;
        }



        bool getInverterPowerWatts(uint8_t group) {
            bool ret = false;
DBGPRINT(String("getInverterPowerWatts: "));
DBGPRINTLN(String(group));
//            switch (mCfg->groups[group].pm_type) {



            return ret;
        }






        // private member variables
        bool mIsInitialized = false;
        zeroExport_t *mCfg;
        settings_t *mConfig;
        HMSYSTEM *mSys;
};



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






















*/

#endif /*__ZEROEXPORT__*/

#endif /* #if defined(ESP32) */