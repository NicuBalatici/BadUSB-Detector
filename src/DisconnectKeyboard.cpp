#include "DisconnectKeyboard.h"
#include "UsbUtils.h"
#include "BadUSBDetector.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <fstream>
#include <map>
#include <vector>
#include <ApplicationServices/ApplicationServices.h>
#include <unistd.h>

using namespace std;
using namespace std::chrono;

constexpr int WINDOW_SIZE = 5;
constexpr double THRESHOLD = 0.75;
constexpr int SILENCE_TIMEOUT_SECONDS = 5;

atomic<bool> INPUT_BLOCKED{false};
using Clock = steady_clock;

static CFMachPortRef gTap = nullptr;

map<CGKeyCode, Clock::time_point> key_press_starts;
map<CGKeyCode, Clock::time_point> key_release_ends;
bool first_key = true;
Clock::time_point last_key_release_time;
Clock::time_point t_last_threat_activity = Clock::now();
string captured_payload;

struct KeyData {
    float flight;
    float inter;
    float hold;
};
vector<KeyData> window_buffer;

BadUSBDetector* ai_agent = nullptr;

void showPopup(const string& message) {
    system("killall osascript > /dev/null 2>&1");
    string command = "osascript -e 'display alert \"⚠️ THREAT ISOLATED\" message \"" + message +
                     "\" as critical buttons {\"Locked\"} default button \"Locked\"' &";
    system(command.c_str());
}

void saveForensics() {
    if (captured_payload.empty()) return;
    ofstream out("forensic_log.txt", ios::app);
    if (out.is_open()) {
        out << "\n===== ATTACK ISOLATED =====\n" << captured_payload << "\n";
        out.close();
    }
    captured_payload.clear();
}

void activateLockdown(const string& source, float threat_score) {
    if (!INPUT_BLOCKED.load()) {
        INPUT_BLOCKED.store(true);
        cout << "\n[!!!] " << source << " TRIGGERED LOCKDOWN [!!!]\n";
        cout << "Threat Confidence: " << (threat_score * 100) << "%\n";
        saveForensics();

        system("diskutil eject /Volumes/* > /dev/null 2>&1");

        showPopup("Input has been paralyzed. Threat Score: " + to_string(threat_score) + "\n."
                  "System will unlock after " + to_string(SILENCE_TIMEOUT_SECONDS) + " seconds of silence.");
    }
}

void process_window() {
    if (window_buffer.size() < WINDOW_SIZE) return;

    int malicious_keystroke_count = 0;

    for (const auto& k : window_buffer) {
        float prob = ai_agent->predict(k.flight, k.inter, k.hold);

        if (prob > THRESHOLD) {
            malicious_keystroke_count++;
        }
    }

    if (malicious_keystroke_count >= 4) {
        float confidence = (float)malicious_keystroke_count / WINDOW_SIZE;
        activateLockdown("XGBoost AI Algorithm", confidence);
        window_buffer.clear();
    } else {
        window_buffer.erase(window_buffer.begin());
    }
}

CGEventRef eventCallbackDisconnect(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void*) {

    if (type == kCGEventTapDisabledByTimeout) {
        if (gTap) CGEventTapEnable(gTap, true);
        return event;
    }

    auto now = Clock::now();

    if (INPUT_BLOCKED.load()) {
        t_last_threat_activity = now;

        if (type == kCGEventKeyDown) {
            UniChar chars[4]; UniCharCount len;
            CGEventKeyboardGetUnicodeString(event, 4, &len, chars);
            if (len > 0) {
                 ofstream q("quarantine_log.txt", ios::out | ios::app);
                 if (q.is_open()) {
                    char c = (char)chars[0];
                    if (c == 13) q << "\n"; else q << c;
                 }
            }
        }
        return nullptr;
    }

    if (type != kCGEventKeyDown && type != kCGEventKeyUp) return event;
    if (CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0) return event;

    CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

    if (type == kCGEventKeyDown) {
        UniChar chars[4]; UniCharCount len;
        CGEventKeyboardGetUnicodeString(event, 4, &len, chars);
        if (len > 0) {
            char c = (char)chars[0];
            if (c >= 32 || c == 13) captured_payload += (c==13 ? '\n' : c);
        }
        key_press_starts[keycode] = now;
    }
    else if (type == kCGEventKeyUp) {
        float hold = 0, flight = 0, inter = 0;

        if (key_press_starts.count(keycode)) {
            hold = duration_cast<milliseconds>(now - key_press_starts[keycode]).count();
            key_press_starts.erase(keycode);
        }

        if (!first_key) {
            inter = duration_cast<milliseconds>(now - last_key_release_time).count();
            flight = inter - hold;

            if (flight > 2000) {
                window_buffer.clear();
            }
            else if (flight > 0 && hold > 0) {
                window_buffer.push_back({flight, inter, hold});
                if (window_buffer.size() >= WINDOW_SIZE) process_window();
            }
        }

        first_key = false;
        last_key_release_time = now;
    }

    return event;
}

void monitorUsbLoop() {
    sleep(2);
    int baseline = getUsbDeviceCount();

    while (true) {
        sleep(1);

        if (INPUT_BLOCKED.load()) {
            auto quiet = duration_cast<seconds>(Clock::now() - t_last_threat_activity).count();

            if (quiet > SILENCE_TIMEOUT_SECONDS) {
                cout << "\n[INFO] Threat Neutralized. Restoring User Control.\n";
                system("killall osascript > /dev/null 2>&1");

                saveForensics();

                window_buffer.clear();
                key_press_starts.clear();
                captured_payload.clear();
                first_key = true;

                baseline = getUsbDeviceCount();
                INPUT_BLOCKED.store(false);
            }
        } else {
            int current = getUsbDeviceCount();
            if (current > baseline) {
                baseline = current;
            } else if (current < baseline) {
                baseline = current;
            }
        }
    }
}

int ejectBadUSB() {
    cout << "=======================================\n";
    cout << " XGBOOST BAD-USB DEFENSE INITIALIZING\n";
    cout << "=======================================\n";

    ai_agent = new BadUSBDetector("badusb_xgboost.json");

    CGEventMask mask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) |
                       CGEventMaskBit(kCGEventLeftMouseDown) | CGEventMaskBit(kCGEventLeftMouseUp) |
                       CGEventMaskBit(kCGEventRightMouseDown) | CGEventMaskBit(kCGEventRightMouseUp) |
                       CGEventMaskBit(kCGEventMouseMoved) |
                       CGEventMaskBit(kCGEventLeftMouseDragged) | CGEventMaskBit(kCGEventRightMouseDragged) |
                       CGEventMaskBit(kCGEventScrollWheel) |
                       CGEventMaskBit(kCGEventOtherMouseDown) | CGEventMaskBit(kCGEventOtherMouseUp);

    CFMachPortRef tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, static_cast<CGEventTapOptions>(0), mask, eventCallbackDisconnect, nullptr);

    if (!tap) { cerr << "FATAL: Must run with sudo!\n"; return 1; }

    gTap = tap;

    CFRunLoopSourceRef src = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), src, kCFRunLoopCommonModes);
    CGEventTapEnable(tap, true);

    thread usb(monitorUsbLoop);
    usb.detach();

    CFRunLoopRun();

    delete ai_agent;
    return 0;
}