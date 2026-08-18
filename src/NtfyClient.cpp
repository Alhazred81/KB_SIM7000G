//ntfyclient.cpp

#include "NtfyClient.h"

NtfyClient::NtfyClient(Stream& modemStream, const char* defaultTopic, const char* server)
    : _modem(modemStream), _defaultTopic(defaultTopic ? defaultTopic : ""), _server(server), _lastHttpCode(0), _dbgStream(nullptr) {}

void NtfyClient::setTopic(const char* topic) {
    if (topic) {
        _defaultTopic = topic;
    }
}

void NtfyClient::logDebug(const String& str) {
    if (_dbgStream) {
        _dbgStream->println("[NTFY] " + str);
    }
}

void NtfyClient::flushInput() {
    while (_modem.available()) {
        _modem.read();
    }
}

bool NtfyClient::sendCommand(const String& cmd, const char* expectedResponse, uint32_t timeoutMs) {
    flushInput();
    logDebug("CMD: " + cmd);
    _modem.println(cmd);

    uint32_t start = millis();
    String response = "";
    while (millis() - start < timeoutMs) {
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
        while (_modem.available()) {
            char c = _modem.read();
            response += c;
            if (response.indexOf("DOWNLOAD") != -1 || response.indexOf(">") != -1) {
                return true;
            }
            if (response.indexOf("ERROR") != -1) {
                return false;
            }
        }
        delay(10);
    }
    return false;
}

bool NtfyClient::send(const char* message, const char* title, NtfyPriority priority) {
    return sendToTopic(_defaultTopic.c_str(), message, false, title, priority);
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

bool NtfyClient::publish(const NtfyMessage& msg, uint32_t timeoutMs) {
    String activeTopic = (msg.topic && strlen(msg.topic) > 0) ? String(msg.topic) : _defaultTopic;

    if (activeTopic.length() == 0 || !msg.message) {
        logDebug("Topic or message missing!");
        return false;
    }

    if (msg.updateDefaultTopic && msg.topic && strlen(msg.topic) > 0) {
        _defaultTopic = msg.topic;
        logDebug("Default topic updated and retained: " + _defaultTopic);
    }

    _lastHttpCode = 0;

    sendCommand("AT+HTTPTERM", "OK", 1000);

    if (!sendCommand("AT+HTTPINIT", "OK", 3000)) {
        logDebug("HTTPINIT failed");
        return false;
    }

    sendCommand("AT+HTTPPARA=\"CID\",1", "OK", 2000);

    String url = "http://";
    url += _server;
    url += "/";
    url += activeTopic;

    if (!sendCommand("AT+HTTPPARA=\"URL\",\"" + url + "\"", "OK", 2000)) {
        sendCommand("AT+HTTPTERM", "OK", 1000);
        return false;
    }

    String jsonPayload = "{";
    jsonPayload += "\"topic\":\"" + activeTopic + "\",";
    jsonPayload += "\"message\":\"" + String(msg.message) + "\"";
    
    if (msg.title && strlen(msg.title) > 0) {
        jsonPayload += ",\"title\":\"" + String(msg.title) + "\"";
    }
    
    jsonPayload += ",\"priority\":" + String(static_cast<int>(msg.priority));
    
    if (msg.tags && strlen(msg.tags) > 0) {
        jsonPayload += ",\"tags\":[\"";
        String tagStr = msg.tags;
        tagStr.replace(",", "\",\"");
        jsonPayload += tagStr + "\"]";
    }
    
    if (msg.clickUrl && strlen(msg.clickUrl) > 0) {
        jsonPayload += ",\"click\":\"" + String(msg.clickUrl) + "\"";
    }
    jsonPayload += "}";

    sendCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK", 2000);

    size_t payloadLen = jsonPayload.length();
    String dataCmd = "AT+HTTPDATA=" + String(payloadLen) + ",10000";

    if (!waitForPrompt(dataCmd, 5000)) {
        logDebug("HTTPDATA prompt not received");
        sendCommand("AT+HTTPTERM", "OK", 1000);
        return false;
    }

    _modem.print(jsonPayload);
    delay(100);

    flushInput();
    logDebug("Executing AT+HTTPACTION=1 (POST -> " + activeTopic + ")");
    _modem.println("AT+HTTPACTION=1");

    uint32_t start = millis();
    bool actionCompleted = false;

    while (millis() - start < timeoutMs) {
        if (_modem.available()) {
            String line = _modem.readStringUntil('\n');
            line.trim();
            logDebug("RESP: " + line);

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
    String activeTopic = (topic && strlen(topic) > 0) ? String(topic) : _defaultTopic;
    NtfyPollResult result;

    if (activeTopic.length() == 0) {
        logDebug("Poll error: No topic specified!");
        return result;
    }

    sendCommand("AT+HTTPTERM", "OK", 1000);

    if (!sendCommand("AT+HTTPINIT", "OK", 3000)) {
        logDebug("HTTPINIT failed");
        return result;
    }

    sendCommand("AT+HTTPPARA=\"CID\",1", "OK", 2000);

    String url = "http://";
    url += _server;
    url += "/";
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
    logDebug("Executing AT+HTTPACTION=0 (GET -> " + url + ")");
    _modem.println("AT+HTTPACTION=0");

    uint32_t start = millis();
    bool actionCompleted = false;
    int dataLen = 0;

    while (millis() - start < timeoutMs) {
        if (_modem.available()) {
            String line = _modem.readStringUntil('\n');
            line.trim();
            logDebug("RESP: " + line);

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
        logDebug("Poll successful. Body: " + result.rawPayload);
    } else {
        logDebug("Poll failed! HTTP Code: " + String(_lastHttpCode));
    }

    sendCommand("AT+HTTPTERM", "OK", 2000);
    return result;
}

NtfyPollResult NtfyClient::pollRaw(const char* since, const char* topic, uint32_t timeoutMs) {
    String activeTopic = (topic && strlen(topic) > 0) ? String(topic) : _defaultTopic;
    NtfyPollResult result;

    if (activeTopic.length() == 0) {
        logDebug("Poll error: No topic specified!");
        return result;
    }

    sendCommand("AT+HTTPTERM", "OK", 1000);

    if (!sendCommand("AT+HTTPINIT", "OK", 3000)) {
        return result;
    }

    sendCommand("AT+HTTPPARA=\"CID\",1", "OK", 2000);

    String url = "http://";
    url += _server;
    url += "/";
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
    logDebug("Executing AT+HTTPACTION=0 (GET Raw)");
    _modem.println("AT+HTTPACTION=0");

    uint32_t start = millis();
    bool actionCompleted = false;
    int dataLen = 0;

    while (millis() - start < timeoutMs) {
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