#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>

// Adjust this value to change the interval between space presses
const int SECONDS_INTERVAL = 5;

void pressSpace() {
    // Simulate space key press
    INPUT input[2] = {};
    
    // Key down
    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = VK_SPACE;
    input[0].ki.dwFlags = 0;
    
    // Key up
    input[1].type = INPUT_KEYBOARD;
    input[1].ki.wVk = VK_SPACE;
    input[1].ki.dwFlags = KEYEVENTF_KEYUP;
    
    SendInput(2, input, sizeof(INPUT));
}

int main() {
    std::cout << "Auto Space Presser Started\n";
    std::cout << "Press Ctrl+C to stop\n";
    std::cout << "Pressing space every " << SECONDS_INTERVAL << " seconds...\n";
    std::cout << "Waiting " << SECONDS_INTERVAL << " seconds before first press...\n\n";
    
    // Wait the interval time before starting
    std::this_thread::sleep_for(std::chrono::seconds(SECONDS_INTERVAL));
    
    int count = 0;
    while (true) {
        pressSpace();
        count++;
        std::cout << "Space pressed (" << count << ")\n";
        std::this_thread::sleep_for(std::chrono::seconds(SECONDS_INTERVAL));
    }
    
    return 0;
}