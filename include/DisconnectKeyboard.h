#ifndef DESIGN_DISCONNECTKEYBOARD_H
#define DESIGN_DISCONNECTKEYBOARD_H
#include <ApplicationServices/ApplicationServices.h>

void activateLockdown();
CGEventRef eventCallbackDisconnect(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon);
int ejectBadUSB();
void monitorUsbLoop();

#endif