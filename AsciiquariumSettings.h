#pragma once

#include <windows.h>
#include <algorithm>

static const char* const kAsciiquariumRegistryKey =
    "Software\\AsciiquariumScreensaver";

struct AsciiquariumSettings {
    DWORD rows;
    DWORD speedPercent;
    DWORD fishPercent;
    DWORD seaweedPercent;
    DWORD bubblePercent;

    AsciiquariumSettings()
        : rows(96), speedPercent(100), fishPercent(100), seaweedPercent(100),
          bubblePercent(100) {}
};

inline DWORD clampSetting(DWORD value, DWORD minimum, DWORD maximum) {
    return std::max(minimum, std::min(maximum, value));
}

inline void normalizeSettings(AsciiquariumSettings& settings) {
    settings.rows = clampSetting(settings.rows, 40, 500);
    settings.speedPercent = clampSetting(settings.speedPercent, 25, 300);
    settings.fishPercent = clampSetting(settings.fishPercent, 25, 300);
    settings.seaweedPercent = clampSetting(settings.seaweedPercent, 0, 300);
    settings.bubblePercent = clampSetting(settings.bubblePercent, 0, 300);
}

inline void readRegistryDword(HKEY key, const char* name, DWORD& value) {
    DWORD type = 0;
    DWORD size = sizeof(value);
    DWORD stored = 0;
    if (RegQueryValueExA(key, name, NULL, &type,
                         reinterpret_cast<BYTE*>(&stored), &size) == ERROR_SUCCESS &&
        type == REG_DWORD && size == sizeof(stored)) {
        value = stored;
    }
}

inline AsciiquariumSettings loadAsciiquariumSettings() {
    AsciiquariumSettings settings;
    HKEY key = NULL;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, kAsciiquariumRegistryKey, 0,
                      KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
        readRegistryDword(key, "Rows", settings.rows);
        readRegistryDword(key, "SpeedPercent", settings.speedPercent);
        readRegistryDword(key, "FishPercent", settings.fishPercent);
        readRegistryDword(key, "SeaweedPercent", settings.seaweedPercent);
        readRegistryDword(key, "BubblePercent", settings.bubblePercent);
        RegCloseKey(key);
    }
    normalizeSettings(settings);
    return settings;
}

inline bool saveAsciiquariumSettings(AsciiquariumSettings settings) {
    normalizeSettings(settings);
    HKEY key = NULL;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, kAsciiquariumRegistryKey, 0, NULL, 0,
                        KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS) {
        return false;
    }
    bool ok =
        RegSetValueExA(key, "Rows", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&settings.rows), sizeof(DWORD)) == ERROR_SUCCESS &&
        RegSetValueExA(key, "SpeedPercent", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&settings.speedPercent), sizeof(DWORD)) == ERROR_SUCCESS &&
        RegSetValueExA(key, "FishPercent", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&settings.fishPercent), sizeof(DWORD)) == ERROR_SUCCESS &&
        RegSetValueExA(key, "SeaweedPercent", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&settings.seaweedPercent), sizeof(DWORD)) == ERROR_SUCCESS &&
        RegSetValueExA(key, "BubblePercent", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&settings.bubblePercent), sizeof(DWORD)) == ERROR_SUCCESS;
    RegCloseKey(key);
    return ok;
}
