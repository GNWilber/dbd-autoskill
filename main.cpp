#include <windows.h>
#include <iostream>
#include <string>

bool saveEnabled = false;
int saveIndex = 1;
DWORD lastSaveTick = 0;

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

bool HasWhitePixel(BYTE* lpBits, int width, int height) {
    int rowSize = ((width * 3 + 3) / 4) * 4;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            BYTE b = lpBits[y * rowSize + x * 3 + 0];
            BYTE g = lpBits[y * rowSize + x * 3 + 1];
            BYTE r = lpBits[y * rowSize + x * 3 + 2];
            if (r == 255 && g == 255 && b == 255)
                return true;
        }
    }
    return false;
}

bool CaptureAndCheckWhitePixel(int width, int height, int posX, int posY, bool doSave) {
    HDC hScreen = GetDC(NULL);
    HDC hMem = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);
    HBITMAP old = (HBITMAP)SelectObject(hMem, hBitmap);

    bool ok = BitBlt(hMem, 0, 0, width, height, hScreen, posX, posY, SRCCOPY);

    bool hasWhite = false;

    if (ok) {
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

        hasWhite = HasWhitePixel(buf, width, height);

        delete[] buf;
    }

    // SAVE TEST IMAGE EVERY 1 SECOND
    if (doSave) {
        DWORD now = GetTickCount();
        if (now - lastSaveTick >= 500) {
            {
                char filename[32];
                sprintf(filename, "%d.bmp", saveIndex);
                
                SaveBitmapToFile(filename, hBitmap, hMem, width, height);
                
                saveIndex++;
                if (saveIndex > 20) saveIndex = 1;  
            }
            {
                char filename[32];
                sprintf(filename, "%d.bmp", 0);
                
                SaveBitmapToFile(filename, hBitmap, hMem, width, height);
            }
            lastSaveTick = now;
        }
    }

    SelectObject(hMem, old);
    DeleteObject(hBitmap);
    DeleteDC(hMem);
    ReleaseDC(NULL, hScreen);

    return hasWhite;
}

int main() {
    SetProcessDPIAware();

    int fps = 90;
    int size = 186;
    int posX = 1187;
    int posY = 607;
    int pro = 1;

    std::cout << "Press Enter for defaults or type anything to adjust parameters: ";
    std::string s;
    std::getline(std::cin, s);

    if (!s.empty()) {
        std::cout << "fps (default 90): ";
        std::cin >> fps;
        std::cout << "size (default 186): ";
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

    LARGE_INTEGER start;
    QueryPerformanceCounter(&start);

    int frameCount = 0;

    std::cout << "Start capture: " << size << "x" << size
              << " at position (" << posX << ", " << posY << ")"
              << " " << fps << " FPS  mode=" << (proMode ? "pro" : "eco")
              << " save=" << (saveEnabled ? "yes" : "no") << "\n";

    while (true) {
        LARGE_INTEGER frameStart;
        QueryPerformanceCounter(&frameStart);

        bool hasWhite = CaptureAndCheckWhitePixel(size, size, posX, posY, saveEnabled);
        std::cout << (hasWhite ? "true" : "false") << "\n";

        frameCount++;

        LARGE_INTEGER cur;
        QueryPerformanceCounter(&cur);
        double elapsed = (cur.QuadPart - frameStart.QuadPart) * 1000.0 / freq.QuadPart;

        if (!proMode) {
            if (elapsed < frameDelay - 1)
                Sleep((DWORD)(frameDelay - elapsed - 1));
        }

        // busy wait
        while (true) {
            QueryPerformanceCounter(&cur);
            if ((cur.QuadPart - frameStart.QuadPart) * 1000.0 / freq.QuadPart >= frameDelay)
                break;
        }
    }
}