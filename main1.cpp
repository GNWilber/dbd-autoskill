#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>

struct PixelPos {
    int x, y;
};

bool saveEnabled = false;
int savedFrameCount = 0;

// State tracking
bool firstCondition = false;
bool secondCondition = false;
std::vector<PixelPos> whitePixelPositions;
DWORD firstConditionStartTime = 0;
const DWORD DETECTION_TIMEOUT = 1200; // ms
const DWORD RESET_DELAY = 500; // ms

void SaveBitmapToFile(const char* filename, HBITMAP hBitmap, HDC hDC, int width, int height) {
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    BITMAPINFOHEADER bi = {0};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = height;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;

    DWORD bmpSize = ((width * 3 + 3) & ~3) * height;
    BYTE* bits = new BYTE[bmpSize];

    GetDIBits(hDC, hBitmap, 0, height, bits, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

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

// Check if pixel is in the ring (between inner radius 178 and outer radius 180)
// Outlines don't count (strictly between, not inclusive)
bool IsInRing(int x, int y, int centerX, int centerY) {
    double dx = x - centerX;
    double dy = y - centerY;
    double dist = sqrt(dx * dx + dy * dy);
    
    // Ring between radius 178 and 180 (exclusive of boundaries)
    return dist > 89.0 && dist < 92.0;
}

// Check if pixel is white (at least 0xFE in all channels)
bool IsWhitePixel(BYTE r, BYTE g, BYTE b) {
    return r >= 0xFE && g >= 0xFE && b >= 0xFE;
}

// Find white pixels in the ring
std::vector<PixelPos> FindWhitePixelsInRing(BYTE* lpBits, int width, int height) {
    std::vector<PixelPos> positions;
    int rowSize = ((width * 3 + 3) / 4) * 4;
    int centerX = width / 2;
    int centerY = height / 2;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (IsInRing(x, y, centerX, centerY)) {
                BYTE b = lpBits[y * rowSize + x * 3 + 0];
                BYTE g = lpBits[y * rowSize + x * 3 + 1];
                BYTE r = lpBits[y * rowSize + x * 3 + 2];
                
                if (IsWhitePixel(r, g, b)) {
                    positions.push_back({x, y});
                }
            }
        }
    }
    
    return positions;
}

// Check if any remembered pixel has red channel >= 0xB0
bool CheckRedThreshold(BYTE* lpBits, int width, int height, const std::vector<PixelPos>& positions) {
    int rowSize = ((width * 3 + 3) / 4) * 4;
    
    for (const auto& pos : positions) {
        if (pos.x >= 0 && pos.x < width && pos.y >= 0 && pos.y < height) {
            BYTE r = lpBits[pos.y * rowSize + pos.x * 3 + 2];
            if (r >= 0xB0) {
                return true;
            }
        }
    }
    
    return false;
}

void ProcessFrame(int width, int height, int posX, int posY, bool doSave, HDC hMem, HBITMAP hBitmap) {
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    BITMAPINFOHEADER bi = {0};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = height;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;

    DWORD sizeBytes = ((width * 3 + 3) & ~3) * height;
    BYTE* buf = new BYTE[sizeBytes];

    GetDIBits(hMem, hBitmap, 0, height, buf, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    DWORD currentTime = GetTickCount();

    if (!firstCondition && !secondCondition) {
        // Look for white pixels in the ring
        std::vector<PixelPos> foundPixels = FindWhitePixelsInRing(buf, width, height);
        
        if (!foundPixels.empty()) {
            // First condition triggered
            firstCondition = true;
            whitePixelPositions = foundPixels;
            firstConditionStartTime = currentTime;
        }
    } else if (firstCondition && !secondCondition) {
        // First condition is active, check for red threshold
        DWORD elapsed = currentTime - firstConditionStartTime;
        
        if (elapsed <= DETECTION_TIMEOUT) {
            if (CheckRedThreshold(buf, width, height, whitePixelPositions)) {
                // Second condition triggered!
                secondCondition = true;
                savedFrameCount = 0;
                std::cout << "DETECTED - Starting capture sequence\n";
            }
        } else {
            // Timeout - no red threshold detected within 1200ms
            Sleep(RESET_DELAY);
            firstCondition = false;
            whitePixelPositions.clear();
        }
    } else if (secondCondition) {
        // Second condition is active - save frames and output
        std::cout << "DETECTED - Frame " << (savedFrameCount + 1) << "\n";
        
        // Save if enabled and under limit
        if (doSave && savedFrameCount < 100) {
            char filename[64];
            sprintf(filename, "detect_%03d.bmp", savedFrameCount + 1);
            SaveBitmapToFile(filename, hBitmap, hMem, width, height);
            savedFrameCount++;
        }
        
        // Check if we should stop (reached 100 frames or timeout)
        if (savedFrameCount >= 100 || (currentTime - firstConditionStartTime) > DETECTION_TIMEOUT + 500) {
            std::cout << "Capture sequence complete - " << savedFrameCount << " frames saved\n";
            Sleep(RESET_DELAY);
            firstCondition = false;
            secondCondition = false;
            whitePixelPositions.clear();
            savedFrameCount = 0;
        }
    }

    delete[] buf;
}

int main() {
    SetProcessDPIAware();

    int fps = 90;
    int size = 362; // Default large enough to contain ring (radius 180 * 2 + margin)
    int posX = 1187;
    int posY = 607;
    int pro = 1;

    std::cout << "Press Enter for defaults or type anything to adjust parameters: ";
    std::string s;
    std::getline(std::cin, s);

    if (!s.empty()) {
        std::cout << "fps (default 90): ";
        std::cin >> fps;
        std::cout << "size (default 362): ";
        std::cin >> size;
        std::cout << "position X (default 1187): ";
        std::cin >> posX;
        std::cout << "position Y (default 607): ";
        std::cin >> posY;
        std::cout << "pro mode? (1=yes, 0=no, default 1): ";
        std::cin >> pro;
        std::cout << "save file? (1=yes, 0=no, default 0): ";
        std::cin >> saveEnabled;
    }

    bool proMode = (pro != 0);
    double frameDelay = 1000.0 / fps;

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    std::cout << "\nStart capture: " << size << "x" << size
              << " at position (" << posX << ", " << posY << ")"
              << " @ " << fps << " FPS  mode=" << (proMode ? "pro" : "eco")
              << " save=" << (saveEnabled ? "yes" : "no") << "\n";
    std::cout << "Monitoring ring (radius 178-180 pixels) for white pixels (>=FE)...\n";
    std::cout << "Detection: white pixels -> wait for red channel >=B0 within 1200ms\n\n";

    while (true) {
        LARGE_INTEGER frameStart;
        QueryPerformanceCounter(&frameStart);

        HDC hScreen = GetDC(NULL);
        HDC hMem = CreateCompatibleDC(hScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, size, size);
        HBITMAP old = (HBITMAP)SelectObject(hMem, hBitmap);

        bool ok = BitBlt(hMem, 0, 0, size, size, hScreen, posX, posY, SRCCOPY);

        if (ok) {
            ProcessFrame(size, size, posX, posY, saveEnabled, hMem, hBitmap);
        }

        SelectObject(hMem, old);
        DeleteObject(hBitmap);
        DeleteDC(hMem);
        ReleaseDC(NULL, hScreen);

        LARGE_INTEGER cur;
        QueryPerformanceCounter(&cur);
        double elapsed = (cur.QuadPart - frameStart.QuadPart) * 1000.0 / freq.QuadPart;

        if (!proMode) {
            if (elapsed < frameDelay - 1)
                Sleep((DWORD)(frameDelay - elapsed - 1));
        }

        // busy wait for precise timing
        while (true) {
            QueryPerformanceCounter(&cur);
            if ((cur.QuadPart - frameStart.QuadPart) * 1000.0 / freq.QuadPart >= frameDelay)
                break;
        }
    }

    return 0;
}