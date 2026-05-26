#ifndef I2CDEVICE_H
#define I2CDEVICE_H

#include <Arduino.h>
#include <Wire.h>
class I2CDevice {
public:
    I2CDevice(byte i2c_address, TwoWire *i2c_bus);
    ~I2CDevice();
    byte readRegister(byte offset);
    bool writeRegister(byte offset, byte value);
    void readMultipleRegisters(uint8_t offset, uint8_t *buffer, uint8_t buffer_size, uint8_t length);
    bool writeMultipleRegisters(uint8_t offset, const uint8_t *buffer, uint8_t buffer_size, uint8_t length);
protected:
    byte _i2c_address;
    TwoWire *_wire;
};
#endif