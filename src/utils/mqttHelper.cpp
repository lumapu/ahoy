#include "mqttHelper.h"

namespace mqttHelper {

    /** checkBoolProperty
     * TODO: modify header
     * @param tmpTopic
     * @param subTopic
     * @param payload
     * @param len
     * @param cfg
     * @param log
     * @returns bool
     */

    bool checkCharProperty(const char *tmpTopic, const char *subTopic, const uint8_t *payload, size_t len, char *cfg, int cfgSize, DynamicJsonHandler *log) {
        // Check if tmpTopic starts with subTopic
        if (strncmp(tmpTopic, subTopic, strlen(subTopic)) == 0) {
            // Convert payload to a String instance
            String sPayload = String((const char*)payload).substring(0, len);

            // Copy the String into cfg and ensure cfg is null-terminated
            strncpy(cfg, sPayload.c_str(), cfgSize - 1);
            cfg[cfgSize - 1] = '\0';  // Ensure cfg is null-terminated

            // Add the property to the log
            log->addProperty("v", cfg);

            return true;  // Successfully processed the property
        }

        return false;  // Did not process the property
    }

    bool checkBoolProperty(const char *tmpTopic, const char *subTopic, const uint8_t *payload, size_t len, bool *cfg, DynamicJsonHandler *log) {
        if (strncmp(tmpTopic, subTopic, strlen(subTopic)) == 0) {
            String sPayload = String((const char*)payload).substring(0, len);

            if (sPayload == "1" || sPayload == "true") {
                *cfg = true;
                log->addProperty("v", *cfg);
            } else if (sPayload == "0" || sPayload == "false") {
                *cfg = false;
                log->addProperty("v", *cfg);
            } else {
                DBGPRINTLN(F("Payload is not a valid boolean value"));
                log->addProperty("v", "payload error");
            }
            return true;
        }
        return false;
    }

}
// --- END Namespace ---

    /** checkProperty
     *
     */
/*
    // final function what "all" combines other function into one
    template <typename T>
    bool checkProperty(const char *tmpTopic, const char *subTopic, const uint8_t *payload, size_t len, T *cfg, DynamicJsonHandler *log) {
        if (strncmp(tmpTopic, subTopic, strlen(subTopic)) == 0) {
            String sPayload = String((const char*)payload).substring(0, len);

            // Überprüfung und Zuweisung je nach Typ T
            if constexpr (std::is_integral_v<T>) {
                T value;
                sscanf(sPayload.c_str(), "%lld", &value);  // Beachte "%lld" für int64_t und uint64_t
                if (value <= std::numeric_limits<T>::max() && value >= std::numeric_limits<T>::min()) {
                    *cfg = value;
                } else {
                    log->addProperty("v", F("Fehler: Der Wert passt nicht in den Ziel-Typ T"));
                    return false;
                }
            } else if constexpr (std::is_same_v<T, bool>) {
                *cfg = (sPayload == "1" || sPayload == "true");
                if (*cfg) {
                    log->addProperty("v", "true");
                } else {
                    log->addProperty("v", "false");
                }
            } else if constexpr (std::is_same_v<T, char*> || std::is_same_v<T, const char*>) {
                strncpy(*cfg, sPayload.c_str(), cfgSize - 1);
                (*cfg)[cfgSize - 1] = '\0';
            } else {
                // Handle andere Datentypen
                return false;
            }

            // Füge die Eigenschaft zum Log hinzu
            log->addProperty("v", sPayload.c_str());

            return true;
        }

        return false;
    }
*/

