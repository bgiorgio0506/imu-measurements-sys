/*
 * imu.cpp
 * Author: Giorgio Bella
 * Created on: 2024-06-01
 * Sensor: BNO055
 * Driver: Adafruit_BNO055
 * Description: 
 */

 #include "imu.h"

 bool IMU::begin() {
    delay(400); // Wait for sensor to boot up
    /*
    * Power up self test. During start up the device perform a self test to check if every instrument (Accelerometer, Gyros, Mag) are connected. 
    * The following tests are executed
    *
    * | Components |                                     | Test type |
    * =========================================================================
    * Accelerometer                                    Verify chip ID
    * Magnetometer                                     Verify chip ID
    * Gyroscope                                        Verify chip ID
    * Microcontroller                                  Memory Build In Self Test
    *
    * The results of the POST are stored at register ST_RESULT, where a bit set indicates test
    * passed and cleared indicates self test failed.
    *
    * Refer to section 3.9 of the datasheet for more details
    */
    return readRegister(BNO055_ST_RESULT) == 0b1111;
 }