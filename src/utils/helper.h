//-----------------------------------------------------------------------------
// 2022 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __HELPER_H__
#define __HELPER_H__

#include <cstdint>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>

namespace ah {
    void ip2Arr(uint8_t ip[], const char *ipStr);
    void ip2Char(uint8_t ip[], char *str);
    double round3(double value);
}

#endif /*__HELPER_H__*/
