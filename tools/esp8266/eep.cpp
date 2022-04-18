#include "eep.h"
#include <EEPROM.h>


//-----------------------------------------------------------------------------
eep::eep() {
    EEPROM.begin(500);
}


//-----------------------------------------------------------------------------
eep::~eep() {
    EEPROM.end();
}


//-----------------------------------------------------------------------------
void eep::read(uint32_t addr, char *str, uint8_t length) {
    for(uint8_t i = 0; i < length; i ++) {
        *(str++) = (char)EEPROM.read(addr++);
    }
}


//-----------------------------------------------------------------------------
void eep::read(uint32_t addr, float *value) {
    uint8_t *p = (uint8_t*)value;
    for(uint8_t i = 0; i < 4; i ++) {
        *(p++) = (uint8_t)EEPROM.read(addr++);
    }
}


//-----------------------------------------------------------------------------
void eep::read(uint32_t addr, bool *value) {
    uint8_t intVal = 0x00;
    intVal = EEPROM.read(addr++);
    *value = (intVal == 0x01);
}


//-----------------------------------------------------------------------------
void eep::read(uint32_t addr, uint8_t *value) {
    *value = (EEPROM.read(addr++));
}


//-----------------------------------------------------------------------------
void eep::read(uint32_t addr, uint8_t data[], uint8_t length) {
    for(uint8_t i = 0; i < length; i ++) {
        *(data++) = EEPROM.read(addr++);
    }
}


//-----------------------------------------------------------------------------
void eep::read(uint32_t addr, uint16_t *value) {
    *value  = (EEPROM.read(addr++) << 8);
    *value |= (EEPROM.read(addr++));
}


//-----------------------------------------------------------------------------
void eep::read(uint32_t addr, uint32_t *value) {
    *value  = (EEPROM.read(addr++) << 24);
    *value |= (EEPROM.read(addr++) << 16);
    *value |= (EEPROM.read(addr++) <<  8);
    *value |= (EEPROM.read(addr++));
}


//-----------------------------------------------------------------------------
void eep::write(uint32_t addr, const char *str, uint8_t length) {
    for(uint8_t i = 0; i < length; i ++) {
        EEPROM.write(addr++, str[i]);
    }
    EEPROM.commit();
}


//-----------------------------------------------------------------------------
void eep::write(uint32_t addr, uint8_t data[], uint8_t length) {
    for(uint8_t i = 0; i < length; i ++) {
        EEPROM.write(addr++, data[i]);
    }
    EEPROM.commit();
}


//-----------------------------------------------------------------------------
void eep::write(uint32_t addr, float value) {
    uint8_t *p = (uint8_t*)&value;
    for(uint8_t i = 0; i < 4; i ++) {
        EEPROM.write(addr++, p[i]);
    }
    EEPROM.commit();
}


//-----------------------------------------------------------------------------
void eep::write(uint32_t addr, bool value) {
    uint8_t intVal = (value) ? 0x01 : 0x00;
    EEPROM.write(addr++, intVal);
    EEPROM.commit();
}


//-----------------------------------------------------------------------------
void eep::write(uint32_t addr, uint8_t value) {
    EEPROM.write(addr++, value);
    EEPROM.commit();
}


//-----------------------------------------------------------------------------
void eep::write(uint32_t addr, uint16_t value) {
    EEPROM.write(addr++, (value >> 8) & 0xff);
    EEPROM.write(addr++, (value     ) & 0xff);
    EEPROM.commit();
}


//-----------------------------------------------------------------------------
void eep::write(uint32_t addr, uint32_t value) {
    EEPROM.write(addr++, (value >> 24) & 0xff);
    EEPROM.write(addr++, (value >> 16) & 0xff);
    EEPROM.write(addr++, (value >>  8) & 0xff);
    EEPROM.write(addr++, (value      ) & 0xff);
    EEPROM.commit();
}
