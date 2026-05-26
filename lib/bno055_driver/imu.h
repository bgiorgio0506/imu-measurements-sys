/* imu.h
 * Author: Giorgio Bella
 * Created on: 2024-06-01
 * Sensor: BNO055
 * Driver: Adafruit_BNO055
 * Description: Header file for the IMU class
 */
#ifndef IMU_H
#define IMU_H

#include <Arduino.h>
#include "sensor_constant.h"
#include "I2CDevice.h"
#include "util/quaternion.h"
#include "util/enabledAxes.h"
#include "util/gyro.h"

class IMU : public I2CDevice {
    private:
        int BNO055_ACCEL_CONVERSION_FACTOR = 100.0; // LSB/ m/s^2
        int BNO055_GYRO_CONVERSION_FACTOR = 16.0; // LSB/ dps
        int BNO055_MAG_CONVERSION_FACTOR = 16.0; // LSB/ uT
        int BNO055_TEMP_CONVERSION_FACTOR = 1.0; // LSB/ °C

        Vector<int16_t> getVector(byte offset);
        Vector<int16_t> getVector(const byte *buffer, int start_index);

        Quaternion getQuaternion(const byte *buffer, int start_index);

        BNO055_OPERATION_MODE current_operation_mode = CONFIGMODE; // Default operation mode after power-on or reset
    public: 
    //I2C Device Setup functions
    using I2CDevice::I2CDevice; // Inherit constructor from I2CDevice
    bool begin();
    #pragma region CONFIGURATION
    /**
     * Set the power mode for the BNO055. Refer to section 3.2 of the datasheet
     * @param mode The BNO055_POWER_MODE to be set
     * @return Whether writing the new mode to the PWR_MODE register was successful
     */
    bool setPowerMode(BNO055_POWER_MODE mode);

    /**
     * @return The current BNO055_POWER_MODE that the sensor is in
     */
    BNO055_POWER_MODE getPowerMode();

    /**
     * Set the operation mode for the BNO055. Refer to section 3.3 of the datasheet
     * @param mode The BNO055_OPERATION_MODE to be set
     * @return Whether writing the new mode to the OPR_MODE register was successful
     */
    bool setOperationMode(BNO055_OPERATION_MODE mode);

    /**
     * @return The current BNO055_OPERATION_MODE that the sensor is in
     */
    BNO055_OPERATION_MODE getOperationMode();

    /**
     * Trigger a power-on reset (POR), wait for the reset to happen (650ms) and then run a test
     * to see if the restarted properly.
     * @return Whether the reset was successful.
     */
    bool resetSystem();

    bool performSelfTest();
    #pragma endregion CONFIGURATION

    #pragma region DATA_READING
    //Data reading functions
    bno055_burst_t readAllData();

    /*
    * Set the page ID for the BNO055. Refer to section 3.4 of the datasheet
    * @param page_id The page ID to be set
    * @return Whether writing the new page ID to the PAGE_ID register was successful
    */
    bool setPageID(uint8_t page_id);

    /*
     * @return The current page ID that the sensor is on
     */
    uint8_t getPageID();
    #pragma endregion DATA_READING

    #pragma region RAW_DATA_READING
    //Get functions for different instrument the data is raw format. 
    Vector<double> getAccelerometer();
    Vector<double> getMagnetometer();
    Vector<double> getGyroscope();
    Vector<double> getLinearAcceleration();
    Vector<double> getGravityVector();
    Quaternion getQuaternion();
    Vector<double> getEuler();

    //Temp reading functions
    byte getTemperature();
    BNO055_TEMP_SOURCE_TYPE getTemperatureSource();
    bool setTemperatureSource(BNO055_TEMP_SOURCE_TYPE source);
    #pragma endregion RAW_DATA_READING

    #pragma region INSTRUMENT_CONFIGURATION
    bool setAccelerometerConfig(uint8_t config);
    bool setGyroscopeConfig(uint8_t config);
    bool setGyroscopeOperationMode(uint8_t mode);
    bool setMagnetometerConfig(uint8_t config);
    #pragma endregion INSTRUMENT_CONFIGURATION

    #pragma region CALIBRATION
    bno055_calib_stat_t getCalibrationStatus();
    #pragma endregion CALIBRATION

    #pragma region INTERRUPT_IMPL
    bool clearInterrupts();

    class Interrupt {
        public: 
        bool enable();
        bool disable();
        bool mask();
        bool unmask();
        Interrupt(IMU *imu, int enable_bit, int mask_bit, EnabledAxes axes, void (*callback)(), int pin0){
            this->ENABLE_BIT = enable_bit;
            this->MASK_BIT = mask_bit;
            this->sensor = imu;
            this->axes = axes;
            this->callback = callback;
            this->pin0 = pin0;
        };

        protected:
            int ENABLE_BIT;
            int MASK_BIT;
            IMU *sensor;
            EnabledAxes axes;

            void (*callback)();
            int pin0;

        private:
            bool editInterruptState(byte offset, int bit, bool enable);
    };
    #pragma endregion INTERRUPT_IMPL

    #pragma region INTERRUPT_OBJS
    class AccHighGInterrupt : public Interrupt {
        private:
            byte duration;
            byte threshold;
        public:
        AccHighGInterrupt(IMU *imu, EnabledAxes axesSetting, void (*callback)(), int pin0, byte duration, byte threshold) : Interrupt(imu, 5, 5, axesSetting, callback, pin0) {
            this->duration = duration;
            this->threshold = threshold;
        };

        bool setup(){
            this->sensor ->setPageID(1);
            bool result = sensor ->writeRegister(BNO055_ACC_HG_DURATION, this->duration) &&
                          sensor -> writeRegister(BNO055_ACC_HG_THRES, this->threshold) &&
                          sensor -> writeRegister(BNO055_ACC_INT_SETTINGS, this->axes.getEnabledAxes() << 5);
            sensor->setPageID(0);
            return result;
        }
    };

    class AccAnyMotionInterrupt : public Interrupt {
        private:
            byte threshold;
            byte duration;
        public:
        AccAnyMotionInterrupt(IMU *imu, EnabledAxes axesSetting, void (*callback)(), int pin0, byte threshold, byte duration) : Interrupt(imu, 6, 6, axesSetting, callback, pin0) {
            this->threshold = threshold;
            this->duration = duration;
        };

        bool setup(){
            if(duration > 4 || threshold < 0) duration = 0;
            this->sensor ->setPageID(1);
            bool result = sensor -> writeRegister(BNO055_ACC_AM_THRES, this ->threshold) && 
                          sensor -> writeRegister(BNO055_ACC_INT_SETTINGS, this->axes.getEnabledAxes() << 2 || duration);
            sensor->setPageID(0);
            return result; 
            
        }
    };

    class AccSlowMotionInterrupt : public Interrupt {
        private:
            byte threshold;
            byte duration;
            bool mode = false;
        public:
        enum MODE {
            SLOW_MOTION = 0,
            NO_MOTION = 1
        };

        AccSlowMotionInterrupt(IMU *imu, EnabledAxes axesSetting, void (*callback)(), int pin0, byte threshold, byte duration, MODE mode) : Interrupt(imu, 7, 7, axesSetting, callback, pin0) {
            this->threshold = threshold;
            this->duration = duration;
            this->mode = mode;
        };

        bool setup(){
            byte setting = 0b01111111 & (duration << 1 | mode);
            sensor->setPageID(1);
            bool result =
                    sensor->writeRegister(BNO055_ACC_NM_THRES, threshold) &&
                    sensor->writeRegister(BNO055_ACC_NM_SET, setting) &&
                    sensor->writeRegister(BNO055_ACC_INT_SETTINGS, this->axes.getEnabledAxes() << 2);
            sensor->setPageID(0);
            return result;
            
        };

    };

    class GyroHighRateInterrupt : public Interrupt, public GyroHRAxes {
        private:
            GyroHRAxes axesSetting{};
            bool filterEnabled;
        public:
        using GyroHRAxisSetting = GyroHRAxes::GyroHRAxisSetting;
        GyroHRAxes::GyroHRAxisSetting xAxisSetting;
        GyroHRAxes::GyroHRAxisSetting yAxisSetting;
        GyroHRAxes::GyroHRAxisSetting zAxisSetting;

        GyroHighRateInterrupt(IMU *imu, EnabledAxes axesSetting, void (*callback)(), int pin0, bool filterEnabled, GyroHRAxes gryoAxesSetting) : Interrupt(imu, 3, 3, axesSetting, callback, pin0) {
           this -> filterEnabled = filterEnabled;
           this -> axesSetting = gryoAxesSetting;
        };

        bool setup(){
            byte buffer[6];
            axesSetting.get(buffer);

            byte currentSetting = sensor->readRegister(BNO055_GYR_INT_SETTING);
            byte newSetting = (
                    (
                            (0b1 & filterEnabled) << 7) | (axes.getEnabledAxes() << 3) // construct new values
                            | (0b01000111 & currentSetting) // retain old setting
                    );

            sensor->setPageID(1); //prep for interrupt config
            bool result = sensor->writeMultipleRegisters(BNO055_GYR_HR_X_SET, buffer, 6, 6)&&
                          sensor->writeRegister(BNO055_GYR_INT_SETTING, newSetting);
            sensor->setPageID(0);
            return result;
        }

    };

    class GyroAnyMotionInterrupt : public Interrupt {
        private:
            bool filterEnabled;
            byte awakeThreshold;
            byte awakeDuration;
            byte slopeSample;
        public:
        GyroAnyMotionInterrupt(IMU *imu, EnabledAxes axesSetting, void (*callback)(), int pin0, bool filterEnabled, byte awakeThreshold, byte awakeDuration, byte slopeSample) : Interrupt(imu, 2, 2, axesSetting, callback, pin0) {
            this->filterEnabled = filterEnabled;
            this->awakeThreshold = awakeThreshold;
            this->awakeDuration = awakeDuration;
            this->slopeSample = slopeSample;
        };

        bool setup(){
            //build the new setting byte
            byte currentSetting = sensor->readRegister(BNO055_GYR_INT_SETTING);
            byte newSetting = ((0b1 & filterEnabled) << 6) | (axes.getEnabledAxes())
                    | (0b01000111 & currentSetting);
            sensor->setPageID(1); //prep for interrupt config
            bool result = sensor->writeRegister(BNO055_GYR_AM_THRES, awakeThreshold) &&
                          sensor->writeRegister(BNO055_GYR_AM_SET, (0b00001111 & (((0b11 & awakeDuration) << 2) | (0b11 & slopeSample)))) &&
                          sensor->writeRegister(BNO055_GYR_INT_SETTING, newSetting);
            sensor->setPageID(0);
            return result;
        }
    };
     #pragma endregion INTERRUPT_OBJS
    
    
};

#endif // IMU_H