#ifndef __EEP_H__
#define __EEP_H__

#include "Arduino.h"

class eep {
    public:
        eep();
        ~eep();

        void read(uint32_t addr, char *str, uint8_t length);
        void read(uint32_t addr, float *value);
        void read(uint32_t addr, bool *value);
        void read(uint32_t addr, uint8_t *value);
        void read(uint32_t addr, uint8_t data[], uint8_t length);
        void read(uint32_t addr, uint16_t *value);
        void read(uint32_t addr, uint32_t *value);
        void write(uint32_t addr, const char *str, uint8_t length);
        void write(uint32_t addr, uint8_t data[], uint8_t length);
        void write(uint32_t addr, float value);
        void write(uint32_t addr, bool value);
        void write(uint32_t addr, uint8_t value);
        void write(uint32_t addr, uint16_t value);
        void write(uint32_t addr, uint32_t value);

    private:

};

#endif /*__EEP_H__*/
