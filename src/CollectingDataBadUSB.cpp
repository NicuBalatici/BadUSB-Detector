#include "../include/CollectingDataBadUSB.h"
#include <iostream>
#include <chrono>
#include <ApplicationServices/ApplicationServices.h>

using namespace std;

ofstream outFile;
const string OUTPUT_FILE = "../data/captured_keys_badusb.txt";

ofstream csvFile;
const string CSV_FILE_BADUSB_DATA = "../data/badusb_data.csv";


using Clock = chrono::steady_clock;
using TimePoint = chrono::time_point<Clock>;
static TimePoint t_plug_in;
static TimePoint t_last_key_down;
static bool is_first_key = true;


//It is run only when the USB device is connected
void setConnectionTime() {
    t_plug_in = Clock::now();
    is_first_key = true;

    csvFile.open(CSV_FILE_BADUSB_DATA, ios::out | ios::app);
    if (csvFile.tellp() == 0) {
        csvFile << "character,reaction_time_ms,inter_char_delay_ms,key_hold_time_ms,label\n";
    }
}

void measureAndRecordMetrics(int charCode, int eventType) {
    TimePoint t_now = Clock::now();
    static TimePoint t_current_press_start;

    static long long stored_inter_char_delay = 0;

    if (eventType == 1) {
        t_current_press_start = t_now;

        if (is_first_key) {
            long long reaction_time_ms = 0;
            reaction_time_ms = chrono::duration_cast<chrono::milliseconds>(t_now - t_plug_in).count();
            cout << "REACTION TIME: " << reaction_time_ms << "ms" << endl;
        }

        if (!is_first_key) {
            stored_inter_char_delay = chrono::duration_cast<chrono::milliseconds>(t_now - t_last_key_down).count();
            cout << "RHYTHM DELAY:  " << stored_inter_char_delay << "ms" << endl;
        } else {
            stored_inter_char_delay = 0;
        }

        t_last_key_down = t_now;
    }else if (eventType == 0) {
        long long hold_time_ms = chrono::duration_cast<chrono::milliseconds>(t_now - t_current_press_start).count();
        cout << "[METRIC] HOLD DURATION: " << hold_time_ms << "ms" << endl;

        long long reaction_final = 0;

        if (is_first_key) {
            reaction_final = chrono::duration_cast<chrono::milliseconds>(t_current_press_start - t_plug_in).count();
            is_first_key = false;
        }
        string safeChar;

        switch (charCode) {
            case 32:
                safeChar = "[SPACE]";
                break;
            case 13:
            case 10:
                safeChar = "[ENTER]";
                break;
            case 9:
                safeChar = "[TAB]";
                break;
            case 44:
                safeChar = "[COMMA]";
                break;
            case 27:
                safeChar = "[ESC]";
                break;
            default:
                if (charCode < 32) {
                    safeChar = "[CTRL]";
                } else {
                    safeChar = string(1, (char)charCode);
                }
                break;
        }

        if (csvFile.is_open()) {
            csvFile << safeChar << ","
                    << reaction_final << ","
                    << stored_inter_char_delay << ","
                    << hold_time_ms << ","
                    << "1" << "\n";
            csvFile.flush();
        }
    }
}

CGEventRef eventCallbackBADUSB(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
    // It checks if the event is actually a keyboard press.
    if (type != kCGEventKeyDown && type != kCGEventKeyUp) {
        return event;
    }

    // Translates the electronic signal into a human-readable format
    UniChar chars[4];
    UniCharCount actualLength;
    CGEventKeyboardGetUnicodeString(event, 4, &actualLength, chars);
    int charCode = (actualLength > 0) ? static_cast<int>(chars[0]) : 0;

    // If the key is being pressed down (not released), it prints the letter to the screen and saves it to the .txt file
    if (type == kCGEventKeyDown && actualLength > 0) {
        char c = static_cast<char>(chars[0]);
        cout << c << flush;
        if (outFile.is_open()) {
            outFile << c;
            outFile.flush();
        }
    }

    if (type == kCGEventKeyDown) {
        measureAndRecordMetrics(charCode, 1); // Calculates the *Interval*
    } else {
        measureAndRecordMetrics(charCode, 0); // Calculates the *Hold Duration*
    }

    return event;
}

int startDataCollectionBadUSB(){
    cout << "\"CAPTURING INPUTS\" Mode" << endl;

    //Counting the current USB devices
    int baseline = getUsbDeviceCount();
    cout << "[*] Baseline: " << baseline << " devices." << endl;

    //Checking for a new device connected
    while (true) {
        if (getUsbDeviceCount() > baseline) {
            cout << "\nNew device detected! Starting capture..." << endl;
            setConnectionTime();
            break;
        }
        usleep(100000);
    }

    //Opening captured_badusb.txt file
    outFile.open(OUTPUT_FILE, ios::out | ios::trunc);
    if (!outFile.is_open()) {
        cerr << "ERROR opening captured_badusb.txt!" << endl;
        return 1;
    }

    //Opening thesis_training_data.csv file
    if (!csvFile.is_open()) {
        csvFile.open(CSV_FILE_BADUSB_DATA, ios::out | ios::trunc);
    }

    // Installs a system-wide "Event Tap" (Hook) into the macOS Run Loop to intercept
    // and process all keyboard events (Press, Release, Modifiers) before they reach the active application.
    CGEventMask eventMask = (1 << kCGEventKeyDown) | (1 << kCGEventKeyUp) | (1 << kCGEventFlagsChanged);
    CFMachPortRef eventTap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionDefault,
        eventMask,
        eventCallbackBADUSB,
        nullptr
    );

    if (!eventTap) {
        cerr << "ERROR: Failed to create event tap! Check permissions." << endl;
        return 1;
    }

    CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(eventTap, true);

    cout << "Capturing..." << endl;
    cout << "Saving to:" << endl;
    cout << "1. " << OUTPUT_FILE << " (Raw Content)" << endl;
    cout << "2. " << CSV_FILE_BADUSB_DATA << " (Thesis Metrics)" << endl;

    CFRunLoopRun();
    return 0;
}