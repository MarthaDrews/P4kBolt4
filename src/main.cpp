#include <Arduino.h>
#include <NimBLEDevice.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <AS5600.h>
#include <ESP32Encoder.h>
#include <ArduinoJson.h>
#include <WebSocketsServer.h>
#include "BMDCamera.h"

// Pin definitions
#define POTI_PIN 36
#define AS5600_SDA 21
#define AS5600_SCL 22

// Constants
#define MAX_CAMERAS 8
#define WIFI_SSID "BMPCC-node"
#define WIFI_PASS "esp32p4k"
#define BLE_SCAN_DURATION 5

// Global objects
WebServer server(80);
WebSocketsServer webSocket(81);
AS5600 encoder;

// Camera states and settings
CameraState cameras[MAX_CAMERAS];
int activeCameraIndex = 0;
int activeCameraCount = 0;

// Focus control settings
struct FocusSettings {
    int encoderDegrees = 360;
    float sensitivity = 1.0;
    bool synchronized = false;
    float lastEncoderValue = 0;
    float lastPotiValue = 0;
} focusSettings;

// Bluetooth scanning and connection
class MyAdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        if (advertisedDevice->getName() == "BMPCC4K" && activeCameraCount < MAX_CAMERAS) {
            NimBLEDevice::getScan()->stop();
            connectToCamera(advertisedDevice);
        }
    }
};

void connectToCamera(NimBLEAdvertisedDevice* advertisedDevice) {
    if (activeCameraCount >= MAX_CAMERAS) return;

    NimBLEClient* client = NimBLEDevice::createClient();
    BMDCamera* camera = new BMDCamera(client);
    
    if (camera->connect()) {
        cameras[activeCameraCount].client = client;
        cameras[activeCameraCount].connected = true;
        cameras[activeCameraCount].name = advertisedDevice->getName();
        activeCameraCount++;
        
        // Notify web interface
        String json;
        StaticJsonDocument<200> doc;
        doc["type"] = "camera_connected";
        doc["index"] = activeCameraCount - 1;
        doc["name"] = advertisedDevice->getName();
        serializeJson(doc, json);
        webSocket.broadcastTXT(json);
    } else {
        delete camera;
    }
}

void handleFocusControl() {
    static unsigned long lastUpdate = 0;
    const unsigned long updateInterval = 20; // 50Hz update rate
    
    if (millis() - lastUpdate < updateInterval) return;
    lastUpdate = millis();
    
    // Read encoder
    float encoderValue = encoder.getRawAngle() * 360.0f / 4096.0f;
    float encoderDelta = 0;
    
    if (abs(encoderValue - focusSettings.lastEncoderValue) > 180) {
        // Handle wrap-around
        if (encoderValue > focusSettings.lastEncoderValue) {
            encoderDelta = -(360 - (encoderValue - focusSettings.lastEncoderValue));
        } else {
            encoderDelta = 360 - (focusSettings.lastEncoderValue - encoderValue);
        }
    } else {
        encoderDelta = encoderValue - focusSettings.lastEncoderValue;
    }
    
    focusSettings.lastEncoderValue = encoderValue;
    
    // Read potentiometer
    float potiValue = analogRead(POTI_PIN) / 4095.0f;
    float potiDelta = potiValue - focusSettings.lastPotiValue;
    focusSettings.lastPotiValue = potiValue;
    
    // Apply sensitivity
    float focusDelta = (encoderDelta / focusSettings.encoderDegrees + potiDelta) * focusSettings.sensitivity;
    
    // Update focus on active camera(s)
    if (focusSettings.synchronized) {
        // Update all connected cameras
        for (int i = 0; i < activeCameraCount; i++) {
            if (cameras[i].connected && cameras[i].paired) {
                cameras[i].client->setFocus(focusDelta);
            }
        }
    } else if (activeCameraIndex < activeCameraCount) {
        // Update only active camera
        if (cameras[activeCameraIndex].connected && cameras[activeCameraIndex].paired) {
            cameras[activeCameraIndex].client->setFocus(focusDelta);
        }
    }
}

void handleWebSocketMessage(uint8_t num, uint8_t* payload, size_t length) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.println("Failed to parse WebSocket message");
        return;
    }
    
    const char* type = doc["type"];
    
    if (strcmp(type, "connect_camera") == 0) {
        NimBLEDevice::getScan()->start(BLE_SCAN_DURATION, false);
    } 
    else if (strcmp(type, "set_focus") == 0) {
        float value = doc["value"];
        if (activeCameraIndex < activeCameraCount) {
            cameras[activeCameraIndex].client->setFocus(value);
        }
    }
    else if (strcmp(type, "set_sensitivity") == 0) {
        focusSettings.sensitivity = doc["value"];
    }
    else if (strcmp(type, "set_encoder_degrees") == 0) {
        focusSettings.encoderDegrees = doc["value"];
    }
    else if (strcmp(type, "set_sync_mode") == 0) {
        focusSettings.synchronized = doc["value"];
    }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            Serial.printf("[%u] Connected!\n", num);
            break;
        case WStype_TEXT:
            handleWebSocketMessage(num, payload, length);
            break;
    }
}

void handleRoot() {
    File file = SPIFFS.open("/index.html", "r");
    if (!file) {
        server.send(404, "text/plain", "File not found");
        return;
    }
    server.streamFile(file, "text/html");
    file.close();
}

void setup() {
    Serial.begin(115200);
    
    // Initialize SPIFFS
    if(!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    // Initialize I2C for AS5600
    Wire.begin(AS5600_SDA, AS5600_SCL);
    encoder.begin(AS5600_SDA, AS5600_SCL);
    
    // Initialize ADC for potentiometer
    analogReadResolution(12);
    
    // Initialize WiFi Access Point
    WiFi.softAP(WIFI_SSID, WIFI_PASS);
    Serial.println("WiFi AP Started");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    // Initialize NimBLE
    NimBLEDevice::init("ESP32 Camera Control");
    NimBLEDevice::getScan()->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    NimBLEDevice::getScan()->setActiveScan(true);
    
    // Setup web server routes
    server.on("/", HTTP_GET, handleRoot);
    server.serveStatic("/", SPIFFS, "/");
    
    // Setup WebSocket
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    
    // Start server
    server.begin();
}

void loop() {
    webSocket.loop();
    server.handleClient();
    handleFocusControl();
    delay(1);
}