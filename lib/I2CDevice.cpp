/*
*/

#include "I2CDevice.h"

I2CDevice::I2CDevice(byte i2c_address, TwoWire *i2c_bus) : _i2c_address(i2c_address), _wire(i2c_bus) {
    this -> _i2c_address = i2c_address;
    this -> _wire = i2c_bus;
}

I2CDevice::~I2CDevice() {
    // Nothing to clean up
}

byte I2CDevice::readRegister(byte offset) {
    _wire->beginTransmission(_i2c_address);
    _wire->write(offset);
    _wire->endTransmission(false); // Send a restart, not a stop
    _wire->requestFrom(_i2c_address, (byte)1);
    if (_wire->available()) {
        return _wire->read();
    }
    return 0; // Return 0 if no data is available
}

bool I2CDevice::writeRegister(byte offset, byte value) {
    _wire->beginTransmission(_i2c_address);
    _wire->write(offset);
    _wire->write(value);
    return _wire->endTransmission() == 0; // Return true if transmission was successful
}

void I2CDevice::readMultipleRegisters(uint8_t offset, uint8_t *buffer, uint8_t buffer_size, uint8_t length) {
    if(buffer == nullptr || buffer_size == 0) {
        return; // Invalid buffer
    }

    if (length > buffer_size) {
        length = buffer_size; // Prevent buffer overflow
    }

    uint8_t bytesRead = 0;
    _wire->beginTransmission(_i2c_address);
    _wire->write(offset);
    uint8_t i2c_err = _wire->endTransmission(false); // Send a restart, not a stop

    if (i2c_err != 0) {
        return; // Transmission error
    }

    uint8_t requested_buffer_size = _wire->requestFrom(_i2c_address, length);
    while (_wire->available() && bytesRead < requested_buffer_size && bytesRead < length) {
        int value = _wire->read();
        if (value < 0) {
            break;
        }

        buffer[bytesRead++] = static_cast<uint8_t>(value);
    }
}

bool I2CDevice::writeMultipleRegisters(uint8_t offset, const uint8_t *buffer, uint8_t buffer_size, uint8_t length) {
    if(buffer == nullptr || buffer_size == 0) {
        return false; // Invalid buffer
    }

    if (length > buffer_size) {
        length = buffer_size; // Prevent buffer overflow
    }

    _wire->beginTransmission(_i2c_address);
    _wire->write(offset);
    for (uint8_t i = 0; i < length; i++) {
        _wire->write(buffer[i]);
    }
    return _wire->endTransmission() == 0; // Return true if transmission was successful
}
