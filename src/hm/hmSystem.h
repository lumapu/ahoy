//-----------------------------------------------------------------------------
// 2022 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __HM_SYSTEM_H__
#define __HM_SYSTEM_H__

#include "hmInverter.h"
#include "hmRadio.h"
#include "../config/settings.h"

#define AC_POWER_PATH AHOY_HIST_PATH "/ac_power"
#define AC_FORMAT_FILE_NAME "%02u_%02u_%04u.bin"

template <uint8_t MAX_INVERTER=3, class INVERTERTYPE=Inverter<float>>
class HmSystem {
    public:
        HmRadio<> Radio;

        HmSystem() {}

        void setup() {
            mNumInv = 0;
            Radio.setup();
        }

        void setup(uint32_t *timestamp, uint8_t ampPwr, uint8_t irqPin, uint8_t cePin, uint8_t csPin, uint8_t sclkPin, uint8_t mosiPin, uint8_t misoPin) {
            mTimestamp = timestamp;
            mNumInv = 0;
            Radio.setup(ampPwr, irqPin, cePin, csPin, sclkPin, mosiPin, misoPin);
        }

        void addInverters(cfgInst_t *config) {
            Inverter<> *iv;
            for (uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                iv = addInverter(&config->iv[i]);
                if (0ULL != config->iv[i].serial.u64) {
                    if (NULL != iv) {
                        DPRINT(DBG_INFO, "added inverter ");
                        if(iv->config->serial.b[5] == 0x11)
                            DBGPRINT("HM");
                        else {
                            DBGPRINT(((iv->config->serial.b[4] & 0x03) == 0x01) ? " (2nd Gen) " : " (3rd Gen) ");
                        }

                        DBGPRINTLN(String(iv->config->serial.u64, HEX));

                        if((iv->config->serial.b[5] == 0x10) && ((iv->config->serial.b[4] & 0x03) == 0x01))
                            DPRINTLN(DBG_WARN, F("MI Inverter are not fully supported now!!!"));
                    }
                }
            }
        }

        INVERTERTYPE *addInverter(cfgIv_t *config) {
            DPRINTLN(DBG_VERBOSE, F("hmSystem.h:addInverter"));
            if(MAX_INVERTER <= mNumInv) {
                DPRINT(DBG_WARN, F("max number of inverters reached!"));
                return NULL;
            }
            INVERTERTYPE *p = &mInverter[mNumInv];
            p->id         = mNumInv;
            p->config     = config;
            DPRINT(DBG_VERBOSE, "SERIAL: " + String(p->config->serial.b[5], HEX));
            DPRINTLN(DBG_VERBOSE, " " + String(p->config->serial.b[4], HEX));
            if((p->config->serial.b[5] == 0x11) || (p->config->serial.b[5] == 0x10)) {
                switch(p->config->serial.b[4]) {
                    case 0x22:
                    case 0x21: p->type = INV_TYPE_1CH; break;
                    case 0x42:
                    case 0x41: p->type = INV_TYPE_2CH; break;
                    case 0x62:
                    case 0x61: p->type = INV_TYPE_4CH; break;
                    default:
                        DPRINTLN(DBG_ERROR, F("unknown inverter type"));
                        break;
                }

                if(p->config->serial.b[5] == 0x11)
                    p->ivGen = IV_HM;
                else if((p->config->serial.b[4] & 0x03) == 0x02) // MI 3rd Gen -> same as HM
                    p->ivGen = IV_HM;
                else // MI 2nd Gen
                    p->ivGen = IV_MI;
            }
            else if(p->config->serial.u64 != 0ULL)
                DPRINTLN(DBG_ERROR, F("inverter type can't be detected!"));

            p->init();

            mNumInv ++;
            return p;
        }

        INVERTERTYPE *findInverter(uint8_t buf[]) {
            DPRINTLN(DBG_VERBOSE, F("hmSystem.h:findInverter"));
            INVERTERTYPE *p;
            for(uint8_t i = 0; i < mNumInv; i++) {
                p = &mInverter[i];
                if((p->config->serial.b[3] == buf[0])
                    && (p->config->serial.b[2] == buf[1])
                    && (p->config->serial.b[1] == buf[2])
                    && (p->config->serial.b[0] == buf[3]))
                    return p;
            }
            return NULL;
        }

        INVERTERTYPE *getInverterByPos(uint8_t pos, bool check = true) {
            DPRINTLN(DBG_VERBOSE, F("hmSystem.h:getInverterByPos"));
            if(pos >= MAX_INVERTER)
                return NULL;
            else if((mInverter[pos].initialized && mInverter[pos].config->serial.u64 != 0ULL) || false == check)
                return &mInverter[pos];
            else
                return NULL;
        }

        uint8_t getNumInverters(void) {
            /*uint8_t num = 0;
            INVERTERTYPE *p;
            for(uint8_t i = 0; i < MAX_NUM_INVERTERS; i++) {
                p = &mInverter[i];
                if(p->config->serial.u64 != 0ULL)
                    num++;
            }
            return num;*/
            return MAX_NUM_INVERTERS;
        }

        void enableDebug() {
            Radio.enableDebug();
        }

        //-----------------------------------------------------------------------------
        void cleanup_history ()
        {
            time_t time_today;
            INVERTERTYPE *p;
            uint16_t i;

            cur_pac_index = 0;
            for (i = 0, p = mInverter; i < MAX_NUM_INVERTERS; i++, p++) {
                p->pac_cnt = 0;
                p->pac_sum = 0;
            }

            if ((time_today = *mTimestamp)) {
#ifdef ESP32
                File ac_power_dir;
                File file;
#else
                Dir ac_power_dir;
#endif
                char cur_file_name[sizeof (AC_FORMAT_FILE_NAME)];

                time_today = gTimezone.toLocal (time_today);
                snprintf (cur_file_name, sizeof (cur_file_name), AC_FORMAT_FILE_NAME,
                    day(time_today), month(time_today), year(time_today));

                /* design: no dataserver, cleanup old history */
#ifdef ESP32
                if ((ac_power_dir = LittleFS.open (AC_POWER_PATH))) {
                    while ((file = ac_power_dir.openNextFile())) {
                        const char *fullName = file.name();
                        char *name;

                        if ((name = strrchr (fullName, '/')) && name[1]) {
                            name++;
                        } else {
                            name = (char *)fullName;
                        }
                        if (strcmp (name, cur_file_name)) {
                            char *path = strdup (fullName);

                            file.close ();
                            if (path) {
                                DPRINTLN (DBG_INFO, "Remove file " + String (name) +
                                    ", Size: " + String (ac_power_dir.size()));
                                LittleFS.remove (path);
                                free (path);
                            }
                        } else {
                            file.close();
                        }
                    }
                    ac_power_dir.close();
                }
#else
                ac_power_dir = LittleFS.openDir (AC_POWER_PATH);
                while (ac_power_dir.next()) {
                    if (ac_power_dir.fileName() != cur_file_name) {
                        DPRINTLN (DBG_INFO, "Remove file " + ac_power_dir.fileName() +
                            ", Size: " + String (ac_power_dir.fileSize()));
                        LittleFS.remove (AC_POWER_PATH "/" + ac_power_dir.fileName());
                    }
                }
#endif
            } else {
                DPRINTLN (DBG_WARN, "cleanup_history, no time yet");
            }
        }

        //-----------------------------------------------------------------------------
        File open_hist ()
        {
            time_t time_today;
            File file = (File) NULL;
            char file_name[sizeof (AC_POWER_PATH) + sizeof (AC_FORMAT_FILE_NAME)];

            if ((time_today = *mTimestamp)) {
                time_today = gTimezone.toLocal (time_today);
                snprintf (file_name, sizeof (file_name), AC_POWER_PATH "/" AC_FORMAT_FILE_NAME,
                    day(time_today), month(time_today), year(time_today));
                file = LittleFS.open (file_name, "r");
                if (!file) {
                    DPRINT (DBG_VERBOSE, "open_hist, failed to open ");  // typical: during night time after midnight
                    DBGPRINTLN (file_name);
                }
            } else {
                DPRINTLN (DBG_WARN, "open_history, no time yet");
            }
            return file;
        }

        //-----------------------------------------------------------------------------
        bool has_pac_value ()
        {
            if (*mTimestamp) {
                INVERTERTYPE *p;
                uint16_t i;

                for (i = 0, p = mInverter; i < MAX_NUM_INVERTERS; i++, p++) {
                    if (p->config->serial.u64 && p->isConnected && p->pac_cnt) {
                        return true;
                    }
                }
            }
            return false;
        }

        //-----------------------------------------------------------------------------
        uint16_t get_pac_average (bool cleanup)
        {
            uint32_t pac_average = 0;
            INVERTERTYPE *p;
            uint16_t i;

            for (i = 0, p = mInverter; i < MAX_NUM_INVERTERS; i++, p++) {
                if (p->config->serial.u64 && p->isConnected && p->pac_cnt) {
                    pac_average += (p->pac_sum + (p->pac_cnt >> 1)) / p->pac_cnt;
                }
                if (cleanup) {
                    p->pac_sum = 0;
                    p->pac_cnt = 0;
                }
            }
            if (pac_average > UINT16_MAX) {
                pac_average = UINT16_MAX;
            }
            return (uint16_t)pac_average;
        }

        //-----------------------------------------------------------------------------
        bool get_cur_value (uint16_t *interval, uint16_t *pac)
        {
            if (has_pac_value()) {
                *interval = cur_pac_index;
                *pac = get_pac_average (false);
                return true;
            }
            DPRINTLN (DBG_VERBOSE, "get_cur_value: none");
            return false;
        }

        //-----------------------------------------------------------------------------
        void close_hist (File file)
        {
            if (file) {
                file.close ();
            }
        }

        //-----------------------------------------------------------------------------
        void handle_pac (INVERTERTYPE *p, uint16_t pac)
        {
           time_t time_today;

           if ((time_today = *mTimestamp)) {
                uint32_t pac_index;

                time_today = gTimezone.toLocal (time_today);
                pac_index = hour(time_today) * 60 + minute(time_today);
                pac_index /= AHOY_PAC_INTERVAL;
                if (pac_index != cur_pac_index) {
                    if (has_pac_value ()) {
                        /* calc sum of all inverter averages for last interval */
                        /* and cleanup all counts and sums */
                        uint16_t pac_average = get_pac_average(true);
                        File file;
                        char file_name[sizeof (AC_POWER_PATH) + sizeof (AC_FORMAT_FILE_NAME)];

                        snprintf (file_name, sizeof (file_name), AC_POWER_PATH "/" AC_FORMAT_FILE_NAME,
                            day (time_today), month (time_today), year (time_today));
                        // append last average
                        if ((file = LittleFS.open (file_name, "a"))) {
                            unsigned char buf[4];
                            buf[0] = cur_pac_index & 0xff;
                            buf[1] = cur_pac_index >> 8;
                            buf[2] = pac_average & 0xff;
                            buf[3] = pac_average >> 8;
                            if (file.write (buf, sizeof (buf)) != sizeof (buf)) {
                                DPRINTLN (DBG_WARN, "handle_pac, failed_to_write");
                            } else {
                                DPRINTLN (DBG_DEBUG, "handle_pac, write to " + String(file_name));
                            }
                            file.close ();
                        } else {
                            DPRINTLN (DBG_WARN, "handle_pac, failed to open");
                        }
                    }
                    cur_pac_index = pac_index;
                }
                if ((pac_index >= AHOY_MIN_PAC_SUN_HOUR * 60 / AHOY_PAC_INTERVAL) &&
                        (pac_index < AHOY_MAX_PAC_SUN_HOUR * 60 / AHOY_PAC_INTERVAL)) {
                    p->pac_sum += pac;
                    p->pac_cnt++;
                } else {
                    DPRINTLN (DBG_DEBUG, "handle_pac, outside daylight, minutes: " + String (pac_index * AHOY_PAC_INTERVAL));
                }
            } else {
                DPRINTLN (DBG_INFO, "handle_pac, no time2");
            }
        }

    private:
        INVERTERTYPE mInverter[MAX_INVERTER];
        uint8_t mNumInv;
        uint32_t *mTimestamp;
        uint16_t cur_pac_index;

};

#endif /*__HM_SYSTEM_H__*/
