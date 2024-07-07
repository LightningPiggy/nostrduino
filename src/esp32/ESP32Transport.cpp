#include "ESP32Transport.h"


using namespace nostr;

NostrString esp32::ESP32Transport::getInvoiceFromLNAddr(NostrString addr,
                                             unsigned long long amount,
                                             NostrString comment) {
    // username@domain.com
    // becomes https://domain.com/.well-known/lnurlp/username
    unsigned int atpos = NostrString_indexOf(addr, "@");
    if (atpos == -1) {
        return "";
    }
    NostrString username = NostrString_substring(addr, 0, atpos);
    NostrString domain = NostrString_substring(addr, atpos + 1);
    NostrString url = "https://" + domain + "/.well-known/lnurlp/" +
                      NostrString_urlEncode(username);

    HTTPClient http;
    http.begin(url.c_str());
    int httpCode = http.GET();
    if (httpCode <= 0 || httpCode != HTTP_CODE_OK) return "";
    NostrString json = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        Utils::log("Failed to parse lnaddr JSON " + json);
        return "";
    }
    NostrString status = doc["status"];
    if (status != "OK") {
        Utils::log("LNURLP status not OK");
        return "";
    }
    NostrString callback = doc["callback"];
    if (callback == "") {
        Utils::log("LNURLP callback not found");
        return "";
    }
    NostrString lnurlp = callback;
    if (NostrString_indexOf(lnurlp, "?") != -1) {
        lnurlp += "&";
    } else {
        lnurlp += "?";
    }
    lnurlp += "amount=" + NostrString_intToString(amount);
    if (NostrString_length(comment) > 0) {
        lnurlp += "&comment=" + NostrString_urlEncode(comment);
    }

    http.end();
    http.begin(lnurlp.c_str());
    httpCode = http.GET();
    if (httpCode <= 0 || httpCode != HTTP_CODE_OK) return "";
    json=http.getString();
    error = deserializeJson(doc, json);
    if (error) {
        Utils::log("Failed to parse lnurlp JSON "+json);
        return "";
    }
    NostrString callbackStatus = doc["status"];
    if (callbackStatus != "OK") {
        Utils::log("LNURLP callback status not OK");
        return "";
    }
    NostrString invoice = doc["pr"];
    if (invoice == "") {
        Utils::log("LNURLP invoice not found");
        return "";
    }
    http.end();
    return invoice;
}

Connection *esp32::ESP32Transport::connect(NostrString url) {
    ESP32Connection *conn = new ESP32Connection(this,url);
    connections.push_back(conn);
    return conn;
}

void esp32::ESP32Transport::close() {
    for (auto conn : connections) {
        conn->disconnect();
        delete conn;
    }
    connections.clear();
}

void esp32::ESP32Transport::disconnect(Connection *conn) {
    for (int i = 0; i < connections.size(); i++) {
        if (connections[i] == conn) {
            conn->disconnect();
            delete conn;
            connections.erase(connections.begin() + i);
            return;
        }
    }
}

esp32::ESP32Connection::ESP32Connection(ESP32Transport *transport,
                                        NostrString url) {
    this->transport=transport;
    bool ssl = NostrString_startsWith(url, "wss://");
    url = NostrString_substring(url, ssl ? 6 : 5);
    NostrString host =
        NostrString_substring(url, 0, NostrString_indexOf(url, "/"));
    NostrString path =
        NostrString_substring(url, NostrString_indexOf(url, "/"));
    if (path.equals("")) path = "/";
    int port = ssl ? 443 : 80;
    if (NostrString_indexOf(host, ":") != -1) {
        NostrString portStr =
            NostrString_substring(host, NostrString_indexOf(host, ":") + 1);
        port = NostrString_toInt(portStr);
        host = NostrString_substring(host, 0, NostrString_indexOf(host, ":"));
    }
    if (ssl) {
        Utils::log("Connecting to " + host + " : " + port + " with path " +
                   path + " using SSL...");
        ws.beginSSL(host, port, path);
    } else {
        Utils::log("Connecting to " + host + " : " + port + " with path " +
                   path + "...");
        ws.begin(host, port, path);
    }
    ws.setReconnectInterval(5000);
    ws.onEvent([this](WStype_t type, uint8_t *payload, size_t length) {
        switch (type) {
            case WStype_DISCONNECTED:
                Utils::log("ESP32Connection disconnected.");
                break;
            case WStype_CONNECTED:
                Utils::log("ESP32Connection connected.");
                break;
            case WStype_TEXT: {
                NostrString message = NostrString_fromChars((char *)payload);
                for (auto &listener : messageListeners) {
                    listener(message);
                }             
                
                break;
            }
            case WStype_ERROR:
                Utils::log("ESP32Connection error.");
                break;
            default:
                break;
        }
    });
}

void esp32::ESP32Connection::send(NostrString message) {
    ws.sendTXT((uint8_t *)NostrString_toChars(message), message.length());
}

void esp32::ESP32Connection::disconnect() {
    ws.disconnect();
    this->transport->disconnect(this);
}

bool esp32::ESP32Transport::isReady() {
    return (WiFi.status() == WL_CONNECTED);
}

void esp32::ESP32Connection::loop() {
     ws.loop(); 
}

bool esp32::ESP32Connection::isReady() { return ws.isConnected(); }

void esp32::ESP32Connection::addMessageListener(
    std::function<void(NostrString)> listener) {
    messageListeners.push_back(listener);
}

esp32::ESP32Transport::~ESP32Transport() { close(); }

esp32::ESP32Connection::~ESP32Connection() { ws.disconnect(); }

esp32::ESP32Transport::ESP32Transport() {}