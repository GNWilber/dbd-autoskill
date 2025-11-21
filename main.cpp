#include <windows.h>
#include <iostream>
#include <fstream>

bool SaveBitmapToFile(HBITMAP hBitmap, HDC hDC, const char* filename) {
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);
    
    BITMAPINFOHEADER bi = {0};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = bmp.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    
    DWORD dwBmpSize = ((bmp.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmp.bmHeight;
    BYTE* lpBits = new BYTE[dwBmpSize];
    
    GetDIBits(hDC, hBitmap, 0, bmp.bmHeight, lpBits, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    
    BITMAPFILEHEADER bfh = {0};
    bfh.bfType = 0x4D42; // "BM"
    bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwBmpSize;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        delete[] lpBits;
        return false;
    }
    
    file.write((char*)&bfh, sizeof(BITMAPFILEHEADER));
    file.write((char*)&bi, sizeof(BITMAPINFOHEADER));
    file.write((char*)lpBits, dwBmpSize);
    file.close();
    
    delete[] lpBits;
    return true;
}

bool CaptureScreenCenter(int width, int height, const char* filename) {
    // Get screen dimensions
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // Calculate center position
    int x = (screenWidth - width) / 2;
    int y = (screenHeight - height) / 2;
    
    // Get screen DC
    HDC hScreenDC = GetDC(NULL);
    if (!hScreenDC) {
        std::cerr << "Failed to get screen DC" << std::endl;
        return false;
    }
    
    // Create compatible DC and bitmap
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);
    
    // Copy screen region to memory DC
    bool success = BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, x, y, SRCCOPY);
    
    if (success) {
        // Save bitmap to file
        success = SaveBitmapToFile(hBitmap, hMemoryDC, filename);
    }
    
    // Cleanup
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
    
    return success;
}

int main() {
    SetProcessDPIAware();
    const char* filename = "screenshot_center.bmp";
    
    std::cout << "Capturing 500x500 screenshot from center of screen..." << std::endl;
    
    if (CaptureScreenCenter(200, 200, filename)) {
        std::cout << "Screenshot saved successfully to: " << filename << std::endl;
        return 0;
    } else {
        std::cerr << "Failed to capture screenshot!" << std::endl;
        return 1;
    }
}