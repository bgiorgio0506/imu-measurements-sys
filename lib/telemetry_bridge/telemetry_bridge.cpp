#include "telemetry_bridge.h"

TelemetryBridge::TelemetryBridge() : _mqtt(_wifiClient) {}

bool TelemetryBridge::begin(const TelemetryConfig& config) {
  _config = config;

  if (!hasText(_config.wifiSsid) || !hasText(_config.mqttHost) || !hasText(_config.deviceId) || !hasText(_config.baseTopic)) {
    log("TelemetryBridge: missing required config");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  if (_config.useStaticIp) {
    WiFi.config(_config.localIp, _config.gateway, _config.subnet, _config.dns1, _config.dns2);
  }

  _mqtt.setServer(_config.mqttHost, _config.mqttPort);
  _mqtt.setKeepAlive(_config.mqttKeepAliveSeconds);
  _mqtt.setBufferSize(_config.mqttBufferSize);
  _mqtt.setCallback([this](char* topic, byte* payload, unsigned int length) {
    this->onMqttMessage(topic, payload, length);
  });

  _begun = true;
  connectWifiIfNeeded();
  return true;
}

void TelemetryBridge::loop() {
  if (!_begun) return;

  connectWifiIfNeeded();

  if (isWifiConnected()) {
    connectMqttIfNeeded();
  }

  if (isMqttConnected()) {
    _mqtt.loop();
  }
}

bool TelemetryBridge::publishTelemetry(const char* jsonPayload) {
  return publishRaw(topicTelemetry().c_str(), jsonPayload, false);
}

bool TelemetryBridge::publishTelemetry(const String& jsonPayload) {
  return publishTelemetry(jsonPayload.c_str());
}

bool TelemetryBridge::publishStatus(const char* message, bool retained) {
  return publishRaw(topicStatus().c_str(), message, retained);
}

bool TelemetryBridge::publishStatus(const String& message, bool retained) {
  return publishStatus(message.c_str(), retained);
}

bool TelemetryBridge::publishEvent(const char* eventName, const char* jsonPayload) {
   if (!hasText(eventName)) return false;

  String eventTopic = topicEvent("event") + "/" + eventName;
  const char* payload = hasText(jsonPayload) ? jsonPayload : "{}";
  return publishRaw(eventTopic.c_str(), payload, false);
}

bool TelemetryBridge::publishEvent(const String& eventName, const String& jsonPayload) {
  return publishEvent(eventName.c_str(), jsonPayload.length() ? jsonPayload.c_str() : nullptr);
}

bool TelemetryBridge::subscribeCommand() {
  String commandTopic = topicEvent("command") + "/#";
  return _mqtt.subscribe(commandTopic.c_str());
}

bool TelemetryBridge::publish(const char* topicSuffix, const char* payload, bool retained) {
 if (!hasText(topicSuffix)) return false;
  return publishRaw(topic(topicSuffix).c_str(), payload, retained);
}

bool TelemetryBridge::publishRaw(const char* fullTopic, const char* payload, bool retained) {
  if (!isMqttConnected() || !hasText(fullTopic) || payload == nullptr) return false;
  return _mqtt.publish(fullTopic, payload, retained);
}

void TelemetryBridge::setCommandCallback(CommandCallback callback) {
  _commandCallback = callback;
}

void TelemetryBridge::setLogCallback(LogCallback callback) {
  _logCallback = callback;
}

bool TelemetryBridge::isWifiConnected()  {
  return WiFi.status() == WL_CONNECTED;
}

bool TelemetryBridge::isMqttConnected() {
  return _mqtt.connected();
}

String TelemetryBridge::ipAdressToString(const IPAddress& ip) {
  return isWifiConnected() ? WiFi.localIP().toString() : String("");
}

int TelemetryBridge::getRssi() {
  return isWifiConnected() ? WiFi.RSSI() : 0;
}

String TelemetryBridge::topicTelemetry()  {
  return topic("telemetry", _config);
}

String TelemetryBridge::topicStatus()  {
  return topic("status", _config);
}

String TelemetryBridge::topicEvent(const char* eventName)  {
  return topic("event", _config);
}


void TelemetryBridge::connectWifiIfNeeded() {
  if (isWifiConnected()) return;

  uint32_t now = millis();
  if (_lastWifiAttempt != 0 && now - _lastWifiAttempt < _config.wifiRetryMs) return;

  _lastWifiAttempt = now;
  log("WiFi: connecting to " + String(_config.wifiSsid));
  WiFi.disconnect(false);
  WiFi.begin(_config.wifiSsid, _config.wifiPassword);
}

void TelemetryBridge::connectMqttIfNeeded() {
   if (isMqttConnected()) return;

  uint32_t now = millis();
  if (_lastMqttAttempt != 0 && now - _lastMqttAttempt < _config.mqttRetryMs) return;

  _lastMqttAttempt = now;
  connectMqttOnce();
}

bool TelemetryBridge::connectMqttOnce() {
  String clientId = mqttClientId();
  String willTopic = topicStatus();

  log("MQTT: connecting to " + String(_config.mqttHost) + ":" + String(_config.mqttPort));

  bool ok = false;

  if (hasText(_config.mqttUser)) {
    ok = _mqtt.connect(
      clientId.c_str(),
      _config.mqttUser,
      _config.mqttPassword,
      willTopic.c_str(),
      1,
      _config.retainStatus,
      "offline"
    );
  } else {
    ok = _mqtt.connect(
      clientId.c_str(),
      nullptr,
      nullptr,
      willTopic.c_str(),
      1,
      _config.retainStatus,
      "offline"
    );
  }

  if (!ok) {
    log("MQTT: connect failed, state=" + String(_mqtt.state()));
    return false;
  }

  log("MQTT: connected");
  publishStatus("online", _config.retainStatus);
  subscribeCommand();
  return true;
}

void TelemetryBridge::onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String topicStr(topic);
  String payloadStr((char*)payload, length);
  log("MQTT: message received on topic " + topicStr + ": " + payloadStr);

  if (_commandCallback) {
    _commandCallback(topicStr, payloadStr);
  }
}

String TelemetryBridge::mqttClientId() {
  String clientId = "esp32-"+String(_config.deviceId) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  return clientId;
}

void TelemetryBridge::setLogStream(Print& stream, bool timestamps) {
  _logStream = &stream;
  _logStreamTimestamps = timestamps;
}
void TelemetryBridge::clearLogStream() {
  _logStream = nullptr;
}

void TelemetryBridge::log( const String& message) {
  if (_logCallback) {
    _logCallback(message);
  }
}

void TelemetryBridge::writeStreamLog(const String& message) {
  if (_logStream) {
    if (_logStreamTimestamps) {
      _logStream->print("[");
      _logStream->print(millis());
      _logStream->print("] ");
    }
    _logStream->println(message);
  }
}

void TelemetryBridge::logPublishedMessage(const char* fullTopic, const char* payload, bool ok)  {
  if(!_mirrorPublishedMessagesToLog) return;
  String line = ok ? "MQTT TX " : "MQTT TX FAILED ";
  line += hasText(fullTopic) ? fullTopic : "<no-topic>";
  line += " => ";
  line += payload != nullptr ? payload : "<null>";

  log(line);
  
}

#pragma region utils
static bool hasText(const char* value) {
  return value != nullptr && value[0] != '\0';
}

static String topic(const char* suffix = nullptr, const TelemetryConfig& config = TelemetryConfig()) {
  String t = String(config.baseTopic);{
  String t = String(config.baseTopic);

  if (!t.endsWith("/")) t += "/";
  t += config.deviceId;

  if (hasText(suffix)) {
    t += "/";
    t += suffix;
  }

  return t;
}