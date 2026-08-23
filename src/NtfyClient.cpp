// ntfyClient.cpp

#include "NtfyClient.h"
#include <Preferences.h>

// Alapértelmezett beállítások
String gNtfyTopic = "KB_Teszt_20260813_666"; 
String gNtfyServer = "http://ntfy.sh"; 
String gNtfyNickname = ""; 
bool gNtfyStartupMsg = true;

void loadNtfyConfig() {
  Preferences prefs;
  prefs.begin("ntfy_cfg", true); // Olvasás mód
  gNtfyServer = prefs.getString("server", "http://ntfy.sh");
  gNtfyTopic = prefs.getString("topic", "KB_Teszt_20260813_666");
  gNtfyNickname = prefs.getString("nickname", "");
  gNtfyStartupMsg = prefs.getBool("startup", true);
  prefs.end();
}

void saveNtfyConfig(const String& server, const String& topic, const String& nickname, bool startupMsg) {
  gNtfyServer = server;
  gNtfyTopic = topic;
  gNtfyNickname = nickname;
  gNtfyStartupMsg = startupMsg;
  
  Preferences prefs;
  prefs.begin("ntfy_cfg", false); // Írás mód
  prefs.putString("server", gNtfyServer);
  prefs.putString("topic", gNtfyTopic);
  prefs.putString("nickname", gNtfyNickname);
  prefs.putBool("startup", gNtfyStartupMsg);
  prefs.end();
}

NtfyClient::NtfyClient(Stream& modemStream, const char* defaultTopic, const char* serverAddress)
    : _modem(modemStream), _lastHttpCode(0), _dbgStream(nullptr) {
    // Itt direkt nem tároljuk el a paramétereket fixen, hogy mindig a dinamikus globálisat használja
}

void NtfyClient::setTopic(const char* topic) {
    if (topic) {
        gNtfyTopic = topic;
    }
}

void NtfyClient::logDebug(const String& str) {
    if (_dbgStream) _dbgStream->println("[NTFY] " + str);
}

void NtfyClient::flushInput() {
    while (_modem.available()) _modem.read();
}

bool NtfyClient::sendCommand(const String& cmd, const char* expectedResponse, uint32_t timeoutMs) {
    flushInput();
    logDebug("CMD: " + cmd);
    _modem.println(cmd);

    uint32_t start = millis();
    String response = "";
    while (millis() - start < timeoutMs) {
        yield(); 
        while (_modem.available()) {
            char c = _modem.read();
            response += c;
            if (response.indexOf(expectedResponse) != -1) {
                logDebug("RESP OK: " + response);
                return true;
            }
            if (response.indexOf("ERROR") != -1) {
                logDebug("RESP ERR: " + response);
                return false;
            }
        }
        delay(10);
    }
    logDebug("TIMEOUT on: " + cmd);
    return false;
}

bool NtfyClient::waitForPrompt(const String& cmd, uint32_t timeoutMs) {
    flushInput();
    logDebug("CMD (waiting for prompt): " + cmd);
    _modem.println(cmd);

    uint32_t start = millis();
    String response = "";
    while (millis() - start < timeoutMs) {
        yield(); 
        while (_modem.available()) {
            char c = _modem.read();
            response += c;
            if (response.indexOf("DOWNLOAD") != -1 || response.indexOf(">") != -1) {
                logDebug("Prompt received OK");
                return true;
            }
            if (response.indexOf("ERROR") != -1) {
                logDebug("Prompt ERROR received");
                return false;
            }
        }
        delay(10);
    }
    logDebug("Prompt TIMEOUT");
    return false;
}

bool NtfyClient::send(const char* message, const char* title, NtfyPriority priority) {
    return sendToTopic(gNtfyTopic.c_str(), message, false, title, priority);
}

bool NtfyClient::sendToTopic(const char* topic, const char* message, bool retainTopic, const char* title, NtfyPriority priority) {
    NtfyMessage msg;
    msg.topic = topic;
    msg.message = message;
    msg.title = title;
    msg.priority = priority;
    msg.updateDefaultTopic = retainTopic;
    return publish(msg);
}

// --- KLASSZIKUS HTTP STACK ---
bool NtfyClient::publish(const NtfyMessage& msg, uint32_t timeoutMs) {
    // DINAMIKUS ÉRTÉKOLVASÁS: Mindig az aktuális globális topicot használjuk!
    String activeTopic = (msg.topic && strlen(msg.topic) > 0) ? String(msg.topic) : gNtfyTopic;

    if (activeTopic.length() == 0 || !msg.message) {
        logDebug("Topic or message missing!");
        return false;
    }

    if (msg.updateDefaultTopic && msg.topic && strlen(msg.topic) > 0) {
        gNtfyTopic = msg.topic;
    }

    _lastHttpCode = 0;

    // DINAMIKUS ÉRTÉKOLVASÁS: Mindig az aktuális globális szervert használjuk!
    String serverToUse = gNtfyServer.length() > 0 ? gNtfyServer : "http://ntfy.sh";
    
    if (serverToUse.startsWith("https://")) {
        serverToUse.replace("https://", "http://");
    } else if (!serverToUse.startsWith("http://")) {
        serverToUse = "http://" + serverToUse;
    }

    String url = serverToUse;
    if (!url.endsWith("/")) url += "/";
    url += activeTopic;

    bool firstParam = true;
    if (msg.title && strlen(msg.title) > 0) {
        String encodedTitle = msg.title;
        encodedTitle.replace(" ", "+");
        url += (firstParam ? "?" : "&") + String("title=") + encodedTitle;
        firstParam = false;
    }
    
    if (msg.priority != NtfyPriority::Default) {
        url += (firstParam ? "?" : "&") + String("priority=") + String(static_cast<int>(msg.priority));
        firstParam = false;
    }

    logDebug("HTTP Publishing to URL: " + url);

    sendCommand("AT+HTTPTERM", "OK", 1000);

    if (!sendCommand("AT+HTTPINIT", "OK", 3000)) return false;

    sendCommand("AT+HTTPPARA=\"CID\",1", "OK", 2000);
    
    if (!sendCommand("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK", 2000)) {
        sendCommand("AT+HTTPTERM", "OK", 1000);
        return false;
    }

    sendCommand("AT+HTTPPARA=\"CONTENT\",\"text/plain\"", "OK", 2000);

    String textPayload = String(msg.message);
    size_t payloadLen = textPayload.length();
    
    flushInput();
    String dataCmd = "AT+HTTPDATA=" + String(payloadLen) + ",10000";

    if (!waitForPrompt(dataCmd, 5000)) {
        logDebug("HTTPDATA prompt not received");
        sendCommand("AT+HTTPTERM", "OK", 1000);
        return false;
    }

    delay(100);
    _modem.print(textPayload);
    _modem.flush();
    
    // Robusztus válaszolvasó
    uint32_t dataStart = millis();
    bool dataOk = false;
    String dataResp = "";
    
    while (millis() - dataStart < 5000) {
        yield();
        while (_modem.available()) {
            char c = _modem.read();
            dataResp += c;
        }
        if (dataResp.indexOf("OK") != -1) {
            dataOk = true;
            break;
        }
        if (dataResp.indexOf("ERROR") != -1) {
            break;
        }
        delay(10);
    }
    
    if (!dataOk) {
        dataResp.replace("\r", "<CR>");
        dataResp.replace("\n", "<LF>");
        logDebug("Failed to ingest HTTPDATA. Modem resp: " + dataResp);
        sendCommand("AT+HTTPTERM", "OK", 1000);
        return false;
    }

    flushInput();
    logDebug("Executing AT+HTTPACTION=1 (POST)");
    _modem.println("AT+HTTPACTION=1");

    uint32_t start = millis();
    bool actionCompleted = false;
    uint32_t actionTimeout = 35000;

    while (millis() - start < actionTimeout) {
        yield(); 
        if (_modem.available()) {
            String line = _modem.readStringUntil('\n');
            line.trim();
            if (line.length() > 0) logDebug("RESP: " + line);

            if (line.startsWith("+HTTPACTION:")) {
                int firstComma = line.indexOf(',');
                int secondComma = line.indexOf(',', firstComma + 1);
                if (firstComma != -1 && secondComma != -1) {
                    _lastHttpCode = line.substring(firstComma + 1, secondComma).toInt();
                }
                actionCompleted = true;
                break;
            }
        }
        delay(10);
    }

    sendCommand("AT+HTTPTERM", "OK", 2000);

    if (actionCompleted && _lastHttpCode >= 200 && _lastHttpCode < 300) {
        logDebug("ntfy published successfully! HTTP Code: " + String(_lastHttpCode));
        return true;
    }

    logDebug("ntfy publish failed! HTTP Code: " + String(_lastHttpCode));
    return false;
}

String NtfyClient::readHttpResponseBody(uint32_t timeoutMs) {
    flushInput();
    logDebug("Executing AT+HTTPREAD");
    _modem.println("AT+HTTPREAD");

    uint32_t start = millis();
    String body = "";
    bool readingBody = false;

    while (millis() - start < timeoutMs) {
        yield();
        if (_modem.available()) {
            String line = _modem.readStringUntil('\n');
            line.trim();

            if (line.startsWith("+HTTPREAD:")) {
                readingBody = true;
                continue;
            }
            if (line == "OK") {
                break;
            }
            if (readingBody && line.length() > 0) {
                if (body.length() > 0) body += "\n";
                body += line;
            }
        }
        delay(10);
    }
    return body;
}

NtfyPollResult NtfyClient::pollMessages(const char* since, const char* topic, uint32_t timeoutMs) {
    String activeTopic = (topic && strlen(topic) > 0) ? String(topic) : gNtfyTopic;
    NtfyPollResult result;

    if (activeTopic.length() == 0) return result;

    String serverToUse = gNtfyServer.length() > 0 ? gNtfyServer : "http://ntfy.sh";
    if (serverToUse.startsWith("https://")) {
        serverToUse.replace("https://", "http://");
    } else if (!serverToUse.startsWith("http://")) {
        serverToUse = "http://" + serverToUse;
    }

    sendCommand("AT+HTTPTERM", "OK", 1000);
    if (!sendCommand("AT+HTTPINIT", "OK", 3000)) return result;
    sendCommand("AT+HTTPPARA=\"CID\",1", "OK", 2000);

    String url = serverToUse;
    if (!url.endsWith("/")) url += "/";
    url += activeTopic;
    url += "/json?poll=1";
    if (since && strlen(since) > 0) {
        url += "&since=";
        url += since;
    }

    if (!sendCommand("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK", 2000)) {
        sendCommand("AT+HTTPTERM", "OK", 1000);
        return result;
    }

    flushInput();
    _modem.println("AT+HTTPACTION=0");

    uint32_t start = millis();
    bool actionCompleted = false;
    int dataLen = 0;

    while (millis() - start < timeoutMs) {
        yield();
        if (_modem.available()) {
            String line = _modem.readStringUntil('\n');
            line.trim();

            if (line.startsWith("+HTTPACTION:")) {
                int firstComma = line.indexOf(',');
                int secondComma = line.indexOf(',', firstComma + 1);
                if (firstComma != -1 && secondComma != -1) {
                    _lastHttpCode = line.substring(firstComma + 1, secondComma).toInt();
                    dataLen = line.substring(secondComma + 1).toInt();
                }
                actionCompleted = true;
                break;
            }
        }
        delay(10);
    }

    result.httpCode = _lastHttpCode;
    if (actionCompleted && _lastHttpCode >= 200 && _lastHttpCode < 300) {
        if (dataLen > 0) {
            result.rawPayload = readHttpResponseBody(5000);
        }
        result.success = true;
    }

    sendCommand("AT+HTTPTERM", "OK", 2000);
    return result;
}

NtfyPollResult NtfyClient::pollRaw(const char* since, const char* topic, uint32_t timeoutMs) {
    String activeTopic = (topic && strlen(topic) > 0) ? String(topic) : gNtfyTopic;
    NtfyPollResult result;

    if (activeTopic.length() == 0) return result;

    String serverToUse = gNtfyServer.length() > 0 ? gNtfyServer : "http://ntfy.sh";
    if (serverToUse.startsWith("https://")) {
        serverToUse.replace("https://", "http://");
    } else if (!serverToUse.startsWith("http://")) {
        serverToUse = "http://" + serverToUse;
    }

    sendCommand("AT+HTTPTERM", "OK", 1000);
    if (!sendCommand("AT+HTTPINIT", "OK", 3000)) return result;
    sendCommand("AT+HTTPPARA=\"CID\",1", "OK", 2000);

    String url = serverToUse;
    if (!url.endsWith("/")) url += "/";
    url += activeTopic;
    url += "/raw?poll=1";
    if (since && strlen(since) > 0) {
        url += "&since=";
        url += since;
    }

    if (!sendCommand("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK", 2000)) {
        sendCommand("AT+HTTPTERM", "OK", 1000);
        return result;
    }

    flushInput();
    _modem.println("AT+HTTPACTION=0");

    uint32_t start = millis();
    bool actionCompleted = false;
    int dataLen = 0;

    while (millis() - start < timeoutMs) {
        yield();
        if (_modem.available()) {
            String line = _modem.readStringUntil('\n');
            line.trim();

            if (line.startsWith("+HTTPACTION:")) {
                int firstComma = line.indexOf(',');
                int secondComma = line.indexOf(',', firstComma + 1);
                if (firstComma != -1 && secondComma != -1) {
                    _lastHttpCode = line.substring(firstComma + 1, secondComma).toInt();
                    dataLen = line.substring(secondComma + 1).toInt();
                }
                actionCompleted = true;
                break;
            }
        }
        delay(10);
    }

    result.httpCode = _lastHttpCode;
    if (actionCompleted && _lastHttpCode >= 200 && _lastHttpCode < 300) {
        if (dataLen > 0) {
            result.rawPayload = readHttpResponseBody(5000);
        }
        result.success = true;
    }

    sendCommand("AT+HTTPTERM", "OK", 2000);
    return result;
}