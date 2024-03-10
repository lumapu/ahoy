//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __PLUGIN_LANG_H__
#define __PLUGIN_LANG_H__

#ifdef LANG_DE
    #define STR_MONTHNAME_3_CHAR_LIST "ErrJanFebMrzAprMaiJunJulAugSepOktNovDez"
    #define STR_DAYNAME_3_CHAR_LIST   "ErrSonMonDieMitDonFreSam"
    #define STR_OFFLINE               "aus"
    #define STR_ONLINE                "aktiv"
    #define STR_NO_INVERTER           "kein inverter"
    #define STR_NO_WIFI               "WLAN nicht verbunden"
    #define STR_VERSION               "Version"
    #define STR_ACTIVE_INVERTERS      "aktive WR"
    #define STR_TODAY                 "heute"
    #define STR_TODAY                 "Gesamt"
#elif LANG_FR
    #define STR_MONTHNAME_3_CHAR_LIST "ErrJanFevMarAvrMaiJunJulAouSepOctNovDec"
    #define STR_DAYNAME_3_CHAR_LIST   "ErrDimLunMarMerJeuVenSam"
    #define STR_OFFLINE               "eteint"
    #define STR_ONLINE                "online"
    #define STR_NO_INVERTER           "pas d'onduleur"
    #define STR_NO_WIFI               "WiFi not connected"
    #define STR_VERSION               "Version"
    #define STR_ACTIVE_INVERTERS      "active Inv"
    #define STR_TODAY                 "today"
    #define STR_TODAY                 "total"
#else
    #define STR_MONTHNAME_3_CHAR_LIST "ErrJanFebMarAprMayJunJulAugSepOctNovDec"
    #define STR_DAYNAME_3_CHAR_LIST   "ErrSunMonTueWedThuFriSat"
    #define STR_OFFLINE               "offline"
    #define STR_ONLINE                "online"
    #define STR_NO_INVERTER           "no inverter"
    #define STR_NO_WIFI               "WiFi not connected"
    #define STR_VERSION               "Version"
    #define STR_ACTIVE_INVERTERS      "active Inv"
    #define STR_TODAY                 "today"
    #define STR_TODAY                 "total"
#endif

#endif /*__PLUGIN_LANG_H__*/
