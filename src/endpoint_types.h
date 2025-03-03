#pragma once

#include <Arduino.h>

// Define HTTP methods as an enum
enum class HttpMethod {
    GET,
    POST,
    UNKNOWN
};

// Define all possible endpoints as an enum
enum class Endpoint {
    WIFI_CONFIG,
    SYSTEM_INFO,
    WIFI_RESET,
    CRYPTO_INFO,
    NAME_INFO,
    WIFI_STATUS,
    WIFI_SCAN,
    INITIALIZE,
    BLE_STOP,
    CRYPTO_SIGN,
    UNKNOWN
}; 