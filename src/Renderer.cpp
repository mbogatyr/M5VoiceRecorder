#include "Renderer.h"

#include <math.h>
#include <string.h>

namespace {

// Palette of the approved mockups, RGB888.
constexpr uint32_t kWhite = 0xFFFFFF;
constexpr uint32_t kGray = 0x8E8E93;
constexpr uint32_t kTrack = 0x2C2C2E;
constexpr uint32_t kDarkCircle = 0x3A3A3C;
constexpr uint32_t kRed = 0xFF453A;
constexpr uint32_t kCyan = 0x64D2FF;
constexpr uint32_t kGreen = 0x30D158;
constexpr uint32_t kAmber = 0xFFD60A;
constexpr uint32_t kBlue = 0x0A84FF;

// M5GFX fonts are ASCII only: the middle dot, arrows and icons are drawn.
const lgfx::IFont *const kTiny = &fonts::Font0;
const lgfx::IFont *const kSmall = &fonts::DejaVu9;
const lgfx::IFont *const kLabel = &fonts::DejaVu12;
const lgfx::IFont *const kIp = &fonts::FreeSansBold9pt7b;
const lgfx::IFont *const kBig = &fonts::FreeSansBold18pt7b;

// The KEY1 zone: right of x = 168, centred on the button.
constexpr int kKey1X = 196;
constexpr int kKey1Y = 62;
constexpr int kKey1LabelBaseline = 96;
// The KEY2 hint: centred under the button, which spans x 142..232.
constexpr int kKey2X = 187;

void formatTenths(uint32_t bytes, char *out, size_t size) {
    const unsigned tenths = (bytes + 50000u) / 100000u;
    snprintf(out, size, "%u.%u", tenths / 10, tenths % 10);
}

} // namespace

void Renderer::begin() {
    M5.Display.setRotation(1); // landscape, KEY1 to the right of the screen
    M5.Display.fillScreen(TFT_BLACK);

    canvas_.setColorDepth(16);
    canvas_.setPsram(true); // 240*135*2 = 65 KB; the StickS3 has 8 MB of PSRAM
    canvas_.createSprite(M5.Display.width(), M5.Display.height());
}

void Renderer::draw(const ScreenModel &model, const LevelMeter &meter) {
    const bool unchanged =
        hasPrevious_ && memcmp(&previousModel_, &model, sizeof model) == 0;
    if (unchanged) {
        return;
    }
    paint(model, meter);
    previousModel_ = model;
    hasPrevious_ = true;
}

void Renderer::writeSnapshot(Print &out) {
    out.printf("SNAP %d %d\n", canvas_.width(), canvas_.height());
    // A 16-bit LovyanGFX sprite already keeps its pixels byte-swapped for
    // SPI, i.e. high byte first, so the buffer goes out as it is.
    out.write(static_cast<const uint8_t *>(canvas_.getBuffer()),
              canvas_.width() * canvas_.height() * 2);
    out.flush();
}

void Renderer::paint(const ScreenModel &m, const LevelMeter &meter) {
    canvas_.fillSprite(TFT_BLACK);

    // The battery stays put through fades: it is on every screen.
    level_ = 255;
    paintBattery(m);

    level_ = m.fadeLevel;
    switch (m.showPrevious ? m.previous : m.screen) {
    case ScreenId::Ready:
        paintReady(m, false);
        break;
    case ScreenId::Full:
        paintReady(m, true);
        break;
    case ScreenId::Recording:
    case ScreenId::RecordingWifi:
        paintRecording(m, meter);
        break;
    case ScreenId::Saved:
        paintSaved(m);
        break;
    case ScreenId::Wifi:
        paintWifi(m, false);
        break;
    case ScreenId::WifiFull:
        paintWifi(m, true);
        break;
    case ScreenId::Preparing:
        paintMessage(false);
        break;
    case ScreenId::NoMic:
        paintMessage(true);
        break;
    }

    canvas_.pushSprite(0, 0);
}

// ---- helpers ---------------------------------------------------------------

uint16_t Renderer::colScaled(uint32_t rgb, uint16_t scale) const {
    const uint32_t s = static_cast<uint32_t>(scale) * (level_ + 1) / 256;
    const uint8_t r = ((rgb >> 16) & 0xFF) * s / 256;
    const uint8_t g = ((rgb >> 8) & 0xFF) * s / 256;
    const uint8_t b = (rgb & 0xFF) * s / 256;
    return canvas_.color565(r, g, b);
}

uint16_t Renderer::col(uint32_t rgb) const { return colScaled(rgb, 256); }

int Renderer::width(const char *s, const lgfx::IFont *font, bool bold) {
    return canvas_.textWidth(s, font) + (bold ? 1 : 0);
}

int Renderer::text(const char *s, int x, int y, const lgfx::IFont *font, uint32_t rgb,
                   textdatum_t datum, bool bold) {
    canvas_.setFont(font);
    canvas_.setTextDatum(datum);
    canvas_.setTextColor(col(rgb));
    canvas_.drawString(s, x, y);
    if (bold) {
        canvas_.drawString(s, x + 1, y);
    }
    return width(s, font, bold);
}

int Renderer::separator(int x, int baseline, int height, uint32_t rgb) {
    canvas_.fillCircle(x + 4, baseline - height / 2, 1, col(rgb));
    return 9;
}

void Renderer::bar(int x, int y, int w, int h, uint32_t filled, uint32_t total,
                   uint32_t rgb) {
    const int r = h / 2;
    canvas_.fillSmoothRoundRect(x, y, w, h, r, col(kTrack));
    if (total == 0 || filled == 0) {
        return;
    }
    const uint64_t f = filled > total ? total : filled;
    const int fw = static_cast<int>((w * f + total / 2) / total);
    if (fw > 0) {
        canvas_.fillSmoothRoundRect(x, y, fw < h ? h : fw, h, r, col(rgb));
    }
}

void Renderer::paintKey1Arrow() {
    canvas_.fillTriangle(233, kKey1Y - 4, 233, kKey1Y + 4, 238, kKey1Y, col(kBlue));
}

void Renderer::paintKey2Hint(const char *label) {
    const int tw = width(label, kLabel, true);
    const int total = 7 + 4 + tw;
    const int x = kKey2X - total / 2;
    canvas_.fillTriangle(x, 11, x + 6, 11, x + 3, 5, col(kCyan));
    text(label, x + 11, 13, kLabel, kCyan, baseline_left, true);
}

void Renderer::micIcon(int cx, int cy, uint32_t rgb, bool crossed) {
    const uint16_t c = col(rgb);
    canvas_.fillSmoothRoundRect(cx - 5, cy - 16, 11, 19, 5, c);
    canvas_.fillArc(cx, cy - 4, 8, 10, 0, 180, c);
    canvas_.fillRect(cx - 1, cy + 6, 3, 6, c);
    canvas_.fillRect(cx - 6, cy + 11, 13, 2, c);
    if (crossed) {
        // A black cut first, so the slash stands out against the icon.
        canvas_.drawWideLine(cx - 12, cy - 17, cx + 12, cy + 12, 3.0f, TFT_BLACK);
        canvas_.drawWideLine(cx - 12, cy - 17, cx + 12, cy + 12, 1.2f, c);
    }
}

// ---- screens ---------------------------------------------------------------

void Renderer::paintBattery(const ScreenModel &m) {
    const uint16_t outline = col(kGray);
    canvas_.drawRoundRect(6, 5, 15, 7, 2, outline);
    canvas_.fillRect(21, 7, 2, 3, outline);

    const uint32_t fill = m.batteryPercent > 20 ? kGreen
                          : m.batteryPercent > 10 ? kAmber
                                                  : kRed;
    const int w = (m.batteryPercent * 13 + 50) / 100;
    if (w > 0) {
        canvas_.fillRect(7, 6, w, 5, col(fill));
    }

    char pct[8];
    snprintf(pct, sizeof pct, "%u%%", m.batteryPercent);
    const int pw = text(pct, 25, 4, kTiny, kGray);

    if (m.wifi) {
        // Wi-Fi mark: a dot and two arcs, 13 px wide, right of the %.
        const uint16_t cyan = col(kCyan);
        const int cx = 25 + pw + 10;
        canvas_.fillCircle(cx, 11, 1, cyan);
        canvas_.fillArc(cx, 11, 3, 4, 225, 315, cyan);
        canvas_.fillArc(cx, 11, 6, 7, 225, 315, cyan);
    }
}

// Screen 1, or screen 5 when the memory is full.
void Renderer::paintReady(const ScreenModel &m, bool full) {
    paintKey2Hint("Wi-Fi");

    const uint32_t accent = full ? kAmber : kGray;
    char line[40];
    int x = 10;
    if (full) {
        x += text("Memory full", x, 35, kSmall, kAmber, baseline_left, true);
    } else {
        x += text("Ready", x, 35, kSmall, kGray, baseline_left);
    }
    x += separator(x, 35, 7, accent);
    if (m.recordingCount == 0) {
        strcpy(line, "no recordings");
    } else {
        snprintf(line, sizeof line, "%u recording%s", static_cast<unsigned>(m.recordingCount),
                 m.recordingCount == 1 ? "" : "s");
    }
    text(line, x, 35, kSmall, accent, baseline_left, full);

    char time[12];
    rec::formatDuration(full ? 0 : m.secondsLeft, time, sizeof time);
    const int tw = text(time, 8, 72, kBig, full ? kAmber : kWhite, baseline_left);
    text("min", 8 + tw + 6, 60, kSmall, kGray, baseline_left);
    text("left", 8 + tw + 6, 71, kSmall, kGray, baseline_left);

    const uint32_t used = m.totalBytes > m.freeBytes ? m.totalBytes - m.freeBytes : 0;
    bar(10, 94, 140, 5, full ? m.totalBytes : used, m.totalBytes, full ? kAmber : kGray);

    if (full) {
        text("Download and delete via Wi-Fi", 10, 114, kSmall, kGray, baseline_left);
        canvas_.fillSmoothCircle(kKey1X, kKey1Y, 16, col(kDarkCircle));
        text("Full", kKey1X, kKey1LabelBaseline, kLabel, kGray, baseline_center, true);
        return;
    }

    char freeMb[12], totalMb[12];
    formatTenths(m.freeBytes, freeMb, sizeof freeMb);
    formatTenths(m.totalBytes, totalMb, sizeof totalMb);
    snprintf(line, sizeof line, "%s of %s MB free", freeMb, totalMb);
    text(line, 10, 114, kSmall, kGray, baseline_left);

    canvas_.fillSmoothCircle(kKey1X, kKey1Y, 16, col(kRed));
    text("Record", kKey1X, kKey1LabelBaseline, kLabel, kWhite, baseline_center, true);
    paintKey1Arrow();
}

// Screen 2. No "REC" label: the pulsing ring around Stop says it.
void Renderer::paintRecording(const ScreenModel &m, const LevelMeter &meter) {
    char s[16];
    rec::formatDuration(m.elapsedSeconds, s, sizeof s);
    const int tw = text(s, 8, 45, kBig, kWhite, baseline_left);

    const int infoX = (8 + tw + 8) > 104 ? 8 + tw + 8 : 104;
    rec::formatMegabytes(m.recordedBytes, s, sizeof s);
    text(s, infoX, 32, kLabel, kWhite, baseline_left, true);
    char left[16];
    rec::formatDuration(m.secondsLeft, left, sizeof left);
    snprintf(s, sizeof s, "%s left", left);
    text(s, infoX, 45, kSmall, kGray, baseline_left);

    // Waveform: 54 bars, the newest on the right, older ones fainter.
    constexpr int kCenterY = 80;
    constexpr int kMaxH = 40;
    for (size_t i = 0; i < LevelMeter::kBars; ++i) {
        const int h = meter.bar(i) * kMaxH / 255;
        const int bh = h < 2 ? 2 : h;
        const uint16_t fade = static_cast<uint16_t>(90 + 166 * i / (LevelMeter::kBars - 1));
        canvas_.fillRect(8 + static_cast<int>(i) * 3, kCenterY - bh / 2, 2, bh,
                         colScaled(kRed, fade));
    }

    const uint32_t used = m.totalBytes > m.freeBytes ? m.totalBytes - m.freeBytes : 0;
    bar(8, 112, 156, 3, used + m.recordedBytes, m.totalBytes, kRed);

    // Pulsing ring: grows from 0.6x to 1.6x of 15 px and fades out, eased.
    const float t = static_cast<float>(m.ringPhase) / kRingSteps;
    const float eased = 1.0f - (1.0f - t) * (1.0f - t);
    const int r = static_cast<int>(lroundf(15.0f * (0.6f + eased)));
    const uint16_t alpha = static_cast<uint16_t>(230.0f * (1.0f - eased));
    if (alpha > 0 && r > 2) {
        canvas_.fillArc(kKey1X, kKey1Y, r - 2, r, 0, 360, colScaled(kRed, alpha));
    }
    canvas_.fillSmoothRoundRect(kKey1X - 6, kKey1Y - 6, 12, 12, 2, col(kWhite));
    text("Stop", kKey1X, kKey1LabelBaseline, kLabel, kWhite, baseline_center, true);
    paintKey1Arrow();
}

// Screen 3: a summary centred across the whole width, no button hints.
void Renderer::paintSaved(const ScreenModel &m) {
    constexpr int kCx = 120;

    const int lw = width("Saved", kLabel, true);
    const int x0 = kCx - (14 + 4 + lw) / 2;
    const uint16_t green = col(kGreen);
    canvas_.drawCircle(x0 + 7, 27, 6, green);
    canvas_.drawCircle(x0 + 7, 27, 5, green);
    canvas_.drawWideLine(x0 + 4, 27, x0 + 6, 29, 0.7f, green);
    canvas_.drawWideLine(x0 + 6, 29, x0 + 10, 25, 0.7f, green);
    text("Saved", x0 + 18, 31, kLabel, kGreen, baseline_left, true);

    char upper[rec::kNameSize];
    strlcpy(upper, m.savedName, sizeof upper);
    for (char *p = upper; *p; ++p) {
        if (*p >= 'a' && *p <= 'z') {
            *p = static_cast<char>(*p - 'a' + 'A');
        }
    }
    text(upper, kCx, 44, kSmall, kGray, baseline_center);

    char s[24];
    rec::formatDuration(m.savedSeconds, s, sizeof s);
    text(s, kCx, 82, kBig, kWhite, baseline_center);

    char size[16], rest[24];
    rec::formatMegabytes(m.savedBytes, size, sizeof size);
    if (m.savedFull) {
        strcpy(rest, "Memory full");
    } else {
        char left[12];
        rec::formatDuration(m.secondsLeft, left, sizeof left);
        snprintf(rest, sizeof rest, "%s min left", left);
    }
    const int w1 = width(size, kLabel, true);
    const int w2 = width(rest, kSmall);
    int x = kCx - (w1 + 9 + w2) / 2;
    x += text(size, x, 103, kLabel, kWhite, baseline_left, true);
    x += separator(x, 103, 7, kGray);
    text(rest, x, 103, kSmall, m.savedFull ? kAmber : kGray, baseline_left);
}

// Screen 4, or 9 when the memory is full. There is no KEY1 zone: in Wi-Fi
// mode recording starts from the web page.
void Renderer::paintWifi(const ScreenModel &m, bool full) {
    paintKey2Hint("Exit");

    // Wi-Fi icon: three arcs opening upwards over a dot.
    const uint16_t cyan = col(kCyan);
    constexpr int kIx = 28, kIy = 62;
    canvas_.fillSmoothCircle(kIx, kIy, 2, cyan);
    canvas_.fillArc(kIx, kIy, 6, 9, 225, 315, cyan);
    canvas_.fillArc(kIx, kIy, 12, 15, 225, 315, cyan);
    canvas_.fillArc(kIx, kIy, 18, 21, 225, 315, cyan);

    // Under the icon there is room for about 9 characters of Font0 before
    // the right column starts at x = 66.
    char s[32];
    const uint32_t dot = m.stations > 0 ? kGreen : kGray;
    canvas_.fillCircle(7, 83, 2, col(dot));
    if (m.stations == 0) {
        strcpy(s, "waiting");
    } else {
        snprintf(s, sizeof s, "%u device%s", m.stations, m.stations == 1 ? "" : "s");
    }
    text(s, 12, 80, kTiny, dot);

    text("Join network", 66, 32, kSmall, kGray, baseline_left);
    text("Voice Recorder", 66, 46, kLabel, kWhite, baseline_left, true);
    text("no password", 66, 60, kSmall, kGray, baseline_left);
    text("Open in browser", 66, 80, kSmall, kGray, baseline_left);
    text("192.168.4.1", 66, 98, kIp, kCyan, baseline_left);

    snprintf(s, sizeof s, "%u recording%s", static_cast<unsigned>(m.recordingCount),
             m.recordingCount == 1 ? "" : "s");
    if (full) {
        int x = 66 + text("Memory full", 66, 118, kSmall, kAmber, baseline_left, true);
        x += separator(x, 118, 7, kAmber);
        text(s, x, 118, kSmall, kAmber, baseline_left, true);
        return;
    }
    int x = 66 + text(s, 66, 118, kSmall, kGray, baseline_left);
    x += separator(x, 118, 7, kGray);
    char size[16];
    rec::formatMegabytes(m.recordingsBytes, size, sizeof size);
    text(size, x, 118, kSmall, kGray, baseline_left);
}

// Screens 6 and 7.
void Renderer::paintMessage(bool error) {
    micIcon(120, 50, error ? kRed : kGray, error);
    text(error ? "No microphone" : "Preparing storage", 120, 88, kLabel, kWhite,
         baseline_center, true);
    text(error ? "restart with the power button" : "first start only, a few seconds", 120,
         102, kSmall, kGray, baseline_center);
}
