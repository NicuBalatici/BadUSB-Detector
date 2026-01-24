#include "../include/UsbUtils.h"
#include <string>

using namespace std;

int getUsbDeviceCount() {
    FILE* pipe = popen("ioreg -p IOUSB | grep -c \"class\"", "r");
    if (!pipe) return 0;

    char buffer[128];
    int count = 0;
    if (fgets(buffer, sizeof(buffer), pipe)) {
        count = stoi(buffer);
    }
    pclose(pipe);
    return count;
}