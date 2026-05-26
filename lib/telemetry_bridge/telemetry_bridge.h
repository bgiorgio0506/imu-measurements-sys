#ifndef TELEMETRY_BRIDGE_H
#define TELEMETRY_BRIDGE_H
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
struct TelemetryConfig
{
    const char *wifiSsid = nullptr;
    const char *wifiPassword = nullptr;

    const char *mqttHost = nullptr;
    uint16_t mqttPort = 1883;
    const char *mqttUser = nullptr;
    const char *mqttPassword = nullptr;

    const char *deviceId = "esp32";
    const char *baseTopic = "esp32";

    // Reconnection behavior
    uint32_t wifiRetryMs = 10000;
    uint32_t mqttRetryMs = 5000;

    // MQTT behavior
    uint16_t mqttBufferSize = 512;
    uint16_t mqttKeepAliveSeconds = 30;
    bool retainStatus = true;

    // Log behavior
    bool logWithTimestamp = true;
    bool mirrorPublishedMessagesToLog = false;
    bool mirrorReceivedCommandsToLog = true;
    // Optional: static network config. Leave disabled for DHCP.
    bool useStaticIp = false;
    IPAddress localIp;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns1 = IPAddress(8, 8, 8, 8);
    IPAddress dns2 = IPAddress(1, 1, 1, 1);
};

class TelemetryBridge
{
public:
    using CommandCallback = std::function<void(const String &topic, const String &payload)>;
    using LogCallback = std::function<void(const String &message)>;

    TelemetryBridge();

    bool begin(const TelemetryConfig &config);
    void loop();

    bool publishTelemetry(const char *jsonPayload);
    bool publishTelemetry(const String &jsonPayload);
    bool publishStatus(const char *message, bool retained = true);
    bool publishStatus(const String &message, bool retained = true);
    bool publishEvent(const char *eventName, const char *jsonPayload = nullptr);
    bool publishEvent(const String &eventName, const String &jsonPayload = "");

    bool subscribeCommand();
    bool publish(const char *topicSuffix, const char *payload, bool retained = false);
    bool publishRaw(const char *fullTopic, const char *payload, bool retained = false);

    void setCommandCallback(CommandCallback callback);
    void setLogCallback(LogCallback callback);
    void setLogStream(Print &stream, bool timestamps = true);
    void clearLogStream();

    void setmirrorPublishedMessagesToLog(bool mirror) { _mirrorPublishedMessagesToLog = mirror; }
    void setMirrorReceivedCommandsToLog(bool mirror) { _mirrorReceivedCommandsToLog = mirror; }

    String ipAdressToString(const IPAddress &ip);
    int getRssi();

private:
    void connectWifiIfNeeded();
    void connectMqttIfNeeded();
    bool connectMqttOnce();
    void onMqttMessage(char *topic, byte *payload, unsigned int length);
    String topicTelemetry();
    String topicStatus();
    String topicEvent(const char *eventName);
    String mqttClientId();
    bool isWifiConnected();
    bool isMqttConnected();


    void log(const String &message);
    void writeStreamLog( const String &message);
    void logPublishedMessage(const char *fullTopic, const char *payload, bool ok);

    TelemetryConfig _config;
    WiFiClient _wifiClient;
    PubSubClient _mqtt{_wifiClient};
    CommandCallback _commandCallback;
    LogCallback _logCallback;
    Print *_logStream = nullptr;
    bool _logStreamTimestamps = true;
    bool _mirrorPublishedMessagesToLog = false;
    bool _mirrorReceivedCommandsToLog = true;

    bool _begun = false;
    uint32_t _lastWifiAttempt = 0;
    uint32_t _lastMqttAttempt = 0;
};

#endif // TELEMETRY_BRIDGE_H