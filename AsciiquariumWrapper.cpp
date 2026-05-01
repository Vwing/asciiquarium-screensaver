#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <scrnsave.h>
#include <string>
#include <sstream>
#include <vector>

#pragma comment(lib, "scrnsave.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")

#define REGISTRY_KEY "Software\\AsciiquariumScreensaver"

PROCESS_INFORMATION g_ChildProcess = {0};
HANDLE              g_ExitEvent    = NULL;
std::wstring        g_ExitEventName;

static std::wstring GetAppPath() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring p(path);
    size_t slash = p.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
        return L"AsciiquariumApp.exe";
    return p.substr(0, slash + 1) + L"AsciiquariumApp.exe";
}

static void CreateExitEvent() {
    DWORD pid = GetCurrentProcessId();
    DWORD tid = GetCurrentThreadId();
    SYSTEMTIME st;
    GetSystemTime(&st);
    std::wstringstream ss;
    ss << L"Global\\AsciiquariumExit_" << pid << L"_" << tid
       << L"_" << st.wMilliseconds;
    g_ExitEventName = ss.str();
    g_ExitEvent = CreateEventW(NULL, TRUE, FALSE, g_ExitEventName.c_str());
}

static PROCESS_INFORMATION LaunchChild() {
    PROCESS_INFORMATION pi = {0};
    STARTUPINFOW si = {sizeof(si)};

    std::wstring appPath = GetAppPath();
    if (GetFileAttributesW(appPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::wstring err = L"Cannot find AsciiquariumApp.exe at:\n" + appPath;
        MessageBoxW(NULL, err.c_str(), L"Asciiquarium Screensaver", MB_OK | MB_ICONERROR);
        return pi;
    }

    std::wstring cmdLine = L"\"" + appPath + L"\" --exitEvent " + g_ExitEventName;
    std::vector<wchar_t> buf(cmdLine.begin(), cmdLine.end());
    buf.push_back(0);

    CreateProcessW(NULL, buf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    return pi;
}

LRESULT WINAPI ScreenSaverProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static bool started = false;

    switch (msg) {
    case WM_CREATE:
        if (!started) {
            started = true;
            CreateExitEvent();
            g_ChildProcess = LaunchChild();
            if (!g_ChildProcess.hProcess) {
                PostQuitMessage(0);
                return -1;
            }
        }
        return 0;

    case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MOUSEMOVE: {
        if (msg == WM_MOUSEMOVE) {
            static POINT last = {-1, -1};
            POINT cur = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (last.x == -1) { last = cur; return 0; }
            if (last.x == cur.x && last.y == cur.y) return 0;
            last = cur;
        }
        if (g_ExitEvent)
            SetEvent(g_ExitEvent);
        if (g_ChildProcess.hProcess) {
            WaitForSingleObject(g_ChildProcess.hProcess, 1000);
            CloseHandle(g_ChildProcess.hProcess);
            CloseHandle(g_ChildProcess.hThread);
            g_ChildProcess = {0};
        }
        PostQuitMessage(0);
        return 0;
    }

    case WM_DESTROY:
        if (g_ExitEvent) {
            SetEvent(g_ExitEvent);
            CloseHandle(g_ExitEvent);
            g_ExitEvent = NULL;
        }
        if (g_ChildProcess.hProcess) {
            WaitForSingleObject(g_ChildProcess.hProcess, 1000);
            TerminateProcess(g_ChildProcess.hProcess, 0);
            CloseHandle(g_ChildProcess.hProcess);
            CloseHandle(g_ChildProcess.hThread);
            g_ChildProcess = {0};
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefScreenSaverProc(hwnd, msg, wParam, lParam);
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND hDlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG:
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return TRUE;
}
