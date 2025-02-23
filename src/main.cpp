#include "endpoint_types.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <uECC.h>
#include "mbedtls/md.h"
#include <mbedtls/base64.h>
#include "html.h"
#include "crypto.h"
#include <HTTPClient.h>
#include <esp_system.h>
#include "p1data.h"
// #if defined(USE_BLE_SETUP)
//     #include "ble_handler.h"
// #endif
#include "endpoint_mapper.h"
#include <WiFiClientSecure.h>
#include "esp_heap_caps.h"
#include "graphql.h"

// Define LED pin - adjust based on your board
#if defined(ARDUINO_HELTEC_WIFI_LORA_32) || defined(ARDUINO_HELTEC_WIFI_32)
    #define LED_PIN 25  // Heltec boards typically use GPIO25
#else
    #define LED_PIN 2   // Most ESP32 dev boards use GPIO2
#endif

// Configuration
const char* AP_SSID = "ESP32_Setup_V2";  // Name of the WiFi network created by ESP32
const char* AP_PASSWORD = "12345678";  // Password for the setup network
const char* MDNS_NAME = "myesp32";     // mDNS name - device will be accessible as myesp32.local
const char* API_URL = "https://api.srcful.dev/";
const char* DATA_URL = "https://mainnet.srcful.dev/gw/data/";

// Cryptographic key configuration
// This is a test private key - replace with your own secure key. In production keys are individial to each device and should be stored securely on the device.
// const char* PRIVATE_KEY_HEX = "52b16b97ac80d359c281ea6a04365e83acda5cf01d684ef9cbad42494309218c";
// const char* EXPECTED_PUBLIC_KEY_HEX = "07799794a4c69dbd809db46788b9ec501f32a87486385403c4e374871c67a65530862e7134e4570a0a138adaee4942a462acaf1297995a6b35707661c9257fb9";


const char* PRIVATE_KEY_HEX = "4cc43b88635b9eaf81655ed51e062fab4a46296d72f01fc6fd853b08f0c2383a";
const char* EXPECTED_PUBLIC_KEY_HEX = "3e70c4705ff5945bfea058aaa68128e6f7d54fd7e08c640f4791668f8267a6e8c36ee19214698f1956e948bf339492fb11e0dc5a79a76dd0c235b431ee5aa782";

// Global variables
WebServer server(80);
bool isProvisioned = false;
String configuredSSID = "";
String configuredPassword = "";
unsigned long lastJWTTime = 0;
const unsigned long JWT_INTERVAL = 10000; // 10 seconds in milliseconds
std::vector<String> lastScanResults;
unsigned long lastScanTime = 0;
const unsigned long SCAN_CACHE_TIME = 10000; // Cache scan results for 10 seconds
unsigned long bleShutdownTime = 0; // Time when BLE should be shut down (0 = no shutdown scheduled)

#if defined(USE_BLE_SETUP)
    #include "ble_handler.h"
    BLEHandler bleHandler(&server);
    bool isBleActive = false;  // Track BLE state
#endif

// Function declarations
void setupAP();
void setupEndpoints();
bool connectToWiFi(const String& ssid, const String& password, bool updateGlobals = true);
void runSigningTest();
String getId();
void scanWiFiNetworks();
void setupSSL();
void sendJWT();  // Add JWT sending function declaration

// Add hardcoded WiFi credentials
const char* WIFI_SSID = "may the source";
const char* WIFI_PSK = "B3W1thY0u!";

void setup() {
    Serial.begin(115200);

    Serial.printf("Total heap: %d\n", ESP.getHeapSize());
    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
    Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());
    
    // Initialize SSL early
    initSSL();
    
    // Setup LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    
    #if defined(DIRECT_CONNECT)
        // Connect to WiFi directly
        Serial.println("Connecting to WiFi...");
        WiFi.mode(WIFI_STA);
        if (connectToWiFi(WIFI_SSID, WIFI_PSK)) {
            digitalWrite(LED_PIN, HIGH); // Solid LED when connected
        } else {
            Serial.println("WiFi connection failed");
            digitalWrite(LED_PIN, LOW);
        }
    #endif
    
    // Setup SSL with optimized memory settings
    setupSSL();
    
    // Perform initial WiFi scan
    Serial.println("Starting WiFi scan...");
    scanWiFiNetworks();
    Serial.println("WiFi scan completed");
    
    // Verify public key
    Serial.println("Verifying public key...");
    String publicKey = crypto_get_public_key(PRIVATE_KEY_HEX);
    if (publicKey.length() == 0) {
        Serial.println("Failed to generate public key!");
        return;
    }
    
    if (publicKey != String(EXPECTED_PUBLIC_KEY_HEX)) {
        Serial.println("WARNING: Generated public key does not match expected public key!");
        return;
    }
    Serial.println("Key pair verified successfully");
    
    // Run signing test
    Serial.println("Running signing test...");
    runSigningTest();
    Serial.println("Signing test completed");
    
    // Continue with normal setup
    #if defined(USE_SOFTAP_SETUP)
        Serial.println("Setting up AP mode...");
        setupAP();
    #endif
    
    #if defined(USE_BLE_SETUP)
        Serial.println("Setting up BLE...");
        bleHandler.init();
        isBleActive = true;  // Set BLE active flag
    #endif
    
    #if defined(DIRECT_CONNECT)
        Serial.println("Using direct connection mode");
    #endif

    #if !defined(USE_SOFTAP_SETUP) && !defined(USE_BLE_SETUP) && !defined(DIRECT_CONNECT)
        Serial.println("ERROR: No connection mode defined!");
        #error "Must define either USE_SOFTAP_SETUP, USE_BLE_SETUP, or DIRECT_CONNECT"
    #endif
    
    Serial.println("Setting up MDNS...");
    if (MDNS.begin(MDNS_NAME)) {
        Serial.println("MDNS responder started");
    } else {
        Serial.println("Error setting up MDNS responder!");
    }
    
    Serial.println("Setting up HTTP endpoints...");
    setupEndpoints();
    Serial.println("Starting HTTP server...");
    server.begin();
    Serial.println("HTTP server started");
    Serial.println("Setup completed successfully!");
    Serial.print("Free heap after setup: ");
    Serial.println(ESP.getFreeHeap());
}

void loop() {
    static unsigned long lastCheck = 0;
    static bool wasConnected = false;
    
    // Check WiFi status every 5 seconds
    if (millis() - lastCheck > 5000) {
        lastCheck = millis();
        if (WiFi.status() == WL_CONNECTED) {
            if (!wasConnected) {
                Serial.println("WiFi connected");
                Serial.print("IP address: ");
                Serial.println(WiFi.localIP());
                wasConnected = true;
            }

            // Send JWT if conditions are met
            #if defined(USE_BLE_SETUP)
            if (!isBleActive && (millis() - lastJWTTime >= JWT_INTERVAL)) {
                sendJWT();
                lastJWTTime = millis();
            }
            #else
            if (millis() - lastJWTTime >= JWT_INTERVAL) {
                sendJWT();
                lastJWTTime = millis();
            }
            #endif

        } else {
            if (wasConnected) {
                Serial.println("WiFi connection lost!");
                wasConnected = false;
            }
            #if defined(DIRECT_CONNECT)
                // Only try to reconnect automatically in direct connect mode
                digitalWrite(LED_PIN, millis() % 1000 < 500); // Blink LED when disconnected
                WiFi.begin(WIFI_SSID, WIFI_PSK);
                Serial.println("WiFi disconnected, attempting to reconnect...");
            #endif
        }
        
        // Print some debug info
        Serial.print("Free heap: ");
        Serial.println(ESP.getFreeHeap());
        Serial.print("WiFi status: ");
        Serial.println(WiFi.status());
    }

    // Check if it's time to shut down BLE
    #if defined(USE_BLE_SETUP)
    if (bleShutdownTime > 0 && millis() >= bleShutdownTime) {
        Serial.println("Executing scheduled BLE shutdown");
        bleHandler.stop();
        isBleActive = false;
        bleShutdownTime = 0;  // Reset the timer
    }
    #endif

    // Handle incoming client requests if we're connected
    if (WiFi.status() == WL_CONNECTED) {
        digitalWrite(LED_PIN, HIGH); // Solid LED when connected
        server.handleClient();
    }
    
    yield();
}

void setupEndpoints() {
    Serial.println("Setting up endpoints...");

    // Print server configuration
    Serial.print("Server port: ");
    Serial.println(80);  // Default port
    
    // Handle root path separately as it serves HTML
    Serial.println("Registering root (/) endpoint...");
    server.on("/", HTTP_GET, []() {
        Serial.println("Handling root request");
        if (!isProvisioned) {
            #if defined(USE_SOFTAP_SETUP)
                // Check if we need a fresh scan
                if (millis() - lastScanTime >= SCAN_CACHE_TIME) {
                    scanWiFiNetworks();
                }
                
                // Create the network options HTML
                String networkOptions = "";
                for (const String& ssid : lastScanResults) {
                    networkOptions += "          <option value=\"" + ssid + "\">" + ssid + "</option>\n";
                }
                
                String html = String(WIFI_SETUP_HTML);
                html.replace("MDNS_NAME", MDNS_NAME);
                html.replace("NETWORK_OPTIONS", networkOptions);
                server.send(200, "text/html", html);
            #elif defined(USE_BLE_SETUP)
                // BLE setup page
                server.send(200, "text/html", "Please use BLE to configure device");
            #endif
        } else {
            // If already provisioned, redirect to system info
            server.sendHeader("Location", "/api/system/info", true);
            server.send(302, "text/plain", "");
        }
    });

    // Handle API endpoints using the endpoint mapper
    server.on("/api/wifi", HTTP_POST, []() {
        Serial.println("Handling POST /api/wifi request");
        EndpointRequest request;
        request.method = HttpMethod::POST;
        request.endpoint = Endpoint::WIFI_CONFIG;
        request.content = server.arg("plain");
        request.offset = 0;

        EndpointResponse response = EndpointMapper::route(request);
        server.send(response.statusCode, response.contentType, response.data);
    });

    server.on("/api/system/info", HTTP_GET, []() {
        EndpointRequest request;
        request.method = HttpMethod::GET;
        request.endpoint = Endpoint::SYSTEM_INFO;
        request.content = "";
        request.offset = 0;

        EndpointResponse response = EndpointMapper::route(request);
        server.send(response.statusCode, response.contentType, response.data);
    });

    server.on("/api/wifi/reset", HTTP_POST, []() {
        EndpointRequest request;
        request.method = HttpMethod::POST;
        request.endpoint = Endpoint::WIFI_RESET;
        request.content = "";
        request.offset = 0;

        EndpointResponse response = EndpointMapper::route(request);
        server.send(response.statusCode, response.contentType, response.data);
    });

    server.on("/api/crypto", HTTP_GET, []() {
        Serial.println("Handling GET /api/crypto request");
        EndpointRequest request;
        request.method = HttpMethod::GET;
        request.endpoint = Endpoint::CRYPTO_INFO;
        request.content = "";
        request.offset = 0;

        EndpointResponse response = EndpointMapper::route(request);
        server.send(response.statusCode, response.contentType, response.data);
    });

    server.on("/api/name", HTTP_GET, []() {
        EndpointRequest request;
        request.method = HttpMethod::GET;
        request.endpoint = Endpoint::NAME_INFO;
        request.content = "";
        request.offset = 0;

        EndpointResponse response = EndpointMapper::route(request);
        server.send(response.statusCode, response.contentType, response.data);
    });

    server.on("/api/wifi", HTTP_GET, []() {
        EndpointRequest request;
        request.method = HttpMethod::GET;
        request.endpoint = Endpoint::WIFI_STATUS;
        request.content = "";
        request.offset = 0;

        EndpointResponse response = EndpointMapper::route(request);
        server.send(response.statusCode, response.contentType, response.data);
    });

    server.on("/api/wifi/scan", HTTP_GET, []() {
        EndpointRequest request;
        request.method = HttpMethod::GET;
        request.endpoint = Endpoint::WIFI_SCAN;
        request.content = "";
        request.offset = 0;

        EndpointResponse response = EndpointMapper::route(request);
        server.send(response.statusCode, response.contentType, response.data);
    });

    server.on("/api/initialize", HTTP_GET, []() {
        EndpointRequest request;
        request.method = HttpMethod::GET;
        request.endpoint = Endpoint::INITIALIZE;
        request.content = "";
        request.offset = 0;

        EndpointResponse response = EndpointMapper::route(request);
        server.send(response.statusCode, response.contentType, response.data);
    });

    server.on("/api/initialize", HTTP_POST, []() {
        EndpointRequest request;
        request.method = HttpMethod::POST;
        request.endpoint = Endpoint::INITIALIZE;
        request.content = server.arg("plain");
        request.offset = 0;

        EndpointResponse response = EndpointMapper::route(request);
        server.send(response.statusCode, response.contentType, response.data);
    });

    server.on("/api/ble/stop", HTTP_POST, []() {
        Serial.println("Handling POST /api/ble/stop request");
        EndpointRequest request;
        request.method = HttpMethod::POST;
        request.endpoint = Endpoint::BLE_STOP;
        request.content = "";
        request.offset = 0;

        EndpointResponse response = EndpointMapper::route(request);
        server.send(response.statusCode, response.contentType, response.data);
    });

    // Handle not found
    server.onNotFound([]() {
        Serial.println("404 - Not found: " + server.uri());
        EndpointRequest request;
        request.method = server.method() == HTTP_GET ? HttpMethod::GET : HttpMethod::POST;
        request.endpoint = EndpointMapper::pathToEndpoint(server.uri());
        request.content = server.arg("plain");
        request.offset = 0;

        EndpointResponse response = EndpointMapper::route(request);
        server.send(response.statusCode, response.contentType, response.data);
    });
}

void setupAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
}

bool connectToWiFi(const String& ssid, const String& password, bool updateGlobals) {
    if (ssid.length() > 0 && password.length() > 0) {
        Serial.println("Connecting to WiFi...");
        Serial.print("SSID: ");
        Serial.println(ssid);
        Serial.print("Password length: ");
        Serial.println(password.length());
        
        // First disconnect and wait a bit
        WiFi.disconnect(true);  // true = disconnect and clear settings
        delay(1000);  // Give it time to complete
        
        // Set mode and wait
        WiFi.mode(WIFI_STA);
        delay(100);
        
        // Start connection
        WiFi.begin(ssid.c_str(), password.c_str());
        
        // Wait for connection with longer timeout
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 30) {  // Increased timeout to 15 seconds
            delay(500);
            Serial.print(".");
            attempts++;
        }
        Serial.println();
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("WiFi connected");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            
            // Initialize NTP time synchronization
            Serial.println("Initializing NTP...");
            initNTP();
            Serial.println("NTP initialized");
            
            // Configure low power WiFi
            WiFi.setSleep(true);  // Enable modem sleep
            
            // Update global variables if requested
            if (updateGlobals) {
                configuredSSID = ssid;
                configuredPassword = password;
                isProvisioned = true;
            }
            
            return true;
        } else {
            Serial.println("WiFi connection failed");
            WiFi.disconnect(true);  // Clean disconnect on failure
            return false;
        }
    } else {
        Serial.println("No WiFi credentials provided");
        return false;
    }
}

void runSigningTest() {
  Serial.println("\n=== Running Signing Test ===");
  
  // Original JWT test
  String deviceId = getId();
  const char* testHeader = R"({"alg":"ES256K","typ":"JWT"})";
  String testPayloadStr = "{\"sub\":\"" + deviceId + "\",\"name\":\"John Doe\",\"iat\":1516239022}";
  
  Serial.println("Creating test JWT...");
  Serial.print("Header: ");
  Serial.println(testHeader);
  Serial.print("Payload: ");
  Serial.println(testPayloadStr);
  
  String jwt = crypto_create_jwt(testHeader, testPayloadStr.c_str(), PRIVATE_KEY_HEX);
  
  if (jwt.length() == 0) {
    Serial.println("TEST FAILED: JWT creation failed!");
    return;
  }
  
  Serial.println("\nFinal JWT:");
  Serial.println(jwt);
  
  // Add specific test case
  Serial.println("\n=== Running Specific Signature Test ===");
  const char* testMessage = "zap_000098f89ec964:Bygcy876b3bsjMvvhZxghvs3EyR5y6a7vpvAp5D62n2w";
  Serial.print("Test message: ");
  Serial.println(testMessage);
  
  String hexSignature = crypto_create_signature_hex(testMessage, PRIVATE_KEY_HEX);
  Serial.print("Hex signature: ");
  Serial.println(hexSignature);
  
  String b64urlSignature = crypto_create_signature_base64url(testMessage, PRIVATE_KEY_HEX);
  Serial.print("Base64URL signature: ");
  Serial.println(b64urlSignature);
  
  Serial.println("=== Signing Tests Complete ===\n");
}

String getId() {
  uint64_t chipId = ESP.getEfuseMac();
  char serial[17];
  snprintf(serial, sizeof(serial), "%016llx", chipId);
  
  String id = "zap-" + String(serial);
  
  // Ensure exactly 18 characters
  if (id.length() > 18) {
    // Truncate to 18 chars if longer
    id = id.substring(0, 18);
  } else while (id.length() < 18) {
    // Pad with 'e' if shorter
    id += 'e';
  }
  
  return id;
}

void scanWiFiNetworks() {
  Serial.println("Scanning WiFi networks...");
  
  // Set WiFi to station mode to perform scan
  WiFi.mode(WIFI_STA);
  
  int n = WiFi.scanNetworks();
  Serial.println("Scan completed");
  
  lastScanResults.clear();
  
  if (n == 0) {
    Serial.println("No networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    
    // Store unique SSIDs (some networks might broadcast on multiple channels)
    std::vector<String> uniqueSSIDs;
    for (int i = 0; i < n; ++i) {
      String ssid = WiFi.SSID(i);
      if (std::find(uniqueSSIDs.begin(), uniqueSSIDs.end(), ssid) == uniqueSSIDs.end()) {
        uniqueSSIDs.push_back(ssid);
      }
    }
    
    // Sort alphabetically
    std::sort(uniqueSSIDs.begin(), uniqueSSIDs.end());
    
    // Store in global vector
    lastScanResults = uniqueSSIDs;
  }
  
  lastScanTime = millis();
  
  // Return to original mode if we were in AP mode
  if (isProvisioned) {
    WiFi.mode(WIFI_STA);
  } else {
    setupAP();
  }
}

void setupSSL() {
    // Nothing needed here - HTTPClient handles SSL internally
}

// Add JWT sending function
void sendJWT() {
    String deviceId = getId();
    
    // Create JWT using P1 data
    String jwt = createP1JWT(PRIVATE_KEY_HEX, deviceId);
    if (jwt.length() > 0) {
        Serial.println("P1 JWT created successfully");
        Serial.println("JWT: " + jwt);  // Add JWT logging
        
        // Create HTTP client and configure it
        HTTPClient http;
        http.setTimeout(10000);  // 10 second timeout
        
        Serial.print("Sending JWT to: ");
        Serial.println(DATA_URL);
        
        // Start the request
        if (http.begin(DATA_URL)) {
            // Add headers
            http.addHeader("Content-Type", "text/plain");
            
            // Send POST request with JWT as body
            int httpResponseCode = http.POST(jwt);
            
            if (httpResponseCode > 0) {
                Serial.print("HTTP Response code: ");
                Serial.println(httpResponseCode);
                String response = http.getString();
                Serial.println("Response: " + response);
            } else {
                Serial.print("Error code: ");
                Serial.println(httpResponseCode);
            }
            
            // Clean up
            http.end();
        } else {
            Serial.println("Failed to connect to server");
        }
    } else {
        Serial.println("Failed to create P1 JWT");
    }
}