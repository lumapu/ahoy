//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __POWERMETER_H__
#define __POWERMETER_H__

#include <AsyncJson.h>
#include <base64.h>
#include <HTTPClient.h>
#include <string.h>
#include <list>

#include "config/settings.h"
#include "plugins/zeroExport/lib/sml.h"

typedef struct {
    const unsigned char OBIS[6];
    void (*Fn)(double &);
    float *Arg;
} OBISHandler;

class powermeter {
   public:
    powermeter() {
    }

    ~powermeter() {
    }

    bool setup(zeroExport_t *cfg /*Hier muss noch geklärt werden was gebraucht wird*/) {
        mCfg = cfg;

        return true;
    }

    void loop(void) {
    }

    /** groupGetPowermeter
     * Holt die Daten vom Powermeter
     * @param group
     * @returns true/false
     */
    bool getData(JsonObject logObj, uint8_t group) {
        bool result = false;
        switch (mCfg->groups[group].pm_type) {
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
            logObj["err"] = "type: " + String(mCfg->groups[group].pm_type);
        }

        return result;
    }

   private:
    /** getPowermeterWattsShelly
     * ...
     * @param logObj
     * @param group
     * @returns true/false
     */
    bool getPowermeterWattsShelly(JsonObject logObj, uint8_t group) {
        bool result = false;

        logObj["mod"] = "getPowermeterWattsShelly";

        mCfg->groups[group].pmPower = 0;
        mCfg->groups[group].pmPowerL1 = 0;
        mCfg->groups[group].pmPowerL2 = 0;
        mCfg->groups[group].pmPowerL3 = 0;

        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.setUserAgent("Ahoy-Agent");
        // TODO: Ahoy-0.8.850024-zero
        http.setConnectTimeout(1000);
        http.setTimeout(1000);
        // TODO: Timeout von 1000 reduzieren?
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Accept", "application/json");

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
                if (doc.containsKey(F("total_power"))) {
                    // Shelly 3EM
                    mCfg->groups[group].pmPower = doc["total_power"];
                    result = true;
                } else if (doc.containsKey(F("em:0"))) {
                    // Shelly pro 3EM
                    mCfg->groups[group].pmPower = doc["em:0"]["total_act_power"];
                    result = true;
                } else {
                    // Keine Daten
                    mCfg->groups[group].pmPower = 0;
                }

                if (doc.containsKey(F("emeters"))) {
                    // Shelly 3EM
                    mCfg->groups[group].pmPowerL1 = doc["emeters"][0]["power"];
                    result = true;
                } else if (doc.containsKey(F("em:0"))) {
                    // Shelly pro 3EM
                    mCfg->groups[group].pmPowerL1 = doc["em:0"]["a_act_power"];
                    result = true;
                } else if (doc.containsKey(F("switch:0"))) {
                    // Shelly plus1pm plus2pm
                    mCfg->groups[group].pmPowerL1 = doc["switch:0"]["apower"];
                    mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL1;
                    result = true;
                } else if (doc.containsKey(F("apower"))) {
                    // Shelly Alternative
                    mCfg->groups[group].pmPowerL1 = doc["apower"];
                    mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL1;
                    result = true;
                } else {
                    // Keine Daten
                    mCfg->groups[group].pmPowerL1 = 0;
                }

                if (doc.containsKey(F("emeters"))) {
                    // Shelly 3EM
                    mCfg->groups[group].pmPowerL2 = doc["emeters"][1]["power"];
                    result = true;
                } else if (doc.containsKey(F("em:0"))) {
                    // Shelly pro 3EM
                    mCfg->groups[group].pmPowerL2 = doc["em:0"]["b_act_power"];
                    result = true;
                } else if (doc.containsKey(F("switch:1"))) {
                    // Shelly plus1pm plus2pm
                    mCfg->groups[group].pmPowerL2 = doc["switch.1"]["apower"];
                    mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL2;
                    result = true;
                    //} else if (doc.containsKey(F("apower"))) {
                    // Shelly Alternative
                    //    mCfg->groups[group].pmPowerL2 = doc["apower"];
                    //    mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL2;
                    //    ret = true;
                } else {
                    // Keine Daten
                    mCfg->groups[group].pmPowerL2 = 0;
                }

                if (doc.containsKey(F("emeters"))) {
                    // Shelly 3EM
                    mCfg->groups[group].pmPowerL3 = doc["emeters"][2]["power"];
                    result = true;
                } else if (doc.containsKey(F("em:0"))) {
                    // Shelly pro 3EM
                    mCfg->groups[group].pmPowerL3 = doc["em:0"]["c_act_power"];
                    result = true;
                } else if (doc.containsKey(F("switch:2"))) {
                    // Shelly plus1pm plus2pm
                    mCfg->groups[group].pmPowerL3 = doc["switch:2"]["apower"];
                    mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL3;
                    result = true;
                    //} else if (doc.containsKey(F("apower"))) {
                    // Shelly Alternative
                    //    mCfg->groups[group].pmPowerL3 = doc["apower"];
                    //    mCfg->groups[group].pmPower += mCfg->groups[group].pmPowerL3;
                    //    result = true;
                } else {
                    // Keine Daten
                    mCfg->groups[group].pmPowerL3 = 0;
                }
            }
        }
        http.end();

        logObj["P"] = mCfg->groups[group].pmPower;
        logObj["P1"] = mCfg->groups[group].pmPowerL1;
        logObj["P2"] = mCfg->groups[group].pmPowerL2;
        logObj["P3"] = mCfg->groups[group].pmPowerL3;

        return result;
    }

    /** getPowermeterWattsTasmota
     * ...
     * @param logObj
     * @param group
     * @returns true/false
     *
     * Vorlage:
     * http://IP/cm?cmnd=Status0
     * {
     *  "Status":{
     *      "Module":1,
     *      "DeviceName":"Tasmota",
     *      "FriendlyName":["Tasmota"],
     *      "Topic":"Tasmota",
     *      "ButtonTopic":"0",
     *      "Power":0,
     *      "PowerOnState":3,
     *      "LedState":1,
     *      "LedMask":"FFFF",
     *      "SaveData":1,
     *      "SaveState":1,
     *      "SwitchTopic":"0",
     *      "SwitchMode":[0,0,0,0,0,0,0,0],
     *      "ButtonRetain":0,
     *      "SwitchRetain":0,
     *      "SensorRetain":0,
     *      "PowerRetain":0,
     *      "InfoRetain":0,
     *      "StateRetain":0},
     *      "StatusPRM":{"Baudrate":9600,"SerialConfig":"8N1","GroupTopic":"tasmotas","OtaUrl":"http://ota.tasmota.com/tasmota/release/tasmota.bin.gz","RestartReason":"Software/System restart","Uptime":"202T01:24:51","StartupUTC":"2023-08-13T15:21:13","Sleep":50,"CfgHolder":4617,"BootCount":27,"BCResetTime":"2023-02-04T16:45:38","SaveCount":150,"SaveAddress":"F5000"},
     *      "StatusFWR":{"Version":"11.1.0(tasmota)","BuildDateTime":"2022-05-05T03:23:22","Boot":31,"Core":"2_7_4_9","SDK":"2.2.2-dev(38a443e)","CpuFrequency":80,"Hardware":"ESP8266EX","CR":"378/699"},
     *      "StatusLOG":{"SerialLog":0,"WebLog":2,"MqttLog":0,"SysLog":0,"LogHost":"","LogPort":514,"SSId":["Odyssee2001",""],"TelePeriod":300,"Resolution":"558180C0","SetOption":["00008009","2805C80001000600003C5A0A190000000000","00000080","00006000","00004000"]},
     *      "StatusMEM":{"ProgramSize":658,"Free":344,"Heap":17,"ProgramFlashSize":1024,"FlashSize":1024,"FlashChipId":"14325E","FlashFrequency":40,"FlashMode":3,"Features":["00000809","87DAC787","043E8001","000000CF","010013C0","C000F989","00004004","00001000","04000020"],"Drivers":"1,2,3,4,5,6,7,8,9,10,12,16,18,19,20,21,22,24,26,27,29,30,35,37,45,56,62","Sensors":"1,2,3,4,5,6,53"},
     *      "StatusNET":{"Hostname":"Tasmota","IPAddress":"192.168.100.81","Gateway":"192.168.100.1","Subnetmask":"255.255.255.0","DNSServer1":"192.168.100.1","DNSServer2":"0.0.0.0","Mac":"4C:11:AE:11:F8:50","Webserver":2,"HTTP_API":1,"WifiConfig":4,"WifiPower":17.0},
     *      "StatusMQT":{"MqttHost":"192.168.100.80","MqttPort":1883,"MqttClientMask":"Tasmota","MqttClient":"Tasmota","MqttUser":"mqttuser","MqttCount":156,"MAX_PACKET_SIZE":1200,"KEEPALIVE":30,"SOCKET_TIMEOUT":4},
     *      "StatusTIM":{"UTC":"2024-03-02T16:46:04","Local":"2024-03-02T17:46:04","StartDST":"2024-03-31T02:00:00","EndDST":"2024-10-27T03:00:00","Timezone":"+01:00","Sunrise":"07:29","Sunset":"18:35"},
     *      "StatusSNS":{
     *          "Time":"2024-03-02T17:46:04",
     *          "PV":{
     *              "Bezug":0.364,
     *              "Einspeisung":3559.439,
     *              "Leistung":-14
     *          }
     *      },
     *      "StatusSTS":{"Time":"2024-03-02T17:46:04","Uptime":"202T01:24:51","UptimeSec":17457891,"Heap":16,"SleepMode":"Dynamic","Sleep":50,"LoadAvg":19,"MqttCount":156,"POWER":"OFF","Wifi":{"AP":1,"SSId":"Odyssee2001","BSSId":"34:31:C4:22:92:74","Channel":6,"Mode":"11n","RSSI":100,"Signal":-50,"LinkCount":15,"Downtime":"0T00:08:22"}
     *      }
     *  }
     */
    bool getPowermeterWattsTasmota(JsonObject logObj, uint8_t group) {
        bool result = false;

        logObj["mod"] = "getPowermeterWattsTasmota";

        mCfg->groups[group].pmPower = 0;
        mCfg->groups[group].pmPowerL1 = 0;
        mCfg->groups[group].pmPowerL2 = 0;
        mCfg->groups[group].pmPowerL3 = 0;

        result = true;
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

    /** getPowermeterWattsMqtt
     * ...
     * @param logObj
     * @param group
     * @returns true/false
     */
    bool getPowermeterWattsMqtt(JsonObject logObj, uint8_t group) {
        bool result = false;

        logObj["mod"] = "getPowermeterWattsMqtt";

        mCfg->groups[group].pmPower = 0;
        mCfg->groups[group].pmPowerL1 = 0;
        mCfg->groups[group].pmPowerL2 = 0;
        mCfg->groups[group].pmPowerL3 = 0;

        // Hier neuer Code - Anfang
        // TODO: Noch nicht komplett

        result = true;

        // Hier neuer Code - Ende

        logObj["P"] = mCfg->groups[group].pmPower;
        logObj["P1"] = mCfg->groups[group].pmPowerL1;
        logObj["P2"] = mCfg->groups[group].pmPowerL2;
        logObj["P3"] = mCfg->groups[group].pmPowerL3;

        return result;
    }

    /** getPowermeterWattsHichi
     * ...
     * @param logObj
     * @param group
     * @returns true/false
     */
    bool getPowermeterWattsHichi(JsonObject logObj, uint8_t group) {
        bool result = false;

        logObj["mod"] = "getPowermeterWattsHichi";

        mCfg->groups[group].pmPower = 0;
        mCfg->groups[group].pmPowerL1 = 0;
        mCfg->groups[group].pmPowerL2 = 0;
        mCfg->groups[group].pmPowerL3 = 0;

        // Hier neuer Code - Anfang
        // TODO: Noch nicht komplett

        result = true;

        // Hier neuer Code - Ende

        logObj["P"] = mCfg->groups[group].pmPower;
        logObj["P1"] = mCfg->groups[group].pmPowerL1;
        logObj["P2"] = mCfg->groups[group].pmPowerL2;
        logObj["P3"] = mCfg->groups[group].pmPowerL3;

        return result;
    }

    /** getPowermeterWattsTibber
     * ...
     * @param logObj
     * @param group
     * @returns true/false
     * @TODO: Username & Passwort wird mittels base64 verschlüsselt. Dies wird für die Authentizierung benötigt. Wichtig diese im WebUI unkenntlich zu machen und base64 im eeprom zu speichern, statt klartext.
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
        {{0x01, 0x00, 0x10, 0x07, 0x00, 0xff}, &smlOBISW, &_powerMeterTotal},  // total - OBIS-Kennzahl für momentane Gesamtwirkleistung

        {{0x01, 0x00, 0x24, 0x07, 0x00, 0xff}, &smlOBISW, &_powerMeter1Power},  // OBIS-Kennzahl für momentane Wirkleistung in Phase L1
        {{0x01, 0x00, 0x38, 0x07, 0x00, 0xff}, &smlOBISW, &_powerMeter2Power},  // OBIS-Kennzahl für momentane Wirkleistung in Phase L2
        {{0x01, 0x00, 0x4c, 0x07, 0x00, 0xff}, &smlOBISW, &_powerMeter3Power},  // OBIS-Kennzahl für momentane Wirkleistung in Phase L3

        {{0x01, 0x00, 0x01, 0x08, 0x00, 0xff}, &smlOBISWh, &_powerMeterImport},
        {{0x01, 0x00, 0x02, 0x08, 0x00, 0xff}, &smlOBISWh, &_powerMeterExport}};

    bool getPowermeterWattsTibber(JsonObject logObj, uint8_t group) {
        bool result = false;

        mCfg->groups[group].pmPower = 0;
        mCfg->groups[group].pmPowerL1 = 0;
        mCfg->groups[group].pmPowerL2 = 0;
        mCfg->groups[group].pmPowerL3 = 0;

        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.setUserAgent("Ahoy-Agent");
        http.setConnectTimeout(1000);
        http.setTimeout(1000);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Accept", "application/json");

        String url = String("http://") + mCfg->groups[group].pm_url + String("/") + String(mCfg->groups[group].pm_jsonPath);
        String auth = base64::encode(String(mCfg->groups[group].pm_user) + String(":") + String(mCfg->groups[group].pm_pass));

        http.begin(url);
        http.addHeader("Authorization", "Basic " + auth);

        if (http.GET() == HTTP_CODE_OK) {
            String myString = http.getString();

            char floatBuffer[20];
            double readVal = 0;

            unsigned char c;
            for (int i = 0; i < http.getSize(); ++i) {
                c = myString[i];
                sml_states_t smlCurrentState = smlState(c);

                switch (smlCurrentState) {
                    case SML_FINAL:
                        mCfg->groups[group].pmPower = _powerMeterTotal;

                        mCfg->groups[group].pmPowerL1 = _powerMeter1Power;
                        mCfg->groups[group].pmPowerL2 = _powerMeter2Power;
                        mCfg->groups[group].pmPowerL3 = _powerMeter3Power;

                        if(! (_powerMeter1Power && _powerMeter2Power && _powerMeter3Power))
                        {
                            mCfg->groups[group].pmPowerL1 = _powerMeterTotal / 3;
                            mCfg->groups[group].pmPowerL2 = _powerMeterTotal / 3;
                            mCfg->groups[group].pmPowerL3 = _powerMeterTotal / 3;
                        }

// TODO: Ein return an dieser Stelle verhindert das ordnungsgemäße http.end()
                        result = true;
//                        return true;
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

        logObj["P"] = mCfg->groups[group].pmPower;
        logObj["P1"] = mCfg->groups[group].pmPowerL1;
        logObj["P2"] = mCfg->groups[group].pmPowerL2;
        logObj["P3"] = mCfg->groups[group].pmPowerL3;

        return result;
    }

   private:
    zeroExport_t *mCfg;
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
*/

#endif /*__POWERMETER_H__*/
