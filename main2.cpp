// Compile with: g++ -O3 -std=c++11 main.cpp -o screenshot.exe -lgdi32
// This optimizes the code for maximum performance

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <thread>
#include <mutex>

// Configuration constants
const int CAPTURE_SIZE = 186;               // Size of screenshot in pixels
const int CAPTURE_POS_X = 1187;             // X position of capture area
const int CAPTURE_POS_Y = 607;              // Y position of capture area
const double RING_OUTER_RADIUS = 89.0;      // Outer ring radius in pixels
const double RING_INNER_RADIUS = 84.0;      // Inner ring radius in pixels
const int MIN_WHITE_PIXELS = 16;            // Minimum white pixels to trigger first condition
const int MIN_RED_PIXELS = 2;               // Minimum red pixels to trigger second condition
const int TIMER_DURATION_MS = 1200;         // Timer duration in milliseconds
const int RESET_DELAY_MS = 500;             // Delay after condition reset
const BYTE RED_THRESHOLD = 50;              // Red channel threshold (0-255)
const BYTE OTHER_CHANNEL_MAX = 150;         // Max value for green/blue when checking red
const BYTE WHITE_THRESHOLD = 0xFE;          // White pixel threshold (254)

bool saveEnabled = false;
std::mutex saveMutex;                       // Mutex for thread-safe file operations

// Structure to hold pixel coordinates
struct PixelPos {
    int x, y;
};

// Save bitmap to file with highlighted pixels
// - whitePixels: pixels marked in pink (first condition)
// - redPixels: pixels marked in yellow (second condition)
void SaveBitmapWithHighlights(const char* filename, BYTE* originalBits, int width, int height,
                               const std::vector<PixelPos>& whitePixels,
                               const std::vector<PixelPos>& redPixels) {
    std::lock_guard<std::mutex> lock(saveMutex);
    
    int rowSize = ((width * 3 + 3) / 4) * 4;
    DWORD bmpSize = rowSize * height;
    
    // Copy original bits to modify
    BYTE* bits = new BYTE[bmpSize];
    memcpy(bits, originalBits, bmpSize);
    
    // Mark white pixels (first condition) in PINK (FF 00 FF)
    for (const auto& pos : whitePixels) {
        if (pos.x >= 0 && pos.x < width && pos.y >= 0 && pos.y < height) {
            int index = pos.y * rowSize + pos.x * 3;
            bits[index + 0] = 0xFF;  // B
            bits[index + 1] = 0x00;  // G
            bits[index + 2] = 0xFF;  // R
        }
    }
    
    // Mark red pixels (second condition) in YELLOW (00 FF FF)
    for (const auto& pos : redPixels) {
        if (pos.x >= 0 && pos.x < width && pos.y >= 0 && pos.y < height) {
            int index = pos.y * rowSize + pos.x * 3;
            bits[index + 0] = 0x00;  // B
            bits[index + 1] = 0xFF;  // G
            bits[index + 2] = 0xFF;  // R
        }
    }
    
    // Create BMP file
    BITMAPINFOHEADER bi = {0};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = height;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    
    HANDLE hFile = CreateFileA(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        BITMAPFILEHEADER bfh = {0};
        bfh.bfType = 0x4D42;
        bfh.bfSize = sizeof(bfh) + sizeof(bi) + bmpSize;
        bfh.bfOffBits = sizeof(bfh) + sizeof(bi);

        DWORD written;
        WriteFile(hFile, &bfh, sizeof(bfh), &written, NULL);
        WriteFile(hFile, &bi, sizeof(bi), &written, NULL);
        WriteFile(hFile, bits, bmpSize, &written, NULL);
        CloseHandle(hFile);
    }
    
    delete[] bits;
}

// Check if pixel is within the ring area (between inner and outer radius)
bool IsInRing(int x, int y, int centerX, int centerY, double innerRadius, double outerRadius) {
    double dx = x - centerX;
    double dy = y - centerY;
    double distSq = dx * dx + dy * dy;
    double innerSq = innerRadius * innerRadius;
    double outerSq = outerRadius * outerRadius;
    return distSq > innerSq && distSq < outerSq;
}

// Check if pixel is white-ish (all channels >= WHITE_THRESHOLD)
bool IsWhiteish(BYTE r, BYTE g, BYTE b) {
    return r >= WHITE_THRESHOLD && g >= WHITE_THRESHOLD && b >= WHITE_THRESHOLD;
}

// Check if pixel is red (R >= RED_THRESHOLD, G and B < OTHER_CHANNEL_MAX)
bool IsReddish(BYTE r, BYTE g, BYTE b) {
    return r >= RED_THRESHOLD && g < OTHER_CHANNEL_MAX && b < OTHER_CHANNEL_MAX;
}

// Main capture and processing function
bool CaptureAndProcess(int size, int posX, int posY, 
                       std::vector<PixelPos>& whitePixels,
                       std::vector<PixelPos>& redPixels,
                       bool& firstCondition, bool& secondCondition,
                       LARGE_INTEGER& timerStart, LARGE_INTEGER freq) {
    
    // Get screen device context
    HDC hScreen = GetDC(NULL);
    HDC hMem = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, size, size);
    HBITMAP old = (HBITMAP)SelectObject(hMem, hBitmap);

    // Capture screen region
    bool ok = BitBlt(hMem, 0, 0, size, size, hScreen, posX, posY, SRCCOPY);

    if (!ok) {
        SelectObject(hMem, old);
        DeleteObject(hBitmap);
        DeleteDC(hMem);
        ReleaseDC(NULL, hScreen);
        return false;
    }

    // Get bitmap bits
    BITMAPINFOHEADER bi = {0};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = size;
    bi.biHeight = size;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;

    DWORD sizeBytes = ((size * 3 + 3) & ~3) * size;
    BYTE* buf = new BYTE[sizeBytes];
    GetDIBits(hMem, hBitmap, 0, size, buf, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    int rowSize = ((size * 3 + 3) / 4) * 4;
    int centerX = size / 2;
    int centerY = size / 2;

    // Stage 1: Looking for white pixels in ring
    if (!firstCondition) {
        whitePixels.clear();
        redPixels.clear();
        
        // Scan all pixels in the ring area
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                if (IsInRing(x, y, centerX, centerY, RING_INNER_RADIUS, RING_OUTER_RADIUS)) {
                    BYTE b = buf[y * rowSize + x * 3 + 0];
                    BYTE g = buf[y * rowSize + x * 3 + 1];
                    BYTE r = buf[y * rowSize + x * 3 + 2];
                    
                    if (IsWhiteish(r, g, b)) {
                        PixelPos pos;
                        pos.x = x;
                        pos.y = y;
                        whitePixels.push_back(pos);
                    }
                }
            }
        }
        
        // First condition: at least MIN_WHITE_PIXELS white pixels found
        if (whitePixels.size() >= MIN_WHITE_PIXELS) {
            firstCondition = true;
            QueryPerformanceCounter(&timerStart);
        }
    }
    // Stage 2: Check if remembered pixels turn red
    else {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double elapsed = (now.QuadPart - timerStart.QuadPart) * 1000.0 / freq.QuadPart;
        
        // Timeout - reset and wait
        if (elapsed >= TIMER_DURATION_MS) {
            firstCondition = false;
            whitePixels.clear();
            redPixels.clear();
            delete[] buf;
            SelectObject(hMem, old);
            DeleteObject(hBitmap);
            DeleteDC(hMem);
            ReleaseDC(NULL, hScreen);
            Sleep(RESET_DELAY_MS);
            return false;
        }
        
        // Check if remembered pixels have turned red
        redPixels.clear();
        for (const auto& pos : whitePixels) {
            int x = pos.x;
            int y = pos.y;
            if (x >= 0 && x < size && y >= 0 && y < size) {
                BYTE b = buf[y * rowSize + x * 3 + 0];
                BYTE g = buf[y * rowSize + x * 3 + 1];
                BYTE r = buf[y * rowSize + x * 3 + 2];
                
                if (IsReddish(r, g, b)) {
                    redPixels.push_back(pos);
                }
            }
        }
        
        // Second condition: at least MIN_RED_PIXELS turned red
        if (redPixels.size() >= MIN_RED_PIXELS) {
            secondCondition = true;
            
            // Save image in separate thread if enabled
            if (saveEnabled) {
                // Copy buffer for thread
                BYTE* bufCopy = new BYTE[sizeBytes];
                memcpy(bufCopy, buf, sizeBytes);
                
                // Copy vectors for thread
                std::vector<PixelPos> whiteCopy = whitePixels;
                std::vector<PixelPos> redCopy = redPixels;
                
                // Launch save thread
                std::thread saveThread([bufCopy, size, whiteCopy, redCopy]() {
                    SaveBitmapWithHighlights("output.bmp", bufCopy, size, size, whiteCopy, redCopy);
                    delete[] bufCopy;
                });
                saveThread.detach(); // Run independently
            }
        }
    }

    // Cleanup
    delete[] buf;
    SelectObject(hMem, old);
    DeleteObject(hBitmap);
    DeleteDC(hMem);
    ReleaseDC(NULL, hScreen);
    
    return secondCondition;
}

int main() {
    SetProcessDPIAware();

    int fps = 90;
    int pro = 1;

    // Get configuration from user
    std::cout << "Configuration:\n";
    std::cout << "fps (default " << fps << "): ";
    std::cin >> fps;
    std::cout << "pro mode? (1=yes, 0=no, default " << pro << "): ";
    std::cin >> pro;
    std::cout << "save file? (1=yes, 0=no, default " << (saveEnabled ? 1 : 0) << "): ";
    std::cin >> saveEnabled;

    bool proMode = (pro != 0);
    double frameDelay = 1000.0 / fps;

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    // Display configuration
    std::cout << "\nStart capture: " << CAPTURE_SIZE << "x" << CAPTURE_SIZE
              << " at position (" << CAPTURE_POS_X << ", " << CAPTURE_POS_Y << ")"
              << " @ " << fps << " FPS  mode=" << (proMode ? "pro" : "eco")
              << " save=" << (saveEnabled ? "yes" : "no") << "\n";
    std::cout << "Monitoring for white pixels (>=" << (int)WHITE_THRESHOLD << ") in ring "
              << "(inner radius=" << RING_INNER_RADIUS << ", outer radius=" << RING_OUTER_RADIUS << ")...\n";
    std::cout << "First condition: >= " << MIN_WHITE_PIXELS << " white pixels\n";
    std::cout << "Second condition: >= " << MIN_RED_PIXELS << " pixels with R>=" << (int)RED_THRESHOLD 
              << " AND G,B<" << (int)OTHER_CHANNEL_MAX << "\n";
    std::cout << "Timer: " << TIMER_DURATION_MS << "ms, Reset delay: " << RESET_DELAY_MS << "ms\n\n";

    // Main loop variables
    std::vector<PixelPos> whitePixels;
    std::vector<PixelPos> redPixels;
    bool firstCondition = false;
    bool secondCondition = false;
    LARGE_INTEGER timerStart;

    // Main capture loop
    while (true) {
        LARGE_INTEGER frameStart;
        QueryPerformanceCounter(&frameStart);

        // Process frame
        bool result = CaptureAndProcess(CAPTURE_SIZE, CAPTURE_POS_X, CAPTURE_POS_Y, 
                                       whitePixels, redPixels,
                                       firstCondition, secondCondition,
                                       timerStart, freq);

        // Handle second condition trigger
        if (secondCondition) {
            std::cout << "SECOND CONDITION TRUE (detected " << redPixels.size() << " red pixels)\n";
            
            // Reset after delay
            Sleep(RESET_DELAY_MS);
            firstCondition = false;
            secondCondition = false;
            whitePixels.clear();
            redPixels.clear();
        }

        // Frame rate control
        LARGE_INTEGER cur;
        QueryPerformanceCounter(&cur);
        double elapsed = (cur.QuadPart - frameStart.QuadPart) * 1000.0 / freq.QuadPart;

        // Eco mode: use Sleep for most of the delay
        if (!proMode) {
            if (elapsed < frameDelay - 1)
                Sleep((DWORD)(frameDelay - elapsed - 1));
        }

        // Busy wait for precise timing
        while (true) {
            QueryPerformanceCounter(&cur);
            if ((cur.QuadPart - frameStart.QuadPart) * 1000.0 / freq.QuadPart >= frameDelay)
                break;
        }
    }
}