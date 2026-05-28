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

unsigned long lastTelemetry = 0;
unsigned long lastStatusUpdate = 0;
unsigned long missionStartTime = 0;

struct MissionConfig
{
  // telemetry settings
  const int TELEMETRY_FREQUENCY_MS = 100; // how often to publish telemetry data
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
  if (payload == cmdPrefix + ":" + "starttest")
  {
    bridge.publishStatus("Starting test...");
    mission_status = STARTED;
    missionStartTime = millis();
  }
  else if (payload == cmdPrefix + ":" + "stoptest")
  {
    bridge.publishStatus("Stopping test...");
    mission_status = STOPPED;
  }
  else if (payload == cmdPrefix + ":" + "hi")
  {
    bridge.publishStatus("hello");
  }


  if (mission_status == STOPPED)
  {
    if (payload == cmdPrefix + ":" + "powermode normal")
    {
      imu.setPowerMode(NORMAL);
      bridge.publishStatus("Set power mode to NORMAL");
    }
    else if (payload == cmdPrefix + ":" + "powermode low")
    {
      imu.setPowerMode(LOW_POWER);
      bridge.publishStatus("Set power mode to LOW POWER");
    }
    else if (payload == cmdPrefix + ":" + "powermode suspend")
    {
      imu.setPowerMode(SUSPEND);
      bridge.publishStatus("Set power mode to SUSPEND");
    }
    else if (payload == cmdPrefix + ":" + "reset")
    {
      imu.resetSystem();
      bridge.publishStatus("Sensor reset");
    }
    else if (payload == cmdPrefix + ":" + "selftest")
    {
      bool result = imu.performSelfTest();
      bridge.publishStatus(String("Self-test result: ") + (result ? "PASS" : "FAIL"));
    }
    else if (payload == cmdPrefix + ":" + "operationmode config")
    {
      imu.setOperationMode(CONFIGMODE);
      bridge.publishStatus("Set operation mode to CONFIGMODE");
    }
    else if (payload == cmdPrefix + ":" + "operationmode imu")
    {
      imu.setOperationMode(IMU_MODE);
      bridge.publishStatus("Set operation mode to IMU_MODE");
    } 
  }
}

void setupBridge()
{
  SerialBT.begin("ESP32_IMU_DEBUG_LOG"); // Bluetooth device name
  config.wifiSsid = "TELEMETRY";
  config.wifiPassword = "PASSWORD";
  config.mqttHost = "10.79.43.181";
  config.mqttPort = 1883; // standard MQTT port
  config.deviceId = "esp32";
  config.baseTopic = "imu";
  config.logWithTimestamp = true;
  config.mirrorPublishedMessagesToLog = true;
  config.mirrorReceivedCommandsToLog = true;

  bridge.setLogStream(SerialBT, true);
  bridge.setCommandCallback(onCommand);
  bridge.setLogCallback([](const String& message) {
  //timestamp for log message
  unsigned long timestamp = millis();
  String logMessage = "[LOG at:" + String(timestamp) + " ms] " + message;
  Serial.println(logMessage);                // USB Serial Monitor output
});
}


StaticJsonDocument<512> serializeDataToJson(bno055_burst_t &data, char *buffer, size_t bufferSize)
{
  StaticJsonDocument<512> doc;
  unsigned long timestamp = millis();
  doc["timestamp"] = timestamp;
  doc["sensor"] = "bno055";

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


void scanI2C()
{
  Serial.println("Scanning I2C bus...");

  byte found = 0;

  for (byte address = 1; address < 127; address++)
  {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.print("I2C device found at 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      found++;
    }
  }

  if (found == 0)
  {
    Serial.println("No I2C devices found");
  }
}

void setup()
{
  Serial.begin(115200);
  setupBridge();
  bridge.begin(config);

  Wire.begin(16, 17); // SDA=GPIO16, SCL=GPIO17 config corretta
  delay(500);

  scanI2C(); 

  imu.begin();
  imu.setOperationMode(IMU_MODE);
  // Print setup info
  Serial.println("\n========= SETUP INFO =========\n");
  Serial.println("WiFi IP: " + WiFi.localIP().toString());
  Serial.println("MQTT Host: " + String(config.mqttHost) + ":" + String(config.mqttPort));
  Serial.println("Device ID: " + String(config.deviceId));
  Serial.println("Base Topic: " + String(config.baseTopic));
  Serial.println("Telemetry Frequency: " + String(missionConf.TELEMETRY_FREQUENCY_MS) + " ms");
  Serial.println("Power Mode: NORMAL");
  Serial.println("Operation Mode: IMU_MODE");
  Serial.println("\n=============================\n");
}

// Idea need to build a testing procedure for the mission that the IMU will be used
// I need a mission clock, I need a general clock to mark start end of mission
// I need repeatable test procedure so maybe use the interrupt functions of the sensor to trigger data collection at specific times or events
void loop()
{
  bridge.loop();
  unsigned long now = millis();
  if (mission_status == STARTED && now - lastTelemetry >= missionConf.TELEMETRY_FREQUENCY_MS)
  { // Every 5 seconds, with a 50ms window to account for loop timing
    bno055_burst_t data = imu.readAllData();

    // need to serilize the data into json format, maybe use ArduinoJson library for that
    char buffer[512];
    serializeDataToJson(data, buffer, sizeof(buffer));
    Serial.println(buffer); // Print JSON to Serial Monitor for debugging
    bridge.publishTelemetry(buffer);
    lastTelemetry = now;
  }

  //Output mission status every 10 seconds
   if (now - lastStatusUpdate >= 10000) {
    // Every 10 seconds, with a 50ms window
    String statusMsg = "Mission status: ";
    if (mission_status == STANDING_BY) {
      statusMsg += "STANDING BY";
    } else if (mission_status == STARTED) {
      statusMsg += "STARTED";
    } else if (mission_status == STOPPED) {
      statusMsg += "STOPPED";
    }
    bridge.publishStatus(statusMsg);
    lastStatusUpdate = now;
  }

  if(mission_status == STARTED && now - missionStartTime >= missionConf.TEST_DURATION_MS) {
    bridge.publishStatus("Test duration reached, stopping test...");
    mission_status = STOPPED;
  }
}
