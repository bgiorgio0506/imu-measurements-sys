#include <Arduino.h>
#include <ArduinoJson.h>
#include <BluetoothSerial.h>
#include "telemetry_bridge.h"
#include "imu.h"
#include "parser.h"

const int BNO055_I2C_ADDRESS = 0x28; // Default I2C address for BNO055
const String cmdPrefix = "imu";
int mission_status = 0;
double lastTheta = 0.0; // variable to store the last theta value for test procedure
IMU imu(BNO055_I2C_ADDRESS, &Wire);
TelemetryBridge bridge;
TelemetryConfig config;
BluetoothSerial SerialBT;

unsigned long lastTelemetry = 0;
unsigned long lastStatusUpdate = 0;
unsigned long missionStartTime = 0;
enum MISSION_STATUS
{
  STANDING_BY = 0,
  STOPPED = 1,
  STARTED = 2
};

enum EXCITATION_AXIS
{
  X_AXIS = 0,
  Y_AXIS = 1,
  Z_AXIS = 2
};

struct MissionConfig
{
  // telemetry settings
  int TELEMETRY_FREQUENCY_MS = 100; // how often to publish telemetry data
  int TEST_WAIT_BEFORE_START_MS = 30000; // how long to wait after receiving the start command before starting to publish telemetry data, this is to allow the system to stabilize after excitation starts
  // test procedure settings
  int TEST_DURATION_OVERRIDE_MS = 60000; // how long each test should run if the tolerance is not met during the test.
  double TEST_TOLERANCE_DPS = 0.03;         // this indicate difference of theta(t+dt) - theta(t). Once this difference is less or equal to the tolerance value the test is considered to be completed status STOPPED rad/s.
  EXCITATION_AXIS exciteAxis = X_AXIS;   // this indicate the axis to excite during the test procedure, default is X axis
};

MissionConfig missionConf;
#pragma region CommandParsing

parsed_command_t parseCommand(const String &payload)
{
  // Command format: imu:command [args] es "imu:setpwmode --mode=normal" or "imu:starttest --duration=60000 --tolerance=0.03 --exciteaxis=0"
  parsed_command_t cmd;
  cmd_parse_status_t status = CommandParser::parse(payload.c_str(), &cmd);
  if (status != CMD_PARSE_OK)
  {
    bridge.publishStatus("Failed to parse command: " + payload + " Error code: " + String(status));
    return parsed_command_t();
  }
  bridge.publishStatus("Parsed command: " + String(cmd.command) + " with " + String(cmd.arg_count) + " args");
  for (int i = 0; i < cmd.arg_count; i++)
  {
    cmd_arg_t arg = cmd.args[i];
    String argTypeStr;
    switch (arg.type)
    {
    case CMD_ARG_FLAG:
      argTypeStr = "FLAG";
      break;
    case CMD_ARG_kEY_VALUE:
      argTypeStr = "KEY_VALUE";
      break;
    case CMD_ARG_POSITIONAL:
      argTypeStr = "POSITIONAL";
      break;
    }
    bridge.publishStatus("Arg " + String(i) + ": type=" + argTypeStr + " key=" + String(arg.key) + " value=" + String(arg.value));
  }
  return cmd;
}

void onCommand(const String &topic, const String &payload)
{
  bridge.publishStatus("Received command: " + payload);

  // Need to parse the command especially fro test proc commands
  parsed_command_t cmd = parseCommand(payload);
  bridge.publishStatus("Command to execute: " + String(cmd.command));
  if (String(cmd.command) == "starttest")
  {
    missionConf.TEST_DURATION_OVERRIDE_MS = 60000; // default value
    missionConf.TEST_WAIT_BEFORE_START_MS = 30000; // default value
    missionConf.TEST_TOLERANCE_DPS = 0.03;         // default value
    missionConf.exciteAxis = X_AXIS;               // default value
    for (int i = 0; i < cmd.arg_count; i++)
    {
      cmd_arg_t arg = cmd.args[i];
      if (arg.type == CMD_ARG_kEY_VALUE)
      {
        if (String(arg.key) == "duration")
        {
          missionConf.TEST_DURATION_OVERRIDE_MS = atoi(arg.value);
        }
        else if (String(arg.key) == "tolerance")
        {
          missionConf.TEST_TOLERANCE_DPS = atof(arg.value);
        }
        else if (String(arg.key) == "exciteaxis")
        {
          int axis = atoi(arg.value);
          if (axis >= 0 && axis <= 2)
          {
            missionConf.exciteAxis = static_cast<EXCITATION_AXIS>(axis);
          }
        } else if (String(arg.key) == "buffer_time"){
          missionConf.TEST_WAIT_BEFORE_START_MS = atoi(arg.value);
        }
      }
    }
    bridge.publishStatus("Starting test with duration=" + String(missionConf.TEST_DURATION_OVERRIDE_MS) + "ms, tolerance=" + String(missionConf.TEST_TOLERANCE_DPS) + "dps, exciteAxis=" + String(missionConf.exciteAxis) + "WAITING " + String(missionConf.TEST_WAIT_BEFORE_START_MS) + "ms before starting to publish telemetry data");
    delay(missionConf.TEST_WAIT_BEFORE_START_MS); // wait before starting the test to allow the system to stabilize after excitation starts
    mission_status = STARTED;
    missionStartTime = millis();
    bridge.publishStatus("STARTED");
    bridge.publishEvent("test_started", "duration=" + String(missionConf.TEST_DURATION_OVERRIDE_MS) + ";tolerance=" + String(missionConf.TEST_TOLERANCE_DPS) + ";exciteAxis=" + String(missionConf.exciteAxis));
  }
  else if (String(cmd.command) == "stoptest")
  {
    mission_status = STOPPED;
    bridge.publishStatus("STOPPED");
  }
  else if (String(cmd.command) == "hi")
  {
    bridge.publishStatus("Hello from ESP32!");
  }
  else if (String(cmd.command) == "setmissionstatus")
  {
    for (int i = 0; i < cmd.arg_count; i++)
    {
      cmd_arg_t arg = cmd.args[i];
      if (arg.type == CMD_ARG_kEY_VALUE)
      {
        if (String(arg.key) == "status")
        {
          String status = String(arg.value);
          if (status == "standing_by")
          {
            mission_status = STANDING_BY;
            bridge.publishStatus("Setting mission status to STANDING_BY");
          }
          else if (status == "stopped")
          {
            mission_status = STOPPED;
            bridge.publishEvent("test_stopped_manually", "reason=command");
            bridge.publishStatus("Setting mission status to STOPPED");
          }
          else if (status == "started")
          {
            bridge.publishEvent("test_started_manually", "reason=command");
            bridge.publishStatus("Setting mission status to STARTED");
            mission_status = STARTED;
          }
          else
          {
            bridge.publishStatus("Unknown mission status: " + status);
          }
        }
      }
    }
  }
  else if (String(cmd.command) == "chipinfo")
  {
    // publish some info about the chip
    bridge.publishStatus("Chip model: " + String(ESP.getChipModel()));
    bridge.publishStatus("Chip revision: " + String(ESP.getChipRevision()));
    bridge.publishStatus("CPU frequency: " + String(ESP.getCpuFreqMHz()) + " MHz");
    bridge.publishStatus("Free heap: " + String(ESP.getFreeHeap()) + " bytes");

    for (int i = 0; i < cmd.arg_count; i++)
    {
      cmd_arg_t arg = cmd.args[i];
      if (arg.type == CMD_ARG_FLAG)
      {
        if (String(arg.key) == "debug")
        {
          // need current page, mode, chipid,calib
          bridge.publishStatus("Debug info:");
          bridge.publishStatus("Current operation mode: " + String(imu.getOperationMode()));
          bridge.publishStatus("Current power mode: " + String(imu.getPowerMode()));
          bridge.publishStatus("Current page ID: " + String(imu.getPageID()));
          bridge.publishStatus("Chip ID: " + String(imu.readRegister(BNO055_CHIP_ID)));
          bno055_calib_stat_t calib = imu.getCalibrationStatus();
          bridge.publishStatus("Calibration status - accel: " + String(calib.accel) + " gyro: " + String(calib.gyro) + " mag: " + String(calib.mag) + " sys: " + String(calib.sys));
        }
      }
    }
  }

  if (mission_status == STOPPED || mission_status == STANDING_BY)
  {
    // CONF COMMANDS
    if (String(cmd.command) == "setpwmode")
    {
      for (int i = 0; i < cmd.arg_count; i++)
      {
        cmd_arg_t arg = cmd.args[i];
        if (arg.type == CMD_ARG_kEY_VALUE)
        {
          if (String(arg.key) == "mode")
          {
            String mode = String(arg.value);
            if (mode == "normal")
            {
              // set normal power mode
              bridge.publishStatus("Setting power mode to NORMAL");
              imu.setPowerMode(NORMAL);
              bridge.publishEvent("power_mode_changed", "mode=NORMAL");
            }
            else if (mode == "low")
            {
              // set low power mode
              bridge.publishStatus("Setting power mode to LOW");
              imu.setPowerMode(LOW_POWER);
              bridge.publishEvent("power_mode_changed", "mode=LOW_POWER");
            }
            else if (mode == "suspend")
            {
              // set suspend power mode
              bridge.publishStatus("Setting power mode to SUSPEND");
              imu.setPowerMode(SUSPEND);
              bridge.publishEvent("power_mode_changed", "mode=SUSPEND");
            }
            else
            {
              bridge.publishStatus("Unknown power mode: " + mode);
            }
          }
        }
      }
    }
    else if (String(cmd.command) == "setopmode")
    {
      for (int i = 0; i < cmd.arg_count; i++)
      {
        cmd_arg_t arg = cmd.args[i];
        if (arg.type == CMD_ARG_kEY_VALUE)
        {
          if (String(arg.key) == "mode")
          {
            String mode = String(arg.value);
            if (mode == "config")
            {
              // set config operation mode
              bridge.publishStatus("Setting operation mode to CONFIG");
              imu.setOperationMode(CONFIGMODE);
              bridge.publishEvent("operation_mode_changed", "mode=CONFIG");
            }
            else if (mode == "imu")
            {
              // set imu operation mode
              bridge.publishStatus("Setting operation mode to IMU");
              imu.setOperationMode(IMU_MODE);
              bridge.publishEvent("operation_mode_changed", "mode=IMU");
            }
            else if (mode == "ndof")
            {
              // set ndof operation mode
              bridge.publishStatus("Setting operation mode to NDOF");
              imu.setOperationMode(NDOF);
              bridge.publishEvent("operation_mode_changed", "mode=NDOF");
            }
            else
            {
              bridge.publishStatus("Unknown operation mode: " + mode);
            }
          }
        }
      }
    }
    else if (String(cmd.command) == "calibrate")
    {
      // get calibration status
      bridge.publishStatus("Getting calibration status");
      BNO055_OPERATION_MODE currentMode = imu.getOperationMode();
      imu.setOperationMode(NDOF); // need to be in config mode to read calibration status
      imu.setPageID(0);           // need to be in page 0 to read calibration status
      bno055_calib_stat_t calib = imu.getCalibrationStatus();
      delay(100); // small delay to ensure imu is ready after mode change
      bridge.publishStatus("Calibration status - accel: " + String(calib.accel) + " gyro: " + String(calib.gyro) + " mag: " + String(calib.mag) + " sys: " + String(calib.sys));
      // wait for calibration, but abort if it takes too long
      const unsigned long CALIBRATION_TIMEOUT_MS = 60000; // 60 seconds
      unsigned long calibStart = millis();
      bool calib_timed_out = false;
      while (calib.accel < 3 || calib.gyro < 3 || calib.mag < 3 || calib.sys < 3)
      {
        if (millis() - calibStart >= CALIBRATION_TIMEOUT_MS) {
          bridge.publishStatus("Calibration timeout, aborting");
          imu.setOperationMode(currentMode); // restore previous mode
          bridge.publishEvent("calibration_timeout", "");
          calib_timed_out = true;
          break;
        }

        delay(500);
        calib = imu.getCalibrationStatus();
        bridge.publishStatus("Calibration status - accel: " + String(calib.accel) + " gyro: " + String(calib.gyro) + " mag: " + String(calib.mag) + " sys: " + String(calib.sys));
      }

      if (calib_timed_out) {
        // abort handling: do not proceed with success path
        return;
      }
      bridge.publishEvent("calibration_status", "accel=" + String(calib.accel) + ";gyro=" + String(calib.gyro) + ";mag=" + String(calib.mag) + ";sys=" + String(calib.sys));
      bridge.publishStatus("Calibration complete");
      for (int i = 0; i < cmd.arg_count; i++)
      {
        cmd_arg_t arg = cmd.args[i];
        if (arg.type == CMD_ARG_kEY_VALUE)
        {
          if (String(arg.key) == "mode")
          {
            String mode = String(arg.value);
            if (mode == "config")
            {
              // set config operation mode
              bridge.publishStatus("Already in CONFIG mode sensor are clibrated setting op mode IMU");
              imu.setOperationMode(IMU_MODE);
              bridge.publishEvent("operation_mode_changed", "mode=IMU");
            }
            else if (mode == "imu")
            {
              // set imu operation mode
              bridge.publishStatus("Setting operation mode to IMU");
              imu.setOperationMode(IMU_MODE);
              bridge.publishEvent("operation_mode_changed", "mode=IMU");
            }
            else if (mode == "ndof")
            {
              // set ndof operation mode
              bridge.publishStatus("Setting operation mode to NDOF");
              imu.setOperationMode(NDOF);
              bridge.publishEvent("operation_mode_changed", "mode=NDOF");
            }
            else
            {
              bridge.publishStatus("Unknown operation mode: " + mode);
              imu.setOperationMode(currentMode); // restore previous mode
              bridge.publishEvent("operation_mode_changed", "mode=" + String(currentMode));
            }
          }
          else
          {
           imu.setOperationMode(IMU_MODE); // set to IMU mode to be able to read sensor data and then put in suspend mode
           bridge.publishEvent("operation_mode_changed", "mode=IMU_MODE");
          }
        }
        else if (arg.type == CMD_ARG_FLAG)
        {
          if (String(arg.key) == "standby")
          {
            bridge.publishStatus("Setting operation mode to STANDBY");
            imu.setOperationMode(IMU_MODE); // set to IMU mode to be able to read sensor data and then put in suspend mode
            bridge.publishEvent("operation_mode_changed", "mode=IMU_MODE");
            mission_status = STANDING_BY; // set mission status to stopped to stop telemetry publishing
          }
        }
      }
    }
    else if (String(cmd.command) == "reset")
    {
      bridge.publishStatus("Resetting IMU");
      imu.resetSystem();
      bridge.publishEvent("imu_reset", "");
    }
    else if (String(cmd.command) == "selftest")
    {
      bridge.publishStatus("Performing self test");
      bool result = imu.performSelfTest();
      bridge.publishEvent("self_test_result", "result=" + String(result));
      bridge.publishStatus("Self test result: " + String(result));
    }
  }
}

void setupBridge()
{
  SerialBT.begin("ESP32_IMU_DEBUG_LOG"); // Bluetooth device name
  config.wifiSsid = "TELEMETRY";
  config.wifiPassword = "IMU1_telemetry";
  config.mqttHost = "10.79.43.181";
  config.mqttPort = 1883; // standard MQTT port
  config.deviceId = "esp32";
  config.baseTopic = "imu";
  config.logWithTimestamp = true;
  config.mirrorPublishedMessagesToLog = true;
  config.mirrorReceivedCommandsToLog = true;

  bridge.setLogStream(SerialBT, true);
  bridge.setCommandCallback(onCommand);
  bridge.setLogCallback([](const String &message)
    {
      // timestamp for log message
      unsigned long timestamp = millis();
      String logMessage = "[LOG at:" + String(timestamp) + " ms] " + message;
      Serial.println(logMessage); // USB Serial Monitor output
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
      if (address < 16)
        Serial.print("0");
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
  // Initialize IMU
  // TODO: record offset for calibration, maybe add a command to trigger calibration and store the offset values in non-volatile storage
  imu.begin();
  imu.setOperationMode(IMU_MODE);
  delay(500); // small delay to ensure imu is ready after mode change
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

void loop()
{
  bridge.loop();
  unsigned long now = millis();
  if (mission_status == STARTED && now - lastTelemetry >= missionConf.TELEMETRY_FREQUENCY_MS)
  { // Every 5 seconds, with a 50ms window to account for loop timing

    double deltaTheta = 0.0;
    bno055_burst_t data = imu.readAllData();

    // need to serilize the data into json format, maybe use ArduinoJson library for that
    char buffer[512];
    serializeDataToJson(data, buffer, sizeof(buffer));
    Serial.println(buffer); // Print JSON to Serial Monitor for debugging
    bridge.publishTelemetry(buffer);
    lastTelemetry = now;

    // Mission criteria check the ovverride duration check is indipendent
    if (mission_status == STARTED)
    {
      if (missionConf.exciteAxis == X_AXIS)
      {
        deltaTheta = abs(data.euler.getX() - lastTheta);
      }
      else if (missionConf.exciteAxis == Y_AXIS)
      {
        deltaTheta = abs(data.euler.getY() - lastTheta);
      }
      else if (missionConf.exciteAxis == Z_AXIS)
      {
        deltaTheta = abs(data.euler.getZ() - lastTheta);
      }
      if (deltaTheta <= missionConf.TEST_TOLERANCE_DPS)
      {
        mission_status = STOPPED;
        bridge.publishStatus("Test completed, stopping test...");
        bridge.publishEvent("test_completed", "duration=" + String(now - missionStartTime) + ";deltaTheta=" + String(deltaTheta));
      }
    }

    if (missionConf.exciteAxis == X_AXIS)
    {
      lastTheta = data.euler.getX();
    }
    else if (missionConf.exciteAxis == Y_AXIS)
    {
      lastTheta = data.euler.getY();
    }
    else if (missionConf.exciteAxis == Z_AXIS)
    {
      lastTheta = data.euler.getZ();
    }
  }

  // Output mission status every 10 seconds
  if (now - lastStatusUpdate >= 10000)
  {
    // Every 10 seconds, with a 50ms window
    String statusMsg = "Mission status: ";
    if (mission_status == STANDING_BY)
    {
      statusMsg += "STANDING BY";
    }
    else if (mission_status == STARTED)
    {
      statusMsg += "STARTED";
    }
    else if (mission_status == STOPPED)
    {
      statusMsg += "STOPPED";
    }
    bridge.publishStatus(statusMsg);
    lastStatusUpdate = now;
  }

  if (mission_status == STARTED && now - missionStartTime >= missionConf.TEST_DURATION_OVERRIDE_MS)
  {
    bridge.publishStatus("Test duration reached, stopping test...");
    mission_status = STOPPED;
  }
}
