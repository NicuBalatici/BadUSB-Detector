#include "../include/DisconnectKeyboard.h"
#include "../include/UsbUtils.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <ApplicationServices/ApplicationServices.h>
#include <unistd.h>

using namespace std;
using Clock = chrono::steady_clock;

static auto t_last_press = Clock::now();
static auto t_last_threat_activity = Clock::now();

constexpr int SILENCE_TIMEOUT_SECONDS = 5; //after the ML algorithm is finished, this should be modififed

atomic<bool> INPUT_BLOCKED{false};

void activateLockdown() {
    if (!INPUT_BLOCKED.load()) {
        cout << "\n!!! Lockdown activated — blocking all inputs\n";
        INPUT_BLOCKED.store(true);

        t_last_threat_activity = Clock::now();

        system("diskutil eject /Volumes/* > /dev/null 2>&1");
    }
}

CGEventRef eventCallbackDisconnect(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
    if (INPUT_BLOCKED.load()) {
        if (type == kCGEventKeyDown || type == kCGEventLeftMouseDown) {
            t_last_threat_activity = Clock::now();
        }
        return nullptr;
    }

    if (type == kCGEventKeyDown) {
        auto now = Clock::now();
        t_last_press = now;
    }

    return event;
}

void monitorUsbLoop() {
    sleep(2);
    int baseline = getUsbDeviceCount();
    if (baseline == 0) { sleep(1); baseline = getUsbDeviceCount(); }

    cout << "Watchdog active. Baseline: " << baseline << endl;

    while (true) {
        sleep(1);

        if (INPUT_BLOCKED.load()) {
            auto now = Clock::now();
            auto quiet_seconds = chrono::duration_cast<chrono::seconds>(now - t_last_threat_activity).count();

            if (quiet_seconds > SILENCE_TIMEOUT_SECONDS) {
                cout << "\nNo activity for " << SILENCE_TIMEOUT_SECONDS << "s. Attack finished." << endl;
                cout << "EXIT - Closing defense program automatically." << endl;
                exit(0);
            }
        }else {
            int current = getUsbDeviceCount();
            if (current > baseline) {
                cout << "NEW USB DEVICE DETECTED\n";
                activateLockdown();
                baseline = current;
            }
        }
    }
}

int ejectBadUSB() {
    cout << "BadUSB Defense - Auto-Close Enabled\n";

    constexpr CGEventMask eventMask =
        CGEventMaskBit(kCGEventKeyDown) |
        CGEventMaskBit(kCGEventKeyUp) |
        CGEventMaskBit(kCGEventFlagsChanged) |
        CGEventMaskBit(kCGEventMouseMoved) |
        CGEventMaskBit(kCGEventLeftMouseDown) |
        CGEventMaskBit(kCGEventLeftMouseUp) |
        CGEventMaskBit(kCGEventRightMouseDown) |
        CGEventMaskBit(kCGEventRightMouseUp) |
        CGEventMaskBit(kCGEventOtherMouseDown) |
        CGEventMaskBit(kCGEventOtherMouseUp) |
        CGEventMaskBit(kCGEventScrollWheel);

    CFMachPortRef eventTap = CGEventTapCreate(
        kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, eventMask, eventCallbackDisconnect, nullptr
    );

    if (!eventTap) {
        cerr << "ERROR! Run with sudo!\n"; return 1;
    }

    CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(eventTap, true);

    thread usbMonitor(monitorUsbLoop);
    usbMonitor.detach();

    CFRunLoopRun();
    return 0;
}