// Compile with: g++ -O3 -std=c++11 main.cpp -o screenshot.exe -lgdi32

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <thread>
#include <mutex>
#include <random>
#include <fstream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <conio.h>

// ============================================================================
// DEFAULT CONFIGURATION - Used when creating new config.json
// ============================================================================

// Capture settings
int CAPTURE_SIZE = 186;
int CAPTURE_POS_X = 1187;
int CAPTURE_POS_Y = 607;
int FPS = 90;

// Ring detection settings
double RING_OUTER_RADIUS = 89.0;
double RING_INNER_RADIUS = 85.0;
int RING_CENTER_OFFSET_X = -1;
int RING_CENTER_OFFSET_Y = 0;

// Safety check rectangles
int SAFETY_RECT1_X = 63;
int SAFETY_RECT1_Y = 79;
int SAFETY_RECT1_WIDTH = 60;
int SAFETY_RECT1_HEIGHT = 6;
int SAFETY_RECT2_X = 63;
int SAFETY_RECT2_Y = 103;
int SAFETY_RECT2_WIDTH = 60;
int SAFETY_RECT2_HEIGHT = 4;

// Detection thresholds
int MIN_WHITE_PIXELS = 30;
int MIN_RED_PIXELS = 2;
int TIMER_DURATION_MS = 1200;
int RESET_DELAY_MS = 200;

// Color thresholds (0-255)
int WHITE_THRESHOLD = 0xFE;
int RED_THRESHOLD = 50;
int OTHER_CHANNEL_MAX = 150;
int RED_DOMINANCE = 20;

// Key press settings
int SPACE_PRESS_MIN_MS = 50;
int SPACE_PRESS_MAX_MS = 90;

// Save settings
bool SAVE_ENABLED = true;

// ============================================================================
// GLOBALS
// ============================================================================

std::mutex saveMutex;

// ============================================================================
// JSON CONFIGURATION
// ============================================================================

void SaveConfigToJson(const char* filename) {
    std::ofstream file(filename);
    if (!file) {
        std::cerr << "Failed to create config file\n";
        return;
    }
    
    file << "{\n";
    file << "  \"capture_size\": " << CAPTURE_SIZE << ",\n";
    file << "  \"capture_pos_x\": " << CAPTURE_POS_X << ",\n";
    file << "  \"capture_pos_y\": " << CAPTURE_POS_Y << ",\n";
    file << "  \"fps\": " << FPS << ",\n";
    file << "  \"ring_outer_radius\": " << RING_OUTER_RADIUS << ",\n";
    file << "  \"ring_inner_radius\": " << RING_INNER_RADIUS << ",\n";
    file << "  \"ring_center_offset_x\": " << RING_CENTER_OFFSET_X << ",\n";
    file << "  \"ring_center_offset_y\": " << RING_CENTER_OFFSET_Y << ",\n";
    file << "  \"safety_rect1_x\": " << SAFETY_RECT1_X << ",\n";
    file << "  \"safety_rect1_y\": " << SAFETY_RECT1_Y << ",\n";
    file << "  \"safety_rect1_width\": " << SAFETY_RECT1_WIDTH << ",\n";
    file << "  \"safety_rect1_height\": " << SAFETY_RECT1_HEIGHT << ",\n";
    file << "  \"safety_rect2_x\": " << SAFETY_RECT2_X << ",\n";
    file << "  \"safety_rect2_y\": " << SAFETY_RECT2_Y << ",\n";
    file << "  \"safety_rect2_width\": " << SAFETY_RECT2_WIDTH << ",\n";
    file << "  \"safety_rect2_height\": " << SAFETY_RECT2_HEIGHT << ",\n";
    file << "  \"min_white_pixels\": " << MIN_WHITE_PIXELS << ",\n";
    file << "  \"min_red_pixels\": " << MIN_RED_PIXELS << ",\n";
    file << "  \"timer_duration_ms\": " << TIMER_DURATION_MS << ",\n";
    file << "  \"reset_delay_ms\": " << RESET_DELAY_MS << ",\n";
    file << "  \"white_threshold\": " << WHITE_THRESHOLD << ",\n";
    file << "  \"red_threshold\": " << RED_THRESHOLD << ",\n";
    file << "  \"other_channel_max\": " << OTHER_CHANNEL_MAX << ",\n";
    file << "  \"red_dominance\": " << RED_DOMINANCE << ",\n";
    file << "  \"space_press_min_ms\": " << SPACE_PRESS_MIN_MS << ",\n";
    file << "  \"space_press_max_ms\": " << SPACE_PRESS_MAX_MS << ",\n";
    file << "  \"save_enabled\": " << (SAVE_ENABLED ? "true" : "false") << "\n";
    file << "}\n";
    
    file.close();
    std::cout << "Configuration saved to " << filename << "\n";
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r\"");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r\",");
    return str.substr(first, (last - first + 1));
}

bool LoadConfigFromJson(const char* filename) {
    std::ifstream file(filename);
    if (!file) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        
        std::string key = trim(line.substr(0, colon));
        std::string value = trim(line.substr(colon + 1));
        
        if (key == "capture_size") CAPTURE_SIZE = std::stoi(value);
        else if (key == "capture_pos_x") CAPTURE_POS_X = std::stoi(value);
        else if (key == "capture_pos_y") CAPTURE_POS_Y = std::stoi(value);
        else if (key == "fps") FPS = std::stoi(value);
        else if (key == "ring_outer_radius") RING_OUTER_RADIUS = std::stod(value);
        else if (key == "ring_inner_radius") RING_INNER_RADIUS = std::stod(value);
        else if (key == "ring_center_offset_x") RING_CENTER_OFFSET_X = std::stoi(value);
        else if (key == "ring_center_offset_y") RING_CENTER_OFFSET_Y = std::stoi(value);
        else if (key == "safety_rect1_x") SAFETY_RECT1_X = std::stoi(value);
        else if (key == "safety_rect1_y") SAFETY_RECT1_Y = std::stoi(value);
        else if (key == "safety_rect1_width") SAFETY_RECT1_WIDTH = std::stoi(value);
        else if (key == "safety_rect1_height") SAFETY_RECT1_HEIGHT = std::stoi(value);
        else if (key == "safety_rect2_x") SAFETY_RECT2_X = std::stoi(value);
        else if (key == "safety_rect2_y") SAFETY_RECT2_Y = std::stoi(value);
        else if (key == "safety_rect2_width") SAFETY_RECT2_WIDTH = std::stoi(value);
        else if (key == "safety_rect2_height") SAFETY_RECT2_HEIGHT = std::stoi(value);
        else if (key == "min_white_pixels") MIN_WHITE_PIXELS = std::stoi(value);
        else if (key == "min_red_pixels") MIN_RED_PIXELS = std::stoi(value);
        else if (key == "timer_duration_ms") TIMER_DURATION_MS = std::stoi(value);
        else if (key == "reset_delay_ms") RESET_DELAY_MS = std::stoi(value);
        else if (key == "white_threshold") WHITE_THRESHOLD = std::stoi(value);
        else if (key == "red_threshold") RED_THRESHOLD = std::stoi(value);
        else if (key == "other_channel_max") OTHER_CHANNEL_MAX = std::stoi(value);
        else if (key == "red_dominance") RED_DOMINANCE = std::stoi(value);
        else if (key == "space_press_min_ms") SPACE_PRESS_MIN_MS = std::stoi(value);
        else if (key == "space_press_max_ms") SPACE_PRESS_MAX_MS = std::stoi(value);
        else if (key == "save_enabled") SAVE_ENABLED = (value == "true");
    }
    
    file.close();
    return true;
}

// ============================================================================
// STRUCTURES
// ============================================================================

struct PixelPos {
    int x, y;
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void OpenIndexHtml() {
    // Open index.html with default browser
    ShellExecute(NULL, "open", "index.html", NULL, NULL, SW_SHOWNORMAL);
}

void PressSpaceKey() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(SPACE_PRESS_MIN_MS, SPACE_PRESS_MAX_MS);
    int pressDuration = dis(gen);
    
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_SPACE;
    input.ki.dwFlags = 0;
    
    SendInput(1, &input, sizeof(INPUT));
    Sleep(pressDuration);
    
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

void SaveBitmapWithHighlights(const char* filename, BYTE* originalBits, int width, int height,
                               const std::vector<PixelPos>& whitePixels,
                               const std::vector<PixelPos>& redPixels) {
    std::lock_guard<std::mutex> lock(saveMutex);
    
    int rowSize = ((width * 3 + 3) / 4) * 4;
    DWORD bmpSize = rowSize * height;
    
    BYTE* bits = new BYTE[bmpSize];
    memcpy(bits, originalBits, bmpSize);
    
    // Mark safety rectangles in BLUE
    for (int y = SAFETY_RECT1_Y; y < SAFETY_RECT1_Y + SAFETY_RECT1_HEIGHT; y++) {
        for (int x = SAFETY_RECT1_X; x < SAFETY_RECT1_X + SAFETY_RECT1_WIDTH; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                int index = y * rowSize + x * 3;
                bits[index + 0] = 0xFF; bits[index + 1] = 0x00; bits[index + 2] = 0x00;
            }
        }
    }
    for (int y = SAFETY_RECT2_Y; y < SAFETY_RECT2_Y + SAFETY_RECT2_HEIGHT; y++) {
        for (int x = SAFETY_RECT2_X; x < SAFETY_RECT2_X + SAFETY_RECT2_WIDTH; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                int index = y * rowSize + x * 3;
                bits[index + 0] = 0xFF; bits[index + 1] = 0x00; bits[index + 2] = 0x00;
            }
        }
    }
    
    // Mark white pixels in PINK
    for (const auto& pos : whitePixels) {
        if (pos.x >= 0 && pos.x < width && pos.y >= 0 && pos.y < height) {
            int index = pos.y * rowSize + pos.x * 3;
            bits[index + 0] = 0xFF; bits[index + 1] = 0x00; bits[index + 2] = 0xFF;
        }
    }
    
    // Mark red pixels in YELLOW
    for (const auto& pos : redPixels) {
        if (pos.x >= 0 && pos.x < width && pos.y >= 0 && pos.y < height) {
            int index = pos.y * rowSize + pos.x * 3;
            bits[index + 0] = 0x00; bits[index + 1] = 0xFF; bits[index + 2] = 0xFF;
        }
    }
    
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

std::vector<std::vector<PixelPos>> FindConnectedGroups(
    const std::vector<PixelPos>& pixels, int width, int height, int minSize) {
    
    std::vector<std::vector<PixelPos>> groups;
    std::vector<bool> visited(pixels.size(), false);
    
    std::vector<std::vector<bool>> pixelMap(height, std::vector<bool>(width, false));
    for (const auto& p : pixels) {
        if (p.x >= 0 && p.x < width && p.y >= 0 && p.y < height) {
            pixelMap[p.y][p.x] = true;
        }
    }
    
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
            
            int dx[] = {0, 0, -1, 1};
            int dy[] = {-1, 1, 0, 0};
            
            for (int d = 0; d < 4; d++) {
                int nx = current.x + dx[d];
                int ny = current.y + dy[d];
                
                if (nx >= 0 && nx < width && ny >= 0 && ny < height && pixelMap[ny][nx]) {
                    for (size_t j = 0; j < pixels.size(); j++) {
                        if (!visited[j] && pixels[j].x == nx && pixels[j].y == ny) {
                            toVisit.push_back(j);
                            break;
                        }
                    }
                }
            }
        }
        
        if (group.size() >= minSize) {
            groups.push_back(group);
        }
    }
    
    return groups;
}

// ============================================================================
// PIXEL CHECKING FUNCTIONS
// ============================================================================

bool IsInRing(int x, int y, int centerX, int centerY, double innerRadius, double outerRadius) {
    double dx = x - centerX;
    double dy = y - centerY;
    double distSq = dx * dx + dy * dy;
    double innerSq = innerRadius * innerRadius;
    double outerSq = outerRadius * outerRadius;
    return distSq > innerSq && distSq < outerSq;
}

bool IsWhiteish(BYTE r, BYTE g, BYTE b) {
    return r >= WHITE_THRESHOLD && g >= WHITE_THRESHOLD && b >= WHITE_THRESHOLD;
}

bool IsBlack(BYTE r, BYTE g, BYTE b) {
    return r == 0 && g == 0 && b == 0;
}

bool IsReddish(BYTE r, BYTE g, BYTE b) {
    return r >= RED_THRESHOLD && 
           g < OTHER_CHANNEL_MAX && 
           b < OTHER_CHANNEL_MAX &&
           r >= (g + RED_DOMINANCE) &&
           r >= (b + RED_DOMINANCE);
}

bool IsRectangleBlack(BYTE* buf, int bufWidth, int x, int y, int width, int height) {
    int rowSize = ((bufWidth * 3 + 3) / 4) * 4;
    
    for (int py = y; py < y + height; py++) {
        for (int px = x; px < x + width; px++) {
            if (px >= 0 && px < bufWidth && py >= 0) {
                int index = py * rowSize + px * 3;
                BYTE b = buf[index + 0];
                BYTE g = buf[index + 1];
                BYTE r = buf[index + 2];
                
                if (!IsBlack(r, g, b)) {
                    return false;
                }
            }
        }
    }
    return true;
}

// ============================================================================
// MAIN CAPTURE AND PROCESSING
// ============================================================================

bool CaptureAndProcess(int size, int posX, int posY, 
                       std::vector<PixelPos>& whitePixels,
                       std::vector<PixelPos>& redPixels,
                       bool& firstCondition, bool& secondCondition,
                       LARGE_INTEGER& timerStart, LARGE_INTEGER freq) {
    
    HDC hScreen = GetDC(NULL);
    HDC hMem = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, size, size);
    HBITMAP old = (HBITMAP)SelectObject(hMem, hBitmap);

    bool ok = BitBlt(hMem, 0, 0, size, size, hScreen, posX, posY, SRCCOPY);

    if (!ok) {
        SelectObject(hMem, old);
        DeleteObject(hBitmap);
        DeleteDC(hMem);
        ReleaseDC(NULL, hScreen);
        return false;
    }

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
    int centerX = size / 2 + RING_CENTER_OFFSET_X;
    int centerY = size / 2 + RING_CENTER_OFFSET_Y;

    if (!firstCondition) {
        whitePixels.clear();
        redPixels.clear();
        
        bool rect1Black = IsRectangleBlack(buf, size, SAFETY_RECT1_X, SAFETY_RECT1_Y, 
                                           SAFETY_RECT1_WIDTH, SAFETY_RECT1_HEIGHT);
        bool rect2Black = IsRectangleBlack(buf, size, SAFETY_RECT2_X, SAFETY_RECT2_Y, 
                                           SAFETY_RECT2_WIDTH, SAFETY_RECT2_HEIGHT);
        
        if (!rect1Black || !rect2Black) {
            delete[] buf;
            SelectObject(hMem, old);
            DeleteObject(hBitmap);
            DeleteDC(hMem);
            ReleaseDC(NULL, hScreen);
            return false;
        }
        
        std::vector<PixelPos> candidatePixels;
        
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
        
        std::vector<std::vector<PixelPos>> groups = 
            FindConnectedGroups(candidatePixels, size, size, MIN_WHITE_PIXELS);
        
        for (const auto& group : groups) {
            for (const auto& pixel : group) {
                whitePixels.push_back(pixel);
            }
        }
        
        if (!whitePixels.empty()) {
            firstCondition = true;
            QueryPerformanceCounter(&timerStart);
        }
    }
    else {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double elapsed = (now.QuadPart - timerStart.QuadPart) * 1000.0 / freq.QuadPart;
        
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
        
        // DOUBLE-CHECK: Verify safety rectangles are still black
        bool rect1Black = IsRectangleBlack(buf, size, SAFETY_RECT1_X, SAFETY_RECT1_Y, 
                                           SAFETY_RECT1_WIDTH, SAFETY_RECT1_HEIGHT);
        bool rect2Black = IsRectangleBlack(buf, size, SAFETY_RECT2_X, SAFETY_RECT2_Y, 
                                           SAFETY_RECT2_WIDTH, SAFETY_RECT2_HEIGHT);
        
        if (!rect1Black || !rect2Black) {
            // Safety rectangles no longer black, reset condition
            std::cout << "Safety check failed in second condition - resetting\n";
            firstCondition = false;
            whitePixels.clear();
            redPixels.clear();
            delete[] buf;
            SelectObject(hMem, old);
            DeleteObject(hBitmap);
            DeleteDC(hMem);
            ReleaseDC(NULL, hScreen);
            return false;
        }
        
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
        
        if (redPixels.size() >= MIN_RED_PIXELS) {
            secondCondition = true;
            
            std::thread spaceThread([]() {
                PressSpaceKey();
            });
            spaceThread.detach();
            
            if (SAVE_ENABLED) {
                BYTE* bufCopy = new BYTE[sizeBytes];
                memcpy(bufCopy, buf, sizeBytes);
                
                std::vector<PixelPos> whiteCopy = whitePixels;
                std::vector<PixelPos> redCopy = redPixels;
                
                std::thread saveThread([bufCopy, size, whiteCopy, redCopy]() {
                    SaveBitmapWithHighlights("output.bmp", bufCopy, size, size, whiteCopy, redCopy);
                    delete[] bufCopy;
                });
                saveThread.detach();
            }
        }
    }

    delete[] buf;
    SelectObject(hMem, old);
    DeleteObject(hBitmap);
    DeleteDC(hMem);
    ReleaseDC(NULL, hScreen);
    
    return secondCondition;
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    SetProcessDPIAware();

    const char* configFile = "config.json";
    
    // Open index.html in browser
    OpenIndexHtml();
    
    std::cout << "=== DBD Auto-Skill Check ===\n\n";
    
    // 5 second countdown with reset detection
    const int COUNTDOWN_SECONDS = 5;
    bool resetRequested = false;
    std::string inputBuffer;
    
    std::cout << "Type 'reset' to restore default settings\n";
    std::cout << "Starting in ";
    
    auto startTime = std::chrono::steady_clock::now();
    auto endTime = startTime + std::chrono::seconds(COUNTDOWN_SECONDS);
    
    while (std::chrono::steady_clock::now() < endTime) {
        // Check for keyboard input (non-blocking)
        if (_kbhit()) {
            char c = _getch();
            inputBuffer += std::tolower(c);
            
            // Check if "reset" is in buffer
            if (inputBuffer.find("reset") != std::string::npos) {
                resetRequested = true;
                std::cout << "\n\nReset requested!\n";
                break;
            }
        }
        
        // Calculate remaining time
        auto now = std::chrono::steady_clock::now();
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - now);
        int remainingSeconds = remaining.count() / 1000;
        int progress = ((COUNTDOWN_SECONDS - remainingSeconds) * 100) / COUNTDOWN_SECONDS;
        
        // Update progress bar (only every 100ms to reduce flicker)
        static int lastProgress = -1;
        if (progress != lastProgress) {
            std::cout << "\r";
            std::cout << remainingSeconds + 1 << "s [";
            for (int i = 0; i < 20; i++) {
                if (i < progress / 5) {
                    std::cout << "=";
                } else {
                    std::cout << " ";
                }
            }
            std::cout << "] " << progress << "%  ";
            std::cout.flush();
            lastProgress = progress;
        }
        
        Sleep(50);
    }
    
    std::cout << "\n\n";
    
    // Handle reset or load config
    if (resetRequested) {
        std::cout << "Resetting configuration to defaults...\n";
        SaveConfigToJson(configFile);
    } else {
        if (LoadConfigFromJson(configFile)) {
            std::cout << "Configuration loaded from " << configFile << "\n";
        } else {
            std::cout << "Config file not found, creating default " << configFile << "\n";
            SaveConfigToJson(configFile);
        }
    }

    double frameDelay = 1000.0 / FPS;

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    std::cout << "\n=== ACTIVE CONFIGURATION ===\n";
    std::cout << "Capture: " << CAPTURE_SIZE << "x" << CAPTURE_SIZE
              << " at (" << CAPTURE_POS_X << "," << CAPTURE_POS_Y << ") @ " << FPS << " FPS\n";
    std::cout << "Safety rectangles (must be black):\n";
    std::cout << "  R1: (" << SAFETY_RECT1_X << "," << SAFETY_RECT1_Y << ") " 
              << SAFETY_RECT1_WIDTH << "x" << SAFETY_RECT1_HEIGHT << "\n";
    std::cout << "  R2: (" << SAFETY_RECT2_X << "," << SAFETY_RECT2_Y << ") " 
              << SAFETY_RECT2_WIDTH << "x" << SAFETY_RECT2_HEIGHT << "\n";
    std::cout << "Ring: inner=" << RING_INNER_RADIUS << " outer=" << RING_OUTER_RADIUS 
              << " offset=(" << RING_CENTER_OFFSET_X << "," << RING_CENTER_OFFSET_Y << ")\n";
    std::cout << "Conditions: white>=" << MIN_WHITE_PIXELS << " (connected), red>=" << MIN_RED_PIXELS << "\n";
    std::cout << "Thresholds: white>=" << WHITE_THRESHOLD << ", red>=" << RED_THRESHOLD 
              << ", other<" << OTHER_CHANNEL_MAX << ", dominance=" << RED_DOMINANCE << "\n";
    std::cout << "Timing: timer=" << TIMER_DURATION_MS << "ms, reset=" << RESET_DELAY_MS 
              << "ms, space=" << SPACE_PRESS_MIN_MS << "-" << SPACE_PRESS_MAX_MS << "ms\n";
    std::cout << "Save: " << (SAVE_ENABLED ? "yes" : "no") << "\n";
    std::cout << "============================\n\n";

    std::vector<PixelPos> whitePixels;
    std::vector<PixelPos> redPixels;
    bool firstCondition = false;
    bool secondCondition = false;
    LARGE_INTEGER timerStart;

    while (true) {
        LARGE_INTEGER frameStart;
        QueryPerformanceCounter(&frameStart);

        CaptureAndProcess(CAPTURE_SIZE, CAPTURE_POS_X, CAPTURE_POS_Y, 
                         whitePixels, redPixels,
                         firstCondition, secondCondition,
                         timerStart, freq);

        if (secondCondition) {
            std::cout << "SECOND CONDITION TRUE (detected " << redPixels.size() << " red pixels)\n";
            
            Sleep(RESET_DELAY_MS);
            firstCondition = false;
            secondCondition = false;
            whitePixels.clear();
            redPixels.clear();
        }

        while (true) {
            LARGE_INTEGER cur;
            QueryPerformanceCounter(&cur);
            if ((cur.QuadPart - frameStart.QuadPart) * 1000.0 / freq.QuadPart >= frameDelay)
                break;
        }
    }
}