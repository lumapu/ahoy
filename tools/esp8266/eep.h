//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __EEP_H__
#define __EEP_H__

#include "Arduino.h"
#include <EEPROM.h>

class eep {
    public:
        eep() {
            EEPROM.begin(4096);
        }
        ~eep() {
            EEPROM.end();
        }

        void read(uint32_t addr, char *str, uint8_t length) {
            for(uint8_t i = 0; i < length; i ++) {
                *(str++) = (char)EEPROM.read(addr++);
            }
        }

        void read(uint32_t addr, float *value) {
            uint8_t *p = (uint8_t*)value;
            for(uint8_t i = 0; i < 4; i ++) {
                *(p++) = (uint8_t)EEPROM.read(addr++);
            }
        }

        void read(uint32_t addr, bool *value) {
            uint8_t intVal = 0x00;
            intVal = EEPROM.read(addr++);
            *value = (intVal == 0x01);
        }

        void read(uint32_t addr, uint8_t *value) {
            *value = (EEPROM.read(addr++));
        }

        void read(uint32_t addr, uint8_t data[], uint16_t length) {
            for(uint16_t i = 0; i < length; i ++) {
                *(data++) = EEPROM.read(addr++);
            }
        }

        void read(uint32_t addr, uint16_t *value) {
            *value  = (EEPROM.read(addr++) << 8);
            *value |= (EEPROM.read(addr++));
        }

        void read(uint32_t addr, uint16_t data[], uint16_t length) {
            for(uint16_t i = 0; i < length; i ++) {
                *(data)    = (EEPROM.read(addr++) << 8);
                *(data++) |= (EEPROM.read(addr++));
            }
        }

        void read(uint32_t addr, uint32_t *value) {
            *value  = (EEPROM.read(addr++) << 24);
            *value |= (EEPROM.read(addr++) << 16);
            *value |= (EEPROM.read(addr++) <<  8);
            *value |= (EEPROM.read(addr++));
        }

        void read(uint32_t addr, uint64_t *value) {
            read(addr, (uint32_t *)value);
            *value <<= 32;
            uint32_t tmp;
            read(addr+4, &tmp);
            *value |= tmp;
            /**value  = (EEPROM.read(addr++) << 56);
            *value |= (EEPROM.read(addr++) << 48);
            *value |= (EEPROM.read(addr++) << 40);
            *value |= (EEPROM.read(addr++) << 32);
            *value |= (EEPROM.read(addr++) << 24);
            *value |= (EEPROM.read(addr++) << 16);
            *value |= (EEPROM.read(addr++) <<  8);
            *value |= (EEPROM.read(addr++));*/
        }

        void write(uint32_t addr, const char *str, uint8_t length) {
            for(uint8_t i = 0; i < length; i ++) {
                EEPROM.write(addr++, str[i]);
            }
        }

        void write(uint32_t addr, uint8_t data[], uint16_t length) {
            for(uint16_t i = 0; i < length; i ++) {
                EEPROM.write(addr++, data[i]);
            }
        }

        void write(uint32_t addr, float value) {
            uint8_t *p = (uint8_t*)&value;
            for(uint8_t i = 0; i < 4; i ++) {
                EEPROM.write(addr++, p[i]);
            }
        }

        void write(uint32_t addr, bool value) {
            uint8_t intVal = (value) ? 0x01 : 0x00;
            EEPROM.write(addr++, intVal);
        }

        void write(uint32_t addr, uint8_t value) {
            EEPROM.write(addr++, value);
        }

        void write(uint32_t addr, uint16_t value) {
            EEPROM.write(addr++, (value >> 8) & 0xff);
            EEPROM.write(addr++, (value     ) & 0xff);
        }


        void write(uint32_t addr, uint16_t data[], uint16_t length) {
            for(uint16_t i = 0; i < length; i ++) {
                EEPROM.write(addr++, (data[i] >> 8) & 0xff);
                EEPROM.write(addr++, (data[i]     ) & 0xff);
            }
        }

        void write(uint32_t addr, uint32_t value) {
            EEPROM.write(addr++, (value >> 24) & 0xff);
            EEPROM.write(addr++, (value >> 16) & 0xff);
            EEPROM.write(addr++, (value >>  8) & 0xff);
            EEPROM.write(addr++, (value      ) & 0xff);
        }

        void write(uint32_t addr, uint64_t value) {
            EEPROM.write(addr++, (value >> 56) & 0xff);
            EEPROM.write(addr++, (value >> 48) & 0xff);
            EEPROM.write(addr++, (value >> 40) & 0xff);
            EEPROM.write(addr++, (value >> 32) & 0xff);
            EEPROM.write(addr++, (value >> 24) & 0xff);
            EEPROM.write(addr++, (value >> 16) & 0xff);
            EEPROM.write(addr++, (value >>  8) & 0xff);
            EEPROM.write(addr++, (value      ) & 0xff);
        }

        void commit(void) {
            EEPROM.commit();
        }
};

#endif /*__EEP_H__*/
