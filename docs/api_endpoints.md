# ESP32 Firmware API Endpoints Documentation

This document describes all available API endpoints for the ESP32 firmware.

## Table of Contents
- [WiFi Configuration](#wifi-configuration)
- [System Information](#system-information)
- [WiFi Reset](#wifi-reset)
- [Crypto Information](#crypto-information)
- [Name Information](#name-information)
- [WiFi Status](#wifi-status)
- [WiFi Scan](#wifi-scan)
- [Initialize](#initialize)
- [BLE Stop](#ble-stop)
- [Crypto Sign](#crypto-sign)

---

## WiFi Configuration

Configure WiFi credentials for the device.

**Endpoint:** `/api/wifi`  
**Method:** `POST`  
**Content Type:** `application/json`

### Request Parameters

| Parameter | Type     | Required | Description              |
|-----------|----------|----------|--------------------------|
| ssid      | string   | Yes      | WiFi network name        |
| psk       | string   | Yes      | WiFi password            |

### Response

#### Success (200 OK)
```json
{
  "status": "success",
  "message": "WiFi credentials updated and connected"
}
```

#### Error (400 Bad Request)
```json
{
  "status": "error",
  "message": "Missing credentials"
}
```

#### Error (500 Internal Server Error)
```json
{
  "status": "error",
  "message": "Failed to connect with provided credentials"
}
```

---

## System Information

Get system information about the device.

**Endpoint:** `/api/system/info`  
**Method:** `GET`  
**Content Type:** `application/json`

### Response

#### Success (200 OK)
```json
{
  "deviceId": "DEVICE_SERIAL_NUMBER",
  "heap": 123456,
  "cpuFreq": 240,
  "flashSize": 4194304,
  "sdkVersion": "ESP-IDF v4.4",
  "publicKey": "PUBLIC_KEY_HEX",
  "wifiStatus": "connected",
  "localIP": "192.168.1.100",
  "ssid": "NETWORK_NAME",
  "rssi": -65
}
```

If not connected to WiFi, the response will exclude localIP, ssid, and rssi, and wifiStatus will be "disconnected".

---

## WiFi Reset

Reset WiFi configuration and start AP mode.

**Endpoint:** `/api/wifi/reset`  
**Method:** `POST`  
**Content Type:** `application/json`

### Response

#### Success (200 OK)
```json
{
  "status": "success",
  "message": "WiFi reset successful"
}
```

---

## Crypto Information

Get cryptographic information about the device.

**Endpoint:** `/api/crypto`  
**Method:** `GET`  
**Content Type:** `application/json`

### Response

#### Success (200 OK)
```json
{
  "deviceName": "software_zap",
  "serialNumber": "DEVICE_SERIAL_NUMBER",
  "publicKey": "PUBLIC_KEY_HEX"
}
```

---

## Name Information

Retrieve the device's three word name from the backend. This can be used to check for a valid connection to the backend API.

**Endpoint:** `/api/name`  
**Method:** `GET`  
**Content Type:** `application/json`

### Response

#### Success (200 OK)
```json
{
  "name": "DEVICE_NAME"
}
```

---

## WiFi Status

Get current WiFi connection status and available networks.

**Endpoint:** `/api/wifi`  
**Method:** `GET`  
**Content Type:** `application/json`

### Response

#### Success (200 OK)
```json
{
  "ssids": [
    "Network1",
    "Network2",
    "Network3"
  ],
  "connected": "CONNECTED_NETWORK_NAME"
}
```

If not connected to any WiFi network, the "connected" field will be null.

---

## WiFi Scan

Initiate an asynchronous WiFi network scan.

**Endpoint:** `/api/wifi/scan`  
**Method:** `GET`  
**Content Type:** `application/json`

### Response

#### Success (200 OK)
```json
{
  "status": "scan initiated"
}
```

---

## Initialize

Initialize the device with a wallet address.

**Endpoint:** `/api/initialize`  
**Method:** `GET`  
**Content Type:** `text/html`

Returns an HTML form for initialization.

---

**Endpoint:** `/api/initialize`  
**Method:** `POST`  
**Content Type:** `application/json`

### Request Parameters

| Parameter | Type     | Required | Description              |
|-----------|----------|----------|--------------------------|
| wallet    | string   | Yes      | Wallet address           |

### Response

#### Success (200 OK)
```json
{
  "idAndWallet": "DEVICE_SERIAL:WALLET_ADDRESS",
  "signature": "SIGNATURE_HEX"
}
```

#### Error (400 Bad Request)
```json
{
  "status": "error",
  "message": "Invalid JSON or missing wallet"
}
```

---

## BLE Stop

Schedule BLE service to stop.

**Endpoint:** `/api/ble/stop`  
**Method:** `POST`  
**Content Type:** `application/json`

### Response

#### Success (200 OK)
```json
{
  "status": "success",
  "message": "BLE shutdown scheduled"
}
```

#### Error (400 Bad Request) - When BLE not enabled
```json
{
  "status": "error",
  "message": "BLE not enabled"
}
```

---

## Crypto Sign

Sign data using the device's private key.

**Endpoint:** `/api/crypto/sign`  
**Method:** `POST`  
**Content Type:** `application/json`

### Request Parameters

| Parameter | Type     | Required | Description                                            |
|-----------|----------|----------|--------------------------------------------------------|
| message   | string   | No       | Message to sign (must not contain `|` characters)      |
| timestamp | string   | No       | Timestamp to use (must not contain `|` characters)     |

### Response

#### Success (200 OK)
```json
{
  "message": "MESSAGE|NONCE|TIMESTAMP|SERIAL",
  "sign": "SIGNATURE_HEX"
}
```

**Note:** If no message is provided, the resulting message format will be `NONCE|TIMESTAMP|SERIAL` (without the leading pipe character).

#### Error (400 Bad Request) - When message contains pipe characters
```json
{
  "status": "error",
  "message": "Message cannot contain | characters"
}
```

#### Error (400 Bad Request) - When timestamp contains pipe characters
```json
{
  "status": "error",
  "message": "Timestamp cannot contain | characters"
}
```

---

## Authentication

None of these endpoints require authentication. The device is designed to be accessed on a local network or via BLE.

## Format of Responses

Most responses are in JSON format with appropriate HTTP status codes. Some endpoints return HTML content.

## Additional Notes

- These endpoints can be accessed both via HTTP and BLE interfaces
- BLE interfaces use a custom protocol to map these endpoints to BLE characteristics
- For timestamp formatting, the format is `YYYY-MM-DDThh:mm:ssZ` (UTC time) 