//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __PLUGIN_LANG_H__
#define __PLUGIN_LANG_H__

#ifdef LANG_DE
    #define STR_MONTHNAME_3_CHAR_LIST "ErrJanFebMrzAprMaiJunJulAugSepOktNovDez"
    #define STR_DAYNAME_3_CHAR_LIST   "ErrSonMonDieMitDonFreSam"
    #define STR_OFFLINE   "aus"
    #define STR_NO_INVERTER "kein inverter"
#elif LANG_FR
    #define STR_MONTHNAME_3_CHAR_LIST "ErrJanFevMarAvrMaiJunJulAouSepOctNovDec"
    #define STR_DAYNAME_3_CHAR_LIST   "ErrDimLunMarMerJeuVenSam"
    #define STR_OFFLINE   "eteint"
    #define STR_NO_INVERTER "pas d'onduleur"
#else
    #define STR_MONTHNAME_3_CHAR_LIST "ErrJanFebMarAprMayJunJulAugSepOctNovDec"
    #define STR_DAYNAME_3_CHAR_LIST   "ErrSunMonTueWedThuFriSat"
    #define STR_OFFLINE   "offline"
    #define STR_NO_INVERTER "no inverter"
#endif

#endif /*__PLUGIN_LANG_H__*/