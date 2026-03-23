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
#include <iomanip>
#include <cstdio>
#include <ApplicationServices/ApplicationServices.h>
#include <unistd.h>
#include "GeminiAnalyzer.h"

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

string pre_catch_payload;

struct KeyData {
    float flight;
    float inter;
    float hold;
};
vector<KeyData> window_buffer;

BadUSBDetector* ai_agent = nullptr;

string getCurrentTimestamp() {
    auto t = time(nullptr);
    auto tm = *localtime(&t);
    ostringstream oss;
    oss << put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void showPopup(const string& message) {
    system("killall osascript > /dev/null 2>&1");
    system("say -v Samantha 'Security Alert. Malicious USB Detected.' &");
    string command = "osascript -e 'display alert \"⚠️ THREAT ISOLATED\" message \"" + message +
                     "\" as critical buttons {\"Locked\"} default button \"Locked\"' &";
    system(command.c_str());
}

bool askUserToViewSecondPopup() {
    string script = R"(osascript -e 'button returned of (display alert "Threat Neutralized" message "The system has been unlocked. Would you like to view the attack description?" buttons {"No", "Yes"} default button "Yes" as informational)')";
    FILE* pipe = popen(script.c_str(), "r");
    if (!pipe) return false;

    char buffer[128];
    string result = "";
    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);

    return (result.find("Yes") != string::npos);
}

void savePreCatchForensics() {
    if (pre_catch_payload.empty()) return;

    ofstream out("badusb_pre_catch.log", ios::app);
    if (out.is_open()) {
        out << "\n[" << getCurrentTimestamp() << "] ===== AI TRIGGERED =====\n";
        out << "Payload executed before detection:\n";
        out << pre_catch_payload << "\n";
        out << "========================================\n";
        out.close();
    }
    pre_catch_payload.clear();
}

void activateLockdown(const string& source, float threat_score) {
    if (!INPUT_BLOCKED.load()) {
        INPUT_BLOCKED.store(true);

        stringstream ss;
        ss << fixed << setprecision(2) << (threat_score * 100);

        cout << "\n[!!!] " << source << " TRIGGERED LOCKDOWN [!!!]\n";
        cout << "Threat Confidence: " << ss.str() << "%\n";

        savePreCatchForensics();

        system("diskutil eject /Volumes/* > /dev/null 2>&1");

        ofstream q("badusb_post_catch.log", ios::out | ios::app);
        if (q.is_open()) {
            q << "\n[" << getCurrentTimestamp() << "] ===== BLOCKED PAYLOAD =====\n";
        }

        showPopup("Input has been paralyzed!"
                  "\nThreat Score: " + ss.str() + "%.\n"+
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
                 ofstream q("badusb_post_catch.log", ios::out | ios::app);
                 if (q.is_open()) {
                    char c = (char)chars[0];
                    if (c == 13 || c == 3) q << "\n";
                    else if (c >= 32 && c <= 126) q << c;
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
            if (c == 13 || c == 3) pre_catch_payload += '\n';
            else if (c >= 32 && c <= 126) pre_catch_payload += c;
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
                pre_catch_payload.clear();
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

string getApiKey() {
    ifstream file("api_key.txt");
    string key;
    if (file.is_open()) {
        getline(file, key);
        file.close();

        key.erase(remove(key.begin(), key.end(), '\n'), key.end());
        key.erase(remove(key.begin(), key.end(), '\r'), key.end());
    } else {
        cerr << "[ERROR] Could not find api_key.txt! Make sure it is in your root folder." << endl;
    }
    return key;
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
                system("say -v Samantha 'System secured.' &");
                savePreCatchForensics();

                INPUT_BLOCKED.store(false);

                if (askUserToViewSecondPopup()) {
                    cout << "[INFO] Establishing secure C++ connection to Gemini AI...\n";

                    system(R"(osascript -e 'display notification "Generating Native Threat Intelligence Report..." with title "C++ AI Bridge"' &)");

                    GeminiAnalyzer ai_analyst(getApiKey());
                    ai_analyst.generateThreatReport("badusb_post_catch.log");
                }else{
                    cout << "[INFO] Closing Forensic Logs...\n";
                }

                window_buffer.clear();
                key_press_starts.clear();
                pre_catch_payload.clear();
                first_key = true;
                baseline = getUsbDeviceCount();
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