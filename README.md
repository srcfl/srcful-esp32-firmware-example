# Srcful ESP32 Firmware

Example project for ESP32-based hardware gateway for the Srcful platform.

## Overview

This firmware provides a gateway interface for the Srcful Energy Network using ESP32 hardware. It supports both WiFi and Bluetooth Low Energy (BLE) connectivity, allowing for flexible deployment options. The firmware offers a comprehensive API that enables device configuration, cryptographic operations, and system management.

## Features

- **Dual Connectivity**: Connect via WiFi or BLE
- **Secure Communications**: Cryptographic operations including signing
- **Device Management**: Configure, reset, and manage the device
- **Network Management**: WiFi scanning and configuration
- **Status Reporting**: Detailed system information and status

## Getting Started

### Prerequisites

- ESP32 development board
- Arduino IDE with ESP32 board support installed
- Required libraries:
  - ArduinoJson
  - BLEDevice
  - WiFi
  - WebServer
  - uECC
  - mbedtls

### Installation

1. Clone this repository:
   ```
   git clone https://github.com/srcful/srcful-esp32-firmware-example.git
   ```

2. Open the project in Arduino IDE or your preferred ESP32 development environment

3. Install required libraries through the Arduino Library Manager

4. Configure your device settings in the appropriate configuration files

5. Upload the firmware to your ESP32 device

### Initial Setup

Once the firmware is uploaded, the device will start in AP (Access Point) mode. Connect to the device's WiFi network and use the web interface to configure it for your local WiFi network.

## API Reference

The firmware exposes several API endpoints for interaction. These can be accessed via HTTP when connected to WiFi, or via BLE.

Key available endpoints include:

- `/api/wifi` - Configure WiFi settings or get WiFi status
- `/api/crypto` - Get device cryptographic information 
- `/api/crypto/sign` - Sign data with the device's private key
- `/api/initialize` - Initialize the device with a wallet address

For comprehensive API documentation, see [API Endpoints Documentation](docs/api_endpoints.md).

## Data Transmission and Authentication

### JSON Web Tokens (JWT)

The firmware uses JSON Web Tokens (JWT) for secure data transmission and authentication:

* **JWT Structure**: Each token consists of three parts separated by dots:
  * Header - Contains token type and the signing algorithm (ES256)
  * Payload - Contains the actual data being transmitted
  * Signature - Created using the device's private key to verify authenticity

* **Cryptographic Security**: 
  * Tokens are signed using ECDSA with the secp256r1 curve
  * Each device has a unique private key for signing operations
  * The signature can be verified using the device's public key

* **Automatic Data Transmission**: 
  * Device regularly creates and sends JWTs containing telemetry data
  * Data is transmitted at configurable intervals to the Sourceful Energy Network backend
  * JWTs payload includes timestamps to ensure data integrity and sequencing

### Data Flow

1. **Data Collection**: The device collects data (such as P1 meter readings)
2. **JWT Creation**: The data is packaged into a JWT payload
3. **Signing**: The JWT is signed using the device's private key
4. **Transmission**: The complete JWT is sent to the configured endpoint via HTTP POST
5. **Verification**: The server verifies the token signature using the device's public key
6. **Processing**: Upon successful verification, the data is processed and stored

### Manual JWT Creation

The firmware includes a web interface for manually creating and testing JWTs. This can be useful for:
* Verifying the signing process is working correctly
* Testing data transmission to different endpoints
* Debugging connectivity issues

## BLE Interface

The BLE interface provides access to the same functionality as the HTTP API. The device advertises as "Sourceful Gateway Zap" and uses custom service and characteristic UUIDs for communication.

The BLE protocol is compatible with the API endpoints structure, mapping requests and responses between the BLE characteristics and HTTP-style endpoints.

## Security Notes

- The device stores a private key that is used for cryptographic operations - this should be handled in a more secure way in a production setting
- Communication over HTTP is not encrypted; for production use, consider implementing additional security measures - though the indended use is over a local network - not the Internet.
- BLE communications have limited security features enabled by default

## Development and Contribution

### Project Structure

- `src/` - Main source code
  - `main.cpp` - Entry point and initialization
  - `endpoints.h/cpp` - API endpoint implementations
  - `endpoint_mapper.h/cpp` - Maps URLs to endpoint handlers
  - `crypto.h/cpp` - Cryptographic operations
  - `ble_handler.h/cpp` - BLE communication handling

### Adding New Endpoints

1. Define a new endpoint type in `src/endpoint_types.h`
2. Add a path constant in `src/endpoint_mapper.h` and define it in `src/endpoint_mapper.cpp`
3. Declare your handler function in `src/endpoints.h`
4. Implement your handler in `src/endpoints.cpp`
5. Update the route method in `src/endpoint_mapper.cpp` to handle the new endpoint

## License

This project is licensed under the [MIT License](LICENSE) - see the LICENSE file for details.

