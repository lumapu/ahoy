#include <string.h>
#include <stdlib.h>
#include <HardwareSerial.h>
#include <user_interface.h>
#include "../utils/dbg.h"
#include "../utils/scheduler.h"
#include "../config/settings.h"
#include "SML_OBIS_Parser.h"

#ifdef AHOY_SML_OBIS_SUPPORT

// you might use this testwise if you dont have an IR sensor connected to your AHOY-DTU
// #define SML_OBIS_TEST

// at least the size of the largest entry that is of any interest
#define SML_MAX_SERIAL_BUF 32

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define SML_ESCAPE_CHAR   0x1b
#define SML_VERSION1_CHAR 0x01
#define SML_MAX_LIST_LAYER 8

#define SML_EXT_LENGTH 0x80
#define SML_OBIS_GRID_POWER_PATH AHOY_HIST_PATH "/grid_power"
#define SML_OBIS_FORMAT_FILE_NAME "%02u_%02u_%04u.bin"

#define SML_MSG_GET_LIST_RSP        0x701

#define OBIS_SIG_YIELD_IN_ALL       "\x01\x08\x00"
#define OBIS_SIG_YIELD_OUT_ALL      "\x02\x08\x00"
#define OBIS_SIG_POWER_ALL          "\x10\x07\x00"
#define OBIS_SIG_POWER_L1           "\x24\x07\x00"
#define OBIS_SIG_POWER_L2           "\x38\x07\x00"
#define OBIS_SIG_POWER_L3           "\x4c\x07\x00"
/* the folloing OBIS objects may not be transmitted by your electricity meter */
#define OBIS_SIG_VOLTAGE_L1         "\x20\x07\x00"
#define OBIS_SIG_VOLTAGE_L2         "\x34\x07\x00"
#define OBIS_SIG_VOLTAGE_L3         "\x48\x07\x00"
#define OBIS_SIG_CURRENT_L1         "\x1f\x07\x00"
#define OBIS_SIG_CURRENT_L2         "\x33\x07\x00"
#define OBIS_SIG_CURRENT_L3         "\x47\x07\x00"

typedef enum _sml_state {
    SML_ST_FIND_START_TAG = 0,
    SML_ST_FIND_VERSION,
    SML_ST_FIND_MSG,
    SML_ST_FIND_LIST_ENTRIES,
    SML_ST_SKIP_LIST_ENTRY,
    SML_ST_FIND_END_TAG,
    SML_ST_CHECK_CRC
} sml_state_t;

typedef enum _sml_list_entry_type {
    SML_TYPE_OCTET_STRING = 0x00,
    SML_TYPE_BOOL = 0x40,
    SML_TYPE_INT = 0x50,
    SML_TYPE_UINT = 0x60,
    SML_TYPE_LIST = 0x70
} sml_list_entry_type_t;

typedef enum _obis_state {
    OBIS_ST_NONE = 0,
    OBIS_ST_SERIAL_NR,
    OBIS_ST_YIELD_IN_ALL,
    OBIS_ST_YIELD_OUT_ALL,
    OBIS_ST_POWER_ALL,
    OBIS_ST_POWER_L1,
    OBIS_ST_POWER_L2,
    OBIS_ST_POWER_L3,
    OBIS_ST_VOLTAGE_L1,
    OBIS_ST_VOLTAGE_L2,
    OBIS_ST_VOLTAGE_L3,
    OBIS_ST_CURRENT_L1,
    OBIS_ST_CURRENT_L2,
    OBIS_ST_CURRENT_L3,
    OBIS_ST_UNKNOWN
} obis_state_t;

static sml_state_t sml_state = SML_ST_FIND_START_TAG;
static uint16_t cur_sml_list_layer;
static unsigned char sml_list_layer_entries [SML_MAX_LIST_LAYER];
static unsigned char sml_serial_buf[SML_MAX_SERIAL_BUF];
static unsigned char *cur_serial_buf = sml_serial_buf;
static uint16 sml_serial_len = 0;
static uint16 sml_skip_len = 0;
static obis_state_t obis_state = OBIS_ST_NONE;
static int obis_power_all_scale, obis_power_all_value;
/* design: max 16 bit fuer aktuelle Powerwerte */
static int16_t obis_cur_pac;
static uint16_t obis_crc;
static uint16_t obis_cur_pac_cnt;
static uint16_t obis_cur_pac_index;
static int32_t obis_pac_sum;
static uint32_t *obis_timestamp;
static int obis_yield_in_all_scale, obis_yield_out_all_scale;
static uint64_t obis_yield_in_all_value, obis_yield_out_all_value;
static bool sml_trace_obis = false;
static IApp *mApp;

const unsigned char version_seq[] = { SML_VERSION1_CHAR, SML_VERSION1_CHAR, SML_VERSION1_CHAR, SML_VERSION1_CHAR };
const unsigned char esc_seq[] = {SML_ESCAPE_CHAR, SML_ESCAPE_CHAR, SML_ESCAPE_CHAR, SML_ESCAPE_CHAR};

#ifdef SML_OBIS_TEST
static size_t sml_test_telegram_offset;
const unsigned char sml_test_telegram[] = {
    0x1b, 0x1b, 0x1b, 0x1b,                     // Escapesequenz
    0x01, 0x01, 0x01, 0x01,                     // Version 1
    0x76,                                       // Liste mit 6 Eintraegen (1. SML Nachricht dieses Telegramms)
    	0x05, 0x03, 0x2b, 0x18, 0x20,
    	0x62, 0x00,
    	0x62, 0x00,
    	0x72,
    		0x63, 0x01, 0x01,                   // Messagetyp: OpenResponse
    		0x76,
    			0x01,
    			0x01,
    			0x05, 0x01, 0x0e, 0x5d, 0x5b,
    			0x0b, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    			0x01,
    			0x01,
    	0x63, 0x53, 0x34,
    	0x00,
    0x76,                                       // Liste mit 6 Eintraegen (2. SML Nachricht dieses Telegramms)
    	0x05, 0x03, 0x2b, 0x18, 0x21,
    	0x62, 0x00,
    	0x62, 0x00,
    	0x72,
    		0x63, 0x07, 0x01,                   // Messagetyp: GetListResponse
    		0x77,
    			0x01,
    			0x0b, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    			0x07, 0x01, 0x00, 0x62, 0x0a, 0xff, 0xff,
    			0x72,
    				0x62, 0x01,
    				0x65, 0x02, 0x1a, 0x58, 0x7f,
    			0x7a,
    				0x77,
    					0x07, 0x01, 0x00, 0x01, 0x08, 0x00, 0xff, // OBIS Kennzahl fuer Wirkenergie Bezug gesamt tariflos
    					0x65, 0x00, 0x01, 0x01, 0x80,
    					0x01,
    					0x62, 0x1e,             // Einheit "Wh"
    					0x52, 0xff,             // Skalierung 0.1
    					0x59, 0x00, 0x00, 0x00, 0x00, 0x00, 0x85, 0xc3, 0x05, // Wert fuer Wirkenergie Bezug gesamt tariflos
    					0x01,
    				0x77,
    					0x07, 0x01, 0x00, 0x01, 0x08, 0x01, 0xff, // OBIS-Kennzahl fuer Wirkenergie Bezug Tarif 1
    					0x01,
    					0x01,
    					0x62, 0x1e,             // Einheit "Wh"
    					0x52, 0xff,             // Skalierung 0.1
    					0x59, 0x00, 0x00, 0x00, 0x00, 0x00, 0x85, 0xc3, 0x05, // Wert fuer Wirkenergie Bezug Tarif 1
    					0x01,
    				0x77,
    					0x07, 0x01, 0x00, 0x01, 0x08, 0x02, 0xff, // OBIS-Kennzahl fuer Wirkenergie Bezug Tarif 2
    					0x01,
    					0x01,
    					0x62, 0x1e,             // Einheit "Wh"
    					0x52, 0xff,	            // Skalierung 0.1
    					0x59, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Wert fuer Wirkenergie Bezug Tarif 2
    					0x01,
    				0x77,
    					0x07, 0x01, 0x00, 0x02, 0x08, 0x00, 0xff, // OBIS-Kennzahl für Wirkenergie Einspeisung gesamt tariflos
    					0x01,
    					0x01,
    					0x62, 0x1e,             // Einheit "Wh"
    					0x52, 0xff,             // Skalierung 0.1
    					0x59, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Wert für Wirkenergie Einspeisung gesamt tariflos
    					0x01,
    				0x77,
    					0x07, 0x01, 0x00, 0x02, 0x08, 0x01, 0xff, // OBIS-Kennzahl für Wirkenergie Einspeisung Tarif1
    					0x01,
    					0x01,
    					0x62, 0x1e,             // Einheit "Wh"
    					0x52, 0xff,             // Skalierung 0.1
    					0x59, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Wert fuer Wirkenergie Einspeisung Tarif 1
    					0x01,
    				0x77,
    					0x07, 0x01, 0x00, 0x02, 0x08, 0x02, 0xff, // OBIS-Kennzahl für Wirkenergie Einspeisung Tarif 2
    					0x01,
    					0x01,
    					0x62, 0x1e,             // Einheit "Wh"
    					0x52, 0xff,             // Skalierung 0.1
    					0x59, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Wert für Wirkenergie Einspeisung Tarif 2
    					0x01,
    				0x77,
    					0x07, 0x01, 0x00, 0x10, 0x07, 0x00, 0xff, // OBIS-Kennzahl fuer momentane Gesamtwirkleistung
    					0x01,
    					0x01,
    					0x62, 0x1b,             // Einheit "W"
    					0x52, 0x00,             // Skalierung 1
    					0x55, 0x00, 0x00, 0x00, 0x2a, // Wert fuer momentane Gesamtwirkleistung
    					0x01,
    				0x77,
    					0x07, 0x01, 0x00, 0x24, 0x07, 0x00, 0xff, // OBIS-Kennzahl fuer momentane Wirkleistung L1
    					0x01,
    					0x01,
    					0x62, 0x1b,             // Einheit "W"
    					0x52, 0x00,             // Skalierung 1
    					0x55, 0x00, 0x00, 0x00, 0x2a, // Wert fuer momentane Wirkleistung L1
    					0x01,
    				0x77,
    					0x07, 0x01, 0x00, 0x38, 0x07, 0x00, 0xff, // OBIS-Kennzahl fuer momentane Wirkleistung L2
    					0x01,
    					0x01,
    					0x62, 0x1b,             // Einheit "W"
    					0x52, 0x00,             // Skalierung 1
    					0x55, 0x00, 0x00, 0x00, 0x00, // Wert für momentane Wirkleistung L2
    					0x01,
    				0x77,
    					0x07, 0x01, 0x00, 0x4c, 0x07, 0x00, 0xff, // OBIS-Kennzahl fuer momentane Wirkleistung L3
    					0x01,
    					0x01,
    					0x62, 0x1b,             // Einheit "W"
    					0x52, 0x00,             // Skalierung 1
    					0x55, 0x00, 0x00, 0x00, 0x00, // Wert fuer momentane Wirkleistung L3
    					0x01,
    			0x01,
    			0x01,
    	0x63, 0x23, 0x59,
    	0x00,
    0x76,                                       // Liste mit 6 Eintraegen (3. SML Nachricht dieses Telegramms)
    	0x05, 0x03, 0x2b, 0x18, 0x22,
    	0x62, 0x00,
    	0x62, 0x00,
    	0x72,
    		0x63, 0x02, 0x01,                   // Messagetyp: CloseResponse
    		0x71,
    			0x01,
    	0x63, 0x91, 0x26,
    	0x00,
    0x1b, 0x1b, 0x1b, 0x1b,                     // Escapesequenz
    0x1a, 0x00, 0xbf, 0xd7	                    // 1a + Fuellbyte + CRC16 des gesamten Telegrammes (ggf. passend eintragen)
};
#endif

//-----------------------------------------------------------------------------
// DIN EN 62056-46, Polynom 0x1021
static const uint16_t obis_crctab[256] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf, 0x8c48, 0x9dc1, 0xaf5a, 0xbed3,
    0xca6c, 0xdbe5, 0xe97e, 0xf8f7, 0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876, 0x2102, 0x308b, 0x0210, 0x1399,
    0x6726, 0x76af, 0x4434, 0x55bd, 0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c, 0xbdcb, 0xac42, 0x9ed9, 0x8f50,
    0xfbef, 0xea66, 0xd8fd, 0xc974, 0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3, 0x5285, 0x430c, 0x7197, 0x601e,
    0x14a1, 0x0528, 0x37b3, 0x263a, 0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9, 0xef4e, 0xfec7, 0xcc5c, 0xddd5,
    0xa96a, 0xb8e3, 0x8a78, 0x9bf1, 0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70, 0x8408, 0x9581, 0xa71a, 0xb693,
    0xc22c, 0xd3a5, 0xe13e, 0xf0b7, 0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036, 0x18c1, 0x0948, 0x3bd3, 0x2a5a,
    0x5ee5, 0x4f6c, 0x7df7, 0x6c7e, 0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd, 0xb58b, 0xa402, 0x9699, 0x8710,
    0xf3af, 0xe226, 0xd0bd, 0xc134, 0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3, 0x4a44, 0x5bcd, 0x6956, 0x78df,
    0x0c60, 0x1de9, 0x2f72, 0x3efb, 0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a, 0xe70e, 0xf687, 0xc41c, 0xd595,
    0xa12a, 0xb0a3, 0x8238, 0x93b1, 0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330, 0x7bc7, 0x6a4e, 0x58d5, 0x495c,
    0x3de3, 0x2c6a, 0x1ef1, 0x0f78};

//-----------------------------------------------------------------------------
void sml_init_obis_crc ()
{
    obis_crc = 0xffff;
}

//-----------------------------------------------------------------------------
void sml_calc_obis_crc (unsigned int len, unsigned char *data)
{
    while (len--) {
        obis_crc = (obis_crc >> 8) ^ obis_crctab[(obis_crc ^ *data++) & 0xff];
    }
}

//-----------------------------------------------------------------------------
uint16_t sml_finit_obis_crc ()
{
    obis_crc ^= 0xffff;
    obis_crc = (obis_crc << 8) | (obis_crc >> 8);
    return obis_crc;
}

//-----------------------------------------------------------------------------
void sml_set_trace_obis (bool trace_flag)
{
    sml_trace_obis = trace_flag;
}

//-----------------------------------------------------------------------------
void sml_cleanup_history ()
{
    time_t time_today;

    time_today = *obis_timestamp;

    obis_cur_pac = 0;
    obis_cur_pac_cnt = 0;
    obis_cur_pac_index = 0;
    obis_pac_sum = 0;
    if (time_today) {
        Dir grid_power_dir;
        struct tm tm_today;
        char cur_file_name[sizeof (SML_OBIS_FORMAT_FILE_NAME)];

        localtime_r (&time_today, &tm_today);
        snprintf (cur_file_name, sizeof (cur_file_name), SML_OBIS_FORMAT_FILE_NAME,
            tm_today.tm_mday, tm_today.tm_mon+1, tm_today.tm_year + 1900);
        grid_power_dir = LittleFS.openDir (SML_OBIS_GRID_POWER_PATH);
        /* design: no dataserver, cleanup old history */

        while (grid_power_dir.next()) {
            if (grid_power_dir.fileName() != cur_file_name) {
                DPRINTLN (DBG_INFO, "Remove file " + grid_power_dir.fileName() +
                    ", Size: " + String (grid_power_dir.fileSize()));
                LittleFS.remove (SML_OBIS_GRID_POWER_PATH "/" + grid_power_dir.fileName());
            }
        }
    } else {
        DPRINTLN (DBG_WARN, "sml_cleanup_history, no time yet");
    }
}

//-----------------------------------------------------------------------------
File sml_open_hist ()
{
    File file = (File) NULL;

    if (*obis_timestamp) {
        char file_name[sizeof (SML_OBIS_GRID_POWER_PATH) + sizeof (SML_OBIS_FORMAT_FILE_NAME)];
        time_t time_today = *obis_timestamp;
        struct tm tm_today;

        localtime_r (&time_today, &tm_today);
        snprintf (file_name, sizeof (file_name), SML_OBIS_GRID_POWER_PATH "/" SML_OBIS_FORMAT_FILE_NAME,
            tm_today.tm_mday, tm_today.tm_mon+1, tm_today.tm_year + 1900);
        file = LittleFS.open (file_name, "r");
        if (!file) {
            DPRINT (DBG_WARN, "sml_open_hist, failed to open ");
            DBGPRINTLN (file_name);
        }
    } else {
        DPRINTLN (DBG_WARN, "sml_open_history, no time yet");
    }
    return file;
}

//-----------------------------------------------------------------------------
void sml_close_hist (File file)
{
    if (file) {
        file.close ();
    }
}

//-----------------------------------------------------------------------------
int sml_find_hist_power (File file, uint16_t index)
{
    if (file) {
        size_t len;
        uint16_t cmp_index = 0;     /* init wegen Compilerwarnung */
        unsigned char data[4];

        while ((len = file.read (data, sizeof (data))) == sizeof (data)) {
            cmp_index = data[0] + (data[1] << 8);
            if (cmp_index >= index) {
                break;
            }
//            yield();   /* do not do this here: seems to cause hanger */
        }
        if (len < sizeof (data)) {
            if (index == obis_cur_pac_index) {
                return sml_get_obis_pac_average ();
            }
            DPRINTLN (DBG_DEBUG, "sml_find_hist_power(1), cant find " + String (index));
            return -1;
        }
        if (cmp_index == index) {
            return (int16_t)(data[2] + (data[3] << 8));
        }
        DPRINTLN (DBG_DEBUG, "sml_find_hist_power(2), cant find " + String (index) + ", found " + String (cmp_index));
        file.seek (file.position() - sizeof (data));
    } else if ((index == obis_cur_pac_index) && obis_cur_pac_cnt) {
        return sml_get_obis_pac_average ();
    }
    return -1;
}

//-----------------------------------------------------------------------------
void sml_setup (IApp *app, uint32_t *timestamp)
{
    obis_timestamp = timestamp;
    mApp = app;
}

//-----------------------------------------------------------------------------
int16_t sml_get_obis_pac ()
{
    return obis_cur_pac;
}

//-----------------------------------------------------------------------------
void sml_handle_obis_state (unsigned char *buf)
{
#ifdef undef
    if (sml_trace_obis) {
        DPRINTLN(DBG_INFO, "OBIS " + String(buf[0], HEX) + "-" + String(buf[1], HEX) + ":" + String(buf[2], HEX) +
            "." + String (buf[3], HEX) + "." + String(buf[4], HEX) + "*" + String(buf[5], HEX));
    }
#endif
    if (obis_state != OBIS_ST_NONE) {
        if (buf[0] == 1) {
            if (!memcmp (&buf[2], OBIS_SIG_YIELD_IN_ALL, 3)) {
                obis_state = OBIS_ST_YIELD_IN_ALL;
            } else if (!memcmp (&buf[2], OBIS_SIG_YIELD_OUT_ALL, 3)) {
                obis_state = OBIS_ST_YIELD_OUT_ALL;
            } else if (!memcmp (&buf[2], OBIS_SIG_POWER_ALL, 3)) {
                obis_state = OBIS_ST_POWER_ALL;
            } else if (!memcmp (&buf[2], OBIS_SIG_POWER_L1, 3)) {
                obis_state = OBIS_ST_POWER_L1;
            } else if (!memcmp (&buf[2], OBIS_SIG_POWER_L2, 3)) {
                obis_state = OBIS_ST_POWER_L2;
            } else if (!memcmp (&buf[2], OBIS_SIG_POWER_L3, 3)) {
                obis_state = OBIS_ST_POWER_L3;
            } else if (!memcmp (&buf[2], OBIS_SIG_CURRENT_L1, 3)) {
                obis_state = OBIS_ST_CURRENT_L1;
            } else if (!memcmp (&buf[2], OBIS_SIG_CURRENT_L2, 3)) {
                obis_state = OBIS_ST_CURRENT_L2;
            } else if (!memcmp (&buf[2], OBIS_SIG_CURRENT_L3, 3)) {
                obis_state = OBIS_ST_CURRENT_L3;
            } else if (!memcmp (&buf[2], OBIS_SIG_VOLTAGE_L1, 3)) {
                obis_state = OBIS_ST_VOLTAGE_L1;
            } else if (!memcmp (&buf[2], OBIS_SIG_VOLTAGE_L2, 3)) {
                obis_state = OBIS_ST_VOLTAGE_L2;
            } else if (!memcmp (&buf[2], OBIS_SIG_VOLTAGE_L3, 3)) {
                obis_state = OBIS_ST_VOLTAGE_L3;
            } else {
                obis_state = OBIS_ST_UNKNOWN;
            }
        } else {
            obis_state = OBIS_ST_UNKNOWN;
        }
    }
}

//-----------------------------------------------------------------------------
int64_t sml_obis_get_uint (unsigned char *data, unsigned int len)
{
    int64_t value = 0;

    if (len > 8) {
        DPRINTLN(DBG_WARN, "Int too big");
    } else {
        unsigned int i;

        for (i=0; i<len; i++) {
            value = (value << 8) | *data++;
        }
    }
    return value;
}

//-----------------------------------------------------------------------------
int sml_obis_scale_uint (uint64_t value, int scale)
{
    if (scale > 0) {
        value = value * (10 * scale);
    } else if (scale < 0) {
        value = value / (10 * -scale);
    }
    return (int)value;
}
//-----------------------------------------------------------------------------
int64_t sml_obis_get_int (unsigned char *data, unsigned int len)
{
    int64_t value = 0;

    if (len > 8) {
        DPRINTLN(DBG_WARN, "Int too big");
    } else {
        unsigned int i;

        if ((len > 0) && (*data & 0x80)) {
            value = -1LL;
        }
        for (i=0; i<len; i++) {
            value = (value << 8) | *data++;
        }
    }
    return value;
}

//-----------------------------------------------------------------------------
int sml_obis_scale_int (int64_t value, int scale)
{
    if (scale > 0) {
        value = value * (10 * scale);
    } else if (scale < 0) {
        value = value / (10 * -scale);
    }
    return (int)value;
}

//-----------------------------------------------------------------------------
int16_t sml_get_obis_pac_average ()
{
    int32_t average;
    int16_t pac_average = 0;

    if (obis_cur_pac_cnt) {
        if (obis_pac_sum >= 0) {
            average = (obis_pac_sum + (obis_cur_pac_cnt >> 1)) / obis_cur_pac_cnt;
            if (average > INT16_MAX) {
                pac_average = INT16_MAX;
            } else {
                pac_average = average;
            }
        } else {
            average = (obis_pac_sum - (obis_cur_pac_cnt >> 1)) / obis_cur_pac_cnt;
            if (average < INT16_MIN) {
                pac_average = INT16_MIN;
            } else {
                pac_average = average;
            }
        }
    }
    return pac_average;
}

//-----------------------------------------------------------------------------
void sml_handle_obis_pac (int16_t pac)
{
    obis_cur_pac = pac;

    if (*obis_timestamp) {
        uint32_t pac_index = gTimezone.toLocal (*obis_timestamp);

        pac_index = hour(pac_index) * 60 + minute(pac_index);
        pac_index /= AHOY_PAC_INTERVAL;

        if (pac_index != obis_cur_pac_index) {
            /* calc average for last interval */
            if (obis_cur_pac_cnt) {
                int16_t pac_average = sml_get_obis_pac_average();
                File file;
                char file_name[sizeof (SML_OBIS_GRID_POWER_PATH) + sizeof (SML_OBIS_FORMAT_FILE_NAME)];
                time_t time_today = *obis_timestamp;
                struct tm tm_today;

                localtime_r (&time_today, &tm_today);
                snprintf (file_name, sizeof (file_name), SML_OBIS_GRID_POWER_PATH "/" SML_OBIS_FORMAT_FILE_NAME,
                    tm_today.tm_mday, tm_today.tm_mon+1, tm_today.tm_year + 1900);
                // append last average
                if ((file = LittleFS.open (file_name, "a"))) {
                    unsigned char buf[4];
                    buf[0] = obis_cur_pac_index & 0xff;
                    buf[1] = obis_cur_pac_index >> 8;
                    buf[2] = pac_average & 0xff;
                    buf[3] = pac_average >> 8;
                    if (file.write (buf, sizeof (buf)) != sizeof (buf)) {
                        DPRINTLN (DBG_WARN, "sml_handle_obis_pac, failed_to_write");
                    } else {
                        DPRINTLN (DBG_DEBUG, "sml_handle_obis_pac, write to " + String(file_name));
                    }
                    file.close ();
                } else {
                    DPRINTLN (DBG_WARN, "sml_handle_obis_pac, failed to open");
                }
                obis_cur_pac_cnt = 0;
                obis_pac_sum = 0;
            }
            obis_cur_pac_index = pac_index;
        }
        if ((pac_index >= AHOY_MIN_PAC_SUN_HOUR * 60 / AHOY_PAC_INTERVAL) &&
                (pac_index < AHOY_MAX_PAC_SUN_HOUR * 60 / AHOY_PAC_INTERVAL)) {
            obis_pac_sum += pac;
            obis_cur_pac_cnt++;
        } else {
            DPRINTLN (DBG_INFO, "sml_handle_obis_pac, outside daylight, minutes: " + String (pac_index * AHOY_PAC_INTERVAL));
        }
    } else {
        DPRINTLN (DBG_INFO, "sml_handle_obis_pac, no time2");
    }
}

//-----------------------------------------------------------------------------
uint16_t sml_fill_buf (uint16_t len)
{
    len = min (sizeof (sml_serial_buf) - (cur_serial_buf - sml_serial_buf) - sml_serial_len, len);

#ifdef SML_OBIS_TEST
    size_t partlen;

    partlen = min (len, sizeof (sml_test_telegram) - sml_test_telegram_offset);
    memcpy (cur_serial_buf + sml_serial_len, &sml_test_telegram[sml_test_telegram_offset], partlen);
    sml_serial_len += partlen;
    sml_test_telegram_offset += partlen;
    if (sml_test_telegram_offset >= sizeof (sml_test_telegram)) {
        sml_test_telegram_offset = 0;
    }
    if (partlen < len) {
        memcpy (cur_serial_buf + sml_serial_len, &sml_test_telegram[sml_test_telegram_offset], len - partlen);
        sml_serial_len += len - partlen;
        sml_test_telegram_offset += len - partlen;
    }
#else
    if (len) {
        len = Serial.readBytes(cur_serial_buf + sml_serial_len, len);
        sml_serial_len += len;
    }
#endif
    return len;
}

//-----------------------------------------------------------------------------
bool sml_get_list_entries (uint16_t layer)
{
    bool error = false;

    while (!error && sml_serial_len) {
        sml_list_entry_type_t type = (sml_list_entry_type_t)(*cur_serial_buf & 0x70);
        unsigned char entry_len;
        // Acc. to Spec there might be len_info > 2. But does this happen in real life?
        uint16 len_info = (*cur_serial_buf & SML_EXT_LENGTH) ? 2 : 1;

#ifdef undef
        DPRINT (DBG_INFO, "get_list_entries");
        DBGPRINT (", layer " + String (layer));
        DBGPRINT (", entries " + String (sml_list_layer_entries[layer]));
        DBGPRINT (", type 0x" + String (type, HEX));
        DBGPRINT (", len_info " + String (len_info));
        DBGPRINTLN (", sml_len " + String (sml_serial_len));
#endif

        if (sml_serial_len < len_info) {
            if (cur_serial_buf > sml_serial_buf) {
                memmove (sml_serial_buf, cur_serial_buf, sml_serial_len);
                cur_serial_buf = sml_serial_buf;
            }
            error = true;
        } else {
            if (len_info == 2) {
                entry_len = (*cur_serial_buf << 4) | (*(cur_serial_buf+1) & 0xf);
            } else {
                entry_len = *cur_serial_buf & 0x0f; /* bei Listen andere Bedeutung */
            }
            if ((type == SML_TYPE_LIST) || (sml_serial_len >= entry_len)) {
                sml_calc_obis_crc (len_info, cur_serial_buf);
                sml_serial_len -= len_info;
                if (entry_len && (type != SML_TYPE_LIST)) {
                    entry_len -= len_info;
                }
                if (sml_serial_len) {
                    cur_serial_buf += len_info;
                } else {
                    cur_serial_buf = sml_serial_buf;
                }
                if (sml_list_layer_entries[layer]) {
                    switch (type) {
                        case SML_TYPE_OCTET_STRING:
                            if ((layer == 4) && (entry_len == 6)) {
                                sml_handle_obis_state (cur_serial_buf);
                            }
                            break;
                        case SML_TYPE_BOOL:
                            break;
                        case SML_TYPE_INT:
                            if (layer == 1) {
                                if ((obis_state == OBIS_ST_NONE) &&
                                        (sml_list_layer_entries[layer] == 2) &&
                                        (sml_obis_get_int (cur_serial_buf, entry_len) == SML_MSG_GET_LIST_RSP)) {
                                    obis_state = OBIS_ST_UNKNOWN;
                                }
                            } else if (layer == 4) {
                                if (obis_state == OBIS_ST_POWER_ALL) {
                                    if (sml_list_layer_entries[layer] == 3) {
                                        obis_power_all_scale = (int)sml_obis_get_int (cur_serial_buf, entry_len);
                                    } else if (sml_list_layer_entries[layer] == 2) {
                                        obis_power_all_value = (int)sml_obis_get_int (cur_serial_buf, entry_len);
                                    }
                                } else if (obis_state == OBIS_ST_YIELD_IN_ALL) {
                                    if (sml_list_layer_entries[layer] == 3) {
                                        obis_yield_in_all_scale = (int)sml_obis_get_int (cur_serial_buf, entry_len);
                                    } else if (sml_list_layer_entries[layer] == 2) {
                                        obis_yield_in_all_value = sml_obis_get_int (cur_serial_buf, entry_len);
                                    }
                                } else if (obis_state == OBIS_ST_YIELD_OUT_ALL) {
                                    if (sml_list_layer_entries[layer] == 3) {
                                        obis_yield_out_all_scale = (int)sml_obis_get_int (cur_serial_buf, entry_len);
                                    } else if (sml_list_layer_entries[layer] == 2) {
                                        obis_yield_out_all_value = sml_obis_get_int (cur_serial_buf, entry_len);
                                    }
                                }
                            }
                            break;
                        case SML_TYPE_UINT:
                            if (layer == 1) {
                                if ((obis_state == OBIS_ST_NONE) &&
                                        (sml_list_layer_entries[layer] == 2) &&
                                        (sml_obis_get_uint (cur_serial_buf, entry_len) == SML_MSG_GET_LIST_RSP)) {
                                    obis_state = OBIS_ST_UNKNOWN;
                                }
                            } else if (layer == 4) {
                                if (obis_state == OBIS_ST_YIELD_IN_ALL) {
                                    if (sml_list_layer_entries[layer] == 2) {
                                        obis_yield_in_all_value = sml_obis_get_uint (cur_serial_buf, entry_len);
                                    }
                                } else if (obis_state == OBIS_ST_YIELD_OUT_ALL) {
                                    if (sml_list_layer_entries[layer] == 2) {
                                        obis_yield_out_all_value = sml_obis_get_uint (cur_serial_buf, entry_len);
                                    }
                                }
                            }
                            break;
                        case SML_TYPE_LIST:
                            if (layer + 1 < SML_MAX_LIST_LAYER) {
                                sml_list_layer_entries[layer]--;
                                layer++;
                                cur_sml_list_layer = layer;
                                sml_list_layer_entries[layer] = entry_len;
#ifdef undef
                                DPRINTLN(DBG_INFO, "Open layer " + String(layer) + ", entries " + String(entry_len));
#endif
                                if (!sml_serial_len) {
                                    error = true;
                                }
                            } else {
                                sml_state = SML_ST_FIND_START_TAG;
                                return sml_serial_len ? false : true;
                            }
                            break;
                        default:
                            DPRINT(DBG_WARN, "Ill Element 0x" + String(type, HEX));
                            DBGPRINTLN(", len " + String (entry_len + len_info));
                            /* design: aussteigen */
                            sml_state = SML_ST_FIND_START_TAG;
                            return sml_serial_len ? false : true;
                    }
                    if (type != SML_TYPE_LIST) {
                        sml_calc_obis_crc (entry_len, cur_serial_buf);
                        sml_serial_len -= entry_len;
                        if (sml_serial_len) {
                            cur_serial_buf += entry_len;
                        } else {
                            cur_serial_buf = sml_serial_buf;
                            error = true;
                        }
                        sml_list_layer_entries[layer]--;
                    }
                }
                while (!sml_list_layer_entries[layer]) {
#ifdef undef
                    DPRINTLN(DBG_INFO, "COMPLETE, layer " + String (layer));
#endif
                    if (layer) {
                        layer--;
                        cur_sml_list_layer = layer;
                    } else {
                        sml_state = SML_ST_FIND_MSG;
                        return sml_serial_len ? false : true;
                    }
                }
            } else if (entry_len > sizeof (sml_serial_buf)) {
                DPRINTLN (DBG_INFO, "skip " + String (entry_len));
                sml_skip_len = entry_len;
                sml_state = SML_ST_SKIP_LIST_ENTRY;
                return false;
            } else {
                if (cur_serial_buf > sml_serial_buf) {
                    memmove (sml_serial_buf, cur_serial_buf, sml_serial_len);
                    cur_serial_buf = sml_serial_buf;
                }
                error = true;
            }
        }
    }
    return error;
}

//-----------------------------------------------------------------------------
uint16_t sml_parse_stream (uint16 len)
{
    bool parse_continue;
    uint16_t serial_read;

    serial_read = sml_fill_buf (len);

    do {
        parse_continue = false;
        switch (sml_state) {
            case SML_ST_FIND_START_TAG:
                if (sml_serial_len >= sizeof (esc_seq)) {
                    unsigned char *last_serial_buf = cur_serial_buf;

                    if ((cur_serial_buf = (unsigned char *)memmem (cur_serial_buf, sml_serial_len, esc_seq, sizeof (esc_seq)))) {
                        sml_init_obis_crc ();
                        sml_calc_obis_crc (sizeof (esc_seq), cur_serial_buf);
                        sml_serial_len -= cur_serial_buf - last_serial_buf;
                        sml_serial_len -= sizeof (esc_seq);
                        if (sml_serial_len) {
                            cur_serial_buf += sizeof (esc_seq);
                            parse_continue = true;
                        } else {
                            cur_serial_buf = sml_serial_buf;
                        }
                        sml_state = SML_ST_FIND_VERSION;
#ifdef undef
                        DPRINTLN(DBG_INFO, "START_TAG, rest " + String(sml_serial_len));
#endif
                    } else {
                        cur_serial_buf = last_serial_buf + sml_serial_len;
                        last_serial_buf = cur_serial_buf;

                        /* handle up to last 3 esc chars */
                        while ((*(cur_serial_buf - 1) == SML_ESCAPE_CHAR)) {
                            cur_serial_buf--;
                        }
                        if ((sml_serial_len = last_serial_buf - cur_serial_buf)) {
                            memmove (sml_serial_buf, cur_serial_buf, sml_serial_len);
                        }
                        cur_serial_buf = sml_serial_buf;
                    }
                } else if (cur_serial_buf > sml_serial_buf) {
                    memmove (sml_serial_buf, cur_serial_buf, sml_serial_len);
                    cur_serial_buf = sml_serial_buf;
                }
                break;
            case SML_ST_FIND_VERSION:
                if (sml_serial_len >=sizeof (version_seq)) {
                    if (!memcmp (cur_serial_buf, version_seq, sizeof (version_seq))) {
                        sml_calc_obis_crc (sizeof (version_seq), cur_serial_buf);
                        sml_state = SML_ST_FIND_MSG;
#ifdef undef
                        DPRINTLN(DBG_INFO, "VERSION, rest " + String (sml_serial_len - sizeof (version_seq)));
#endif
                    } else {
#ifdef undef
                        DPRINTLN(DBG_INFO, "no VERSION, rest " + String (sml_serial_len - sizeof (version_seq)));
#endif
                        sml_state = SML_ST_FIND_START_TAG;
                    }
                    sml_serial_len -= sizeof (version_seq);
                    if (sml_serial_len) {
                        cur_serial_buf += sizeof (version_seq);
                        parse_continue = true;
                    } else {
                        cur_serial_buf = sml_serial_buf;
                    }
                } else if (cur_serial_buf > sml_serial_buf) {
                    memmove (sml_serial_buf, cur_serial_buf, sml_serial_len);
                    cur_serial_buf = sml_serial_buf;
                }
                break;
            case SML_ST_FIND_MSG:
                if (sml_serial_len) {
                    if (*cur_serial_buf == 0x1b) {
                        sml_state = SML_ST_FIND_END_TAG;
                        parse_continue = true;
                    } else if ((*cur_serial_buf & 0x70) == 0x70) {
                        /* todo: extended list on 1st level */
                        sml_calc_obis_crc (1, cur_serial_buf);
#ifdef undef
                        DPRINTLN (DBG_INFO, "TOPLIST 0x" + String(*cur_serial_buf, HEX) + ", rest " + String (sml_serial_len - 1));
#endif
                        sml_state = SML_ST_FIND_LIST_ENTRIES;
                        cur_sml_list_layer = 0;
                        obis_state = OBIS_ST_NONE;
                        sml_list_layer_entries[0] = *cur_serial_buf & 0xf;
                        sml_serial_len--;
                        if (sml_serial_len) {
                            cur_serial_buf++;
                            parse_continue = true;
                        } else {
                            cur_serial_buf = sml_serial_buf;
                        }
                    } else if (*cur_serial_buf == 0x00) {
                        /* fill byte (depends on the size of the telegram) */
                        sml_calc_obis_crc (1, cur_serial_buf);
                        sml_serial_len--;
                        if (sml_serial_len) {
                            cur_serial_buf++;
                            parse_continue = true;
                        } else {
                            cur_serial_buf = sml_serial_buf;
                        }
                    } else {
                        DPRINTLN(DBG_WARN, "Unexpected 0x" + String(*cur_serial_buf, HEX) + ", rest: " + String (sml_serial_len));
                        sml_state = SML_ST_FIND_START_TAG;
                        parse_continue = true;
                    }
                }
                break;
            case SML_ST_FIND_LIST_ENTRIES:
                parse_continue = !sml_get_list_entries (cur_sml_list_layer);
                break;
            case SML_ST_SKIP_LIST_ENTRY:
                if (sml_serial_len) {   /* design: keep rcv buf small and skip irrelevant long list entries */
                    size_t len = min (sml_serial_len, sml_skip_len);

                    sml_calc_obis_crc (len, cur_serial_buf);
                    sml_serial_len -= len;
                    if (sml_serial_len) {
                        cur_serial_buf += len;
                        parse_continue = true;
                    } else {
                        cur_serial_buf = sml_serial_buf;
                    }
                    sml_skip_len -= len;
                    if (!sml_skip_len) {
                        sml_state = SML_ST_FIND_LIST_ENTRIES;
                        sml_list_layer_entries[cur_sml_list_layer]--;
                        while (!sml_list_layer_entries[cur_sml_list_layer]) {
#ifdef undef
                            DPRINTLN(DBG_INFO, "COMPLETE, layer " + String (cur_sml_list_layer));
#endif
                            if (cur_sml_list_layer) {
                                cur_sml_list_layer--;
                            } else {
                                sml_state = SML_ST_FIND_MSG;
                                break;
                            }
                        }
                    }
                }
                break;
            case SML_ST_FIND_END_TAG:
                if (sml_serial_len >= sizeof (esc_seq)) {
                    if (!memcmp (cur_serial_buf, esc_seq, sizeof (esc_seq))) {
                        sml_calc_obis_crc (sizeof (esc_seq), cur_serial_buf);
                        sml_state = SML_ST_CHECK_CRC;
                    } else {
                        DPRINTLN(DBG_WARN, "Missing END_TAG, found 0x" + String (*cur_serial_buf) +
                            " 0x" + String (*(cur_serial_buf+1)) +
                            " 0x" + String (*(cur_serial_buf+2)) +
                            " 0x" + String (*(cur_serial_buf+3)));
                        sml_state = SML_ST_FIND_START_TAG;
                    }
                    sml_serial_len -= sizeof (esc_seq);
                    if (sml_serial_len) {
                        cur_serial_buf += sizeof (esc_seq);
                        parse_continue = true;
                    } else {
                        cur_serial_buf = sml_serial_buf;
                    }
                } else if (cur_serial_buf > sml_serial_buf) {
                    memmove (sml_serial_buf, cur_serial_buf, sml_serial_len);
                    cur_serial_buf = sml_serial_buf;
                }
                break;
            case SML_ST_CHECK_CRC:
                if (sml_serial_len >= 4) {
                    if (*cur_serial_buf == 0x1a) {
                        uint16_t calc_crc16, rcv_crc16;

                        sml_calc_obis_crc (2, cur_serial_buf);
                        calc_crc16 = sml_finit_obis_crc ();
                        rcv_crc16 = (*(cur_serial_buf+2) << 8) + *(cur_serial_buf+3);
                        if (calc_crc16 == rcv_crc16) {
                            obis_power_all_value = sml_obis_scale_int (obis_power_all_value, obis_power_all_scale);
#ifdef undef
                            obis_yield_in_all_value = sml_obis_scale_uint (obis_yield_in_all_value, obis_yield_in_all_scale);
                            obis_yield_out_all_value = sml_obis_scale_uint (obis_yield_out_all_value, obis_yield_out_all_scale);
                            DPRINTLN(DBG_INFO, "Power " + String (obis_power_all_value) +
                                ", Yield in " + String (obis_yield_in_all_value) +
                                ", Yield out " + String (obis_yield_out_all_value));
#else
                            DPRINTLN(DBG_INFO, "Power " + String (obis_power_all_value));
#endif
                            sml_handle_obis_pac (obis_power_all_value);
                        } else {
                            DPRINTLN(DBG_WARN, "CRC ERROR 0x" + String (calc_crc16, HEX) + " <-> 0x" + String (rcv_crc16, HEX));
                        }
                    }
                    sml_state = SML_ST_FIND_START_TAG;
                    sml_serial_len -= 4;
                    if (sml_serial_len) {
                        cur_serial_buf += 4;
                        parse_continue = true;
                    } else {
                        cur_serial_buf = sml_serial_buf;
                    }
                } else if (cur_serial_buf > sml_serial_buf) {
                    memmove (sml_serial_buf, cur_serial_buf, sml_serial_len);
                    cur_serial_buf = sml_serial_buf;
                }
                break;
        }
    } while (parse_continue);
    return serial_read;
}

//-----------------------------------------------------------------------------
void sml_loop ()
{
    uint16_t serial_avail;
    uint16_t serial_read = 0;

#ifdef SML_OBIS_TEST
    uint32_t cur_uptime;
    static uint32_t last_uptime;
    if (((cur_uptime = mApp->getUptime()) > 30) && (cur_uptime != last_uptime)) {
        last_uptime = cur_uptime;
        serial_avail = sizeof (sml_test_telegram) >> 2;
    } else {
        serial_avail = 0;
    }
#else
    serial_avail = Serial.available();
#endif
    if (serial_avail > 0) {
        do {
            serial_read = sml_parse_stream (serial_avail);
            serial_avail -= serial_read;
            //  yield();  /* unconditionally called this might have a bad effect for TX Retransmit to the Inverter via NRF24L01+ (not quite sure) */
        } while (serial_read && serial_avail);
    }
}

#endif
