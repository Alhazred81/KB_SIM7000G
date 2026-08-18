//  NtfyClient.h

#ifndef NTFY_CLIENT_H
#define NTFY_CLIENT_H

#include <Arduino.h>
#include <Stream.h>

enum class NtfyPriority : int {
    Min = 1,
    Low = 2,
    Default = 3,
    High = 4,
    Max = 5
};

struct NtfyMessage {
    const char* topic = nullptr;
    const char* message = nullptr;
    const char* title = nullptr;
    NtfyPriority priority = NtfyPriority::Default;
    const char* tags = nullptr;
    const char* clickUrl = nullptr;
    bool updateDefaultTopic = false;
};

struct NtfyPollResult {
    bool success = false;
    String rawPayload = "";
    int httpCode = 0;
};

class NtfyClient {
public:
    NtfyClient(Stream& modemStream, const char* defaultTopic = "", const char* server = "ntfy.sh");

    void setTopic(const char* topic);
    void setDebugStream(Stream* dbg) { _dbgStream = dbg; }

    bool send(const char* message, const char* title = "", NtfyPriority priority = NtfyPriority::Default);
    bool sendToTopic(const char* topic, const char* message, bool retainTopic = false, const char* title = "", NtfyPriority priority = NtfyPriority::Default);
    bool publish(const NtfyMessage& msg, uint32_t timeoutMs = 15000);

    NtfyPollResult pollMessages(const char* since = "all", const char* topic = "", uint32_t timeoutMs = 15000);
    NtfyPollResult pollRaw(const char* since = "all", const char* topic = "", uint32_t timeoutMs = 15000);

    int getLastHttpCode() const { return _lastHttpCode; }

private:
    Stream& _modem;
    Stream* _dbgStream = nullptr;
    String _defaultTopic;
    String _server;
    int _lastHttpCode = 0;

    void logDebug(const String& str);
    void flushInput();
    bool sendCommand(const String& cmd, const char* expectedResponse = "OK", uint32_t timeoutMs = 5000);
    bool waitForPrompt(const String& cmd, uint32_t timeoutMs = 5000);
    String readHttpResponseBody(uint32_t timeoutMs = 5000);
};

#endif // NTFY_CLIENT_H