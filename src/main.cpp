#include <Arduino.h>
#include <ArduinoJson.h>
#include <BluetoothSerial.h>
#include "telemetry_bridge.h"
#include "imu.h"

const int BNO055_I2C_ADDRESS = 0x28; // Default I2C address for BNO055
const String cmdPrefix = "imu";
int mission_status = 0;
IMU imu(BNO055_I2C_ADDRESS, &Wire);
TelemetryBridge bridge;
TelemetryConfig config;
BluetoothSerial SerialBT;

struct MissionConfig
{
  // telemetry settings
  const int TELEMETRY_FREQUENCY_MS = 1000; // how often to publish telemetry data
  // test procedure settings
  const int TEST_DURATION_MS = 60000; // how long each test should run
};

enum MISSION_STATUS
{
  STANDING_BY = 0,
  STOPPED = 1,
  STARTED = 2
};

MissionConfig missionConf;
void onCommand(const String &topic, const String &payload)
{
  bridge.publishStatus("Received command: " + payload);
  if (payload == cmdPrefix + "" + "starttest")
  {
    bridge.publishStatus("Starting test...");
    mission_status = STARTED;
  }
  else if (payload == cmdPrefix + "" + "stoptest")
  {
    bridge.publishStatus("Stopping test...");
    mission_status = STOPPED;
  }
  else if (payload == cmdPrefix + "" + "hi")
  {
    bridge.publishStatus("hello");
  }

  if (mission_status == STOPPED)
  {
    if (payload == cmdPrefix + "" + "powermode normal")
    {
      imu.setPowerMode(NORMAL);
      bridge.publishStatus("Set power mode to NORMAL");
    }
    else if (payload == cmdPrefix + "" + "powermode low")
    {
      imu.setPowerMode(LOW_POWER);
      bridge.publishStatus("Set power mode to LOW POWER");
    }
    else if (payload == cmdPrefix + "" + "powermode suspend")
    {
      imu.setPowerMode(SUSPEND);
      bridge.publishStatus("Set power mode to SUSPEND");
    }
    else if (payload == cmdPrefix + "" + "reset")
    {
      imu.resetSystem();
      bridge.publishStatus("Sensor reset");
    }
    else if (payload == cmdPrefix + "" + "selftest")
    {
      bool result = imu.performSelfTest();
      bridge.publishStatus(String("Self-test result: ") + (result ? "PASS" : "FAIL"));
    }
    else if (payload == cmdPrefix + "" + "operationmode config")
    {
      imu.setOperationMode(CONFIGMODE);
      bridge.publishStatus("Set operation mode to CONFIGMODE");
    }
    else if (payload == cmdPrefix + "" + "operationmode imu")
    {
      imu.setOperationMode(IMU_MODE);
      bridge.publishStatus("Set operation mode to IMU_MODE");
    }
  }
}

void setupBridge()
{
  SerialBT.begin("ESP32_IMU_DEBUG_LOG"); // Bluetooth device name
  config.wifiSsid = "YOUR_WIFI_NAME";
  config.wifiPassword = "YOUR_WIFI_PASSWORD";
  config.mqttHost = "YOUR_MQTT_HOST";
  config.mqttPort = 1883; // standard MQTT port
  config.deviceId = "YOUR_DEVICE_ID";
  config.baseTopic = "YOUR_BASE_TOPIC";
  config.logWithTimestamp = true;
  config.mirrorPublishedMessagesToLog = true;
  config.mirrorReceivedCommandsToLog = true;

  bridge.setLogStream(SerialBT, true);
  bridge.setCommandCallback(onCommand);
}


StaticJsonDocument<512> serializeDataToJson(bno055_burst_t &data, char *buffer, size_t bufferSize)
{
  StaticJsonDocument<512> doc;
  unsigned long timestamp = millis();
  doc["timestamp"] = timestamp;
  doc["senor"] = "bno055";

  JsonObject accel = doc.createNestedObject("accel");
  accel["x"] = data.accel.getX();
  accel["y"] = data.accel.getY();
  accel["z"] = data.accel.getZ();
  JsonObject mag = doc.createNestedObject("mag");
  mag["x"] = data.mag.getX();
  mag["y"] = data.mag.getY();
  mag["z"] = data.mag.getZ();
  JsonObject gyro = doc.createNestedObject("gyro");
  gyro["x"] = data.gyro.getX();
  gyro["y"] = data.gyro.getY();
  gyro["z"] = data.gyro.getZ();
  JsonObject linearAccel = doc.createNestedObject("linearAccel");
  linearAccel["x"] = data.linearAccel.getX();
  linearAccel["y"] = data.linearAccel.getY();
  linearAccel["z"] = data.linearAccel.getZ();
  JsonObject gravityVector = doc.createNestedObject("gravityVector");
  gravityVector["x"] = data.gravityVector.getX();
  gravityVector["y"] = data.gravityVector.getY();
  gravityVector["z"] = data.gravityVector.getZ();
  JsonObject quaternion = doc.createNestedObject("quaternion");
  quaternion["w"] = data.quaternion.getW();
  quaternion["x"] = data.quaternion.getX();
  quaternion["y"] = data.quaternion.getY();
  quaternion["z"] = data.quaternion.getZ();
  JsonObject euler = doc.createNestedObject("euler");
  euler["roll"] = data.euler.getX();
  euler["pitch"] = data.euler.getY();
  euler["yaw"] = data.euler.getZ();

  serializeJson(doc, buffer, bufferSize);
  return doc;
}

void setup()
{
  Wire.begin(16, 17); // SDA=GPIO16, SCL=GPIO17 // se non funziona inverti
  imu.begin();
  imu.setOperationMode(IMU_MODE);
  setupBridge();
  bridge.begin(config);
}

// Idea need to build a testing procedure for the mission that the IMU will be used
// I need a mission clock, I need a general clock to mark start end of mission
// I need repeatable test procedure so maybe use the interrupt functions of the sensor to trigger data collection at specific times or events
void loop()
{
  bridge.loop();
  unsigned long now = millis();
  unsigned long lastTelemetry = 0;
  if (mission_status == STARTED && now - lastTelemetry >= missionConf.TELEMETRY_FREQUENCY_MS)
  { // Every 5 seconds, with a 50ms window to account for loop timing
    bno055_burst_t data = imu.readAllData();
    // need to serilize the data into json format, maybe use ArduinoJson library for that
    char buffer[512];
    StaticJsonDocument<512> doc = serializeDataToJson(data, buffer, sizeof(buffer));
    bridge.publishTelemetry(doc.as<String>());
    lastTelemetry = now;
  }
}
