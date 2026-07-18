#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <scrnsave.h>
#include <string>
#include <sstream>
#include <vector>
#include "resource.h"
#include "AsciiquariumSettings.h"

#pragma comment(lib, "scrnsave.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")

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

static bool IsPreviewWindow(HWND hwnd) {
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    return GetParent(hwnd) != NULL || (style & WS_CHILD) != 0;
}

static PROCESS_INFORMATION LaunchChild(HWND ownerHwnd, bool previewMode) {
    PROCESS_INFORMATION pi = {0};
    STARTUPINFOW si = {sizeof(si)};

    std::wstring appPath = GetAppPath();
    if (GetFileAttributesW(appPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::wstring err = L"Cannot find AsciiquariumApp.exe at:\n" + appPath;
        MessageBoxW(NULL, err.c_str(), L"Asciiquarium Screensaver", MB_OK | MB_ICONERROR);
        return pi;
    }

    std::wstringstream cmd;
    cmd << L"\"" << appPath << L"\" --exitEvent " << g_ExitEventName
        << L" --owner " << reinterpret_cast<UINT_PTR>(ownerHwnd);
    if (previewMode)
        cmd << L" --preview";
    std::wstring cmdLine = cmd.str();
    std::vector<wchar_t> buf(cmdLine.begin(), cmdLine.end());
    buf.push_back(0);

    CreateProcessW(NULL, buf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    return pi;
}

LRESULT WINAPI ScreenSaverProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static bool started = false;
    static bool previewMode = false;

    switch (msg) {
    case WM_CREATE:
        if (!started) {
            started = true;
            previewMode = IsPreviewWindow(hwnd);
            CreateExitEvent();
            g_ChildProcess = LaunchChild(hwnd, previewMode);
            if (!g_ChildProcess.hProcess) {
                PostQuitMessage(0);
                return -1;
            }
        }
        return 0;

    case WM_ACTIVATE:
    case WM_ACTIVATEAPP:
    case WM_NCACTIVATE:
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MOUSEMOVE: {
        if (previewMode)
            return 0;

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
    static AsciiquariumSettings settings;
    auto setFields = [hDlg]() {
        SetDlgItemInt(hDlg, IDC_ROWS, settings.rows, FALSE);
        SetDlgItemInt(hDlg, IDC_SPEED, settings.speedPercent, FALSE);
        SetDlgItemInt(hDlg, IDC_FISH, settings.fishPercent, FALSE);
        SetDlgItemInt(hDlg, IDC_SEAWEED, settings.seaweedPercent, FALSE);
        SetDlgItemInt(hDlg, IDC_BUBBLES, settings.bubblePercent, FALSE);
    };
    switch (msg) {
    case WM_INITDIALOG:
        settings = loadAsciiquariumSettings();
        setFields();
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_DEFAULTS) {
            settings = AsciiquariumSettings();
            setFields();
            return TRUE;
        }
        if (LOWORD(wParam) == IDOK) {
            BOOL validRows = FALSE, validSpeed = FALSE, validFish = FALSE;
            BOOL validSeaweed = FALSE, validBubbles = FALSE;
            settings.rows = GetDlgItemInt(hDlg, IDC_ROWS, &validRows, FALSE);
            settings.speedPercent = GetDlgItemInt(hDlg, IDC_SPEED, &validSpeed, FALSE);
            settings.fishPercent = GetDlgItemInt(hDlg, IDC_FISH, &validFish, FALSE);
            settings.seaweedPercent = GetDlgItemInt(hDlg, IDC_SEAWEED, &validSeaweed, FALSE);
            settings.bubblePercent = GetDlgItemInt(hDlg, IDC_BUBBLES, &validBubbles, FALSE);
            if (!validRows || settings.rows < 40 || settings.rows > 500 ||
                !validSpeed || settings.speedPercent < 25 || settings.speedPercent > 300 ||
                !validFish || settings.fishPercent < 25 || settings.fishPercent > 300 ||
                !validSeaweed || settings.seaweedPercent > 300 ||
                !validBubbles || settings.bubblePercent > 300) {
                MessageBoxA(hDlg,
                    "Rows must be 40-500. Speed and fish must be 25-300%. "
                    "Seaweed and bubbles must be 0-300%.",
                    "Invalid settings", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            if (!saveAsciiquariumSettings(settings)) {
                MessageBoxA(hDlg, "The settings could not be saved.",
                            "Asciiquarium Screensaver", MB_OK | MB_ICONERROR);
                return TRUE;
            }
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL WINAPI RegisterDialogClasses(HANDLE) {
    return TRUE;
}
