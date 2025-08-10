#pragma once

#include <string>

#if defined(_WIN32)
    #include <windows.h>
    #include <shlobj.h>
    #pragma comment(lib, "Shell32.lib")
#elif defined(__APPLE__) || defined(__linux__)
    #include <cstdlib>
    #include <fstream>
#endif

enum class UserDir {
    Home,
    Documents,
    Desktop,
    Downloads
};

namespace UserDirectories {

#if defined(_WIN32)

    inline std::string WideToUtf8(const std::wstring& wstr) {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, nullptr, nullptr);
        return strTo;
    }

    inline std::string GetKnownFolder(REFKNOWNFOLDERID folderId) {
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(folderId, 0, nullptr, &path))) {
            std::wstring ws(path);
            CoTaskMemFree(path);
            return WideToUtf8(ws);
        }
        return "";
    }

#endif

    inline std::string Get(UserDir dir) {
    #if defined(_WIN32)
        switch (dir) {
            case UserDir::Home:     return GetKnownFolder(FOLDERID_Profile);
            case UserDir::Documents:return GetKnownFolder(FOLDERID_Documents);
            case UserDir::Desktop:  return GetKnownFolder(FOLDERID_Desktop);
            case UserDir::Downloads:  return GetKnownFolder(FOLDERID_Downloads);
        }
    #elif defined(__APPLE__) || defined(__linux__)
        const char* home = std::getenv("HOME");
        if (!home) return "";

        switch (dir) {
            case UserDir::Home:
                return std::string(home);
            case UserDir::Documents:
                return std::string(home) + "/Documents";
            case UserDir::Desktop:
                return std::string(home) + "/Desktop";
            case UserDir::Downloads:
                return std::string(home) + "/Downloads";
        }
    #endif
        return "";
    }

}
