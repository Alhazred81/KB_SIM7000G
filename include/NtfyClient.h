// NTFY_CLIENT_H
#ifndef NTFY_CLIENT_H
#define NTFY_CLIENT_H

#include <Arduino.h>
#include <Stream.h>

enum class NtfyPriority {
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
    const char* tags = nullptr;
    const char* clickUrl = nullptr;
    NtfyPriority priority = NtfyPriority::Default;
    bool updateDefaultTopic = false;
};

struct NtfyPollResult {
    bool success = false;
    int httpCode = 0;
    String rawPayload = "";
};

// --- GLOBÁLIS VÁLTOZÓK ÉS FÜGGVÉNYEK A KONFIGURÁCIÓHOZ ---
extern String gNtfyTopic;
extern String gNtfyServer;
extern String gNtfyNickname;
extern bool gNtfyStartupMsg;

void loadNtfyConfig();
void saveNtfyConfig(const String& server, const String& topic, const String& nickname, bool startupMsg);

class NtfyClient {
public:
    NtfyClient(Stream& modemStream, const char* defaultTopic = "", const char* serverAddress = "ntfy.sh");
    
    void setTopic(const char* topic);
    void setDebugStream(Stream* dbgStream) { _dbgStream = dbgStream; }
    int getLastHttpCode() const { return _lastHttpCode; }

    bool send(const char* message, const char* title = nullptr, NtfyPriority priority = NtfyPriority::Default);
    bool sendToTopic(const char* topic, const char* message, bool retainTopic = false, const char* title = nullptr, NtfyPriority priority = NtfyPriority::Default);
    
    bool publish(const NtfyMessage& msg, uint32_t timeoutMs = 35000);

    NtfyPollResult pollMessages(const char* since = nullptr, const char* topic = nullptr, uint32_t timeoutMs = 15000);
    NtfyPollResult pollRaw(const char* since = nullptr, const char* topic = nullptr, uint32_t timeoutMs = 15000);

private:
    Stream& _modem;
    String _defaultTopic;
    String _server;
    int _lastHttpCode;
    Stream* _dbgStream;

    void flushInput();
    bool sendCommand(const String& cmd, const char* expectedResponse, uint32_t timeoutMs);
    bool waitForPrompt(const String& cmd, uint32_t timeoutMs);
    void logDebug(const String& str);
    String readHttpResponseBody(uint32_t timeoutMs);
};

#endif 