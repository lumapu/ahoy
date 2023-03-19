//-----------------------------------------------------------------------------
// 2023 Ahoy, https://ahoydtu.de
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __PUB_MQTT_DEFS_H__
#define __PUB_MQTT_DEFS_H__

#include <Arduino.h>

enum {
    STR_TRUE,
    STR_FALSE
};

const char* const dict[] PROGMEM = {
    "true",
    "false"
};

enum {
    MQTT_STR_LWT_CONN,
    MQTT_STR_LWT_NOT_CONN,
    MQTT_STR_AVAILABLE,
    MQTT_STR_LAST_SUCCESS,
    MQTT_STR_TOTAL,
    MQTT_STR_GOT_TOPIC
};

const char* const mqttStr[] PROGMEM = {
    "connected",
    "not connected",
    "available",
    "last_success",
    "total",
    "MQTT got topic: "
};


enum {
    MQTT_UPTIME = 0,
    MQTT_RSSI,
    MQTT_FREE_HEAP,
    MQTT_HEAP_FRAG,
    MQTT_SUNRISE,
    MQTT_SUNSET,
    MQTT_COMM_START,
    MQTT_COMM_STOP,
    MQTT_DIS_NIGHT_COMM,
    MQTT_COMM_DISABLED,
    MQTT_COMM_DIS_TS,
    MQTT_VERSION,
    MQTT_DEVICE,
    MQTT_IP_ADDR,
    MQTT_STATUS,
    MQTT_ALARM,
    MQTT_ALARM_START,
    MQTT_ALARM_END,
    MQTT_LWT_ONLINE,
    MQTT_LWT_OFFLINE,
    MQTT_ACK_PWR_LMT
};

const char* const subtopics[] PROGMEM = {
    "uptime",
    "wifi_rssi",
    "free_heap",
	"heap_frag",
    "sunrise",
    "sunset",
    "comm_start",
    "comm_stop",
    "dis_night_comm",
    "comm_disabled",
    "comm_dis_ts",
    "version",
    "device",
    "ip_addr",
    "status",
    "alarm",
    "alarm_start",
    "alarm_end",
    "connected",
    "not_connected",
    "ack_pwr_limit"
};

enum {
    MQTT_SUBS_SET_TIME
};

const char* const subscr[] PROGMEM = {
    "setup/set_time"
};

#endif /*__PUB_MQTT_DEFS_H__*/
