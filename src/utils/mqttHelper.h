#ifndef __MQTT_HELPER_H__
#define __MQTT_HELPER_H__

#include <Arduino.h>
#include <cstdint>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include "DynamicJsonHandler.h"
#include "helper.h"
#include <limits>

namespace mqttHelper {
    template <typename T>
    bool checkIntegerProperty(const char *tmpTopic, const char *subTopic, const uint8_t *payload, size_t len, T *cfg, DynamicJsonHandler *log) {
        if (strncmp(tmpTopic, subTopic, strlen(subTopic)) == 0) {
            // Konvertiere payload in einen String
            String sPayload = String((const char*)payload).substring(0, len);

            // Konvertiere den String in den gewünschten Integer-Typ T
            T value;
            sscanf(sPayload.c_str(), "%d", &value);  // Beispielhaft für int, anpassen je nach T

            // Überprüfung, ob der Wert in den Ziel-Typ T passt
            if (sPayload.toInt() <= std::numeric_limits<T>::max() && sPayload.toInt() >= std::numeric_limits<T>::min()) {
                // Weise den Wert cfg zu
                *cfg = value;

                // Füge die Eigenschaft zum Log hinzu
                log->addProperty("v", ah::round1(*cfg));

                return true;
            } else {
                // Handle den Fall, wenn der Wert außerhalb des gültigen Bereichs liegt
                log->addProperty("v", F("Fehler: Der Wert passt nicht in den Ziel-Typ T"));
                return false;
            }
        }
        return false;
    }

    bool checkCharProperty(const char *tmpTopic, const char *subTopic, const uint8_t *payload, size_t len, char *cfg, int cfgSize, DynamicJsonHandler *log);
    bool checkBoolProperty(const char *tmpTopic, const char *subTopic, const uint8_t *payload, size_t len, bool *cfg, DynamicJsonHandler *log);
}

#endif /*__MQTT_HELPER_H__*/