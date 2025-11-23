// Compile with: g++ -O3 -std=c++11 main.cpp -o screenshot.exe -lgdi32
// This optimizes the code for maximum performance

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <thread>
#include <mutex>
#include <random>

// Configuration constants
const int CAPTURE_SIZE = 186;               // Size of screenshot in pixels
const int CAPTURE_POS_X = 1187;             // X position of capture area
const int CAPTURE_POS_Y = 607;              // Y position of capture area
const double RING_OUTER_RADIUS = 89.0;      // Outer ring radius in pixels
const double RING_INNER_RADIUS = 85.0;      // Inner ring radius in pixels
const int MIN_WHITE_PIXELS = 30;            // Minimum white pixels to trigger first condition
const int MIN_RED_PIXELS = 2;               // Minimum red pixels to trigger second condition
const int TIMER_DURATION_MS = 1200;         // Timer duration in milliseconds
const int RESET_DELAY_MS = 1000;            // Delay after condition reset
const BYTE RED_THRESHOLD = 50;              // Red channel threshold (0-255)
const BYTE OTHER_CHANNEL_MAX = 150;         // Max value for green/blue when checking red
const BYTE RED_DOMINANCE = 20;              // Red must be this much higher than other channels
const BYTE WHITE_THRESHOLD = 0xFE;          // White pixel threshold (254)

bool saveEnabled = false;
std::mutex saveMutex;                       // Mutex for thread-safe file operations

// Simulate SPACE key press for random duration between 80-150ms
void PressSpaceKey() {
    // Random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(80, 150);
    int pressDuration = dis(gen);
    
    // Prepare input structure for key down
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_SPACE;
    input.ki.dwFlags = 0; // 0 for key press
    
    // Send key down
    SendInput(1, &input, sizeof(INPUT));
    
    // Hold for random duration
    Sleep(pressDuration);
    
    // Prepare input structure for key up
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    
    // Send key up
    SendInput(1, &input, sizeof(INPUT));
}

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

// Find connected groups of pixels using flood fill
// Returns groups where each group has at least minSize pixels
std::vector<std::vector<PixelPos>> FindConnectedGroups(
    const std::vector<PixelPos>& pixels, int width, int height, int minSize) {
    
    std::vector<std::vector<PixelPos>> groups;
    std::vector<bool> visited(pixels.size(), false);
    
    // Create a map for quick lookup
    std::vector<std::vector<bool>> pixelMap(height, std::vector<bool>(width, false));
    for (const auto& p : pixels) {
        if (p.x >= 0 && p.x < width && p.y >= 0 && p.y < height) {
            pixelMap[p.y][p.x] = true;
        }
    }
    
    // Flood fill to find connected components
    for (size_t i = 0; i < pixels.size(); i++) {
        if (visited[i]) continue;
        
        std::vector<PixelPos> group;
        std::vector<size_t> toVisit;
        toVisit.push_back(i);
        
        while (!toVisit.empty()) {
            size_t idx = toVisit.back();
            toVisit.pop_back();
            
            if (visited[idx]) continue;
            visited[idx] = true;
            
            PixelPos current = pixels[idx];
            group.push_back(current);
            
            // Check 4 neighbors (up, down, left, right)
            int dx[] = {0, 0, -1, 1};
            int dy[] = {-1, 1, 0, 0};
            
            for (int d = 0; d < 4; d++) {
                int nx = current.x + dx[d];
                int ny = current.y + dy[d];
                
                if (nx >= 0 && nx < width && ny >= 0 && ny < height && pixelMap[ny][nx]) {
                    // Find this neighbor in the pixels list
                    for (size_t j = 0; j < pixels.size(); j++) {
                        if (!visited[j] && pixels[j].x == nx && pixels[j].y == ny) {
                            toVisit.push_back(j);
                            break;
                        }
                    }
                }
            }
        }
        
        // Only keep groups with at least minSize pixels
        if (group.size() >= minSize) {
            groups.push_back(group);
        }
    }
    
    return groups;
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

// Check if pixel is red (R >= RED_THRESHOLD, G and B < OTHER_CHANNEL_MAX, and R is RED_DOMINANCE higher than G and B)
bool IsReddish(BYTE r, BYTE g, BYTE b) {
    return r >= RED_THRESHOLD && 
           g < OTHER_CHANNEL_MAX && 
           b < OTHER_CHANNEL_MAX &&
           r >= (g + RED_DOMINANCE) &&
           r >= (b + RED_DOMINANCE);
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
    int centerX = size / 2 - 1;  // Offset 1 pixel to the left
    int centerY = size / 2;

    // Stage 1: Looking for white pixels in ring
    if (!firstCondition) {
        whitePixels.clear();
        redPixels.clear();
        
        std::vector<PixelPos> candidatePixels;
        
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
                        candidatePixels.push_back(pos);
                    }
                }
            }
        }
        
        // Find connected groups of white pixels
        std::vector<std::vector<PixelPos>> groups = 
            FindConnectedGroups(candidatePixels, size, size, MIN_WHITE_PIXELS);
        
        // Add all pixels from valid groups to whitePixels
        for (const auto& group : groups) {
            for (const auto& pixel : group) {
                whitePixels.push_back(pixel);
            }
        }
        
        // First condition: at least one valid group found
        if (!whitePixels.empty()) {
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
            
            // Press SPACE key in separate thread
            std::thread spaceThread([]() {
                PressSpaceKey();
            });
            spaceThread.detach(); // Run independently
            
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
    std::cout << "First condition: >= " << MIN_WHITE_PIXELS << " connected white pixels (must share sides)\n";
    std::cout << "Second condition: >= " << MIN_RED_PIXELS << " pixels with R>=" << (int)RED_THRESHOLD 
              << " AND G,B<" << (int)OTHER_CHANNEL_MAX 
              << " AND R>" << (int)RED_DOMINANCE << " more than G,B\n";
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