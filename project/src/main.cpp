#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <AS5600.h>
#include <ESP32Encoder.h>
#include <ArduinoJson.h>
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
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
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
        ws.textAll(json.c_str());
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

void handleWebSocketMessage(AsyncWebSocketClient *client, const char *data) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, data);
    
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
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
    
    // Setup WebSocket
    ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, 
                  AwsEventType type, void *arg, uint8_t *data, size_t len) {
        if (type == WS_EVT_DATA) {
            data[len] = 0;
            handleWebSocketMessage(client, (char*)data);
        }
    });
    server.addHandler(&ws);
    
    // Start server
    server.begin();
}

void loop() {
    handleFocusControl();
    delay(1);
}