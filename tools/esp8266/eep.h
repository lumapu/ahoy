//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __EEP_H__
#define __EEP_H__

#include "Arduino.h"
#include <Preferences.h>
#ifdef ESP32
    #include <nvs_flash.h>
#endif
using namespace std;

class eep {
    public:

    //initializing local variables to pass in for method parameters from Preferences.h
    string nameFiller = "";
    bool onlyRead;
    const char* label;
    const char* keyVal;
    uint8_t num;
        eep() {

            #ifdef ESP32
                Preferences.begin(nameFiller, onlyRead, label);
            #endif

        }

        //destructor
        ~eep() {
            Preferences.end();
        }

        //following read methods initiated to read in incrementing the size and value
        void read(uint32_t addr, char *str, uint8_t length) {
            for(uint8_t i = 0; i < length; i ++) {
                *(str++) = Preferences.putUChar(keyVal, num);
            }
        }

        void read(uint32_t addr, float *value) {
            uint8_t *p = (uint8_t*)value;
            for(uint8_t i = 0; i < 4; i ++) {
                *(p++) = Preferences.getBytesLength(addr++);
            }
        }

        void read(uint32_t addr, bool *value) {
            uint8_t intVal = 0x00;
            intVal = (int)Preferences.getBytesLength(addr++);
            *value = (intVal == 0x01);
        }

        void read(uint32_t addr, uint8_t *value) {
            *value = Preferences.getBytesLength(addr++);
        }

        void read(uint32_t addr, uint8_t data[], uint16_t length) {
            for(uint16_t i = 0; i < length; i ++) {
                *(data++) = Preferences.getBytesLength(addr++);
            }
        }

        void read(uint32_t addr, uint16_t *value) {
            *value  = (Preferences.getBytesLength(addr++) << 8);
            *value |= (Preferences.getBytesLength(addr++));
        }

        void read(uint32_t addr, uint16_t data[], uint16_t length) {
            for(uint16_t i = 0; i < length; i ++) {
                *(data)    = (Preferences.getBytesLength(addr++) << 8);
                *(data++) |= (Preferences.getBytesLength(addr++));
            }
        }

        void read(uint32_t addr, uint32_t *value) {
            *value  = (Preferences.getBytesLength(addr++) << 24);
            *value |= (Preferences.getBytesLength(addr++) << 16);
            *value |= (Preferences.getBytesLength(addr++) <<  8);
            *value |= (Preferences.getBytesLength(addr++));
        }

        void read(uint32_t addr, uint64_t *value) {
            read(addr, (uint32_t *)value);
            *value <<= 32;
            uint32_t tmp;
            read(addr+4, &tmp);
            *value |= tmp;
        }

        //following write methods to write in incrementing the size and value
        void write(uint32_t addr, const char *str, uint8_t length) {
            for(uint8_t i = 0; i < length; i ++) {
                Preferences.putString(addr++, str[i]);
            }
        }

        void write(uint32_t addr, uint8_t data[], uint16_t length) {
            for(uint16_t i = 0; i < length; i ++) {
                Preferences.putInt(addr++, data[i]);
            }
        }

        void write(uint32_t addr, float value) {
            uint8_t *p = (uint8_t*)&value;
            for(uint8_t i = 0; i < 4; i ++) {
                Preferences.putFloat(addr++, p[i]);
            }
        }

        void write(uint32_t addr, bool value) {
            uint8_t intVal = (value) ? 0x01 : 0x00;
            Preferences.putBool(addr++, intVal);
        }

        void write(uint32_t addr, uint8_t value) {
            Preferences.putUInt(addr++, value);
        }

        void write(uint32_t addr, uint16_t value) {
            Preferences.putUInt(addr++, (value >> 8) & 0xff);
            Preferences.putUInt(addr++, (value     ) & 0xff);
        }


        void write(uint32_t addr, uint16_t data[], uint16_t length) {
            for(uint16_t i = 0; i < length; i ++) {
                Preferences.putUInt(addr++, (data[i] >> 8) & 0xff);
                Preferences.putUInt(addr++, (data[i]     ) & 0xff);
            }
        }

        void write(uint32_t addr, uint32_t value) {
            Preferences.putUInt(addr++, (value >> 24) & 0xff);
            Preferences.putUInt(addr++, (value >> 16) & 0xff);
            Preferences.putUInt(addr++, (value >>  8) & 0xff);
            Preferences.putUInt(addr++, (value      ) & 0xff);
        }

        void write(uint32_t addr, uint64_t value) {
            Preferences.putUInt(addr++, (value >> 56) & 0xff);
            Preferences.putUInt(addr++, (value >> 48) & 0xff);
            Preferences.putUInt(addr++, (value >> 40) & 0xff);
            Preferences.putUInt(addr++, (value >> 32) & 0xff);
            Preferences.putUInt(addr++, (value >> 24) & 0xff);
            Preferences.putUInt(addr++, (value >> 16) & 0xff);
            Preferences.putUInt(addr++, (value >>  8) & 0xff);
            Preferences.putUInt(addr++, (value      ) & 0xff);
        }

        //commit will be called to revise changes made to Preferences lib
        void commit(void) {
            Preferences.commit();
        }
};

#endif /*__EEP_H__*/
