#pragma once

#include <NimBLEClient.h>
#include <vector>

// Blackmagic Camera Service UUIDs
#define CAMERA_SERVICE_UUID "291D567A-6D75-11E6-8B77-86F30CA893D3"
#define CAMERA_CONTROL_UUID "5DD3465F-1AEE-4299-8493-D2ECA2F8E1BB"
#define CAMERA_STATUS_UUID "7FE8691D-95DC-4FC5-8ABD-CA74339B51B9"
#define CAMERA_TIMECODE_UUID "6D8F2110-86F1-41BF-9AFB-451D87E976C8"

class BMDCamera {
public:
    BMDCamera(NimBLEClient* client);
    
    bool connect();
    void disconnect();
    bool isConnected() const;
    bool isPaired() const;
    
    // Camera controls
    bool setFocus(float value);
    bool setAutoFocus();
    bool setISO(int value);
    bool setShutter(float value);
    bool setAutoExposure();
    bool setWhiteBalance(int value);
    bool setAutoWhiteBalance();
    bool setTint(int value);
    bool setAperture(float value);
    bool setFrameRate(float value);
    
    // Status
    float getBatteryLevel() const;
    bool isRecording() const;
    const String& getName() const;

private:
    bool sendCommand(uint8_t category, uint8_t parameter, uint8_t type, 
                    uint8_t operation, const std::vector<uint8_t>& data);
    
    NimBLEClient* client;
    NimBLERemoteService* cameraService;
    NimBLERemoteCharacteristic* controlChar;
    
    String name;
    float batteryLevel;
    bool recording;
    bool paired;
};

// Camera state structure
struct CameraState {
    NimBLEClient* client = nullptr;
    bool connected = false;
    bool paired = false;
    String name;
    float batteryLevel = 0;
    bool recording = false;
    uint32_t lastUpdate = 0;
};