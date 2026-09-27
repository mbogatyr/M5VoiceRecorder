#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include <functional>

#include "Storage.h"

// The Wi-Fi mode: an open access point "Voice Recorder" and a web page at
// http://192.168.4.1 that starts and stops recording, lists the recordings
// and plays, downloads and deletes them.
//
// Requests are handled inside loop() (handleClient), one at a time, so the
// handlers may call straight into main.cpp through the callbacks below.
// While recording, file downloads and deletions are refused (409): a
// download would hold the loop for seconds and the recording would get
// gaps.
//
// There is deliberately no captive-portal DNS: macOS would open the page
// in its captive-network sheet, which cannot download files.
class WebPortal {
  public:
    static constexpr const char *kSsid = "Voice Recorder";

    // What main.cpp provides. record/stop return false and set `reason`
    // when they cannot be done.
    struct Hooks {
        std::function<bool()> recording;
        std::function<String()> statusJson;
        std::function<bool(String &reason)> record;
        std::function<bool(String &reason)> stop;
    };

    WebPortal(Storage &storage, Hooks hooks) : storage_(storage), hooks_(hooks) {}

    void start();
    void stop();
    void loop();

    bool active() const { return active_; }
    uint8_t stations() const;

    // Whether a request came in since the last call; keeps the screen on.
    bool takeActivity();

  private:
    void handlePage();
    void handleStatus();
    void handleRecord();
    void handleStop();
    void handleList();
    void handleDownload();
    void handleDelete();
    void handleDeleteAll();

    // Answers 409 and returns true while a recording is running.
    bool refuseWhileRecording();

    Storage &storage_;
    Hooks hooks_;
    WebServer server_{80};
    bool active_ = false;
    bool activity_ = false;
    bool routesAdded_ = false;
};
