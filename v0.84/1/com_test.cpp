#include <iostream>
#include <windows.h>
#include "ftd2xx.h"

int main() {
    FT_HANDLE ftHandle;
    FT_STATUS ftStatus;
    DWORD bytesWritten;
    char serialNumber[] = "A5069RR4";
    char data[] = "LT";

    // Open the device by its serial number
    ftStatus = FT_OpenEx(serialNumber, FT_OPEN_BY_SERIAL_NUMBER, &ftHandle);
    if (ftStatus != FT_OK) {
        std::cerr << "Error opening device with serial number " << serialNumber << ". FT_STATUS: " << ftStatus << std::endl;
        return 1;
    }

    std::cout << "Device with serial number " << serialNumber << " opened successfully." << std::endl;

    // Set Baud Rate to a standard value
    ftStatus = FT_SetBaudRate(ftHandle, 9600);
    if (ftStatus != FT_OK) {
        std::cerr << "Error setting baud rate. FT_STATUS: " << ftStatus << std::endl;
        FT_Close(ftHandle);
        return 1;
    }

    std::cout << "Baud rate set to 9600." << std::endl;

    // Loop to send "LT" repeatedly
    std::cout << "Starting to send 'LT' repeatedly. Press Ctrl+C to stop." << std::endl;
    while (true) {
        ftStatus = FT_Write(ftHandle, data, sizeof(data) - 1, &bytesWritten);
        if (ftStatus != FT_OK) {
            std::cerr << "Error writing to device. FT_STATUS: " << ftStatus << std::endl;
            break;
        }
        if (bytesWritten != sizeof(data) - 1) {
            std::cerr << "Warning: Did not write all bytes." << std::endl;
        }
        // Add a small delay to avoid overwhelming the device and the console output
        Sleep(100); // 100ms delay
    }

    // Close the device
    FT_Close(ftHandle);
    std::cout << "Device closed." << std::endl;

    return 0;
}
