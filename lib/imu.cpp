/*
 * imu.cpp
 * Author: Giorgio Bella
 * Created on: 2024-06-01
 * Sensor: BNO055
 * Driver: Adafruit_BNO055
 * Description:
 */

#include "imu.h"

#pragma region I2C_DEVICE_SETUP
bool IMU::begin()
{
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
#pragma end region I2C_DEVICE_SETUP

#pragma region CONFIGURATION_GETTERS_SETTERS

BNO055_POWER_MODE IMU::getPowerMode()
{
   return static_cast<BNO055_POWER_MODE>(readRegister(BNO055_PWR_MODE_REG));
}

bool IMU::setPowerMode(BNO055_POWER_MODE mode)
{
   /*
 * BNO055 Power Management
 * Reference: BNO055 datasheet, Section 3.2 "Power management",
 * including Sections 3.2.1, 3.2.2, and 3.2.3.
 *
 * The BNO055 has two separate power supply pins:
 *   VDD   : Main power supply for the internal sensors.
 *   VDDIO : Separate power supply for the microcontroller and digital interfaces.
 *
 * Power-up requirement:
 *   VDD must be powered on before, or at the same time as, VDDIO.
 *
 * The BNO055 performs a power-on reset (POR) at every power-up.
 * After reset, the device initializes the register map with default values
 * and starts in CONFIG mode.
 *
 * A reset can also be triggered by:
 *   - Driving the nRESET pin low for at least 20 ns.
 *   - Setting the RST_SYS bit in the SYS_TRIGGER register.
 *
 * Supported power modes:
 *   NORMAL_MODE  : All sensors required by the selected operation mode are ON.
 *                  The register map and MCU peripherals remain active.
 *
 *   LOW_POWER_MODE:
 *                  If no motion is detected for a configurable duration
 *                  default 5 seconds, the device enters low power mode.
 *                  In this mode, only the accelerometer remains active.
 *                  Motion detection wakes the device back to normal mode.
 *
 *   SUSPEND_MODE :
 *                  The system is paused. All sensors and the microcontroller
 *                  are put into sleep mode. Register values are not updated.
 *                  To exit suspend mode, write a new mode to the PWR_MODE register.
 *
 * PWR_MODE register values:
 *   Normal Mode    : xxxxxx00b
 *   Low Power Mode : xxxxxx01b
 *   Suspend Mode   : xxxxxx10b
 *
 * Notes:
 *   - Low power mode only supports no-motion and any-motion interrupts.
 *   - High-G and slow-motion interrupts are not available in low power mode.
 *   - Low power mode is not applicable when the accelerometer is not used.
 * 
 * Flow : 
 * SET OPMODE CONFIGMODE -> SET PWRMODE -> SET OPMODE to previous mode
 */
   BNO055_OPERATION_MODE currOpMode = getOperationMode();
   setOperationMode(CONFIGMODE); // Must be in config mode to change power mode

   if (mode > SUSPEND || mode < NORMAL)
      mode = NORMAL; // default to normal if invalid mode is given
   
   bool res = writeRegister(BNO055_PWR_MODE_REG, mode);
   setOperationMode(currOpMode); // Restore the original operation mode
   return res;
}

BNO055_OPERATION_MODE IMU::getOperationMode()
{
   return static_cast<BNO055_OPERATION_MODE>(readRegister(BNO055_OPR_MODE_REG));
}

bool IMU::setOperationMode(BNO055_OPERATION_MODE mode){
    /*
 * BNO055 Operation Modes
 * Reference: BNO055 datasheet, Section 3.3 "Operation Modes",
 * Table 3-3 "Operating modes overview".
 *
 * The BNO055 supports multiple operation modes. Each mode enables only the
 * sensors required for that mode, while unused sensors are placed in suspend
 * mode to reduce power consumption.
 *
 * After power-on, the default operation mode is CONFIGMODE.
 *
 * Non-fusion modes:
 *   CONFIGMODE  : Configuration mode; no sensor output available.
 *   ACCONLY     : Accelerometer only.
 *   MAGONLY     : Magnetometer only.
 *   GYROONLY    : Gyroscope only.
 *   ACCMAG      : Accelerometer + magnetometer.
 *   ACCGYRO     : Accelerometer + gyroscope.
 *   MAGGYRO     : Magnetometer + gyroscope.
 *   AMG         : Accelerometer + magnetometer + gyroscope.
 *
 * Fusion modes:
 *   IMU         : Accelerometer + gyroscope; provides relative orientation.
 *   COMPASS     : Accelerometer + magnetometer; provides absolute orientation.
 *   M4G         : Magnetometer-assisted gyroscope mode.
 *   NDOF_FMC_OFF: 9-axis fusion without fast magnetometer calibration;
 *                 provides absolute orientation.
 *   NDOF        : Full 9-axis fusion mode;
 *                 provides absolute orientation.
 *
 * To change operation modes safely, switch to CONFIGMODE first if required,
 * then write the desired mode value to the operation mode register.
 * 
 * Flow staggered write : 
 * 1. we write CONFIGMODE -> BNO055 goes to config mode
 * 2. wait 20ms for the mode change to take effect
 * 3. we write the opMode 
 */
   //short circuit 
   if(mode == getOperationMode()) return true;
   bool writeConfModeRes = writeRegister(BNO055_OPR_MODE_REG, CONFIGMODE);
   delay(20); // Wait for mode change to take effect

   if(mode == CONFIGMODE) return writeConfModeRes; // If we're just switching to config mode, we're done

   bool writeOpModeRes = writeRegister(BNO055_OPR_MODE_REG, mode);
   delay(8); // it takes 8ms to switch from CONFIGMODEù

   //update the state 
   if(writeOpModeRes) current_operation_mode = mode;

   return writeOpModeRes;
   
}

bool IMU::resetSystem(){
   /**
    * Flow write the RST_SYS flag to SYS_TRIGGER then wait the POR time 650ms per datasheet
    * 
   */
   bool writeRes = writeRegister(BNO055_SYS_TRIGGER, 0b00100000);
   delay(650); // Wait for reset to complete (datasheet says 650ms, but 400ms seems to be sufficient in testing)

   return performSelfTest() && writeRes; // Check if reset was successful by performing self test and checking if the write was successful
}

bool IMU::performSelfTest(){
   /*
    * Perform self test by reading the self test result register and checking if all tests passed
    * Refer to section 3.9 of the datasheet for more details on the self test and the meaning of each bit in the result register
    * 
    * Flow = Wirte 1 to the SYS_TRIGGER  then wait for the same time as powerup (datasheet don't specify time for selftest) 
    * then read the register (make sure to filter the reserved bits and ST_MCU as the is no selftest performed on the MCU)
    */

   bool writeRes = writeRegister(BNO055_SYS_TRIGGER, 0b1); // Trigger self test
   delay(400); // Wait for self test to complete (using power-up time as a reference)
   byte result = 0b00000111 & readRegister(BNO055_ST_RESULT); // Read self test result and filter reserved bits and ST_MCU bit
   return (result == 0b111) && writeRes; // Check if all tests passed (bits 0-2 should be set)
   
}

bool IMU::setPageID(uint8_t page_id){
   /*
   * Flow  validate page_id (only 0 and 1 are valid, default to 0 if invalid), then write the new page ID to the PAGE_ID register
   * Refer to section 4.2 register map
   */
   if (!(page_id == 0 || page_id == 1)) page_id = 0;
    return writeRegister(BNO055_PAGE_ID, page_id);
}

#pragma endregion CONFIGURATION_GETTERS_SETTERS

#pragma region DATA_READING
Vector<int16_t> IMU::getVector(byte offset){
   Vector<int16_t> result{};
   byte buffer[6];
   memset(buffer, 0, 6); // Clear the buffer before reading
   readMultipleRegisters(offset, buffer, 6, 6); // Read 6 bytes starting from the given offset

   for (size_t i = 0; i < 3; i++)
   {
      result.vec[i] = static_cast<int16_t>(buffer[2 * i] | (buffer[2 * i + 1] << 8)); // Combine LSB and MSB to form a signed 16-bit integer
   }
   return result;
}

Vector<int16_t> IMU::getVector(const byte *buffer, int start_index){
   Vector<int16_t> result{};
   for (size_t i = 0; i < 3; i++)
   {
      result.vec[i] = static_cast<int16_t>(buffer[start_index + 2 * i] | (buffer[start_index + 2 * i + 1] << 8)); // Combine LSB and MSB to form a signed 16-bit integer
   }
   return result;
}

Vector<double> IMU::getAccelerometer() {
   Vector<int16_t> rawData = getVector(BNO055_ACC_DATA_X_LSB);
   return rawData.divideByScalar(BNO055_ACCEL_CONVERSION_FACTOR);
}

Vector<double> IMU::getMagnetometer() {
   Vector<int16_t> rawData = getVector(BNO055_MAG_DATA_X_LSB);
   return rawData.divideByScalar(BNO055_MAG_CONVERSION_FACTOR);
}

Vector<double> IMU::getGyroscope() {
   Vector<int16_t> rawData = getVector(BNO055_GYR_DATA_X_LSB);
   return rawData.divideByScalar(BNO055_GYRO_CONVERSION_FACTOR);
}

Vector<double> IMU::getLinearAcceleration() {
   Vector<int16_t> rawData = getVector(BNO055_LIA_DATA_X_LSB);
   return rawData.divideByScalar(BNO055_ACCEL_CONVERSION_FACTOR);
}

Vector<double> IMU::getGravityVector() {
   Vector<int16_t> rawData = getVector(BNO055_GRV_DATA_X_LSB);
   return rawData.divideByScalar(BNO055_ACCEL_CONVERSION_FACTOR);
}

Vector<double> IMU::getEuler() {
   Vector<int16_t> rawData = getVector(BNO055_GRV_DATA_X_LSB);
   return rawData.divideByScalar(BNO055_GYRO_CONVERSION_FACTOR);
}

Quaternion IMU::getQuaternion(){
   Quaternion quatData{};
   byte quatBuffer[8];
   memset(quatBuffer, 0, 8); // Clear the buffer before reading
   readMultipleRegisters(BNO055_QUA_DATA_W_LSB, quatBuffer, 8, 8); // Read 8 bytes starting from the quaternion data register

   int16_t w = static_cast<int16_t>(quatBuffer[0] | (quatBuffer[1] << 8));
   int16_t x = static_cast<int16_t>(quatBuffer[2] | (quatBuffer[3] << 8));
   int16_t y = static_cast<int16_t>(quatBuffer[4] | (quatBuffer[5] << 8));
   int16_t z = static_cast<int16_t>(quatBuffer[6] | (quatBuffer[7] << 8));

   quatData.setOrientation(
      static_cast<double>(w),
      static_cast<double>(x),
      static_cast<double>(y),
      static_cast<double>(z)
   );
   return quatData;
}

Quaternion IMU::getQuaternion(const byte *buffer, int start_index){
   Quaternion quatData{};

   int16_t w = static_cast<int16_t>(buffer[start_index] | (buffer[start_index + 1] << 8));
   int16_t x = static_cast<int16_t>(buffer[start_index + 2] | (buffer[start_index + 3] << 8));
   int16_t y = static_cast<int16_t>(buffer[start_index + 4] | (buffer[start_index + 5] << 8));
   int16_t z = static_cast<int16_t>(buffer[start_index + 6] | (buffer[start_index + 7] << 8));

   quatData.setOrientation(
      static_cast<double>(w),
      static_cast<double>(x),
      static_cast<double>(y),
      static_cast<double>(z)
   );
   return quatData;
}

bno055_burst_t IMU::readAllData(){
   /*
 * BNO055 burst-read buffer dimensioning.
 *
 * Reference: BNO055 datasheet register map, Page 0.
 *
 * The main sensor output registers are contiguous:
 *
 *   0x08 - 0x0D : Accelerometer data, 6 bytes
 *   0x0E - 0x13 : Magnetometer data, 6 bytes
 *   0x14 - 0x19 : Gyroscope data, 6 bytes
 *   0x1A - 0x1F : Euler orientation data, 6 bytes
 *   0x20 - 0x27 : Quaternion data, 8 bytes
 *   0x28 - 0x2D : Linear acceleration data, 6 bytes
 *   0x2E - 0x33 : Gravity vector data, 6 bytes
 *
 * Total full output burst:
 *
 *   first register = 0x08
 *   last register  = 0x33
 *
 *   length = 0x33 - 0x08 + 1 = 44 bytes
 *
 * Do not dimension this from the number of decoded int16_t fields.
 * Dimension it from the number of raw bytes read from the BNO055.
 */
   bno055_burst_t data;
   byte buffer[44];
   switch (current_operation_mode)
   {
      case ACCONLY:
         data.accel = getAccelerometer();
         break;
      case MAGONLY:
         data.mag = getMagnetometer();
         break;
      case GYROONLY:
         data.gyro = getGyroscope();
         break;
      case ACCMAG:
         memset(buffer, 0, 12); // Clear the buffer before reading
         readMultipleRegisters(BNO055_ACC_DATA_X_LSB, buffer, 12, 12); // Read 12 bytes starting from the accelerometer data register
         data.accel = getVector(buffer, 0).divideByScalar(BNO055_ACCEL_CONVERSION_FACTOR); // Decode accelerometer data from the
         data.mag = getVector(buffer, 6).divideByScalar(BNO055_MAG_CONVERSION_FACTOR); // Decode magnetometer data from the buffer
         break;
      case ACCGYRO:
         memset(buffer, 0, 18); // Clear the buffer before reading
         readMultipleRegisters(BNO055_ACC_DATA_X_LSB, buffer, 18, 18); // Read 18 bytes starting from the accelerometer data register
         data.accel = getVector(buffer, 0).divideByScalar(BNO055_ACCEL_CONVERSION_FACTOR); // Decode accelerometer data from the buffer
         data.gyro = getVector(buffer, 12).divideByScalar(BNO055_GYRO_CONVERSION_FACTOR); // Decode gyroscope data from the buffer
      case MAGGYRO:
         memset(buffer, 0, 12); // Clear the buffer before reading
         readMultipleRegisters(BNO055_MAG_DATA_X_LSB, buffer, 12, 12); // Read 12 bytes starting from the magnetometer data register
         data.mag = getVector(buffer, 6).divideByScalar(BNO055_MAG_CONVERSION_FACTOR); // Decode magnetometer data from the buffer
         data.gyro = getVector(buffer, 12).divideByScalar(BNO055_GYRO_CONVERSION_FACTOR); // Decode gyroscope data from the buffer
      case AMG:
         memset(buffer, 0, 18); // Clear the buffer before reading
         readMultipleRegisters(BNO055_ACC_DATA_X_LSB, buffer, 18, 18); // Read 18 bytes starting from the accelerometer data register
         data.accel = getVector(buffer, 0).divideByScalar(BNO055_ACCEL_CONVERSION_FACTOR); // Decode accelerometer data from the buffer
         data.mag = getVector(buffer, 6).divideByScalar(BNO055_MAG_CONVERSION_FACTOR); // Decode magnetometer data from the buffer
         data.gyro = getVector(buffer, 12).divideByScalar(BNO055_GYRO_CONVERSION_FACTOR); // Decode gyroscope data from the buffer
      case IMU_MODE: case COMPASS: case M4G: case NDOF_FMC_OFF: case NDOF:
         memset(buffer, 0, 44); // Clear the buffer before reading
         readMultipleRegisters(BNO055_ACC_DATA_X_LSB, buffer, 44, 44); // Read 44 bytes starting from the accelerometer data register
         data.accel = getVector(buffer, 0).divideByScalar(BNO055_ACCEL_CONVERSION_FACTOR); // Decode accelerometer data from the buffer
         data.mag = getVector(buffer, 6).divideByScalar(BNO055_MAG_CONVERSION_FACTOR); // Decode magnetometer data from the buffer
         data.gyro = getVector(buffer, 12).divideByScalar(BNO055_GYRO_CONVERSION_FACTOR); // Decode gyroscope data from the buffer
         data.euler = getVector(buffer, 18).divideByScalar(BNO055_GYRO_CONVERSION_FACTOR); // Decode Euler orientation data from the buffer (LSB = 16 LSB/°)
         data.quaternion = getQuaternion(buffer, 24); // Decode quaternion orientation data from the buffer (LSB = 16384 LSB/unit)
         data.linearAccel = getVector(buffer, 32).divideByScalar(BNO055_ACCEL_CONVERSION_FACTOR); // Decode linear acceleration data from the buffer
         data.gravityVector = getVector(buffer, 36).divideByScalar(BNO055_ACCEL_CONVERSION_FACTOR); // Decode gravity vector data from the buffer
         break;
   default:
      break;
   }
   return data;
}
#pragma endregion DATA_READING

#pragma region TEMP
byte IMU::getTemperature() {
   return readRegister(BNO055_TEMP_REG);
}

BNO055_TEMP_SOURCE_TYPE IMU::getTemperatureSource() {
   return static_cast<BNO055_TEMP_SOURCE_TYPE>(readRegister(BNO055_TEMP_SOURCE));
}

bool IMU::setTemperatureSource(BNO055_TEMP_SOURCE_TYPE source) {
   return writeRegister(BNO055_TEMP_SOURCE, source);
}
#pragma endregion TEMP

#pragma region CONFIGURATION
/*
* All configutation have the same flow but switch statment are slow so we diveded in 3 functions.
* Flow : change PageID 1 then write the config register then set PageID to 0 
* Success: is defined by the success of every single op
*/

bool IMU::setAccelerometerConfig(byte config) {
   bool pageRes = setPageID(1);
   bool writeRes = writeRegister(BNO055_ACC_CONFIG, config);
   bool pageResetRes = setPageID(0);
   return pageRes && writeRes && pageResetRes;
}

bool IMU::setGyroscopeConfig(byte config) {
   bool pageRes = setPageID(1);
   bool writeRes = writeRegister(BNO055_GYR_CONFIG_0, config);
   bool pageResetRes = setPageID(0);
   return pageRes && writeRes && pageResetRes;
}

bool IMU::setGyroscopeOperationMode(byte opMode){
   bool pageRes = setPageID(1);
   bool writeRes = writeRegister(BNO055_GYR_CONFIG_1, opMode);
   bool pageResetRes = setPageID(0);
   return pageRes && writeRes && pageResetRes;
}

bool IMU::setMagnetometerConfig(byte config) {
   bool pageRes = setPageID(1);
   bool writeRes = writeRegister(BNO055_MAG_CONFIG, config);
   bool pageResetRes = setPageID(0);
   return pageRes && writeRes && pageResetRes;
}

#pragma endregion CONFIGURATION

#pragma region CALIBRATION
/*
 * BNO055 Calibration
 * Reference: BNO055 datasheet, Section 3.10 "Calibration".
 *
 * The BNO055 has an internal calibration system that can be used to improve
 * the accuracy of sensor readings. The calibration status can be read from
 * the CALIB_STAT register, which indicates the calibration level of each
 * sensor (accelerometer, magnetometer, gyroscope) and the overall system.
 *
 * Calibration levels:
 *   0 : Uncalibrated
 *   3 : Fully calibrated
 *
*/

bno055_calib_stat_t IMU::getCalibrationStatus() {
   byte result = readRegister(BNO055_CALIB_STAT);

   bno055_calib_stat_t status;
   status.mag = result & 0b00000011;
   status.accel = (result >> 2) & 0b11;
   status.gyro = (result >> 4) & 0b11;
   status.sys = (result >> 6) & 0b11;
    
   return status;
}

#pragma endregion CALIBRATION

#pragma region INTERRUPTS

bool IMU::clearInterrupts() {
   return writeRegister(BNO055_SYS_TRIGGER, 0b01000000);
}

bool IMU::Interrupt::enable(){
    return editInterruptState(BNO055_INT_EN, ENABLE_BIT, true);
}

bool IMU::Interrupt::disable(){
    return editInterruptState(BNO055_INT_EN, ENABLE_BIT, false);
}

bool IMU::Interrupt::mask(){
   pinMode(pin0, INPUT_PULLDOWN); // Set the pin as input to avoid driving it low when masking the interrupt
   attachInterrupt(digitalPinToInterrupt(pin0), callback, RISING); // Attach the interrupt to the pin with the provided callback
   return editInterruptState(BNO055_INT_MSK, MASK_BIT, true);

}

bool IMU::Interrupt::unmask(){
   detachInterrupt(digitalPinToInterrupt(pin0)); // Detach the interrupt from the pin to allow it to drive the pin low when unmasking the interrupt
   return editInterruptState(BNO055_INT_MSK, MASK_BIT, false);
}

byte EnabledAxes::getEnabledAxes(){
    byte result = 0b000;
    bitWrite(result, 2, z);
    bitWrite(result, 1, y);
    bitWrite(result, 0, x);
    return result;
}

#pragma endregion INTERRUPTS




