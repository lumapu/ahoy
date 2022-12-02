//-----------------------------------------------------------------------------
// 2022 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#include "helper.h"

namespace ah {
    void ip2Arr(uint8_t ip[], const char *ipStr) {
        memset(ip, 0, 4);
        char *tmp = new char[strlen(ipStr)+1];
        strncpy(tmp, ipStr, strlen(ipStr)+1);
        char *p = strtok(tmp, ".");
        uint8_t i = 0;
        while(NULL != p) {
            ip[i++] = atoi(p);
            p = strtok(NULL, ".");
        }
        delete[] tmp;
    }

    // note: char *str needs to be at least 16 bytes long
    void ip2Char(uint8_t ip[], char *str) {
        if(0 == ip[0])
            str[0] = '\0';
        else
            snprintf(str, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    }

    double round3(double value) {
        return (int)(value * 1000 + 0.5) / 1000.0;
    }
}
