#include <Arduino.h>
#include "imu.h"

const int BNO055_I2C_ADDRESS = 0x28; // Default I2C address for BNO055
IMU imu(BNO055_I2C_ADDRESS, &Wire);

void setup() {
  Wire.begin(16, 17);  // SDA=GPIO16, SCL=GPIO17
  imu.begin();
  imu.setOperationMode(IMU_MODE);
  // put your setup code here, to run once:
}


//Idea need to build a testing procedure for the mission that the IMU will be used 
//I need a mission clock, I need a general clock to mark start end of mission
//I need repeatable test procedure so maybe use the interrupt functions of the sensor to trigger data collection at specific times or events
void loop() {
  // put your main code here, to run repeatedly:
  Quaternion quat = imu.getQuaternion();
  Quaternion::EulerAngles angles = quat.getEulerAngles();
  Vector<double> acc = imu.getAccelerometer();

  //Serial with Bluetooth
  Serial.print("Roll: ");
  Serial.print(angles.roll);
  Serial.print(" Pitch: ");
  Serial.print(angles.pitch);
  Serial.print(" Yaw: ");
  Serial.print(angles.yaw);
  Serial.print(" AccX: ");
  Serial.print(acc.getX());
  Serial.print(" AccY: ");
  Serial.print(acc.getY());
  Serial.print(" AccZ: ");
  Serial.println(acc.getZ());

}