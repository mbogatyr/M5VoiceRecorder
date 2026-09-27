#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "Storage.h"

// The Wi-Fi mode: an open access point "Voice Recorder" and a web page at
// http://192.168.4.1 that lists the recordings and plays, downloads and
// deletes them.
//
// There is deliberately no captive-portal DNS: macOS would open the page
// in its captive-network sheet, which cannot download files.
class WebPortal {
  public:
    static constexpr const char *kSsid = "Voice Recorder";

    explicit WebPortal(Storage &storage) : storage_(storage) {}

    void start();
    void stop();
    void loop();

    bool active() const { return active_; }
    uint8_t stations() const;

    // Whether a request came in since the last call; keeps the screen on.
    bool takeActivity();

  private:
    void handlePage();
    void handleList();
    void handleDownload();
    void handleDelete();

    Storage &storage_;
    WebServer server_{80};
    bool active_ = false;
    bool activity_ = false;
    bool routesAdded_ = false;
};
