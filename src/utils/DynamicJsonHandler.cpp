#include "DynamicJsonHandler.h"

DynamicJsonHandler::DynamicJsonHandler() : doc(min_size) {
}

DynamicJsonHandler::~DynamicJsonHandler() {
    delete &doc;
}

String DynamicJsonHandler::toString() {
    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}

void DynamicJsonHandler::clear() {
    doc.clear();
}

size_t DynamicJsonHandler::size() const {
    return doc.memoryUsage();
}

void DynamicJsonHandler::resizeDocument(size_t requiredSize) {
    // TODO: multiplikator zwei muss ersetzt werden? Kann noch minimal werden.
    size_t newCapacity = min(max(requiredSize * 2, min_size), max_size);
    DynamicJsonDocument newDoc(newCapacity);
    newDoc.set(doc); // Bestehende Daten kopieren
    doc = std::move(newDoc);
}

size_t DynamicJsonHandler::min(size_t a, size_t b) {
    return (a < b) ? a : b;
}

size_t DynamicJsonHandler::max(size_t a, size_t b) {
    return (a > b) ? a : b;
}
