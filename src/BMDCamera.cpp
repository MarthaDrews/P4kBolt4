#include "BMDCamera.h"

BMDCamera::BMDCamera(NimBLEClient* client) : 
    client(client), 
    cameraService(nullptr),
    controlChar(nullptr),
    batteryLevel(0),
    recording(false),
    paired(false) {
}

bool BMDCamera::connect() {
    if (!client->connect()) {
        return false;
    }
    
    cameraService = client->getService(CAMERA_SERVICE_UUID);
    if (!cameraService) {
        client->disconnect();
        return false;
    }
    
    controlChar = cameraService->getCharacteristic(CAMERA_CONTROL_UUID);
    if (!controlChar) {
        client->disconnect();
        return false;
    }
    
    // Subscribe to notifications
    NimBLERemoteCharacteristic* statusChar = cameraService->getCharacteristic(
        "7FE8691D-95DC-4FC5-8ABD-CA74339B51B9"
    );
    if (statusChar) {
        statusChar->subscribe(true, [this](NimBLERemoteCharacteristic* pChar, 
                                         uint8_t* pData, size_t length, bool isNotify) {
            uint8_t status = pData[0];
            paired = (status & 0x04) != 0;
            // Handle other status flags
        });
    }
    
    return true;
}

void BMDCamera::disconnect() {
    if (client && client->isConnected()) {
        client->disconnect();
    }
    paired = false;
}

bool BMDCamera::isConnected() const {
    return client && client->isConnected();
}

bool BMDCamera::isPaired() const {
    return paired;
}

bool BMDCamera::sendCommand(uint8_t category, uint8_t parameter, uint8_t type,
                          uint8_t operation, const std::vector<uint8_t>& data) {
    if (!controlChar || !paired) return false;
    
    std::vector<uint8_t> packet;
    packet.push_back(0); // destination (camera 0)
    packet.push_back(data.size() + 4); // command length
    packet.push_back(0); // command id
    packet.push_back(0); // reserved
    packet.push_back(category);
    packet.push_back(parameter);
    packet.push_back(type);
    packet.push_back(operation);
    
    // Add command data
    packet.insert(packet.end(), data.begin(), data.end());
    
    // Add padding to 32-bit boundary
    while (packet.size() % 4 != 0) {
        packet.push_back(0);
    }
    
    return controlChar->writeValue(packet.data(), packet.size(), true);
}

bool BMDCamera::setFocus(float value) {
    // Convert float to fixed16 format (0.0 - 1.0 to 0 - 2048)
    int16_t fixed16 = static_cast<int16_t>(value * 2048);
    std::vector<uint8_t> data = {
        static_cast<uint8_t>(fixed16 & 0xFF),
        static_cast<uint8_t>((fixed16 >> 8) & 0xFF)
    };
    return sendCommand(0, 0, 128, 0, data); // Lens, Focus, fixed16, assign
}

bool BMDCamera::setAutoFocus() {
    return sendCommand(0, 1, 0, 0, std::vector<uint8_t>()); // Lens, AutoFocus, void, trigger
}

bool BMDCamera::setISO(int value) {
    std::vector<uint8_t> data(4);
    memcpy(data.data(), &value, 4);
    return sendCommand(1, 14, 3, 0, data); // Video, ISO, int32, assign
}

bool BMDCamera::setShutter(float value) {
    int32_t angle = static_cast<int32_t>(value * 100);
    std::vector<uint8_t> data(4);
    memcpy(data.data(), &angle, 4);
    return sendCommand(1, 11, 3, 0, data); // Video, Shutter, int32, assign
}

bool BMDCamera::setWhiteBalance(int value) {
    std::vector<uint8_t> data(2);
    int16_t temp = static_cast<int16_t>(value);
    memcpy(data.data(), &temp, 2);
    return sendCommand(1, 2, 2, 0, data); // Video, White Balance, int16, assign
}

bool BMDCamera::setTint(int value) {
    std::vector<uint8_t> data(2);
    int16_t tint = static_cast<int16_t>(value);
    memcpy(data.data(), &tint, 2);
    data.push_back(0); // Padding for white balance color temp
    data.push_back(0);
    return sendCommand(1, 2, 2, 0, data); // Video, White Balance, int16, assign
}

bool BMDCamera::setAperture(float value) {
    int16_t fixed16 = static_cast<int16_t>(value * 2048);
    std::vector<uint8_t> data = {
        static_cast<uint8_t>(fixed16 & 0xFF),
        static_cast<uint8_t>((fixed16 >> 8) & 0xFF)
    };
    return sendCommand(0, 3, 128, 0, data); // Lens, Aperture, fixed16, assign
}

bool BMDCamera::setFrameRate(float value) {
    int16_t fps = static_cast<int16_t>(value * 100);
    std::vector<uint8_t> data(2);
    memcpy(data.data(), &fps, 2);
    return sendCommand(1, 9, 2, 0, data); // Video, Frame Rate, int16, assign
}

float BMDCamera::getBatteryLevel() const {
    return batteryLevel;
}

bool BMDCamera::isRecording() const {
    return recording;
}

const String& BMDCamera::getName() const {
    return name;
}