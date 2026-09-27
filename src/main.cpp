#include <M5Unified.h>
#include <LittleFS.h>

#include "AudioCapture.h"
#include "BatteryFilter.h"
#include "LevelMeter.h"
#include "RecorderState.h"
#include "RecordingWriter.h"
#include "Renderer.h"
#include "ScreenPolicy.h"
#include "Storage.h"
#include "WavHeader.h"
#include "WebPortal.h"

namespace {

constexpr uint8_t kBrightness = 120;
constexpr uint8_t kDimBrightness = 16;
constexpr uint32_t kBatteryPeriodMs = 5000;
constexpr uint32_t kStationsPeriodMs = 1000;

// The web page's requests; defined further down.
String statusJson();
bool webRecord(String &reason);
bool webStop(String &reason);

AudioCapture audio;
Storage storage;
RecordingWriter writer;
WebPortal portal(storage, WebPortal::Hooks{
                              [] { return writer.active(); },
                              [] { return statusJson(); },
                              [](String &reason) { return webRecord(reason); },
                              [](String &reason) { return webStop(reason); },
                          });
Renderer renderer;
LevelMeter meter;
RecorderState state;
ScreenPolicy screenPolicy;

Backlight backlight = Backlight::On;
BatteryFilter battery;
uint32_t lastBatteryMs = 0;
uint8_t stations = 0;
uint32_t lastStationsMs = 0;

// Recording in progress.
uint32_t recordingStartMs = 0;
uint32_t recordingBudget = 0;

// The last finished recording, for screen 3.
struct {
    char name[rec::kNameSize] = "";
    uint32_t seconds = 0;
    uint32_t bytes = 0;
} saved;

// Test hooks over Serial (see "Self-testing on the board" in CLAUDE.md).
bool serialKey1 = false;
bool serialKey2 = false;
// "v <n>" shows screen n for a few seconds, to check screens that
// otherwise need a fault, the first start or a full memory (6, 7, 9).
ScreenId previewScreen = ScreenId::Ready;
uint32_t previewUntilMs = 0;
char command[48];
size_t commandLen = 0;

// Once a second while recording: proves the capture keeps up.
struct {
    uint32_t sinceMs = 0;
    uint32_t blocks = 0;
} stats;

void setBacklight(Backlight next) {
    if (next == backlight) {
        return;
    }
    if (backlight == Backlight::Off) {
        M5.Display.wakeup();
        // The panel's contents are lost during sleep.
        renderer.invalidate();
    }
    switch (next) {
    case Backlight::On:
        M5.Display.setBrightness(kBrightness);
        break;
    case Backlight::Dim:
        M5.Display.setBrightness(kDimBrightness);
        break;
    case Backlight::Off:
        // The backlight is the main power consumer, so it is turned off
        // separately from putting the panel itself to sleep.
        M5.Display.setBrightness(0);
        M5.Display.sleep();
        break;
    }
    backlight = next;
}

void readBattery(uint32_t now, bool force) {
    if (!force && now - lastBatteryMs < kBatteryPeriodMs) {
        return;
    }
    lastBatteryMs = now;
    const int32_t level = M5.Power.getBatteryLevel();
    battery.update(static_cast<uint8_t>(level < 0 ? 0 : (level > 100 ? 100 : level)));
}

void startRecording(uint32_t now) {
    char name[rec::kNameSize];
    storage.nextName(name);
    if (!writer.start(name, storage.recordableBytes(), now)) {
        Serial.printf("rec: cannot open %s\n", name);
        state.cancelRecording(now);
        return;
    }
    recordingStartMs = now;
    recordingBudget = storage.recordableBytes();
    meter.clear();
    stats.sinceMs = now;
    stats.blocks = audio.deliveredBlocks();
    Serial.printf("rec: start %s, room for %u s\n", name,
                  static_cast<unsigned>(wav::secondsForFreeBytes(recordingBudget)));
}

void stopRecording() {
    const uint32_t start = millis();
    writer.stop(); // 0.1-0.25 s: the header rewrite copies a flash block
    const uint32_t closed = millis();
    strlcpy(saved.name, writer.name(), sizeof saved.name);
    saved.seconds = wav::secondsForDataBytes(writer.dataBytes());
    saved.bytes = wav::kHeaderBytes + writer.dataBytes();
    storage.noteAdded(saved.name, saved.bytes);
    Serial.printf("rec: stop %s, %u s, %u bytes, lost blocks %u (close %u ms)\n", saved.name,
                  static_cast<unsigned>(saved.seconds), static_cast<unsigned>(saved.bytes),
                  static_cast<unsigned>(audio.lostBlocks()),
                  static_cast<unsigned>(closed - start));
}

void execute(Command command, uint32_t now) {
    switch (command) {
    case Command::None:
        break;
    case Command::StartRecording:
        startRecording(now);
        break;
    case Command::StopRecording:
        stopRecording();
        break;
    case Command::WifiOn:
        portal.start();
        Serial.println("wifi: on");
        break;
    case Command::WifiOff:
        portal.stop();
        storage.refresh();
        Serial.println("wifi: off");
        break;
    }
}

ScreenId screenFor(View view) {
    const bool room = storage.roomForRecording();
    switch (view.mode) {
    case Mode::Recording:
        return view.wifi ? ScreenId::RecordingWifi : ScreenId::Recording;
    case Mode::Saved:
        return ScreenId::Saved;
    case Mode::Ready:
    default:
        if (view.wifi) {
            return room ? ScreenId::Wifi : ScreenId::WifiFull;
        }
        return room ? ScreenId::Ready : ScreenId::Full;
    }
}

void buildModel(ScreenModel &m, uint32_t now) {
    memset(&m, 0, sizeof m);
    m.screen = screenFor(state.view());
    m.previous = screenFor(state.previous());
    m.wifi = state.wifi();
    if (static_cast<int32_t>(previewUntilMs - now) > 0) {
        m.screen = m.previous = previewScreen;
        m.wifi = previewScreen == ScreenId::Wifi || previewScreen == ScreenId::RecordingWifi ||
                 previewScreen == ScreenId::WifiFull;
    }
    const Fade fade = state.fade(now);
    m.showPrevious = fade.showPrevious;
    m.fadeLevel = fade.level;

    m.batteryPercent = battery.shown();

    m.freeBytes = storage.freeBytes();
    m.totalBytes = storage.totalBytes();
    m.recordingCount = storage.recordings().size();
    m.recordingsBytes = storage.recordingsBytes();

    if (writer.active()) {
        const uint32_t written = writer.dataBytes();
        m.secondsLeft = wav::secondsForFreeBytes(
            recordingBudget > written + wav::kHeaderBytes ? recordingBudget - written - wav::kHeaderBytes : 0);
        m.elapsedSeconds = wav::secondsForDataBytes(written);
        m.recordedBytes = wav::kHeaderBytes + written;
        m.meterVersion = meter.version();
        m.ringPhase = static_cast<uint8_t>((now - recordingStartMs) % Renderer::kRingPeriodMs /
                                           (Renderer::kRingPeriodMs / Renderer::kRingSteps));
    } else {
        m.secondsLeft = wav::secondsForFreeBytes(storage.recordableBytes());
    }

    strlcpy(m.savedName, saved.name, sizeof m.savedName);
    m.savedSeconds = saved.seconds;
    m.savedBytes = saved.bytes;
    m.savedFull = state.stoppedFull();

    if (state.wifi() || state.previous().wifi) {
        m.stations = stations;
    }
}

// ---- The web page ----------------------------------------------------------

uint32_t secondsLeftNow() {
    if (writer.active()) {
        const uint32_t used = writer.dataBytes() + wav::kHeaderBytes;
        return wav::secondsForFreeBytes(recordingBudget > used ? recordingBudget - used : 0);
    }
    return wav::secondsForFreeBytes(storage.recordableBytes());
}

// {state, name, seconds, bytes, secondsLeft, canRecord, stoppedFull[, levels]}
// name/seconds/bytes: the recording in progress, or else the last one saved.
// Cheap on purpose: the page asks for it every 400 ms while recording.
String statusJson() {
    const Mode mode = state.mode();
    const bool rec = writer.active();
    String j;
    j.reserve(rec ? 300 : 200);
    j += "{\"state\":\"";
    j += mode == Mode::Recording ? "recording" : (mode == Mode::Saved ? "saved" : "ready");
    j += "\",\"name\":\"";
    j += rec ? writer.name() : saved.name;
    j += "\",\"seconds\":";
    j += rec ? wav::secondsForDataBytes(writer.dataBytes()) : saved.seconds;
    j += ",\"bytes\":";
    j += rec ? wav::kHeaderBytes + writer.dataBytes() : saved.bytes;
    j += ",\"secondsLeft\":";
    j += secondsLeftNow();
    j += ",\"canRecord\":";
    j += (!rec && storage.roomForRecording()) ? "true" : "false";
    j += ",\"stoppedFull\":";
    j += state.stoppedFull() ? "true" : "false";
    if (rec) {
        char hex[2 * LevelMeter::kBars + 1];
        meter.toHex(hex);
        j += ",\"levels\":\"";
        j += hex;
        j += '"';
    }
    j += '}';
    return j;
}

// Record and Stop from the page go through the same state machine as the
// keys. The handlers run inside loop(), so calling it here is safe.
bool webRecord(String &reason) {
    const uint32_t now = millis();
    if (writer.active()) {
        reason = "Already recording";
        return false;
    }
    if (!storage.roomForRecording()) {
        reason = "Memory full";
        return false;
    }
    if (!state.wifi()) {
        reason = "Wi-Fi mode is off";
        return false;
    }
    Input in;
    in.webRecord = true;
    execute(state.update(now, in), now);
    if (!writer.active()) {
        reason = "The recording could not start";
        return false;
    }
    return true;
}

bool webStop(String &reason) {
    const uint32_t now = millis();
    if (!writer.active()) {
        reason = "Not recording";
        return false;
    }
    Input in;
    in.webStop = true;
    in.roomLeft = writer.roomLeft();
    execute(state.update(now, in), now);
    return true;
}

// ---- Serial test hooks -----------------------------------------------------

void sendFile(const char *name) {
    if (writer.active()) {
        Serial.println("ERR recording");
        return;
    }
    if (!rec::isRecordingName(name)) {
        Serial.println("ERR name");
        return;
    }
    File f = LittleFS.open(String("/") + name, "r");
    if (!f) {
        Serial.println("ERR missing");
        return;
    }
    Serial.printf("FILE %s %u\n", name, static_cast<unsigned>(f.size()));
    static uint8_t buf[2048];
    for (size_t n; (n = f.read(buf, sizeof buf)) > 0;) {
        Serial.write(buf, n);
    }
    Serial.flush();
    f.close();
}

void listFiles() {
    storage.refresh();
    Serial.printf("LIST %u free %u total %u\n", static_cast<unsigned>(storage.recordings().size()),
                  static_cast<unsigned>(storage.freeBytes()), static_cast<unsigned>(storage.totalBytes()));
    for (const Storage::Entry &e : storage.recordings()) {
        Serial.printf("%s %u\n", e.name, static_cast<unsigned>(e.bytes));
    }
    Serial.println("END");
}

// Test hook: takes up storage with /filler.bin so screen 5 can be checked
// without recording for 20 minutes. "fill 0" removes it.
void fill(uint32_t kilobytes) {
    if (state.mode() != Mode::Ready) {
        Serial.println("ERR busy");
        return;
    }
    LittleFS.remove("/filler.bin");
    if (kilobytes > 0) {
        File f = LittleFS.open("/filler.bin", "w");
        static uint8_t zeros[4096];
        for (uint32_t i = 0; f && i < kilobytes / 4; ++i) {
            if (f.write(zeros, sizeof zeros) != sizeof zeros) {
                break;
            }
        }
        f.close();
    }
    storage.refresh();
    Serial.printf("FILL %u bytes free\n", static_cast<unsigned>(storage.freeBytes()));
}

void runCommand(const char *line) {
    if (strcmp(line, "r") == 0) {
        serialKey1 = true;
    } else if (strcmp(line, "w") == 0) {
        serialKey2 = true;
    } else if (strcmp(line, "s") == 0) {
        renderer.writeSnapshot(Serial);
    } else if (strcmp(line, "l") == 0) {
        listFiles();
    } else if (strncmp(line, "g ", 2) == 0) {
        sendFile(line + 2);
    } else if (strcmp(line, "wr") == 0 || strcmp(line, "ws") == 0) {
        // Web Record / web Stop without joining the network.
        String reason;
        const bool ok = line[1] == 'r' ? webRecord(reason) : webStop(reason);
        Serial.printf("WEB %s\n", ok ? "ok" : reason.c_str());
    } else if (strncmp(line, "v ", 2) == 0) {
        const int n = atoi(line + 2);
        if (n >= 1 && n <= 9) {
            previewScreen = static_cast<ScreenId>(n);
            previewUntilMs = millis() + 3000;
        }
    } else if (strncmp(line, "fill ", 5) == 0) {
        fill(static_cast<uint32_t>(atol(line + 5)));
    } else if (line[0] != '\0') {
        Serial.printf("ERR unknown '%s'\n", line);
    }
}

void pollSerial() {
    while (Serial.available() > 0) {
        const int c = Serial.read();
        if (c == '\n' || c == '\r') {
            command[commandLen] = '\0';
            runCommand(command);
            commandLen = 0;
        } else if (commandLen + 1 < sizeof command) {
            command[commandLen++] = static_cast<char>(c);
        }
    }
}

void reportStats(uint32_t now) {
    if (!writer.active() || now - stats.sinceMs < 1000) {
        return;
    }
    const uint32_t blocks = audio.deliveredBlocks() - stats.blocks;
    const uint32_t sps = blocks * AudioCapture::kBlockLen * 1000 / (now - stats.sinceMs);
    Serial.printf("rec sps %u  write max %u ms  lost %u  data %u  left %u s\n",
                  static_cast<unsigned>(sps), static_cast<unsigned>(writer.takeMaxWriteMs()),
                  static_cast<unsigned>(audio.lostBlocks()),
                  static_cast<unsigned>(writer.dataBytes()),
                  static_cast<unsigned>(wav::secondsForFreeBytes(
                      recordingBudget > writer.dataBytes() ? recordingBudget - writer.dataBytes() : 0)));
    stats.sinceMs = now;
    stats.blocks = audio.deliveredBlocks();
}

void haltWith(ScreenId screen) {
    ScreenModel m;
    memset(&m, 0, sizeof m);
    m.screen = screen;
    m.fadeLevel = 255;
    m.batteryPercent = battery.shown();
    renderer.draw(m, meter);
}

} // namespace

void setup() {
    auto cfg = M5.config();
    // The speaker sits on the same I2S lines as the mic.
    cfg.internal_spk = false;
    M5.begin(cfg);
    Serial.begin(115200);

    M5.Display.setBrightness(kBrightness);
    renderer.begin();
    readBattery(millis(), true);

    if (!storage.mount()) {
        haltWith(ScreenId::Preparing); // screen 6
        Serial.println("storage: formatting");
        storage.format();
    }
    storage.repairAll();
    storage.refresh();
    Serial.printf("storage: %u recordings, %u of %u bytes free\n",
                  static_cast<unsigned>(storage.recordings().size()),
                  static_cast<unsigned>(storage.freeBytes()),
                  static_cast<unsigned>(storage.totalBytes()));

    if (!audio.begin()) {
        haltWith(ScreenId::NoMic); // screen 7
        for (;;) {
            delay(1000);
        }
    }

    state.begin(millis());
    screenPolicy.begin(millis());
}

void loop() {
    M5.update();
    const uint32_t now = millis();

    serialKey1 = serialKey2 = false;
    pollSerial();

    // The mic is always drained, even when not recording: otherwise its
    // queue runs dry and the codec keeps stale audio for the next start.
    while (const int16_t *block = audio.next()) {
        if (!writer.active()) {
            continue;
        }
        for (size_t i = 0; i < AudioCapture::kAdpcmBlocksPerCapture; ++i) {
            const int16_t *part = block + i * ima::kSamplesPerBlock;
            writer.write(part, now);
            meter.push(part, ima::kSamplesPerBlock);
        }
    }

    const bool press1 = M5.BtnA.wasPressed();
    const bool press2 = M5.BtnB.wasPressed();
    // A press while the screen is off only wakes it. Serial keys are test
    // hooks and always act.
    const bool awake = backlight != Backlight::Off;
    const bool key1 = (awake && press1) || serialKey1;
    const bool key2 = (awake && press2) || serialKey2;

    Input in;
    in.key1 = key1;
    in.key2 = key2;
    in.roomLeft = writer.active() ? writer.roomLeft() : storage.roomForRecording();
    execute(state.update(now, in), now);

    const bool activity = press1 || press2 || serialKey1 || serialKey2 || portal.takeActivity();
    setBacklight(screenPolicy.update(now, activity, state.mode() == Mode::Recording));

    readBattery(now, false);
    if (portal.active() && now - lastStationsMs >= kStationsPeriodMs) {
        lastStationsMs = now;
        stations = portal.stations();
    }

    if (backlight != Backlight::Off) {
        ScreenModel model;
        buildModel(model, now);
        renderer.draw(model, meter);
    }

    portal.loop();
    reportStats(now);

    // Power-off is not handled here: a double press of the power button
    // does it on its own through the PMIC.
    delay(writer.active() || portal.active() ? 2 : 10);
}
