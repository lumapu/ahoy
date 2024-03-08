//-----------------------------------------------------------------------------
// 2024 Ahoy, https://ahoydtu.de
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __LANG_H__
#define __LANG_H__

#ifdef LANG_DE
    #define REBOOT_ESP_APPLY_CHANGES "starte AhoyDTU neu, um die Änderungen zu speichern"
#elif LANG_FR
    #define REBOOT_ESP_APPLY_CHANGES "redémarrer AhoyDTU pour appliquer tous vos changements de configuration"
#else
    #define REBOOT_ESP_APPLY_CHANGES "reboot AhoyDTU to apply all your configuration changes"
#endif

#ifdef LANG_DE
    #define TIME_NOT_SET "keine gültige Zeit, daher keine Kommunikation zum Wechselrichter möglich"
#elif LANG_FR
    #define TIME_NOT_SET "heure non définie. Aucune communication avec l'onduleur possible"
#else
    #define TIME_NOT_SET "time not set. No communication to inverter possible"
#endif

#ifdef LANG_DE
    #define WAS_IN_CH_12_TO_14 "Der ESP war in WLAN Kanal 12 bis 14, was uU. zu Abstürzen führt"
#elif LANG_FR
    #define WAS_IN_CH_12_TO_14 "Votre ESP était sur les canaux wifi 12 à 14. Cela peut entraîner des redémarrages de votre AhoyDTU"
#else
    #define WAS_IN_CH_12_TO_14 "Your ESP was in wifi channel 12 to 14. It may cause reboots of your AhoyDTU"
#endif

#ifdef LANG_DE
    #define PATH_NOT_FOUND "Pfad nicht gefunden: "
#elif LANG_FR
    #define PATH_NOT_FOUND "Chemin introuvable : "
#else
    #define PATH_NOT_FOUND "Path not found: "
#endif

#ifdef LANG_DE
    #define INCOMPLETE_INPUT "Unvollständige Eingabe"
#elif LANG_FR
    #define INCOMPLETE_INPUT "Saisie incomplète"
#else
    #define INCOMPLETE_INPUT "Incomplete input"
#endif

#ifdef LANG_DE
    #define INVALID_INPUT "Ungültige Eingabe"
#elif LANG_FR
    #define INVALID_INPUT "Saisie invalide"
#else
    #define INVALID_INPUT "Invalid input"
#endif

#ifdef LANG_DE
    #define NOT_ENOUGH_MEM "nicht genügend Speicher"
#elif LANG_FR
    #define NOT_ENOUGH_MEM "Mémoire insuffisante"
#else
    #define NOT_ENOUGH_MEM "Not enough memory"
#endif

#ifdef LANG_DE
    #define DESER_FAILED "Deserialisierung fehlgeschlagen"
#elif LANG_FR
    #define DESER_FAILED "Échec de la désérialisation"
#else
    #define DESER_FAILED "Deserialization failed"
#endif

#ifdef LANG_DE
    #define INV_NOT_FOUND "Wechselrichter nicht gefunden!"
#elif LANG_FR
    #define INV_NOT_FOUND "onduleur non trouvé !"
#else
    #define INV_NOT_FOUND "inverter not found!"
#endif

#ifdef LANG_DE
    #define FACTORY_RESET "Ahoy auf Werkseinstellungen zurücksetzen"
#elif LANG_FR
    #define FACTORY_RESET "Réinitialisation d'usine d'Ahoy"
#else
    #define FACTORY_RESET "Ahoy Factory Reset"
#endif

#ifdef LANG_DE
    #define BTN_REBOOT "Ahoy neustarten"
#elif LANG_FR
    #define BTN_REBOOT "Redémarrer"
#else
    #define BTN_REBOOT "Reboot"
#endif

#endif /*__LANG_H__*/

