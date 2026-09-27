#include "WebPortal.h"

#include <LittleFS.h>
#include <WiFi.h>
#include <uri/UriBraces.h>

#include "WavHeader.h"
#include "WebPage.h"

void WebPortal::start() {
    if (active_) {
        return;
    }
    WiFi.mode(WIFI_AP);
    WiFi.softAP(kSsid); // open network, 192.168.4.1

    if (!routesAdded_) {
        server_.on("/", HTTP_GET, [this] { handlePage(); });
        server_.on("/api/status", HTTP_GET, [this] { handleStatus(); });
        server_.on("/api/record", HTTP_POST, [this] { handleRecord(); });
        server_.on("/api/stop", HTTP_POST, [this] { handleStop(); });
        server_.on("/api/recordings", HTTP_GET, [this] { handleList(); });
        server_.on("/api/recordings", HTTP_DELETE, [this] { handleDeleteAll(); });
        server_.on(UriBraces("/rec/{}"), HTTP_GET, [this] { handleDownload(); });
        server_.on(UriBraces("/rec/{}"), HTTP_DELETE, [this] { handleDelete(); });
        server_.onNotFound([this] {
            activity_ = true;
            server_.send(404, "text/plain", "Not found");
        });
        routesAdded_ = true;
    }
    server_.begin();
    active_ = true;
}

void WebPortal::stop() {
    if (!active_) {
        return;
    }
    server_.stop();
    // ESP-IDF 4.4 logs a couple of "rxcb ... failed" netif errors here
    // whatever the order of these calls. They are harmless: the radio does
    // go off (checked on the board by scanning from a Mac).
    WiFi.softAPdisconnect(false);
    WiFi.mode(WIFI_OFF);
    active_ = false;
}

void WebPortal::loop() {
    if (active_) {
        server_.handleClient();
    }
}

uint8_t WebPortal::stations() const {
    return active_ ? WiFi.softAPgetStationNum() : 0;
}

bool WebPortal::takeActivity() {
    const bool a = activity_;
    activity_ = false;
    return a;
}

void WebPortal::handlePage() {
    activity_ = true;
    server_.send_P(200, "text/html; charset=utf-8", kWebPage);
}

bool WebPortal::refuseWhileRecording() {
    if (!hooks_.recording()) {
        return false;
    }
    server_.send(409, "text/plain", "Recording in progress");
    return true;
}

void WebPortal::handleStatus() {
    activity_ = true;
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json", hooks_.statusJson());
}

void WebPortal::handleRecord() {
    activity_ = true;
    String reason;
    if (!hooks_.record(reason)) {
        server_.send(409, "text/plain", reason);
        return;
    }
    server_.send(200, "application/json", hooks_.statusJson());
}

void WebPortal::handleStop() {
    activity_ = true;
    String reason;
    if (!hooks_.stop(reason)) {
        server_.send(409, "text/plain", reason);
        return;
    }
    server_.send(200, "application/json", hooks_.statusJson());
}

void WebPortal::handleList() {
    activity_ = true;
    // refresh() walks the whole file system; while recording the cached
    // list is good enough (the page shows it greyed out anyway).
    if (!hooks_.recording()) {
        storage_.refresh();
    }

    String json;
    json.reserve(96 + storage_.recordings().size() * 64);
    json += "{\"total\":";
    json += storage_.totalBytes();
    json += ",\"free\":";
    json += storage_.freeBytes();
    json += ",\"secondsLeft\":";
    json += wav::secondsForFreeBytes(storage_.recordableBytes());
    json += ",\"recordings\":[";
    bool first = true;
    for (const Storage::Entry &e : storage_.recordings()) {
        if (!first) {
            json += ',';
        }
        first = false;
        json += "{\"name\":\"";
        json += e.name;
        json += "\",\"bytes\":";
        json += e.bytes;
        json += ",\"seconds\":";
        json += wav::secondsForDataBytes(wav::repairedDataBytes(e.bytes));
        json += '}';
    }
    json += "]}";
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json", json);
}

void WebPortal::handleDownload() {
    activity_ = true;
    if (refuseWhileRecording()) {
        return;
    }
    const String name = server_.pathArg(0);
    if (!rec::isRecordingName(name.c_str())) {
        server_.send(404, "text/plain", "Not found");
        return;
    }
    File f = LittleFS.open("/" + name, "r");
    if (!f) {
        server_.send(404, "text/plain", "Not found");
        return;
    }
    server_.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
    server_.streamFile(f, "audio/wav");
    f.close();
}

void WebPortal::handleDelete() {
    activity_ = true;
    if (refuseWhileRecording()) {
        return;
    }
    const String name = server_.pathArg(0);
    if (!storage_.remove(name.c_str())) {
        server_.send(404, "text/plain", "Not found");
        return;
    }
    storage_.refresh();
    server_.send(204);
}

void WebPortal::handleDeleteAll() {
    activity_ = true;
    if (refuseWhileRecording()) {
        return;
    }
    const uint32_t removed = storage_.removeAll();
    server_.send(200, "application/json", String("{\"deleted\":") + removed + "}");
}
