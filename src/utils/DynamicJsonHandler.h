//-----------------------------------------------------------------------------
// 2024 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//
// The DynamicJsonHandler class is a helper class designed to facilitate the handling of JSON documents on embedded systems such as the ESP32. 
// It uses the ArduinoJson library to dynamically manage JSON data and provides functionality for adding properties, 
// serializing the document, and managing storage.
//
// Written from tictrick & DanielR92
//
//-----------------------------------------------------------------------------

#ifndef __DYNAMICJSONHANDLER_H__
#define __DYNAMICJSONHANDLER_H__

#include <Arduino.h>
#include <ArduinoJson.h>
#include <string>
#include "config/settings.h"    // needed for MAX_ALLOWED_BUF_SIZE

class DynamicJsonHandler {
public:
    DynamicJsonHandler();
    ~DynamicJsonHandler();

    template<typename T>
    void addProperty(const std::string& key, const T& value);

    String toString();
    void clear();
    size_t size() const;

private:
    DynamicJsonDocument doc;
    const size_t min_size = 256;
    const size_t max_size = MAX_ALLOWED_BUF_SIZE / 2; // Max RAM : 2 = da es für resizeDocument eng werden könnte?

    void resizeDocument(size_t requiredSize);
    size_t min(size_t a, size_t b);
    size_t max(size_t a, size_t b);
};

template<typename T>
void DynamicJsonHandler::addProperty(const std::string& key, const T& value) {
    size_t additionalSize = JSON_OBJECT_SIZE(1) + key.length() + sizeof(value);
    if (doc.memoryUsage() + additionalSize > doc.capacity()) {
        resizeDocument(doc.memoryUsage() + additionalSize);
    }
    doc[key] = value;
}

#endif /*__DYNAMICJSONHANDLER_H__*/
