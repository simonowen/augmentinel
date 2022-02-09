#include "stdafx.h"
#include "Settings.h"

std::wstring settings_path;

void InitSettings(const std::string& app_name)
{
	wchar_t wpath[MAX_PATH];
	GetModuleFileName(NULL, wpath, _countof(wpath));
	fs::path path = wpath;

	// Use the application name with a .ini extension for settings.
	path.replace_filename(app_name);
	path.replace_extension(L".ini");

	// Use any existing file bundled with the application (portable support)
	if (!std::filesystem::exists(path))
	{
		auto ini_filename = path.filename();

		// Fall back on using the default AppData location.
		SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, wpath);
		path = wpath;
		path /= ini_filename;
	}

	settings_path = path;
}

std::vector<std::wstring> GetSettingKeys(const std::wstring& section)
{
	assert(!settings_path.empty());
	std::vector<wchar_t> key_strings(0x10000);	// 64K max

	GetPrivateProfileString(
		section.c_str(),
		NULL, L"",
		key_strings.data(),
		static_cast<DWORD>(key_strings.size()),
		settings_path.c_str());

	std::vector<std::wstring> keys;
	for (auto p = key_strings.data(); *p; p += wcslen(p) + 1)
		keys.emplace_back(p);

	return keys;
}

std::wstring GetSetting(const std::wstring& key, const std::wstring& default_value, const std::wstring& section)
{
	assert(!settings_path.empty());

	wchar_t value[0x400];	// 1K max
	GetPrivateProfileString(section.c_str(), key.c_str(), default_value.c_str(), value, _countof(value), settings_path.c_str());
	return value[0] ? value : default_value;
}

int GetSetting(const std::wstring& key, int default_value, const std::wstring& section)
{
	assert(!settings_path.empty());
	return GetPrivateProfileInt(section.c_str(), key.c_str(), default_value, settings_path.c_str());
}

bool GetFlag(const std::wstring& key, bool default_value, const std::wstring& section)
{
	assert(!settings_path.empty());

	wchar_t value[64];
	GetPrivateProfileString(section.c_str(), key.c_str(), L"", value, _countof(value), settings_path.c_str());
	return value[0] ? !!std::stoul(value) : default_value;
}

void RemoveSetting(const std::wstring& key, const std::wstring& section)
{
	WritePrivateProfileString(section.c_str(), key.c_str(), nullptr, settings_path.c_str());
}
