#include "endpoint_types.h"
#include "ble_handler.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <esp_system.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "crypto.h"
#include "endpoint_mapper.h"

BLEHandler::BLEHandler(WebServer* server) : webServer(server) {
    pServer = nullptr;
    pService = nullptr;
    pRequestChar = nullptr;
    pResponseChar = nullptr;
    isAdvertising = false;
}

void BLEHandler::init() {
    // Initialize BLE with reduced features
    BLEDevice::init("Sourceful Gateway Zap");
    btStart();
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);  // Release classic BT memory
    
    // Create server
    pServer = BLEDevice::createServer();
    
    // Create service
    pService = pServer->createService(SRCFUL_SERVICE_UUID);
    
    // Create characteristics with notification support
    pRequestChar = pService->createCharacteristic(
        SRCFUL_REQUEST_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE | 
        BLECharacteristic::PROPERTY_NOTIFY
    );
    
    pResponseChar = pService->createCharacteristic(
        SRCFUL_RESPONSE_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | 
        BLECharacteristic::PROPERTY_NOTIFY | 
        BLECharacteristic::PROPERTY_INDICATE
    );
    
    // Create BLE Descriptors for notifications
    pRequestChar->addDescriptor(new BLE2902());
    pResponseChar->addDescriptor(new BLE2902());
    
    // Set callbacks
    pRequestCallback = new BLERequestCallback(this);
    pResponseCallback = new BLEResponseCallback(this);
    pRequestChar->setCallbacks(pRequestCallback);
    pResponseChar->setCallbacks(pResponseCallback);
    
    // Start service
    pService->start();

    // Improved advertising configuration
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SRCFUL_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
    pAdvertising->setMaxPreferred(0x12);
    
    // Start advertising
    BLEDevice::startAdvertising();
    isAdvertising = true;
    Serial.println("BLE service started and advertising");
}

void BLEHandler::stop() {
    if (pServer != nullptr) {
        pServer->getAdvertising()->stop();
        isAdvertising = false;
        BLEDevice::deinit(true);  // true = release memory
        Serial.println("BLE stopped and resources released");
    }
}

void BLEHandler::checkAdvertising() {
    if (!isAdvertising) {
        Serial.println("BLE advertising stopped - restarting");
        BLEDevice::startAdvertising();
        isAdvertising = true;
    }
}

// Optimize string literals by making them PROGMEM
static const char RESPONSE_OK[] PROGMEM = "EGWTP/1.1 200 OK\r\n";
static const char CONTENT_TYPE[] PROGMEM = "Content-Type: text/json\r\n";
static const char CONTENT_LENGTH[] PROGMEM = "Content-Length: ";
static const char LOCATION[] PROGMEM = "Location: ";
static const char METHOD[] PROGMEM = "Method: ";
static const char OFFSET[] PROGMEM = "Offset: ";

// Error messages
static const char ERROR_INVALID_JSON[] PROGMEM = "{\"status\":\"error\",\"message\":\"Invalid JSON\"}";
static const char ERROR_MISSING_CREDS[] PROGMEM = "{\"status\":\"error\",\"message\":\"Missing credentials\"}";
static const char ERROR_INVALID_REQUEST[] PROGMEM = "{\"status\":\"error\",\"message\":\"Invalid request format\"}";
static const char ERROR_NOT_FOUND[] PROGMEM = "{\"status\":\"error\",\"message\":\"Endpoint not found\"}";
static const char SUCCESS_WIFI_UPDATE[] PROGMEM = "{\"status\":\"success\",\"message\":\"WiFi credentials updated\"}";
static const char SUCCESS_WIFI_RESET[] PROGMEM = "{\"status\":\"success\",\"message\":\"WiFi reset successful\"}";

// Modify constructResponse to use PROGMEM strings
String BLEHandler::constructResponse(const String& location, const String& method, 
                                   const String& data, int offset) {
    String header;
    
    // Add each part separately to avoid concatenation issues
    header += FPSTR(RESPONSE_OK);
    header += FPSTR(LOCATION);
    header += location;
    header += "\r\n";
    
    header += FPSTR(METHOD);
    header += method;
    header += "\r\n";
    
    header += FPSTR(CONTENT_TYPE);
    
    header += FPSTR(CONTENT_LENGTH);
    header += String(data.length());
    header += "\r\n";
    
    if (offset > 0) {
        header += FPSTR(OFFSET);
        header += String(offset);
        header += "\r\n";
    }
    
    header += "\r\n";
    String response = header + data.substring(offset);
    
    if (response.length() > MAX_BLE_PACKET_SIZE) {
        response = response.substring(0, MAX_BLE_PACKET_SIZE);
    }
    
    return response;
}

bool BLEHandler::sendResponse(const String& location, const String& method, 
                            const String& data, int offset) {
    String response = constructResponse(location, method, data, offset);
    pResponseChar->setValue((uint8_t*)response.c_str(), response.length());
    pResponseChar->notify();  // Add notification
    return true;
}

// Update error responses to use PROGMEM strings
void BLEHandler::handleRequest(const String& request) {
    String method, path, content;
    int offset = 0;
    
    if (!parseRequest(request, method, path, content, offset)) {
        sendResponse(path, method, FPSTR(ERROR_INVALID_REQUEST), 0);
        return;
    }
    
    handleRequestInternal(method, path, content, offset);
}

void BLEHandler::handleRequestInternal(const String& method, const String& path, 
                                     const String& content, int offset) {
    // Create endpoint request
    EndpointRequest request;
    request.method = EndpointMapper::stringToMethod(method);
    request.endpoint = EndpointMapper::pathToEndpoint(path);
    request.content = content;
    request.offset = offset;

    // Route request through endpoint mapper
    EndpointResponse response = EndpointMapper::route(request);

    // Send response using BLE protocol format
    sendResponse(path, method, response.data, offset);
}

bool BLEHandler::parseRequest(const String& request, String& method, String& path, 
                            String& content, int& offset) {
    int headerEnd = request.indexOf("\r\n\r\n");
    if (headerEnd == -1) return false;
    
    String header = request.substring(0, headerEnd);
    content = request.substring(headerEnd + 4);
    
    // Parse first line
    int firstLineEnd = header.indexOf("\r\n");
    String firstLine = header.substring(0, firstLineEnd);
    
    if (!firstLine.endsWith(" EGWTTP/1.1")) return false;
    
    int methodEnd = firstLine.indexOf(" ");
    method = firstLine.substring(0, methodEnd);
    
    // Get path and trim whitespace
    path = firstLine.substring(methodEnd + 1, firstLine.length() - 10);
    path.trim();  // Trim as a separate operation
    
    // Parse headers
    offset = 0;
    if (header.indexOf("Offset: ") != -1) {
        int offsetStart = header.indexOf("Offset: ") + 8;
        int offsetEnd = header.indexOf("\r\n", offsetStart);
        offset = header.substring(offsetStart, offsetEnd).toInt();
    }
    
    return true;
}

void BLERequestCallback::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
        Serial.println("Received BLE write request:");
        Serial.println(value.c_str());
        String request = String(value.c_str());
        handler->handleRequest(request);
    }
}

void BLEResponseCallback::onRead(BLECharacteristic* pCharacteristic) {
    Serial.println("BLE read request received");
} 