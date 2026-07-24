// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "main.h"
#include "shellintegration.h"

static const WCHAR c_drive_verb[] = TEXT("Software\\Classes\\Drive\\shell\\Elucidisk");
static const WCHAR c_directory_verb[] = TEXT("Software\\Classes\\Directory\\shell\\Elucidisk");
static const WCHAR c_drive_caption[] = TEXT("Scan drive with Elucidisk");
static const WCHAR c_directory_caption[] = TEXT("Scan folder with Elucidisk");

static LONG GetExecutablePath(std::wstring& path)
{
    std::vector<WCHAR> buffer(MAX_PATH);

    for (;;)
    {
        SetLastError(ERROR_SUCCESS);
        const DWORD length = GetModuleFileNameW(0, buffer.data(), DWORD(buffer.size()));
        if (!length)
            return GetLastError();

        if (length < buffer.size() - 1 ||
            (length < buffer.size() && buffer[length] == L'\0'))
        {
            path.assign(buffer.data(), length);
            return ERROR_SUCCESS;
        }

        if (buffer.size() >= 32768)
            return ERROR_INSUFFICIENT_BUFFER;
        buffer.resize(buffer.size() * 2);
    }
}

static LONG SetStringValue(HKEY key, const WCHAR* name, const std::wstring& value)
{
    return RegSetValueExW(key, name, 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(value.c_str()),
                          DWORD((value.length() + 1) * sizeof(WCHAR)));
}

static LONG RegisterVerb(const WCHAR* verb, const WCHAR* caption, const std::wstring& executable)
{
    HKEY raw_key = nullptr;
    LONG result = RegCreateKeyExW(HKEY_CURRENT_USER, verb, 0, nullptr,
                                  REG_OPTION_NON_VOLATILE, KEY_SET_VALUE,
                                  nullptr, &raw_key, nullptr);
    if (result != ERROR_SUCCESS)
        return result;

    result = SetStringValue(raw_key, nullptr, caption);
    if (result == ERROR_SUCCESS)
        result = SetStringValue(raw_key, L"Icon", executable);
    RegCloseKey(raw_key);

    if (result != ERROR_SUCCESS)
    {
        RegDeleteTreeW(HKEY_CURRENT_USER, verb);
        return result;
    }

    std::wstring command_key(verb);
    command_key += L"\\command";
    raw_key = nullptr;
    result = RegCreateKeyExW(HKEY_CURRENT_USER, command_key.c_str(), 0, nullptr,
                             REG_OPTION_NON_VOLATILE, KEY_SET_VALUE,
                             nullptr, &raw_key, nullptr);
    if (result != ERROR_SUCCESS)
    {
        RegDeleteTreeW(HKEY_CURRENT_USER, verb);
        return result;
    }

    const std::wstring command = L"\"" + executable + L"\" \"%1\"";
    result = SetStringValue(raw_key, nullptr, command);
    RegCloseKey(raw_key);
    if (result != ERROR_SUCCESS)
        RegDeleteTreeW(HKEY_CURRENT_USER, verb);
    return result;
}

LONG RegisterShellIntegration()
{
    std::wstring executable;
    const LONG result = GetExecutablePath(executable);
    if (result != ERROR_SUCCESS)
        return result;

    const LONG drive_result = RegisterVerb(c_drive_verb, c_drive_caption, executable);
    const LONG directory_result = RegisterVerb(c_directory_verb, c_directory_caption, executable);

    if (drive_result != ERROR_SUCCESS)
        return drive_result;
    if (directory_result != ERROR_SUCCESS)
        return directory_result;
    return ERROR_SUCCESS;
}

LONG UnregisterShellIntegration()
{
    const LONG drive_result = RegDeleteTreeW(HKEY_CURRENT_USER, c_drive_verb);
    const LONG directory_result = RegDeleteTreeW(HKEY_CURRENT_USER, c_directory_verb);

    if (drive_result != ERROR_SUCCESS && drive_result != ERROR_FILE_NOT_FOUND)
        return drive_result;
    if (directory_result != ERROR_SUCCESS && directory_result != ERROR_FILE_NOT_FOUND)
        return directory_result;
    return ERROR_SUCCESS;
}

static LONG IsVerbRegistered(const WCHAR* verb, bool& registered)
{
    HKEY key = nullptr;
    const LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, verb, 0, KEY_READ, &key);
    if (result == ERROR_SUCCESS)
    {
        RegCloseKey(key);
        registered = true;
        return ERROR_SUCCESS;
    }

    if (result == ERROR_FILE_NOT_FOUND || result == ERROR_PATH_NOT_FOUND)
    {
        registered = false;
        return ERROR_SUCCESS;
    }

    return result;
}

int IsShellIntegrationRegistered()
{
    bool drive_registered = false;
    bool directory_registered = false;
    const LONG drive_result = IsVerbRegistered(c_drive_verb, drive_registered);
    const LONG directory_result = IsVerbRegistered(c_directory_verb, directory_registered);

    if (drive_result != ERROR_SUCCESS || directory_result != ERROR_SUCCESS)
        return -1;
    return drive_registered || directory_registered;
}
